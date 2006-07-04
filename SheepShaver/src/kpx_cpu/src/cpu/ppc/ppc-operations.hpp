/*
 *  ppc-operations.cpp - PowerPC specific operations
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

#ifndef PPC_OPERATIONS_H
#define PPC_OPERATIONS_H

#include <math.h>
#include "mathlib/mathlib.hpp"

/**
 *	Define an unary/binary/trinary operation
 *
 *		NAME	Name of the operation
 *		TYPE	Type of operands and result
 *		EXPR	C++ expression defining the operation, parameters are x/y/z/t
 **/

#define DEFINE_ALIAS_OP(NAME, T_NAME, TYPE)		\
typedef op_template_##T_NAME<TYPE> op_##NAME

#define DEFINE_OP1(NAME, TYPE, EXPR)			\
struct op_##NAME {								\
	static inline TYPE apply(TYPE x) {			\
		return EXPR;							\
	}											\
}

#define DEFINE_TEMPLATE_OP1(NAME, EXPR)			\
template< class TYPE >							\
DEFINE_OP1(template_##NAME, TYPE, EXPR)

#define DEFINE_OP2(NAME, TYPE, EXPR)			\
struct op_##NAME {								\
	static inline TYPE apply(TYPE x, TYPE y) {	\
		return EXPR;							\
	}											\
}

#define DEFINE_TEMPLATE_OP2(NAME, EXPR)			\
template< class TYPE >							\
DEFINE_OP2(template_##NAME, TYPE, EXPR)

#define DEFINE_OP3(NAME, TYPE, EXPR)					\
struct op_##NAME {										\
	static inline TYPE apply(TYPE x, TYPE y, TYPE z) {	\
		return EXPR;									\
	}													\
}

#define DEFINE_OP4(NAME, TYPE, EXPR)							\
struct op_##NAME {												\
	static inline TYPE apply(TYPE x, TYPE y, TYPE z, TYPE t) {	\
		return EXPR;											\
	}															\
}

// Basic operations

DEFINE_TEMPLATE_OP1(nop, x);
DEFINE_TEMPLATE_OP2(add, x + y);
DEFINE_TEMPLATE_OP2(sub, x - y);
DEFINE_TEMPLATE_OP2(mul, x * y);
DEFINE_TEMPLATE_OP2(div, x / y);
DEFINE_TEMPLATE_OP2(and, x & y);
DEFINE_TEMPLATE_OP2(or,  x | y);
DEFINE_TEMPLATE_OP2(xor, x ^ y);
DEFINE_TEMPLATE_OP2(orc, x | ~y);
DEFINE_TEMPLATE_OP2(andc,x & ~y);
DEFINE_TEMPLATE_OP2(nand,~(x & y));
DEFINE_TEMPLATE_OP2(nor, ~(x | y));
DEFINE_TEMPLATE_OP2(eqv, ~(x ^ y));

// Integer basic operations

DEFINE_ALIAS_OP(nop, nop, uint32);
DEFINE_ALIAS_OP(add, add, uint32);
DEFINE_ALIAS_OP(sub, sub, uint32);
DEFINE_ALIAS_OP(mul, mul, uint32);
DEFINE_ALIAS_OP(smul,mul, int32);
DEFINE_ALIAS_OP(div, div, uint32);
DEFINE_ALIAS_OP(sdiv,div, int32);
DEFINE_OP1(neg, uint32, -x);
DEFINE_OP1(compl, uint32, ~x);
DEFINE_OP2(mod, uint32, x % y);
DEFINE_ALIAS_OP(and, and, uint32);
DEFINE_ALIAS_OP(or,  or,  uint32);
DEFINE_ALIAS_OP(xor, xor, uint32);
DEFINE_ALIAS_OP(orc, orc, uint32);
DEFINE_ALIAS_OP(andc,andc,uint32);
DEFINE_ALIAS_OP(nand,nand,uint32);
DEFINE_ALIAS_OP(nor, nor, uint32);
DEFINE_ALIAS_OP(eqv, eqv, uint32);
DEFINE_OP2(shll, uint32, x << y);
DEFINE_OP2(shrl, uint32, x >> y);
DEFINE_OP2(shra, uint32, (int32)x >> y);
DEFINE_OP2(rotl, uint32, ((x << y) | (x >> (32 - y))));
DEFINE_OP2(rotr, uint32, ((x >> y) | (x << (32 - y))));

DEFINE_OP4(ppc_rlwimi, uint32, (op_rotl::apply(x, y) & z) | (t & ~z));
DEFINE_OP3(ppc_rlwinm, uint32, (op_rotl::apply(x, y) & z));
DEFINE_OP3(ppc_rlwnm, uint32, (op_rotl::apply(x, (y & 0x1f)) & z));

