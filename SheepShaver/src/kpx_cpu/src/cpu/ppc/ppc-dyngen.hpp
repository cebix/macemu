/*
 *  ppc-dyngen.hpp - PowerPC dynamic translation
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

#ifndef PPC_DYNGEN_H
#define PPC_DYNGEN_H

#include "sysdeps.h"
#include "cpu/ppc/ppc-config.hpp"

#if PPC_ENABLE_JIT
#include "cpu/jit/jit-config.hpp"
#include "cpu/jit/basic-dyngen.hpp"

class powerpc_dyngen
	: public basic_dyngen
{
	// Code generators for PowerPC synthetic instructions
#ifndef NO_DEFINE_ALIAS
#	define DEFINE_GEN(NAME,ARGS) void NAME ARGS;
#	include "ppc-dyngen-ops.hpp"
#endif

	struct RC_cache {
		enum {
			STATUS_TRASH,
			STATUS_VALID,
			STATUS_VALID_LOGICAL
		};
		int val_status;
		int so_status;
		int crf;

		// Used only by generated code if not enough native registers
		// are available to cache them all
		uint32 cc_lhs;
		uint32 cc_rhs;

		RC_cache()
			: val_status(STATUS_TRASH), so_status(STATUS_TRASH), crf(-1)
			{ }

		bool has_field(int test_crf)
			{ return val_status != STATUS_TRASH && crf == test_crf; }

		void cache_field(int new_crf, int new_status = STATUS_VALID)
			{ val_status = so_status = new_status; crf = new_crf; }
	};
	RC_cache rc_cache;

public:

	// Make rc_cache accessible to codegen helper
	friend class powerpc_dyngen_helper;

	// Default constructor
	powerpc_dyngen(dyngen_cpu_base cpu)
		: basic_dyngen(cpu)
		{ }

	// Load/store registers
	void gen_load_A0_GPR(int i);
	void gen_load_T0_GPR(int i);
	void gen_load_T1_GPR(int i);
	void gen_store_A0_GPR(int i);
	void gen_store_T0_GPR(int i);
	void gen_store_T1_GPR(int i);

	// Raw aliases
#define DEFINE_ALIAS_RAW(NAME, PRE, POST, ARGLIST, ARGS) \
	void gen_##NAME ARGLIST { PRE; gen_op_##NAME ARGS; POST; }

#define DEFINE_ALIAS_0(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(),())
#define DEFINE_ALIAS_1(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1),(p1))
#define DEFINE_ALIAS_2(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1,long p2),(p1,p2))
#define DEFINE_ALIAS_3(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1,long p2,long p3),(p1,p2,p3))
#ifdef NO_DEFINE_ALIAS
#define DEFINE_ALIAS(NAME,N)
#define DEFINE_ALIAS_CLOBBER_SO(NAME,N)
#define DEFINE_ALIAS_CLOBBER_CR(NAME,N)
#else
#define DEFINE_ALIAS(NAME,N)			DEFINE_ALIAS_##N(NAME,,)
#define DEFINE_ALIAS_CLOBBER_CR(NAME,N)	DEFINE_ALIAS_##N(NAME,gen_commit_cr(),)
#define DEFINE_ALIAS_CLOBBER_SO(NAME,N)	DEFINE_ALIAS_##N(NAME,gen_commit_so(),)
#endif

	// Condition registers
private:
	void do_gen_commit_so();
	void do_gen_commit_cr();
	void gen_commit_so_cache_cr(int crf);
	void gen_commit_rc_cache_cr(int crf);
	void gen_commit_logical_rc_cache_cr(int crf);
public:
	void invalidate_so_cache();
	void invalidate_cr_cache();
	void gen_commit_so();
	void gen_commit_cr();
	DEFINE_ALIAS_CLOBBER_CR(load_T0_CR,0);
	DEFINE_ALIAS_CLOBBER_CR(store_T0_CR,0);
	DEFINE_ALIAS(load_T0_XER,0);
	DEFINE_ALIAS(store_T0_XER,0);
	void gen_load_T0_crb(int i);
	void gen_load_T1_crb(int i);
	void gen_store_T0_crb(int i);
	void gen_store_T1_crb(int i);
	void gen_load_T0_cr(int crf);
	void gen_store_T0_cr(int crf);

	// Special purpose registers
	DEFINE_ALIAS(load_T0_PC,0);
	DEFINE_ALIAS(store_T0_PC,0);
	DEFINE_ALIAS(set_PC_T1,0);
	DEFINE_ALIAS(set_PC_im,1);
	DEFINE_ALIAS(inc_PC,1);
	DEFINE_ALIAS(load_T0_LR,0);
	DEFINE_ALIAS(store_T0_LR,0);
	DEFINE_ALIAS(load_T0_CTR,0);
	DEFINE_ALIAS(load_T1_CTR,0);
	DEFINE_ALIAS(store_T0_CTR,0);
	DEFINE_ALIAS(store_T1_CTR,0);
	DEFINE_ALIAS(load_T1_PC,0);
	DEFINE_ALIAS(load_T1_LR,0);
	DEFINE_ALIAS(store_im_LR,1);

	// Control Flow
	DEFINE_ALIAS(decrement_ctr_T1,0);
	DEFINE_ALIAS(branch_if_T0,2);
	DEFINE_ALIAS(branch_if_T1,2);
	DEFINE_ALIAS(branch_if_not_T0,2);
	DEFINE_ALIAS(branch_if_not_T1,2);
	DEFINE_ALIAS(branch_if_T0_T1,2);
	DEFINE_ALIAS(branch_T1_if_T0,1);

	// Compare & Record instructions
	DEFINE_ALIAS_CLOBBER_SO(record_nego_T0,0);
	void gen_record_cr0_T0();
	DEFINE_ALIAS(compare_T0_T1,0);
	DEFINE_ALIAS(compare_T0_0,0);
	DEFINE_ALIAS(compare_T0_im,1);
	void gen_compare_T0_T1(int crf);
	void gen_compare_T0_im(int crf, int32 value);
	void gen_compare_logical_T0_T1(int crf);
	void gen_compare_logical_T0_im(int crf, int32 value);

	// Multiply/Divide instructions
	DEFINE_ALIAS(mulhw_T0_T1,0);
	DEFINE_ALIAS(mulhwu_T0_T1,0);
	DEFINE_ALIAS(mulli_T0_im,1);
	DEFINE_ALIAS_CLOBBER_SO(mullwo_T0_T1,0);
	DEFINE_ALIAS(divw_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(divwo_T0_T1,0);
	DEFINE_ALIAS(divwu_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(divwuo_T0_T1,0);

	// Shift/Rotate instructions
	DEFINE_ALIAS(slw_T0_T1,0);
	DEFINE_ALIAS(srw_T0_T1,0);
	DEFINE_ALIAS(sraw_T0_T1,0);
	DEFINE_ALIAS(sraw_T0_im,1);
	DEFINE_ALIAS(rlwimi_T0_T1,2);
	DEFINE_ALIAS(rlwinm_T0_T1,2);
	DEFINE_ALIAS(rlwnm_T0_T1,1);
	DEFINE_ALIAS(cntlzw_32_T0,0);

	// Add/Sub related instructions
	DEFINE_ALIAS(addo_T0_T1,0);
	DEFINE_ALIAS(addc_T0_im,1);
	DEFINE_ALIAS(addc_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(addco_T0_T1,0);
	DEFINE_ALIAS(adde_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(addeo_T0_T1,0);
	DEFINE_ALIAS(addme_T0,0);
	DEFINE_ALIAS_CLOBBER_SO(addmeo_T0,0);
	DEFINE_ALIAS(addze_T0,0);
	DEFINE_ALIAS_CLOBBER_SO(addzeo_T0,0);
	DEFINE_ALIAS(subf_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(subfo_T0_T1,0);
	DEFINE_ALIAS(subfc_T0_im,1);
	DEFINE_ALIAS(subfc_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(subfco_T0_T1,0);
	DEFINE_ALIAS(subfe_T0_T1,0);
	DEFINE_ALIAS_CLOBBER_SO(subfeo_T0_T1,0);
	DEFINE_ALIAS(subfme_T0,0);
	DEFINE_ALIAS_CLOBBER_SO(subfmeo_T0,0);
	DEFINE_ALIAS(subfze_T0,0);
	DEFINE_ALIAS_CLOBBER_SO(subfzeo_T0,0);

	// Branch instructions
	void gen_bc(int bo, int bi, uint32 tpc, uint32 npc);
#define DEFINE_ALIAS_GRP_1(CR,CTR)				\
	DEFINE_ALIAS(b##CR##_##CTR,2);				\
	DEFINE_ALIAS(bn##CR##_##CTR,2);
#define DEFINE_ALIAS_GRP_2(CR)					\
	DEFINE_ALIAS_GRP_1(CR,0x);					\
	DEFINE_ALIAS_GRP_1(CR,10);					\
	DEFINE_ALIAS_GRP_1(CR,11);
	DEFINE_ALIAS_GRP_2(lt);
	DEFINE_ALIAS_GRP_2(gt);
	DEFINE_ALIAS_GRP_2(eq);
	DEFINE_ALIAS_GRP_2(so);
#undef DEFINE_ALAIS_GRP_2
#undef DEFINE_ALIAS_GRP_1

#undef DEFINE_ALIAS
#undef DEFINE_ALIAS_0
#undef DEFINE_ALIAS_1
#undef DEFINE_ALIAS_2
#undef DEFINE_ALIAS_3
#undef DEFINE_ALIAS_RAW
};

#endif /* PPC_ENABLE_JIT */

#endif /* PPC_DYNGEN_H */
