/*
 *  ppc-execute.cpp - PowerPC semantics
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

#include <stdio.h>
#include <math.h>
#include <time.h>

#include "sysdeps.h"
#include "cpu/vm.hpp"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-operands.hpp"
#include "cpu/ppc/ppc-operations.hpp"

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

#define DEBUG 0
#include "debug.h"

/**
 *	Helper class to apply an unary/binary/trinary operation
 *
 *		OP		Operation to perform
 *		RA		Input operand register
 *		RB		Input operand register or immediate (optional: operand_NONE)
 *		RC		Input operand register or immediate (optional: operand_NONE)
 **/

template< class OP, class RA, class RB, class RC >
struct op_apply {
	template< class T >
	static inline T apply(T a, T b, T c) {
		return OP::apply(a, b, c);
	}
};

template< class OP, class RA, class RB >
struct op_apply<OP, RA, RB, null_operand> {
	template< class T >
	static inline T apply(T a, T b, T c) {
		return OP::apply(a, b);
	}
};

template< class OP, class RA >
struct op_apply<OP, RA, null_operand, null_operand> {
	template< class T >
	static inline T apply(T a, T b, T c) {
		return OP::apply(a);
	}
};

/**
 *	Illegal & NOP instructions
 **/

void powerpc_cpu::execute_illegal(uint32 opcode)
{
	fprintf(stderr, "Illegal instruction at %08x, opcode = %08x\n", pc(), opcode);
#if ENABLE_MON
	disass_ppc(stdout, pc(), opcode);

	// Start up mon in real-mode
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
#endif
	abort();
}

void powerpc_cpu::execute_nop(uint32 opcode)
{
	increment_pc(4);
}

/**
 *	Helper class to compute the overflow/carry condition
 *
 *		OP		Operation to perform
 */

template< class OP >
struct op_carry {
	static inline bool apply(uint32, uint32, uint32) {
		return false;
	}
};

template<>
struct op_carry<op_add> {
	static inline bool apply(uint32 a, uint32 b, uint32 c) {
		// TODO: use 32-bit arithmetics
		uint64 carry = (uint64)a + (uint64)b + (uint64)c;
		return (carry >> 32) != 0;
	}
};

template< class OP >
struct op_overflow {
	static inline bool apply(uint32, uint32, uint32) {
		return false;
	}
};

template<>
struct op_overflow<op_neg> {
	static inline bool apply(uint32 a, uint32, uint32) {
		return a == 0x80000000;
	};
};

template<>
struct op_overflow<op_add> {
	static inline bool apply(uint32 a, uint32 b, uint32 c) {
		// TODO: use 32-bit arithmetics
		int64 overflow = (int64)(int32)a + (int64)(int32)b + (int64)(int32)c;
		return (((uint64)overflow) >> 63) ^ (((uint32)overflow) >> 31);
	}
};

/**
 *	Perform an addition/substraction
 *
 *		RA		Input operand register, possibly 0
 *		RB		Input operand either register or immediate
 *		RC		Input carry
 *		CA		Predicate to compute the carry out of the operation
 *		OE		Predicate to compute the overflow flag
 *		Rc		Predicate to record CR0
 **/

template< class RA, class RB, class RC, class CA, class OE, class Rc >
void powerpc_cpu::execute_addition(uint32 opcode)
{
	const uint32 a = RA::get(this, opcode);
	const uint32 b = RB::get(this, opcode);
	const uint32 c = RC::get(this, opcode);
	uint32 d = a + b + c;

	// Set XER (CA) if instruction affects carry bit
	if (CA::test(opcode))
		xer().set_ca(op_carry<op_add>::apply(a, b, c));

	// Set XER (OV, SO) if instruction has OE set
	if (OE::test(opcode))
		xer().set_ov(op_overflow<op_add>::apply(a, b, c));

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((int32)d);

	// Commit result to output operand
	operand_RD::set(this, opcode, d);

	increment_pc(4);
}

/**
 *	Generic arithmetic instruction
 *
 *		OP		Operation to perform
 *		RD		Output register
 *		RA		Input operand register
 *		RB		Input operand register or immediate (optional: operand_NONE)
 *		RC		Input operand register or immediate (optional: operand_NONE)
 *		OE		Predicate to compute overflow flag
 *		Rc		Predicate to record CR0
 **/

