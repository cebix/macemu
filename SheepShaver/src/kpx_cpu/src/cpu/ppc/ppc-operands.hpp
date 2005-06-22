/*
 *  ppc-operands.hpp - PowerPC operands definition
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

#ifndef PPC_OPERANDS_H
#define PPC_OPERANDS_H

/**
 *		Numerical limits of vector registers components
 **/

template< class type >
struct vector_numeric_limits {
	static type min() throw();
	static type max() throw();
};

#define DEFINE_NUMERIC_LIMITS(TYPE, SMAX, USFX)							\
template<>																\
struct vector_numeric_limits<TYPE> {									\
	static inline TYPE min() throw() { return -(SMAX) - 1; }			\
	static inline TYPE max() throw() { return SMAX; }					\
};																		\
template<>																\
struct vector_numeric_limits<u##TYPE> {									\
	static inline u##TYPE min() throw() { return 0##USFX; }				\
	static inline u##TYPE max() throw() { return SMAX * 2##USFX + 1; }	\
}

DEFINE_NUMERIC_LIMITS( int8, 127, U);
DEFINE_NUMERIC_LIMITS(int16, 32767, U);
DEFINE_NUMERIC_LIMITS(int32, 2147483647L, UL);
DEFINE_NUMERIC_LIMITS(int64, 9223372036854775807LL, ULL);

#undef DEFINE_NUMERIC_LIMITS

/**
 *		Compile time checks
 **/

template< int a, int b >
struct ensure_equals;

template< int n >
struct ensure_equals<n, n> { };

template< class type, int size >
struct ensure_sizeof : ensure_equals<sizeof(type), size> { };

/**
 *		General purpose registers
 **/

template< class field, class op >
struct input_gpr_op {
	static inline uint32 get(powerpc_cpu * cpu, uint32 opcode) {
		return op::apply(cpu->gpr(field::extract(opcode)));
	}
};

template< class field >
struct input_gpr : input_gpr_op< field, op_nop > { };

template< class field >
struct output_gpr {
	static inline void set(powerpc_cpu * cpu, uint32 opcode, uint32 value) {
		cpu->gpr(field::extract(opcode)) = value;
	}
};

template< class field >
struct gpr_operand : input_gpr< field >, output_gpr< field > { };

template< class field, int except_regno >
struct input_gpr_except {
	static inline uint32 get(powerpc_cpu * cpu, uint32 opcode) {
		const int regno = field::extract(opcode);
		return regno != except_regno ? cpu->gpr(regno) : 0;
	};
	static inline void set(powerpc_cpu * cpu, uint32 opcode, uint32 value) {
		const int regno = field::extract(opcode);
		if (regno == except_regno) abort();
		cpu->gpr(regno) = value;
	}
};

/**
 *		Floating-point registers
 **/

template< class field >
struct input_fpr {
	static inline double get(powerpc_cpu * cpu, uint32 opcode) {
		return cpu->fpr(field::extract(opcode));
	}
};

template< class field >
struct output_fpr {
	static inline void set(powerpc_cpu * cpu, uint32 opcode, double value) {
		cpu->fpr(field::extract(opcode)) = value;
	}
};

template< class field >
struct fpr_operand : input_fpr< field >, output_fpr< field > { };

template< class field >
struct input_fpr_dw {
	static inline uint64 get(powerpc_cpu * cpu, uint32 opcode) {
		return cpu->fpr_dw(field::extract(opcode));
	}
};

template< class field >
struct output_fpr_dw {
	static inline void set(powerpc_cpu * cpu, uint32 opcode, uint64 value) {
		cpu->fpr_dw(field::extract(opcode)) = value;
	}
};

template< class field >
struct fpr_dw_operand : input_fpr_dw< field >, output_fpr_dw< field > { };

/**
 *		Vector registers
 **/

struct ev_direct {
	static inline int byte_element(int i) { return i; }
	static inline int half_element(int i) { return i; }
	static inline int word_element(int i) { return i; }
};

// This supposes elements are loaded by 4-byte word parts
#ifdef WORDS_BIGENDIAN
typedef ev_direct ev_mixed;
#else
struct ev_mixed : public ev_direct {
#if 0
	static inline int byte_element(int i) { return (i & ~3) + (3 - (i & 3)); }
	static inline int half_element(int i) { return (i & ~1) + (1 - (i & 1)); }
#else
	static inline int byte_element(int i) {
		static const int lookup[16] = {
			3,  2,  1,  0,
			7,  6,  5,  4,
			11, 10, 9,  8,
			15, 14, 13, 12
		};
		return lookup[i];
	}
	static inline int half_element(int i) {
		static const int lookup[8] = {
			1, 0, 3, 2,
			5, 4, 7, 6
		};
		return lookup[i];
	}
#endif
};
#endif

