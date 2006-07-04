/*
 *  ppc-execute.hpp - PowerPC semantic action templates
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

#ifndef PPC_EXECUTE_H
#define PPC_EXECUTE_H

// This file is designed to be included from implementation files only.
#ifdef DYNGEN_OPS
#define PPC_CPU							powerpc_dyngen_helper
#define DEFINE_HELPER(NAME, ARGS)		static inline uint32 NAME ARGS
#define RETURN(VAL)						dyngen_barrier(); return (VAL)
#else
#define PPC_CPU							powerpc_cpu
#define DEFINE_HELPER(NAME, ARGS)		inline uint32 powerpc_cpu::NAME ARGS
#define RETURN(VAL)						return (VAL)
#endif


template< bool SB > struct register_value { typedef uint32 type; };
template< > struct register_value< true > { typedef  int32 type; };

/**
 *	Helper class to apply an unary/binary/trinary operation
 *
 *		OP		Operation to perform
 *		RA		Input operand register
 *		RB		Input operand register or immediate (optional: operand_NONE)
 *		RC		Input operand register or immediate (optional: operand_NONE)
 **/

struct null_operand;
struct null_vector_operand;

template< class RT, class OP, class RA, class RB, class RC >
struct op_apply {
	template< class A, class B, class C >
	static inline RT apply(A a, B b, C c) {
		return OP::apply(a, b, c);
	}
};

template< class RT, class OP, class RA, class RB >
struct op_apply<RT, OP, RA, RB, null_operand> {
	template< class A, class B, class C >
	static inline RT apply(A a, B b, C) {
		return OP::apply(a, b);
	}
};

template< class RT, class OP, class RA >
struct op_apply<RT, OP, RA, null_operand, null_operand> {
	template< class A, class B, class C >
	static inline RT apply(A a, B, C) {
		return OP::apply(a);
	}
};

template< class RT, class OP, class RA, class RB >
struct op_apply<RT, OP, RA, RB, null_vector_operand> {
	template< class A, class B, class C >
	static inline RT apply(A a, B b, C) {
		return (RT)OP::apply(a, b);
	}
};

template< class RT, class OP, class RA >
struct op_apply<RT, OP, RA, null_vector_operand, null_vector_operand> {
	template< class A, class B, class C >
	static inline RT apply(A a, B, C) {
		return (RT)OP::apply(a);
	}
};

template< class RT, class OP, class RB >
struct op_apply<RT, OP, null_vector_operand, RB, null_vector_operand> {
	template< class A, class B, class C >
	static inline RT apply(A, B b, C) {
		return (RT)OP::apply(b);
	}
};

/**
 *		Add instruction templates
 **/

template< bool EX, bool CA, bool OE >
DEFINE_HELPER(do_execute_addition, (uint32 RA, uint32 RB))
{
	uint32 RD = RA + RB + (EX ? PPC_CPU::xer().get_ca() : 0);

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	if (EX) {
		const bool ca = _RB ^ ((_RB ^ _RA) & (_RA ^ _RD));
		PPC_CPU::xer().set_ca(ca);
	}
	else if (CA) {
		const bool ca = (uint32)RD < (uint32)RA;
		PPC_CPU::xer().set_ca(ca);
	}

	if (OE)
		PPC_CPU::xer().set_ov((_RB ^ _RD) & (_RA ^ _RD));

	RETURN(RD);
}

/**
 *		Subtract instruction templates
 **/

template< bool CA, bool OE >
DEFINE_HELPER(do_execute_subtract, (uint32 RA, uint32 RB))
{
	uint32 RD = RB - RA;

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	if (CA)
		PPC_CPU::xer().set_ca((uint32)RD <= (uint32)RB);

	if (OE)
		PPC_CPU::xer().set_ov((_RA ^ _RB) & (_RD ^ _RB));

	RETURN(RD);
}

template< bool OE >
DEFINE_HELPER(do_execute_subtract_extended, (uint32 RA, uint32 RB))
{
	const uint32 RD = ~RA + RB + PPC_CPU::xer().get_ca();

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	const bool ca = !_RA ^ ((_RA ^ _RD) & (_RB ^ _RD));
	PPC_CPU::xer().set_ca(ca);

	if (OE)
		PPC_CPU::xer().set_ov((_RA ^ _RB) & (_RD ^ _RB));

	RETURN(RD);
}