template< class OP, class RD, class RA, class RB, class RC, class OE, class Rc >
void powerpc_cpu::execute_generic_arith(uint32 opcode)
{
	const uint32 a = RA::get(this, opcode);
	const uint32 b = RB::get(this, opcode);
	const uint32 c = RC::get(this, opcode);

	uint32 d = op_apply<OP, RA, RB, RC>::apply(a, b, c);

	// Set XER (OV, SO) if instruction has OE set
	if (OE::test(opcode))
		xer().set_ov(op_overflow<OP>::apply(a, b, c));

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((int32)d);

	// commit result to output operand
	RD::set(this, opcode, d);

	increment_pc(4);
}

/**
 *	Rotate Left Word Immediate then Mask Insert
 *
 *		SH		Shift count
 *		MA		Mask value
 *		Rc		Predicate to record CR0
 **/

template< class SH, class MA, class Rc >
void powerpc_cpu::execute_rlwimi(uint32 opcode)
{
	const uint32 n = SH::get(this, opcode);
	const uint32 m = MA::get(this, opcode);
	const uint32 rs = operand_RS::get(this, opcode);
	const uint32 ra = operand_RA::get(this, opcode);
	uint32 d = op_ppc_rlwimi::apply(rs, n, m, ra);

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((int32)d);

	// Commit result to output operand
	operand_RA::set(this, opcode, d);

	increment_pc(4);
}

/**
 *	Shift instructions
 *
 *		OP		Operation to perform
 *		RD		Output operand
 *		RA		Source operand
 *		SH		Shift count
 *		SO		Shift operation
 *		CA		Predicate to compute carry bit
 *		Rc		Predicate to record CR0
 **/

template< class OP >
struct invalid_shift {
	static inline uint32 value(uint32) {
		return 0;
	}
};

template<>
struct invalid_shift<op_shra> {
	static inline uint32 value(uint32 r) {
		return 0 - (r >> 31);
	}
};

template< class OP, class RD, class RA, class SH, class SO, class CA, class Rc >
void powerpc_cpu::execute_shift(uint32 opcode)
{
	const uint32 n = SO::apply(SH::get(this, opcode));
	const uint32 r = RA::get(this, opcode);

	// Shift operation is valid only if rB[26] = 0
	// TODO: optimize srw case where shift operation would zero result
	uint32 d = (n & 0x20) ? invalid_shift<OP>::value(r) : OP::apply(r, n);

	// Set XER (CA) if instruction is algebraic variant
	if (CA::test(opcode)) {
		const uint32 carry = (r & 0x80000000) && (r & ~(0xffffffff << n));
		xer().set_ca(carry);
	}

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((int32)d);

	// Commit result to output operand
	RD::set(this, opcode, d);

	increment_pc(4);
}

/**
 *	Branch conditional instructions
 *
 *		PC		Input program counter (PC, LR, CTR)
 *		BO		BO operand
 *		DP		Displacement operand
 *		AA		Predicate for absolute address
 *		LK		Predicate to record NPC into link register
 **/

template< class PC, class BO, class DP, class AA, class LK >
void powerpc_cpu::execute_branch(uint32 opcode)
{
	const int bo = BO::get(this, opcode);
	bool ctr_ok = true;
	bool cond_ok = true;

	if (BO_CONDITIONAL_BRANCH(bo)) {
		cond_ok = cr().test(BI_field::extract(opcode));
		if (!BO_BRANCH_IF_TRUE(bo))
			cond_ok = !cond_ok;
	}

	if (BO_DECREMENT_CTR(bo)) {
		ctr_ok = (ctr() -= 1) == 0;
		if (!BO_BRANCH_IF_CTR_ZERO(bo))
			ctr_ok = !ctr_ok;
	}

	const uint32 npc = pc() + 4;
	if (ctr_ok && cond_ok)
		pc() = ((AA::test(opcode) ? 0 : PC::get(this, opcode)) + DP::get(this, opcode)) & -4;
	else
		pc() = npc;

	if (LK::test(opcode))
		lr() = npc;
}

