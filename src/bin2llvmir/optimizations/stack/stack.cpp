/**
* @file src/bin2llvmir/optimizations/stack/stack.cpp
* @brief Reconstruct stack.
* @copyright (c) 2017 Avast Software, licensed under the MIT license
*/

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>

#include "retdec/bin2llvmir/analyses/reaching_definitions.h"
#include "retdec/bin2llvmir/optimizations/stack/stack.h"
#include "retdec/bin2llvmir/providers/asm_instruction.h"
#include "retdec/bin2llvmir/utils/ir_modifier.h"
#define debug_enabled true
#include "retdec/bin2llvmir/utils/llvm.h"

using namespace llvm;

namespace retdec {
namespace bin2llvmir {

char StackAnalysis::ID = 0;

static RegisterPass<StackAnalysis> X(
		"stack",
		"Stack optimization",
		false, // Only looks at CFG
		false // Analysis Pass
);

StackAnalysis::StackAnalysis() :
		ModulePass(ID)
{

}

bool StackAnalysis::runOnModule(llvm::Module& m)
{
	_module = &m;
	_config = ConfigProvider::getConfig(_module);
	_abi = AbiProvider::getAbi(_module);
	_dbgf = DebugFormatProvider::getDebugFormat(_module);

	return run();
}

bool StackAnalysis::runOnModuleCustom(
		llvm::Module& m,
		Config* c,
		Abi* abi,
		DebugFormat* dbgf)
{
	_module = &m;
	_config = c;
	_abi = abi;
	_dbgf = dbgf;
	return run();
}

bool StackAnalysis::run()
{
	if (_config == nullptr)
	{
		return false;
	}

	ReachingDefinitionsAnalysis RDA;
	RDA.runOnModule(*_module, _abi);

	std::map<llvm::Function*, std::vector<llvm::AllocaInst*>> stackVarsMap;
	std::map<llvm::AllocaInst*, std::vector<llvm::Instruction*>> usesOfStack;

	for (auto& f : *_module)
	{
		std::set<llvm::AllocaInst*> stackVars;

		std::map<Value*, Value*> val2val;
		for (inst_iterator I = inst_begin(f), E = inst_end(f); I != E;)
		{
			Instruction& i = *I;
			++I;

			if (StoreInst *store = dyn_cast<StoreInst>(&i))
			{
				if (AsmInstruction::isLlvmToAsmInstruction(store))
				{
					continue;
				}

				handleInstruction(
						RDA,
						store,
						store->getValueOperand(),
						store->getValueOperand()->getType(),
						val2val,
						stackVars,
						usesOfStack);

				if (isa<GlobalVariable>(store->getPointerOperand()))
				{
					continue;
				}

				handleInstruction(
						RDA,
						store,
						store->getPointerOperand(),
						store->getValueOperand()->getType(),
						val2val,
						stackVars,
						usesOfStack);
			}
			else if (LoadInst* load = dyn_cast<LoadInst>(&i))
			{
				if (isa<GlobalVariable>(load->getPointerOperand()))
				{
					continue;
				}

				handleInstruction(
						RDA,
						load,
						load->getPointerOperand(),
						load->getType(),
						val2val,
						stackVars,
						usesOfStack);
			}

		}

		std::vector<llvm::AllocaInst*> stckV(stackVars.begin(), stackVars.end());

		std::stable_sort(
			stckV.begin(),
			stckV.end(),
			[this](Value* a, Value* b) -> bool
			{
				return _config->getStackVariableOffset(a) >
						_config->getStackVariableOffset(b);
			});

		stackVarsMap[&f] = stckV;
	}

	mergeStackTypesPieces(stackVarsMap, usesOfStack);

	return false;
}

void StackAnalysis::mergeStackTypesPieces(std::map<Function*,
				std::vector<AllocaInst*>> &stackVarsMap,
				std::map<AllocaInst*, std::vector<llvm::Instruction*>> &usesOfStack) const
{
	for (auto i: stackVarsMap)
	{
		if (i.second.size() < 2)
			continue;

		for (auto j = i.second.begin(), k = j+1; k != i.second.end(); j++, k++)
		{
			LOG << "\t" << llvmObjToString(*j) << std::endl;

			size_t stackDiff = _config->getStackVariableOffset(*j)
						- _config->getStackVariableOffset(*k);

			size_t hiTySize = _abi->getTypeByteSize((*j)->getAllocatedType());
			size_t loTySize = _abi->getTypeByteSize((*k)->getAllocatedType());

			if (stackDiff + hiTySize == loTySize)
			{
				replaceUsageWithShiftWork(*j, *k, usesOfStack);
			}
		}
	}
}

void StackAnalysis::replaceUsageWithShiftWork(AllocaInst *r, AllocaInst *w,
	std::map<AllocaInst*, std::vector<llvm::Instruction*>> &usesOfStack) const
{
	for (auto i: usesOfStack[r])
	{
		/*
		if (StoreInst *store = dyn_cast<StoreInst>(i))
		{
			auto l = new LoadInst(w, "", store);
			auto intType = IntegerType::get(_module->getContext(), 64);

			auto an = BinaryOperator::Create(
					Instruction::And,
					l,
					llvm::ConstantInt::get(intType, 0xffffffff00000000),
					"",
					store);

			auto ext = 


			auto or = BinaryOperator::Create(
					Instruction::Or,
					a,
					ext,
					"",
					store);

			store->replaceAllUsesWith(or);
		}
		else */if (LoadInst *load = dyn_cast<LoadInst>(i))
		{
			auto size = 32;
			auto halfInt = IntegerType::get(_module->getContext(), 32);
			auto intType = IntegerType::get(_module->getContext(), 64);

			auto l = new LoadInst(w, "", load);
			auto sh = BinaryOperator::Create(
					Instruction::LShr,
					l,
					llvm::ConstantInt::get(intType, size),
					"",
					load);

			auto tr = new TruncInst(sh, halfInt, "", load);

			load->replaceAllUsesWith(tr);
		}
	}
}

