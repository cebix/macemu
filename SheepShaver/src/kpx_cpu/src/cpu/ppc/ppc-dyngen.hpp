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

public:

	// Make rc_cache accessible to codegen helper
	friend class powerpc_dyngen_helper;

	// Default constructor
	powerpc_dyngen(dyngen_cpu_base cpu, int cache_size = -1)
		: basic_dyngen(cpu, cache_size)
		{ }

	// Load/store registers
	void gen_load_A0_GPR(int i);
	void gen_load_T0_GPR(int i);
	void gen_load_T1_GPR(int i);
	void gen_load_T2_GPR(int i);
	void gen_store_A0_GPR(int i);
	void gen_store_T0_GPR(int i);
	void gen_store_T1_GPR(int i);
	void gen_store_T2_GPR(int i);
	void gen_load_F0_FPR(int i);
	void gen_load_F1_FPR(int i);
	void gen_load_F2_FPR(int i);
	void gen_store_FD_FPR(int i);
	void gen_store_F0_FPR(int i);
	void gen_store_F1_FPR(int i);
	void gen_store_F2_FPR(int i);

	// Raw aliases
#define DEFINE_ALIAS_RAW(NAME, PRE, POST, ARGLIST, ARGS) \
	void gen_##NAME ARGLIST { PRE; gen_op_##NAME ARGS; POST; }

