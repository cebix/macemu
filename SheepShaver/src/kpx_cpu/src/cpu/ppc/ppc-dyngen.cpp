/*
 *  ppc-dyngen.cpp - PowerPC dynamic translation
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

#include "sysdeps.h"
#include "cpu/ppc/ppc-dyngen.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-instructions.hpp"

#include <assert.h>
#include <stdlib.h>

#define DYNGEN_IMPL 1
#define DEFINE_GEN(NAME,ARGS) void powerpc_dyngen::NAME ARGS
#include "ppc-dyngen-ops.hpp"


/**
 *		Determine x86 CPU features
 **/

/* XXX: move that in CPU dependent bits */
#if defined(__i386__) || defined(__x86_64__)
static uint32 cpu_features = 0;

enum {
	HWCAP_I386_CMOV		= 1 << 15,
	HWCAP_I386_MMX		= 1 << 23,
	HWCAP_I386_SSE		= 1 << 25,
	HWCAP_I386_SSE2		= 1 << 26,
};

static unsigned int x86_cpuid(void)
{
	int fl1, fl2;

#ifndef __x86_64__
	/* See if we can use cpuid. On AMD64 we always can.  */
	__asm__ ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
			 "pushl %0; popfl; pushfl; popl %0; popfl"
			 : "=&r" (fl1), "=&r" (fl2)
			 : "i" (0x00200000));
	if (((fl1 ^ fl2) & 0x00200000) == 0)
		return (0);
#endif

	/* Host supports cpuid.  See if cpuid gives capabilities, try
	   CPUID(0).  Preserve %ebx and %ecx; cpuid insn clobbers these, we
	   don't need their CPUID values here, and %ebx may be the PIC
	   register.  */
#ifdef __x86_64__
	__asm__ ("pushq %%rcx; pushq %%rbx; cpuid; popq %%rbx; popq %%rcx"
			 : "=a" (fl1) : "0" (0) : "rdx", "cc");
#else
	__asm__ ("push %%ecx ; push %%ebx ; cpuid ; pop %%ebx ; pop %%ecx"
			 : "=a" (fl1) : "0" (0) : "edx", "cc");
#endif
	if (fl1 == 0)
		return (0);

	/* Invoke CPUID(1), return %edx; caller can examine bits to
	   determine what's supported.  */
#ifdef __x86_64__
	__asm__ ("push %%rcx ; push %%rbx ; cpuid ; pop %%rbx ; pop %%rcx" : "=d" (fl2) : "a" (1) : "cc");
#else
	__asm__ ("push %%ecx ; push %%ebx ; cpuid ; pop %%ebx ; pop %%ecx" : "=d" (fl2) : "a" (1) : "cc");
#endif

	return fl2;
}
#endif

powerpc_dyngen::powerpc_dyngen(dyngen_cpu_base cpu, int cache_size)
	: basic_dyngen(cpu, cache_size)
{
#if defined(__i386__) || defined(__x86_64__)
	cpu_features = x86_cpuid();
#ifdef SHEEPSHAVER
	if (cpu_features & (HWCAP_I386_MMX | HWCAP_I386_SSE | HWCAP_I386_SSE2)) {
		printf("Detected CPU features:");
		if (cpu_features & HWCAP_I386_MMX)
			printf(" MMX");
		if (cpu_features & HWCAP_I386_SSE)
			printf(" SSE");
		if (cpu_features & HWCAP_I386_SSE2)
			printf(" SSE2");
		printf("\n");
	}
#endif
#endif
}

void powerpc_dyngen::gen_compare_T0_T1(int crf)
{
	gen_op_compare_T0_T1();
	gen_store_T0_crf(crf);
}

void powerpc_dyngen::gen_compare_T0_im(int crf, int32 value)
{
	if (value == 0)
		gen_op_compare_T0_0();
	else
		gen_op_compare_T0_im(value);
	gen_store_T0_crf(crf);
}

void powerpc_dyngen::gen_compare_logical_T0_T1(int crf)
{
	gen_op_compare_logical_T0_T1();
	gen_store_T0_crf(crf);
}

