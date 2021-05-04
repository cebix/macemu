/*
 * compiler/codegen_arm.cpp - ARM code generator
 *
 * Copyright (c) 2013 Jens Heitmann of ARAnyM dev team (see AUTHORS)
 *
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * JIT compiler m68k -> ARM
 *
 * Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 * Adaptation for Basilisk II and improvements, copyright 2000-2004 Gwenole Beauchesne
 * Portions related to CPU detection come from linux/arch/i386/kernel/setup.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Current state:
 * 	- Experimental
 *	- Still optimizable
 *	- Not clock cycle optimized
 *	- as a first step this compiler emulates x86 instruction to be compatible
 *	  with gencomp. Better would be a specialized version of gencomp compiling
 *	  68k instructions to ARM compatible instructions. This is a step for the
 *	  future
 *
 */

#include "flags_arm.h"

// Declare the built-in __clear_cache function.
extern void __clear_cache (char*, char*);

/*************************************************************************
 * Some basic information about the the target CPU                       *
 *************************************************************************/

#define R0_INDEX 0
#define R1_INDEX 1
#define R2_INDEX 2
#define R3_INDEX 3
#define R4_INDEX 4
#define R5_INDEX 5
#define R6_INDEX 6
#define R7_INDEX 7
#define R8_INDEX  8
#define R9_INDEX  9
#define R10_INDEX 10
#define R11_INDEX 11
#define R12_INDEX 12
#define R13_INDEX 13
#define R14_INDEX 14
#define R15_INDEX 15

#define RSP_INDEX 13
#define RLR_INDEX 14
#define RPC_INDEX 15

/* The register in which subroutines return an integer return value */
#define REG_RESULT R0_INDEX

/* The registers subroutines take their first and second argument in */
#define REG_PAR1 R0_INDEX
#define REG_PAR2 R1_INDEX

#define REG_WORK1 R2_INDEX
#define REG_WORK2 R3_INDEX

//#define REG_DATAPTR R10_INDEX

#define REG_PC_PRE R0_INDEX /* The register we use for preloading regs.pc_p */
#define REG_PC_TMP R1_INDEX /* Another register that is not the above */

#define SHIFTCOUNT_NREG R1_INDEX  /* Register that can be used for shiftcount.
			      -1 if any reg will do. Normally this can be set to -1 but compemu_support is tied to 1 */
#define MUL_NREG1 R0_INDEX /* %r4 will hold the low 32 bits after a 32x32 mul */
#define MUL_NREG2 R1_INDEX /* %r5 will hold the high 32 bits */

#define STACK_ALIGN		4
#define STACK_OFFSET	sizeof(void *)
#define STACK_SHADOW_SPACE 0

uae_s8 always_used[]={2,3,-1};
uae_s8 can_byte[]={0,1,4,5,6,7,8,9,10,11,12,-1};
uae_s8 can_word[]={0,1,4,5,6,7,8,9,10,11,12,-1};

uae_u8 call_saved[]={0,0,0,0,1,1,1,1,1,1,1,1,0,1,1,1};

/* This *should* be the same as call_saved. But:
   - We might not really know which registers are saved, and which aren't,
     so we need to preserve some, but don't want to rely on everyone else
     also saving those registers
   - Special registers (such like the stack pointer) should not be "preserved"
     by pushing, even though they are "saved" across function calls
*/
static const uae_u8 need_to_preserve[]={0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0};
static const uae_u32 PRESERVE_MASK = ((1<<R4_INDEX)|(1<<R5_INDEX)|(1<<R6_INDEX)|(1<<R7_INDEX)|(1<<R8_INDEX)|(1<<R9_INDEX)
		|(1<<R10_INDEX)|(1<<R11_INDEX)|(1<<R12_INDEX));

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
#define CLOBBER_SW16
#define CLOBBER_SW32
#define CLOBBER_SETCC
#define CLOBBER_MUL  clobber_flags()
#define CLOBBER_BT   clobber_flags()
#define CLOBBER_BSF  clobber_flags()

#include "codegen_arm.h"

#define arm_emit_byte(B)		emit_byte(B)
#define arm_emit_word(W)		emit_word(W)
#define arm_emit_long(L)		emit_long(L)
#define arm_emit_quad(Q)		emit_quad(Q)
#define arm_get_target()		get_target()
#define arm_emit_failure(MSG)	jit_fail(MSG, __FILE__, __LINE__, __FUNCTION__)

const bool optimize_imm8		= true;

/*
 * Helper functions for immediate optimization
 */
static inline int isbyte(uae_s32 x)
{
	return (x>=-128 && x<=127);
}

static inline int is8bit(uae_s32 x)
{
	return (x>=-255 && x<=255);
}

static inline int isword(uae_s32 x)
{
	return (x>=-32768 && x<=32767);
}

#define jit_unimplemented(fmt, ...) do{ panicbug("**** Unimplemented ****"); panicbug(fmt, ## __VA_ARGS__); abort(); }while (0)

#if 0 /* currently unused */
static void jit_fail(const char *msg, const char *file, int line, const char *function)
{
	panicbug("JIT failure in function %s from file %s at line %d: %s",
			function, file, line, msg);
	abort();
}
#endif

LOWFUNC(NONE,WRITE,1,raw_push_l_r,(RR4 r))
{
	PUSH(r);
}

LOWFUNC(NONE,READ,1,raw_pop_l_r,(RR4 r))
{
	POP(r);
}

LOWFUNC(RMW,NONE,2,raw_adc_b,(RW1 d, RR1 s))
{
	MVN_ri(REG_WORK1, 0);								// mvn		r2,#0
	LSL_rri(REG_WORK2, d, 24); 							// lsl      r3, %[d], #24
	ORR_rrrLSRi(REG_WORK2, REG_WORK2, REG_WORK1, 8);	// orr		r3, r3, r2, lsr #8
	LSL_rri(REG_WORK1, s, 24);							// lsl     	r2, %[s], #24

	ADCS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adcs    r3, r3, r2

	BIC_rri(d, d, 0xFF);								// bic	   %[d],%[d],#0xFF
	ORR_rrrLSRi(d, d, REG_WORK2, 24);					// orr 	   %[d],%[d], R3 LSR #24
}

LOWFUNC(RMW,NONE,2,raw_adc_w,(RW2 d, RR2 s))
{
	MVN_ri(REG_WORK1, 0);								// mvn		r2,#0
	LSL_rri(REG_WORK2, d, 16); 							// lsl     	r3, %[d], #16
	ORR_rrrLSRi(REG_WORK2, REG_WORK2, REG_WORK1, 16);	// orr		r3, r3, r2, lsr #16
	LSL_rri(REG_WORK1, s, 16); 							// lsl      r2, %[s], #16

	ADCS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds    	r3, r3, r2
#ifdef ARMV6_ASSEMBLY
	PKHTB_rrrASRi(d,d,REG_WORK2,16);
#else
	BIC_rri(d, d, 0xff);								// bic		%[d],%[d],#0xff
	BIC_rri(d, d, 0xff00);								// bic		%[d],%[d],#0xff00
	ORR_rrrLSRi(d, d, REG_WORK2, 16); 					// orr     %[d], %[d], r3, lsr #16
#endif
}

LOWFUNC(RMW,NONE,2,raw_adc_l,(RW4 d, RR4 s))
{
	ADCS_rrr(d, d, s);									// adcs	 	%[d],%[d],%[s]
}

LOWFUNC(WRITE,NONE,2,raw_add_b,(RW1 d, RR1 s))
{
	LSL_rri(REG_WORK1, s, 24);							// lsl     r2, %[s], #24
	LSL_rri(REG_WORK2, d, 24); 							// lsl     r3, %[d], #24

	ADDS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds    r3, r3, r2

	BIC_rri(d, d, 0xFF);								// bic	   %[d],%[d],#0xFF
	ORR_rrrLSRi(d, d, REG_WORK2, 24);					// orr 	   %[d],%[d], r3 LSR #24
}

LOWFUNC(WRITE,NONE,2,raw_add_w,(RW2 d, RR2 s))
{
	LSL_rri(REG_WORK1, s, 16); 							// lsl     r2, %[s], #16
	LSL_rri(REG_WORK2, d, 16); 							// lsl     r3, %[d], #16

	ADDS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds    r3, r3, r2

#ifdef ARMV6_ASSEMBLY
	PKHTB_rrrASRi(d,d,REG_WORK2,16);
#else
	BIC_rri(d, d, 0xff);								// bic		%[d],%[d],#0xff
	BIC_rri(d, d, 0xff00);								// bic		%[d],%[d],#0xff00
	ORR_rrrLSRi(d, d, REG_WORK2, 16); 					// orr     r7, r7, r3, LSR #16
#endif
}

LOWFUNC(WRITE,NONE,2,raw_add_l,(RW4 d, RR4 s))
{
	ADDS_rrr(d, d, s); 									// adds  	%[d], %[d], %[s]
}

