/*
 *  ppc-dyngen.cpp - PowerPC dynamic translation
 *
 *  Kheperix (C) 2003-2004 Gwenole Beauchesne
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

#include <assert.h>
#include <stdlib.h>

#define DYNGEN_IMPL 1
#define DEFINE_GEN(NAME,ARGS) void powerpc_dyngen::NAME ARGS
#include "ppc-dyngen-ops.hpp"

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

void powerpc_dyngen::gen_bc_A0(int bo, int bi, uint32 npc)
{
#if 1
	if (BO_CONDITIONAL_BRANCH(bo)) {
		gen_load_T0_CR();
		gen_and_32_T0_im(1 << (31 - bi));
	}
	switch (bo >> 1) {
#define _(A,B,C,D) (((A) << 3)| ((B) << 2) | ((C) << 1) | (D))
	case _(0,0,0,0): gen_op_branch_A0_bo_0000(npc); break;
	case _(0,0,0,1): gen_op_branch_A0_bo_0001(npc); break;
	case _(0,0,1,0):
	case _(0,0,1,1): gen_op_branch_A0_bo_001x(npc); break;
	case _(0,1,0,0): gen_op_branch_A0_bo_0100(npc); break;
	case _(0,1,0,1): gen_op_branch_A0_bo_0101(npc); break;
	case _(0,1,1,0):
	case _(0,1,1,1): gen_op_branch_A0_bo_011x(npc); break;
	case _(1,0,0,0):
	case _(1,1,0,0): gen_op_branch_A0_bo_1x00(npc); break;
	case _(1,0,0,1):
	case _(1,1,0,1): gen_op_branch_A0_bo_1x01(npc); break;
	case _(1,0,1,0):
	case _(1,0,1,1):
	case _(1,1,1,0):
	case _(1,1,1,1): gen_op_branch_A0_bo_1x1x(); break;
#undef _
	default: abort();
	}
#else
	if (BO_CONDITIONAL_BRANCH(bo)) {
		gen_load_T0_CR();
		gen_and_32_T0_im(1 << (31 - bi));
		const int n = (bo >> 1) & 7;
		switch (n) {
#define _(TRUE,DCTR,CTR0) (((TRUE) ? 4 : 0) | ((DCTR) ? 0 : 2) | ((CTR0) ? 1 : 0))
		case _(0,0,0): gen_op_branch_if_not_T0_ctr_0x(npc); break;
		case _(0,0,1): gen_op_branch_if_not_T0_ctr_0x(npc); break;
		case _(0,1,0): gen_op_branch_if_not_T0_ctr_10(npc); break;
		case _(0,1,1): gen_op_branch_if_not_T0_ctr_11(npc); break;
		case _(1,0,0): gen_op_branch_if_T0_ctr_0x(npc); break;
		case _(1,0,1): gen_op_branch_if_T0_ctr_0x(npc); break;
		case _(1,1,0): gen_op_branch_if_T0_ctr_10(npc); break;
		case _(1,1,1): gen_op_branch_if_T0_ctr_11(npc); break;
#undef _
		default: abort();
		}
	}
	else {
		if (BO_DECREMENT_CTR(bo)) {
			gen_decrement_ctr_T0();
			if (BO_BRANCH_IF_CTR_ZERO(bo))
				gen_branch_A0_if_not_T0(npc);
			else
				gen_branch_A0_if_T0(npc);
		}
		else {
			// Branch always
			gen_set_PC_A0();
		}
	}
#endif
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

void powerpc_dyngen::gen_vaddfp(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vaddfp_VD_V0_V1();
}

void powerpc_dyngen::gen_vsubfp(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vsubfp_VD_V0_V1();
}

void powerpc_dyngen::gen_vmaddfp(int vD, int vA, int vB, int vC)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_load_ad_V2_VR(vC);
	gen_op_vmaddfp_VD_V0_V1_V2();
}

void powerpc_dyngen::gen_vnmsubfp(int vD, int vA, int vB, int vC)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_load_ad_V2_VR(vC);
	gen_op_vnmsubfp_VD_V0_V1_V2();
}

void powerpc_dyngen::gen_vmaxfp(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vmaxfp_VD_V0_V1();
}

void powerpc_dyngen::gen_vminfp(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vminfp_VD_V0_V1();
}

void powerpc_dyngen::gen_vand(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vand_VD_V0_V1();
}

void powerpc_dyngen::gen_vandc(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vandc_VD_V0_V1();
}

void powerpc_dyngen::gen_vnor(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vnor_VD_V0_V1();
}

void powerpc_dyngen::gen_vor(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vor_VD_V0_V1();
}

void powerpc_dyngen::gen_vxor(int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_op_vxor_VD_V0_V1();
}
