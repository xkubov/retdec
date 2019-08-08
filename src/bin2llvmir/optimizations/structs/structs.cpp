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

llvm::GlobalVariable* StructsAnalysis::correctUsageOfGlobalStructure(GlobalVariable& gl)
{
	if (!holdsStructureType(&gl))
		return &gl;

	auto addr = _config->getGlobalAddress(&gl);
	auto defAlign = getAlignment(getStructType(&gl));
	auto alignment = defAlign;

	auto newStructure = createCopy(&gl);
	auto irm = IrModifier(_module, _config);
	auto image = FileImageProvider::getFileImage(_module);

	auto add = addr;
	
	std::size_t idx = 0;

	for (auto elem: getStructType(newStructure)->elements())
	{
		auto elemSize = _abi->getTypeByteSize(elem);
		if (auto* st = dyn_cast<StructType>(elem))
		{
			elemSize = _abi->getTypeByteSize(*st->element_begin());
		}

		LOG << std::dec;
		LOG << "alignment: " << alignment << std::endl;
		LOG << "elem size: " << elemSize << std::endl;
		LOG << "addr: " << std::hex << add << std::endl;	

		if (alignment < elemSize) {
			add += alignment;
			LOG << "inced addr: " << std::hex << add << std::endl;	
			alignment = defAlign;
		}
		alignment -= elemSize;
		
		LOG << "Has struct " << isa<StructType>(elem) << std::endl;
		LOG << "elem addr: " << std::hex << add << std::endl;	

		auto structElement = _config->getLlvmGlobalVariable(add);
		structElement = dyn_cast<GlobalVariable>(irm.changeObjectType(image, structElement, elem));

		if (isa<StructType>(elem))
			structElement = correctUsageOfGlobalStructure(*structElement);

		for (auto* u: structElement->users()) {
			if (auto* ld = dyn_cast<LoadInst>(u))
			{
				auto elemVal = getElement(newStructure, idx);
				elemVal->insertBefore(ld);
				auto load = new LoadInst(elem, elemVal, "", ld);
				ld->replaceAllUsesWith(load);
				ld->eraseFromParent();
			}
			else if (auto* st = dyn_cast<StoreInst>(u))
			{
				auto elemVal = getElement(newStructure, idx);
				elemVal->insertBefore(st);
				new StoreInst(st->getValueOperand(), elemVal, st);
				//store->insertAfter(st);
				st->eraseFromParent();
			}
		}

		add += elemSize;
		idx++;
	}

	auto origStr = _config->getLlvmGlobalVariable(addr);
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

		correctUsageOfGlobalStructure(*g);
	}
	
	for (auto& f: _module->functions()) {
		for (auto& b: f) {
			for (auto& i: b) {
				//exit(4);
			}
		}
	}

	return false;
}

} // namespace bin2llvmir
} // namespace retdec