	void StackAnalysis::handleInstruction(
		ReachingDefinitionsAnalysis& RDA,
		llvm::Instruction* inst,
		llvm::Value* val,
		llvm::Type* type,
		std::map<llvm::Value*, llvm::Value*>& val2val,
		std::set<llvm::AllocaInst*> &stackVars,
		std::map<llvm::AllocaInst*, std::vector<llvm::Instruction*>> &usesOfStack)
{
	LOG << llvmObjToString(inst) << std::endl;

	SymbolicTree root(RDA, val, &val2val);
	LOG << root << std::endl;

	if (!root.isVal2ValMapUsed())
	{
		bool stackPtr = false;
		for (SymbolicTree* n : root.getPostOrder())
		{
			if (_abi->isStackPointerRegister(n->value))
			{
				stackPtr = true;
				break;
			}
		}
		if (!stackPtr)
		{
			LOG << "===> no SP" << std::endl;
			return;
		}
	}

	auto* debugSv = getDebugStackVariable(inst->getFunction(), root);

	root.simplifyNode();
	LOG << root << std::endl;

	if (debugSv == nullptr)
	{
		debugSv = getDebugStackVariable(inst->getFunction(), root);
	}

	auto* ci = dyn_cast_or_null<ConstantInt>(root.value);
	if (ci == nullptr)
	{
		return;
	}

	if (auto* s = dyn_cast<StoreInst>(inst))
	{
		if (s->getValueOperand() == val)
		{
			val2val[inst] = ci;
		}
	}

	LOG << "===> " << llvmObjToString(ci) << std::endl;
	LOG << "===> " << ci->getSExtValue() << std::endl;

	std::string name = debugSv ? debugSv->getName() : "";
	Type* t = debugSv ?
			llvm_utils::stringToLlvmTypeDefault(_module, debugSv->type.getLlvmIr()) :
			type;

	IrModifier irModif(_module, _config);
	auto p = irModif.getStackVariable(
			inst->getFunction(),
			ci->getSExtValue(),
			t,
			name);

	AllocaInst* a = p.first;
	auto* ca = p.second;

	if (debugSv)
	{
		ca->setIsFromDebug(true);
		ca->setRealName(debugSv->getName());
	}

	LOG << "===> " << llvmObjToString(a) << std::endl;
	LOG << "===> " << llvmObjToString(inst) << std::endl;
	LOG << std::endl;

	stackVars.insert(a);
	usesOfStack[a].push_back(inst);

	auto* s = dyn_cast<StoreInst>(inst);
	auto* l = dyn_cast<LoadInst>(inst);
	if (s && s->getPointerOperand() == val)
	{
		auto* conv = IrModifier::convertValueToType(
				s->getValueOperand(),
				a->getType()->getElementType(),
				inst);
		new StoreInst(conv, a, inst);
		s->eraseFromParent();
	}
	else if (l && l->getPointerOperand() == val)
	{
		auto* nl = new LoadInst(a, "", l);
		auto* conv = IrModifier::convertValueToType(nl, l->getType(), l);
		l->replaceAllUsesWith(conv);
		l->eraseFromParent();
	}
	else
	{
		auto* conv = IrModifier::convertValueToType(a, val->getType(), inst);
		inst->replaceUsesOfWith(val, conv);
	}
}

/**
 * Find a value that is being added to the stack pointer register in \p root.
 * Find a debug variable with offset equal to this value.
 */
retdec::config::Object* StackAnalysis::getDebugStackVariable(
		llvm::Function* fnc,
		SymbolicTree& root)
{
	if (_dbgf == nullptr)
	{
		return nullptr;
	}
	auto* debugFnc = _dbgf->getFunction(_config->getFunctionAddress(fnc));
	if (debugFnc == nullptr)
	{
		return nullptr;
	}

	retdec::utils::Maybe<int> baseOffset;
	if (auto* ci = dyn_cast_or_null<ConstantInt>(root.value))
	{
		baseOffset = ci->getSExtValue();
	}
	else
	{
		for (SymbolicTree* n : root.getLevelOrder())
		{
			if (isa<AddOperator>(n->value)
					&& n->ops.size() == 2
					&& isa<LoadInst>(n->ops[0].value)
					&& isa<ConstantInt>(n->ops[1].value))
			{
				auto* l = cast<LoadInst>(n->ops[0].value);
				auto* ci = cast<ConstantInt>(n->ops[1].value);
				if (_abi->isRegister(l->getPointerOperand()))
				{
					baseOffset = ci->getSExtValue();
				}
				break;
			}
		}
	}
	if (baseOffset.isUndefined())
	{
		return nullptr;
	}

	for (auto& p : debugFnc->locals)
	{
		auto& var = p.second;
		if (!var.getStorage().isStack())
		{
			continue;
		}
		if (var.getStorage().getStackOffset() == baseOffset)
		{
			return &var;
		}
	}

	return nullptr;
}

} // namespace bin2llvmir
} // namespace retdec
