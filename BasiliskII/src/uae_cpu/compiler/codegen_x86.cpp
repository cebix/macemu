/*
 *  compiler/codegen_x86.cpp - IA-32 code generator
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2005
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Portions related to CPU detection come from linux/arch/i386/kernel/setup.c
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* This should eventually end up in machdep/, but for now, x86 is the
   only target, and it's easier this way... */

#include "flags_x86.h"

/*************************************************************************
 * Some basic information about the the target CPU                       *
 *************************************************************************/

#define EAX_INDEX 0
#define ECX_INDEX 1
#define EDX_INDEX 2
#define EBX_INDEX 3
#define ESP_INDEX 4
#define EBP_INDEX 5
#define ESI_INDEX 6
#define EDI_INDEX 7
#if defined(__x86_64__)
#define R8_INDEX  8
#define R9_INDEX  9
#define R10_INDEX 10
#define R11_INDEX 11
#define R12_INDEX 12
#define R13_INDEX 13
#define R14_INDEX 14
#define R15_INDEX 15
#endif
/* XXX this has to match X86_Reg8H_Base + 4 */
#define AH_INDEX (0x10+4+EAX_INDEX)
#define CH_INDEX (0x10+4+ECX_INDEX)
#define DH_INDEX (0x10+4+EDX_INDEX)
#define BH_INDEX (0x10+4+EBX_INDEX)

/* The register in which subroutines return an integer return value */
#define REG_RESULT EAX_INDEX

/* The registers subroutines take their first and second argument in */
#if defined( _MSC_VER ) && !defined( USE_NORMAL_CALLING_CONVENTION )
/* Handle the _fastcall parameters of ECX and EDX */
#define REG_PAR1 ECX_INDEX
#define REG_PAR2 EDX_INDEX
#elif defined(__x86_64__)
#define REG_PAR1 EDI_INDEX
#define REG_PAR2 ESI_INDEX
#else
#define REG_PAR1 EAX_INDEX
#define REG_PAR2 EDX_INDEX
#endif

#define REG_PC_PRE EAX_INDEX /* The register we use for preloading regs.pc_p */
#if defined( _MSC_VER ) && !defined( USE_NORMAL_CALLING_CONVENTION )
#define REG_PC_TMP EAX_INDEX
#else
#define REG_PC_TMP ECX_INDEX /* Another register that is not the above */
#endif

#define SHIFTCOUNT_NREG ECX_INDEX  /* Register that can be used for shiftcount.
			      -1 if any reg will do */
#define MUL_NREG1 EAX_INDEX /* %eax will hold the low 32 bits after a 32x32 mul */
#define MUL_NREG2 EDX_INDEX /* %edx will hold the high 32 bits */

#define STACK_ALIGN		16
#define STACK_OFFSET	sizeof(void *)

uae_s8 always_used[]={4,-1};
#if defined(__x86_64__)
uae_s8 can_byte[]={0,1,2,3,5,6,7,8,9,10,11,12,13,14,15,-1};
uae_s8 can_word[]={0,1,2,3,5,6,7,8,9,10,11,12,13,14,15,-1};
#else
uae_s8 can_byte[]={0,1,2,3,-1};
uae_s8 can_word[]={0,1,2,3,5,6,7,-1};
#endif

#if USE_OPTIMIZED_CALLS
/* Make sure interpretive core does not use cpuopti */
uae_u8 call_saved[]={0,0,0,1,1,1,1,1};
#error FIXME: code not ready
#else
/* cpuopti mutate instruction handlers to assume registers are saved
   by the caller */
uae_u8 call_saved[]={0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0};
#endif

/* This *should* be the same as call_saved. But:
   - We might not really know which registers are saved, and which aren't,
     so we need to preserve some, but don't want to rely on everyone else
     also saving those registers
   - Special registers (such like the stack pointer) should not be "preserved"
     by pushing, even though they are "saved" across function calls
*/
#if defined(__x86_64__)
/* callee-saved registers as defined by Linux AMD64 ABI: rbx, rbp, rsp, r12 - r15 */
/* preserve r11 because it's generally used to hold pointers to functions */
static const uae_u8 need_to_preserve[]={0,0,0,1,0,1,0,0,0,0,0,1,1,1,1,1};
#else
/* callee-saved registers as defined by System V IA-32 ABI: edi, esi, ebx, ebp */
static const uae_u8 need_to_preserve[]={0,0,0,1,0,1,1,1};
#endif

/* Whether classes of instructions do or don't clobber the native flags */
#define CLOBBER_MOV
#define CLOBBER_LEA
#define CLOBBER_CMOV
#define CLOBBER_POP
#define CLOBBER_PUSH
#define CLOBBER_SUB  clobber_flags()
#define CLOBBER_SBB  clobber_flags()
#define CLOBBER_CMP  clobber_flags()
#define CLOBBER_ADD  clobber_flags()
#define CLOBBER_ADC  clobber_flags()
#define CLOBBER_AND  clobber_flags()
#define CLOBBER_OR   clobber_flags()
#define CLOBBER_XOR  clobber_flags()

#define CLOBBER_ROL  clobber_flags()
#define CLOBBER_ROR  clobber_flags()
#define CLOBBER_SHLL clobber_flags()
#define CLOBBER_SHRL clobber_flags()
#define CLOBBER_SHRA clobber_flags()
#define CLOBBER_TEST clobber_flags()
#define CLOBBER_CL16 
#define CLOBBER_CL8  
#define CLOBBER_SE32
#define CLOBBER_SE16
#define CLOBBER_SE8
#define CLOBBER_ZE32
#define CLOBBER_ZE16
#define CLOBBER_ZE8
#define CLOBBER_SW16 clobber_flags()
#define CLOBBER_SW32
#define CLOBBER_SETCC
#define CLOBBER_MUL  clobber_flags()
#define CLOBBER_BT   clobber_flags()
#define CLOBBER_BSF  clobber_flags()

/* The older code generator is now deprecated.  */
#define USE_NEW_RTASM 1

#if USE_NEW_RTASM

#if defined(__x86_64__)
#define X86_TARGET_64BIT		1
/* The address override prefix causes a 5 cycles penalty on Intel Core
   processors. Another solution would be to decompose the load in an LEA,
   MOV (to zero-extend), MOV (from memory): is it better? */
#define ADDR32					x86_emit_byte(0x67),
#else
#define ADDR32					/**/
#endif
#define X86_FLAT_REGISTERS		0
#define X86_OPTIMIZE_ALU		1
#define X86_OPTIMIZE_ROTSHI		1
#include "codegen_x86.h"

#define x86_emit_byte(B)		emit_byte(B)
#define x86_emit_word(W)		emit_word(W)
#define x86_emit_long(L)		emit_long(L)
#define x86_emit_quad(Q)		emit_quad(Q)
#define x86_get_target()		get_target()
#define x86_emit_failure(MSG)	jit_fail(MSG, __FILE__, __LINE__, __FUNCTION__)

static void jit_fail(const char *msg, const char *file, int line, const char *function)
{
	fprintf(stderr, "JIT failure in function %s from file %s at line %d: %s\n",
			function, file, line, msg);
	abort();
}

LOWFUNC(NONE,WRITE,1,raw_push_l_r,(R4 r))
{
#if defined(__x86_64__)
	PUSHQr(r);
#else
	PUSHLr(r);
#endif
}
LENDFUNC(NONE,WRITE,1,raw_push_l_r,(R4 r))

LOWFUNC(NONE,READ,1,raw_pop_l_r,(R4 r))
{
#if defined(__x86_64__)
	POPQr(r);
#else
	POPLr(r);
#endif
}
LENDFUNC(NONE,READ,1,raw_pop_l_r,(R4 r))

LOWFUNC(NONE,READ,1,raw_pop_l_m,(MEMW d))
{
#if defined(__x86_64__)
	POPQm(d, X86_NOREG, X86_NOREG, 1);
#else
	POPLm(d, X86_NOREG, X86_NOREG, 1);
#endif
}
LENDFUNC(NONE,READ,1,raw_pop_l_m,(MEMW d))