/**
 *	Compare instructions
 *
 *		RB		Second operand (GPR, SIMM, UIMM)
 *		CT		Type of variables to be compared (uint32, int32)
 **/

template< class RB, typename CT >
void powerpc_cpu::execute_compare(uint32 opcode)
{
	const uint32 a = operand_RA::get(this, opcode);
	const uint32 b = RB::get(this, opcode);
	const uint32 crfd = crfD_field::extract(opcode);
	record_cr(crfd, (CT)a < (CT)b ? -1 : ((CT)a > (CT)b ? +1 : 0));
	increment_pc(4);
}

/**
 *	Operations on condition register
 *
 *		OP		Operation to perform
 **/

template< class OP >
void powerpc_cpu::execute_cr_op(uint32 opcode)
{
#if PPC_HAVE_SPLIT_CR
	const uint32 crbA = crbA_field::extract(opcode);
	uint32 a = (cr().get(crbA / 4) << (crbA % 4));
	const uint32 crbB = crbB_field::extract(opcode);
	uint32 b = (cr().get(crbB / 4) << (crbB % 4));
	const uint32 crbD = crbD_field::extract(opcode);
	uint32 d = ((OP::apply(a, b) & 8) >> (crbD % 4));
	cr().set(crbD / 4, d | (cr().get(crbD / 4) & ~(1 << (3 - (crbD % 4)))));
#else
	const uint32 crbA = crbA_field::extract(opcode);
	uint32 a = (cr().get() >> (31 - crbA)) & 1;
	const uint32 crbB = crbB_field::extract(opcode);
	uint32 b = (cr().get() >> (31 - crbB)) & 1;
	const uint32 crbD = crbD_field::extract(opcode);
	uint32 d = OP::apply(a, b) & 1;
	cr().set((cr().get() & ~(1 << (31 - crbD))) | (d << (31 - crbD)));
#endif
	increment_pc(4);
}

/**
 *	Divide instructions
 *
 *		SB		Signed division
 *		OE		Predicate to compute overflow
 *		Rc		Predicate to record CR0
 **/

template< bool SB, class OE, class Rc >
void powerpc_cpu::execute_divide(uint32 opcode)
{
	const uint32 a = operand_RA::get(this, opcode);
	const uint32 b = operand_RB::get(this, opcode);
	uint32 d;

	if (b == 0 || (SB && a == 0x80000000 && b == 0xffffffff)) {
		// Reference manual says rD is undefined
		d = 0;
		if (SB) {
			// However, checking against a real PowerPC (7410) yields
			// that rD gets all bits set to rA MSB
			d = -(a >> 31);
		}
		if (OE::test(opcode))
			xer().set_ov(1);
	}
	else {
		d = SB ? (int32)a/(int32)b : a/b;
		if (OE::test(opcode))
			xer().set_ov(0);
	}

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((int32)d);

	// Commit result to output operand
	operand_RD::set(this, opcode, d);

	increment_pc(4);
}

/**
 *	Multiply instructions
 *
 *		HI		Predicate for multiply high word
 *		SB		Predicate for signed operation
 *		OE		Predicate to compute overflow
 *		Rc		Predicate to record CR0
 **/

template< bool HI, bool SB, class OE, class Rc >
void powerpc_cpu::execute_multiply(uint32 opcode)
{
	const uint32 a = operand_RA::get(this, opcode);
	const uint32 b = operand_RB::get(this, opcode);
	uint64 d = SB ? (int64)(int32)a * (int64)(int32)b : (uint64)a * (uint64)b;

	// Overflow if the product cannot be represented in 32 bits
	if (OE::test(opcode)) {
		xer().set_ov((d & UVAL64(0xffffffff80000000)) != 0 &&
					 (d & UVAL64(0xffffffff80000000)) != UVAL64(0xffffffff80000000));
	}

	// Only keep high word if multiply high instruction
	if (HI)
		d >>= 32;

	// Set CR0 (LT, GT, EQ, SO) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr0((uint32)d);

	// Commit result to output operand
	operand_RD::set(this, opcode, (uint32)d);

	increment_pc(4);
}