void powerpc_dyngen::gen_compare_logical_T0_im(int crf, int32 value)
{
	if (value == 0)
		gen_op_compare_logical_T0_0();
	else
		gen_op_compare_logical_T0_im(value);
	gen_store_T0_crf(crf);
}

void powerpc_dyngen::gen_mtcrf_T0_im(uint32 mask)
{
	gen_op_mtcrf_T0_im(mask);
}


/**
 *		Load/store multiple words
 **/

#define DEFINE_INSN(OP)							\
void powerpc_dyngen::gen_##OP##_T0(int r)		\
{												\
	switch (r) {								\
	case 26: gen_op_##OP##_T0_26(); break;		\
	case 27: gen_op_##OP##_T0_27(); break;		\
	case 28: gen_op_##OP##_T0_28(); break;		\
	case 29: gen_op_##OP##_T0_29(); break;		\
	case 30: gen_op_##OP##_T0_30(); break;		\
	case 31: gen_op_##OP##_T0_31(); break;		\
	default: gen_op_##OP##_T0_im(r); break;		\
	}											\
}

DEFINE_INSN(lmw);
DEFINE_INSN(stmw);

#undef DEFINE_INSN


/**
 *		Load/store registers
 **/

#define DEFINE_INSN(OP, REG, REGT)						\
void powerpc_dyngen::gen_##OP##_##REG##_##REGT(int i)	\
{														\
	switch (i) {										\
	case 0: gen_op_##OP##_##REG##_##REGT##0(); break;	\
	case 1: gen_op_##OP##_##REG##_##REGT##1(); break;	\
	case 2: gen_op_##OP##_##REG##_##REGT##2(); break;	\
	case 3: gen_op_##OP##_##REG##_##REGT##3(); break;	\
	case 4: gen_op_##OP##_##REG##_##REGT##4(); break;	\
	case 5: gen_op_##OP##_##REG##_##REGT##5(); break;	\
	case 6: gen_op_##OP##_##REG##_##REGT##6(); break;	\
	case 7: gen_op_##OP##_##REG##_##REGT##7(); break;	\
	case 8: gen_op_##OP##_##REG##_##REGT##8(); break;	\
	case 9: gen_op_##OP##_##REG##_##REGT##9(); break;	\
	case 10: gen_op_##OP##_##REG##_##REGT##10(); break;	\
	case 11: gen_op_##OP##_##REG##_##REGT##11(); break;	\
	case 12: gen_op_##OP##_##REG##_##REGT##12(); break;	\
	case 13: gen_op_##OP##_##REG##_##REGT##13(); break;	\
	case 14: gen_op_##OP##_##REG##_##REGT##14(); break;	\
	case 15: gen_op_##OP##_##REG##_##REGT##15(); break;	\
	case 16: gen_op_##OP##_##REG##_##REGT##16(); break;	\
	case 17: gen_op_##OP##_##REG##_##REGT##17(); break;	\
	case 18: gen_op_##OP##_##REG##_##REGT##18(); break;	\
	case 19: gen_op_##OP##_##REG##_##REGT##19(); break;	\
	case 20: gen_op_##OP##_##REG##_##REGT##20(); break;	\
	case 21: gen_op_##OP##_##REG##_##REGT##21(); break;	\
	case 22: gen_op_##OP##_##REG##_##REGT##22(); break;	\
	case 23: gen_op_##OP##_##REG##_##REGT##23(); break;	\
	case 24: gen_op_##OP##_##REG##_##REGT##24(); break;	\
	case 25: gen_op_##OP##_##REG##_##REGT##25(); break;	\
	case 26: gen_op_##OP##_##REG##_##REGT##26(); break;	\
	case 27: gen_op_##OP##_##REG##_##REGT##27(); break;	\
	case 28: gen_op_##OP##_##REG##_##REGT##28(); break;	\
	case 29: gen_op_##OP##_##REG##_##REGT##29(); break;	\
	case 30: gen_op_##OP##_##REG##_##REGT##30(); break;	\
	case 31: gen_op_##OP##_##REG##_##REGT##31(); break;	\
	default: abort();									\
	}													\
}