struct null_vector_operand {
	typedef uint32 type;
	typedef uint32 element_type;
	static const uint32	element_size = sizeof(element_type);
	static inline type const_ref(powerpc_cpu *, uint32) { return 0; } // fake so that compiler optimizes it out
	static inline element_type get_element(type const & reg, int i) { return 0; }
};

template< class field >
struct vimm_operand {
	typedef uint32 type;
	typedef uint32 element_type;
	static const uint32 element_size = sizeof(element_type);
	static inline type const_ref(powerpc_cpu *, uint32 opcode) { return field::extract(opcode); }
	static inline element_type get_element(type const & reg, int i) { return reg; }
};

template< class field >
struct input_vr {
	static inline powerpc_vr const & const_ref(powerpc_cpu * cpu, uint32 opcode) {
		return cpu->vr(field::extract(opcode));
	}
};

template< class field >
struct output_vr {
	static inline powerpc_vr & ref(powerpc_cpu * cpu, uint32 opcode) {
		return cpu->vr(field::extract(opcode));
	}
};

template< class field, class value_type >
struct vector_operand : input_vr< field >, output_vr< field > {
	typedef powerpc_vr	type;
	typedef value_type	element_type;
	static const uint32	element_size = sizeof(element_type);
	static inline bool	saturate(element_type) { return false; }
};

template< class field, class value_type, class sat_type >
struct vector_saturate_operand : input_vr< field >, output_vr< field > {
	typedef powerpc_vr	type;
	typedef sat_type	element_type;
	static const uint32	element_size = sizeof(value_type);
	static inline bool saturate(element_type & v) {
		bool sat = false;
		if (v > vector_numeric_limits<value_type>::max()) {
			v = vector_numeric_limits<value_type>::max();
			sat = true;
		}
		else if (v < vector_numeric_limits<value_type>::min()) {
			v = vector_numeric_limits<value_type>::min();
			sat = true;
		}
		return sat;
	}
};

template< class field, class value_type, class sat_type = int16, class ev = ev_direct >
struct v16qi_sat_operand : vector_saturate_operand< field, value_type, sat_type >, ensure_sizeof< sat_type, 2 > {
	static inline sat_type get_element(powerpc_vr const & reg, int i) {
		return (sat_type)(value_type)reg.b[ev::byte_element(i)];
	}
	static inline void set_element(powerpc_vr & reg, int i, sat_type value) {
		reg.b[ev::byte_element(i)] = value;
	}
};

template< class field, class value_type, class sat_type = int32, class ev = ev_direct >
struct v8hi_sat_operand : vector_saturate_operand< field, value_type, sat_type >, ensure_sizeof< sat_type, 4 > {
	static inline sat_type get_element(powerpc_vr const & reg, int i) {
		return (sat_type)(value_type)reg.h[ev::half_element(i)];
	}
	static inline void set_element(powerpc_vr & reg, int i, sat_type value) {
		reg.h[ev::half_element(i)] = value;
	}
};

template< class field, class value_type, class sat_type = int64 >
struct v4si_sat_operand : vector_saturate_operand< field, value_type, sat_type >, ensure_sizeof< sat_type, 8 > {
	static inline sat_type get_element(powerpc_vr const & reg, int i) {
		return (sat_type)(value_type)reg.w[i];
	}
	static inline void set_element(powerpc_vr & reg, int i, sat_type value) {
		reg.w[i] = value;
	}
};

template< class field, class value_type = uint8, class ev = ev_direct >
struct v16qi_operand : vector_operand< field, value_type >, ensure_sizeof< value_type, 1 > {
	static inline value_type get_element(powerpc_vr const & reg, int i) {
		return reg.b[ev::byte_element(i)];
	}
	static inline void set_element(powerpc_vr & reg, int i, value_type value) {
		reg.b[ev::byte_element(i)] = value;
	}
};

template< class field, class value_type = uint16, class ev = ev_direct >
struct v8hi_operand : vector_operand< field, value_type >, ensure_sizeof< value_type, 2 > {
	static inline value_type get_element(powerpc_vr const & reg, int i) {
		return reg.h[ev::half_element(i)];
	}
	static inline void set_element(powerpc_vr & reg, int i, value_type value) {
		reg.h[ev::half_element(i)] = value;
	}
};