/**
 *	Floating-point arithmetics
 *
 *		OP		Operation to perform
 *		RD		Output register
 *		RA		Input operand
 *		RB		Input operand (optional)
 *		RC		Input operand (optional)
 *		Rc		Predicate to record CR1
 *		FPSCR	Predicate to compute FPSCR bits
 **/

template< class OP, class RD, class RA, class RB, class RC, class Rc, bool FPSCR >
void powerpc_cpu::execute_fp_arith(uint32 opcode)
{
	const double a = RA::get(this, opcode);
	const double b = RB::get(this, opcode);
	const double c = RC::get(this, opcode);
	double d = op_apply<OP, RA, RB, RC>::apply(a, b, c);

#if 0
	// FIXME: Compute FPSCR bits if instruction requests it
	if (FPSCR) {

		// Always update VX
		if (fpscr() & (FPSCR_VXSNAN_field::mask() | FPSCR_VXISI_field::mask() | \
					   FPSCR_VXISI_field::mask() | FPSCR_VXIDI_field::mask() | \
					   FPSCR_VXZDZ_field::mask() | FPSCR_VXIMZ_field::mask() | \
					   FPSCR_VXVC_field::mask() | FPSCR_VXSOFT_field::mask() | \
					   FPSCR_VXSQRT_field::mask() | FPSCR_VXCVI_field::mask()))
			fpscr() |= FPSCR_VX_field::mask();
		else
			fpscr() &= ~FPSCR_VX_field::mask();

		// Always update FEX
		if (((fpscr() & FPSCR_VX_field::mask()) && (fpscr() & FPSCR_VE_field::mask())) \
			|| ((fpscr() & FPSCR_OX_field::mask()) && (fpscr() & FPSCR_OE_field::mask())) \
			|| ((fpscr() & FPSCR_UX_field::mask()) && (fpscr() & FPSCR_UE_field::mask())) \
			|| ((fpscr() & FPSCR_ZX_field::mask()) && (fpscr() & FPSCR_ZE_field::mask())) \
			|| ((fpscr() & FPSCR_XX_field::mask()) && (fpscr() & FPSCR_XE_field::mask())))
			fpscr() |= FPSCR_FEX_field::mask();
		else
			fpscr() &= ~FPSCR_FEX_field::mask();
	}
#endif
	
	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	// Commit result to output operand
	RD::set(this, opcode, d);
	increment_pc(4);
}

/**
 *	Load/store instructions
 *
 *		OP		Operation to perform on loaded value
 *		RA		Base operand
 *		RB		Displacement (GPR(RB), EXTS(d))
 *		LD		Load operation?
 *		SZ		Size of load/store operation
 *		UP		Update RA with EA
 *		RX		Reverse operand
 **/

template< int SZ, bool RX >
struct memory_helper;

#define DEFINE_MEMORY_HELPER(SIZE)																\
template< bool RX >																				\
struct memory_helper<SIZE, RX>																	\
{																								\
	static inline uint32 load(uint32 ea) {														\
		return RX ? vm_read_memory_##SIZE##_reversed(ea) : vm_read_memory_##SIZE(ea);			\
	}																							\
	static inline void store(uint32 ea, uint32 value) {											\
		RX ? vm_write_memory_##SIZE##_reversed(ea, value) : vm_write_memory_##SIZE(ea, value);	\
	}																							\
}

DEFINE_MEMORY_HELPER(1);
DEFINE_MEMORY_HELPER(2);
DEFINE_MEMORY_HELPER(4);

template< class OP, class RA, class RB, bool LD, int SZ, bool UP, bool RX >
void powerpc_cpu::execute_loadstore(uint32 opcode)
{
	const uint32 a = RA::get(this, opcode);
	const uint32 b = RB::get(this, opcode);
	const uint32 ea = a + b;

	if (LD)
		operand_RD::set(this, opcode, OP::apply(memory_helper<SZ, RX>::load(ea)));
	else
		memory_helper<SZ, RX>::store(ea, operand_RS::get(this, opcode));

	if (UP)
		RA::set(this, opcode, ea);

	increment_pc(4);
}

