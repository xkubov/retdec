/**
* @file src/bin2llvmir/optimizations/structs/structs.cpp
* @brief Reconstruct structures.
* @copyright (c) 2019 Avast Software, licensed under the MIT license
*/

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>

#include "retdec/bin2llvmir/analyses/reaching_definitions.h"
#include "retdec/bin2llvmir/utils/ir_modifier.h"
#include "retdec/bin2llvmir/optimizations/structs/structs.h"
#include "retdec/bin2llvmir/providers/asm_instruction.h"
#include "retdec/bin2llvmir/utils/ir_modifier.h"
#define debug_enabled true
#include "retdec/bin2llvmir/utils/llvm.h"
#include "retdec/bin2llvmir/utils/debug.h"

using namespace llvm;

namespace retdec {
namespace bin2llvmir {

char StructsAnalysis::ID = 0;

static RegisterPass<StructsAnalysis> X(
		"structs",
		"Structures optimization",
		false, // Only looks at CFG
		false // Analysis Pass
);

StructsAnalysis::StructsAnalysis() :
		ModulePass(ID)
{

}

bool StructsAnalysis::runOnModule(llvm::Module& m)
{
	_module = &m;
	_config = ConfigProvider::getConfig(_module);
	_abi = AbiProvider::getAbi(_module);
	_dbgf = DebugFormatProvider::getDebugFormat(_module);
	return run();
}

bool StructsAnalysis::runOnModuleCustom(
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

StructType* StructsAnalysis::getStructType(const llvm::Value* var) const
{
	if (auto* t = dyn_cast<PointerType>(var->getType())) {
		auto* el = t->getElementType();

		return dyn_cast<StructType>(el);
	}

	return dyn_cast<StructType>(var->getType());
}

bool StructsAnalysis::holdsStructureType(const llvm::Value* var) const
{
	return getStructType(var) != nullptr;
}

GlobalVariable* StructsAnalysis::createCopy(GlobalVariable* gv)
{
	auto type = dyn_cast<PointerType>(gv->getType())->getElementType();

	auto gl = new GlobalVariable(*_module, type, false, gv->getLinkage(), gv->getInitializer());

	return gl;
}

Instruction* StructsAnalysis::getElement(llvm::Value* v, std::size_t idx) const
{
	auto zero = ConstantInt::get(IntegerType::get(_module->getContext(), 32), 0);
	auto eIdx= ConstantInt::get(IntegerType::get(_module->getContext(), 32), idx);

	return GetElementPtrInst::CreateInBounds(getStructType(v), v, {zero, eIdx});
}

Instruction* StructsAnalysis::getElement(llvm::Value* v, const std::vector<Value*> &idxs) const
{
	return GetElementPtrInst::CreateInBounds(getStructType(v), v, idxs);
}

std::size_t StructsAnalysis::getAlignment(StructType* st) const
{
	std::size_t alignment = 0;
	for (auto e: st->elements())
	{
		std::size_t eSize = 0;

		if (auto* st = dyn_cast<StructType>(e))
			eSize = getAlignment(st);

		else
			eSize = _abi->getTypeByteSize(e);

		//TODO: did we tought through arrays?
		
		if (eSize > alignment)
			alignment = eSize;
	}

	if (alignment > _abi->getWordSize())
		alignment = _abi->getWordSize();

	return alignment;
}

/**
 * Expects element to have same type as element at idx in str.
 */
void StructsAnalysis::replaceElementWithStrIdx(llvm::Value* element, llvm::Value* str, std::size_t idx)
{
	auto elementType = dyn_cast<PointerType>(element->getType())->getElementType();
	std::vector<User*> uses;
	for (auto* u: element->users())
	{
		uses.push_back(u);
	}

	for (auto u: uses) {
		if (auto* ld = dyn_cast<LoadInst>(u))
		{
			auto elemVal = getElement(str, idx);
			elemVal->insertBefore(ld);
			auto load = new LoadInst(elementType, elemVal, "", ld);
			ld->replaceAllUsesWith(load);
			ld->eraseFromParent();
		}
		else if (auto* st = dyn_cast<StoreInst>(u))
		{
			auto elemVal = getElement(str, idx);
			elemVal->insertBefore(st);
			new StoreInst(st->getValueOperand(), elemVal, st);
			st->eraseFromParent();
		}
		else if (auto* ep = dyn_cast<GetElementPtrInst>(u))
		{
			auto zero = ConstantInt::get(IntegerType::get(_module->getContext(), 32), 0);
			auto eIdx = ConstantInt::get(IntegerType::get(_module->getContext(), 32), idx);
			std::vector<Value*> idxs = {zero, eIdx};
			bool isFirst = true;
			for (auto& i : ep->indices())
			{
				if (!isFirst)
					idxs.push_back(i.get());
				else
					isFirst = false;
			}

			auto elem = getElement(str, idxs);
			elem->insertBefore(ep);
			ep->replaceAllUsesWith(elem);
			ep->eraseFromParent();
		}
	}

}

llvm::GlobalVariable* StructsAnalysis::correctUsageOfGlobalStructure(GlobalVariable* gl, retdec::utils::Address& addr, size_t structLevel)
{
	if (!holdsStructureType(gl))
		return gl;

	auto defAlign = getAlignment(getStructType(gl));
	auto alignment = defAlign;

	auto origAddr = addr;

	auto newStructure = createCopy(gl);
	auto irm = IrModifier(_module, _config);
	auto image = FileImageProvider::getFileImage(_module);

	std::size_t idx = 0;

	for (auto elem: getStructType(newStructure)->elements())
	{
		if (auto*elemStr = dyn_cast<StructType>(elem))
		{
			auto na = getAlignment(elemStr);
			if (defAlign > alignment)
				addr += (alignment)%na;

			alignment = defAlign;
			auto structElement = _config->getLlvmGlobalVariable(addr);
			structElement = dyn_cast<GlobalVariable>(irm.changeObjectType(image, structElement, elem));
			auto oldAddr = addr;
				structElement = correctUsageOfGlobalStructure(structElement, addr, structLevel+1);
				addr += (addr-oldAddr)%na; // another solution would be to do this on the end as: addr+=alignment;
				replaceElementWithStrIdx(structElement, newStructure, idx++);

				continue;
			}
			
			auto elemSize = _abi->getTypeByteSize(elem);
			if (alignment < elemSize) {

				addr += alignment;
				alignment = defAlign;
			}
			
			auto structElement = _config->getLlvmGlobalVariable(addr);
			structElement = dyn_cast<GlobalVariable>(irm.changeObjectType(image, structElement, elem));
			for (size_t i = 0; i < structLevel; i++)
			{
				LOG << "\t";
			}
			LOG << addr << " " << llvmObjToString(dyn_cast<PointerType>(structElement->getType())->getElementType()) << std::endl;

			replaceElementWithStrIdx(structElement, newStructure, idx++);

			alignment -= elemSize;
			addr += elemSize;
	}

	auto origStr = _config->getLlvmGlobalVariable(origAddr);
	newStructure->takeName(origStr);

	return newStructure;
}

bool StructsAnalysis::run()
{
	std::vector<GlobalVariable*> globalList;
	for (auto& g: _module->getGlobalList())
	{
		globalList.push_back(&g);
	}

	for (auto& g: globalList)
	{
		if (!_config->isGlobalVariable(g)) {
			continue;
		}
	
		auto addr = _config->getGlobalAddress(g);
		correctUsageOfGlobalStructure(g, addr);
	}
	
//	for (auto& f: _module->functions()) {
//		for (auto& b: f) {
//			for (auto& i: b) {
//				//exit(4);
//			}
//		}
//	}

	return false;
}

} // namespace bin2llvmir
} // namespace retdec