DEFINE_ALIAS_OP(add_64, add, uint64);
DEFINE_ALIAS_OP(sub_64, sub, uint64);
DEFINE_ALIAS_OP(smul_64,mul,  int64);
DEFINE_ALIAS_OP(and_64, and, uint64);
DEFINE_ALIAS_OP(andc_64,andc,uint64);
DEFINE_ALIAS_OP(or_64,  or,  uint64);
DEFINE_ALIAS_OP(nor_64, nor, uint64);
DEFINE_ALIAS_OP(xor_64, xor, uint64);

// Floating-point basic operations

DEFINE_OP1(fnop, double, x);
DEFINE_OP1(fabs, double, fabs(x));
DEFINE_OP2(fadd, double, x + y);
DEFINE_OP2(fdiv, double, x / y);
DEFINE_OP3(fmadd, double, mathlib_fmadd(x, y, z));
DEFINE_OP3(fmsub, double, mathlib_fmsub(x, y, z));
DEFINE_OP2(fmul, double, x * y);
DEFINE_OP1(fnabs, double, -fabs(x));
DEFINE_OP1(fneg, double, -x);
DEFINE_OP3(fnmadd, double, -mathlib_fmadd(x, y, z));
DEFINE_OP3(fnmsub, double, -mathlib_fmsub(x, y, z));
DEFINE_OP3(fnmadds, double, -(float)mathlib_fmadd(x, y, z));
DEFINE_OP3(fnmsubs, double, -(float)mathlib_fmsub(x, y, z));
DEFINE_OP2(fsub, double, x - y);
DEFINE_OP3(fsel, double, (x >= 0.0) ? y : z);
DEFINE_OP1(frim, double, floor(x));
DEFINE_OP1(frin, double, round(x));
DEFINE_OP1(frip, double, ceil(x));
DEFINE_OP1(friz, double, trunc(x));

DEFINE_OP2(fadds, float, x + y);
DEFINE_OP2(fsubs, float, x - y);
DEFINE_OP1(exp2, float, exp2f(x));
DEFINE_OP1(log2, float, log2f(x));
DEFINE_OP1(fres, float, 1 / x);
DEFINE_OP1(frsqrt, float, 1 / sqrt(x));
DEFINE_OP1(frsim, float, floorf(x));
DEFINE_OP1(frsin, float, roundf(x));
DEFINE_OP1(frsip, float, ceilf(x));
DEFINE_OP1(frsiz, float, truncf(x));

// Misc operations used in AltiVec instructions

template< class TYPE >
struct op_vrl {
	static inline TYPE apply(TYPE v, TYPE n) {
		const int sh = n & ((8 * sizeof(TYPE)) - 1);
		return ((v << sh) | (v >> ((8 * sizeof(TYPE)) - sh)));
	}
};

template< class TYPE >
struct op_vsl {
	static inline TYPE apply(TYPE v, TYPE n) {
		const int sh = n & ((8 * sizeof(TYPE)) - 1);
		return v << sh;
	}
};

template< class TYPE >
struct op_vsr {
	static inline TYPE apply(TYPE v, TYPE n) {
		const int sh = n & ((8 * sizeof(TYPE)) - 1);
		return v >> sh;
	}
};

template< uint16 round = 0 >
struct op_mhraddsh {
	static inline int32 apply(int32 a, int32 b, int32 c) {
		return (((a * b) + round) >> 15) + c;
	}
};

struct op_cvt_fp2si {
	static inline int64 apply(uint32 a, float b) {
		// Delegate saturation to upper level
		if (mathlib_isinf(b))
			return ((int64)(b < 0 ? 0x80000000 : 0x7fffffff)) << 32;
		if (mathlib_isnan(b))
			return 0;
		return (int64)(b * (1U << a));
	}
};

template< class TYPE >
struct op_cvt_si2fp {
	static inline float apply(uint32 a, TYPE b) {
		return ((float)b) / ((float)(1U << a));
	}
};

template< class TYPE >
struct op_max {
	static inline TYPE apply(TYPE a, TYPE b) {
		return (a > b) ? a : b;
	}
};

template<>
struct op_max<float> {
	static inline float apply(float a, float b) {
		// XXX The maximum of any value and a NaN is a QNaN
		if (mathlib_isnan(a))
			return a;
		if (mathlib_isnan(b))
			return b;
		return a > b ? a : b;
	}
};

template< class TYPE >
struct op_min {
	static inline TYPE apply(TYPE a, TYPE b) {
		return (a < b) ? a : b;
	}
};