template< class field, class value_type = uint32 >
struct v4si_operand : vector_operand< field, value_type >, ensure_sizeof< value_type, 4 > {
	static inline value_type get_element(powerpc_vr const & reg, int i) {
		return reg.w[i];
	}
	static inline void set_element(powerpc_vr & reg, int i, value_type value) {
		reg.w[i] = value;
	}
};

template< class field, class value_type = uint64 >
struct v2di_operand : vector_operand< field, value_type >, ensure_sizeof< value_type, 8 > {
	static inline value_type get_element(powerpc_vr const & reg, int i) {
		return reg.j[i];
	}
	static inline void set_element(powerpc_vr & reg, int i, value_type value) {
		reg.j[i] = value;
	}
};

template< class field >
struct v4sf_operand : vector_operand< field, float > {
	static inline float get_element(powerpc_vr const & reg, int i) {
		return reg.f[i];
	}
	static inline void set_element(powerpc_vr & reg, int i, float value) {
		reg.f[i] = value;
	}
};

template< class field >
struct vSH_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32 opcode) {
		return (cpu->vr(field::extract(opcode)).b[ev_mixed::byte_element(15)] >> 3) & 15;
	}
};


/**
 *		Immediate operands
 **/

struct null_operand {
	static inline uint32 get(powerpc_cpu *, uint32) {
		return 0;
	}
};
template< class field, class operation >
struct immediate_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		return operation::apply(field::extract(opcode));
	}
};

template< int32 N >
struct immediate_value {
	static inline uint32 get(powerpc_cpu *, uint32) {
		return (uint32)N;
	}
};

struct mask_operand {
	static inline uint32 compute(uint32 mb, uint32 me) {
		return ((mb > me) ?
				~(((uint32)-1 >> mb) ^ ((me >= 31) ? 0 : (uint32)-1 >> (me + 1))) :
				(((uint32)-1 >> mb) ^ ((me >= 31) ? 0 : (uint32)-1 >> (me + 1))));
	}
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		const uint32 mb = MB_field::extract(opcode);
		const uint32 me = ME_field::extract(opcode);
		return compute(mb, me);
	}
};

/**
 *		Special purpose registers
 **/

struct pc_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->pc();
	};
};

struct lr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->lr();
	};
};

struct ctr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->ctr();
	};
};

struct cr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->cr().get();
	}
};

template< class field >
struct xer_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return field::extract(cpu->xer().get());
	}
};

template<>
struct xer_operand<XER_CA_field> {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->xer().get_ca();
	}
};

template<>
struct xer_operand<XER_COUNT_field> {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->xer().get_count();
	}
};

template< class field >
struct fpscr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return field::extract(cpu->fpscr());
	}
};

struct spr_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		uint32 spr = SPR_field::extract(opcode);
		return ((spr & 0x1f) << 5) | ((spr >> 5) & 0x1f);
	}
};

struct tbr_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		uint32 tbr = TBR_field::extract(opcode);
		return ((tbr & 0x1f) << 5) | ((tbr >> 5) & 0x1f);
	}
};


/**
 *		Operand aliases for decode table
 **/

