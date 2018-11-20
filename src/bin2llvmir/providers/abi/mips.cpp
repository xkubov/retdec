/**
 * @file src/bin2llvmir/providers/abi/mips.cpp
 * @brief ABI information for MIPS.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "retdec/bin2llvmir/providers/abi/mips.h"

using namespace llvm;

namespace retdec {
namespace bin2llvmir {

AbiMips::AbiMips(llvm::Module* m, Config* c) :
		Abi(m, c)
{
	_regs.reserve(MIPS_REG_ENDING);
	_id2regs.resize(MIPS_REG_ENDING, nullptr);
	_regStackPointerId = MIPS_REG_SP;
	_regZeroReg = MIPS_REG_ZERO;

	_fpRegsAsParams = true;

	// system calls
	_regSyscallId = MIPS_REG_V0;
	_regSyscallReturn = MIPS_REG_V0;
	_syscallRegs = {
			MIPS_REG_A0,
			MIPS_REG_A1,
			MIPS_REG_A2,
			MIPS_REG_A3};

	_regReturn = MIPS_REG_V0;
	_regFPReturn = MIPS_REG_V0;

	if (_config->getConfig().tools.isPspGcc())
	{
		_paramRegs = {
			MIPS_REG_A0,
			MIPS_REG_A1,
			MIPS_REG_A2,
			MIPS_REG_A3,
			MIPS_REG_T0,
			MIPS_REG_T1,
			MIPS_REG_T2,
			MIPS_REG_T3};
	}
	else
	{
		_paramRegs = {
			MIPS_REG_A0,
			MIPS_REG_A1,
			MIPS_REG_A2,
			MIPS_REG_A3};
	}

	_paramFPRegs = {
		MIPS_REG_F12,
		MIPS_REG_F14,
		MIPS_REG_F16,
		MIPS_REG_F18
	};

}

AbiMips::~AbiMips()
{

}

bool AbiMips::isGeneralPurposeRegister(const llvm::Value* val)
{
	uint32_t rid = getRegisterId(val);
	return MIPS_REG_0 <= rid && rid <= MIPS_REG_31;
}

bool AbiMips::isNopInstruction(cs_insn* insn)
{
	// True NOP variants.
	//
	if (insn->id == MIPS_INS_NOP
			|| insn->id == MIPS_INS_SSNOP)
	{
		return true;
	}

	return false;
}

} // namespace bin2llvmir
} // namespace retdec
