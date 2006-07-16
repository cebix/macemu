/*
 *  ppc-jit.cpp - PowerPC dynamic translation (mid-level)
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
#include "cpu/ppc/ppc-jit.hpp"
#include "cpu/ppc/ppc-instructions.hpp"
#include "utils/utils-cpuinfo.hpp"
#include "utils/utils-sentinel.hpp"

// Mid-level code generator info
const powerpc_jit::jit_info_t *powerpc_jit::jit_info[PPC_I(MAX)];

// PowerPC JIT initializer
powerpc_jit::powerpc_jit(dyngen_cpu_base cpu, int cache_size)
	: powerpc_dyngen(cpu, cache_size)
{
	static bool once = true;

	if (once) {
		once = false;

		// default to no handler
		static const jit_info_t jit_not_available = {
			-1,
			(gen_handler_t)&powerpc_jit::gen_not_available,
			0
		};
		for (int i = 0; i < PPC_I(MAX); i++)
			jit_info[i] = &jit_not_available;

		// generic altivec handlers
		static const jit_info_t gen_vector[] = {
#define DEFINE_OP(MNEMO, GEN_OP, DYNGEN_OP) \
			{ PPC_I(MNEMO), (gen_handler_t)&powerpc_jit::gen_vector_generic_##GEN_OP, &powerpc_dyngen::gen_op_##DYNGEN_OP }
			DEFINE_OP(VADDFP,	2, vaddfp_VD_V0_V1),
			DEFINE_OP(VSUBFP,	2, vsubfp_VD_V0_V1),
			DEFINE_OP(VMADDFP,	3, vmaddfp_VD_V0_V1_V2),
			DEFINE_OP(VNMSUBFP,	3, vnmsubfp_VD_V0_V1_V2),
			DEFINE_OP(VAND,		2, vand_VD_V0_V1),
			DEFINE_OP(VANDC,	2, vandc_VD_V0_V1),
			DEFINE_OP(VNOR,		2, vnor_VD_V0_V1),
			DEFINE_OP(VOR,		2, vor_VD_V0_V1),
			DEFINE_OP(VXOR,		2, vxor_VD_V0_V1)
#undef DEFINE_OP
		};
		for (int i = 0; i < sizeof(gen_vector) / sizeof(gen_vector[0]); i++)
			jit_info[gen_vector[i].mnemo] = &gen_vector[i];

#if defined(__i386__) || defined(__x86_64__)
		// MMX optimized handlers
		static const jit_info_t mmx_vector[] = {
#define DEFINE_OP(MNEMO, GEN_OP, DYNGEN_OP) \
			{ PPC_I(MNEMO), (gen_handler_t)&powerpc_jit::gen_vector_mmx_##GEN_OP, &powerpc_dyngen::gen_op_mmx_##DYNGEN_OP }
			DEFINE_OP(VADDUBM,	2, vaddubm),
			DEFINE_OP(VADDUHM,	2, vadduhm),
			DEFINE_OP(VADDUWM,	2, vadduwm),
			DEFINE_OP(VAND,		2, vand),
			DEFINE_OP(VANDC,	2, vandc),
			DEFINE_OP(VCMPEQUB,	2, vcmpequb),
			DEFINE_OP(VCMPEQUH,	2, vcmpequh),
			DEFINE_OP(VCMPEQUW,	2, vcmpequw),
			DEFINE_OP(VCMPGTSB,	2, vcmpgtsb),
			DEFINE_OP(VCMPGTSH,	2, vcmpgtsh),
			DEFINE_OP(VCMPGTSW,	2, vcmpgtsw),
			DEFINE_OP(VOR,		2, vor),
			DEFINE_OP(VSUBUBM,	2, vsububm),
			DEFINE_OP(VSUBUHM,	2, vsubuhm),
			DEFINE_OP(VSUBUWM,	2, vsubuwm),
			DEFINE_OP(VXOR,		2, vxor)
#undef DEFINE_OP
		};
		if (cpuinfo_check_mmx()) {
			for (int i = 0; i < sizeof(mmx_vector) / sizeof(mmx_vector[0]); i++)
				jit_info[mmx_vector[i].mnemo] = &mmx_vector[i];
		}

		// SSE optimized handlers
		static const jit_info_t sse_vector[] = {
			// new MMX instructions brought into SSE capable CPUs
#define DEFINE_OP(MNEMO, GEN_OP, DYNGEN_OP) \
			{ PPC_I(MNEMO), (gen_handler_t)&powerpc_jit::gen_vector_mmx_##GEN_OP, &powerpc_dyngen::gen_op_mmx_##DYNGEN_OP }
			DEFINE_OP(VMAXSH,	2, vmaxsh),
			DEFINE_OP(VMAXUB,	2, vmaxub),
			DEFINE_OP(VMINSH,	2, vminsh),
			DEFINE_OP(VMINUB,	2, vminub),
#undef DEFINE_OP
			// full SSE instructions
#define DEFINE_OP(MNEMO, GEN_OP, DYNGEN_OP) \
			{ PPC_I(MNEMO), (gen_handler_t)&powerpc_jit::gen_vector_generic_##GEN_OP, &powerpc_dyngen::gen_op_sse_##DYNGEN_OP }
			DEFINE_OP(VADDFP,	2, vaddfp),
			DEFINE_OP(VAND,		2, vand),
			DEFINE_OP(VANDC,	2, vandc),
			DEFINE_OP(VCMPEQFP,	2, vcmpeqfp),
			DEFINE_OP(VCMPGEFP,	2, vcmpgefp),
			DEFINE_OP(VCMPGTFP,	2, vcmpgtfp),
			DEFINE_OP(VMADDFP,	3, vmaddfp),
			DEFINE_OP(VMAXFP,	2, vmaxfp),
			DEFINE_OP(VMINFP,	2, vminfp),
			DEFINE_OP(VNMSUBFP,	3, vnmsubfp),
			DEFINE_OP(VOR,		2, vor),
			DEFINE_OP(VSUBFP,	2, vsubfp),
			DEFINE_OP(VXOR,		2, vxor),
			DEFINE_OP(VMINUB,	2, vminub),
			DEFINE_OP(VMAXUB,	2, vmaxub),
			DEFINE_OP(VMINSH,	2, vminsh),
			DEFINE_OP(VMAXSH,	2, vmaxsh)
#undef DEFINE_OP
		};

		if (cpuinfo_check_sse()) {
			for (int i = 0; i < sizeof(sse_vector) / sizeof(sse_vector[0]); i++)
				jit_info[sse_vector[i].mnemo] = &sse_vector[i];
		}

		// generic altivec handlers
		static const jit_info_t sse2_vector[] = {
#define DEFINE_OP(MNEMO, GEN_OP, DYNGEN_OP) \
			{ PPC_I(MNEMO), (gen_handler_t)&powerpc_jit::gen_vector_generic_##GEN_OP, &powerpc_dyngen::gen_op_sse2_##DYNGEN_OP }
			DEFINE_OP(VADDUBM,	2, vaddubm),
			DEFINE_OP(VADDUHM,	2, vadduhm),
			DEFINE_OP(VADDUWM,	2, vadduwm),
			DEFINE_OP(VSUBUBM,	2, vsububm),
			DEFINE_OP(VSUBUHM,	2, vsubuhm),
			DEFINE_OP(VSUBUWM,	2, vsubuwm),
			DEFINE_OP(VAND,		2, vand),
			DEFINE_OP(VANDC,	2, vandc),
			DEFINE_OP(VOR,		2, vor),
			DEFINE_OP(VXOR,		2, vxor),
			DEFINE_OP(VCMPEQUB,	2, vcmpequb),
			DEFINE_OP(VCMPEQUH,	2, vcmpequh),
			DEFINE_OP(VCMPEQUW,	2, vcmpequw),
			DEFINE_OP(VCMPGTSB,	2, vcmpgtsb),
			DEFINE_OP(VCMPGTSH,	2, vcmpgtsh),
			DEFINE_OP(VCMPGTSW,	2, vcmpgtsw),
#undef DEFINE_OP
			{ PPC_I(VSLDOI),
			  (gen_handler_t)&powerpc_jit::gen_sse2_vsldoi, 0 }
		};

		if (cpuinfo_check_sse2()) {
			for (int i = 0; i < sizeof(sse2_vector) / sizeof(sse2_vector[0]); i++)
				jit_info[sse2_vector[i].mnemo] = &sse2_vector[i];
		}
#endif
	}
}

// Dispatch mid-level code generators
bool powerpc_jit::gen_vector_2(int mnemo, int vD, int vA, int vB, bool Rc)
{
	return (this->*((bool (powerpc_jit::*)(int, bool, int, int, int))jit_info[mnemo]->handler))(mnemo, Rc, vD, vA, vB);
}

bool powerpc_jit::gen_vector_3(int mnemo, int vD, int vA, int vB, int vC, bool Rc)
{
	return (this->*((bool (powerpc_jit::*)(int, bool, int, int, int, int))jit_info[mnemo]->handler))(mnemo, Rc, vD, vA, vB, vC);
}


bool powerpc_jit::gen_not_available(int mnemo, bool Rc)
{
	return false;
}

bool powerpc_jit::gen_vector_generic_2(int mnemo, bool Rc, int vD, int vA, int vB)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	jit_info[mnemo]->dyngen_handler(this);
	if (Rc)
		gen_record_cr6_VD();
	return true;
}

bool powerpc_jit::gen_vector_generic_3(int mnemo, bool Rc, int vD, int vA, int vB, int vC)
{
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	gen_load_ad_V2_VR(vC);
	jit_info[mnemo]->dyngen_handler(this);
	if (Rc)
		gen_record_cr6_VD();
	return true;
}

bool powerpc_jit::gen_vector_mmx_2(int mnemo, bool Rc, int vD, int vA, int vB)
{
#if defined(__i386__) || defined(__x86_64__)
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	gen_load_ad_V1_VR(vB);
	jit_info[mnemo]->dyngen_handler(this);
	if (Rc)
		gen_record_cr6_VD();
	gen_op_emms();
	return true;
#endif
	return false;
}

bool powerpc_jit::gen_sse2_vsldoi(int mnemo, bool Rc, int vD, int vA, int vB, int SH)
{
#if defined(__i386__) || defined(__x86_64__)
	gen_load_ad_VD_VR(vD);
	gen_load_ad_V0_VR(vA);
	if (SH == 0)
		gen_op_sse_mov_VD_V0();
	else {
		gen_load_ad_V1_VR(vB);
		powerpc_dyngen::gen_sse2_vsldoi_VD_V0_V1(SH);
	}
	return true;
#endif
	return false;
}