template< class RA, class DP, bool LD >
void powerpc_cpu::execute_loadstore_multiple(uint32 opcode)
{
	const uint32 a = RA::get(this, opcode);
	const uint32 d = DP::get(this, opcode);
	uint32 ea = a + d;

	// FIXME: generate exception if ea is not word-aligned
	if ((ea & 3) != 0) {
#ifdef SHEEPSHAVER
		D(bug("unaligned load/store multiple to %08x\n", ea));
		increment_pc(4);
		return;
#else
		abort();
#endif
	}

	int r = LD ? rD_field::extract(opcode) : rS_field::extract(opcode);
	while (r <= 31) {
		if (LD)
			gpr(r) = vm_read_memory_4(ea);
		else
			vm_write_memory_4(ea, gpr(r));
		r++;
		ea += 4;
	}

	increment_pc(4);
}

/**
 *	Floating-point load/store instructions
 *
 *		RA		Base operand
 *		RB		Displacement (GPR(RB), EXTS(d))
 *		LD		Load operation?
 *		DB		Predicate for double value
 *		UP		Predicate to update RA with EA
 **/

template< class RA, class RB, bool LD, bool DB, bool UP >
void powerpc_cpu::execute_fp_loadstore(uint32 opcode)
{
	const uint32 a = RA::get(this, opcode);
	const uint32 b = RB::get(this, opcode);
	const uint32 ea = a + b;

	if (LD) {
		if (DB)
			operand_fp_dw_RD::set(this, opcode, vm_read_memory_8(ea));
		else {
			any_register x;
			x.i = vm_read_memory_4(ea);
			operand_fp_RD::set(this, opcode, (double)x.f);
		}
	}
	else {
		if (DB)
			vm_write_memory_8(ea, operand_fp_dw_RS::get(this, opcode));
		else {
			any_register x;
			x.f = (float)operand_fp_RS::get(this, opcode);
			vm_write_memory_4(ea, x.i);
		}
	}

	if (UP)
		RA::set(this, opcode, ea);

	increment_pc(4);
}

/**
 *	Load/Store String Word instruction
 *
 *		RA		Input operand as base EA
 *		IM		lswi mode?
 *		NB		Number of bytes to transfer
 **/

template< class RA, bool IM, class NB >
void powerpc_cpu::execute_load_string(uint32 opcode)
{
	uint32 ea = RA::get(this, opcode);
	if (!IM)
		ea += operand_RB::get(this, opcode);

	int nb = NB::get(this, opcode);
	if (IM && nb == 0)
		nb = 32;

	int rd = rD_field::extract(opcode);
#if 1
	int i;
	for (i = 0; nb - i >= 4; i += 4, rd = (rd + 1) & 0x1f)
		gpr(rd) = vm_read_memory_4(ea + i);
	switch (nb - i) {
	case 1:
		gpr(rd) = vm_read_memory_1(ea + i) << 24;
		break;
	case 2:
		gpr(rd) = vm_read_memory_2(ea + i) << 16;
		break;
	case 3:
		gpr(rd) = (vm_read_memory_2(ea + i) << 16) + (vm_read_memory_1(ea + i + 2) << 8);
		break;
	}
#else
	for (int i = 0; i < nb; i++) {
		switch (i & 3) {
		case 0:
			gpr(rd) = vm_read_memory_1(ea + i) << 24;
			break;
		case 1:
			gpr(rd) = (gpr(rd) & 0xff00ffff) | (vm_read_memory_1(ea + i) << 16);
			break;
		case 2:
			gpr(rd) = (gpr(rd) & 0xffff00ff) | (vm_read_memory_1(ea + i) << 8);
			break;
		case 3:
			gpr(rd) = (gpr(rd) & 0xffffff00) | vm_read_memory_1(ea + i);
			rd = (rd + 1) & 0x1f;
			break;
		}
	}
#endif

	increment_pc(4);
}

template< class RA, bool IM, class NB >
void powerpc_cpu::execute_store_string(uint32 opcode)
{
	uint32 ea = RA::get(this, opcode);
	if (!IM)
		ea += operand_RB::get(this, opcode);

	int nb = NB::get(this, opcode);
	if (IM && nb == 0)
		nb = 32;

	int rs = rS_field::extract(opcode);
	int sh = 24;
	for (int i = 0; i < nb; i++) {
		vm_write_memory_1(ea + i, gpr(rs) >> sh);
		sh -= 8;
		if (sh < 0) {
			sh = 24;
			rs = (rs + 1) & 0x1f;
		}
	}

	increment_pc(4);
}