LOWFUNC(WRITE,NONE,2,raw_add_w_ri,(RW2 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
    long offs = data_word_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	  // ldrh    r2, [pc, #offs]
#else
#  ifdef ARMV6_ASSEMBLY
	LDRH_rRI(REG_WORK1, RPC_INDEX, 24); 				// ldrh    r2, [pc, #24]   ; <value>
#  else
	LDRH_rRI(REG_WORK1, RPC_INDEX, 16); 				// ldrh    r2, [pc, #16]   ; <value>
#  endif
#endif
	LSL_rri(REG_WORK2, d, 16);  						// lsl     r3, %[d], #16
	LSL_rri(REG_WORK1, REG_WORK1, 16);  				// lsl     r2, r2, #16

	ADDS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds    r3, r3, r2

#ifdef ARMV6_ASSEMBLY
	PKHTB_rrrASRi(d,d,REG_WORK2,16);
#else
	BIC_rri(d, d, 0xff);								// bic		%[d],%[d],#0xff
	BIC_rri(d, d, 0xff00);								// bic		%[d],%[d],#0xff00
	ORR_rrrLSRi(d, d, REG_WORK2, 16);					// orr     	%[d],%[d], r3, LSR #16
#endif

#if !defined(USE_DATA_BUFFER)
	B_i(0); 											// b       <jp>

	//<value>:
	emit_word(i);
	skip_word(0);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_add_b_ri,(RW1 d, IMM i))
{
	LSL_rri(REG_WORK2, d, 24);  						// lsl     r3, %[d], #24

	ADDS_rri(REG_WORK2, REG_WORK2, i << 24); 			// adds    r3, r3, #0x12000000

	BIC_rri(d, d, 0xFF);								// bic	   %[d],%[d], #0xFF
	ORR_rrrLSRi(d, d, REG_WORK2, 24);					// orr     %[d],%[d], r3, lsr #24
}

LOWFUNC(WRITE,NONE,2,raw_add_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	  // ldr     r2, [pc, #offs]
	ADDS_rrr(d, d, REG_WORK1); 							// adds    %[d], %[d], r2
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 					// ldr     r2, [pc, #4]    ; <value>
	ADDS_rrr(d, d, REG_WORK1); 							// adds    %[d], %[d], r2
	B_i(0);  											// b       <jp>

	//<value>:
	emit_long(i);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_and_b,(RW1 d, RR1 s))
{
	MVN_rrLSLi(REG_WORK1, s, 24);						// mvn 	r2, %[s], lsl #24
	MVN_rrLSRi(REG_WORK1, REG_WORK1, 24);				// mvn 	r2, %[s], lsr #24
	AND_rrr(d, d, REG_WORK1);							// and  %[d], %[d], r2

	LSLS_rri(REG_WORK1, d, 24);							// lsls r2, %[d], #24

	MRS_CPSR(REG_WORK1);                    			// mrs  r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);		// bic  r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    			// msr  CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_and_w,(RW2 d, RR2 s))
{
	MVN_rrLSLi(REG_WORK1, s, 16);						// mvn 		r2, %[s], lsl #16
	MVN_rrLSRi(REG_WORK1, REG_WORK1, 16);				// mvn 		r2, %[s], lsr #16
	AND_rrr(d, d, REG_WORK1); 							// and    	%[d], %[d], r2

	LSLS_rri(REG_WORK1, d, 16);							// lsls 	r2, %[d], #16

	MRS_CPSR(REG_WORK1);                    			// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);		// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    			// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_and_l,(RW4 d, RR4 s))
{
	ANDS_rrr(d, d, s); 									// ands	   r7, r7, r6

	MRS_CPSR(REG_WORK1);                    			// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 		// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    			// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_and_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 16); 					// ldr     r2, [pc, #16]   ; <value>
#endif
	ANDS_rrr(d, d, REG_WORK1); 							// ands    %[d], %[d], r2

	MRS_CPSR(REG_WORK1);                    			// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);		// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    			// msr     CPSR_fc, r2

#if !defined(USE_DATA_BUFFER)
	B_i(0); 											// b       <jp>

	//<value>:
	emit_long(i);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_bsf_l_rr,(W4 d, RR4 s))
{
	MOV_rr(REG_WORK1, s);							// mov r2,%[s]
	RSB_rri(REG_WORK2, REG_WORK1, 0);				// rsb r3,r2,#0
	AND_rrr(REG_WORK1, REG_WORK1, REG_WORK2);		// and r2,r2,r3
	CLZ_rr(REG_WORK2, REG_WORK1);					// clz r3,r2
	MOV_ri(d, 32);									// mov %[d],#32
	SUB_rrr(d, d, REG_WORK2);						// sub %[d],%[d],r3

	MRS_CPSR(REG_WORK2);							// mrs r3,cpsr
	TEQ_ri(d, 0);									// teq %[d],#0
	CC_SUBS_rri(NATIVE_CC_NE, d,d,1);				// sub %[d],%[d],#1
	CC_BIC_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_Z_FLAG);		// bic r3,r3,#0x40000000
	CC_ORR_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_Z_FLAG);		// orr r3,r3,#0x40000000
	MSR_CPSR_r(REG_WORK2);							// msr cpsr,r3
}

LOWFUNC(WRITE,NONE,1,raw_bswap_16,(RW2 r))
{
#if defined(ARMV6_ASSEMBLY)
	REVSH_rr(REG_WORK1,r);						// revsh 	r2,%[r]
	UXTH_rr(REG_WORK1, REG_WORK1);				// utxh		r2,r2
	LSR_rri(r, r, 16);
	ORR_rrrLSLi(r, REG_WORK1, r, 16);			// orr  	%[r], %[r], r2
#else
	MOV_rr(REG_WORK1, r); 						// mov	r2, r6
	BIC_rri(REG_WORK1, REG_WORK1, 0xff0000); 	// bic	r2, r2, #0xff0000
	BIC_rri(REG_WORK1, REG_WORK1, 0xff000000); 	// bic	r2, r2, #0xff000000

	EOR_rrr(r, r, REG_WORK1); 					// eor	r6, r6, r2

	ORR_rrrLSRi(r, r, REG_WORK1, 8); 			// orr	r6, r6, r2, lsr #8
	BIC_rri(REG_WORK1, REG_WORK1, 0xff00); 		// bic	r2, r2, #0xff00
	ORR_rrrLSLi(r,r,REG_WORK1, 8); 				// orr	r6, r6, r2, lsl #8
#endif
}

LOWFUNC(NONE,NONE,1,raw_bswap_32,(RW4 r))
{
#if defined(ARMV6_ASSEMBLY)
	REV_rr(r,r);								// rev 	   %[r],%[r]
#else
	EOR_rrrRORi(REG_WORK1, r, r, 16);      		// eor     r2, r6, r6, ror #16
	BIC_rri(REG_WORK1, REG_WORK1, 0xff0000); 	// bic     r2, r2, #0xff0000
	ROR_rri(r, r, 8); 							// ror     r6, r6, #8
	EOR_rrrLSRi(r, r, REG_WORK1, 8);      		// eor     r6, r6, r2, lsr #8
#endif
}

LOWFUNC(WRITE,NONE,2,raw_bt_l_ri,(RR4 r, IMM i))
{
	int imm = (1 << (i & 0x1f));

	MRS_CPSR(REG_WORK2);                    	// mrs        r3, CPSR
	TST_ri(r, imm);                            	// tst        r6, #0x1000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// bic        r3, r3, #0x20000000
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// orr        r3, r3, #0x20000000
	MSR_CPSR_r(REG_WORK2);                    	// msr        CPSR_fc, r3
}

LOWFUNC(WRITE,NONE,2,raw_bt_l_rr,(RR4 r, RR4 b))
{
	AND_rri(REG_WORK2, b, 0x1f);               	// and  r3, r7, #0x1f
	LSR_rrr(REG_WORK1, r, REG_WORK2); 			// lsr	r2, r6, r3

	MRS_CPSR(REG_WORK2);                       	// mrs	r3, CPSR
	TST_ri(REG_WORK1, 1);              			// tst	r2, #1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// orr	r3, r3, #0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// bic	r3, r3, #0x20000000
	MSR_CPSR_r(REG_WORK2);                      // msr   CPSR_fc, r3
}

LOWFUNC(WRITE,NONE,2,raw_btc_l_rr,(RW4 r, RR4 b))
{
	MOV_ri(REG_WORK1, 1);                  		// mov	r2, #1
	AND_rri(REG_WORK2, b, 0x1f);               	// and  r3, r7, #0x1f
	LSL_rrr(REG_WORK1, REG_WORK1, REG_WORK2); 	// lsl	r2, r2, r3

	MRS_CPSR(REG_WORK2);                       	// mrs	r3, CPSR
	TST_rr(r, REG_WORK1);                      	// tst	r6, r2
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);	// orr	r3, r3, #0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG);  // bic	r3, r3, #0x20000000
	EOR_rrr(r, r, REG_WORK1);                  	// eor  r6, r6, r2
	MSR_CPSR_r(REG_WORK2);                     	// msr  CPSR_fc, r3
}

LOWFUNC(WRITE,NONE,2,raw_btr_l_rr,(RW4 r, RR4 b))
{
	MOV_ri(REG_WORK1, 1);                      	// mov	r2, #1
	AND_rri(REG_WORK2, b, 0x1f);               	// and  r3, r7, #0x1f
	LSL_rrr(REG_WORK1, REG_WORK1, REG_WORK2);  	// lsl	r2, r2, r3

	MRS_CPSR(REG_WORK2);                       	// mrs	r3, CPSR
	TST_rr(r, REG_WORK1);                      	// tst	r6, r2
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);	// orr	r3, r3, #0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// bic	r3, r3, #0x20000000
	BIC_rrr(r, r, REG_WORK1);                  	// bic  r6, r6, r2
	MSR_CPSR_r(REG_WORK2);                     	// msr  CPSR_fc, r3
}

LOWFUNC(WRITE,NONE,2,raw_bts_l_rr,(RW4 r, RR4 b))
{
	MOV_ri(REG_WORK1, 1);                    	// mov	r2, #1
	AND_rri(REG_WORK2, b, 0x1f);               	// and  r3, r7, #0x1f
	LSL_rrr(REG_WORK1, REG_WORK1, REG_WORK2);  	// lsl	r2, r2, r3

	MRS_CPSR(REG_WORK2);                       	// mrs	r3, CPSR
	TST_rr(r, REG_WORK1);                      	// tst	r6, r2
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);	// orr	r3, r3, #0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG);  // bic	r3, r3, #0x20000000
	ORR_rrr(r, r, REG_WORK1);                  	// orr  r6, r6, r2
	MSR_CPSR_r(REG_WORK2);                      // msr  CPSR_fc, r3
}

LOWFUNC(READ,NONE,3,raw_cmov_l_rr,(RW4 d, RR4 s, IMM cc))
{
	switch (cc) {
		case 9: // LS
			BEQ_i(0);										// beq  <set>  Z != 0
			BCC_i(0);										// bcc  <continue>  C == 0

			//<set>:
			MOV_rr(d, s);									// mov	r7,r6
			break;

		case 8: // HI
			BEQ_i(1);										// beq  <continue> 	Z != 0
			BCS_i(0);										// bcs	<continue>	C != 0
			MOV_rr(d, s);									// mov	r7,#0
			break;

		default:
			CC_MOV_rr(cc, d, s);		 					// MOVcc	R7,#1
			break;
		}
	//<continue>:
}