typedef null_operand							operand_NONE;
typedef cr_operand								operand_CR;
typedef pc_operand								operand_PC;
typedef lr_operand								operand_LR;
typedef ctr_operand								operand_CTR;
typedef input_gpr_op<rA_field, op_compl>		operand_RA_compl;
typedef gpr_operand<rA_field>					operand_RA;
typedef gpr_operand<rB_field>					operand_RB;
typedef gpr_operand<rS_field>					operand_RS;
typedef gpr_operand<rD_field>					operand_RD;
typedef input_gpr_except<rA_field, 0>			operand_RA_or_0; // RA ? GPR(RA) : 0
typedef immediate_value<0>						operand_RA_is_0; // RA -> 0
typedef immediate_value<1>						operand_ONE;
typedef immediate_value<0>						operand_ZERO;
typedef immediate_value<-1>						operand_MINUS_ONE;
typedef null_operand							operand_fp_NONE;
typedef fpr_operand<frA_field>					operand_fp_RA;
typedef fpr_operand<frB_field>					operand_fp_RB;
typedef fpr_operand<frC_field>					operand_fp_RC;
typedef fpr_operand<frD_field>					operand_fp_RD;
typedef fpr_operand<frS_field>					operand_fp_RS;
typedef fpr_dw_operand<frA_field>				operand_fp_dw_RA;
typedef fpr_dw_operand<frB_field>				operand_fp_dw_RB;
typedef fpr_dw_operand<frC_field>				operand_fp_dw_RC;
typedef fpr_dw_operand<frD_field>				operand_fp_dw_RD;
typedef fpr_dw_operand<frS_field>				operand_fp_dw_RS;
typedef xer_operand<XER_CA_field>				operand_XER_CA;
typedef xer_operand<XER_COUNT_field>			operand_XER_COUNT;
typedef fpscr_operand<FPSCR_RN_field>			operand_FPSCR_RN;
typedef spr_operand								operand_SPR;
typedef tbr_operand								operand_TBR;
typedef mask_operand							operand_MASK;
typedef null_vector_operand						operand_vD_NONE;
typedef null_vector_operand						operand_vA_NONE;
typedef null_vector_operand						operand_vB_NONE;
typedef null_vector_operand						operand_vC_NONE;
typedef v16qi_operand<vD_field>					operand_vD_V16QI;
typedef v16qi_operand<vA_field>					operand_vA_V16QI;
typedef v16qi_operand<vB_field>					operand_vB_V16QI;
typedef v16qi_operand<vC_field>					operand_vC_V16QI;
typedef v16qi_operand<vD_field, int8>			operand_vD_V16QIs;
typedef v16qi_operand<vA_field, int8>			operand_vA_V16QIs;
typedef v16qi_operand<vB_field, int8>			operand_vB_V16QIs;
typedef v16qi_operand<vC_field, int8>			operand_vC_V16QIs;
typedef v16qi_operand<vD_field, int8, ev_mixed>	operand_vD_V16QIms;
typedef v16qi_operand<vB_field, int8, ev_mixed>	operand_vB_V16QIms;
typedef v8hi_operand<vD_field>					operand_vD_V8HI;
typedef v8hi_operand<vA_field>					operand_vA_V8HI;
typedef v8hi_operand<vB_field>					operand_vB_V8HI;
typedef v8hi_operand<vC_field>					operand_vC_V8HI;
typedef v8hi_operand<vD_field, int16>			operand_vD_V8HIs;
typedef v8hi_operand<vA_field, int16>			operand_vA_V8HIs;
typedef v8hi_operand<vB_field, int16>			operand_vB_V8HIs;
typedef v8hi_operand<vC_field, int16>			operand_vC_V8HIs;
typedef v8hi_operand<vD_field, int16, ev_mixed>	operand_vD_V8HIms;
typedef v8hi_operand<vB_field, int16, ev_mixed>	operand_vB_V8HIms;
typedef v4si_operand<vD_field>					operand_vD_V4SI;
typedef v4si_operand<vA_field>					operand_vA_V4SI;
typedef v4si_operand<vB_field>					operand_vB_V4SI;
typedef v4si_operand<vC_field>					operand_vC_V4SI;
typedef v4si_operand<vD_field, int32>			operand_vD_V4SIs;
typedef v4si_operand<vA_field, int32>			operand_vA_V4SIs;
typedef v4si_operand<vB_field, int32>			operand_vB_V4SIs;
typedef v4si_operand<vC_field, int32>			operand_vC_V4SIs;
typedef v2di_operand<vD_field>					operand_vD_V2DI;
typedef v2di_operand<vA_field>					operand_vA_V2DI;
typedef v2di_operand<vB_field>					operand_vB_V2DI;
typedef v2di_operand<vC_field>					operand_vC_V2DI;
typedef v2di_operand<vD_field, int64>			operand_vD_V2DIs;
typedef v2di_operand<vA_field, int64>			operand_vA_V2DIs;
typedef v2di_operand<vB_field, int64>			operand_vB_V2DIs;
typedef v2di_operand<vC_field, int64>			operand_vC_V2DIs;
typedef v4sf_operand<vD_field>					operand_vD_V4SF;
typedef v4sf_operand<vA_field>					operand_vA_V4SF;
typedef v4sf_operand<vB_field>					operand_vB_V4SF;
typedef v4sf_operand<vC_field>					operand_vC_V4SF;
typedef v4si_operand<vS_field>					operand_vS_V4SI;
typedef v2di_operand<vS_field>					operand_vS_V2DI;
typedef vimm_operand<vA_field>					operand_vA_UIMM;
typedef vimm_operand<vB_field>					operand_vB_UIMM;
typedef vSH_operand<vB_field>					operand_SHBO;

// vector mixed element accessors
typedef v16qi_operand<vA_field, uint8, ev_mixed>		operand_vA_V16QIm;
typedef v16qi_operand<vB_field, uint8, ev_mixed>		operand_vB_V16QIm;
typedef v16qi_operand<vD_field, uint8, ev_mixed>		operand_vD_V16QIm;
typedef v8hi_operand<vA_field, uint16, ev_mixed>		operand_vA_V8HIm;
typedef v8hi_operand<vB_field, uint16, ev_mixed>		operand_vB_V8HIm;
typedef v8hi_operand<vD_field, uint16, ev_mixed>		operand_vD_V8HIm;