/**
 *	Load Word and Reserve Indexed / Store Word Conditional Indexed
 *
 *		RA		Input operand as base EA
 **/

template< class RA >
void powerpc_cpu::execute_lwarx(uint32 opcode)
{
	const uint32 ea = RA::get(this, opcode) + operand_RB::get(this, opcode);
	regs.reserve_valid = 1;
	regs.reserve_addr = ea;
	regs.reserve_data = vm_read_memory_4(ea);
	operand_RD::set(this, opcode, regs.reserve_data);
	increment_pc(4);
}

template< class RA >
void powerpc_cpu::execute_stwcx(uint32 opcode)
{
	const uint32 ea = RA::get(this, opcode) + operand_RB::get(this, opcode);
	cr().clear(0);
	if (regs.reserve_valid) {
		if (regs.reserve_addr == ea /* physical_addr(EA) */
			&& /* HACK */ regs.reserve_data == vm_read_memory_4(ea)) {
			vm_write_memory_4(ea, operand_RS::get(this, opcode));
			cr().set(0, standalone_CR_EQ_field::mask());
		}
		regs.reserve_valid = 0;
	}
	cr().set_so(0, xer().get_so());
	increment_pc(4);
}

/**
 *	Basic (and probably incorrect) implementation for missing functions
 *	in older C libraries
 **/

#ifndef signbit
#define signbit(X) my_signbit(X)
#endif

static inline bool my_signbit(double X) {
	return X < 0.0;
}

#ifndef isless
#define isless(X, Y) my_isless(X, Y)
#endif

static inline bool my_isless(double X, double Y) {
	return X < Y;
}

#ifndef isgreater
#define isgreater(X, Y) my_isgreater(X, Y)
#endif

static inline bool my_isgreater(double X, double Y) {
	return X > Y;
}

/**
 *	Floating-point compare instruction
 *
 *		OC		Predicate for ordered compare
 **/

static inline bool is_NaN(double v) {
	any_register x; x.d = v;
	return (((x.j & UVAL64(0x7ff0000000000000)) == UVAL64(0x7ff0000000000000)) &&
			((x.j & UVAL64(0x000fffffffffffff)) != 0));
}

static inline bool is_SNaN(double v) {
	any_register x; x.d = v;
	return is_NaN(v) && !(x.j & UVAL64(0x0008000000000000)) ? signbit(v) : false;
}

static inline bool is_QNaN(double v) {
	return is_NaN(v) && !is_SNaN(v);
}

static inline bool is_NaN(float v) {
	any_register x; x.f = v;
	return (((x.i & 0x7f800000) == 0x7f800000) &&
			((x.i & 0x007fffff) != 0));
}

static inline bool is_SNaN(float v) {
	any_register x; x.f = v;
	return is_NaN(v) && !(x.i & 0x00400000) ? signbit(v) : false;
}

static inline bool is_QNaN(float v) {
	return is_NaN(v) && !is_SNaN(v);
}

template< bool OC >
void powerpc_cpu::execute_fp_compare(uint32 opcode)
{
	const double a = operand_fp_RA::get(this, opcode);
	const double b = operand_fp_RB::get(this, opcode);
	const int crfd = crfD_field::extract(opcode);

	if (is_NaN(a) || is_NaN(b))
		cr().set(crfd, 1);
	else if (isless(a, b))
		cr().set(crfd, 8);
	else if (isgreater(a, b))
		cr().set(crfd, 4);
	else
		cr().set(crfd, 2);

#ifndef PPC_NO_FPSCR_UPDATE
	fpscr() = (fpscr() & ~FPSCR_FPCC_field::mask()) | (cr().get(crfd) << 12);
	if (is_SNaN(a) || is_SNaN(b)) {
		fpscr() |= FPSCR_VXSNAN_field::mask();
		if (OC && !FPSCR_VE_field::test(fpscr()))
			fpscr() |= FPSCR_VXVC_field::mask();
	}
	else if (OC && (is_QNaN(a) || is_QNaN(b)))
		fpscr() |= FPSCR_VXVC_field::mask();
#endif

	increment_pc(4);
}

