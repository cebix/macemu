/*
 *  ppc-dyngen.cpp - PowerPC dynamic translation (low-level)
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
#include "utils/utils-cpuinfo.hpp"
#include "cpu/ppc/ppc-dyngen.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-instructions.hpp"

#include <assert.h>
#include <stdlib.h>

#define DYNGEN_IMPL 1
#define DEFINE_GEN(NAME,RET,ARGS) RET powerpc_dyngen::NAME ARGS
#include "ppc-dyngen-ops.hpp"

powerpc_dyngen::powerpc_dyngen(dyngen_cpu_base cpu)
	: basic_dyngen(cpu)
{
#ifdef SHEEPSHAVER
	printf("Detected CPU features:");
	if (cpuinfo_check_mmx())
		printf(" MMX");
	if (cpuinfo_check_sse())
		printf(" SSE");
	if (cpuinfo_check_sse2())
		printf(" SSE2");
	if (cpuinfo_check_sse3())
		printf(" SSE3");
	if (cpuinfo_check_ssse3())
		printf(" SSSE3");
	if (cpuinfo_check_altivec())
		printf(" VMX");
	printf("\n");
#endif
}

uint8 *powerpc_dyngen::gen_start(uint32 pc)
{
	// Generate exit if there are pending spcflags
	uint8 *p = basic_dyngen::gen_start();
	gen_op_spcflags_check();
	gen_op_set_PC_im(pc);
	gen_exec_return();
	dg_set_jmp_target_noflush(jmp_addr[0], gen_align());
	jmp_addr[0] = NULL;
	return p;
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
DEFINE_INSN(load, T0, GPR);
DEFINE_INSN(load, T1, GPR);
DEFINE_INSN(load, T2, GPR);
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
void powerpc_dyngen::gen_##NAME##_##TYPE##_##REG##_T1_im(int32 offset)	\
{																		\
	if (offset == 0)													\
		gen_op_##NAME##_##TYPE##_##REG##_T1_0();						\
	else																\
		gen_op_##NAME##_##TYPE##_##REG##_T1_im(offset);					\
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
	if (BO_CONDITIONAL_BRANCH(bo))
		gen_load_T1_crb(bi);

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
		if (direct_chaining)
			gen_op_branch_chain_2();
		else {
			if (tpc != 0xffffffff)
				gen_op_branch_2_im_im(tpc, npc);
			else
				gen_op_branch_2_T0_im(npc);
		}
	}
	else {
		// one-way branches
		if (direct_chaining)
			gen_op_branch_chain_1();
		else {
			if (tpc != 0xffffffff)
				gen_op_branch_1_im(tpc);
			else
				gen_op_branch_1_T0();
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
