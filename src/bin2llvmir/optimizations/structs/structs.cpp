/**
* @file src/bin2llvmir/optimizations/structs/structs.cpp
* @brief Reconstruct structures.
* @copyright (c) 2019 Avast Software, licensed under the MIT license
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

bool StructsAnalysis::run()
{
	LOG << "Hello world" << std::endl;
	exit(4);

	return false;
}

} // namespace bin2llvmir
} // namespace retdec
