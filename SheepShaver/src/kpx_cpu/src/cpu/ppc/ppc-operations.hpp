/*
 *  ppc-operations.cpp - PowerPC specific operations
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

#ifndef PPC_OPERATIONS_H
#define PPC_OPERATIONS_H

#include <math.h>

/**
 *	Define an unary/binary/trinary operation
 *
 *		NAME	Name of the operation
 *		TYPE	Type of operands and result
 *		EXPR	C++ expression defining the operation, parameters are x/y/z/t
 **/

#define DEFINE_OP1(NAME, TYPE, EXPR)			\
struct op_##NAME {								\
	static inline TYPE apply(TYPE x) {			\
		return EXPR;							\
	}											\
}

#define DEFINE_OP2(NAME, TYPE, EXPR)			\
struct op_##NAME {								\
	static inline TYPE apply(TYPE x, TYPE y) {	\
		return EXPR;							\
	}											\
}

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

// Integer basic operations

DEFINE_OP1(nop, uint32, x);
DEFINE_OP1(neg, uint32, -x);
DEFINE_OP1(compl, uint32, ~x);
DEFINE_OP2(add, uint32, x + y);
DEFINE_OP2(sub, uint32, x - y);
DEFINE_OP2(mul, uint32, x * y);
DEFINE_OP2(smul, int32, x * y);
DEFINE_OP2(div, uint32, x / y);
DEFINE_OP2(sdiv, int32, x / y);
DEFINE_OP2(mod, uint32, x % y);
DEFINE_OP2(and, uint32, x & y);
DEFINE_OP2(or,  uint32, x | y);
DEFINE_OP2(xor, uint32, x ^ y);
DEFINE_OP2(orc, uint32, x | ~y);
DEFINE_OP2(andc,uint32, x & ~y);
DEFINE_OP2(nand,uint32, ~(x & y));
DEFINE_OP2(nor, uint32, ~(x | y));
DEFINE_OP2(eqv, uint32, ~(x ^ y));
DEFINE_OP2(shll, uint32, x << y);
DEFINE_OP2(shrl, uint32, x >> y);
DEFINE_OP2(shra, uint32, (int32)x >> y);
DEFINE_OP2(rotl, uint32, ((x << y) | (x >> (32 - y))));
DEFINE_OP2(rotr, uint32, ((x >> y) | (x << (32 - y))));

DEFINE_OP4(ppc_rlwimi, uint32, (op_rotl::apply(x, y) & z) | (t & ~z));
DEFINE_OP3(ppc_rlwinm, uint32, (op_rotl::apply(x, y) & z));
DEFINE_OP3(ppc_rlwnm, uint32, (op_rotl::apply(x, (y & 0x1f)) & z));


// Floating-point basic operations

DEFINE_OP1(fnop, double, x);
DEFINE_OP1(fabs, double, fabs(x));
DEFINE_OP2(fadd, double, x + y);
DEFINE_OP2(fdiv, double, x / y);
DEFINE_OP3(fmadd, double, (x * y) + z);
DEFINE_OP3(fmsub, double, (x * y) - z);
DEFINE_OP2(fmul, double, x * y);
DEFINE_OP1(fnabs, double, -fabs(x));
DEFINE_OP1(fneg, double, -x);
DEFINE_OP3(fnmadd, double, -((x * y) + z));
DEFINE_OP3(fnmsub, double, -((x * y) - z));
DEFINE_OP2(fsub, double, x - y);

#undef DEFINE_OP1
#undef DEFINE_OP2
#undef DEFINE_OP3
#undef DEFINE_OP4


// Sign/Zero-extend operation

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