#define DEFINE_ALIAS_0(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(),())
#define DEFINE_ALIAS_1(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1),(p1))
#define DEFINE_ALIAS_2(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1,long p2),(p1,p2))
#define DEFINE_ALIAS_3(NAME,PRE,POST)	DEFINE_ALIAS_RAW(NAME,PRE,POST,(long p1,long p2,long p3),(p1,p2,p3))
#ifdef NO_DEFINE_ALIAS
#define DEFINE_ALIAS(NAME,N)
#else
#define DEFINE_ALIAS(NAME,N)			DEFINE_ALIAS_##N(NAME,,)
#endif

	// Misc instructions
	DEFINE_ALIAS(inc_32_mem,1);
	DEFINE_ALIAS(nego_T0,0);

	// Condition registers
	DEFINE_ALIAS(load_T0_CR,0);
	DEFINE_ALIAS(store_T0_CR,0);
	void gen_load_T0_crf(int crf);
	void gen_store_T0_crf(int crf);
	void gen_load_T0_crb(int i);
	void gen_load_T1_crb(int i);
	void gen_store_T0_crb(int i);
	void gen_store_T1_crb(int i);
	void gen_mtcrf_T0_im(uint32 mask);

	// Special purpose registers
	DEFINE_ALIAS(load_T0_XER,0);
	DEFINE_ALIAS(store_T0_XER,0);
	DEFINE_ALIAS(load_T0_PC,0);
	DEFINE_ALIAS(store_T0_PC,0);
	DEFINE_ALIAS(set_PC_im,1);
	DEFINE_ALIAS(set_PC_A0,0);
	DEFINE_ALIAS(inc_PC,1);
	DEFINE_ALIAS(load_T0_LR,0);
	DEFINE_ALIAS(store_T0_LR,0);
	DEFINE_ALIAS(load_T0_CTR,0);
	DEFINE_ALIAS(load_A0_CTR,0);
	DEFINE_ALIAS(store_T0_CTR,0);
	DEFINE_ALIAS(store_T1_CTR,0);
	DEFINE_ALIAS(load_T1_PC,0);
	DEFINE_ALIAS(load_A0_LR,0);
	DEFINE_ALIAS(store_im_LR,1);

	DEFINE_ALIAS(spcflags_init,1);
	DEFINE_ALIAS(spcflags_set,1);
	DEFINE_ALIAS(spcflags_clear,1);

	// Control Flow
	DEFINE_ALIAS(decrement_ctr_T0,0);
	DEFINE_ALIAS(branch_A0_if_T0,1);
	DEFINE_ALIAS(branch_A0_if_not_T0,1);

	// Compare & Record instructions
	DEFINE_ALIAS(record_cr0_T0,0);
	DEFINE_ALIAS(record_cr1,0);
	void gen_compare_T0_T1(int crf);
	void gen_compare_T0_im(int crf, int32 value);
	void gen_compare_logical_T0_T1(int crf);
	void gen_compare_logical_T0_im(int crf, int32 value);

	// Multiply/Divide instructions
	DEFINE_ALIAS(mulhw_T0_T1,0);
	DEFINE_ALIAS(mulhwu_T0_T1,0);
	DEFINE_ALIAS(mulli_T0_im,1);
	DEFINE_ALIAS(mullwo_T0_T1,0);
	DEFINE_ALIAS(divw_T0_T1,0);
	DEFINE_ALIAS(divwo_T0_T1,0);
	DEFINE_ALIAS(divwu_T0_T1,0);
	DEFINE_ALIAS(divwuo_T0_T1,0);

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
	DEFINE_ALIAS(addco_T0_T1,0);
	DEFINE_ALIAS(adde_T0_T1,0);
	DEFINE_ALIAS(addeo_T0_T1,0);
	DEFINE_ALIAS(addme_T0,0);
	DEFINE_ALIAS(addmeo_T0,0);
	DEFINE_ALIAS(addze_T0,0);
	DEFINE_ALIAS(addzeo_T0,0);
	DEFINE_ALIAS(subf_T0_T1,0);
	DEFINE_ALIAS(subfo_T0_T1,0);
	DEFINE_ALIAS(subfc_T0_im,1);
	DEFINE_ALIAS(subfc_T0_T1,0);
	DEFINE_ALIAS(subfco_T0_T1,0);
	DEFINE_ALIAS(subfe_T0_T1,0);
	DEFINE_ALIAS(subfeo_T0_T1,0);
	DEFINE_ALIAS(subfme_T0,0);
	DEFINE_ALIAS(subfmeo_T0,0);
	DEFINE_ALIAS(subfze_T0,0);
	DEFINE_ALIAS(subfzeo_T0,0);

	// Double-precision floating point operations
	DEFINE_ALIAS(fmov_F0_F1,0);
	DEFINE_ALIAS(fmov_F0_F2,0);
	DEFINE_ALIAS(fmov_F1_F0,0);
	DEFINE_ALIAS(fmov_F1_F2,0);
	DEFINE_ALIAS(fmov_F2_F0,0);
	DEFINE_ALIAS(fmov_F2_F1,0);
	DEFINE_ALIAS(fmov_FD_F0,0);
	DEFINE_ALIAS(fmov_FD_F1,0);
	DEFINE_ALIAS(fmov_FD_F2,0);
	DEFINE_ALIAS(fabs_FD_F0,0);
	DEFINE_ALIAS(fneg_FD_F0,0);
	DEFINE_ALIAS(fnabs_FD_F0,0);
	DEFINE_ALIAS(fadd_FD_F0_F1,0);
	DEFINE_ALIAS(fsub_FD_F0_F1,0);
	DEFINE_ALIAS(fmul_FD_F0_F1,0);
	DEFINE_ALIAS(fdiv_FD_F0_F1,0);
	DEFINE_ALIAS(fmadd_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fmsub_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fnmadd_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fnmsub_FD_F0_F1_F2,0);

	// Single-precision floating point operations
	DEFINE_ALIAS(fadds_FD_F0_F1,0);
	DEFINE_ALIAS(fsubs_FD_F0_F1,0);
	DEFINE_ALIAS(fmuls_FD_F0_F1,0);
	DEFINE_ALIAS(fdivs_FD_F0_F1,0);
	DEFINE_ALIAS(fmadds_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fmsubs_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fnmadds_FD_F0_F1_F2,0);
	DEFINE_ALIAS(fnmsubs_FD_F0_F1_F2,0);

	// Load/store floating point data
	DEFINE_ALIAS(load_double_FD_A0_T1,0);
	void gen_load_double_FD_A0_im(int32 offset);
	DEFINE_ALIAS(load_single_FD_A0_T1,0);
	void gen_load_single_FD_A0_im(int32 offset);
	DEFINE_ALIAS(store_double_F0_A0_T1,0);
	void gen_store_double_F0_A0_im(int32 offset);
	DEFINE_ALIAS(store_single_F0_A0_T1,0);
	void gen_store_single_F0_A0_im(int32 offset);

	// Branch instructions
	void gen_bc_A0(int bo, int bi, uint32 npc);

#undef DEFINE_ALIAS
#undef DEFINE_ALIAS_0
#undef DEFINE_ALIAS_1
#undef DEFINE_ALIAS_2
#undef DEFINE_ALIAS_3
#undef DEFINE_ALIAS_RAW
};

#endif /* PPC_ENABLE_JIT */

#endif /* PPC_DYNGEN_H */