// General purpose registers
DEFINE_INSN(load, A0, GPR);
DEFINE_INSN(load, T0, GPR);
DEFINE_INSN(load, T1, GPR);
DEFINE_INSN(load, T2, GPR);
DEFINE_INSN(store, A0, GPR);
DEFINE_INSN(store, T0, GPR);
DEFINE_INSN(store, T1, GPR);
DEFINE_INSN(store, T2, GPR);
DEFINE_INSN(load, F0, FPR);
DEFINE_INSN(load, F1, FPR);
DEFINE_INSN(load, F2, FPR);
DEFINE_INSN(store, F0, FPR);
DEFINE_INSN(store, F1, FPR);
DEFINE_INSN(store, F2, FPR);
DEFINE_INSN(store, FD, FPR);
DEFINE_INSN(load_ad, VD, VR);
DEFINE_INSN(load_ad, V0, VR);
DEFINE_INSN(load_ad, V1, VR);
DEFINE_INSN(load_ad, V2, VR);

// Condition register bitfield
DEFINE_INSN(load, T0, crb);
DEFINE_INSN(load, T1, crb);
DEFINE_INSN(store, T0, crb);
DEFINE_INSN(store, T1, crb);

#undef DEFINE_INSN

// Floating point load store
#define DEFINE_OP(NAME, REG, TYPE)										\
void powerpc_dyngen::gen_##NAME##_##TYPE##_##REG##_A0_im(int32 offset)	\
{																		\
	if (offset == 0)													\
		gen_op_##NAME##_##TYPE##_##REG##_A0_0();						\
	else																\
		gen_op_##NAME##_##TYPE##_##REG##_A0_im(offset);					\
}

DEFINE_OP(load, FD, double);
DEFINE_OP(load, FD, single);
DEFINE_OP(store, F0, double);
DEFINE_OP(store, F0, single);

#undef DEFINE_OP

#define DEFINE_INSN(OP, REG)							\
void powerpc_dyngen::gen_##OP##_##REG##_crf(int crf)	\
{														\
	switch (crf) {										\
	case 0: gen_op_##OP##_##REG##_cr0(); break;			\
	case 1: gen_op_##OP##_##REG##_cr1(); break;			\
	case 2: gen_op_##OP##_##REG##_cr2(); break;			\
	case 3: gen_op_##OP##_##REG##_cr3(); break;			\
	case 4: gen_op_##OP##_##REG##_cr4(); break;			\
	case 5: gen_op_##OP##_##REG##_cr5(); break;			\
	case 6: gen_op_##OP##_##REG##_cr6(); break;			\
	case 7: gen_op_##OP##_##REG##_cr7(); break;			\
	default: abort();									\
	}													\
}

DEFINE_INSN(load, T0);
DEFINE_INSN(store, T0);

#undef DEFINE_INSN

void powerpc_dyngen::gen_bc(int bo, int bi, uint32 tpc, uint32 npc, bool direct_chaining)
{
	if (BO_CONDITIONAL_BRANCH(bo)) {
		gen_load_T0_CR();
		gen_and_32_T0_im(1 << (31 - bi));
	}

	switch (bo >> 1) {
#define _(A,B,C,D) (((A) << 3)| ((B) << 2) | ((C) << 1) | (D))
	case _(0,0,0,0): gen_op_prep_branch_bo_0000(); break;
	case _(0,0,0,1): gen_op_prep_branch_bo_0001(); break;
	case _(0,0,1,0):
	case _(0,0,1,1): gen_op_prep_branch_bo_001x(); break;
	case _(0,1,0,0): gen_op_prep_branch_bo_0100(); break;
	case _(0,1,0,1): gen_op_prep_branch_bo_0101(); break;
	case _(0,1,1,0):
	case _(0,1,1,1): gen_op_prep_branch_bo_011x(); break;
	case _(1,0,0,0):
	case _(1,1,0,0): gen_op_prep_branch_bo_1x00(); break;
	case _(1,0,0,1):
	case _(1,1,0,1): gen_op_prep_branch_bo_1x01(); break;
	case _(1,0,1,0):
	case _(1,0,1,1):
	case _(1,1,1,0):
	case _(1,1,1,1): gen_op_prep_branch_bo_1x1x(); break;
#undef _
	default: abort();
	}
	
	if (BO_CONDITIONAL_BRANCH(bo) || BO_DECREMENT_CTR(bo)) {
		// two-way branches
		if (tpc != 0xffffffff) {
			if (direct_chaining)
				gen_op_branch_chain_2_im_im(tpc, npc);
			else
				gen_op_branch_2_im_im(tpc, npc);
		}
		else {
			if (direct_chaining)
				gen_op_branch_chain_2_A0_im(npc);
			else
				gen_op_branch_2_A0_im(npc);
		}
	}
	else {
		// one-way branches
		if (tpc != 0xffffffff) {
			if (direct_chaining)
				gen_op_branch_chain_1_im(tpc);
			else
				gen_op_branch_1_im(tpc);
		}
		else {
			if (direct_chaining)
				gen_op_branch_chain_1_A0();
			else
				gen_op_branch_1_A0();
		}
	}
}

