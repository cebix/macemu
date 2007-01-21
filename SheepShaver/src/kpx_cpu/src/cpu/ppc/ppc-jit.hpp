/*
 *  ppc-jit.hpp - PowerPC dynamic translation (mid-level)
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

#ifndef PPC_JIT_H
#define PPC_JIT_H

#include "sysdeps.h"
#include "cpu/ppc/ppc-dyngen.hpp"

struct powerpc_jit
	: public powerpc_dyngen
{
	// Default constructor
	powerpc_jit(dyngen_cpu_base cpu);

	// Initialization
	bool initialize(void);

	bool gen_vector_1(int mnemo, int vD);
	bool gen_vector_2(int mnemo, int vD, int vA, int vB);
	bool gen_vector_3(int mnemo, int vD, int vA, int vB, int vC);
	bool gen_vector_compare(int mnemo, int vD, int vA, int vB, bool Rc);

private:
	// Mid-level code generator info
	typedef bool (powerpc_jit::*gen_handler_t)(int, bool);
	typedef void (powerpc_dyngen::*dyngen_handler_t)(void);
	union jit_option_t {
		jit_option_t() { }
		uintptr value;
		jit_option_t(uintptr v) : value(v) { }
		dyngen_handler_t dyngen_handler;
		jit_option_t(dyngen_handler_t const & h) : dyngen_handler(h) { }
	};
	struct jit_info_t {
		int mnemo;
		gen_handler_t handler;
		jit_option_t o;
	};
	static const jit_info_t *jit_info[];

private:
	bool gen_not_available(int mnemo);
	bool gen_vector_generic_1(int mnemo, int vD);
	bool gen_vector_generic_2(int mnemo, int vD, int vA, int vB);
	bool gen_vector_generic_3(int mnemo, int vD, int vA, int vB, int vC);
	bool gen_vector_generic_c(int mnemo, int vD, int vA, int vB, bool Rc);
	bool gen_vector_generic_load(int mnemo, int vD, int rA, int rB);
	bool gen_vector_generic_store(int mnemo, int vS, int rA, int rB);
	bool gen_vector_generic_load_word(int mnemo, int vD, int rA, int rB);
	bool gen_vector_generic_store_word(int mnemo, int vS, int rA, int rB);

#if defined(__i386__) || defined(__x86_64__)
	bool gen_x86_lvx(int mnemo, int vD, int rA, int rB);
	bool gen_x86_lvewx(int mnemo, int vD, int rA, int rB);
	bool gen_x86_stvx(int mnemo, int vS, int rA, int rB);
	bool gen_x86_stvewx(int mnemo, int vS, int rA, int rB);
	bool gen_x86_mtvscr(int mnemo, int vD);
	bool gen_x86_mfvscr(int mnemo, int vB);
	bool gen_mmx_arith_2(int mnemo, int vD, int vA, int vB);
	bool gen_mmx_arith_c(int mnemo, int vD, int vA, int vB, bool Rc);
	void gen_sse_record_cr6(int vD);
	bool gen_sse_arith_2(int mnemo, int vD, int vA, int vB);
	bool gen_sse_arith_s(int mnemo, int vD, int vA, int vB);
	bool gen_sse_arith_c(int mnemo, int vD, int vA, int vB, bool Rc);
	bool gen_sse_vsel(int mnemo, int vD, int vA, int vB, int vC);
	bool gen_sse_vmaddfp(int mnemo, int vD, int vA, int vB, int vC);
	bool gen_sse_vnmsubfp(int mnemo, int vD, int vA, int vB, int vC);
	void gen_sse2_record_cr6(int vD);
	bool gen_sse2_arith_2(int mnemo, int vD, int vA, int vB);
	bool gen_sse2_arith_s(int mnemo, int vD, int vA, int vB);
	bool gen_sse2_arith_c(int mnemo, int vD, int vA, int vB, bool Rc);
	bool gen_sse2_vsel(int mnemo, int vD, int vA, int vB, int vC);
	bool gen_sse2_vsldoi(int mnemo, int vD, int vA, int vB, int SH);
	void gen_sse2_vsplat(int vD, int rValue);
	bool gen_sse2_vspltisb(int mnemo, int vD, int SIMM);
	bool gen_sse2_vspltish(int mnemo, int vD, int SIMM);
	bool gen_sse2_vspltisw(int mnemo, int vD, int SIMM);
	bool gen_sse2_vspltb(int mnemo, int vD, int UIMM, int vB);
	bool gen_sse2_vsplth(int mnemo, int vD, int UIMM, int vB);
	bool gen_sse2_vspltw(int mnemo, int vD, int UIMM, int vB);
#endif
};

#endif /* PPC_JIT_H */