LOWFUNC(WRITE,NONE,2,raw_bt_l_ri,(R4 r, IMM i))
{
	BTLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_bt_l_ri,(R4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_bt_l_rr,(R4 r, R4 b))
{
	BTLrr(b, r);
}
LENDFUNC(WRITE,NONE,2,raw_bt_l_rr,(R4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_btc_l_ri,(RW4 r, IMM i))
{
	BTCLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_btc_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_btc_l_rr,(RW4 r, R4 b))
{
	BTCLrr(b, r);
}
LENDFUNC(WRITE,NONE,2,raw_btc_l_rr,(RW4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_btr_l_ri,(RW4 r, IMM i))
{
	BTRLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_btr_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_btr_l_rr,(RW4 r, R4 b))
{
	BTRLrr(b, r);
}
LENDFUNC(WRITE,NONE,2,raw_btr_l_rr,(RW4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_bts_l_ri,(RW4 r, IMM i))
{
	BTSLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_bts_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_bts_l_rr,(RW4 r, R4 b))
{
	BTSLrr(b, r);
}
LENDFUNC(WRITE,NONE,2,raw_bts_l_rr,(RW4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_sub_w_ri,(RW2 d, IMM i))
{
	SUBWir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_w_ri,(RW2 d, IMM i))

LOWFUNC(NONE,READ,2,raw_mov_l_rm,(W4 d, MEMR s))
{
	MOVLmr(s, X86_NOREG, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,2,raw_mov_l_rm,(W4 d, MEMR s))

LOWFUNC(NONE,WRITE,2,raw_mov_l_mi,(MEMW d, IMM s))
{
	MOVLim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_l_mi,(MEMW d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_w_mi,(MEMW d, IMM s))
{
	MOVWim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_w_mi,(MEMW d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_b_mi,(MEMW d, IMM s))
{
	MOVBim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_b_mi,(MEMW d, IMM s))

LOWFUNC(WRITE,RMW,2,raw_rol_b_mi,(MEMRW d, IMM i))
{
	ROLBim(i, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,RMW,2,raw_rol_b_mi,(MEMRW d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_b_ri,(RW1 r, IMM i))
{
	ROLBir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_rol_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_w_ri,(RW2 r, IMM i))
{
	ROLWir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_rol_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_l_ri,(RW4 r, IMM i))
{
	ROLLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_rol_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_l_rr,(RW4 d, R1 r))
{
	ROLLrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_rol_w_rr,(RW2 d, R1 r))
{
	ROLWrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_rol_b_rr,(RW1 d, R1 r))
{
	ROLBrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_l_rr,(RW4 d, R1 r))
{
	SHLLrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_w_rr,(RW2 d, R1 r))
{
	SHLWrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_b_rr,(RW1 d, R1 r))
{
	SHLBrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_b_ri,(RW1 r, IMM i))
{
	RORBir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_ror_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_ror_w_ri,(RW2 r, IMM i))
{
	RORWir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_ror_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,READ,2,raw_or_l_rm,(RW4 d, MEMR s))
{
	ORLmr(s, X86_NOREG, X86_NOREG, 1, d);
}
LENDFUNC(WRITE,READ,2,raw_or_l_rm,(RW4 d, MEMR s))

LOWFUNC(WRITE,NONE,2,raw_ror_l_ri,(RW4 r, IMM i))
{
	RORLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_ror_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_ror_l_rr,(RW4 d, R1 r))
{
	RORLrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_w_rr,(RW2 d, R1 r))
{
	RORWrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_b_rr,(RW1 d, R1 r))
{
	RORBrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_l_rr,(RW4 d, R1 r))
{
	SHRLrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_w_rr,(RW2 d, R1 r))
{
	SHRWrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_b_rr,(RW1 d, R1 r))
{
	SHRBrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_l_rr,(RW4 d, R1 r))
{
	SARLrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_w_rr,(RW2 d, R1 r))
{
	SARWrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_b_rr,(RW1 d, R1 r))
{
	SARBrr(r, d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_l_ri,(RW4 r, IMM i))
{
	SHLLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shll_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shll_w_ri,(RW2 r, IMM i))
{
	SHLWir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shll_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shll_b_ri,(RW1 r, IMM i))
{
	SHLBir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shll_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_l_ri,(RW4 r, IMM i))
{
	SHRLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_w_ri,(RW2 r, IMM i))
{
	SHRWir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_b_ri,(RW1 r, IMM i))
{
	SHRBir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_l_ri,(RW4 r, IMM i))
{
	SARLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shra_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_w_ri,(RW2 r, IMM i))
{
	SARWir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shra_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_b_ri,(RW1 r, IMM i))
{
	SARBir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_shra_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,1,raw_sahf,(R2 dummy_ah))
{
	SAHF();
}
LENDFUNC(WRITE,NONE,1,raw_sahf,(R2 dummy_ah))

LOWFUNC(NONE,NONE,1,raw_cpuid,(R4 dummy_eax))
{
	CPUID();
}
LENDFUNC(NONE,NONE,1,raw_cpuid,(R4 dummy_eax))

LOWFUNC(READ,NONE,1,raw_lahf,(W2 dummy_ah))
{
	LAHF();
}
LENDFUNC(READ,NONE,1,raw_lahf,(W2 dummy_ah))

LOWFUNC(READ,NONE,2,raw_setcc,(W1 d, IMM cc))
{
	SETCCir(cc, d);
}
LENDFUNC(READ,NONE,2,raw_setcc,(W1 d, IMM cc))

LOWFUNC(READ,WRITE,2,raw_setcc_m,(MEMW d, IMM cc))
{
	SETCCim(cc, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(READ,WRITE,2,raw_setcc_m,(MEMW d, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_b_rr,(RW1 d, R1 s, IMM cc))
{
	/* replacement using branch and mov */
	int8 *target_p = (int8 *)x86_get_target() + 1;
	JCCSii(cc^1, 0);
	MOVBrr(s, d);
	*target_p = (uintptr)x86_get_target() - ((uintptr)target_p + 1);
}
LENDFUNC(READ,NONE,3,raw_cmov_b_rr,(RW1 d, R1 s, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_w_rr,(RW2 d, R2 s, IMM cc))
{
	if (have_cmov)
		CMOVWrr(cc, s, d);
	else { /* replacement using branch and mov */
		int8 *target_p = (int8 *)x86_get_target() + 1;
		JCCSii(cc^1, 0);
		MOVWrr(s, d);
	    *target_p = (uintptr)x86_get_target() - ((uintptr)target_p + 1);
	}
}
LENDFUNC(READ,NONE,3,raw_cmov_w_rr,(RW2 d, R2 s, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_l_rr,(RW4 d, R4 s, IMM cc))
{
	if (have_cmov)
		CMOVLrr(cc, s, d);
	else { /* replacement using branch and mov */
		int8 *target_p = (int8 *)x86_get_target() + 1;
		JCCSii(cc^1, 0);
		MOVLrr(s, d);
	    *target_p = (uintptr)x86_get_target() - ((uintptr)target_p + 1);
	}
}
LENDFUNC(READ,NONE,3,raw_cmov_l_rr,(RW4 d, R4 s, IMM cc))

LOWFUNC(WRITE,NONE,2,raw_bsf_l_rr,(W4 d, R4 s))
{
	BSFLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_bsf_l_rr,(W4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_sign_extend_32_rr,(W4 d, R4 s))
{
	MOVSLQrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_sign_extend_32_rr,(W4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_sign_extend_16_rr,(W4 d, R2 s))
{
	MOVSWLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_sign_extend_16_rr,(W4 d, R2 s))

LOWFUNC(NONE,NONE,2,raw_sign_extend_8_rr,(W4 d, R1 s))
{
	MOVSBLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_sign_extend_8_rr,(W4 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_zero_extend_16_rr,(W4 d, R2 s))
{
	MOVZWLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_zero_extend_16_rr,(W4 d, R2 s))

LOWFUNC(NONE,NONE,2,raw_zero_extend_8_rr,(W4 d, R1 s))
{
	MOVZBLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_zero_extend_8_rr,(W4 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_imul_32_32,(RW4 d, R4 s))
{
	IMULLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_imul_32_32,(RW4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_imul_64_32,(RW4 d, RW4 s))
{
	if (d!=MUL_NREG1 || s!=MUL_NREG2) {
	write_log("Bad register in IMUL: d=%d, s=%d\n",d,s);
	abort();
	}
	IMULLr(s);
}
LENDFUNC(NONE,NONE,2,raw_imul_64_32,(RW4 d, RW4 s))

LOWFUNC(NONE,NONE,2,raw_mul_64_32,(RW4 d, RW4 s))
{
	if (d!=MUL_NREG1 || s!=MUL_NREG2) {
	write_log("Bad register in MUL: d=%d, s=%d\n",d,s);
	abort();
	}
	MULLr(s);
}
LENDFUNC(NONE,NONE,2,raw_mul_64_32,(RW4 d, RW4 s))

LOWFUNC(NONE,NONE,2,raw_mul_32_32,(RW4 d, R4 s))
{
	abort(); /* %^$&%^$%#^ x86! */
}
LENDFUNC(NONE,NONE,2,raw_mul_32_32,(RW4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_mov_b_rr,(W1 d, R1 s))
{
	MOVBrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_b_rr,(W1 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_mov_w_rr,(W2 d, R2 s))
{
	MOVWrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_w_rr,(W2 d, R2 s))

LOWFUNC(NONE,READ,4,raw_mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVLmr(0, baser, index, factor, d);
}
LENDFUNC(NONE,READ,4,raw_mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVWmr(0, baser, index, factor, d);
}
LENDFUNC(NONE,READ,4,raw_mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVBmr(0, baser, index, factor, d);
}
LENDFUNC(NONE,READ,4,raw_mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,WRITE,4,raw_mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))
{
	ADDR32 MOVLrm(s, 0, baser, index, factor);
}
LENDFUNC(NONE,WRITE,4,raw_mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))

LOWFUNC(NONE,WRITE,4,raw_mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))
{
	ADDR32 MOVWrm(s, 0, baser, index, factor);
}
LENDFUNC(NONE,WRITE,4,raw_mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))

LOWFUNC(NONE,WRITE,4,raw_mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))
{
	ADDR32 MOVBrm(s, 0, baser, index, factor);
}
LENDFUNC(NONE,WRITE,4,raw_mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))

LOWFUNC(NONE,WRITE,5,raw_mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))
{
	ADDR32 MOVLrm(s, base, baser, index, factor);
}
LENDFUNC(NONE,WRITE,5,raw_mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))

LOWFUNC(NONE,WRITE,5,raw_mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))
{
	ADDR32 MOVWrm(s, base, baser, index, factor);
}
LENDFUNC(NONE,WRITE,5,raw_mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))

LOWFUNC(NONE,WRITE,5,raw_mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))
{
	ADDR32 MOVBrm(s, base, baser, index, factor);
}
LENDFUNC(NONE,WRITE,5,raw_mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))

LOWFUNC(NONE,READ,5,raw_mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVLmr(base, baser, index, factor, d);
}
LENDFUNC(NONE,READ,5,raw_mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVWmr(base, baser, index, factor, d);
}
LENDFUNC(NONE,READ,5,raw_mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))
{
	ADDR32 MOVBmr(base, baser, index, factor, d);
}
LENDFUNC(NONE,READ,5,raw_mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))
{
	ADDR32 MOVLmr(base, X86_NOREG, index, factor, d);
}
LENDFUNC(NONE,READ,4,raw_mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_cmov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor, IMM cond))
{
	if (have_cmov)
		ADDR32 CMOVLmr(cond, base, X86_NOREG, index, factor, d);
	else { /* replacement using branch and mov */
		int8 *target_p = (int8 *)x86_get_target() + 1;
		JCCSii(cond^1, 0);
		ADDR32 MOVLmr(base, X86_NOREG, index, factor, d);
	    *target_p = (uintptr)x86_get_target() - ((uintptr)target_p + 1);
	}
}
LENDFUNC(NONE,READ,5,raw_cmov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor, IMM cond))

LOWFUNC(NONE,READ,3,raw_cmov_l_rm,(W4 d, IMM mem, IMM cond))
{
	if (have_cmov)
		CMOVLmr(cond, mem, X86_NOREG, X86_NOREG, 1, d);
	else { /* replacement using branch and mov */
		int8 *target_p = (int8 *)x86_get_target() + 1;
		JCCSii(cond^1, 0);
		MOVLmr(mem, X86_NOREG, X86_NOREG, 1, d);
	    *target_p = (uintptr)x86_get_target() - ((uintptr)target_p + 1);
	}
}
LENDFUNC(NONE,READ,3,raw_cmov_l_rm,(W4 d, IMM mem, IMM cond))

LOWFUNC(NONE,READ,3,raw_mov_l_rR,(W4 d, R4 s, IMM offset))
{
	ADDR32 MOVLmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_l_rR,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_w_rR,(W2 d, R4 s, IMM offset))
{
	ADDR32 MOVWmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_w_rR,(W2 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_b_rR,(W1 d, R4 s, IMM offset))
{
	ADDR32 MOVBmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_b_rR,(W1 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_l_brR,(W4 d, R4 s, IMM offset))
{
	ADDR32 MOVLmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_l_brR,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_w_brR,(W2 d, R4 s, IMM offset))
{
	ADDR32 MOVWmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_w_brR,(W2 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_b_brR,(W1 d, R4 s, IMM offset))
{
	ADDR32 MOVBmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,3,raw_mov_b_brR,(W1 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_l_Ri,(R4 d, IMM i, IMM offset))
{
	ADDR32 MOVLim(i, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_Ri,(R4 d, IMM i, IMM offset))
{
	ADDR32 MOVWim(i, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_Ri,(R4 d, IMM i, IMM offset))
{
	ADDR32 MOVBim(i, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_l_Rr,(R4 d, R4 s, IMM offset))
{
	ADDR32 MOVLrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_Rr,(R4 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_Rr,(R4 d, R2 s, IMM offset))
{
	ADDR32 MOVWrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_Rr,(R4 d, R2 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_Rr,(R4 d, R1 s, IMM offset))
{
	ADDR32 MOVBrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_Rr,(R4 d, R1 s, IMM offset))

LOWFUNC(NONE,NONE,3,raw_lea_l_brr,(W4 d, R4 s, IMM offset))
{
	LEALmr(offset, s, X86_NOREG, 1, d);
}
LENDFUNC(NONE,NONE,3,raw_lea_l_brr,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,NONE,5,raw_lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))
{
	LEALmr(offset, s, index, factor, d);
}
LENDFUNC(NONE,NONE,5,raw_lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))

LOWFUNC(NONE,NONE,4,raw_lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))
{
	LEALmr(0, s, index, factor, d);
}
LENDFUNC(NONE,NONE,4,raw_lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))

LOWFUNC(NONE,NONE,4,raw_lea_l_r_scaled,(W4 d, R4 index, IMM factor))
{
	LEALmr(0, X86_NOREG, index, factor, d);
}
LENDFUNC(NONE,NONE,4,raw_lea_l_r_scaled,(W4 d, R4 index, IMM factor))

LOWFUNC(NONE,WRITE,3,raw_mov_l_bRr,(R4 d, R4 s, IMM offset))
{
	ADDR32 MOVLrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_bRr,(R4 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_bRr,(R4 d, R2 s, IMM offset))
{
	ADDR32 MOVWrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_bRr,(R4 d, R2 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_bRr,(R4 d, R1 s, IMM offset))
{
	ADDR32 MOVBrm(s, offset, d, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_bRr,(R4 d, R1 s, IMM offset))

LOWFUNC(NONE,NONE,1,raw_bswap_32,(RW4 r))
{
	BSWAPLr(r);
}
LENDFUNC(NONE,NONE,1,raw_bswap_32,(RW4 r))

LOWFUNC(WRITE,NONE,1,raw_bswap_16,(RW2 r))
{
	ROLWir(8, r);
}
LENDFUNC(WRITE,NONE,1,raw_bswap_16,(RW2 r))

LOWFUNC(NONE,NONE,2,raw_mov_l_rr,(W4 d, R4 s))
{
	MOVLrr(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_l_rr,(W4 d, R4 s))

LOWFUNC(NONE,WRITE,2,raw_mov_l_mr,(IMM d, R4 s))
{
	MOVLrm(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_l_mr,(IMM d, R4 s))

LOWFUNC(NONE,WRITE,2,raw_mov_w_mr,(IMM d, R2 s))
{
	MOVWrm(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_w_mr,(IMM d, R2 s))

LOWFUNC(NONE,READ,2,raw_mov_w_rm,(W2 d, IMM s))
{
	MOVWmr(s, X86_NOREG, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,2,raw_mov_w_rm,(W2 d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_b_mr,(IMM d, R1 s))
{
	MOVBrm(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(NONE,WRITE,2,raw_mov_b_mr,(IMM d, R1 s))

LOWFUNC(NONE,READ,2,raw_mov_b_rm,(W1 d, IMM s))
{
	MOVBmr(s, X86_NOREG, X86_NOREG, 1, d);
}
LENDFUNC(NONE,READ,2,raw_mov_b_rm,(W1 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_l_ri,(W4 d, IMM s))
{
	MOVLir(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_l_ri,(W4 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_w_ri,(W2 d, IMM s))
{
	MOVWir(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_w_ri,(W2 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_b_ri,(W1 d, IMM s))
{
	MOVBir(s, d);
}
LENDFUNC(NONE,NONE,2,raw_mov_b_ri,(W1 d, IMM s))

LOWFUNC(RMW,RMW,2,raw_adc_l_mi,(MEMRW d, IMM s))
{
	ADCLim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(RMW,RMW,2,raw_adc_l_mi,(MEMRW d, IMM s))

LOWFUNC(WRITE,RMW,2,raw_add_l_mi,(IMM d, IMM s)) 
{
	ADDLim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,RMW,2,raw_add_l_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,RMW,2,raw_add_w_mi,(IMM d, IMM s)) 
{
	ADDWim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,RMW,2,raw_add_w_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,RMW,2,raw_add_b_mi,(IMM d, IMM s)) 
{
	ADDBim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,RMW,2,raw_add_b_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,NONE,2,raw_test_l_ri,(R4 d, IMM i))
{
	TESTLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_test_l_ri,(R4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_test_l_rr,(R4 d, R4 s))
{
	TESTLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_test_l_rr,(R4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_test_w_rr,(R2 d, R2 s))
{
	TESTWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_test_w_rr,(R2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_test_b_rr,(R1 d, R1 s))
{
	TESTBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_test_b_rr,(R1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_xor_l_ri,(RW4 d, IMM i))
{
	XORLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_l_ri,(RW4 d, IMM i))
{
	ANDLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_and_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_w_ri,(RW2 d, IMM i))
{
	ANDWir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_and_w_ri,(RW2 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_l,(RW4 d, R4 s))
{
	ANDLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_and_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_and_w,(RW2 d, R2 s))
{
	ANDWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_and_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_and_b,(RW1 d, R1 s))
{
	ANDBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_and_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_or_l_ri,(RW4 d, IMM i))
{
	ORLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_or_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_or_l,(RW4 d, R4 s))
{
	ORLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_or_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_or_w,(RW2 d, R2 s))
{
	ORWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_or_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_or_b,(RW1 d, R1 s))
{
	ORBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_or_b,(RW1 d, R1 s))

LOWFUNC(RMW,NONE,2,raw_adc_l,(RW4 d, R4 s))
{
	ADCLrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_adc_l,(RW4 d, R4 s))

LOWFUNC(RMW,NONE,2,raw_adc_w,(RW2 d, R2 s))
{
	ADCWrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_adc_w,(RW2 d, R2 s))

LOWFUNC(RMW,NONE,2,raw_adc_b,(RW1 d, R1 s))
{
	ADCBrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_adc_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_add_l,(RW4 d, R4 s))
{
	ADDLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_add_w,(RW2 d, R2 s))
{
	ADDWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_add_b,(RW1 d, R1 s))
{
	ADDBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_sub_l_ri,(RW4 d, IMM i))
{
	SUBLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_sub_b_ri,(RW1 d, IMM i))
{
	SUBBir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_b_ri,(RW1 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_l_ri,(RW4 d, IMM i))
{
	ADDLir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_w_ri,(RW2 d, IMM i))
{
	ADDWir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_w_ri,(RW2 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_b_ri,(RW1 d, IMM i))
{
	ADDBir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_add_b_ri,(RW1 d, IMM i))

LOWFUNC(RMW,NONE,2,raw_sbb_l,(RW4 d, R4 s))
{
	SBBLrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_l,(RW4 d, R4 s))

LOWFUNC(RMW,NONE,2,raw_sbb_w,(RW2 d, R2 s))
{
	SBBWrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_w,(RW2 d, R2 s))

LOWFUNC(RMW,NONE,2,raw_sbb_b,(RW1 d, R1 s))
{
	SBBBrr(s, d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_sub_l,(RW4 d, R4 s))
{
	SUBLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_sub_w,(RW2 d, R2 s))
{
	SUBWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_sub_b,(RW1 d, R1 s))
{
	SUBBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_cmp_l,(R4 d, R4 s))
{
	CMPLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_l,(R4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_cmp_l_ri,(R4 r, IMM i))
{
	CMPLir(i, r);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_l_ri,(R4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_cmp_w,(R2 d, R2 s))
{
	CMPWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_w,(R2 d, R2 s))

LOWFUNC(WRITE,READ,2,raw_cmp_b_mi,(MEMR d, IMM s))
{
	CMPBim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))

LOWFUNC(WRITE,NONE,2,raw_cmp_b_ri,(R1 d, IMM i))
{
	CMPBir(i, d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_b_ri,(R1 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_cmp_b,(R1 d, R1 s))
{
	CMPBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_b,(R1 d, R1 s))

LOWFUNC(WRITE,READ,4,raw_cmp_l_rm_indexed,(R4 d, IMM offset, R4 index, IMM factor))
{
	ADDR32 CMPLmr(offset, X86_NOREG, index, factor, d);
}
LENDFUNC(WRITE,READ,4,raw_cmp_l_rm_indexed,(R4 d, IMM offset, R4 index, IMM factor))

LOWFUNC(WRITE,NONE,2,raw_xor_l,(RW4 d, R4 s))
{
	XORLrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_xor_w,(RW2 d, R2 s))
{
	XORWrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_xor_b,(RW1 d, R1 s))
{
	XORBrr(s, d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_b,(RW1 d, R1 s))

LOWFUNC(WRITE,RMW,2,raw_sub_l_mi,(MEMRW d, IMM s))
{
	SUBLim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,RMW,2,raw_sub_l_mi,(MEMRW d, IMM s))

LOWFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))
{
	CMPLim(s, d, X86_NOREG, X86_NOREG, 1);
}
LENDFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))

LOWFUNC(NONE,NONE,2,raw_xchg_l_rr,(RW4 r1, RW4 r2))
{
	XCHGLrr(r2, r1);
}
LENDFUNC(NONE,NONE,2,raw_xchg_l_rr,(RW4 r1, RW4 r2))

LOWFUNC(NONE,NONE,2,raw_xchg_b_rr,(RW4 r1, RW4 r2))
{
	XCHGBrr(r2, r1);
}
LENDFUNC(NONE,NONE,2,raw_xchg_b_rr,(RW4 r1, RW4 r2))

LOWFUNC(READ,WRITE,0,raw_pushfl,(void))
{
	PUSHF();
}
LENDFUNC(READ,WRITE,0,raw_pushfl,(void))

LOWFUNC(WRITE,READ,0,raw_popfl,(void))
{
	POPF();
}
LENDFUNC(WRITE,READ,0,raw_popfl,(void))

/* Generate floating-point instructions */
static inline void x86_fadd_m(MEMR s)
{
	FADDLm(s,X86_NOREG,X86_NOREG,1);
}

#else

const bool optimize_accum		= true;
const bool optimize_imm8		= true;
const bool optimize_shift_once	= true;

/*************************************************************************
 * Actual encoding of the instructions on the target CPU                 *
 *************************************************************************/

static __inline__ int isaccum(int r)
{
	return (r == EAX_INDEX);
}

static __inline__ int isbyte(uae_s32 x)
{
	return (x>=-128 && x<=127);
}

static __inline__ int isword(uae_s32 x)
{
	return (x>=-32768 && x<=32767);
}

LOWFUNC(NONE,WRITE,1,raw_push_l_r,(R4 r))
{
	emit_byte(0x50+r);
}
LENDFUNC(NONE,WRITE,1,raw_push_l_r,(R4 r))

LOWFUNC(NONE,READ,1,raw_pop_l_r,(R4 r))
{
	emit_byte(0x58+r);
}
LENDFUNC(NONE,READ,1,raw_pop_l_r,(R4 r))

LOWFUNC(NONE,READ,1,raw_pop_l_m,(MEMW d))
{
	emit_byte(0x8f);
	emit_byte(0x05);
	emit_long(d);
}
LENDFUNC(NONE,READ,1,raw_pop_l_m,(MEMW d))

LOWFUNC(WRITE,NONE,2,raw_bt_l_ri,(R4 r, IMM i))
{
	emit_byte(0x0f);
	emit_byte(0xba);
	emit_byte(0xe0+r);
	emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_bt_l_ri,(R4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_bt_l_rr,(R4 r, R4 b))
{
	emit_byte(0x0f);
	emit_byte(0xa3);
	emit_byte(0xc0+8*b+r);
}
LENDFUNC(WRITE,NONE,2,raw_bt_l_rr,(R4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_btc_l_ri,(RW4 r, IMM i))
{
	emit_byte(0x0f);
	emit_byte(0xba);
	emit_byte(0xf8+r);
	emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_btc_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_btc_l_rr,(RW4 r, R4 b))
{
	emit_byte(0x0f);
	emit_byte(0xbb);
	emit_byte(0xc0+8*b+r);
}
LENDFUNC(WRITE,NONE,2,raw_btc_l_rr,(RW4 r, R4 b))


LOWFUNC(WRITE,NONE,2,raw_btr_l_ri,(RW4 r, IMM i))
{
	emit_byte(0x0f);
	emit_byte(0xba);
	emit_byte(0xf0+r);
	emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_btr_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_btr_l_rr,(RW4 r, R4 b))
{
	emit_byte(0x0f);
	emit_byte(0xb3);
	emit_byte(0xc0+8*b+r);
}
LENDFUNC(WRITE,NONE,2,raw_btr_l_rr,(RW4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_bts_l_ri,(RW4 r, IMM i))
{
	emit_byte(0x0f);
	emit_byte(0xba);
	emit_byte(0xe8+r);
	emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_bts_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_bts_l_rr,(RW4 r, R4 b))
{
	emit_byte(0x0f);
	emit_byte(0xab);
	emit_byte(0xc0+8*b+r);
}
LENDFUNC(WRITE,NONE,2,raw_bts_l_rr,(RW4 r, R4 b))

LOWFUNC(WRITE,NONE,2,raw_sub_w_ri,(RW2 d, IMM i))
{
    emit_byte(0x66);
    if (isbyte(i)) {
	emit_byte(0x83);
	emit_byte(0xe8+d);
	emit_byte(i);
    }
    else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x2d);
	else {
	emit_byte(0x81);
	emit_byte(0xe8+d);
	}
	emit_word(i);
    }
}
LENDFUNC(WRITE,NONE,2,raw_sub_w_ri,(RW2 d, IMM i))


LOWFUNC(NONE,READ,2,raw_mov_l_rm,(W4 d, MEMR s))
{
    emit_byte(0x8b);
    emit_byte(0x05+8*d);
    emit_long(s);
}
LENDFUNC(NONE,READ,2,raw_mov_l_rm,(W4 d, MEMR s))

LOWFUNC(NONE,WRITE,2,raw_mov_l_mi,(MEMW d, IMM s))
{
    emit_byte(0xc7);
    emit_byte(0x05);
    emit_long(d);
    emit_long(s);
}
LENDFUNC(NONE,WRITE,2,raw_mov_l_mi,(MEMW d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_w_mi,(MEMW d, IMM s))
{
    emit_byte(0x66);
    emit_byte(0xc7);
    emit_byte(0x05);
    emit_long(d);
    emit_word(s);
}
LENDFUNC(NONE,WRITE,2,raw_mov_w_mi,(MEMW d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_b_mi,(MEMW d, IMM s))
{
    emit_byte(0xc6);
    emit_byte(0x05);
    emit_long(d);
    emit_byte(s);
}
LENDFUNC(NONE,WRITE,2,raw_mov_b_mi,(MEMW d, IMM s))

LOWFUNC(WRITE,RMW,2,raw_rol_b_mi,(MEMRW d, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
	emit_byte(0xd0);
	emit_byte(0x05);
	emit_long(d);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0x05);
    emit_long(d);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,RMW,2,raw_rol_b_mi,(MEMRW d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_b_ri,(RW1 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd0);
    emit_byte(0xc0+r);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0xc0+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_rol_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_w_ri,(RW2 r, IMM i))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xc0+r);
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_rol_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_l_ri,(RW4 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd1);
    emit_byte(0xc0+r);
	}
	else {
    emit_byte(0xc1);
    emit_byte(0xc0+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_rol_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_rol_l_rr,(RW4 d, R1 r))
{
    emit_byte(0xd3);
    emit_byte(0xc0+d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_rol_w_rr,(RW2 d, R1 r))
{
    emit_byte(0x66);
    emit_byte(0xd3);
    emit_byte(0xc0+d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_rol_b_rr,(RW1 d, R1 r))
{
    emit_byte(0xd2);
    emit_byte(0xc0+d);
}
LENDFUNC(WRITE,NONE,2,raw_rol_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_l_rr,(RW4 d, R1 r))
{
    emit_byte(0xd3);
    emit_byte(0xe0+d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_w_rr,(RW2 d, R1 r))
{
    emit_byte(0x66);
    emit_byte(0xd3);
    emit_byte(0xe0+d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_b_rr,(RW1 d, R1 r))
{
    emit_byte(0xd2);
    emit_byte(0xe0+d);
}
LENDFUNC(WRITE,NONE,2,raw_shll_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_b_ri,(RW1 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd0);
    emit_byte(0xc8+r);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0xc8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_ror_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_ror_w_ri,(RW2 r, IMM i))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xc8+r);
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_ror_w_ri,(RW2 r, IMM i))

// gb-- used for making an fpcr value in compemu_fpp.cpp
LOWFUNC(WRITE,READ,2,raw_or_l_rm,(RW4 d, MEMR s))
{
    emit_byte(0x0b);
    emit_byte(0x05+8*d);
    emit_long(s);
}
LENDFUNC(WRITE,READ,2,raw_or_l_rm,(RW4 d, MEMR s))

LOWFUNC(WRITE,NONE,2,raw_ror_l_ri,(RW4 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd1);
    emit_byte(0xc8+r);
	}
	else {
    emit_byte(0xc1);
    emit_byte(0xc8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_ror_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_ror_l_rr,(RW4 d, R1 r))
{
    emit_byte(0xd3);
    emit_byte(0xc8+d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_w_rr,(RW2 d, R1 r))
{
    emit_byte(0x66);
    emit_byte(0xd3);
    emit_byte(0xc8+d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_ror_b_rr,(RW1 d, R1 r))
{
    emit_byte(0xd2);
    emit_byte(0xc8+d);
}
LENDFUNC(WRITE,NONE,2,raw_ror_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_l_rr,(RW4 d, R1 r))
{
    emit_byte(0xd3);
    emit_byte(0xe8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_w_rr,(RW2 d, R1 r))
{
    emit_byte(0x66);
    emit_byte(0xd3);
    emit_byte(0xe8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shrl_b_rr,(RW1 d, R1 r))
{
    emit_byte(0xd2);
    emit_byte(0xe8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_l_rr,(RW4 d, R1 r))
{
    emit_byte(0xd3);
    emit_byte(0xf8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_l_rr,(RW4 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_w_rr,(RW2 d, R1 r))
{
    emit_byte(0x66);
    emit_byte(0xd3);
    emit_byte(0xf8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_w_rr,(RW2 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shra_b_rr,(RW1 d, R1 r))
{
    emit_byte(0xd2);
    emit_byte(0xf8+d);
}
LENDFUNC(WRITE,NONE,2,raw_shra_b_rr,(RW1 d, R1 r))

LOWFUNC(WRITE,NONE,2,raw_shll_l_ri,(RW4 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd1);
    emit_byte(0xe0+r);
	}
	else {
    emit_byte(0xc1);
    emit_byte(0xe0+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shll_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shll_w_ri,(RW2 r, IMM i))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xe0+r);
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_shll_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shll_b_ri,(RW1 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd0);
    emit_byte(0xe0+r);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0xe0+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shll_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_l_ri,(RW4 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd1);
    emit_byte(0xe8+r);
	}
	else {
    emit_byte(0xc1);
    emit_byte(0xe8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shrl_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_w_ri,(RW2 r, IMM i))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xe8+r);
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_shrl_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shrl_b_ri,(RW1 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd0);
    emit_byte(0xe8+r);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0xe8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shrl_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_l_ri,(RW4 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd1);
    emit_byte(0xf8+r);
	}
	else {
    emit_byte(0xc1);
    emit_byte(0xf8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shra_l_ri,(RW4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_w_ri,(RW2 r, IMM i))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xf8+r);
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_shra_w_ri,(RW2 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_shra_b_ri,(RW1 r, IMM i))
{
	if (optimize_shift_once && (i == 1)) {
    emit_byte(0xd0);
    emit_byte(0xf8+r);
	}
	else {
    emit_byte(0xc0);
    emit_byte(0xf8+r);
    emit_byte(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_shra_b_ri,(RW1 r, IMM i))

LOWFUNC(WRITE,NONE,1,raw_sahf,(R2 dummy_ah))
{
    emit_byte(0x9e);
}
LENDFUNC(WRITE,NONE,1,raw_sahf,(R2 dummy_ah))

LOWFUNC(NONE,NONE,1,raw_cpuid,(R4 dummy_eax))
{
    emit_byte(0x0f);
    emit_byte(0xa2);
}
LENDFUNC(NONE,NONE,1,raw_cpuid,(R4 dummy_eax))

LOWFUNC(READ,NONE,1,raw_lahf,(W2 dummy_ah))
{
    emit_byte(0x9f);
}
LENDFUNC(READ,NONE,1,raw_lahf,(W2 dummy_ah))

LOWFUNC(READ,NONE,2,raw_setcc,(W1 d, IMM cc))
{
    emit_byte(0x0f);
    emit_byte(0x90+cc);
    emit_byte(0xc0+d);
}
LENDFUNC(READ,NONE,2,raw_setcc,(W1 d, IMM cc))

LOWFUNC(READ,WRITE,2,raw_setcc_m,(MEMW d, IMM cc))
{
    emit_byte(0x0f);
    emit_byte(0x90+cc);
    emit_byte(0x05);
    emit_long(d);
}
LENDFUNC(READ,WRITE,2,raw_setcc_m,(MEMW d, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_b_rr,(RW1 d, R1 s, IMM cc))
{
	/* replacement using branch and mov */
	int uncc=(cc^1);
	emit_byte(0x70+uncc); 
	emit_byte(3);  /* skip next 2 bytes if not cc=true */
    emit_byte(0x88);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(READ,NONE,3,raw_cmov_b_rr,(RW1 d, R1 s, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_w_rr,(RW2 d, R2 s, IMM cc))
{
    if (have_cmov) {
	emit_byte(0x66);
	emit_byte(0x0f);
	emit_byte(0x40+cc);
	emit_byte(0xc0+8*d+s);
    }
    else { /* replacement using branch and mov */
	int uncc=(cc^1);
	emit_byte(0x70+uncc); 
	emit_byte(3);  /* skip next 3 bytes if not cc=true */
	emit_byte(0x66);
	emit_byte(0x89);
	emit_byte(0xc0+8*s+d);
    }
}
LENDFUNC(READ,NONE,3,raw_cmov_w_rr,(RW2 d, R2 s, IMM cc))

LOWFUNC(READ,NONE,3,raw_cmov_l_rr,(RW4 d, R4 s, IMM cc))
{
    if (have_cmov) {
	emit_byte(0x0f);
	emit_byte(0x40+cc);
	emit_byte(0xc0+8*d+s);
    }
    else { /* replacement using branch and mov */
	int uncc=(cc^1);
	emit_byte(0x70+uncc); 
	emit_byte(2);  /* skip next 2 bytes if not cc=true */
	emit_byte(0x89);
	emit_byte(0xc0+8*s+d);
    }
}
LENDFUNC(READ,NONE,3,raw_cmov_l_rr,(RW4 d, R4 s, IMM cc))

LOWFUNC(WRITE,NONE,2,raw_bsf_l_rr,(W4 d, R4 s))
{
    emit_byte(0x0f);
    emit_byte(0xbc);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(WRITE,NONE,2,raw_bsf_l_rr,(W4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_sign_extend_16_rr,(W4 d, R2 s))
{
    emit_byte(0x0f);
    emit_byte(0xbf);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_sign_extend_16_rr,(W4 d, R2 s))

LOWFUNC(NONE,NONE,2,raw_sign_extend_8_rr,(W4 d, R1 s))
{
    emit_byte(0x0f);
    emit_byte(0xbe);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_sign_extend_8_rr,(W4 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_zero_extend_16_rr,(W4 d, R2 s))
{
    emit_byte(0x0f);
    emit_byte(0xb7);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_zero_extend_16_rr,(W4 d, R2 s))

LOWFUNC(NONE,NONE,2,raw_zero_extend_8_rr,(W4 d, R1 s))
{
    emit_byte(0x0f);
    emit_byte(0xb6);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_zero_extend_8_rr,(W4 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_imul_32_32,(RW4 d, R4 s))
{
    emit_byte(0x0f);
    emit_byte(0xaf);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_imul_32_32,(RW4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_imul_64_32,(RW4 d, RW4 s))
{
    if (d!=MUL_NREG1 || s!=MUL_NREG2)
	abort();
    emit_byte(0xf7);
    emit_byte(0xea);
}
LENDFUNC(NONE,NONE,2,raw_imul_64_32,(RW4 d, RW4 s))

LOWFUNC(NONE,NONE,2,raw_mul_64_32,(RW4 d, RW4 s))
{
    if (d!=MUL_NREG1 || s!=MUL_NREG2) {
	printf("Bad register in MUL: d=%d, s=%d\n",d,s);
	abort();
    }
    emit_byte(0xf7);
    emit_byte(0xe2);
}
LENDFUNC(NONE,NONE,2,raw_mul_64_32,(RW4 d, RW4 s))

LOWFUNC(NONE,NONE,2,raw_mul_32_32,(RW4 d, R4 s))
{
    abort(); /* %^$&%^$%#^ x86! */
    emit_byte(0x0f);
    emit_byte(0xaf);
    emit_byte(0xc0+8*d+s);
}
LENDFUNC(NONE,NONE,2,raw_mul_32_32,(RW4 d, R4 s))

LOWFUNC(NONE,NONE,2,raw_mov_b_rr,(W1 d, R1 s))
{
    emit_byte(0x88);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(NONE,NONE,2,raw_mov_b_rr,(W1 d, R1 s))

LOWFUNC(NONE,NONE,2,raw_mov_w_rr,(W2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(NONE,NONE,2,raw_mov_w_rr,(W2 d, R2 s))

LOWFUNC(NONE,READ,4,raw_mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))
{
    int isebp=(baser==5)?0x40:0;
    int fi;
    
    switch(factor) {
     case 1: fi=0; break;
     case 2: fi=1; break;
     case 4: fi=2; break;
     case 8: fi=3; break;
     default: abort();
    }


    emit_byte(0x8b);
    emit_byte(0x04+8*d+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,READ,4,raw_mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))
{
    int fi;
    int isebp;
    
    switch(factor) {
     case 1: fi=0; break;
     case 2: fi=1; break;
     case 4: fi=2; break;
     case 8: fi=3; break;
     default: abort();
    }
    isebp=(baser==5)?0x40:0;
    
    emit_byte(0x66);
    emit_byte(0x8b);
    emit_byte(0x04+8*d+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,READ,4,raw_mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))
{
   int fi;
  int isebp;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }
  isebp=(baser==5)?0x40:0;

   emit_byte(0x8a);
    emit_byte(0x04+8*d+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,READ,4,raw_mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,WRITE,4,raw_mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))
{
  int fi;
  int isebp;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

  
  isebp=(baser==5)?0x40:0;

    emit_byte(0x89);
    emit_byte(0x04+8*s+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,WRITE,4,raw_mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))

LOWFUNC(NONE,WRITE,4,raw_mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))
{
  int fi;
  int isebp;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }
  isebp=(baser==5)?0x40:0;

    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0x04+8*s+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,WRITE,4,raw_mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))

LOWFUNC(NONE,WRITE,4,raw_mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))
{
  int fi;
  int isebp;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }
  isebp=(baser==5)?0x40:0;

    emit_byte(0x88);
    emit_byte(0x04+8*s+isebp);
    emit_byte(baser+8*index+0x40*fi);
    if (isebp)
	emit_byte(0x00);
}
LENDFUNC(NONE,WRITE,4,raw_mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))

LOWFUNC(NONE,WRITE,5,raw_mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x89);
    emit_byte(0x84+8*s);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,WRITE,5,raw_mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))

LOWFUNC(NONE,WRITE,5,raw_mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0x84+8*s);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,WRITE,5,raw_mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))

LOWFUNC(NONE,WRITE,5,raw_mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x88);
    emit_byte(0x84+8*s);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,WRITE,5,raw_mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))

LOWFUNC(NONE,READ,5,raw_mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x8b);
    emit_byte(0x84+8*d);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,READ,5,raw_mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x66);
    emit_byte(0x8b);
    emit_byte(0x84+8*d);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,READ,5,raw_mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))
{
  int fi;

  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x8a);
    emit_byte(0x84+8*d);
    emit_byte(baser+8*index+0x40*fi);
    emit_long(base);
}
LENDFUNC(NONE,READ,5,raw_mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))

LOWFUNC(NONE,READ,4,raw_mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))
{
  int fi;
  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: 
    fprintf(stderr,"Bad factor %d in mov_l_rm_indexed!\n",factor);
    abort();
  }
    emit_byte(0x8b);
    emit_byte(0x04+8*d);
    emit_byte(0x05+8*index+64*fi);
    emit_long(base);
}
LENDFUNC(NONE,READ,4,raw_mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))

LOWFUNC(NONE,READ,5,raw_cmov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor, IMM cond))
{
    int fi;
    switch(factor) {
     case 1: fi=0; break;
     case 2: fi=1; break;
     case 4: fi=2; break;
     case 8: fi=3; break;
     default: 
	fprintf(stderr,"Bad factor %d in mov_l_rm_indexed!\n",factor);
	abort();
    }
    if (have_cmov) {
	emit_byte(0x0f);
	emit_byte(0x40+cond);
	emit_byte(0x04+8*d);
	emit_byte(0x05+8*index+64*fi);
	emit_long(base);
    }
    else { /* replacement using branch and mov */
	int uncc=(cond^1);
	emit_byte(0x70+uncc); 
	emit_byte(7);  /* skip next 7 bytes if not cc=true */
	emit_byte(0x8b);
	emit_byte(0x04+8*d);
	emit_byte(0x05+8*index+64*fi);
	emit_long(base);
    }
}
LENDFUNC(NONE,READ,5,raw_cmov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor, IMM cond))

LOWFUNC(NONE,READ,3,raw_cmov_l_rm,(W4 d, IMM mem, IMM cond))
{
    if (have_cmov) {
	emit_byte(0x0f);
	emit_byte(0x40+cond);
	emit_byte(0x05+8*d);
	emit_long(mem);
    }
    else { /* replacement using branch and mov */
	int uncc=(cond^1);
	emit_byte(0x70+uncc); 
	emit_byte(6);  /* skip next 6 bytes if not cc=true */
	emit_byte(0x8b);
	emit_byte(0x05+8*d);
	emit_long(mem);
    }
}
LENDFUNC(NONE,READ,3,raw_cmov_l_rm,(W4 d, IMM mem, IMM cond))

LOWFUNC(NONE,READ,3,raw_mov_l_rR,(W4 d, R4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x8b);
    emit_byte(0x40+8*d+s);
    emit_byte(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_l_rR,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_w_rR,(W2 d, R4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x66);
    emit_byte(0x8b);
    emit_byte(0x40+8*d+s);
    emit_byte(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_w_rR,(W2 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_b_rR,(W1 d, R4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x8a);
    emit_byte(0x40+8*d+s);
    emit_byte(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_b_rR,(W1 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_l_brR,(W4 d, R4 s, IMM offset))
{
    emit_byte(0x8b);
    emit_byte(0x80+8*d+s);
    emit_long(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_l_brR,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_w_brR,(W2 d, R4 s, IMM offset))
{
    emit_byte(0x66);
    emit_byte(0x8b);
    emit_byte(0x80+8*d+s);
    emit_long(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_w_brR,(W2 d, R4 s, IMM offset))

LOWFUNC(NONE,READ,3,raw_mov_b_brR,(W1 d, R4 s, IMM offset))
{
    emit_byte(0x8a);
    emit_byte(0x80+8*d+s);
    emit_long(offset);
}
LENDFUNC(NONE,READ,3,raw_mov_b_brR,(W1 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_l_Ri,(R4 d, IMM i, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0xc7);
    emit_byte(0x40+d);
    emit_byte(offset);
    emit_long(i);
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_Ri,(R4 d, IMM i, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x66);
    emit_byte(0xc7);
    emit_byte(0x40+d);
    emit_byte(offset);
    emit_word(i);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_Ri,(R4 d, IMM i, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0xc6);
    emit_byte(0x40+d);
    emit_byte(offset);
    emit_byte(i);
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_Ri,(R4 d, IMM i, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_l_Rr,(R4 d, R4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x89);
    emit_byte(0x40+8*s+d);
    emit_byte(offset);
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_Rr,(R4 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_Rr,(R4 d, R2 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0x40+8*s+d);
    emit_byte(offset);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_Rr,(R4 d, R2 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_Rr,(R4 d, R1 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();
    emit_byte(0x88);
    emit_byte(0x40+8*s+d);
    emit_byte(offset);
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_Rr,(R4 d, R1 s, IMM offset))

LOWFUNC(NONE,NONE,3,raw_lea_l_brr,(W4 d, R4 s, IMM offset))
{
	if (optimize_imm8 && isbyte(offset)) {
    emit_byte(0x8d);
	emit_byte(0x40+8*d+s);
	emit_byte(offset);
	}
	else {
    emit_byte(0x8d);
	emit_byte(0x80+8*d+s);
	emit_long(offset);
	}
}
LENDFUNC(NONE,NONE,3,raw_lea_l_brr,(W4 d, R4 s, IMM offset))

LOWFUNC(NONE,NONE,5,raw_lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))
{
  int fi;
  
  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

	if (optimize_imm8 && isbyte(offset)) {
    emit_byte(0x8d);
	emit_byte(0x44+8*d);
	emit_byte(0x40*fi+8*index+s);
	emit_byte(offset);
	}
	else {
    emit_byte(0x8d);
	emit_byte(0x84+8*d);
	emit_byte(0x40*fi+8*index+s);
	emit_long(offset);
	}
}
LENDFUNC(NONE,NONE,5,raw_lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))

LOWFUNC(NONE,NONE,4,raw_lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))
{
  int isebp=(s==5)?0x40:0;
  int fi;
  
  switch(factor) {
  case 1: fi=0; break;
  case 2: fi=1; break;
  case 4: fi=2; break;
  case 8: fi=3; break;
  default: abort();
  }

    emit_byte(0x8d);
    emit_byte(0x04+8*d+isebp);
    emit_byte(0x40*fi+8*index+s);
    if (isebp)
      emit_byte(0);
}
LENDFUNC(NONE,NONE,4,raw_lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))

LOWFUNC(NONE,WRITE,3,raw_mov_l_bRr,(R4 d, R4 s, IMM offset))
{
	if (optimize_imm8 && isbyte(offset)) {
    emit_byte(0x89);
    emit_byte(0x40+8*s+d);
    emit_byte(offset);
	}
	else {
    emit_byte(0x89);
    emit_byte(0x80+8*s+d);
    emit_long(offset);
	}
}
LENDFUNC(NONE,WRITE,3,raw_mov_l_bRr,(R4 d, R4 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_w_bRr,(R4 d, R2 s, IMM offset))
{
    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0x80+8*s+d);
    emit_long(offset);
}
LENDFUNC(NONE,WRITE,3,raw_mov_w_bRr,(R4 d, R2 s, IMM offset))

LOWFUNC(NONE,WRITE,3,raw_mov_b_bRr,(R4 d, R1 s, IMM offset))
{
	if (optimize_imm8 && isbyte(offset)) {
    emit_byte(0x88);
    emit_byte(0x40+8*s+d);
    emit_byte(offset);
	}
	else {
    emit_byte(0x88);
    emit_byte(0x80+8*s+d);
    emit_long(offset);
	}
}
LENDFUNC(NONE,WRITE,3,raw_mov_b_bRr,(R4 d, R1 s, IMM offset))

LOWFUNC(NONE,NONE,1,raw_bswap_32,(RW4 r))
{
    emit_byte(0x0f);
    emit_byte(0xc8+r);
}
LENDFUNC(NONE,NONE,1,raw_bswap_32,(RW4 r))

LOWFUNC(WRITE,NONE,1,raw_bswap_16,(RW2 r))
{
    emit_byte(0x66);
    emit_byte(0xc1);
    emit_byte(0xc0+r);
    emit_byte(0x08);
}
LENDFUNC(WRITE,NONE,1,raw_bswap_16,(RW2 r))

LOWFUNC(NONE,NONE,2,raw_mov_l_rr,(W4 d, R4 s))
{
    emit_byte(0x89);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(NONE,NONE,2,raw_mov_l_rr,(W4 d, R4 s))

LOWFUNC(NONE,WRITE,2,raw_mov_l_mr,(IMM d, R4 s))
{
    emit_byte(0x89);
    emit_byte(0x05+8*s);
    emit_long(d);
}
LENDFUNC(NONE,WRITE,2,raw_mov_l_mr,(IMM d, R4 s))

LOWFUNC(NONE,WRITE,2,raw_mov_w_mr,(IMM d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x89);
    emit_byte(0x05+8*s);
    emit_long(d);
}
LENDFUNC(NONE,WRITE,2,raw_mov_w_mr,(IMM d, R2 s))

LOWFUNC(NONE,READ,2,raw_mov_w_rm,(W2 d, IMM s))
{
    emit_byte(0x66);
    emit_byte(0x8b);
    emit_byte(0x05+8*d);
    emit_long(s);
}
LENDFUNC(NONE,READ,2,raw_mov_w_rm,(W2 d, IMM s))

LOWFUNC(NONE,WRITE,2,raw_mov_b_mr,(IMM d, R1 s))
{
    emit_byte(0x88);
    emit_byte(0x05+8*(s&0xf)); /* XXX this handles %ah case (defined as 0x10+4) and others */
    emit_long(d);
}
LENDFUNC(NONE,WRITE,2,raw_mov_b_mr,(IMM d, R1 s))

LOWFUNC(NONE,READ,2,raw_mov_b_rm,(W1 d, IMM s))
{
    emit_byte(0x8a);
    emit_byte(0x05+8*d);
    emit_long(s);
}
LENDFUNC(NONE,READ,2,raw_mov_b_rm,(W1 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_l_ri,(W4 d, IMM s))
{
    emit_byte(0xb8+d);
    emit_long(s);
}
LENDFUNC(NONE,NONE,2,raw_mov_l_ri,(W4 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_w_ri,(W2 d, IMM s))
{
    emit_byte(0x66);
    emit_byte(0xb8+d);
    emit_word(s);
}
LENDFUNC(NONE,NONE,2,raw_mov_w_ri,(W2 d, IMM s))

LOWFUNC(NONE,NONE,2,raw_mov_b_ri,(W1 d, IMM s))
{
    emit_byte(0xb0+d);
    emit_byte(s);
}
LENDFUNC(NONE,NONE,2,raw_mov_b_ri,(W1 d, IMM s))

LOWFUNC(RMW,RMW,2,raw_adc_l_mi,(MEMRW d, IMM s))
{
    emit_byte(0x81);
    emit_byte(0x15);
    emit_long(d);
    emit_long(s);
}
LENDFUNC(RMW,RMW,2,raw_adc_l_mi,(MEMRW d, IMM s))

LOWFUNC(WRITE,RMW,2,raw_add_l_mi,(IMM d, IMM s)) 
{
	if (optimize_imm8 && isbyte(s)) {
    emit_byte(0x83);
    emit_byte(0x05);
    emit_long(d);
    emit_byte(s);
	}
	else {
    emit_byte(0x81);
    emit_byte(0x05);
    emit_long(d);
    emit_long(s);
	}
}
LENDFUNC(WRITE,RMW,2,raw_add_l_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,RMW,2,raw_add_w_mi,(IMM d, IMM s)) 
{
    emit_byte(0x66);
    emit_byte(0x81);
    emit_byte(0x05);
    emit_long(d);
    emit_word(s);
}
LENDFUNC(WRITE,RMW,2,raw_add_w_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,RMW,2,raw_add_b_mi,(IMM d, IMM s)) 
{
    emit_byte(0x80);
    emit_byte(0x05);
    emit_long(d);
    emit_byte(s);
}
LENDFUNC(WRITE,RMW,2,raw_add_b_mi,(IMM d, IMM s)) 

LOWFUNC(WRITE,NONE,2,raw_test_l_ri,(R4 d, IMM i))
{
	if (optimize_accum && isaccum(d))
	emit_byte(0xa9);
	else {
    emit_byte(0xf7);
    emit_byte(0xc0+d);
	}
    emit_long(i);
}
LENDFUNC(WRITE,NONE,2,raw_test_l_ri,(R4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_test_l_rr,(R4 d, R4 s))
{
    emit_byte(0x85);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_test_l_rr,(R4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_test_w_rr,(R2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x85);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_test_w_rr,(R2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_test_b_rr,(R1 d, R1 s))
{
    emit_byte(0x84);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_test_b_rr,(R1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_xor_l_ri,(RW4 d, IMM i))
{
    emit_byte(0x81);
    emit_byte(0xf0+d);
    emit_long(i);
}
LENDFUNC(WRITE,NONE,2,raw_xor_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_l_ri,(RW4 d, IMM i))
{
	if (optimize_imm8 && isbyte(i)) {
	emit_byte(0x83);
	emit_byte(0xe0+d);
	emit_byte(i);
	}
	else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x25);
	else {
	emit_byte(0x81);
	emit_byte(0xe0+d);
	}
	emit_long(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_and_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_w_ri,(RW2 d, IMM i))
{
	emit_byte(0x66);
	if (optimize_imm8 && isbyte(i)) {
	emit_byte(0x83);
	emit_byte(0xe0+d);
	emit_byte(i);
	}
	else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x25);
	else {
	emit_byte(0x81);
	emit_byte(0xe0+d);
	}
	emit_word(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_and_w_ri,(RW2 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_and_l,(RW4 d, R4 s))
{
    emit_byte(0x21);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_and_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_and_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x21);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_and_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_and_b,(RW1 d, R1 s))
{
    emit_byte(0x20);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_and_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_or_l_ri,(RW4 d, IMM i))
{
	if (optimize_imm8 && isbyte(i)) {
    emit_byte(0x83);
    emit_byte(0xc8+d);
    emit_byte(i);
	}
	else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x0d);
	else {
    emit_byte(0x81);
    emit_byte(0xc8+d);
	}
    emit_long(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_or_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_or_l,(RW4 d, R4 s))
{
    emit_byte(0x09);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_or_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_or_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x09);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_or_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_or_b,(RW1 d, R1 s))
{
    emit_byte(0x08);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_or_b,(RW1 d, R1 s))

LOWFUNC(RMW,NONE,2,raw_adc_l,(RW4 d, R4 s))
{
    emit_byte(0x11);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_adc_l,(RW4 d, R4 s))

LOWFUNC(RMW,NONE,2,raw_adc_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x11);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_adc_w,(RW2 d, R2 s))

LOWFUNC(RMW,NONE,2,raw_adc_b,(RW1 d, R1 s))
{
    emit_byte(0x10);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_adc_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_add_l,(RW4 d, R4 s))
{
    emit_byte(0x01);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_add_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_add_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x01);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_add_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_add_b,(RW1 d, R1 s))
{
    emit_byte(0x00);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_add_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_sub_l_ri,(RW4 d, IMM i))
{
  if (isbyte(i)) {
    emit_byte(0x83);
    emit_byte(0xe8+d);
    emit_byte(i);
  }
  else {
    if (optimize_accum && isaccum(d))
    emit_byte(0x2d);
    else {
    emit_byte(0x81);
    emit_byte(0xe8+d);
    }
    emit_long(i);
  }
}
LENDFUNC(WRITE,NONE,2,raw_sub_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_sub_b_ri,(RW1 d, IMM i))
{
	if (optimize_accum && isaccum(d))
	emit_byte(0x2c);
	else {
    emit_byte(0x80);
    emit_byte(0xe8+d);
	}
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_sub_b_ri,(RW1 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_l_ri,(RW4 d, IMM i))
{
    if (isbyte(i)) {
	emit_byte(0x83);
	emit_byte(0xc0+d);
	emit_byte(i);
    }
    else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x05);
	else {
	emit_byte(0x81);
	emit_byte(0xc0+d);
	}
	emit_long(i);
    }
}
LENDFUNC(WRITE,NONE,2,raw_add_l_ri,(RW4 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_w_ri,(RW2 d, IMM i))
{
	emit_byte(0x66);
    if (isbyte(i)) {
	emit_byte(0x83);
	emit_byte(0xc0+d);
	emit_byte(i);
    }
    else {
	if (optimize_accum && isaccum(d))
	emit_byte(0x05);
	else {
	emit_byte(0x81);
	emit_byte(0xc0+d);
	}
	emit_word(i);
    }
}
LENDFUNC(WRITE,NONE,2,raw_add_w_ri,(RW2 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_add_b_ri,(RW1 d, IMM i))
{
	if (optimize_accum && isaccum(d))
	emit_byte(0x04);
	else {
	emit_byte(0x80);
	emit_byte(0xc0+d);
	}
    emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_add_b_ri,(RW1 d, IMM i))

LOWFUNC(RMW,NONE,2,raw_sbb_l,(RW4 d, R4 s))
{
    emit_byte(0x19);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_l,(RW4 d, R4 s))

LOWFUNC(RMW,NONE,2,raw_sbb_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x19);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_w,(RW2 d, R2 s))

LOWFUNC(RMW,NONE,2,raw_sbb_b,(RW1 d, R1 s))
{
    emit_byte(0x18);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(RMW,NONE,2,raw_sbb_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_sub_l,(RW4 d, R4 s))
{
    emit_byte(0x29);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_sub_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x29);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_sub_b,(RW1 d, R1 s))
{
    emit_byte(0x28);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_sub_b,(RW1 d, R1 s))

LOWFUNC(WRITE,NONE,2,raw_cmp_l,(R4 d, R4 s))
{
    emit_byte(0x39);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_l,(R4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_cmp_l_ri,(R4 r, IMM i))
{
	if (optimize_imm8 && isbyte(i)) {
    emit_byte(0x83);
    emit_byte(0xf8+r);
    emit_byte(i);
	}
	else {
	if (optimize_accum && isaccum(r))
	emit_byte(0x3d);
	else {
    emit_byte(0x81);
    emit_byte(0xf8+r);
	}
    emit_long(i);
	}
}
LENDFUNC(WRITE,NONE,2,raw_cmp_l_ri,(R4 r, IMM i))

LOWFUNC(WRITE,NONE,2,raw_cmp_w,(R2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x39);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_w,(R2 d, R2 s))

LOWFUNC(WRITE,READ,2,raw_cmp_b_mi,(MEMR d, IMM s))
{
    emit_byte(0x80);
    emit_byte(0x3d);
    emit_long(d);
    emit_byte(s);
}
LENDFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))

LOWFUNC(WRITE,NONE,2,raw_cmp_b_ri,(R1 d, IMM i))
{
  if (optimize_accum && isaccum(d))
  emit_byte(0x3c);
  else {
  emit_byte(0x80);
  emit_byte(0xf8+d);
  }
  emit_byte(i);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_b_ri,(R1 d, IMM i))

LOWFUNC(WRITE,NONE,2,raw_cmp_b,(R1 d, R1 s))
{
    emit_byte(0x38);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_cmp_b,(R1 d, R1 s))

LOWFUNC(WRITE,READ,4,raw_cmp_l_rm_indexed,(R4 d, IMM offset, R4 index, IMM factor))
{
    int fi;
    
    switch(factor) {
     case 1: fi=0; break;
     case 2: fi=1; break;
     case 4: fi=2; break;
     case 8: fi=3; break;
     default: abort();
    }
    emit_byte(0x39);
    emit_byte(0x04+8*d);
    emit_byte(5+8*index+0x40*fi);
    emit_long(offset);
}
LENDFUNC(WRITE,READ,4,raw_cmp_l_rm_indexed,(R4 d, IMM offset, R4 index, IMM factor))

LOWFUNC(WRITE,NONE,2,raw_xor_l,(RW4 d, R4 s))
{
    emit_byte(0x31);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_l,(RW4 d, R4 s))

LOWFUNC(WRITE,NONE,2,raw_xor_w,(RW2 d, R2 s))
{
    emit_byte(0x66);
    emit_byte(0x31);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_w,(RW2 d, R2 s))

LOWFUNC(WRITE,NONE,2,raw_xor_b,(RW1 d, R1 s))
{
    emit_byte(0x30);
    emit_byte(0xc0+8*s+d);
}
LENDFUNC(WRITE,NONE,2,raw_xor_b,(RW1 d, R1 s))

LOWFUNC(WRITE,RMW,2,raw_sub_l_mi,(MEMRW d, IMM s))
{
	if (optimize_imm8 && isbyte(s)) {
    emit_byte(0x83);
    emit_byte(0x2d);
    emit_long(d);
    emit_byte(s);
	}
	else {
    emit_byte(0x81);
    emit_byte(0x2d);
    emit_long(d);
    emit_long(s);
	}
}
LENDFUNC(WRITE,RMW,2,raw_sub_l_mi,(MEMRW d, IMM s))

LOWFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))
{
	if (optimize_imm8 && isbyte(s)) {
    emit_byte(0x83);
    emit_byte(0x3d);
    emit_long(d);
    emit_byte(s);
	}
	else {
    emit_byte(0x81);
    emit_byte(0x3d);
    emit_long(d);
    emit_long(s);
	}
}
LENDFUNC(WRITE,READ,2,raw_cmp_l_mi,(MEMR d, IMM s))

LOWFUNC(NONE,NONE,2,raw_xchg_l_rr,(RW4 r1, RW4 r2))
{
  emit_byte(0x87);
  emit_byte(0xc0+8*r1+r2);
}
LENDFUNC(NONE,NONE,2,raw_xchg_l_rr,(RW4 r1, RW4 r2))

LOWFUNC(NONE,NONE,2,raw_xchg_b_rr,(RW4 r1, RW4 r2))
{
  emit_byte(0x86);
  emit_byte(0xc0+8*(r1&0xf)+(r2&0xf)); /* XXX this handles upper-halves registers (e.g. %ah defined as 0x10+4) */
}
LENDFUNC(NONE,NONE,2,raw_xchg_l_rr,(RW4 r1, RW4 r2))

/*************************************************************************
 * FIXME: mem access modes probably wrong                                *
 *************************************************************************/

LOWFUNC(READ,WRITE,0,raw_pushfl,(void))
{
	emit_byte(0x9c);
}
LENDFUNC(READ,WRITE,0,raw_pushfl,(void))

LOWFUNC(WRITE,READ,0,raw_popfl,(void))
{
	emit_byte(0x9d);
}
LENDFUNC(WRITE,READ,0,raw_popfl,(void))

/* Generate floating-point instructions */
static inline void x86_fadd_m(MEMR s)
{
	emit_byte(0xdc);
	emit_byte(0x05);
	emit_long(s);
}

#endif

/*************************************************************************
 * Unoptimizable stuff --- jump                                          *
 *************************************************************************/

static __inline__ void raw_call_r(R4 r)
{
#if USE_NEW_RTASM
    CALLsr(r);
#else
    emit_byte(0xff);
    emit_byte(0xd0+r);
#endif
}

static __inline__ void raw_call_m_indexed(uae_u32 base, uae_u32 r, uae_u32 m)
{
#if USE_NEW_RTASM
    CALLsm(base, X86_NOREG, r, m);
#else
    int mu;
    switch(m) {
     case 1: mu=0; break;
     case 2: mu=1; break;
     case 4: mu=2; break;
     case 8: mu=3; break;
     default: abort();
    }
    emit_byte(0xff);
    emit_byte(0x14);
    emit_byte(0x05+8*r+0x40*mu);
    emit_long(base);
#endif
}

static __inline__ void raw_jmp_r(R4 r)
{
#if USE_NEW_RTASM
    JMPsr(r);
#else
    emit_byte(0xff);
    emit_byte(0xe0+r);
#endif
}

static __inline__ void raw_jmp_m_indexed(uae_u32 base, uae_u32 r, uae_u32 m)
{
#if USE_NEW_RTASM
    JMPsm(base, X86_NOREG, r, m);
#else
    int mu;
    switch(m) {
     case 1: mu=0; break;
     case 2: mu=1; break;
     case 4: mu=2; break;
     case 8: mu=3; break;
     default: abort();
    }
    emit_byte(0xff);
    emit_byte(0x24);
    emit_byte(0x05+8*r+0x40*mu);
    emit_long(base);
#endif
}

static __inline__ void raw_jmp_m(uae_u32 base)
{
    emit_byte(0xff);
    emit_byte(0x25);
    emit_long(base);
}


static __inline__ void raw_call(uae_u32 t)
{
#if USE_NEW_RTASM
    CALLm(t);
#else
    emit_byte(0xe8);
    emit_long(t-(uae_u32)target-4);
#endif
}

static __inline__ void raw_jmp(uae_u32 t)
{
#if USE_NEW_RTASM
    JMPm(t);
#else
    emit_byte(0xe9);
    emit_long(t-(uae_u32)target-4);
#endif
}

static __inline__ void raw_jl(uae_u32 t)
{
    emit_byte(0x0f);
    emit_byte(0x8c);
    emit_long(t-(uintptr)target-4);
}

static __inline__ void raw_jz(uae_u32 t)
{
    emit_byte(0x0f);
    emit_byte(0x84);
    emit_long(t-(uintptr)target-4);
}

static __inline__ void raw_jnz(uae_u32 t)
{
    emit_byte(0x0f);
    emit_byte(0x85);
    emit_long(t-(uintptr)target-4);
}

static __inline__ void raw_jnz_l_oponly(void)
{
    emit_byte(0x0f); 
    emit_byte(0x85); 
}

static __inline__ void raw_jcc_l_oponly(int cc)
{
    emit_byte(0x0f); 
    emit_byte(0x80+cc); 
}

static __inline__ void raw_jnz_b_oponly(void)
{
    emit_byte(0x75); 
}

static __inline__ void raw_jz_b_oponly(void)
{
    emit_byte(0x74); 
}

static __inline__ void raw_jcc_b_oponly(int cc)
{
	emit_byte(0x70+cc);
}

static __inline__ void raw_jmp_l_oponly(void)
{
    emit_byte(0xe9); 
}

static __inline__ void raw_jmp_b_oponly(void)
{
    emit_byte(0xeb); 
}

static __inline__ void raw_ret(void)
{
    emit_byte(0xc3);  
}

static __inline__ void raw_nop(void)
{
    emit_byte(0x90);
}

static __inline__ void raw_emit_nop_filler(int nbytes)
{
  /* Source: GNU Binutils 2.12.90.0.15 */
  /* Various efficient no-op patterns for aligning code labels.
     Note: Don't try to assemble the instructions in the comments.
     0L and 0w are not legal.  */
  static const uae_u8 f32_1[] =
    {0x90};									/* nop					*/
  static const uae_u8 f32_2[] =
    {0x89,0xf6};							/* movl %esi,%esi		*/
  static const uae_u8 f32_3[] =
    {0x8d,0x76,0x00};						/* leal 0(%esi),%esi	*/
  static const uae_u8 f32_4[] =
    {0x8d,0x74,0x26,0x00};					/* leal 0(%esi,1),%esi	*/
  static const uae_u8 f32_5[] =
    {0x90,									/* nop					*/
     0x8d,0x74,0x26,0x00};					/* leal 0(%esi,1),%esi	*/
  static const uae_u8 f32_6[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00};		/* leal 0L(%esi),%esi	*/
  static const uae_u8 f32_7[] =
    {0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
  static const uae_u8 f32_8[] =
    {0x90,									/* nop					*/
     0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
  static const uae_u8 f32_9[] =
    {0x89,0xf6,								/* movl %esi,%esi		*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const uae_u8 f32_10[] =
    {0x8d,0x76,0x00,						/* leal 0(%esi),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const uae_u8 f32_11[] =
    {0x8d,0x74,0x26,0x00,					/* leal 0(%esi,1),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const uae_u8 f32_12[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00,			/* leal 0L(%esi),%esi	*/
     0x8d,0xbf,0x00,0x00,0x00,0x00};		/* leal 0L(%edi),%edi	*/
  static const uae_u8 f32_13[] =
    {0x8d,0xb6,0x00,0x00,0x00,0x00,			/* leal 0L(%esi),%esi	*/
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const uae_u8 f32_14[] =
    {0x8d,0xb4,0x26,0x00,0x00,0x00,0x00,	/* leal 0L(%esi,1),%esi */
     0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
  static const uae_u8 f32_15[] =
    {0xeb,0x0d,0x90,0x90,0x90,0x90,0x90,	/* jmp .+15; lotsa nops	*/
     0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
  static const uae_u8 f32_16[] =
    {0xeb,0x0d,0x90,0x90,0x90,0x90,0x90,	/* jmp .+15; lotsa nops	*/
     0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
  static const uae_u8 *const f32_patt[] = {
    f32_1, f32_2, f32_3, f32_4, f32_5, f32_6, f32_7, f32_8,
    f32_9, f32_10, f32_11, f32_12, f32_13, f32_14, f32_15
  };
  static const uae_u8 prefixes[4] = { 0x66, 0x66, 0x66, 0x66 };

#if defined(__x86_64__)
  /* The recommended way to pad 64bit code is to use NOPs preceded by
     maximally four 0x66 prefixes.  Balance the size of nops.  */
  if (nbytes == 0)
	  return;

  int i;
  int nnops = (nbytes + 3) / 4;
  int len = nbytes / nnops;
  int remains = nbytes - nnops * len;

  for (i = 0; i < remains; i++) {
	  emit_block(prefixes, len);
	  raw_nop();
  }
  for (; i < nnops; i++) {
	  emit_block(prefixes, len - 1);
	  raw_nop();
  }
#else
  int nloops = nbytes / 16;
  while (nloops-- > 0)
	emit_block(f32_16, sizeof(f32_16));

  nbytes %= 16;
  if (nbytes)
	emit_block(f32_patt[nbytes - 1], nbytes);
#endif
}


/*************************************************************************
 * Flag handling, to and fro UAE flag register                           *
 *************************************************************************/

static __inline__ void raw_flags_evicted(int r)
{
  //live.state[FLAGTMP].status=CLEAN;
  live.state[FLAGTMP].status=INMEM;
  live.state[FLAGTMP].realreg=-1;
  /* We just "evicted" FLAGTMP. */
  if (live.nat[r].nholds!=1) {
      /* Huh? */
      abort();
  }
  live.nat[r].nholds=0;
}

#define FLAG_NREG1_FLAGREG 0  /* Set to -1 if any register will do */
static __inline__ void raw_flags_to_reg_FLAGREG(int r)
{
  raw_lahf(0);  /* Most flags in AH */
  //raw_setcc(r,0); /* V flag in AL */
  raw_setcc_m((uintptr)live.state[FLAGTMP].mem,0); 
  
#if 1   /* Let's avoid those nasty partial register stalls */
  //raw_mov_b_mr((uintptr)live.state[FLAGTMP].mem,r);
  raw_mov_b_mr(((uintptr)live.state[FLAGTMP].mem)+1,AH_INDEX);
  raw_flags_evicted(r);
#endif
}

#define FLAG_NREG2_FLAGREG 0  /* Set to -1 if any register will do */
static __inline__ void raw_reg_to_flags_FLAGREG(int r)
{
  raw_cmp_b_ri(r,-127); /* set V */
  raw_sahf(0);
}

#define FLAG_NREG3_FLAGREG 0  /* Set to -1 if any register will do */
static __inline__ void raw_flags_set_zero_FLAGREG(int s, int tmp)
{
    raw_mov_l_rr(tmp,s);
    raw_lahf(s); /* flags into ah */
    raw_and_l_ri(s,0xffffbfff);
    raw_and_l_ri(tmp,0x00004000);
    raw_xor_l_ri(tmp,0x00004000);
    raw_or_l(s,tmp);
    raw_sahf(s);
}

static __inline__ void raw_flags_init_FLAGREG(void) { }

#define FLAG_NREG1_FLAGSTK -1  /* Set to -1 if any register will do */
static __inline__ void raw_flags_to_reg_FLAGSTK(int r)
{
	raw_pushfl();
	raw_pop_l_r(r);
	raw_mov_l_mr((uintptr)live.state[FLAGTMP].mem,r);
	raw_flags_evicted(r);
}

#define FLAG_NREG2_FLAGSTK -1  /* Set to -1 if any register will do */
static __inline__ void raw_reg_to_flags_FLAGSTK(int r)
{
	raw_push_l_r(r);
	raw_popfl();
}

#define FLAG_NREG3_FLAGSTK -1  /* Set to -1 if any register will do */
static __inline__ void raw_flags_set_zero_FLAGSTK(int s, int tmp)
{
    raw_mov_l_rr(tmp,s);
    raw_pushfl();
    raw_pop_l_r(s);
    raw_and_l_ri(s,0xffffffbf);
    raw_and_l_ri(tmp,0x00000040);
    raw_xor_l_ri(tmp,0x00000040);
    raw_or_l(s,tmp);
    raw_push_l_r(s);
    raw_popfl();
}

static __inline__ void raw_flags_init_FLAGSTK(void) { }

#if defined(__x86_64__)
/* Try to use the LAHF/SETO method on x86_64 since it is faster.
   This can't be the default because some older CPUs don't support
   LAHF/SAHF in long mode.  */
static int FLAG_NREG1_FLAGGEN = 0;
static __inline__ void raw_flags_to_reg_FLAGGEN(int r)
{
	if (have_lahf_lm) {
		// NOTE: the interpreter uses the normal EFLAGS layout
		//   pushf/popf CF(0) ZF( 6) SF( 7) OF(11)
		//   sahf/lahf  CF(8) ZF(14) SF(15) OF( 0)
		assert(r == 0);
		raw_setcc(r,0);					/* V flag in AL */
		raw_lea_l_r_scaled(0,0,8);		/* move it to its EFLAGS location */
		raw_mov_b_mr(((uintptr)live.state[FLAGTMP].mem)+1,0);
		raw_lahf(0);					/* most flags in AH */
		raw_mov_b_mr((uintptr)live.state[FLAGTMP].mem,AH_INDEX);
		raw_flags_evicted(r);
	}
	else
		raw_flags_to_reg_FLAGSTK(r);
}

static int FLAG_NREG2_FLAGGEN = 0;
static __inline__ void raw_reg_to_flags_FLAGGEN(int r)
{
	if (have_lahf_lm) {
		raw_xchg_b_rr(0,AH_INDEX);
		raw_cmp_b_ri(r,-120); /* set V */
		raw_sahf(0);
	}
	else
		raw_reg_to_flags_FLAGSTK(r);
}

static int FLAG_NREG3_FLAGGEN = 0;
static __inline__ void raw_flags_set_zero_FLAGGEN(int s, int tmp)
{
	if (have_lahf_lm)
		raw_flags_set_zero_FLAGREG(s, tmp);
	else
		raw_flags_set_zero_FLAGSTK(s, tmp);
}

static __inline__ void raw_flags_init_FLAGGEN(void)
{
	if (have_lahf_lm) {
		FLAG_NREG1_FLAGGEN = FLAG_NREG1_FLAGREG;
		FLAG_NREG2_FLAGGEN = FLAG_NREG2_FLAGREG;
		FLAG_NREG1_FLAGGEN = FLAG_NREG3_FLAGREG;
	}
	else {
		FLAG_NREG1_FLAGGEN = FLAG_NREG1_FLAGSTK;
		FLAG_NREG2_FLAGGEN = FLAG_NREG2_FLAGSTK;
		FLAG_NREG1_FLAGGEN = FLAG_NREG3_FLAGSTK;
	}
}
#endif

#ifdef SAHF_SETO_PROFITABLE
#define FLAG_SUFFIX FLAGREG
#elif defined __x86_64__
#define FLAG_SUFFIX FLAGGEN
#else
#define FLAG_SUFFIX FLAGSTK
#endif

#define FLAG_GLUE_2(x, y)		x ## _ ## y
#define FLAG_GLUE_1(x, y)		FLAG_GLUE_2(x, y)
#define FLAG_GLUE(x)			FLAG_GLUE_1(x, FLAG_SUFFIX)

#define raw_flags_init			FLAG_GLUE(raw_flags_init)
#define FLAG_NREG1				FLAG_GLUE(FLAG_NREG1)
#define raw_flags_to_reg		FLAG_GLUE(raw_flags_to_reg)
#define FLAG_NREG2				FLAG_GLUE(FLAG_NREG2)
#define raw_reg_to_flags		FLAG_GLUE(raw_reg_to_flags)
#define FLAG_NREG3				FLAG_GLUE(FLAG_NREG3)
#define raw_flags_set_zero		FLAG_GLUE(raw_flags_set_zero)

/* Apparently, there are enough instructions between flag store and
   flag reload to avoid the partial memory stall */
static __inline__ void raw_load_flagreg(uae_u32 target, uae_u32 r)
{
#if 1
    raw_mov_l_rm(target,(uintptr)live.state[r].mem);
#else
    raw_mov_b_rm(target,(uintptr)live.state[r].mem);
    raw_mov_b_rm(target+4,((uintptr)live.state[r].mem)+1);
#endif
}

/* FLAGX is byte sized, and we *do* write it at that size */
static __inline__ void raw_load_flagx(uae_u32 target, uae_u32 r)
{
    if (live.nat[target].canbyte)
	raw_mov_b_rm(target,(uintptr)live.state[r].mem);
    else if (live.nat[target].canword)
	raw_mov_w_rm(target,(uintptr)live.state[r].mem);
    else
	raw_mov_l_rm(target,(uintptr)live.state[r].mem);
}

static __inline__ void raw_dec_sp(int off)
{
    if (off) raw_sub_l_ri(ESP_INDEX,off);
}

static __inline__ void raw_inc_sp(int off)
{
    if (off) raw_add_l_ri(ESP_INDEX,off);
}

/*************************************************************************
 * Handling mistaken direct memory access                                *
 *************************************************************************/

// gb-- I don't need that part for JIT Basilisk II
#if defined(NATMEM_OFFSET) && 0
#include <asm/sigcontext.h>
#include <signal.h>

#define SIG_READ 1
#define SIG_WRITE 2

static int in_handler=0;
static uae_u8 veccode[256];

static void vec(int x, struct sigcontext sc)
{
    uae_u8* i=(uae_u8*)sc.eip;
    uae_u32 addr=sc.cr2;
    int r=-1;
    int size=4;
    int dir=-1;
    int len=0;
    int j;
    
    write_log("fault address is %08x at %08x\n",sc.cr2,sc.eip);
    if (!canbang) 
	write_log("Not happy! Canbang is 0 in SIGSEGV handler!\n");
    if (in_handler) 
	write_log("Argh --- Am already in a handler. Shouldn't happen!\n");

    if (canbang && i>=compiled_code && i<=current_compile_p) {
	if (*i==0x66) {
	    i++;
	    size=2;
	    len++;
	}
	
	switch(i[0]) {
	 case 0x8a:
                                  	    if ((i[1]&0xc0)==0x80) {
		r=(i[1]>>3)&7;
		dir=SIG_READ;
		size=1;
		len+=6;
		break;
	    }
	    break;
	 case 0x88:
	    if ((i[1]&0xc0)==0x80) {
		r=(i[1]>>3)&7;
		dir=SIG_WRITE;
		size=1;
		len+=6;
		break;
	    }
	    break;
	 case 0x8b:
	    if ((i[1]&0xc0)==0x80) {
		r=(i[1]>>3)&7;
		dir=SIG_READ;
		len+=6;
		break;
	    }
	    if ((i[1]&0xc0)==0x40) {
		r=(i[1]>>3)&7;
		dir=SIG_READ;
		len+=3;
		break;
	    }
	    break;
	 case 0x89:
	    if ((i[1]&0xc0)==0x80) {
		r=(i[1]>>3)&7;
		dir=SIG_WRITE;
		len+=6;
		break;
	    }
	    if ((i[1]&0xc0)==0x40) {
		r=(i[1]>>3)&7;
		dir=SIG_WRITE;
		len+=3;
		break;
	    }
	    break;
	}	
    }

    if (r!=-1) { 
	void* pr=NULL;
	write_log("register was %d, direction was %d, size was %d\n",r,dir,size);
	
	switch(r) {
	 case 0: pr=&(sc.eax); break;
	 case 1: pr=&(sc.ecx); break;
	 case 2: pr=&(sc.edx); break;
	 case 3: pr=&(sc.ebx); break;
	 case 4: pr=(size>1)?NULL:(((uae_u8*)&(sc.eax))+1); break;
	 case 5: pr=(size>1)?
		     (void*)(&(sc.ebp)):
			 (void*)(((uae_u8*)&(sc.ecx))+1); break;
	 case 6: pr=(size>1)?
		     (void*)(&(sc.esi)):
			 (void*)(((uae_u8*)&(sc.edx))+1); break;
	 case 7: pr=(size>1)?
		     (void*)(&(sc.edi)):
			 (void*)(((uae_u8*)&(sc.ebx))+1); break;
	 default: abort();
	}
	if (pr) {
	    blockinfo* bi;

	    if (currprefs.comp_oldsegv) {
	    addr-=NATMEM_OFFSET;

	    if ((addr>=0x10000000 && addr<0x40000000) ||
		(addr>=0x50000000)) {
		write_log("Suspicious address in %x SEGV handler.\n",addr);
	    }
	    if (dir==SIG_READ) {
		switch(size) {
		 case 1: *((uae_u8*)pr)=get_byte(addr); break;
		 case 2: *((uae_u16*)pr)=get_word(addr); break;
		 case 4: *((uae_u32*)pr)=get_long(addr); break;
		 default: abort();
		}
	    }
	    else { /* write */
		switch(size) {
		 case 1: put_byte(addr,*((uae_u8*)pr)); break;
		 case 2: put_word(addr,*((uae_u16*)pr)); break;
		 case 4: put_long(addr,*((uae_u32*)pr)); break;
		 default: abort();
		}
	    }
	    write_log("Handled one access!\n");
	    fflush(stdout);
	    segvcount++;
	    sc.eip+=len;
	    }
	    else {
		void* tmp=target;
		int i;
		uae_u8 vecbuf[5];
		
		addr-=NATMEM_OFFSET;
		
		if ((addr>=0x10000000 && addr<0x40000000) ||
		    (addr>=0x50000000)) {
		    write_log("Suspicious address in %x SEGV handler.\n",addr);
		}
		
		target=(uae_u8*)sc.eip;
		for (i=0;i<5;i++)
		    vecbuf[i]=target[i];
		emit_byte(0xe9);
		emit_long((uintptr)veccode-(uintptr)target-4);
		write_log("Create jump to %p\n",veccode);
	    
		write_log("Handled one access!\n");
		fflush(stdout);
		segvcount++;
		
		target=veccode;

		if (dir==SIG_READ) {
		    switch(size) {
		     case 1: raw_mov_b_ri(r,get_byte(addr)); break;
		     case 2: raw_mov_w_ri(r,get_byte(addr)); break;
		     case 4: raw_mov_l_ri(r,get_byte(addr)); break;
		     default: abort();
		    }
		}
		else { /* write */
		    switch(size) {
		     case 1: put_byte(addr,*((uae_u8*)pr)); break;
		     case 2: put_word(addr,*((uae_u16*)pr)); break;
		     case 4: put_long(addr,*((uae_u32*)pr)); break;
		     default: abort();
		    }
		}
		for (i=0;i<5;i++)
		    raw_mov_b_mi(sc.eip+i,vecbuf[i]);
		raw_mov_l_mi((uintptr)&in_handler,0);
		emit_byte(0xe9);
		emit_long(sc.eip+len-(uintptr)target-4);
		in_handler=1;
		target=tmp;
	    }
	    bi=active;
	    while (bi) {
		if (bi->handler && 
		    (uae_u8*)bi->direct_handler<=i &&
		    (uae_u8*)bi->nexthandler>i) {
		    write_log("deleted trigger (%p<%p<%p) %p\n",
			      bi->handler,
			      i,
			      bi->nexthandler,
			      bi->pc_p);
		    invalidate_block(bi);
		    raise_in_cl_list(bi);
		    set_special(0);
		    return;
		}
		bi=bi->next;
	    }
	    /* Not found in the active list. Might be a rom routine that
	       is in the dormant list */
	    bi=dormant;
	    while (bi) {
		if (bi->handler && 
		    (uae_u8*)bi->direct_handler<=i &&
		    (uae_u8*)bi->nexthandler>i) {
		    write_log("deleted trigger (%p<%p<%p) %p\n",
			      bi->handler,
			      i,
			      bi->nexthandler,
			      bi->pc_p);
		    invalidate_block(bi);
		    raise_in_cl_list(bi);
		    set_special(0);
		    return;
		}
		bi=bi->next;
	    }
	    write_log("Huh? Could not find trigger!\n");
	    return;
	}
    }
    write_log("Can't handle access!\n");
    for (j=0;j<10;j++) {
	write_log("instruction byte %2d is %02x\n",j,i[j]);
    }
    write_log("Please send the above info (starting at \"fault address\") to\n"
	   "bmeyer@csse.monash.edu.au\n"
	   "This shouldn't happen ;-)\n");
    fflush(stdout);
    signal(SIGSEGV,SIG_DFL);  /* returning here will cause a "real" SEGV */
}
#endif


/*************************************************************************
 * Checking for CPU features                                             *
 *************************************************************************/

struct cpuinfo_x86 {
  uae_u8	x86;			// CPU family
  uae_u8	x86_vendor;		// CPU vendor
  uae_u8	x86_processor;	// CPU canonical processor type
  uae_u8	x86_brand_id;	// CPU BrandID if supported, yield 0 otherwise
  uae_u32	x86_hwcap;
  uae_u8	x86_model;
  uae_u8	x86_mask;
  int		cpuid_level;    // Maximum supported CPUID level, -1=no CPUID
  char		x86_vendor_id[16];
};
struct cpuinfo_x86 cpuinfo;

enum {
  X86_VENDOR_INTEL		= 0,
  X86_VENDOR_CYRIX		= 1,
  X86_VENDOR_AMD		= 2,
  X86_VENDOR_UMC		= 3,
  X86_VENDOR_NEXGEN		= 4,
  X86_VENDOR_CENTAUR	= 5,
  X86_VENDOR_RISE		= 6,
  X86_VENDOR_TRANSMETA	= 7,
  X86_VENDOR_NSC		= 8,
  X86_VENDOR_UNKNOWN	= 0xff
};

enum {
  X86_PROCESSOR_I386,                       /* 80386 */
  X86_PROCESSOR_I486,                       /* 80486DX, 80486SX, 80486DX[24] */
  X86_PROCESSOR_PENTIUM,
  X86_PROCESSOR_PENTIUMPRO,
  X86_PROCESSOR_K6,
  X86_PROCESSOR_ATHLON,
  X86_PROCESSOR_PENTIUM4,
  X86_PROCESSOR_X86_64,
  X86_PROCESSOR_max
};

static const char * x86_processor_string_table[X86_PROCESSOR_max] = {
  "80386",
  "80486",
  "Pentium",
  "PentiumPro",
  "K6",
  "Athlon",
  "Pentium4",
  "x86-64"
};

static struct ptt {
  const int align_loop;
  const int align_loop_max_skip;
  const int align_jump;
  const int align_jump_max_skip;
  const int align_func;
}
x86_alignments[X86_PROCESSOR_max] = {
  {  4,  3,  4,  3,  4 },
  { 16, 15, 16, 15, 16 },
  { 16,  7, 16,  7, 16 },
  { 16, 15, 16,  7, 16 },
  { 32,  7, 32,  7, 32 },
  { 16,  7, 16,  7, 16 },
  {  0,  0,  0,  0,  0 },
  { 16,  7, 16,  7, 16 }
};

static void
x86_get_cpu_vendor(struct cpuinfo_x86 *c)
{
	char *v = c->x86_vendor_id;

	if (!strcmp(v, "GenuineIntel"))
		c->x86_vendor = X86_VENDOR_INTEL;
	else if (!strcmp(v, "AuthenticAMD"))
		c->x86_vendor = X86_VENDOR_AMD;
	else if (!strcmp(v, "CyrixInstead"))
		c->x86_vendor = X86_VENDOR_CYRIX;
	else if (!strcmp(v, "Geode by NSC"))
		c->x86_vendor = X86_VENDOR_NSC;
	else if (!strcmp(v, "UMC UMC UMC "))
		c->x86_vendor = X86_VENDOR_UMC;
	else if (!strcmp(v, "CentaurHauls"))
		c->x86_vendor = X86_VENDOR_CENTAUR;
	else if (!strcmp(v, "NexGenDriven"))
		c->x86_vendor = X86_VENDOR_NEXGEN;
	else if (!strcmp(v, "RiseRiseRise"))
		c->x86_vendor = X86_VENDOR_RISE;
	else if (!strcmp(v, "GenuineTMx86") ||
		 !strcmp(v, "TransmetaCPU"))
		c->x86_vendor = X86_VENDOR_TRANSMETA;
	else
		c->x86_vendor = X86_VENDOR_UNKNOWN;
}

static void
cpuid(uae_u32 op, uae_u32 *eax, uae_u32 *ebx, uae_u32 *ecx, uae_u32 *edx)
{
  const int CPUID_SPACE = 4096;
  uae_u8* cpuid_space = (uae_u8 *)vm_acquire(CPUID_SPACE);
  if (cpuid_space == VM_MAP_FAILED)
    abort();
  vm_protect(cpuid_space, CPUID_SPACE, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE);

  static uae_u32 s_op, s_eax, s_ebx, s_ecx, s_edx;
  uae_u8* tmp=get_target();

  s_op = op;
  set_target(cpuid_space);
  raw_push_l_r(0); /* eax */
  raw_push_l_r(1); /* ecx */
  raw_push_l_r(2); /* edx */
  raw_push_l_r(3); /* ebx */
  raw_mov_l_rm(0,(uintptr)&s_op);
  raw_cpuid(0);
  raw_mov_l_mr((uintptr)&s_eax,0);
  raw_mov_l_mr((uintptr)&s_ebx,3);
  raw_mov_l_mr((uintptr)&s_ecx,1);
  raw_mov_l_mr((uintptr)&s_edx,2);
  raw_pop_l_r(3);
  raw_pop_l_r(2);
  raw_pop_l_r(1);
  raw_pop_l_r(0);
  raw_ret();
  set_target(tmp);

  ((cpuop_func*)cpuid_space)(0);
  if (eax != NULL) *eax = s_eax;
  if (ebx != NULL) *ebx = s_ebx;
  if (ecx != NULL) *ecx = s_ecx;
  if (edx != NULL) *edx = s_edx;

  vm_release(cpuid_space, CPUID_SPACE);
}

static void
raw_init_cpu(void)
{
  struct cpuinfo_x86 *c = &cpuinfo;

  /* Defaults */
  c->x86_processor = X86_PROCESSOR_max;
  c->x86_vendor = X86_VENDOR_UNKNOWN;
  c->cpuid_level = -1;				/* CPUID not detected */
  c->x86_model = c->x86_mask = 0;	/* So far unknown... */
  c->x86_vendor_id[0] = '\0';		/* Unset */
  c->x86_hwcap = 0;
  
  /* Get vendor name */
  c->x86_vendor_id[12] = '\0';
  cpuid(0x00000000,
		(uae_u32 *)&c->cpuid_level,
		(uae_u32 *)&c->x86_vendor_id[0],
		(uae_u32 *)&c->x86_vendor_id[8],
		(uae_u32 *)&c->x86_vendor_id[4]);
  x86_get_cpu_vendor(c);

  /* Intel-defined flags: level 0x00000001 */
  c->x86_brand_id = 0;
  if ( c->cpuid_level >= 0x00000001 ) {
	uae_u32 tfms, brand_id;
	cpuid(0x00000001, &tfms, &brand_id, NULL, &c->x86_hwcap);
	c->x86 = (tfms >> 8) & 15;
	if (c->x86 == 0xf)
		c->x86 += (tfms >> 20) & 0xff; /* extended family */
	c->x86_model = (tfms >> 4) & 15;
	if (c->x86_model == 0xf)
		c->x86_model |= (tfms >> 12) & 0xf0; /* extended model */
	c->x86_brand_id = brand_id & 0xff;
	c->x86_mask = tfms & 15;
  } else {
	/* Have CPUID level 0 only - unheard of */
	c->x86 = 4;
  }

  /* AMD-defined flags: level 0x80000001 */
  uae_u32 xlvl;
  cpuid(0x80000000, &xlvl, NULL, NULL, NULL);
  if ( (xlvl & 0xffff0000) == 0x80000000 ) {
	if ( xlvl >= 0x80000001 ) {
	  uae_u32 features, extra_features;
	  cpuid(0x80000001, NULL, NULL, &extra_features, &features);
	  if (features & (1 << 29)) {
		/* Assume x86-64 if long mode is supported */
		c->x86_processor = X86_PROCESSOR_X86_64;
	  }
	  if (extra_features & (1 << 0))
		  have_lahf_lm = true;
	}
  }
	  
  /* Canonicalize processor ID */
  switch (c->x86) {
  case 3:
	c->x86_processor = X86_PROCESSOR_I386;
	break;
  case 4:
	c->x86_processor = X86_PROCESSOR_I486;
	break;
  case 5:
	if (c->x86_vendor == X86_VENDOR_AMD)
	  c->x86_processor = X86_PROCESSOR_K6;
	else
	  c->x86_processor = X86_PROCESSOR_PENTIUM;
	break;
  case 6:
	if (c->x86_vendor == X86_VENDOR_AMD)
	  c->x86_processor = X86_PROCESSOR_ATHLON;
	else
	  c->x86_processor = X86_PROCESSOR_PENTIUMPRO;
	break;
  case 15:
	  if (c->x86_processor == X86_PROCESSOR_max) {
		  switch (c->x86_vendor) {
		  case X86_VENDOR_INTEL:
			  c->x86_processor = X86_PROCESSOR_PENTIUM4;
			  break;
		  case X86_VENDOR_AMD:
			  /* Assume a 32-bit Athlon processor if not in long mode */
			  c->x86_processor = X86_PROCESSOR_ATHLON;
			  break;
		  }
	  }
	  break;
  }
  if (c->x86_processor == X86_PROCESSOR_max) {
	c->x86_processor = X86_PROCESSOR_I386;
	fprintf(stderr, "Error: unknown processor type, assuming i386\n");
	fprintf(stderr, "  Family  : %d\n", c->x86);
	fprintf(stderr, "  Model   : %d\n", c->x86_model);
	fprintf(stderr, "  Mask    : %d\n", c->x86_mask);
	fprintf(stderr, "  Vendor  : %s [%d]\n", c->x86_vendor_id, c->x86_vendor);
	if (c->x86_brand_id)
	  fprintf(stderr, "  BrandID : %02x\n", c->x86_brand_id);
  }

  /* Have CMOV support? */
  have_cmov = c->x86_hwcap & (1 << 15);
#if defined(__x86_64__)
  if (!have_cmov) {
	  write_log("x86-64 implementations are bound to have CMOV!\n");
	  abort();
  }
#endif

  /* Can the host CPU suffer from partial register stalls? */
  have_rat_stall = (c->x86_vendor == X86_VENDOR_INTEL);
#if 1
  /* It appears that partial register writes are a bad idea even on
	 AMD K7 cores, even though they are not supposed to have the
	 dreaded rat stall. Why? Anyway, that's why we lie about it ;-) */
  if (c->x86_processor == X86_PROCESSOR_ATHLON)
	have_rat_stall = true;
#endif

  /* Alignments */
  if (tune_alignment) {
	align_loops = x86_alignments[c->x86_processor].align_loop;
	align_jumps = x86_alignments[c->x86_processor].align_jump;
  }

  write_log("Max CPUID level=%d Processor is %s [%s]\n",
			c->cpuid_level, c->x86_vendor_id,
			x86_processor_string_table[c->x86_processor]);

  raw_flags_init();
}

static bool target_check_bsf(void)
{
	bool mismatch = false;
	for (int g_ZF = 0; g_ZF <= 1; g_ZF++) {
	for (int g_CF = 0; g_CF <= 1; g_CF++) {
	for (int g_OF = 0; g_OF <= 1; g_OF++) {
	for (int g_SF = 0; g_SF <= 1; g_SF++) {
		for (int value = -1; value <= 1; value++) {
			unsigned long flags = (g_SF << 7) | (g_OF << 11) | (g_ZF << 6) | g_CF;
			unsigned long tmp = value;
			__asm__ __volatile__ ("push %0; popf; bsf %1,%1; pushf; pop %0"
								  : "+r" (flags), "+r" (tmp) : : "cc");
			int OF = (flags >> 11) & 1;
			int SF = (flags >>  7) & 1;
			int ZF = (flags >>  6) & 1;
			int CF = flags & 1;
			tmp = (value == 0);
			if (ZF != tmp || SF != g_SF || OF != g_OF || CF != g_CF)
				mismatch = true;
		}
	}}}}
	if (mismatch)
		write_log("Target CPU defines all flags on BSF instruction\n");
	return !mismatch;
}


/*************************************************************************
 * FPU stuff                                                             *
 *************************************************************************/


static __inline__ void raw_fp_init(void)
{
    int i;
    
    for (i=0;i<N_FREGS;i++)
	live.spos[i]=-2;
    live.tos=-1;  /* Stack is empty */
}

static __inline__ void raw_fp_cleanup_drop(void)
{
#if 0
    /* using FINIT instead of popping all the entries.
       Seems to have side effects --- there is display corruption in
       Quake when this is used */
    if (live.tos>1) {
	emit_byte(0x9b);
	emit_byte(0xdb);
	emit_byte(0xe3);
	live.tos=-1;
    }
#endif
    while (live.tos>=1) {
	emit_byte(0xde);
	emit_byte(0xd9);
	live.tos-=2;
    }
    while (live.tos>=0) {
	emit_byte(0xdd);
	emit_byte(0xd8);
	live.tos--;
    }
    raw_fp_init();
}

static __inline__ void make_tos(int r)
{
    int p,q;

    if (live.spos[r]<0) { /* Register not yet on stack */
	emit_byte(0xd9);
	emit_byte(0xe8);  /* Push '1' on the stack, just to grow it */
	live.tos++;
	live.spos[r]=live.tos;
	live.onstack[live.tos]=r;
	return;
    }
    /* Register is on stack */
    if (live.tos==live.spos[r])
	return;
    p=live.spos[r];
    q=live.onstack[live.tos];

    emit_byte(0xd9);
    emit_byte(0xc8+live.tos-live.spos[r]);  /* exchange it with top of stack */
    live.onstack[live.tos]=r;
    live.spos[r]=live.tos;
    live.onstack[p]=q;
    live.spos[q]=p;
}

static __inline__ void make_tos2(int r, int r2)
{
    int q;

    make_tos(r2); /* Put the reg that's supposed to end up in position2
		     on top */

    if (live.spos[r]<0) { /* Register not yet on stack */
	make_tos(r); /* This will extend the stack */
	return;
    }
    /* Register is on stack */
    emit_byte(0xd9);
    emit_byte(0xc9); /* Move r2 into position 2 */

    q=live.onstack[live.tos-1];
    live.onstack[live.tos]=q;
    live.spos[q]=live.tos;
    live.onstack[live.tos-1]=r2;
    live.spos[r2]=live.tos-1;

    make_tos(r); /* And r into 1 */
}

static __inline__ int stackpos(int r)
{
    if (live.spos[r]<0)
	abort();
    if (live.tos<live.spos[r]) {
	printf("Looking for spos for fnreg %d\n",r);
	abort();
    }
    return live.tos-live.spos[r];
}

static __inline__ void usereg(int r)
{
    if (live.spos[r]<0)
	make_tos(r);
}

/* This is called with one FP value in a reg *above* tos, which it will
   pop off the stack if necessary */
static __inline__ void tos_make(int r)
{
    if (live.spos[r]<0) {
	live.tos++;
	live.spos[r]=live.tos;
	live.onstack[live.tos]=r;
	return;
    }
    emit_byte(0xdd);
    emit_byte(0xd8+(live.tos+1)-live.spos[r]);  /* store top of stack in reg, 
					 and pop it*/
}

/* FP helper functions */
#if USE_NEW_RTASM
#define DEFINE_OP(NAME, GEN)			\
static inline void raw_##NAME(uint32 m)		\
{						\
    GEN(m, X86_NOREG, X86_NOREG, 1);		\
}
DEFINE_OP(fstl,  FSTLm);
DEFINE_OP(fstpl, FSTPLm);
DEFINE_OP(fldl,  FLDLm);
DEFINE_OP(fildl, FILDLm);
DEFINE_OP(fistl, FISTLm);
DEFINE_OP(flds,  FLDSm);
DEFINE_OP(fsts,  FSTSm);
DEFINE_OP(fstpt, FSTPTm);
DEFINE_OP(fldt,  FLDTm);
#else
#define DEFINE_OP(NAME, OP1, OP2)		\
static inline void raw_##NAME(uint32 m)		\
{						\
    emit_byte(OP1);				\
    emit_byte(OP2);				\
    emit_long(m);				\
}
DEFINE_OP(fstl,  0xdd, 0x15);
DEFINE_OP(fstpl, 0xdd, 0x1d);
DEFINE_OP(fldl,  0xdd, 0x05);
DEFINE_OP(fildl, 0xdb, 0x05);
DEFINE_OP(fistl, 0xdb, 0x15);
DEFINE_OP(flds,  0xd9, 0x05);
DEFINE_OP(fsts,  0xd9, 0x15);
DEFINE_OP(fstpt, 0xdb, 0x3d);
DEFINE_OP(fldt,  0xdb, 0x2d);
#endif
#undef DEFINE_OP

LOWFUNC(NONE,WRITE,2,raw_fmov_mr,(MEMW m, FR r))
{
    make_tos(r);
    raw_fstl(m);
}
LENDFUNC(NONE,WRITE,2,raw_fmov_mr,(MEMW m, FR r))

LOWFUNC(NONE,WRITE,2,raw_fmov_mr_drop,(MEMW m, FR r))
{
    make_tos(r);
    raw_fstpl(m);
    live.onstack[live.tos]=-1;
    live.tos--;
    live.spos[r]=-2;
}
LENDFUNC(NONE,WRITE,2,raw_fmov_mr,(MEMW m, FR r))

LOWFUNC(NONE,READ,2,raw_fmov_rm,(FW r, MEMR m))
{
    raw_fldl(m);
    tos_make(r);
}
LENDFUNC(NONE,READ,2,raw_fmov_rm,(FW r, MEMR m))

LOWFUNC(NONE,READ,2,raw_fmovi_rm,(FW r, MEMR m))
{
    raw_fildl(m);
    tos_make(r);
}
LENDFUNC(NONE,READ,2,raw_fmovi_rm,(FW r, MEMR m))

LOWFUNC(NONE,WRITE,2,raw_fmovi_mr,(MEMW m, FR r))
{
    make_tos(r);
    raw_fistl(m);
}
LENDFUNC(NONE,WRITE,2,raw_fmovi_mr,(MEMW m, FR r))

LOWFUNC(NONE,READ,2,raw_fmovs_rm,(FW r, MEMR m))
{
    raw_flds(m);
    tos_make(r);
}
LENDFUNC(NONE,READ,2,raw_fmovs_rm,(FW r, MEMR m))

LOWFUNC(NONE,WRITE,2,raw_fmovs_mr,(MEMW m, FR r))
{
    make_tos(r);
    raw_fsts(m);
}
LENDFUNC(NONE,WRITE,2,raw_fmovs_mr,(MEMW m, FR r))

LOWFUNC(NONE,WRITE,2,raw_fmov_ext_mr,(MEMW m, FR r))
{
    int rs;

    /* Stupid x87 can't write a long double to mem without popping the 
       stack! */
    usereg(r);
    rs=stackpos(r);
    emit_byte(0xd9);     /* Get a copy to the top of stack */
    emit_byte(0xc0+rs);

    raw_fstpt(m);	/* store and pop it */
}
LENDFUNC(NONE,WRITE,2,raw_fmov_ext_mr,(MEMW m, FR r))

LOWFUNC(NONE,WRITE,2,raw_fmov_ext_mr_drop,(MEMW m, FR r))
{
    int rs;

    make_tos(r);
    raw_fstpt(m);	/* store and pop it */
    live.onstack[live.tos]=-1;
    live.tos--;
    live.spos[r]=-2;
}
LENDFUNC(NONE,WRITE,2,raw_fmov_ext_mr,(MEMW m, FR r))

LOWFUNC(NONE,READ,2,raw_fmov_ext_rm,(FW r, MEMR m))
{
    raw_fldt(m);
    tos_make(r);
}
LENDFUNC(NONE,READ,2,raw_fmov_ext_rm,(FW r, MEMR m))

LOWFUNC(NONE,NONE,1,raw_fmov_pi,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xeb);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_pi,(FW r))

LOWFUNC(NONE,NONE,1,raw_fmov_log10_2,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xec);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_log10_2,(FW r))

LOWFUNC(NONE,NONE,1,raw_fmov_log2_e,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xea);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_log2_e,(FW r))

LOWFUNC(NONE,NONE,1,raw_fmov_loge_2,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xed);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_loge_2,(FW r))

LOWFUNC(NONE,NONE,1,raw_fmov_1,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xe8);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_1,(FW r))

LOWFUNC(NONE,NONE,1,raw_fmov_0,(FW r))
{
    emit_byte(0xd9);
    emit_byte(0xee);
    tos_make(r);
}
LENDFUNC(NONE,NONE,1,raw_fmov_0,(FW r))

LOWFUNC(NONE,NONE,2,raw_fmov_rr,(FW d, FR s))
{
    int ds;

    usereg(s);
    ds=stackpos(s);
    if (ds==0 && live.spos[d]>=0) {
	/* source is on top of stack, and we already have the dest */
	int dd=stackpos(d);
	emit_byte(0xdd);
	emit_byte(0xd0+dd);
    }
    else {
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source on tos */
	tos_make(d); /* store to destination, pop if necessary */
    }
}
LENDFUNC(NONE,NONE,2,raw_fmov_rr,(FW d, FR s))

LOWFUNC(NONE,READ,4,raw_fldcw_m_indexed,(R4 index, IMM base))
{
    emit_byte(0xd9);
    emit_byte(0xa8+index);
    emit_long(base);
}
LENDFUNC(NONE,READ,4,raw_fldcw_m_indexed,(R4 index, IMM base))


LOWFUNC(NONE,NONE,2,raw_fsqrt_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xfa); /* take square root */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xfa); /* take square root */
    }	
}
LENDFUNC(NONE,NONE,2,raw_fsqrt_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fabs_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xe1); /* take fabs */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xe1); /* take fabs */
    }	
}
LENDFUNC(NONE,NONE,2,raw_fabs_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_frndint_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xfc); /* take frndint */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xfc); /* take frndint */
    }	
}
LENDFUNC(NONE,NONE,2,raw_frndint_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fcos_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xff); /* take cos */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xff); /* take cos */
    }	
}
LENDFUNC(NONE,NONE,2,raw_fcos_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fsin_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xfe); /* take sin */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xfe); /* take sin */
    }	
}
LENDFUNC(NONE,NONE,2,raw_fsin_rr,(FW d, FR s))

static const double one=1;
LOWFUNC(NONE,NONE,2,raw_ftwotox_rr,(FW d, FR s))
{
    int ds;

    usereg(s);
    ds=stackpos(s);
    emit_byte(0xd9);
    emit_byte(0xc0+ds); /* duplicate source */

    emit_byte(0xd9);
    emit_byte(0xc0);  /* duplicate top of stack. Now up to 8 high */
    emit_byte(0xd9);
    emit_byte(0xfc);  /* rndint */
    emit_byte(0xd9);
    emit_byte(0xc9);  /* swap top two elements */
    emit_byte(0xd8);
    emit_byte(0xe1);  /* subtract rounded from original */
    emit_byte(0xd9);
    emit_byte(0xf0);  /* f2xm1 */
    x86_fadd_m((uintptr)&one);  /* Add '1' without using extra stack space */
    emit_byte(0xd9);
    emit_byte(0xfd);  /* and scale it */
    emit_byte(0xdd);
    emit_byte(0xd9);  /* take he rounded value off */
    tos_make(d); /* store to destination */
}
LENDFUNC(NONE,NONE,2,raw_ftwotox_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fetox_rr,(FW d, FR s))
{
    int ds;

    usereg(s);
    ds=stackpos(s);
    emit_byte(0xd9);
    emit_byte(0xc0+ds); /* duplicate source */
    emit_byte(0xd9);
    emit_byte(0xea);   /* fldl2e */
    emit_byte(0xde);
    emit_byte(0xc9);  /* fmulp --- multiply source by log2(e) */

    emit_byte(0xd9);
    emit_byte(0xc0);  /* duplicate top of stack. Now up to 8 high */
    emit_byte(0xd9);
    emit_byte(0xfc);  /* rndint */
    emit_byte(0xd9);
    emit_byte(0xc9);  /* swap top two elements */
    emit_byte(0xd8);
    emit_byte(0xe1);  /* subtract rounded from original */
    emit_byte(0xd9);
    emit_byte(0xf0);  /* f2xm1 */
    x86_fadd_m((uintptr)&one);  /* Add '1' without using extra stack space */
    emit_byte(0xd9);
    emit_byte(0xfd);  /* and scale it */
    emit_byte(0xdd);
    emit_byte(0xd9);  /* take he rounded value off */
    tos_make(d); /* store to destination */
}
LENDFUNC(NONE,NONE,2,raw_fetox_rr,(FW d, FR s))
 
LOWFUNC(NONE,NONE,2,raw_flog2_rr,(FW d, FR s))
{
    int ds;

    usereg(s);
    ds=stackpos(s);
    emit_byte(0xd9);
    emit_byte(0xc0+ds); /* duplicate source */
    emit_byte(0xd9);
    emit_byte(0xe8); /* push '1' */
    emit_byte(0xd9);
    emit_byte(0xc9); /* swap top two */
    emit_byte(0xd9);
    emit_byte(0xf1); /* take 1*log2(x) */
    tos_make(d); /* store to destination */
}
LENDFUNC(NONE,NONE,2,raw_flog2_rr,(FW d, FR s))


LOWFUNC(NONE,NONE,2,raw_fneg_rr,(FW d, FR s))
{
    int ds;

    if (d!=s) {
	usereg(s);
	ds=stackpos(s);
	emit_byte(0xd9);
	emit_byte(0xc0+ds); /* duplicate source */
	emit_byte(0xd9);
	emit_byte(0xe0); /* take fchs */
	tos_make(d); /* store to destination */
    }
    else {
	make_tos(d);
	emit_byte(0xd9);
	emit_byte(0xe0); /* take fchs */
    }	
}
LENDFUNC(NONE,NONE,2,raw_fneg_rr,(FW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fadd_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    if (live.spos[s]==live.tos) {
	/* Source is on top of stack */
	ds=stackpos(d);
	emit_byte(0xdc);
	emit_byte(0xc0+ds); /* add source to dest*/
    }
    else {
	make_tos(d);
	ds=stackpos(s);
	
	emit_byte(0xd8);
	emit_byte(0xc0+ds); /* add source to dest*/
    }
}
LENDFUNC(NONE,NONE,2,raw_fadd_rr,(FRW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fsub_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    if (live.spos[s]==live.tos) {
	/* Source is on top of stack */
	ds=stackpos(d);
	emit_byte(0xdc);
	emit_byte(0xe8+ds); /* sub source from dest*/
    }
    else {
	make_tos(d);
	ds=stackpos(s);
	
	emit_byte(0xd8);
	emit_byte(0xe0+ds); /* sub src from dest */
    }
}
LENDFUNC(NONE,NONE,2,raw_fsub_rr,(FRW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fcmp_rr,(FR d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    make_tos(d);
    ds=stackpos(s);

    emit_byte(0xdd);
    emit_byte(0xe0+ds); /* cmp dest with source*/
}
LENDFUNC(NONE,NONE,2,raw_fcmp_rr,(FR d, FR s))

LOWFUNC(NONE,NONE,2,raw_fmul_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    if (live.spos[s]==live.tos) {
	/* Source is on top of stack */
	ds=stackpos(d);
	emit_byte(0xdc);
	emit_byte(0xc8+ds); /* mul dest by source*/
    }
    else {
	make_tos(d);
	ds=stackpos(s);
	
	emit_byte(0xd8);
	emit_byte(0xc8+ds); /* mul dest by source*/
    }
}
LENDFUNC(NONE,NONE,2,raw_fmul_rr,(FRW d, FR s))

LOWFUNC(NONE,NONE,2,raw_fdiv_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    if (live.spos[s]==live.tos) {
	/* Source is on top of stack */
	ds=stackpos(d);
	emit_byte(0xdc);
	emit_byte(0xf8+ds); /* div dest by source */
    }
    else {
	make_tos(d);
	ds=stackpos(s);
	
	emit_byte(0xd8);
	emit_byte(0xf0+ds); /* div dest by source*/
    }
}
LENDFUNC(NONE,NONE,2,raw_fdiv_rr,(FRW d, FR s))

LOWFUNC(NONE,NONE,2,raw_frem_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    make_tos2(d,s);
    ds=stackpos(s);

    if (ds!=1) {
	printf("Failed horribly in raw_frem_rr! ds is %d\n",ds);
	abort();
    }
    emit_byte(0xd9);
    emit_byte(0xf8); /* take rem from dest by source */
}
LENDFUNC(NONE,NONE,2,raw_frem_rr,(FRW d, FR s))

LOWFUNC(NONE,NONE,2,raw_frem1_rr,(FRW d, FR s))
{
    int ds;

    usereg(s);
    usereg(d);
    
    make_tos2(d,s);
    ds=stackpos(s);

    if (ds!=1) {
	printf("Failed horribly in raw_frem1_rr! ds is %d\n",ds);
	abort();
    }
    emit_byte(0xd9);
    emit_byte(0xf5); /* take rem1 from dest by source */
}
LENDFUNC(NONE,NONE,2,raw_frem1_rr,(FRW d, FR s))


LOWFUNC(NONE,NONE,1,raw_ftst_r,(FR r))
{
    make_tos(r);
    emit_byte(0xd9);  /* ftst */
    emit_byte(0xe4);
}
LENDFUNC(NONE,NONE,1,raw_ftst_r,(FR r))

/* %eax register is clobbered if target processor doesn't support fucomi */
#define FFLAG_NREG_CLOBBER_CONDITION !have_cmov
#define FFLAG_NREG EAX_INDEX

static __inline__ void raw_fflags_into_flags(int r)
{
    int p;

    usereg(r);
    p=stackpos(r);

    emit_byte(0xd9);
    emit_byte(0xee); /* Push 0 */
    emit_byte(0xd9);
    emit_byte(0xc9+p); /* swap top two around */
	if (have_cmov) {
		// gb-- fucomi is for P6 cores only, not K6-2 then...
    	emit_byte(0xdb);
    	emit_byte(0xe9+p); /* fucomi them */
	}
	else {
		emit_byte(0xdd);
		emit_byte(0xe1+p); /* fucom them */
		emit_byte(0x9b);
		emit_byte(0xdf);
		emit_byte(0xe0); /* fstsw ax */
		raw_sahf(0); /* sahf */
	}
    emit_byte(0xdd);
    emit_byte(0xd9+p);  /* store value back, and get rid of 0 */
}
