/**
 * @file include/retdec/capstone2llvmir/x86/x86_defs.h
 * @brief Additional (on top of Capstone) definitions for x86 translator.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef RETDEC_CAPSTONE2LLVMIR_X86_X86_DEFS_H
#define RETDEC_CAPSTONE2LLVMIR_X86_X86_DEFS_H

/**
 * A flag register addition to @c x86_reg from capstone/x86.h.
 * Translator works with flag registers explicitly, but they are modeled only
 * as @c X86_REG_EFLAGS in the original @c x86_reg enum.
 * This is intentionally not a strongly typed enum to keep it consistent
 * with @c x86_reg enum.
 */
enum x86_reg_rflags
{
// FLAGS
	X86_REG_CF = X86_REG_ENDING + 1, // 0
	// reserved 1
	X86_REG_PF, // 2
	// reserved 3
	X86_REG_AF, // 4
	// reserved 5
	X86_REG_ZF, // 6
	X86_REG_SF, // 7
	X86_REG_TF, // 8
	X86_REG_IF, // 9
	X86_REG_DF, // 10
	X86_REG_OF, // 11
	X86_REG_IOPL, // 12-13
	X86_REG_NT, // 14
	// reserved 15
// EFLAGS
	X86_REG_RF, // 16
	X86_REG_VM, // 17
	X86_REG_AC, // 18
	X86_REG_VIF, // 19
	X86_REG_VIP, // 20
	X86_REG_ID // 21
	// reserved 22-31
// RFLAGS
	// reserved 32-63
};

/**
 * An FPU status register addition to @c x86_reg from capstone/x86.h.
 * Translator works with status registers explicitly, but they are modeled only
 * as @c X86_REG_FPSW in the original @c x86_reg enum.
 * This is intentionally not a strongly typed enum to keep it consistent
 * with @c x86_reg enum.
 */
enum x87_reg_status
{
// Exception flags
// One or more fp exceptions have been detected since the bits were last
// cleared.
	X87_REG_IE = X86_REG_ID + 1,  //  0 -- invalid operation
	X87_REG_DE,  //  1 -- denormalized operand
	X87_REG_ZE,  //  2 -- zero divide
	X87_REG_OE,  //  3 -- overflow
	X87_REG_UE,  //  4 -- underflow
	X87_REG_PE,  //  5 -- precision
// Stack fault
// Indicates that stack overflow or underflow has occurred with x87 FPU data
// register stack.
	X87_REG_SF,  //  6
// Error summanry status
	X87_REG_ES, //  7
// Condition code
// Indicate the results of fp comparison and arithmetic operations.
	X87_REG_C0,  //  8
	X87_REG_C1,  //  9
	X87_REG_C2,  // 10
	X87_REG_C3,  // 14
// Top of stack pointer
	X87_REG_TOP, // 11-13
// FPU busy
	X87_REG_B    // 15
};

/**
 * An FPU control register addition to @c x86_reg from capstone/x86.h.
 * Translator works with control registers explicitly, but it looks like they
 * are not modeled in the original @c x86_reg enum.
 * This is intentionally not a strongly typed enum to keep it consistent
 * with @c x86_reg enum.
 */
enum x87_reg_control
{
// Exception Masks
	X87_REG_IM = X87_REG_B + 1, // 0 -- invalid operation
	X87_REG_DM, // 1 -- denormal operand
	X87_REG_ZM, // 2 -- zero divide
	X87_REG_OM, // 3 -- overflow
	X87_REG_UM, // 4 -- underflow
	X87_REG_PM, // 5 -- precision
	// reserved 6-7
// Precision control
	X87_REG_PC, // 8-9
// Rounding Control
	X87_REG_RC, // 10-11
// Infiinity control
	X87_REG_X, // 12
	// reserved 13-15
};

/**
 * An FPU tag register addition to @c x86_reg from capstone/x86.h.
 * Translator works with tag registers explicitly, but it looks like they
 * are not modeled in the original @c x86_reg enum.
 * This is intentionally not a strongly typed enum to keep it consistent
 * with @c x86_reg enum.
 */
enum x87_reg_tag
{
	X87_REG_TAG0 = X87_REG_X + 1,
	X87_REG_TAG1,
	X87_REG_TAG2,
	X87_REG_TAG3,
	X87_REG_TAG4,
	X87_REG_TAG5,
	X87_REG_TAG6,
	X87_REG_TAG7
};

