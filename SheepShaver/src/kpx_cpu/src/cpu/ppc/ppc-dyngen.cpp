/*
 *  ppc-dyngen.cpp - PowerPC dynamic translation
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
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

// Condition register bitfield
DEFINE_INSN(load, T0, crb);
DEFINE_INSN(load, T1, crb);
DEFINE_INSN(store, T0, crb);
DEFINE_INSN(store, T1, crb);

#undef DEFINE_INSN

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