/**
 *		Vector instructions
 **/

void powerpc_dyngen::gen_load_word_VD_T0(int vD)
{
	gen_load_ad_VD_VR(vD);
	gen_op_load_word_VD_T0();
}

void powerpc_dyngen::gen_store_word_VS_T0(int vS)
{
	gen_load_ad_VD_VR(vS);
	gen_op_store_word_VD_T0();
}

void powerpc_dyngen::gen_load_vect_VD_T0(int vD)
{
	gen_load_ad_VD_VR(vD);
	gen_op_load_vect_VD_T0();
}

void powerpc_dyngen::gen_store_vect_VS_T0(int vS)
{
	gen_load_ad_VD_VR(vS);
	gen_op_store_vect_VD_T0();
}

/**
 *		Code generators for AltiVec instructions
 **/

powerpc_dyngen::gen_handler_t
powerpc_dyngen::vector_codegen(int insn)
{
	gen_handler_t gen_op = 0;
	switch (insn) {
#define GEN_OP(NAME) nv_mem_fun(&powerpc_dyngen::gen_op_##NAME)
	case PPC_I(VADDFP):		gen_op = GEN_OP(vaddfp_VD_V0_V1);	break;
	case PPC_I(VSUBFP):		gen_op = GEN_OP(vsubfp_VD_V0_V1);	break;
	case PPC_I(VMADDFP):	gen_op = GEN_OP(vmaddfp_VD_V0_V1_V2);	break;
	case PPC_I(VNMSUBFP):	gen_op = GEN_OP(vnmsubfp_VD_V0_V1_V2);	break;
	case PPC_I(VMAXFP):		gen_op = GEN_OP(vmaxfp_VD_V0_V1);	break;
	case PPC_I(VMINFP):		gen_op = GEN_OP(vminfp_VD_V0_V1);	break;
	case PPC_I(VAND):		gen_op = GEN_OP(vand_VD_V0_V1);		break;
	case PPC_I(VANDC):		gen_op = GEN_OP(vandc_VD_V0_V1);	break;
	case PPC_I(VNOR):		gen_op = GEN_OP(vnor_VD_V0_V1);		break;
	case PPC_I(VOR):		gen_op = GEN_OP(vor_VD_V0_V1);		break;
	case PPC_I(VXOR):		gen_op = GEN_OP(vxor_VD_V0_V1);		break;
#undef GEN_OP
	}
	return gen_op;
}

