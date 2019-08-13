/**
* @file include/retdec/bin2llvmir/optimizations/strucst/structs.h
* @brief Reconstruct struct.
* @copyright (c) 2017 Avast Software, licensed under the MIT license
*/

#ifndef RETDEC_BIN2LLVMIR_OPTIMIZATIONS_STRUCTS_STRUCTS_H
#define RETDEC_BIN2LLVMIR_OPTIMIZATIONS_STRUCTS_STRUCTS_H

#include <llvm/IR/Module.h>
#include <llvm/Pass.h>

#include "retdec/bin2llvmir/providers/abi/abi.h"
#include "retdec/bin2llvmir/providers/config.h"
#include "retdec/bin2llvmir/providers/debugformat.h"

namespace retdec {
namespace bin2llvmir {

class StructsAnalysis : public llvm::ModulePass
{
	public:
		static char ID;
		StructsAnalysis();
		virtual bool runOnModule(llvm::Module& m) override;
		bool runOnModuleCustom(
				llvm::Module& m,
				Config* c,
				Abi* abi,
				DebugFormat* dbgf = nullptr);

	private:
		bool run();
	
		llvm::StructType* getStructType(const llvm::Value* var) const;
		bool holdsStructureType(const llvm::Value* var) const;

		llvm::GlobalVariable* correctUsageOfGlobalStructure(llvm::GlobalVariable* gv, retdec::utils::Address& addr, size_t structLevel = 0); 

		llvm::Instruction* getElement(llvm::Value* v, std::size_t idx) const;
		llvm::Instruction* getElement(llvm::Value* v, const std::vector<llvm::Value*>& idxs) const;
		llvm::GlobalVariable* createCopy(llvm::GlobalVariable* gv);

		std::size_t getAlignment(llvm::StructType* st) const;

		void replaceElementWithStrIdx(llvm::Value* element, llvm::Value* str, std::size_t idx);

	private:
		llvm::Module* _module = nullptr;
		Config* _config = nullptr;
		Abi* _abi = nullptr;
		DebugFormat* _dbgf = nullptr;
};

} // namespace bin2llvmir
} // namespace retdec

#endif