/**
 *		Divide instruction templates
 **/

template< bool SB, bool OE >
DEFINE_HELPER(do_execute_divide, (uint32 RA, uint32 RB))
{
	typename register_value<SB>::type a = RA;
	typename register_value<SB>::type b = RB;
	uint32 RD;

	if (b == 0 || (SB && a == 0x80000000 && b == -1)) {
		// Reference manual says result is undefined but it gets all
		// bits set to MSB on a real processor
		RD = SB ? ((int32)RA >> 31) : 0;
		if (OE)
			PPC_CPU::xer().set_ov(1);
	}
	else {
		RD = a / b;
		if (OE)
			PPC_CPU::xer().set_ov(0);
	}

	RETURN(RD);
}

/**
 *		FP load/store
 **/

// C.6 Floating-Point Load Instructions
static inline uint64 fp_load_single_convert(uint32 v)
{
	// XXX we currently use the native floating-point capabilities
	any_register x;
	x.i = v;
	x.d = (double)x.f;
	return x.j;
}

// C.7 Floating-Point Store Instructions
static inline uint32 fp_store_single_convert(uint64 v)
{
	int exp = (v >> 52) & 0x7ff;
	if (exp < 874 || exp > 896) {
		// No denormalization required (or "undefined" behaviour)
		// WORD[0 - 1] = frS[0 - 1]
		// WORD[2 - 31] = frS[5 - 34]
		return (uint32)(((v >> 32) & 0xc0000000) | ((v >> 29) & 0x3fffffff));
	}

	// Handle denormalization (874 <= frS[1 - 11] <= 896
	// XXX we currently use the native floating-point capabilities
	any_register x;
	x.j = v;
	x.f = (float)x.d;
	return x.i;
}

/**
 *		FP classification
 **/

static inline bool is_NaN(double v)
{
	any_register x; x.d = v;
	return (((x.j & UVAL64(0x7ff0000000000000)) == UVAL64(0x7ff0000000000000)) &&
			((x.j & UVAL64(0x000fffffffffffff)) != 0));
}

static inline bool is_QNaN(double v)
{
	any_register x; x.d = v;
	return is_NaN(v) && (x.j & UVAL64(0x0008000000000000));
}

static inline bool is_SNaN(double v)
{
	return is_NaN(v) && !is_QNaN(v);
}

static inline bool is_NaN(float v)
{
	any_register x; x.f = v;
	return (((x.i & 0x7f800000) == 0x7f800000) &&
			((x.i & 0x007fffff) != 0));
}

static inline bool is_QNaN(float v)
{
	any_register x; x.f = v;
	return is_NaN(v) && (x.i & 0x00400000);
}

static inline bool is_SNaN(float v)
{
	return is_NaN(v) && !is_QNaN(v);
}

/**
 *		Check for FP Exception Conditions
 **/

template< class OP >
struct fp_exception_condition {
	static inline uint32 apply(double) {
		return 0;
	}
	static inline uint32 apply(double, double) {
		return 0;
	}
	static inline uint32 apply(double, double, double) {
		return 0;
	}
};