#if defined(__i386__) || defined(__x86_64__)
powerpc_dyngen::gen_handler_t
powerpc_dyngen::vector_codegen_mmx(int insn)
{
#ifdef HAVE_gen_op_mmx_nop
	if (!(cpu_features & HWCAP_I386_MMX))
		return 0;

	/* XXX: auto-generate the table with individual handlers */
	gen_handler_t gen_op = 0;
	switch (insn) {
#define GEN_OP(NAME) nv_mem_fun(&powerpc_dyngen::gen_op_mmx_##NAME)
	case PPC_I(VADDUBM):	gen_op = GEN_OP(vaddubm);	break;
	case PPC_I(VADDUHM):	gen_op = GEN_OP(vadduhm);	break;
	case PPC_I(VADDUWM):	gen_op = GEN_OP(vadduwm);	break;
	case PPC_I(VAND):		gen_op = GEN_OP(vand);		break;
	case PPC_I(VANDC):		gen_op = GEN_OP(vandc);		break;
	case PPC_I(VCMPEQUB):	gen_op = GEN_OP(vcmpequb);	break;
	case PPC_I(VCMPEQUH):	gen_op = GEN_OP(vcmpequh);	break;
	case PPC_I(VCMPEQUW):	gen_op = GEN_OP(vcmpequw);	break;
	case PPC_I(VCMPGTSB):	gen_op = GEN_OP(vcmpgtsb);	break;
	case PPC_I(VCMPGTSH):	gen_op = GEN_OP(vcmpgtsh);	break;
	case PPC_I(VCMPGTSW):	gen_op = GEN_OP(vcmpgtsw);	break;
	case PPC_I(VOR):		gen_op = GEN_OP(vor);		break;
	case PPC_I(VSUBUBM):	gen_op = GEN_OP(vsububm);	break;
	case PPC_I(VSUBUHM):	gen_op = GEN_OP(vsubuhm);	break;
	case PPC_I(VSUBUWM):	gen_op = GEN_OP(vsubuwm);	break;
	case PPC_I(VXOR):		gen_op = GEN_OP(vxor);		break;
#undef GEN_OP
	}

#ifdef HAVE_gen_op_sse_nop
	if (gen_op.ptr())
		return gen_op;

	if (!(cpu_features & HWCAP_I386_SSE))
		return 0;

	/* XXX: is the MMX unit really used for those? */
	switch (insn) {
#define GEN_OP(NAME) nv_mem_fun(&powerpc_dyngen::gen_op_mmx_##NAME)
	case PPC_I(VMAXSH):		gen_op = GEN_OP(vmaxsh);	break;
	case PPC_I(VMAXUB):		gen_op = GEN_OP(vmaxub);	break;
	case PPC_I(VMINSH):		gen_op = GEN_OP(vminsh);	break;
	case PPC_I(VMINUB):		gen_op = GEN_OP(vminub);	break;
#undef GEN_OP
	}
#endif
	return gen_op;
#endif

	return 0;
}

powerpc_dyngen::gen_handler_t
powerpc_dyngen::vector_codegen_sse(int insn)
{
#ifdef HAVE_gen_op_sse_nop
	if (!(cpu_features & HWCAP_I386_SSE))
		return 0;

	/* XXX: auto-generate the table with individual handlers */
	gen_handler_t gen_op = 0;
	switch (insn) {
#define GEN_OP(NAME) nv_mem_fun(&powerpc_dyngen::gen_op_sse_##NAME)
	case PPC_I(VADDFP):		gen_op = GEN_OP(vaddfp);	break;
	case PPC_I(VAND):		gen_op = GEN_OP(vand);		break;
	case PPC_I(VANDC):		gen_op = GEN_OP(vandc);		break;
	case PPC_I(VCMPEQFP):	gen_op = GEN_OP(vcmpeqfp);	break;
	case PPC_I(VCMPGEFP):	gen_op = GEN_OP(vcmpgefp);	break;
	case PPC_I(VCMPGTFP):	gen_op = GEN_OP(vcmpgtfp);	break;
	case PPC_I(VMADDFP):	gen_op = GEN_OP(vmaddfp);	break;
	case PPC_I(VMAXFP):		gen_op = GEN_OP(vmaxfp);	break;
	case PPC_I(VMINFP):		gen_op = GEN_OP(vminfp);	break;
	case PPC_I(VNMSUBFP):	gen_op = GEN_OP(vnmsubfp);	break;
	case PPC_I(VOR):		gen_op = GEN_OP(vor);		break;
	case PPC_I(VSUBFP):		gen_op = GEN_OP(vsubfp);	break;
	case PPC_I(VXOR):		gen_op = GEN_OP(vxor);		break;
#undef GEN_OP
	}
	return gen_op;
#endif

	return 0;
}

powerpc_dyngen::gen_handler_t
powerpc_dyngen::vector_codegen_sse2(int insn)
{
	return 0;
}

void powerpc_dyngen::gen_mmx_clear(void)
{
#ifdef HAVE_gen_op_mmx_nop
	if (cpu_features & HWCAP_I386_MMX)
		gen_op_emms();
#endif
}
#endif