LOWFUNC(WRITE,NONE,2,raw_cmp_b,(RR1 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	SXTB_rr(REG_WORK1, d);							// sxtb r2,%[d]
	SXTB_rr(REG_WORK2, s);							// sxtb r3,%[s]
#else
	LSL_rri(REG_WORK1, d, 24);						// lsl  r2,r6,#24
	LSL_rri(REG_WORK2, s, 24);						// lsl  r3,r7,#24
#endif
	CMP_rr(REG_WORK1, REG_WORK2);					// cmp r2, r3

	MRS_CPSR(REG_WORK1);                        	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     		// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_cmp_w,(RR2 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	SXTH_rr(REG_WORK1, d);						// sxtb r2,%[d]
	SXTH_rr(REG_WORK2, s);						// sxtb r3,%[s]
#else
	LSL_rri(REG_WORK1, d, 16); 					// lsl	r6, r1, #16
	LSL_rri(REG_WORK2, s, 16); 					// lsl	r7, r2, #16
#endif

	CMP_rr(REG_WORK1, REG_WORK2); 				// cmp	r7, r6, asr #16

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_cmp_l,(RR4 d, RR4 s))
{
	CMP_rr(d, s);                           	// cmp     r7, r6

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(NONE,NONE,2,raw_imul_32_32,(RW4 d, RR4 s))
{
	SMULL_rrrr(REG_WORK1, REG_WORK2, d, s);		// smull r2,r3,r7,r6
	MOV_rr(d, REG_WORK1);						// mov 	 r7,r2
}

LOWFUNC(NONE,NONE,2,raw_imul_64_32,(RW4 d, RW4 s))
{
	SMULL_rrrr(REG_WORK1, REG_WORK2, d, s);		// smull r2,r3,r7,r6
	MOV_rr(MUL_NREG1, REG_WORK1);						// mov 	 r7,r2
	MOV_rr(MUL_NREG2, REG_WORK2);
}

LOWFUNC(NONE,NONE,3,raw_lea_l_brr,(W4 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
	ADD_rrr(d, s, REG_WORK1);         	  // add     r7, r6, r2
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	ADD_rrr(d, s, REG_WORK1);         	// add     r7, r6, r2
	B_i(0);                           	// b        <jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,5,raw_lea_l_brr_indexed,(W4 d, RR4 s, RR4 index, IMM factor, IMM offset))
{
	int shft;
	switch(factor) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	  // LDR 	R2,[PC, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 8);			// LDR 	R2,[PC, #8]
#endif
	ADD_rrr(REG_WORK1, s, REG_WORK1);			// ADD  R7,R6,R2
	ADD_rrrLSLi(d, REG_WORK1, index, shft);		// ADD  R7,R7,R5,LSL #2
#if !defined(USE_DATA_BUFFER)
	B_i(0);										// B	jp

	emit_long(offset);
	//<jp>;
#endif
}

LOWFUNC(NONE,NONE,4,raw_lea_l_rr_indexed,(W4 d, RR4 s, RR4 index, IMM factor))
{
	int shft;
	switch(factor) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

	ADD_rrrLSLi(d, s, index, shft);		// ADD R7,R6,R5,LSL #2
}

LOWFUNC(NONE,READ,3,raw_mov_b_brR,(W1 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr  r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 12); 	// ldr  r2, [pc, #12]   ; <value>
#endif
	LDRB_rRR(REG_WORK1, REG_WORK1, s); 	// ldrb	r2, [r2, r6]

	BIC_rri(d, d, 0xff);              	// bic	r7, r7, #0xff
	ORR_rrr(d, d, REG_WORK1);          	// orr	r7, r7, r2
#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_b_bRr,(RR4 d, RR1 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	// ldr  r2,[pc, #offs]
	STRB_rRR(s, d, REG_WORK1);			// strb r6,[r7, r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);	// ldr  r2,[pc,#4]
	STRB_rRR(s, d, REG_WORK1);			// strb r6,[r7, r2]
	B_i(0);								// b	<jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_b_mi,(MEMW d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);  	// ldr	r2, [pc, #offs]	; <d>
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 8);  	// ldr	r2, [pc, #8]	; <d>
#endif
	MOV_ri(REG_WORK2, s & 0xFF);		// mov	r3, #0x34
	STRB_rR(REG_WORK2, REG_WORK1);    	// strb	r3, [r2]
#if !defined(USE_DATA_BUFFER)
	B_i(0);                             // b	<jp>

	//d:
	emit_long(d);

	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_b_mr,(IMM d, RR1 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]
	STRB_rR(s, REG_WORK1);	           	// strb	r6, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr	r2, [pc, #4]	; <value>
	STRB_rR(s, REG_WORK1);	           	// strb	r6, [r2]
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,raw_mov_b_ri,(W1 d, IMM s))
{
	BIC_rri(d, d, 0xff);      	// bic	%[d], %[d], #0xff
	ORR_rri(d, d, (s & 0xff)); 	// orr	%[d], %[d], #%[s]
}

LOWFUNC(NONE,READ,2,raw_mov_b_rm,(W1 d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 12); 	// ldr	r2, [pc, #12]	; <value>
#endif
	LDRB_rR(REG_WORK2, REG_WORK1);     	// ldrb	r2, [r2]
	BIC_rri(d, d, 0xff);           		// bic	r7, r7, #0xff
	ORR_rrr(d, REG_WORK2, d);  			// orr	r7, r2, r7
#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,raw_mov_b_rr,(W1 d, RR1 s))
{
	AND_rri(REG_WORK1, s, 0xff);		// and  r2,r2, #0xff
	BIC_rri(d, d, 0x0ff);          		// bic	%[d], %[d], #0xff
	ORR_rrr(d, d, REG_WORK1);      		// orr	%[d], %[d], r2
}

LOWFUNC(NONE,READ,3,raw_mov_l_brR,(W4 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
	LDR_rRR(d, REG_WORK1, s);         	  // ldr     r7, [r2, r6]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	LDR_rRR(d, REG_WORK1, s);         	// ldr     r7, [r2, r6]

	B_i(0);                           	// b       <jp>

	emit_long(offset);			//<value>:
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_l_bRr,(RR4 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	// ldr	r2,[pc, #offs]
	STR_rRR(s, d, REG_WORK1);			        // str  R6,[R7, r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);	// ldr	r2,[pc,#4]	; <value>
	STR_rRR(s, d, REG_WORK1);			// str  R6,[R7, r2]
	B_i(0);								// b 	<jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_l_mi,(MEMW d, IMM s))
{
	// TODO: optimize imm

#if defined(USE_DATA_BUFFER)
  data_check_end(8, 12);
  long offs = data_long_offs(d);

	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr    r2, [pc, #offs]    ; d

	offs = data_long_offs(s);
	LDR_rRI(REG_WORK2, RPC_INDEX, offs); 	// ldr    r3, [pc, #offs]    ; s

	STR_rR(REG_WORK2, REG_WORK1);      	// str    r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 8); 	// ldr    r2, [pc, #8]    ; <value>
	LDR_rRI(REG_WORK2, RPC_INDEX, 8); 	// ldr    r3, [pc, #8]    ; <value2>
	STR_rR(REG_WORK2, REG_WORK1);      	// str    r3, [r2]
	B_i(1);                             // b      <jp>

	emit_long(d);						//<value>:
	emit_long(s);						//<value2>:

	//<jp>:
#endif
}

LOWFUNC(NONE,READ,3,raw_mov_w_brR,(W2 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
#else
#	ifdef ARMV6_ASSEMBLY
		LDR_rRI(REG_WORK1, RPC_INDEX, 8); 	// ldr     r2, [pc, #16]   ; <value>
#	else
		LDR_rRI(REG_WORK1, RPC_INDEX, 16); 	// ldr     r2, [pc, #16]   ; <value>
#	endif
#endif
	LDRH_rRR(REG_WORK1, REG_WORK1, s); 	// ldrh    r2, [r2, r6]

#ifdef ARMV6_ASSEMBLY
	PKHBT_rrr(d,REG_WORK1,d);
#else
	BIC_rri(d, d, 0xff);              	// bic     r7, r7, #0xff
	BIC_rri(d, d, 0xff00);             	// bic     r7, r7, #0xff00
	ORR_rrr(d, d, REG_WORK1);          	// orr     r7, r7, r2
#endif

#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b       <jp>

	emit_long(offset);					//<value>:
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_w_bRr,(RR4 d, RR2 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	// ldr  r2,[pc, #offs]
	STRH_rRR(s, d, REG_WORK1);			// strh r6,[r7, r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);	// ldr  r2,[pc,#4]
	STRH_rRR(s, d, REG_WORK1);			// strh r6,[r7, r2]
	B_i(0);								// b 	<jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_w_mr,(IMM d, RR2 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);  // ldr     r2, [pc,#offs]
	STRH_rR(s, REG_WORK1);             	  // strh    r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);  	// ldr     r2, [pc, #4]    ; <value>
	STRH_rR(s, REG_WORK1);             	// strh    r3, [r2]
	B_i(0);                            	// b       <jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,raw_mov_w_ri,(W2 d, IMM s))
{
#if defined(USE_DATA_BUFFER)
    long offs = data_word_offs(s);
	LDR_rRI(REG_WORK2, RPC_INDEX, offs);   	// ldrh    r3, [pc, #offs]
#else
#	ifdef ARMV6_ASSEMBLY
		LDRH_rRI(REG_WORK2, RPC_INDEX, 12);   	// ldrh    r3, [pc, #12]   ; <value>
#	else
		LDRH_rRI(REG_WORK2, RPC_INDEX, 4);   	// ldrh    r3, [pc, #12]   ; <value>
#	endif
#endif

#ifdef ARMV6_ASSEMBLY
	PKHBT_rrr(d,REG_WORK2,d);
#else
	BIC_rri(REG_WORK1, d, 0xff);          	// bic     r2, r7, #0xff
	BIC_rri(REG_WORK1, REG_WORK1, 0xff00); 	// bic     r2, r2, #0xff00
	ORR_rrr(d, REG_WORK2, REG_WORK1);     	// orr     r7, r3, r2
#endif

#if !defined(USE_DATA_BUFFER)
	B_i(0);                                 // b       <jp>

	//<value>:
	emit_word(s);
	skip_word(0);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_w_mi,(MEMW d, IMM s))
{
	// TODO: optimize imm

#if defined(USE_DATA_BUFFER)
  data_check_end(8, 12);
  long offs = data_long_offs(d);

	LDR_rRI(REG_WORK2, RPC_INDEX, offs);  // ldr	r3, [pc, #offs]	; <mem>

	offs = data_word_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]	; <imm>

	STRH_rR(REG_WORK1, REG_WORK2);      // strh	r2, [r3]
#else
	LDR_rRI(REG_WORK2, RPC_INDEX, 8);  	// ldr	r3, [pc, #8]	; <mem>
	LDRH_rRI(REG_WORK1, RPC_INDEX, 8); 	// ldrh	r2, [pc, #8]	; <imm>
	STRH_rR(REG_WORK1, REG_WORK2);      // strh	r2, [r3]
	B_i(1);                             // b	<jp>

	//mem:
	emit_long(d);
	//imm:
	emit_word(s);
	skip_word(0); 						// Alignment

	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,raw_mov_l_mr,(IMM d, RR4 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
	STR_rR(s, REG_WORK1);             	  // str     r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	STR_rR(s, REG_WORK1);             	// str     r3, [r2]
	B_i(0);                           	// b       <jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_w_Ri,(RR4 d, IMM i, IMM offset))
{
	Dif(!isbyte(offset)) abort();

#if defined(USE_DATA_BUFFER)
    long offs = data_word_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]
#else
	LDRH_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldrh	r2, [pc, #4]	; <value>
#endif
	if (offset >= 0)
		STRH_rRI(REG_WORK1, d, offset); // strh	r2, [r7, #0x54]
	else
		STRH_rRi(REG_WORK1, d, -offset);// strh	r2, [r7, #-0x54]
#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_word(i);
	skip_word(0);
	//<jp>:
#endif
}

LOWFUNC(NONE,READ,2,raw_mov_w_rm,(W2 d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 12); 	// ldr	r2, [pc, #12]	; <value>
#endif
	LDRH_rR(REG_WORK1, REG_WORK1);     	// ldrh	r2, [r2]
	LSR_rri(d, d, 16);              	// lsr	r7, r7, #16
	ORR_rrrLSLi(d, REG_WORK1, d, 16);  	// orr	r7, r2, r7, lsl #16
#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,raw_mov_w_rr,(W2 d, RR2 s))
{
	LSL_rri(REG_WORK1, s, 16);					// lsl  r2, r6, #16
	ORR_rrrLSRi(d, REG_WORK1, d, 16);			// orr  r7, r2, r7, lsr #16
	ROR_rri(d, d, 16);							// ror  r7, r7, #16
}

LOWFUNC(NONE,READ,3,raw_mov_w_rR,(W2 d, RR4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();

	if (offset >= 0)
		LDRH_rRI(REG_WORK1, s, offset);		// ldrh	r2, [r6, #12]
	else
		LDRH_rRi(REG_WORK1, s, -offset);	// ldrh	r2, [r6, #-12]

#ifdef ARMV6_ASSEMBLY
	PKHBT_rrr(d,REG_WORK1,d);
#else
	BIC_rri(d, d, 0xff);         			// bic	r7, r7, #0xff
	BIC_rri(d, d, 0xff00);         			// bic	r7, r7, #0xff00
	ORR_rrr(d, d, REG_WORK1);     			// orr	r7, r7, r2
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_w_Rr,(RR4 d, RR2 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();

	if (offset >= 0)
		STRH_rRI(s, d, offset); // strh	r6, [r7, #0x7f]
	else
		STRH_rRi(s, d, -offset);// strh	r6, [r7, #-0x7f]
}

LOWFUNC(NONE,READ,2,raw_mov_l_rm,(W4 d, MEMR s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	  // ldr     r2, [r10, #offs]
	LDR_rR(d, REG_WORK1);              			// ldr     r7, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	LDR_rR(d, REG_WORK1);              	// ldr     r7, [r2]
	B_i(0);                            	// b       <jp>

	emit_long(s);						//<value>:

	//<jp>:
#endif
}

LOWFUNC(NONE,READ,4,raw_mov_l_rm_indexed,(W4 d, MEMR base, RR4 index, IMM factor))
{
	int shft;
	switch(factor) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(base);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);		  // ldr     r2, [pc, #offs]
	LDR_rRR_LSLi(d, REG_WORK1, index, shft);	// ldr   %[d], [r2, %[index], lsl #[shift]]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);		// ldr     r2, [pc, #4]    ; <value>
	LDR_rRR_LSLi(d, REG_WORK1, index, shft);	// ldr   %[d], [r2, %[index], lsl #[shift]]

	B_i(0); 				             				// b       <jp>
	emit_long(base);						//<value>:
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,3,raw_mov_l_Ri,(RR4 d, IMM i, IMM offset8))
{
	Dif(!isbyte(offset8)) abort();

#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr	r2, [pc, #4]	; <value>
#endif
	if (offset8 >= 0)
		STR_rRI(REG_WORK1, d, offset8);  // str	r2, [r7, #0x54]
	else
		STR_rRi(REG_WORK1, d, -offset8); // str	r2, [r7, #-0x54]
#if !defined(USE_DATA_BUFFER)
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(i);
	//<jp>:
#endif
}

LOWFUNC(NONE,READ,3,raw_mov_l_rR,(W4 d, RR4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();

	if (offset >= 0) {
		LDR_rRI(d, s, offset); // ldr r2, [r1, #-12]
	} else
		LDR_rRi(d, s, -offset); // ldr r2, [r1, #12]
}

LOWFUNC(NONE,NONE,2,raw_mov_l_rr,(W4 d, RR4 s))
{
	MOV_rr(d, s); 					// mov     %[d], %[s]
}

LOWFUNC(NONE,WRITE,3,raw_mov_l_Rr,(RR4 d, RR4 s, IMM offset))
{
	Dif(!isbyte(offset)) abort();

	if (offset >= 0)
		STR_rRI(s, d, offset); // str	r6, [r7, #12]
	else
		STR_rRi(s, d, -offset); // str	r6, [r7, #-12]
}

LOWFUNC(NONE,NONE,2,raw_mul_64_32,(RW4 d, RW4 s))
{
	UMULL_rrrr(REG_WORK1, REG_WORK2, d, s);		// umull r2,r3,r7,r6
	MOV_rr(MUL_NREG1, REG_WORK1);				// mov 	 r7,r2
	MOV_rr(MUL_NREG2, REG_WORK2);
}

LOWFUNC(WRITE,NONE,2,raw_or_b,(RW1 d, RR1 s))
{
    AND_rri(REG_WORK1, s, 0xFF);					// and r2, %[s], 0xFF
    ORR_rrr(d, d, REG_WORK1);						// orr %[d], %[d], r2
    LSLS_rri(REG_WORK1, d, 24);						// lsls r2, %[d], #24

	MRS_CPSR(REG_WORK1);                    		// mrs  r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic  r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr  CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_or_w,(RW2 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, s);							// UXTH r2, %[s]
#else
	BIC_rri(REG_WORK1, s, 0xff000000); 				// bic	r2, %[s], #0xff000000
	BIC_rri(REG_WORK1, REG_WORK1, 0x00ff0000); 		// bic	r2, r2, #0x00ff0000
#endif
	ORR_rrr(d, d, REG_WORK1);						// orr %[d], %[d], r2
	LSLS_rri(REG_WORK1, d, 16);						// lsls r2, %[d], #16

	MRS_CPSR(REG_WORK1);                    		// mrs  r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic  r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr  CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_or_l,(RW4 d, RR4 s))
{
	ORRS_rrr(d, d, s);  							// orrs	   r7, r7, r6

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_or_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);	// LDR r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 16);				// LDR r2, [pc,#16] 	; <value>
#endif
	ORRS_rrr(d, d, REG_WORK1);						// ORRS r7,r7,r2

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2

#if !defined(USE_DATA_BUFFER)
	B_i(0);											// b 		<jp>

	// value:
	emit_long(i);
	//jp:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_rol_b_ri,(RW1 r, IMM i))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly
	int imm = 32 - (i & 0x1f);

	MOV_rrLSLi(REG_WORK1, r, 24);						// mov	r2,r7,lsl #24
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 8); 	// orr r2,r2,r2,lsr #8

	RORS_rri(REG_WORK1, REG_WORK1, imm);				// rors	r2,r2,#(32 - (i & 0x1f))

	MRS_CPSR(REG_WORK2);								// mrs	r3,cpsr
	TST_ri(REG_WORK1, 1);								// tst	r2,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG); // orr r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); // bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);

	AND_rri(REG_WORK1, REG_WORK1, 0xff);				// and r2,r2,#0xff
	BIC_rri(r, r, 0xff); 								// bic r7,r7,#0xff
	ORR_rrr(r, r, REG_WORK1);							// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_rol_b_rr,(RW1 d, RR1 r))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly

	MOV_ri(REG_WORK2, 32);								// mov 	r3,#32
	AND_rri(REG_WORK1, r, 0x1f);						// and	r2,r6,#0x1f
	SUB_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// sub r3,r3,r2

	MOV_rrLSLi(REG_WORK1, d, 24);						// mov	r2,r7,lsl #24
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 8);	// orr r2,r2,r2,lsr #8

	RORS_rrr(REG_WORK1, REG_WORK1, REG_WORK2);			// rors	r2,r2,r3

	MRS_CPSR(REG_WORK2);								// mrs	r3,cpsr
	TST_ri(REG_WORK1, 1);								// tst	r2,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);  		// orr  r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 			// bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);

	AND_rri(REG_WORK1, REG_WORK1, 0xff); 				// and r2,r2,#0xff
	BIC_rri(d, d, 0xff);				 				// bic r7,r7,#0xff

	ORR_rrr(d, d, REG_WORK1);							// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_rol_w_ri,(RW2 r, IMM i))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly
	int imm = 32 - (i & 0x1f);

	MOV_rrLSLi(REG_WORK1, r, 16);						// mov	r2,r7,lsl #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16

	RORS_rri(REG_WORK1, REG_WORK1, imm);				// rors	r2,r2,#(32 - (i & 0x1f))

	MRS_CPSR(REG_WORK2);								// mrs	r3,cpsr
	TST_ri(REG_WORK1, 1);								// tst	r2,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);  		// orr r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 			// bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);

	BIC_rri(r, r, 0xff00); 								// bic r2,r2,#0xff00
	BIC_rri(r, r, 0xff); 								// bic r2,r2,#0xff

	ORR_rrrLSRi(r, r, REG_WORK1, 16);					// orr r7,r7,r2,lsr #16
}

LOWFUNC(WRITE,NONE,2,raw_rol_w_rr,(RW2 d, RR1 r))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly

	MOV_ri(REG_WORK2, 32);								// mov 	r3,#32
	AND_rri(REG_WORK1, r, 0x1f);						// and	r2,r6,#0x1f
	SUB_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// sub r3,r3,r2

	MOV_rrLSLi(REG_WORK1, d, 16);						// mov	r2,r7,lsl #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16

	RORS_rrr(REG_WORK1, REG_WORK1, REG_WORK2);			// rors	r2,r2,r3

	MRS_CPSR(REG_WORK2);								// mrs	r3,cpsr
	TST_ri(REG_WORK1, 1);								// tst	r2,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);  		// orr  r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 			// bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);

	BIC_rri(d, d, 0xff00); 								// bic r2,r2,#0xff00
	BIC_rri(d, d, 0xff); 								// bic r2,r2,#0xff

	ORR_rrrLSRi(d, d, REG_WORK1, 16);					// orr r2,r2,r7,lsr #16
}

LOWFUNC(WRITE,NONE,2,raw_rol_l_ri,(RW4 r, IMM i))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly
	int imm = 32 - (i & 0x1f);

	RORS_rri(r, r, imm);						// rors	r7,r7,#(32 - (i & 0x1f))

	MRS_CPSR(REG_WORK2);						// mrs	r3,cpsr
	TST_ri(r, 1);								// tst	r7,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);  // orr r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); // bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);
}

LOWFUNC(WRITE,NONE,2,raw_ror_l_ri,(RW4 r, IMM i))
{
	RORS_rri(r, r, i & 0x1F);	// RORS r7,r7,#12
}

LOWFUNC(WRITE,NONE,2,raw_rol_l_rr,(RW4 d, RR1 r))
{
	// TODO: Check if the Bittest is necessary. compemu.c seems to do it itself, but meanwhile make sure, that carry is set correctly

	MOV_ri(REG_WORK1, 32);						// mov 	r2,#32
	AND_rri(REG_WORK2, r, 0x1f);				// and	r3,r6,#0x1f
	SUB_rrr(REG_WORK1, REG_WORK1, REG_WORK2); 	// sub r2,r2,r3

	RORS_rrr(d, d, REG_WORK1);					// rors	r7,r7,r2

	MRS_CPSR(REG_WORK2);						// mrs	r3,cpsr
	TST_ri(d, 1);								// tst	r7,#1
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG);  // orr r3,r3,#0x20000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); // bic r3,r3,#0x20000000
	MSR_CPSR_r(REG_WORK2);
}

LOWFUNC(WRITE,NONE,2,raw_ror_l_rr,(RW4 d, RR1 r))
{
	RORS_rrr(d, d, r);			// RORS r7,r7,r6
}

LOWFUNC(WRITE,NONE,2,raw_ror_b_ri,(RW1 r, IMM i))
{
	MOV_rrLSLi(REG_WORK1, r, 24); 						// mov r2,r7,lsl #24
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16); 	// orr r2,r2,r2,lsr #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 8); 	// orr r2,r2,r2,lsr #8

	RORS_rri(REG_WORK1, REG_WORK1, i & 0x1f);			// rors r2,r2,#12

	AND_rri(REG_WORK1, REG_WORK1, 0xff); 				// and r2,r2,#0xff
	BIC_rri(r, r, 0xff);								// bic r7,r7,#0xff
	ORR_rrr(r, r, REG_WORK1);							// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_ror_b_rr,(RW1 d, RR1 r))
{
	MOV_rrLSLi(REG_WORK1, d, 24); 						// mov r2,r7,lsl #24
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16); 	// orr r2,r2,r2,lsr #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 8); 	// orr r2,r2,r2,lsr #8

	RORS_rrr(REG_WORK1, REG_WORK1, r);					// rors r2,r2,r6

	AND_rri(REG_WORK1, REG_WORK1, 0xff); 				// and r2,r2,#0xff
	BIC_rri(d, d, 0xff);								// bic r7,r7,#0xff
	ORR_rrr(d, d, REG_WORK1);							// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_ror_w_ri,(RW2 r, IMM i))
{
	MOV_rrLSLi(REG_WORK1, r, 16);						// mov r2,r7,lsl #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16

	RORS_rri(REG_WORK1, REG_WORK1, i & 0x1f); 			// RORS r2,r2,#12

	BIC_rri(r, r, 0xff00); 								// bic r7,r7,#0xff00
	BIC_rri(r, r, 0xff); 								// bic r7,r7,#0xff

	ORR_rrrLSRi(r, r, REG_WORK1, 16);					// orr r7,r7,r2,lsr #16
}

LOWFUNC(WRITE,NONE,2,raw_ror_w_rr,(RW2 d, RR1 r))
{
	MOV_rrLSLi(REG_WORK1, d, 16);						// mov r2,r7,lsl #16
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, REG_WORK1, 16);	// orr r2,r2,r2,lsr #16

	RORS_rrr(REG_WORK1, REG_WORK1, r); 					// RORS r2,r2,r6

	BIC_rri(d, d, 0xff00); 								// bic r7,r7,#0xff00
	BIC_rri(d, d, 0xff); 								// bic r7,r7,#0xff

	ORR_rrrLSRi(d, d, REG_WORK1, 16);					// orr r7,r7,r2,lsr #16
}

LOWFUNC(RMW,NONE,2,raw_sbb_b,(RW1 d, RR1 s))
{
	MRS_CPSR(REG_WORK1);                        	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     		// msr     CPSR_fc, r2

	LSL_rri(REG_WORK2, d, 24); 					// lsl     r3, %[d], #24
	LSL_rri(REG_WORK1, s, 24);  				// lsl     r2, r6, #24

	SBCS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs    r3, r3, r2
	BIC_rri(d, d, 0xFF);
	ORR_rrrLSRi(d, d, REG_WORK2, 24);  					// orr     r7, r7, r3

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(RMW,NONE,2,raw_sbb_l,(RW4 d, RR4 s))
{
	MRS_CPSR(REG_WORK1);                        	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     		// msr     CPSR_fc, r2

	SBCS_rrr(d, d, s);								// sbcs	   r7, r7, r6

	MRS_CPSR(REG_WORK1);                        	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     		// msr     CPSR_fc, r2
}

LOWFUNC(RMW,NONE,2,raw_sbb_w,(RW2 d, RR2 s))
{
	MRS_CPSR(REG_WORK1);                        	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     		// msr     CPSR_fc, r2

	LSL_rri(REG_WORK2, d, 16); 					// lsl     r3, %[d], #24
	LSL_rri(REG_WORK1, s, 16);  				// lsl     r2, r6, #16

	SBCS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs    r3, r3, r2
	BIC_rri(d,d, 0xff);
	BIC_rri(d,d, 0xff00);
	ORR_rrrLSRi(d, d, REG_WORK2, 16);  					// orr     r7, r7, r3

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(READ,NONE,2,raw_setcc,(W1 d, IMM cc))
{
	switch (cc) {
		case 9: // LS
			BEQ_i(0);										// beq  <doset>
			BCC_i(1);										// bcs 	<unset>

			MOV_ri(d, 1);									// mov	r7,#0
			B_i(0); 			            				// b    <continue>

			//<unset>:
			MOV_ri(d, 0);									// mov	r7,#1
			break;

		case 8: // HI
			BEQ_i(2);										// beq  <unset>  Z != 0
			BCS_i(1);										// bcc  <doset>  C = 0

			//<unset>:
			MOV_ri(d, 1);									// mov	r7,#0
			B_i(0); 			            				// b    <continue>

			//<doset>:
			MOV_ri(d, 0);									// mov	r7,#1
			break;

		default:
			CC_MOV_ri(cc, d, 1);		 					// MOVcc	R7,#1
			CC_MOV_ri(cc^1, d, 0);				 			// MOVcc^1	R7,#0
			break;
		}
	//<continue>:
}

LOWFUNC(READ,WRITE,2,raw_setcc_m,(MEMW d, IMM cc))
{
	switch (cc) {
		case 9: // LS
			BEQ_i(0);										// beq  <doset>
			BCC_i(1);										// bcs 	<doset>

			MOV_ri(REG_WORK1, 1);							// mov	r2,#0
			B_i(0); 			            				// b    <continue>

			//<doset>:
			MOV_ri(REG_WORK1, 0);							// mov	r2,#1
			break;

		case 8: // HI
			BEQ_i(2);										// beq  <unset>  Z != 0
			BCS_i(1);										// bcc  <doset>  C = 0

			MOV_ri(REG_WORK1, 1);							// mov	r2,#0
			B_i(0); 			            				// b    <continue>

			//<doset>:
			MOV_ri(REG_WORK1, 0);							// mov	r2,#1
			break;

		default:
			CC_MOV_ri(cc, REG_WORK1, 1);		 			// MOVcc	R2,#1
			CC_MOV_ri(cc^1, REG_WORK1, 0);		 			// MOVcc^1	R2,#0
			break;
		}
	//<continue>:
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK2, RPC_INDEX, offs); 					// LDR	R3,[PC, #offs]
#else
	LDR_rRI(REG_WORK2, RPC_INDEX, 4); 						// LDR	R3,[PC, #4]
#endif
	STRB_rR(REG_WORK1, REG_WORK2);							// STRB	R2,[R3]
#if !defined(USE_DATA_BUFFER)
	B_i(0);													// B	<jp>

	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_shll_b_ri,(RW1 r, IMM i))
{
	LSL_rri(REG_WORK1, r, 24);					// LSL r2,r7,#24

	LSLS_rri(REG_WORK1, REG_WORK1, i & 0x1f);	// LSLS r2,r2,#12

	BIC_rri(r, r, 0xff);						// BIC r7,r7,0xff
	ORR_rrrLSRi(r, r, REG_WORK1, 24); 			// ORR r7,r7,r2,lsr #24
}

LOWFUNC(WRITE,NONE,2,raw_shll_b_rr,(RW1 d, RR1 r))
{
	LSL_rri(REG_WORK1, d, 24);					// LSL r2,r7,#24
	LSLS_rrr(REG_WORK1, REG_WORK1, r);			// LSLS r2,r2,r6
	BIC_rri(d, d, 0xff);						// BIC r7,r7,#0xff
	ORR_rrrLSRi(d, d, REG_WORK1, 24); 			// ORR r7,r7,r2,lsr #24
}

LOWFUNC(WRITE,NONE,2,raw_shll_l_ri,(RW4 r, IMM i))
{
	LSLS_rri(r,r, i & 0x1f); 					// lsls r7,r7,#12
}

LOWFUNC(WRITE,NONE,2,raw_shll_l_rr,(RW4 d, RR1 r))
{
	LSLS_rrr(d, d, r);
}

LOWFUNC(WRITE,NONE,2,raw_shll_w_ri,(RW2 r, IMM i))
{
	LSL_rri(REG_WORK1, r, 16);					// LSL r2,r7,#16
	LSLS_rri(REG_WORK1, REG_WORK1, i&0x1f);		// LSLS r2,r2,#12

	ORR_rrrLSRi(REG_WORK1, REG_WORK1, r, 16); 	// ORR r2,r2,r7,lsr #16

	ROR_rri(r, REG_WORK1, 16);					// ROR r7,r2,#16
}

LOWFUNC(WRITE,NONE,2,raw_shll_w_rr,(RW2 d, RR1 r))
{
	LSL_rri(REG_WORK1, d, 16);					// LSL r2,r7,#16
	LSLS_rrr(REG_WORK1, REG_WORK1, r);			// LSLS r2,r2,r6
	ORR_rrrLSRi(REG_WORK1, REG_WORK1, d, 16); 	// ORR r2,r2,r7,lsr #16
	ROR_rri(d, REG_WORK1, 16);					// ROR r7,r2,#16
}

LOWFUNC(WRITE,NONE,2,raw_shra_b_ri,(RW1 r, IMM i))
{
	LSL_rri(REG_WORK1, r, 24); 					// lsl r2,r7,#24
	ASR_rri(REG_WORK1, REG_WORK1, 24); 			// asr r2,r2,#24

	ASRS_rri(REG_WORK1, REG_WORK1, i & 0x1f); 	// asrs r2,r2,#12

	AND_rri(REG_WORK1, REG_WORK1, 0xff);		// and r2,r2,#0xff
	BIC_rri(r,r, 0xff); 						// bic r7,r7,#0xff
	ORR_rrr(r,r,REG_WORK1); 					// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shra_b_rr,(RW1 d, RR1 r))
{
	LSL_rri(REG_WORK1, d, 24); 					// lsl r2,r7,#24
	ASR_rri(REG_WORK1, REG_WORK1, 24); 			// asr r2,r2,#24

	ASRS_rrr(REG_WORK1, REG_WORK1, r); 			// asrs r2,r2,r6

	AND_rri(REG_WORK1, REG_WORK1, 0xff);		// and r2,r2,#0xff
	BIC_rri(d,d, 0xff); 						// bic r7,r7,#0xff

	ORR_rrr(d,d,REG_WORK1); 					// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shra_w_ri,(RW2 r, IMM i))
{
	LSL_rri(REG_WORK1, r, 16); 					// lsl r2,r7,#16
	ASR_rri(REG_WORK1, REG_WORK1, 16); 			// asr r2,r2,#16

	ASRS_rri(REG_WORK1, REG_WORK1, i & 0x1f); 	// asrs r2,r2,#12

#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, REG_WORK1);
#else
	BIC_rri(REG_WORK1, REG_WORK1, 0xff000000);
	BIC_rri(REG_WORK1, REG_WORK1, 0xff0000);
#endif

	BIC_rri(r,r,0xff00); 						// bic r7,r7,#0xff00
	BIC_rri(r,r,0xff); 							// bic r7,r7,#0xff

	ORR_rrr(r,r,REG_WORK1); 					// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shra_w_rr,(RW2 d, RR1 r))
{
	LSL_rri(REG_WORK1, d, 16); 					// lsl r2,r7,#16
	ASR_rri(REG_WORK1, REG_WORK1, 16); 			// asr r2,r2,#16

	ASRS_rrr(REG_WORK1, REG_WORK1, r); 			// asrs r2,r2,r6

#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, REG_WORK1);
#else
	BIC_rri(REG_WORK1, REG_WORK1, 0xff000000); 	// bic r2,r2,#0xff000000
	BIC_rri(REG_WORK1, REG_WORK1, 0xff0000); 	// bic r2,r2,#0xff0000
#endif

	BIC_rri(d,d, 0xff00); 						// bic r7,r7,#0xff00
	BIC_rri(d,d, 0xff); 						// bic r7,r7,#0xff

	ORR_rrr(d,d,REG_WORK1); 					// orr r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shra_l_ri,(RW4 r, IMM i))
{
	ASRS_rri(r, r, i & 0x1f);					// ASRS r7,r7,#12
}

LOWFUNC(WRITE,NONE,2,raw_shra_l_rr,(RW4 d, RR1 r))
{
	ASRS_rrr(d, d, r);							// ASRS r7,r7,r6
}

LOWFUNC(WRITE,NONE,2,raw_shrl_b_ri,(RW1 r, IMM i))
{
	AND_rri(REG_WORK1, r, 0xff);				// AND r2,r7,#0xFF

	LSRS_rri(REG_WORK1, REG_WORK1, i & 0x1f);	// LSRS r2,r2,r6

	BIC_rri(r, r, 0xFF);						// BIC r7,r7,#0xff
	ORR_rrr(r, r, REG_WORK1); 					// ORR r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shrl_b_rr,(RW1 d, RR1 r))
{
	AND_rri(REG_WORK1, d, 0xff);				// AND r2,r7,#0xFF

	LSRS_rrr(REG_WORK1, REG_WORK1, r);			// LSRS r2,r2,r6

	BIC_rri(d, d, 0xFF);						// BIC r7,r7,#0xff
	ORR_rrr(d, d, REG_WORK1); 					// ORR r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shrl_l_ri,(RW4 r, IMM i))
{
	LSRS_rri(r, r, i & 0x1f);					// LSRS r7,r7,#12
}

LOWFUNC(WRITE,NONE,2,raw_shrl_w_ri,(RW2 r, IMM i))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, r);
#else
	BIC_rri(REG_WORK1, r, 0xff0000);			// BIC r2,r7,#0xff0000
	BIC_rri(REG_WORK1, REG_WORK1, 0xff000000);	// BIC r2,r2,#0xff000000
#endif

	LSRS_rri(REG_WORK1, REG_WORK1, i & 0x1f);	// LSRS r2,r2,#12

	BIC_rri(r, r, 0xFF);						// BIC r7,r7,#0xff
	BIC_rri(r, r, 0xFF00);						// BIC r7,r7,#0xff00
	ORR_rrr(r, r, REG_WORK1); 					// ORR r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shrl_w_rr,(RW2 d, RR1 r))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, d);
#else
	BIC_rri(REG_WORK1, d, 0xff0000);			// BIC r2,r7,#0xff0000
	BIC_rri(REG_WORK1, REG_WORK1, 0xff000000);	// BIC r2,r2,#0xff000000
#endif

	LSRS_rrr(REG_WORK1, REG_WORK1, r);			// LSRS r2,r2,r6

	BIC_rri(d, d, 0xFF);						// BIC r7,r7,#0xff
	BIC_rri(d, d, 0xFF00);						// BIC r7,r7,#0xff00
	ORR_rrr(d, d, REG_WORK1); 					// ORR r7,r7,r2
}

LOWFUNC(WRITE,NONE,2,raw_shrl_l_rr,(RW4 d, RR1 r))
{
	LSRS_rrr(d, d, r);
}

LOWFUNC(WRITE,NONE,2,raw_sub_b,(RW1 d, RR1 s))
{
	LSL_rri(REG_WORK1, s, 24);  				// lsl     r2, r6, #24
	LSL_rri(REG_WORK2, d, 24); 					// lsl     r3, r7, #24

	SUBS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs    r3, r3, r2
	BIC_rri(d, d, 0xFF);
	ORR_rrrLSRi(d, d, REG_WORK2, 24);  			// orr     r7, r7, r3

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_sub_b_ri,(RW1 d, IMM i))
{
	LSL_rri(REG_WORK2, d, 24);  				// lsl     r3, r7, #24

	SUBS_rri(REG_WORK2, REG_WORK2, i << 24); 	// subs    r3, r3, #0x12000000
	BIC_rri(d, d, 0xFF);						// bic	   r7, r7, #0xFF
	ORR_rrrLSRi(d, d, REG_WORK2, 24);			// orr     r7, r7, r3, lsr #24

	MRS_CPSR(REG_WORK1);                       	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_sub_l,(RW4 d, RR4 s))
{
	SUBS_rrr(d, d, s); 								// subs  r7, r7, r6

	MRS_CPSR(REG_WORK1);                        	// mrs   r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		  		   	     	// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_sub_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 16); 			// ldr     r2, [pc, #16]    ; <value>
#endif
	SUBS_rrr(d, d, REG_WORK1); 					// subs    r7, r7, r2

	MRS_CPSR(REG_WORK1);                       	// mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);	// eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2

#if !defined(USE_DATA_BUFFER)
	B_i(0); 									// b       <jp>

	//<value>:
	emit_long(i);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_sub_w,(RW2 d, RR2 s))
{
	LSL_rri(REG_WORK1, s, 16);  				// lsl     r2, r6, #16
	LSL_rri(REG_WORK2, d, 16); 					// lsl     r3, r7, #16

	SUBS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs    r3, r3, r2
	BIC_rri(d, d, 0xff);
	BIC_rri(d, d, 0xff00);
	ORR_rrrLSRi(d, d, REG_WORK2, 16);  			// orr     r7, r7, r3

	MRS_CPSR(REG_WORK1);                        // mrs     r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);  // eor     r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1); 		     	     	// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_sub_w_ri,(RW2 d, IMM i))
{
	// TODO: optimize_imm

#if defined(USE_DATA_BUFFER)
    long offs = data_word_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);   	  // ldr	r2, [pc, #offs]	; <value>
#else
	LDRH_rRI(REG_WORK1, RPC_INDEX, 36);   		// ldrh	r2, [pc, #36]	; <value>
#endif
	LSL_rri(REG_WORK1, REG_WORK1, 16);      	// lsl	r2, r2, #16
	LSL_rri(REG_WORK2, d, 16);              	// lsl	r3, r6, #16

	SUBS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs	r3, r3, r2
	BIC_rri(d, d, 0xff);
	BIC_rri(d, d, 0xff00);
	ORR_rrrLSRi(d, d, REG_WORK2, 16);          	// orr	r6, r3, r6, lsr #16

	MRS_CPSR(REG_WORK1);                       	// mrs	r2, CPSR
	EOR_rri(REG_WORK1, REG_WORK1, ARM_C_FLAG);	// eor	r2, r2, #0x20000000
	MSR_CPSR_r(REG_WORK1);                     	// msr	CPSR_fc, r2

#if !defined(USE_DATA_BUFFER)
	B_i(0);                                    	// b	<jp>

	emit_word(i);
	skip_word(0);					//<value>:

	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_test_b_rr,(RR1 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	SXTB_rr(REG_WORK1, s);
	SXTB_rr(REG_WORK2, d);
#else
	LSL_rri(REG_WORK1, s, 24); 						// lsl	   r2, r6, #24
	LSL_rri(REG_WORK2, d, 24); 						// lsl	   r3, r7, #24
#endif

	TST_rr(REG_WORK2, REG_WORK1);  					// tst     r3, r2
	
	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_test_l_ri,(RR4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);    	  // ldr	   r2, [pc, #offs]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 16);    			// ldr	   r2, [pc, #16] ; <value>
#endif
	TST_rr(d, REG_WORK1);                  			// tst     r7, r2

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2

#if !defined(USE_DATA_BUFFER)
	B_i(0);											// b	   <jp>

	//<value>:
	emit_long(i);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,raw_test_l_rr,(RR4 d, RR4 s))
{
	TST_rr(d, s);                          			// tst     r7, r6

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_test_w_rr,(RR2 d, RR2 s))
{
#ifdef ARMV6_ASSEMBLY
	SXTH_rr(REG_WORK1, s);
	SXTH_rr(REG_WORK2, d);
#else
	LSL_rri(REG_WORK1, s, 16); 						// lsl	   r2, r6, #16
	LSL_rri(REG_WORK2, d, 16); 						// lsl	   r3, r7, #16
#endif

	TST_rr(REG_WORK2, REG_WORK1);  					// tst     r3, r2

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_xor_b,(RW1 d, RR1 s))
{
	 AND_rri(REG_WORK1, s, 0xFF);					// and r2, %[s], 0xFF
	 EOR_rrr(d, d, REG_WORK1);						// eor %[d], %[d], r2
	 LSLS_rri(REG_WORK1, d, 24);					// lsls r2, %[d], #24

	 MRS_CPSR(REG_WORK1);                    		// mrs  r2, CPSR
	 BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic  r2, r2, #0x30000000
	 MSR_CPSR_r(REG_WORK1);                    		// msr  CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_xor_w,(RW2 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, s);							// UXTH r2, %[s]
#else
	BIC_rri(REG_WORK1, s, 0xff000000); 				// bic	r2, %[s], #0xff000000
	BIC_rri(REG_WORK1, REG_WORK1, 0x00ff0000); 		// bic	r2, r2, #0x00ff0000
#endif
	EOR_rrr(d, d, REG_WORK1);						// eor %[d], %[d], r2
	LSLS_rri(REG_WORK1, d, 16);						// lsls r2, %[d], #16

	MRS_CPSR(REG_WORK1);                    		// mrs  r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS);	// bic  r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr  CPSR_fc, r2
}

LOWFUNC(WRITE,NONE,2,raw_xor_l,(RW4 d, RR4 s))
{
	EORS_rrr(d, d, s);  							// eors	   r7, r7, r6

	MRS_CPSR(REG_WORK1);                    		// mrs     r2, CPSR
	BIC_rri(REG_WORK1, REG_WORK1, ARM_CV_FLAGS); 	// bic     r2, r2, #0x30000000
	MSR_CPSR_r(REG_WORK1);                    		// msr     CPSR_fc, r2
}

LOWFUNC(NONE,NONE,2,raw_sign_extend_16_rr,(W4 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	SXTH_rr(d, s);			// sxth %[d],%[s]
#else
	LSL_rri(d, s, 16); // lsl	r6, r7, #16
	ASR_rri(d, d, 16); // asr	r6, r6, #16
#endif
}

LOWFUNC(NONE,NONE,2,raw_sign_extend_8_rr,(W4 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	SXTB_rr(d, s);		// SXTB %[d],%[s]
#else
	ROR_rri(d, s, 8); 	// ror	r6, r7, #8
	ASR_rri(d, d, 24); 	// asr	r6, r6, #24
#endif
}

LOWFUNC(NONE,NONE,2,raw_zero_extend_8_rr,(W4 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTB_rr(d, s);									// UXTB %[d], %[s]
#else
	ROR_rri(d, s, 8); 	// ror	r2, r1, #8
	LSR_rri(d, d, 24); 	// lsr	r2, r2, #24
#endif
}

LOWFUNC(NONE,NONE,2,raw_zero_extend_16_rr,(W4 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(d, s);									// UXTH %[d], %[s]
#else
	BIC_rri(d, s, 0xff000000); 						// bic	%[d], %[s], #0xff000000
	BIC_rri(d, d, 0x00ff0000); 						// bic	%[d], %[d], #0x00ff0000
#endif
}

static inline void raw_dec_sp(int off)
{
	if (off) {
		LDR_rRI(REG_WORK1, RPC_INDEX, 4); 			// ldr     r2, [pc, #4]    ; <value>
		SUB_rrr(RSP_INDEX, RSP_INDEX, REG_WORK1);	// sub    r7, r7, r2
		B_i(0); 									// b       <jp>
		//<value>:
		emit_long(off);
	}
}

static inline void raw_inc_sp(int off)
{
	if (off) {
		LDR_rRI(REG_WORK1, RPC_INDEX, 4); 			// ldr     r2, [pc, #4]    ; <value>
		ADD_rrr(RSP_INDEX, RSP_INDEX, REG_WORK1);	// sub    r7, r7, r2
		B_i(0); 									// b       <jp>
		//<value>:
		emit_long(off);
	}
}

static inline void raw_push_regs_to_preserve(void) {
	PUSH_REGS(PRESERVE_MASK);
}

static inline void raw_pop_preserved_regs(void) {
	POP_REGS(PRESERVE_MASK);
}

// Verify!!!
/* FLAGX is byte sized, and we *do* write it at that size */
static inline void raw_load_flagx(uae_u32 t)
{
    raw_mov_l_rm(t,(uintptr)live.state[FLAGX].mem);
}

static inline void raw_flags_evicted(int r)
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

static inline void raw_flags_init(void) {
}

static __inline__ void raw_flags_set_zero(int s, int tmp)
{
	 raw_mov_l_rr(tmp,s);
	 MRS_CPSR(s);
	 BIC_rri(s,s,ARM_Z_FLAG);
	 AND_rri(tmp,tmp,ARM_Z_FLAG);
	 EOR_rri(tmp,tmp,ARM_Z_FLAG);
	 ORR_rrr(s,s,tmp);
	 MSR_CPSR_r(s);
}

static inline void raw_flags_to_reg(int r)
{
	MRS_CPSR(r);
	raw_mov_l_mr((uintptr)live.state[FLAGTMP].mem,r);
	raw_flags_evicted(r);
}

static inline void raw_reg_to_flags(int r)
{
	MSR_CPSR_r(r); 		// msr CPSR_fc, %r
}

/* Apparently, there are enough instructions between flag store and
   flag reload to avoid the partial memory stall */
static inline void raw_load_flagreg(uae_u32 t)
{
	raw_mov_l_rm(t,(uintptr)live.state[FLAGTMP].mem);
}

/* %eax register is clobbered if target processor doesn't support fucomi */
#define FFLAG_NREG_CLOBBER_CONDITION !have_cmov
#define FFLAG_NREG R0_INDEX
#define FLAG_NREG2 -1
#define FLAG_NREG1 -1
#define FLAG_NREG3 -1

static inline void raw_fflags_into_flags(int r)
{
	jit_unimplemented("raw_fflags_into_flags %x", r);
}

static inline void raw_fp_init(void)
{
    int i;

    for (i=0;i<N_FREGS;i++)
	live.spos[i]=-2;
    live.tos=-1;  /* Stack is empty */
}

// Verify
static inline void raw_fp_cleanup_drop(void)
{
D(panicbug("raw_fp_cleanup_drop"));

    while (live.tos>=1) {
//	emit_byte(0xde);
//	emit_byte(0xd9);
	live.tos-=2;
    }
    while (live.tos>=0) {
//	emit_byte(0xdd);
//	emit_byte(0xd8);
	live.tos--;
    }
    raw_fp_init();
}

LOWFUNC(NONE,WRITE,2,raw_fmov_mr_drop,(MEMPTRW m, FR r))
{
	jit_unimplemented("raw_fmov_mr_drop %x %x", m, r);
}

LOWFUNC(NONE,WRITE,2,raw_fmov_mr,(MEMPTRW m, FR r))
{
	jit_unimplemented("raw_fmov_mr %x %x", m, r);
}

LOWFUNC(NONE,READ,2,raw_fmov_rm,(FW r, MEMPTRR m))
{
	jit_unimplemented("raw_fmov_rm %x %x", r, m);
}

LOWFUNC(NONE,NONE,2,raw_fmov_rr,(FW d, FR s))
{
	jit_unimplemented("raw_fmov_rr %x %x", d, s);
}

static inline void raw_emit_nop_filler(int nbytes)
{
	nbytes >>= 2;
	while(nbytes--) { NOP(); }
}

static inline void raw_emit_nop(void)
{
  NOP();
}

#ifdef UAE
static
#endif
void compiler_status() {
	jit_log("compiled code starts at %p, current at %p (size 0x%x)", compiled_code, current_compile_p, (unsigned int)(current_compile_p - compiled_code));
}

//
// ARM doesn't have bsf, but clz is a good alternative instruction for it
//
static bool target_check_bsf(void)
{
	return false;
}

static void raw_init_cpu(void)
{
	/* Have CMOV support, because ARM support conditions for all instructions */
	have_cmov = true;

	align_loops = 0;
	align_jumps = 0;

	raw_flags_init();
}

//
// Arm instructions
//
LOWFUNC(WRITE,NONE,2,raw_ADD_l_rr,(RW4 d, RR4 s))
{
	ADD_rrr(d, d, s);
}

LOWFUNC(WRITE,NONE,2,raw_ADD_l_rri,(RW4 d, RR4 s, IMM i))
{
	ADD_rri(d, s, i);
}

LOWFUNC(WRITE,NONE,2,raw_SUB_l_rri,(RW4 d, RR4 s, IMM i))
{
	SUB_rri(d, s, i);
}

LOWFUNC(WRITE,NONE,2,raw_AND_b_rr,(RW1 d, RR1 s))
{
	MVN_rrLSLi(REG_WORK1, s, 24);						// mvn 	r2, %[s], lsl #24
	MVN_rrLSRi(REG_WORK1, REG_WORK1, 24);				// mvn 	r2, %[s], lsr #24
	AND_rrr(d, d, REG_WORK1);							// and  %[d], %[d], r2
}

LOWFUNC(WRITE,NONE,2,raw_AND_l_rr,(RW4 d, RR4 s))
{
	AND_rrr(d, d, s);
}

LOWFUNC(WRITE,NONE,2,raw_AND_l_ri,(RW4 d, IMM i))
{
	AND_rri(d, d, i);
}

LOWFUNC(WRITE,NONE,2,raw_AND_w_rr,(RW2 d, RR2 s))
{
	MVN_rrLSLi(REG_WORK1, s, 16);						// mvn 		r2, %[s], lsl #16
	MVN_rrLSRi(REG_WORK1, REG_WORK1, 16);				// mvn 		r2, %[s], lsr #16
	AND_rrr(d, d, REG_WORK1); 							// and    	%[d], %[d], r2
}

LOWFUNC(WRITE,NONE,2,raw_EOR_b_rr,(RW1 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTB_rr(REG_WORK1, s);							// UXTH r2, %[s]
#else
    AND_rri(REG_WORK1, s, 0xFF);					// and r2, %[s], 0xFF
#endif
    EOR_rrr(d, d, REG_WORK1);						// eor %[d], %[d], r2
}

LOWFUNC(WRITE,NONE,2,raw_EOR_l_rr,(RW4 d, RR4 s))
{
	EOR_rrr(d, d, s);  							// eors	   r7, r7, r6
}

LOWFUNC(WRITE,NONE,2,raw_EOR_w_rr,(RW2 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, s);							// UXTH r2, %[s]
	EOR_rrr(d, d, REG_WORK1);						// eor %[d], %[d], r2
#else
	LSL_rri(REG_WORK1, s, 16); 				// bic	r2, %[s], #0xff000000
	EOR_rrrLSRi(d, d, REG_WORK1, 16);						// orr %[d], %[d], r2
#endif
}

LOWFUNC(WRITE,NONE,2,raw_LDR_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(d, RPC_INDEX, offs);			// ldr     r2, [pc, #offs]
#else
	LDR_rR(d, RPC_INDEX);
	B_i(0);
	emit_long(i);
#endif
}

LOWFUNC(WRITE,NONE,2,raw_MOV_l_ri8,(RW4 d, IMM i))
{
	MOV_ri(d, i);
}

LOWFUNC(WRITE,NONE,2,raw_ORR_b_rr,(RW1 d, RR1 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTB_rr(REG_WORK1, s);							// UXTH r2, %[s]
#else
    AND_rri(REG_WORK1, s, 0xFF);					// and r2, %[s], 0xFF
#endif
    ORR_rrr(d, d, REG_WORK1);						// orr %[d], %[d], r2
}

LOWFUNC(WRITE,NONE,2,raw_ORR_l_rr,(RW4 d, RR4 s))
{
	ORR_rrr(d, d, s);
}

LOWFUNC(WRITE,NONE,2,raw_ORR_w_rr,(RW2 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(REG_WORK1, s);							// UXTH r2, %[s]
	ORR_rrr(d, d, REG_WORK1);						// orr %[d], %[d], r2
#else
	LSL_rri(REG_WORK1, s, 16); 				// bic	r2, %[s], #0xff000000
	ORR_rrrLSRi(d, d, REG_WORK1, 16);						// orr %[d], %[d], r2
#endif
}

LOWFUNC(WRITE,NONE,2,raw_ROR_l_ri,(RW4 r, IMM i))
{
	ROR_rri(r, r, i);
}

//
// compuemu_support used raw calls
//
LOWFUNC(WRITE,RMW,2,compemu_raw_add_l_mi,(IMM d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  data_check_end(8, 24);
  long target = data_long(d, 24);
  long offs = get_data_offset(target);
  
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);         		// ldr	r2, [pc, #offs]	; d
	LDR_rR(REG_WORK2, REG_WORK1);             			// ldr	r3, [r2]

  offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);         		// ldr	r2, [pc, #offs]	; s

	ADD_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds	r3, r3, r2

	offs = get_data_offset(target);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);          	// ldr	r2, [pc, #offs]	; d
	STR_rR(REG_WORK2, REG_WORK1);              			// str	r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 20);         			// ldr	r2, [pc, #20]	; <value>
	LDR_rR(REG_WORK2, REG_WORK1);             			// ldr	r3, [r2]

	LDR_rRI(REG_WORK1, RPC_INDEX, 16);         			// ldr	r2, [pc, #16]	; <value2>

	ADD_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 			// adds	r3, r3, r2

	LDR_rRI(REG_WORK1, RPC_INDEX, 4);          			// ldr	r2, [pc, #4]	; <value>
	STR_rR(REG_WORK2, REG_WORK1);              			// str	r3, [r2]

	B_i(1);                                    			// b	<jp>

	//<value>:
	emit_long(d);
	//<value2>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,compemu_raw_and_l_ri,(RW4 d, IMM i))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(i);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]   ; <value>
	AND_rrr(d, d, REG_WORK1); 					  // ands    %[d], %[d], r2
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 				// ldr     r2, [pc, #16]   ; <value>
	AND_rrr(d, d, REG_WORK1); 					// ands    %[d], %[d], r2
	B_i(0);
	emit_long(i);
#endif
}

LOWFUNC(NONE,NONE,1,compemu_raw_bswap_32,(RW4 r))
{
#if defined(ARMV6_ASSEMBLY)
	REV_rr(r,r);								// rev 	   %[r],%[r]
#else
	EOR_rrrRORi(REG_WORK1, r, r, 16);      		// eor     r2, r6, r6, ror #16
	BIC_rri(REG_WORK1, REG_WORK1, 0xff0000); 	// bic     r2, r2, #0xff0000
	ROR_rri(r, r, 8); 							// ror     r6, r6, #8
	EOR_rrrLSRi(r, r, REG_WORK1, 8);      		// eor     r6, r6, r2, lsr #8
#endif
}

LOWFUNC(WRITE,NONE,2,compemu_raw_bt_l_ri,(RR4 r, IMM i))
{
	int imm = (1 << (i & 0x1f));

	MRS_CPSR(REG_WORK2);                    	// mrs        r3, CPSR
	TST_ri(r, imm);                            	// tst        r6, #0x1000000
	CC_BIC_rri(NATIVE_CC_EQ, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// bic        r3, r3, #0x20000000
	CC_ORR_rri(NATIVE_CC_NE, REG_WORK2, REG_WORK2, ARM_C_FLAG); 	// orr        r3, r3, #0x20000000
	MSR_CPSR_r(REG_WORK2);                    	// msr        CPSR_fc, r3
}

LOWFUNC(NONE,READ,5,compemu_raw_cmov_l_rm_indexed,(W4 d, IMM base, RR4 index, IMM factor, IMM cond))
{
	int shft;
	switch(factor) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

	switch (cond) {
	case 9: // LS
		jit_unimplemented("cmov LS not implemented");
		abort();
	case 8: // HI
		jit_unimplemented("cmov HI not implemented");
		abort();
	default:
#if defined(USE_DATA_BUFFER)
    long offs = data_long_offs(base);
		CC_LDR_rRI(cond, REG_WORK1, RPC_INDEX, offs);			// ldrcc   r2, [pc, #offs]    ; <value>
		CC_LDR_rRR_LSLi(cond, d, REG_WORK1, index, shft);	// ldrcc   %[d], [r2, %[index], lsl #[shift]]
#else
		CC_LDR_rRI(cond, REG_WORK1, RPC_INDEX, 4);			// ldrcc   r2, [pc, #4]    ; <value>
		CC_LDR_rRR_LSLi(cond, d, REG_WORK1, index, shft);	// ldrcc   %[d], [r2, %[index], lsl #[shift]]
		B_i(0); 				             				// b       <jp>
#endif
		break;
	}
#if !defined(USE_DATA_BUFFER)
	emit_long(base);						// <value>:
	//<jp>:
#endif
}

LOWFUNC(WRITE,READ,2,compemu_raw_cmp_l_mi,(MEMR d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  data_check_end(8, 16);
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);    	// ldr     r2, [pc, #offs]    ; d
	LDR_rR(REG_WORK1, REG_WORK1);          		// ldr     r2, [r2]

	offs = data_long_offs(s);
	LDR_rRI(REG_WORK2, RPC_INDEX, offs);     	// ldr     r3, [pc, #offs]    ; s

	CMP_rr(REG_WORK1, REG_WORK2);          		// cmp     r2, r3

#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 12);    	 	// ldr     r2, [pc, #24]    ; <value>
	LDR_rR(REG_WORK1, REG_WORK1);          		// ldr     r2, [r2]

	LDR_rRI(REG_WORK2, RPC_INDEX, 8);     		// ldr     r3, [pc, #20]    ; <value2>

	CMP_rr(REG_WORK1, REG_WORK2);          		// cmp     r2, r3

	B_i(1);                                 	// b	<jp>

	//<value>:
	emit_long(d);
	//<value2>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(WRITE,READ,2,compemu_raw_cmp_l_mi8,(MEMR d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);    // ldr     r2, [pc, #offs]    ; <value>
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 8);    	 	// ldr     r2, [pc, #8]    ; <value>
#endif
	LDR_rR(REG_WORK1, REG_WORK1);          		// ldr     r2, [r2]

	CMP_ri(REG_WORK1, s);		          		// cmp     r2, r3

#if !defined(USE_DATA_BUFFER)
	B_i(0);                                 	// b	<jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,3,compemu_raw_lea_l_brr,(W4 d, RR4 s, IMM offset))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(offset);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]    ; <value>
	ADD_rrr(d, s, REG_WORK1);         	  // add     r7, r6, r2
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	ADD_rrr(d, s, REG_WORK1);         	// add     r7, r6, r2
	B_i(0);                           	// b        <jp>

	//<value>:
	emit_long(offset);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,4,compemu_raw_lea_l_rr_indexed,(W4 d, RR4 s, RR4 index, IMM factor))
{
	int shft;
	switch(factor) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

	ADD_rrrLSLi(d, s, index, shft);		// ADD R7,R6,R5,LSL #2
}

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_b_mr,(IMM d, RR1 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr	r2, [pc, #offs]	; <value>
	STRB_rR(s, REG_WORK1);	           	  // strb	r6, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr	r2, [pc, #4]	; <value>
	STRB_rR(s, REG_WORK1);	           	// strb	r6, [r2]
	B_i(0);                            	// b	<jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_l_mi,(MEMW d, IMM s))
{
	// TODO: optimize imm

#if defined(USE_DATA_BUFFER)
  data_check_end(8, 12);
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr    r2, [pc, #offs]    ; d
	offs = data_long_offs(s);
	LDR_rRI(REG_WORK2, RPC_INDEX, offs); 	// ldr    r3, [pc, #offs]    ; s
	STR_rR(REG_WORK2, REG_WORK1);      	  // str    r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 8); 	// ldr    r2, [pc, #8]    ; <value>
	LDR_rRI(REG_WORK2, RPC_INDEX, 8); 	// ldr    r3, [pc, #8]    ; <value2>
	STR_rR(REG_WORK2, REG_WORK1);      	// str    r3, [r2]
	B_i(1);                             // b      <jp>

	emit_long(d);						//<value>:
	emit_long(s);						//<value2>:

	//<jp>:
#endif
}

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_l_mr,(IMM d, RR4 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]    ; <value>
	STR_rR(s, REG_WORK1);             	  // str     r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	STR_rR(s, REG_WORK1);             	// str     r3, [r2]
	B_i(0);                           	// b       <jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,compemu_raw_mov_l_ri,(W4 d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(s);
	LDR_rRI(d, RPC_INDEX, offs); 	// ldr     %[d], [pc, #offs]    ; <value>
#else
	LDR_rR(d, RPC_INDEX); 	// ldr     %[d], [pc]    ; <value>
	B_i(0);                	// b       <jp>

	//<value>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(NONE,READ,2,compemu_raw_mov_l_rm,(W4 d, MEMR s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs); 	// ldr     r2, [pc, #offs]    ; <value>
	LDR_rR(d, REG_WORK1);              	  // ldr     r7, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4); 	// ldr     r2, [pc, #4]    ; <value>
	LDR_rR(d, REG_WORK1);              	// ldr     r7, [r2]
	B_i(0);                            	// b       <jp>

	emit_long(s);						//<value>:
	//<jp>:
#endif
}

LOWFUNC(NONE,NONE,2,compemu_raw_mov_l_rr,(W4 d, RR4 s))
{
	MOV_rr(d, s); 					// mov     %[d], %[s]
}

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_w_mr,(IMM d, RR2 s))
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(d);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);  // ldr     r2, [pc, #offs]    ; <value>
	STRH_rR(s, REG_WORK1);             	  // strh    r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 4);  	// ldr     r2, [pc, #4]    ; <value>
	STRH_rR(s, REG_WORK1);             	// strh    r3, [r2]
	B_i(0);                            	// b       <jp>

	//<value>:
	emit_long(d);
	//<jp>:
#endif
}

LOWFUNC(WRITE,RMW,2,compemu_raw_sub_l_mi,(MEMRW d, IMM s))
{
#if defined(USE_DATA_BUFFER)
  data_check_end(8, 24);
  long target = data_long(d, 24);
  long offs = get_data_offset(target);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);        // ldr	r2, [pc, #offs]	; d
	LDR_rR(REG_WORK2, REG_WORK1);               // ldr	r3, [r2]

	offs = data_long_offs(s);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);        // ldr	r2, [pc, #offs]	; s

	SUBS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs	r3, r3, r2

	offs = get_data_offset(target);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);       	// ldr	r2, [pc, #offs]	; d
	STR_rR(REG_WORK2, REG_WORK1); 	         		// str	r3, [r2]
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 20);        	// ldr	r2, [pc, #32]	; <value>
	LDR_rR(REG_WORK2, REG_WORK1);               // ldr	r3, [r2]

	LDR_rRI(REG_WORK1, RPC_INDEX, 16);        	// ldr	r2, [pc, #28]	; <value2>

	SUBS_rrr(REG_WORK2, REG_WORK2, REG_WORK1); 	// subs	r3, r3, r2

	LDR_rRI(REG_WORK1, RPC_INDEX, 4);        	// ldr	r2, [pc, #16]	; <value>
	STR_rR(REG_WORK2, REG_WORK1); 	     		// str	r3, [r2]

	B_i(1);                                    	// b	<jp>

	//<value>:
	emit_long(d);
	//<value2>:
	emit_long(s);
	//<jp>:
#endif
}

LOWFUNC(WRITE,NONE,2,compemu_raw_test_l_rr,(RR4 d, RR4 s))
{
	TST_rr(d, s);                          			// tst     r7, r6
}

LOWFUNC(NONE,NONE,2,compemu_raw_zero_extend_16_rr,(W4 d, RR2 s))
{
#if defined(ARMV6_ASSEMBLY)
	UXTH_rr(d, s);									// UXTH %[d], %[s]
#else
	BIC_rri(d, s, 0xff000000); 						// bic	%[d], %[s], #0xff000000
	BIC_rri(d, d, 0x00ff0000); 						// bic	%[d], %[d], #0x00ff0000
#endif
}

static inline void compemu_raw_call(uae_u32 t)
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(t);
	LDR_rRI(REG_WORK1, RPC_INDEX, offs);  // ldr     r2, [pc, #offs]	; <value>
#else
	LDR_rRI(REG_WORK1, RPC_INDEX, 12);  // ldr     r2, [pc, #12]	; <value>
#endif
	PUSH(RLR_INDEX);                    // push    {lr}
	BLX_r(REG_WORK1);					// blx	   r2
	POP(RLR_INDEX);                     // pop     {lr}
#if !defined(USE_DATA_BUFFER)
	B_i(0);                             // b	   <jp>

	//<value>:
	emit_long(t);
	//<jp>:
#endif
}

#if defined(UAE)
static inline void compemu_raw_call_r(RR4 r)
{
	PUSH(RLR_INDEX);  // push    {lr}
	BLX_r(r);					// blx	   r0
	POP(RLR_INDEX);   // pop     {lr}
}
#endif

static inline void compemu_raw_jcc_l_oponly(int cc)
{
	switch (cc) {
	case 9: // LS
		BEQ_i(0);										// beq <dojmp>
		BCC_i(2);										// bcc <jp>

		//<dojmp>:
		LDR_rR(REG_WORK1, RPC_INDEX); 	        		// ldr	r2, [pc]	; <value>
		BX_r(REG_WORK1);								// bx r2
		break;

	case 8: // HI
		BEQ_i(3);										// beq <jp>
		BCS_i(2);										// bcs <jp>

		//<dojmp>:
		LDR_rR(REG_WORK1, RPC_INDEX);  			       	// ldr	r2, [pc]	; <value>
		BX_r(REG_WORK1);								// bx r2
		break;

	default:
		CC_LDR_rRI(cc, REG_WORK1, RPC_INDEX, 4); 		// ldrlt	r2, [pc, #4]	; <value>
		CC_BX_r(cc, REG_WORK1);							// bxlt 	r2
		B_i(0);                                         // b		<jp>
		break;
	}
  // emit of target will be done by caller
}

static inline void compemu_raw_jl(uae_u32 t)
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(t);
	CC_LDR_rRI(NATIVE_CC_LT, RPC_INDEX, RPC_INDEX, offs); 		// ldrlt   pc, [pc, offs]
#else
	CC_LDR_rR(NATIVE_CC_LT, RPC_INDEX, RPC_INDEX); 		// ldrlt   pc, [pc]
	B_i(0);                                             // b       <jp>

	//<value>:
	emit_long(t);
	//<jp>:
#endif
}

static inline void compemu_raw_jmp(uae_u32 t)
{
	LDR_rR(REG_WORK1, RPC_INDEX); 	// ldr   r2, [pc]
	BX_r(REG_WORK1);				// bx r2
	emit_long(t);
}

static inline void compemu_raw_jmp_m_indexed(uae_u32 base, uae_u32 r, uae_u32 m)
{
	int shft;
	switch(m) {
	case 1: shft=0; break;
	case 2: shft=1; break;
	case 4: shft=2; break;
	case 8: shft=3; break;
	default: abort();
	}

	LDR_rR(REG_WORK1, RPC_INDEX);           		// ldr     r2, [pc]    ; <value>
	LDR_rRR_LSLi(RPC_INDEX, REG_WORK1, r, shft);	// ldr     pc, [r2, r6, lsl #3]
	emit_long(base);
}

static inline void compemu_raw_jmp_r(RR4 r)
{
	BX_r(r);
}

static inline void compemu_raw_jnz(uae_u32 t)
{
#if defined(USE_DATA_BUFFER)
  long offs = data_long_offs(t);
	CC_LDR_rRI(NATIVE_CC_NE, RPC_INDEX, RPC_INDEX, offs); 		// ldrne   pc, [pc, offs]
#else
	CC_LDR_rR(NATIVE_CC_NE, RPC_INDEX, RPC_INDEX); 		// ldrne   pc, [pc]
	B_i(0);                                             // b       <jp>

	emit_long(t);
	//<jp>:
#endif
}

static inline void compemu_raw_jz_b_oponly(void)
{
	BNE_i(2);									// bne jp
	LDRSB_rRI(REG_WORK1, RPC_INDEX, 3);			// ldrsb	r2,[pc,#3]
	ADD_rrr(RPC_INDEX, RPC_INDEX, REG_WORK1); 	// add		pc,pc,r2

	skip_n_bytes(3); /* additionally 1 byte skipped by generic code */

	// <jp:>
}

static inline void compemu_raw_jnz_b_oponly(void)
{
	BEQ_i(2);									// beq jp
	LDRSB_rRI(REG_WORK1, RPC_INDEX, 3);			// ldrsb	r2,[pc,#3]
	ADD_rrr(RPC_INDEX, RPC_INDEX, REG_WORK1); 	// add		pc,pc,r2

	skip_n_bytes(3); /* additionally 1 byte skipped by generic code */

	// <jp:>
}

static inline void compemu_raw_branch(IMM d)
{
	B_i((d >> 2) - 1);
}
