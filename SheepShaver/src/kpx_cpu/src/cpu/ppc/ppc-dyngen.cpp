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

void powerpc_dyngen::invalidate_so_cache()
{
	rc_cache.set_so_status(RC_cache::STATUS_TRASH);
}

void powerpc_dyngen::invalidate_cr_cache()
{
	invalidate_so_cache();
	rc_cache.set_val_status(RC_cache::STATUS_TRASH);
	rc_cache.set_crf(-1);
}

void powerpc_dyngen::gen_commit_cr()
{
	if (rc_cache.val_status() == RC_cache::STATUS_VALID &&
		rc_cache.so_status() == RC_cache::STATUS_VALID) {
		const int crf = rc_cache.crf();
		assert(crf != -1);
		gen_store_RC_cr(crf);
		invalidate_cr_cache();
	}
	else if (rc_cache.val_status() == RC_cache::STATUS_VALID) {
		const int crf = rc_cache.crf();
		assert(crf != -1);
		gen_commit_rc_cache_cr(crf);
		invalidate_cr_cache();
	}
	else {
		// Maybe only SO field left to spill
		gen_commit_so();
	}
}

void powerpc_dyngen::gen_commit_so()
{
	if (rc_cache.so_status() == RC_cache::STATUS_VALID) {
		const int crf = rc_cache.crf();
		assert(crf != -1);
		gen_commit_so_cache_cr(crf);
		invalidate_so_cache();
	}
}

void powerpc_dyngen::gen_compare_T0_T1(int crf)
{
	if (!rc_cache.has_field(crf))
		gen_commit_cr();
	gen_op_compare_T0_T1();
	rc_cache.cache_field(crf);
}

void powerpc_dyngen::gen_compare_T0_im(int crf, int32 value)
{
	if (!rc_cache.has_field(crf))
		gen_commit_cr();
	if (value == 0)
		gen_op_compare_T0_0();
	else
		gen_op_compare_T0_im(value);
	rc_cache.cache_field(crf);
}

void powerpc_dyngen::gen_compare_logical_T0_T1(int crf)
{
	if (!rc_cache.has_field(crf))
		gen_commit_cr();
	gen_op_compare_logical_T0_T1();
	rc_cache.cache_field(crf);
}

void powerpc_dyngen::gen_compare_logical_T0_im(int crf, int32 value)
{
	if (!rc_cache.has_field(crf))
		gen_commit_cr();
	if (value == 0)
		gen_op_compare_logical_T0_0();
	else
		gen_op_compare_logical_T0_im(value);
	rc_cache.cache_field(crf);
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
DEFINE_INSN(store, A0, GPR);
DEFINE_INSN(store, T0, GPR);
DEFINE_INSN(store, T1, GPR);

// Condition register bitfield
DEFINE_INSN(load, T0, crb);
DEFINE_INSN(load, T1, crb);
DEFINE_INSN(store, T0, crb);
DEFINE_INSN(store, T1, crb);

#undef DEFINE_INSN

#define DEFINE_INSN(OP, REG)						\
void powerpc_dyngen::gen_##OP##_##REG##_cr(int crf)	\
{													\
	switch (crf) {									\
	case 0: gen_op_##OP##_##REG##_cr0(); break;		\
	case 1: gen_op_##OP##_##REG##_cr1(); break;		\
	case 2: gen_op_##OP##_##REG##_cr2(); break;		\
	case 3: gen_op_##OP##_##REG##_cr3(); break;		\
	case 4: gen_op_##OP##_##REG##_cr4(); break;		\
	case 5: gen_op_##OP##_##REG##_cr5(); break;		\
	case 6: gen_op_##OP##_##REG##_cr6(); break;		\
	case 7: gen_op_##OP##_##REG##_cr7(); break;		\
	default: abort();								\
	}												\
}

DEFINE_INSN(load, RC);
DEFINE_INSN(store, RC);
DEFINE_INSN(commit, so_cache);
DEFINE_INSN(commit, rc_cache);

#undef DEFINE_INSN

void powerpc_dyngen::gen_record_cr0_T0(void)
{
	gen_compare_T0_im(0, 0);
}

/**
 *	Prepare condition register cache for branch
 *
 *		This was meant to be a CR caching system but gain is almost
 *		zero. However, this shows the ability to handle a CR cache
 *		forwards and actually, a superblock-level (traces) optimizer
 *		with proper code generation will benefit from it.
 **/

#define USE_CR_CACHE 1

void powerpc_dyngen::gen_prepare_RC(int bi)
{
	const int crf = bi / 4;
	const int crb = bi % 4;

#if USE_CR_CACHE
	if (rc_cache.crf() == crf) {
		if (crb != 3) {
			// Rematerialize (LT, GT, EQ) if they are not live
			if (rc_cache.val_status() == RC_cache::STATUS_VALID)
				gen_commit_rc_cache_cr(crf);
			else
				gen_load_RC_cr(crf);
		}
		else {
			// Rematerialize SO if it is not in cache
			if (rc_cache.so_status() == RC_cache::STATUS_VALID)
				gen_commit_so_cache_cr(crf);
			else
				gen_load_RC_cr(crf);
		}
		invalidate_cr_cache();
	}
	else {
		// Reload flags from memory
		gen_commit_cr();
		gen_load_RC_cr(crf);
	}
#else
	gen_commit_cr();
	gen_load_RC_cr(crf);
#endif
}

void powerpc_dyngen::gen_bc_A0(int bo, int bi, uint32 npc)
{
	if (BO_CONDITIONAL_BRANCH(bo)) {
		enum { lt, gt, eq, so };
		gen_prepare_RC(bi);
		const int n = ((bi % 4) << 2) | ((bo >> 1) & 3);
#define _(CR,DCTR,CTR0) (((CR) << 2) | ((DCTR) ? 0 : 2) | ((CTR0) ? 1 : 0))
		if (BO_BRANCH_IF_TRUE(bo)) {
			switch (n) {
#define C(CR)											\
			case _(CR,0,0): gen_b##CR##_0x(npc); break;	\
			case _(CR,0,1): gen_b##CR##_0x(npc); break;	\
			case _(CR,1,0): gen_b##CR##_10(npc); break;	\
			case _(CR,1,1): gen_b##CR##_11(npc); break;
			C(lt); C(gt); C(eq); C(so);
#undef C
			}
		}
		else {
			switch (n) {
#define C(CR)												\
			case _(CR,0,0): gen_bn##CR##_0x(npc); break;	\
			case _(CR,0,1): gen_bn##CR##_0x(npc); break;	\
			case _(CR,1,0): gen_bn##CR##_10(npc); break;	\
			case _(CR,1,1): gen_bn##CR##_11(npc); break;
			C(lt); C(gt); C(eq); C(so);
#undef C
			}
		}
#undef _
	}
	else {
		gen_commit_cr();
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
}