template<>
struct op_min<float> {
	static inline float apply(float a, float b) {
		// XXX The minimum of any value and a NaN is a QNaN
		if (mathlib_isnan(a))
			return a;
		if (mathlib_isnan(b))
			return b;
		return a < b ? a : b;
	}
};

template< int nbytes >
struct op_all_ones {
	static const uint32 value = (1U << (8 * nbytes)) - 1;
};

template<>
struct op_all_ones<4> {
	static const uint32 value = 0xffffffff;
};

template< class VX >
struct op_cmp {
	static const uint32 result = op_all_ones<sizeof(VX)>::value;
};

template< class VX >
struct op_cmp_eq {
	static inline uint32 apply(VX a, VX b) {
		return a == b ? op_cmp<VX>::result : 0;
	}
};

template< class VX >
struct op_cmp_ge {
	static inline uint32 apply(VX a, VX b) {
		return a >= b ? op_cmp<VX>::result : 0;
	}
};

template< class VX >
struct op_cmp_gt {
	static inline uint32 apply(VX a, VX b) {
		return a > b ? op_cmp<VX>::result : 0;
	}
};

struct op_cmpbfp {
	static inline uint32 apply(float a, float b) {
		const bool le = a <= b;
		const bool ge = a >= -b;
		return (le ? 0 : (1 << 31)) | (ge ? 0 : (1 << 30));
	}
};

DEFINE_OP3(vsel, uint32, ((y & z) | (x & ~z)));
DEFINE_OP3(vmaddfp, float, ((x * z) + y));
DEFINE_OP3(vnmsubfp, float, -((x * z) - y));
DEFINE_OP3(mladduh, uint32, ((x * y) + z) & 0xffff);
DEFINE_OP2(addcuw, uint32, ((uint64)x + (uint64)y) >> 32);
DEFINE_OP2(subcuw, uint32, (~((int64)x - (int64)y) >> 32) & 1);
DEFINE_OP2(avgsb, int8,   (((int16)x + (int16)y + 1) >> 1));
DEFINE_OP2(avgsh, int16,  (((int32)x + (int32)y + 1) >> 1));
DEFINE_OP2(avgsw, int32,  (((int64)x + (int64)y + 1) >> 1));
DEFINE_OP2(avgub, uint8,  ((uint16)x + (uint16)y + 1) >> 1);
DEFINE_OP2(avguh, uint16, ((uint32)x + (uint32)y + 1) >> 1);
DEFINE_OP2(avguw, uint32, ((uint64)x + (uint64)y + 1) >> 1);


#undef DEFINE_OP1
#undef DEFINE_OP2
#undef DEFINE_OP3
#undef DEFINE_OP4

#undef DEFINE_TEMPLATE_OP1
#undef DEFINE_TEMPLATE_OP2
#undef DEFINE_TEMPLATE_OP3

#undef DEFINE_ALIAS_OP


// Sign/Zero-extend operation

struct op_sign_extend_5_32 {
	static inline uint32 apply(uint32 value) {
		if (value & 0x10)
			value -= 0x20;
		return value;
	}
};

struct op_sign_extend_16_32 {
	static inline uint32 apply(uint32 value) {
		return (uint32)(int32)(int16)value;
	}
};

struct op_sign_extend_8_32 {
	static inline uint32 apply(uint32 value) {
		return (uint32)(int32)(int8)value;
	}
};

struct op_zero_extend_16_32 {
	static inline uint32 apply(uint32 value) {
		return (uint32)(uint16)value;
	}
};

struct op_zero_extend_8_32 {
	static inline uint32 apply(uint32 value) {
		return (uint32)(uint8)value;
	}
};

struct op_sign_extend_16_32_shifted {
	static inline uint32 apply(uint32 value) {
		return op_sign_extend_16_32::apply(value) << 16;
	}
};

struct op_zero_extend_16_32_shifted {
	static inline uint32 apply(uint32 value) {
		return op_zero_extend_16_32::apply(value) << 16;
	}
};

struct op_sign_extend_BD_32 {
	static inline uint32 apply(uint32 value) {
		return op_sign_extend_16_32::apply(value << 2);
	}
};

struct op_sign_extend_LI_32 {
	static inline uint32 apply(uint32 value) {
		if (value & 0x800000)
			value |= 0xff000000;
		return value << 2;
	}
};


// And Word with Immediate value

template< uint32 value >
struct op_andi {
	static inline uint32 apply(uint32 x) {
		return x & value;
	}
};


// Count Leading Zero Word

struct op_cntlzw {
	static inline uint32 apply(uint32 x) {
		uint32 n;
		uint32 m = 0x80000000;
		for (n = 0; n < 32; n++, m >>= 1)
			if (x & m)
				break;
		return n;
	}
};

#endif /* PPC_OPERATIONS_H */
