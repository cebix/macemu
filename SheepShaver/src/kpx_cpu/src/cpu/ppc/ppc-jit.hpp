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
	powerpc_jit(dyngen_cpu_base cpu, int cache_size = -1);

	bool gen_vector_2(int mnemo, int vD, int vA, int vB, bool Rc = false);
	bool gen_vector_3(int mnemo, int vD, int vA, int vB, int vC, bool Rc = false);

private:
	// Mid-level code generator info
	typedef bool (powerpc_jit::*gen_handler_t)(int, bool);
	struct jit_info_t {
		int mnemo;
		gen_handler_t handler;
		powerpc_dyngen::gen_handler_t dyngen_handler;
	};
	static const jit_info_t *jit_info[];

private:
	bool gen_not_available(int mnemo, bool Rc);
	bool gen_vector_generic_2(int mnemo, bool Rc, int vD, int vA, int vB);
	bool gen_vector_generic_3(int mnemo, bool Rc, int vD, int vA, int vB, int vC);
	bool gen_vector_mmx_2(int mnemo, bool Rc, int vD, int vA, int vB);
	bool gen_sse2_vsldoi(int mnemo, bool Rc, int vD, int vA, int vB, int SH);
};

#endif /* PPC_JIT_H */