/**
 *	Floating Convert to Integer Word instructions
 *
 *		RN		Rounding mode
 *		Rc		Predicate to record CR1
 **/

template< class RN, class Rc >
void powerpc_cpu::execute_fp_int_convert(uint32 opcode)
{
	const double b = operand_fp_RB::get(this, opcode);
	const uint32 r = RN::get(this, opcode);
	any_register d;

	switch (r) {
	case 0: // Round to nearest
		d.j = (int32)(b + 0.5);
		break;
	case 1: // Round toward zero
		d.j = (int32)b;
		break;
	case 2: // Round toward +infinity
		d.j = (int32)ceil(b);
		break;
	case 3: // Round toward -infinity
		d.j = (int32)floor(b);
		break;
	}

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	// Commit result to output operand
	operand_fp_RD::set(this, opcode, d.d);
	increment_pc(4);
}

/**
 *	Floating-point Round to Single
 *
 *		Rc		Predicate to record CR1
 **/

void powerpc_cpu::fp_classify(double x)
{
#ifndef PPC_NO_FPSCR_UPDATE
	uint32 c = fpscr() & ~FPSCR_FPRF_field::mask();
	uint8 fc = fpclassify(x);
	switch (fc) {
	case FP_NAN:
		c |= FPSCR_FPRF_FU_field::mask() | FPSCR_FPRF_C_field::mask();
		break;
	case FP_ZERO:
		c |= FPSCR_FPRF_FE_field::mask();
		if (signbit(x))
			c |= FPSCR_FPRF_C_field::mask();
		break;
	case FP_INFINITE:
		c |= FPSCR_FPRF_FU_field::mask();
		goto FL_FG_field;
	case FP_SUBNORMAL:
		c |= FPSCR_FPRF_C_field::mask();
		// fall-through
	case FP_NORMAL:
	  FL_FG_field:
		if (x < 0)
			c |= FPSCR_FPRF_FL_field::mask();
		else
			c |= FPSCR_FPRF_FG_field::mask();
		break;
	}
	fpscr() = c;
#endif
}

template< class Rc >
void powerpc_cpu::execute_fp_round(uint32 opcode)
{
	const double b = operand_fp_RB::get(this, opcode);
	float d = (float)b;

	// FPSCR[FPRF] is set to the class and sign of the result
	fp_classify(d);

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	// Commit result to output operand
	operand_fp_RD::set(this, opcode, (double)d);
	increment_pc(4);
}

/**
 *		System Call instruction
 **/

void powerpc_cpu::execute_syscall(uint32 opcode)
{
	cr().set_so(0, execute_do_syscall && !execute_do_syscall(this));
	increment_pc(4);
}

/**
 *		Instructions dealing with system registers
 **/

void powerpc_cpu::execute_mcrf(uint32 opcode)
{
	const int crfS = crfS_field::extract(opcode);
	const int crfD = crfD_field::extract(opcode);
	cr().set(crfD, cr().get(crfS));
	increment_pc(4);
}

void powerpc_cpu::execute_mtcrf(uint32 opcode)
{
	uint32 mask = field2mask[CRM_field::extract(opcode)];
	cr().set((operand_RS::get(this, opcode) & mask) | (cr().get() & ~mask));
	increment_pc(4);
}

template< class FM, class RB, class Rc >
void powerpc_cpu::execute_mtfsf(uint32 opcode)
{
	const uint64 fsf = RB::get(this, opcode);
	const uint32 f = FM::get(this, opcode);
	uint32 m = field2mask[f];

	// FPSCR[FX] is altered only if FM[0] = 1
	if ((f & 0x80) == 0)
		m &= ~FPSCR_FX_field::mask();

#ifndef PPC_NO_FPSCR_UPDATE
	// Move frB bits to FPSCR according to field mask
	fpscr() = (fsf & m) | (fpscr() & ~m);
#endif

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	increment_pc(4);
}