template< class FP >
struct fp_invalid_operation_condition {
	static inline uint32 apply(int flags) {
		uint32 exceptions = 0;
		if (FPSCR_VXSOFT_field::test(flags))
			exceptions |= FPSCR_VXSOFT_field::mask();
		return 0;
	}
	static inline uint32 apply(int flags, FP a) {
		uint32 exceptions = 0;
		if (FPSCR_VXSNAN_field::test(flags) && is_SNaN(a))
			exceptions |= FPSCR_VXSNAN_field::mask();
		if (FPSCR_VXVC_field::test(flags) && is_NaN(a))
			exceptions |= FPSCR_VXVC_field::mask();
		if (FPSCR_VXSQRT_field::test(flags) && signbit(a))
			exceptions |= FPSCR_VXSQRT_field::mask();
		return exceptions;
	}
	static inline uint32 apply(int flags, FP a, FP b, bool negate = false) {
		uint32 exceptions = apply(flags) | apply(flags, a) | apply(flags, b);
		if (FPSCR_VXISI_field::test(flags) && isinf(a) && isinf(b)) {
			if (( negate && (signbit(a) == signbit(b))) ||
				(!negate && (signbit(a) != signbit(b))))
				exceptions |= FPSCR_VXISI_field::mask();
		}
		if (FPSCR_VXIDI_field::test(flags) && isinf(a) && isinf(b))
			exceptions |= FPSCR_VXIDI_field::mask();
		if (FPSCR_VXZDZ_field::test(flags) && a == 0 && b == 0)
			exceptions |= FPSCR_VXZDZ_field::mask();
		if (FPSCR_VXIMZ_field::test(flags) && ((a == 0 && isinf(b)) || (isinf(a) && b == 0)))
			exceptions |= FPSCR_VXIMZ_field::mask();
		return exceptions;
	}
};

#define DEFINE_FP_INVALID_OPERATION(OP, TYPE, EXCP, NEGATE)						\
template<>																		\
struct fp_exception_condition<OP> {												\
	static inline uint32 apply(TYPE a, TYPE b) {								\
		return fp_invalid_operation_condition<TYPE>::apply(EXCP, a, b, NEGATE);	\
	}																			\
};

DEFINE_FP_INVALID_OPERATION(op_fadd, double, FPSCR_VXSNAN_field::mask() | FPSCR_VXISI_field::mask(), 0);
DEFINE_FP_INVALID_OPERATION(op_fsub, double, FPSCR_VXSNAN_field::mask() | FPSCR_VXISI_field::mask(), 1);
DEFINE_FP_INVALID_OPERATION(op_fmul, double, FPSCR_VXSNAN_field::mask() | FPSCR_VXIMZ_field::mask(), 0);

template< class FP >
struct fp_divide_exception_condition {
	static inline uint32 apply(FP a, FP b) {
		int exceptions =
			fp_invalid_operation_condition<FP>::
			apply(FPSCR_VXSNAN_field::mask() | FPSCR_VXIDI_field::mask() | FPSCR_VXZDZ_field::mask(),
				  a, b);
		if (isfinite(a) && a != 0 && b == 0)
			exceptions = FPSCR_ZX_field::mask();
		return exceptions;
	}
};

template<> struct fp_exception_condition<op_fdiv> : fp_divide_exception_condition<double> { };

template< class FP, bool negate >
struct fp_fma_exception_condition {
	static inline uint32 apply(FP a, FP b, FP c) {
		int exceptions =
			fp_invalid_operation_condition<FP>::
			apply(FPSCR_VXSNAN_field::mask(), a) |
			fp_invalid_operation_condition<FP>::
			apply(FPSCR_VXSNAN_field::mask(), b) |
			fp_invalid_operation_condition<FP>::
			apply(FPSCR_VXSNAN_field::mask(), c) |
			fp_invalid_operation_condition<FP>::
			apply(FPSCR_VXIMZ_field::mask(), a, b);
		// 2.1.5 -- The XER bit definitions are based on the
		// operation of an instruction considered as a whole,
		// not on intermediate results
		if ((isinf(a) || isinf(b)) && isinf(c)) {
			FP m = a * b;
			if (isinf(m)) {
				// make sure the intermediate result is an infinity and not a NaN
				// FIXME: it could be faster to use (exceptions & IMZ) == 0 instead of isinf()
				if (( negate && (signbit(m) == signbit(c))) ||
					(!negate && (signbit(m) != signbit(c))))
					exceptions |= FPSCR_VXISI_field::mask();
			}
		}
		return exceptions;
	}
};

template<> struct fp_exception_condition<op_fmadd> : fp_fma_exception_condition<double, false> { };
template<> struct fp_exception_condition<op_fmsub> : fp_fma_exception_condition<double, true> { };
template<> struct fp_exception_condition<op_fnmadd> : fp_fma_exception_condition<double, false> { };
template<> struct fp_exception_condition<op_fnmsub> : fp_fma_exception_condition<double, true> { };

#endif /* PPC_EXECUTE_H */