enum x86_reg_sse
{
	X86_REG_XMMF0 = X87_REG_TAG7 + 1,
	X86_REG_XMMF1,
	X86_REG_XMMF2,
	X86_REG_XMMF3,
	X86_REG_XMMF4,
	X86_REG_XMMF5,
	X86_REG_XMMF6,
	X86_REG_XMMF7,
	X86_REG_XMMF8,
	X86_REG_XMMF9,
	X86_REG_XMMF10,
	X86_REG_XMMF11,
	X86_REG_XMMF12,
	X86_REG_XMMF13,
	X86_REG_XMMF14,
	X86_REG_XMMF15,
	X86_REG_XMMF16,
	X86_REG_XMMF17,
	X86_REG_XMMF18,
	X86_REG_XMMF19,
	X86_REG_XMMF20,
	X86_REG_XMMF21,
	X86_REG_XMMF22,
	X86_REG_XMMF23,
	X86_REG_XMMF24,
	X86_REG_XMMF25,
	X86_REG_XMMF26,
	X86_REG_XMMF27,
	X86_REG_XMMF28,
	X86_REG_XMMF29,
	X86_REG_XMMF30,
	X86_REG_XMMF31,

	X86_REG_XMMD0,
	X86_REG_XMMD1,
	X86_REG_XMMD2,
	X86_REG_XMMD3,
	X86_REG_XMMD4,
	X86_REG_XMMD5,
	X86_REG_XMMD6,
	X86_REG_XMMD7,
	X86_REG_XMMD8,
	X86_REG_XMMD9,
	X86_REG_XMMD10,
	X86_REG_XMMD11,
	X86_REG_XMMD12,
	X86_REG_XMMD13,
	X86_REG_XMMD14,
	X86_REG_XMMD15,
	X86_REG_XMMD16,
	X86_REG_XMMD17,
	X86_REG_XMMD18,
	X86_REG_XMMD19,
	X86_REG_XMMD20,
	X86_REG_XMMD21,
	X86_REG_XMMD22,
	X86_REG_XMMD23,
	X86_REG_XMMD24,
	X86_REG_XMMD25,
	X86_REG_XMMD26,
	X86_REG_XMMD27,
	X86_REG_XMMD28,
	X86_REG_XMMD29,
	X86_REG_XMMD30,
	X86_REG_XMMD31
};

enum x86_reg_sse_float
{
	X86_REG_XMMF0_3 = X86_REG_XMMD31 + 1,
	X86_REG_XMMF1_3,
	X86_REG_XMMF2_3,
	X86_REG_XMMF3_3,
	X86_REG_XMMF4_3,
	X86_REG_XMMF5_3,
	X86_REG_XMMF6_3,
	X86_REG_XMMF7_3,
	X86_REG_XMMF8_3,
	X86_REG_XMMF9_3,
	X86_REG_XMMF10_3,
	X86_REG_XMMF11_3,
	X86_REG_XMMF12_3,
	X86_REG_XMMF13_3,
	X86_REG_XMMF14_3,
	X86_REG_XMMF15_3,
	X86_REG_XMMF16_3,
	X86_REG_XMMF17_3,
	X86_REG_XMMF18_3,
	X86_REG_XMMF19_3,
	X86_REG_XMMF20_3,
	X86_REG_XMMF21_3,
	X86_REG_XMMF22_3,
	X86_REG_XMMF23_3,
	X86_REG_XMMF24_3,
	X86_REG_XMMF25_3,
	X86_REG_XMMF26_3,
	X86_REG_XMMF27_3,
	X86_REG_XMMF28_3,
	X86_REG_XMMF29_3,
	X86_REG_XMMF30_3,
	X86_REG_XMMF31_3,
};

enum x86_reg_sse_double
{
	X86_REG_XMMD0_1 = X86_REG_XMMF31_3 + 1,
	X86_REG_XMMD1_1,
	X86_REG_XMMD2_1,
	X86_REG_XMMD3_1,
	X86_REG_XMMD4_1,
	X86_REG_XMMD5_1,
	X86_REG_XMMD6_1,
	X86_REG_XMMD7_1,
	X86_REG_XMMD8_1,
	X86_REG_XMMD9_1,
	X86_REG_XMMD10_1,
	X86_REG_XMMD11_1,
	X86_REG_XMMD12_1,
	X86_REG_XMMD13_1,
	X86_REG_XMMD14_1,
	X86_REG_XMMD15_1,
	X86_REG_XMMD16_1,
	X86_REG_XMMD17_1,
	X86_REG_XMMD18_1,
	X86_REG_XMMD19_1,
	X86_REG_XMMD20_1,
	X86_REG_XMMD21_1,
	X86_REG_XMMD22_1,
	X86_REG_XMMD23_1,
	X86_REG_XMMD24_1,
	X86_REG_XMMD25_1,
	X86_REG_XMMD26_1,
	X86_REG_XMMD27_1,
	X86_REG_XMMD28_1,
	X86_REG_XMMD29_1,
	X86_REG_XMMD30_1,
	X86_REG_XMMD31_1,
};

/**
 * Representation of x86 address spaces.
 *
 * Based on values in X86ISelDAGToDag.cpp.
 */
enum class x86_addr_space
{
	DEFAULT = 0,
	GS = 256,
	FS = 257,
	SS = 258
};

#endif