#define DEFINE_VECTOR_SAT_OPERAND(EV, REG, OP)										\
template< class value_type >														\
struct operand_##REG##_##EV##_SAT : OP##_sat_operand<REG##_field, value_type> { }

DEFINE_VECTOR_SAT_OPERAND(V4SI, vD, v4si);
DEFINE_VECTOR_SAT_OPERAND(V4SI, vA, v4si);
DEFINE_VECTOR_SAT_OPERAND(V4SI, vB, v4si);
DEFINE_VECTOR_SAT_OPERAND(V4SI, vC, v4si);
DEFINE_VECTOR_SAT_OPERAND(V8HI, vD, v8hi);
DEFINE_VECTOR_SAT_OPERAND(V8HI, vA, v8hi);
DEFINE_VECTOR_SAT_OPERAND(V8HI, vB, v8hi);
DEFINE_VECTOR_SAT_OPERAND(V8HI, vC, v8hi);
DEFINE_VECTOR_SAT_OPERAND(V16QI, vD, v16qi);
DEFINE_VECTOR_SAT_OPERAND(V16QI, vA, v16qi);
DEFINE_VECTOR_SAT_OPERAND(V16QI, vB, v16qi);
DEFINE_VECTOR_SAT_OPERAND(V16QI, vC, v16qi);

#undef DEFINE_VECTOR_SAT_OPERAND

#define DEFINE_VECTOR_MIXED_SAT_OPERAND(EV, SAT, REG, OP, TYPE)										   \
template< class value_type >																	   \
struct operand_##REG##_##EV##m_##SAT : OP##_sat_operand<REG##_field, value_type, TYPE, ev_mixed> { }

DEFINE_VECTOR_MIXED_SAT_OPERAND(V16QI, SAT, vA, v16qi, int16);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V16QI, SAT, vB, v16qi, int16);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V16QI, SAT, vD, v16qi, int16);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V16QI, USAT, vD, v16qi, uint16);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V8HI, SAT, vA, v8hi, int32);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V8HI, SAT, vB, v8hi, int32);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V8HI, SAT, vD, v8hi, int32);
DEFINE_VECTOR_MIXED_SAT_OPERAND(V8HI, USAT, vD, v8hi, uint32);

#undef DEFINE_VECTOR_MIXED_SAT_OPERAND

#define DEFINE_VECTOR_USAT_OPERAND(EV, REG, OP, TYPE)										\
template< class value_type >																\
struct operand_##REG##_##EV##_USAT : OP##_sat_operand<REG##_field, value_type, TYPE> { }

// FIXME: temporary for vector pack unsigned saturate variants
DEFINE_VECTOR_USAT_OPERAND(V4SI,  vD, v4si,  uint64);
DEFINE_VECTOR_USAT_OPERAND(V8HI,  vD, v8hi,  uint32);
DEFINE_VECTOR_USAT_OPERAND(V16QI, vD, v16qi, uint16);

#undef DEFINE_VECTOR_USAT_OPERAND

#define DEFINE_IMMEDIATE_OPERAND(NAME, FIELD, OP) \
typedef immediate_operand<FIELD##_field, op_##OP> operand_##NAME

DEFINE_IMMEDIATE_OPERAND(LI, LI, sign_extend_LI_32);
DEFINE_IMMEDIATE_OPERAND(BO, BO, nop);
DEFINE_IMMEDIATE_OPERAND(BD, BD, sign_extend_BD_32);
DEFINE_IMMEDIATE_OPERAND(IMM, IMM, nop);
DEFINE_IMMEDIATE_OPERAND(UIMM, UIMM, zero_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(UIMM_shifted, UIMM, zero_extend_16_32_shifted);
DEFINE_IMMEDIATE_OPERAND(SIMM, SIMM, sign_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(SIMM_shifted, SIMM, sign_extend_16_32_shifted);
DEFINE_IMMEDIATE_OPERAND(D, d, sign_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(NB, NB, nop);
DEFINE_IMMEDIATE_OPERAND(SH, SH, nop);
DEFINE_IMMEDIATE_OPERAND(FM, FM, nop);
DEFINE_IMMEDIATE_OPERAND(SHB, vSH, nop);

#undef DEFINE_IMMEDIATE_OPERAND

#endif /* PPC_OPERANDS_H */