template< class RB, class Rc >
void powerpc_cpu::execute_mtfsfi(uint32 opcode)
{
	const uint32 crfD = crfD_field::extract(opcode);
	uint32 m = 0xf << (4 * (7 - crfD));

	// FPSCR[FX] is altered only if crfD = 0
	if (crfD == 0)
		m &= ~FPSCR_FX_field::mask();

#ifndef PPC_NO_FPSCR_UPDATE
	// Move immediate to FPSCR according to field crfD
	fpscr() = (RB::get(this, opcode) & m) | (fpscr() & ~m);
#endif

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	increment_pc(4);
}

template< class RB, class Rc >
void powerpc_cpu::execute_mtfsb(uint32 opcode)
{
	// Bit crbD of the FPSCR is set or cleared
	const uint32 crbD = crbD_field::extract(opcode);
	fpscr() = (fpscr() & ~(1 << (31 - crbD))) | (RB::get(this, opcode) << (31 - crbD));

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	increment_pc(4);
}

template< class Rc >
void powerpc_cpu::execute_mffs(uint32 opcode)
{
	// Move FPSCR to FPR(FRD)
	operand_fp_dw_RD::set(this, opcode, fpscr());

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	increment_pc(4);
}

void powerpc_cpu::execute_mfmsr(uint32 opcode)
{
	operand_RD::set(this, opcode, 0xf072);
	increment_pc(4);
}

template< class SPR >
void powerpc_cpu::execute_mfspr(uint32 opcode)
{
	uint32 spr = SPR::get(this, opcode);
	uint32 d;
	switch (spr) {
	case 1: d = xer().get(); break;
	case 8: d = lr(); break;
	case 9: d = ctr(); break;
#ifdef SHEEPSHAVER
	case 25: // SDR1
		d = 0xdead001f;
		break;
	case 287: { // PVR
		extern uint32 PVR;
		d = PVR;
		break;
	}
	default: d = 0;
#else
	default: execute_illegal(opcode);
#endif
	}
	operand_RD::set(this, opcode, d);
	increment_pc(4);
}

template< class SPR >
void powerpc_cpu::execute_mtspr(uint32 opcode)
{
	uint32 spr = SPR::get(this, opcode);
	const uint32 s = operand_RS::get(this, opcode);

	switch (spr) {
	case 1: xer().set(s); break;
	case 8: lr() = s; break;
	case 9: ctr() = s; break;
#ifndef SHEEPSHAVER
	default: execute_illegal(opcode);
#endif
	}

	increment_pc(4);
}

template< class TBR >
void powerpc_cpu::execute_mftbr(uint32 opcode)
{
	uint32 tbr = TBR::get(this, opcode);
	uint32 d;
	switch (tbr) {
	case 268: d = clock(); break;
	case 269: d = tbu(); break;
	default: execute_illegal(opcode);
	}
	operand_RD::set(this, opcode, d);
	increment_pc(4);
}

/**
 *		Instruction cache management
 **/

void powerpc_cpu::execute_invalidate_cache_range()
{
	if (cache_range.start != cache_range.end) {
		invalidate_cache_range(cache_range.start, cache_range.end);
		cache_range.start = cache_range.end = 0;
	}
}

template< class RA, class RB >
void powerpc_cpu::execute_icbi(uint32 opcode)
{
	const uint32 ea = RA::get(this, opcode) + RB::get(this, opcode);
	const uint32 block_start = ea - (ea % 32);

	if (block_start == cache_range.end) {
		// Extend region to invalidate
		cache_range.end += 32;
	}
	else {
		// New region to invalidate
		execute_invalidate_cache_range();
		cache_range.start = block_start;
		cache_range.end = cache_range.start + 32;
	}

	increment_pc(4);
}

void powerpc_cpu::execute_isync(uint32 opcode)
{
	execute_invalidate_cache_range();
	increment_pc(4);
}

/**
 *		(Fake) data cache management
 **/

template< class RA, class RB >
void powerpc_cpu::execute_dcbz(uint32 opcode)
{
	uint32 ea = RA::get(this, opcode) + RB::get(this, opcode);
	vm_memset(ea - (ea % 32), 0, 32);
	increment_pc(4);
}

/**
 *		Explicit template instantiations
 **/

#include "ppc-execute-impl.cpp"
