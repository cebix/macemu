/*
 *  ppc-execute.cpp - PowerPC semantics
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

#include <stdio.h>
#include <math.h>
#include <time.h>

#include "cpu/vm.hpp"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-operands.hpp"
#include "cpu/ppc/ppc-operations.hpp"
#include "cpu/ppc/ppc-execute.hpp"

#ifndef SHEEPSHAVER
#include "basic-kernel.hpp"
#endif

#ifdef SHEEPSHAVER
#include "main.h"
#endif

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

#define DEBUG 0
#include "debug.h"

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
 *  Floating-point rounding modes conversion
 **/

static inline int ppc_to_native_rounding_mode(int round)
{
	switch (round) {
	case 0: return FE_TONEAREST;
	case 1: return FE_TOWARDZERO;
	case 2: return FE_UPWARD;
	case 3: return FE_DOWNWARD;
	}
}

static inline int native_to_ppc_rounding_mode(int round)
{
	switch (round) {
	case FE_TONEAREST:	return 0;
	case FE_TOWARDZERO:	return 1;
	case FE_UPWARD:		return 2;
	case FE_DOWNWARD:	return 3;
	}
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

	uint32 d = op_apply<uint32, OP, RA, RB, RC>::apply(a, b, c);

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
	uint32 d;

	// Shift operation is valid only if rB[26] = 0
	if (n & 0x20) {
		d = invalid_shift<OP>::value(r);
		if (CA::test(opcode))
			xer().set_ca(d >> 31);
	}
	else {
		d = OP::apply(r, n);
		if (CA::test(opcode)) {
			const uint32 ca = (r & 0x80000000) && (r & ~(0xffffffff << n));
			xer().set_ca(ca);
		}
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
	const uint32 crbA = crbA_field::extract(opcode);
	uint32 a = (cr().get() >> (31 - crbA)) & 1;
	const uint32 crbB = crbB_field::extract(opcode);
	uint32 b = (cr().get() >> (31 - crbB)) & 1;
	const uint32 crbD = crbD_field::extract(opcode);
	uint32 d = OP::apply(a, b) & 1;
	cr().set((cr().get() & ~(1 << (31 - crbD))) | (d << (31 - crbD)));
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

	// Specialize divide semantic action
	if (OE::test(opcode))
		d = do_execute_divide<SB, true>(a, b);
	else
		d = do_execute_divide<SB, false>(a, b);

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
 *  Record FPSCR
 *
 *		Update FP exception bits
 **/

void powerpc_cpu::record_fpscr(int exceptions)
{
#if PPC_ENABLE_FPU_EXCEPTIONS
	// Reset non-sticky bits
	fpscr() &= ~(FPSCR_VX_field::mask() | FPSCR_FEX_field::mask());

	// Always update FX if any exception bit was set
	if (exceptions)
		fpscr() |= FPSCR_FX_field::mask() | exceptions;

	// Always update VX
	if (fpscr() & (FPSCR_VXSNAN_field::mask() | FPSCR_VXISI_field::mask() |
				   FPSCR_VXISI_field::mask() | FPSCR_VXIDI_field::mask() |
				   FPSCR_VXZDZ_field::mask() | FPSCR_VXIMZ_field::mask() |
				   FPSCR_VXVC_field::mask() | FPSCR_VXSOFT_field::mask() |
				   FPSCR_VXSQRT_field::mask() | FPSCR_VXCVI_field::mask()))
		fpscr() |= FPSCR_VX_field::mask();

	// Always update FEX
	if (((fpscr() & FPSCR_VX_field::mask()) && (fpscr() & FPSCR_VE_field::mask())) ||
		((fpscr() & FPSCR_OX_field::mask()) && (fpscr() & FPSCR_OE_field::mask())) ||
		((fpscr() & FPSCR_UX_field::mask()) && (fpscr() & FPSCR_UE_field::mask())) ||
		((fpscr() & FPSCR_ZX_field::mask()) && (fpscr() & FPSCR_ZE_field::mask())) ||
		((fpscr() & FPSCR_XX_field::mask()) && (fpscr() & FPSCR_XE_field::mask())))
		fpscr() |= FPSCR_FEX_field::mask();
#endif
}

/**
 *	Floating-point arithmetics
 *
 *		FP		Floating Point type
 *		OP		Operation to perform
 *		RD		Output register
 *		RA		Input operand
 *		RB		Input operand (optional)
 *		RC		Input operand (optional)
 *		Rc		Predicate to record CR1
 *		FPSCR	Predicate to compute FPSCR bits
 **/

template< class FP, class OP, class RD, class RA, class RB, class RC, class Rc, bool FPSCR >
void powerpc_cpu::execute_fp_arith(uint32 opcode)
{
	const double a = RA::get(this, opcode);
	const double b = RB::get(this, opcode);
	const double c = RC::get(this, opcode);

#if PPC_ENABLE_FPU_EXCEPTIONS
	int exceptions;
	if (FPSCR) {
		exceptions = op_apply<uint32, fp_exception_condition<OP>, RA, RB, RC>::apply(a, b, c);
		feclearexcept(FE_ALL_EXCEPT);
		febarrier();
	}
#endif

	FP d = op_apply<double, OP, RA, RB, RC>::apply(a, b, c);

	if (FPSCR) {

		// Update FPSCR exception bits
#if PPC_ENABLE_FPU_EXCEPTIONS
		febarrier();
		int raised = fetestexcept(FE_ALL_EXCEPT);
		if (raised & FE_INEXACT)
			exceptions |= FPSCR_XX_field::mask();
		if (raised & FE_DIVBYZERO)
			exceptions |= FPSCR_ZX_field::mask();
		if (raised & FE_UNDERFLOW)
			exceptions |= FPSCR_UX_field::mask();
		if (raised & FE_OVERFLOW)
			exceptions |= FPSCR_OX_field::mask();
		record_fpscr(exceptions);
#endif

		// FPSCR[FPRF] is set to the class and sign of the result
		if (!FPSCR_VE_field::test(fpscr()))
			fp_classify(d);
	}
	
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
	uint64 v;

	if (LD) {
		if (DB)
			v = vm_read_memory_8(ea);
		else
			v = fp_load_single_convert(vm_read_memory_4(ea));
		operand_fp_dw_RD::set(this, opcode, v);
	}
	else {
		v = operand_fp_dw_RS::get(this, opcode);
		if (DB)
			vm_write_memory_8(ea, v);
		else
			vm_write_memory_4(ea, fp_store_single_convert(v));
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
	uint32 reserve_data = vm_read_memory_4(ea);
	regs().reserve_valid = 1;
	regs().reserve_addr = ea;
#if KPX_MAX_CPUS != 1
	regs().reserve_data = reserve_data;
#endif
	operand_RD::set(this, opcode, reserve_data);
	increment_pc(4);
}

template< class RA >
void powerpc_cpu::execute_stwcx(uint32 opcode)
{
	const uint32 ea = RA::get(this, opcode) + operand_RB::get(this, opcode);
	cr().clear(0);
	if (regs().reserve_valid) {
		if (regs().reserve_addr == ea /* physical_addr(EA) */
#if KPX_MAX_CPUS != 1
			/* HACK: if another processor wrote to the reserved block,
			   nothing happens, i.e. we should operate as if reserve == 0 */
			&& regs().reserve_data == vm_read_memory_4(ea)
#endif
			) {
			vm_write_memory_4(ea, operand_RS::get(this, opcode));
			cr().set(0, standalone_CR_EQ_field::mask());
		}
		regs().reserve_valid = 0;
	}
	cr().set_so(0, xer().get_so());
	increment_pc(4);
}

/**
 *	Floating-point compare instruction
 *
 *		OC		Predicate for ordered compare
 **/

template< bool OC >
void powerpc_cpu::execute_fp_compare(uint32 opcode)
{
	const double a = operand_fp_RA::get(this, opcode);
	const double b = operand_fp_RB::get(this, opcode);
	const int crfd = crfD_field::extract(opcode);
	int c;

	if (is_NaN(a) || is_NaN(b))
		c = 1;
	else if (isless(a, b))
		c = 8;
	else if (isgreater(a, b))
		c = 4;
	else
		c = 2;

	FPSCR_FPCC_field::insert(fpscr(), c);
	cr().set(crfd, c);

	// Update FPSCR exception bits
#if PPC_ENABLE_FPU_EXCEPTIONS
	int exceptions = 0;
	if (is_SNaN(a) || is_SNaN(b)) {
		exceptions |= FPSCR_VXSNAN_field::mask();
		if (OC && !FPSCR_VE_field::test(fpscr()))
			exceptions |= FPSCR_VXVC_field::mask();
	}
	else if (OC && (is_QNaN(a) || is_QNaN(b)))
		exceptions |= FPSCR_VXVC_field::mask();
	record_fpscr(exceptions);
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

#if PPC_ENABLE_FPU_EXCEPTIONS
	int exceptions = 0;
	if (is_NaN(b)) {
		exceptions |= FPSCR_VXCVI_field::mask();
		if (is_SNaN(b))
			exceptions |= FPSCR_VXSNAN_field::mask();
	}
	if (isinf(b))
		exceptions |= FPSCR_VXCVI_field::mask();

	feclearexcept(FE_ALL_EXCEPT);
	febarrier();
#endif

	// Convert to integer word if operand fits bounds
	if (b >= -(double)0x80000000 && b <= (double)0x7fffffff) {
#if defined mathlib_lrint
		int old_round = fegetround();
		fesetround(ppc_to_native_rounding_mode(r));
		d.j = (int32)mathlib_lrint(b);
		fesetround(old_round);
#else
		switch (r) {
		case 0: d.j = (int32)op_frin::apply(b); break; // near
		case 1: d.j = (int32)op_friz::apply(b); break; // zero
		case 2: d.j = (int32)op_frip::apply(b); break; // +inf
		case 3: d.j = (int32)op_frim::apply(b); break; // -inf
		}
#endif
	}

	// NOTE: this catches infinity and NaN operands
	else if (b > 0)
		d.j = 0x7fffffff;
	else
		d.j = 0x80000000;

	// Update FPSCR exception bits
#if PPC_ENABLE_FPU_EXCEPTIONS
	febarrier();
	int raised = fetestexcept(FE_ALL_EXCEPT);
	if (raised & FE_UNDERFLOW)
		exceptions |= FPSCR_UX_field::mask();
	if (raised & FE_INEXACT)
		exceptions |= FPSCR_XX_field::mask();
	record_fpscr(exceptions);
#endif

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

template< class FP >
void powerpc_cpu::fp_classify(FP x)
{
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
}

template< class Rc >
void powerpc_cpu::execute_fp_round(uint32 opcode)
{
	const double b = operand_fp_RB::get(this, opcode);

#if PPC_ENABLE_FPU_EXCEPTIONS
	int exceptions =
		fp_invalid_operation_condition<double>::
		apply(FPSCR_VXSNAN_field::mask(), b);

	feclearexcept(FE_ALL_EXCEPT);
	febarrier();
#endif

	float d = (float)b;

	// Update FPSCR exception bits
#if PPC_ENABLE_FPU_EXCEPTIONS
	febarrier();
	int raised = fetestexcept(FE_ALL_EXCEPT);
	if (raised & FE_UNDERFLOW)
		exceptions |= FPSCR_UX_field::mask();
	if (raised & FE_OVERFLOW)
		exceptions |= FPSCR_OX_field::mask();
	if (raised & FE_INEXACT)
		exceptions |= FPSCR_XX_field::mask();
	record_fpscr(exceptions);
#endif

	// FPSCR[FPRF] is set to the class and sign of the result
	if (!FPSCR_VE_field::test(fpscr()))
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
#ifdef SHEEPSHAVER
	execute_illegal(opcode);
#else
	cr().set_so(0, execute_do_syscall && !execute_do_syscall(this));
#endif
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

void powerpc_cpu::execute_mcrfs(uint32 opcode)
{
	const int crfS = crfS_field::extract(opcode);
	const int crfD = crfD_field::extract(opcode);

	// The contents of FPSCR field crfS are copied to CR field crfD
	const uint32 m = 0xf << (28 - 4 * crfS);
	cr().set(crfD, (fpscr() & m) >> (28 - 4 * crfS));

	// All exception bits copied (except FEX and VX) are cleared in the FPSCR
	fpscr() &= ~(m & (FPSCR_FX_field::mask() | FPSCR_OX_field::mask() |
					  FPSCR_UX_field::mask() | FPSCR_ZX_field::mask() |
					  FPSCR_XX_field::mask() | FPSCR_VXSNAN_field::mask() |
					  FPSCR_VXISI_field::mask() | FPSCR_VXIDI_field::mask() |
					  FPSCR_VXZDZ_field::mask() | FPSCR_VXIMZ_field::mask() |
					  FPSCR_VXVC_field::mask() | FPSCR_VXSOFT_field::mask() |
					  FPSCR_VXSQRT_field::mask() | FPSCR_VXCVI_field::mask()));

	increment_pc(4);
}

void powerpc_cpu::execute_mcrxr(uint32 opcode)
{
	const int crfD = crfD_field::extract(opcode);
	const uint32 x = xer().get();
	cr().set(crfD, x >> 28);
	xer().set(x & 0x0fffffff);
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

	// The mtfsf instruction cannot alter FPSCR[FEX] nor FPSCR[VX] explicitly
	int exceptions = fsf & m;
	exceptions &= ~(FPSCR_FEX_field::mask() | FPSCR_VX_field::mask());

	// Move frB bits to FPSCR according to field mask
	fpscr() = (fpscr() & ~m) | exceptions;

	// Update FPSCR exception bits (don't implicitly update FX)
	record_fpscr(0);

	// Update native FP control word
	if (m & FPSCR_RN_field::mask())
		fesetround(ppc_to_native_rounding_mode(FPSCR_RN_field::extract(fpscr())));

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

	// The mtfsfi instruction cannot alter FPSCR[FEX] nor FPSCR[VX] explicitly
	int exceptions = RB::get(this, opcode) & m;
	exceptions &= ~(FPSCR_FEX_field::mask() | FPSCR_VX_field::mask());

	// Move immediate to FPSCR according to field crfD
	fpscr() = (fpscr() & ~m) | exceptions;

	// Update native FP control word
	if (m & FPSCR_RN_field::mask())
		fesetround(ppc_to_native_rounding_mode(FPSCR_RN_field::extract(fpscr())));

	// Update FPSCR exception bits (don't implicitly update FX)
	record_fpscr(0);

	// Set CR1 (FX, FEX, VX, VOX) if instruction has Rc set
	if (Rc::test(opcode))
		record_cr1();

	increment_pc(4);
}

template< class RB, class Rc >
void powerpc_cpu::execute_mtfsb(uint32 opcode)
{
	const bool set_bit = RB::get(this, opcode);

	// The mtfsb0 and mtfsb1 instructions cannot alter FPSCR[FEX] nor FPSCR[VX] explicitly
	uint32 m = 1 << (31 - crbD_field::extract(opcode));
	m &= ~(FPSCR_FEX_field::mask() | FPSCR_VX_field::mask());

	// Bit crbD of the FPSCR is set or clear
	fpscr() &= ~m;

	// Update FPSCR exception bits
	record_fpscr(set_bit ? m : 0);

	// Update native FP control word if FPSCR[RN] changed
	if (m & FPSCR_RN_field::mask())
		fesetround(ppc_to_native_rounding_mode(FPSCR_RN_field::extract(fpscr())));

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
	const uint32 spr = SPR::get(this, opcode);
	uint32 d;
	switch (spr) {
	case powerpc_registers::SPR_XER:	d = xer().get();break;
	case powerpc_registers::SPR_LR:		d = lr();		break;
	case powerpc_registers::SPR_CTR:	d = ctr();		break;
	case powerpc_registers::SPR_VRSAVE:	d = vrsave();	break;
#ifdef SHEEPSHAVER
	case powerpc_registers::SPR_SDR1:	d = 0xdead001f;	break;
	case powerpc_registers::SPR_PVR: {
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
	const uint32 spr = SPR::get(this, opcode);
	const uint32 s = operand_RS::get(this, opcode);

	switch (spr) {
	case powerpc_registers::SPR_XER:	xer().set(s);	break;
	case powerpc_registers::SPR_LR:		lr() = s;		break;
	case powerpc_registers::SPR_CTR:	ctr() = s;		break;
	case powerpc_registers::SPR_VRSAVE:	vrsave() = s;	break;
#ifndef SHEEPSHAVER
	default: execute_illegal(opcode);
#endif
	}

	increment_pc(4);
}

// Compute with 96 bit intermediate result: (a * b) / c
static uint64 muldiv64(uint64 a, uint32 b, uint32 c)
{
	union {
		uint64 ll;
		struct {
#ifdef WORDS_BIGENDIAN
			uint32 high, low;
#else
			uint32 low, high;
#endif
		} l;
	} u, res;

	u.ll = a;
	uint64 rl = (uint64)u.l.low * (uint64)b;
	uint64 rh = (uint64)u.l.high * (uint64)b;
	rh += (rl >> 32);
	res.l.high = rh / c;
	res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
	return res.ll;
}

static inline uint64 get_tb_ticks(void)
{
	uint64 ticks;
#ifdef SHEEPSHAVER
	const uint32 TBFreq = TimebaseSpeed;
	ticks = muldiv64(GetTicks_usec(), TBFreq, 1000000);
#else
	const uint32 TBFreq = 25 * 1000 * 1000; // 25 MHz
	ticks = muldiv64((uint64)clock(), TBFreq, CLOCKS_PER_SEC);
#endif
	return ticks;
}

template< class TBR >
void powerpc_cpu::execute_mftbr(uint32 opcode)
{
	uint32 tbr = TBR::get(this, opcode);
	uint32 d;
	switch (tbr) {
	case 268: d = (uint32)get_tb_ticks(); break;
	case 269: d = (get_tb_ticks() >> 32); break;
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
 *		Vector load/store instructions
 **/

template< bool SL >
void powerpc_cpu::execute_vector_load_for_shift(uint32 opcode)
{
	const uint32 ra = operand_RA_or_0::get(this, opcode);
	const uint32 rb = operand_RB::get(this, opcode);
	const uint32 ea = ra + rb;
	powerpc_vr & vD = vr(vD_field::extract(opcode));
	int j = SL ? (ea & 0xf) : (0x10 - (ea & 0xf));
	for (int i = 0; i < 16; i++)
		vD.b[ev_mixed::byte_element(i)] = j++;
	increment_pc(4);
}

template< class VD, class RA, class RB >
void powerpc_cpu::execute_vector_load(uint32 opcode)
{
	uint32 ea = RA::get(this, opcode) + RB::get(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	switch (VD::element_size) {
	case 1:
		VD::set_element(vD, (ea & 0x0f), vm_read_memory_1(ea));
		break;
	case 2:
		VD::set_element(vD, ((ea >> 1) & 0x07), vm_read_memory_2(ea & ~1));
		break;
	case 4:
		VD::set_element(vD, ((ea >> 2) & 0x03), vm_read_memory_4(ea & ~3));
		break;
	case 8:
		ea &= ~15;
		vD.w[0] = vm_read_memory_4(ea +  0);
		vD.w[1] = vm_read_memory_4(ea +  4);
		vD.w[2] = vm_read_memory_4(ea +  8);
		vD.w[3] = vm_read_memory_4(ea + 12);
		break;
	}
	increment_pc(4);
}

template< class VS, class RA, class RB >
void powerpc_cpu::execute_vector_store(uint32 opcode)
{
	uint32 ea = RA::get(this, opcode) + RB::get(this, opcode);
	typename VS::type & vS = VS::ref(this, opcode);
	switch (VS::element_size) {
	case 1:
		vm_write_memory_1(ea, VS::get_element(vS, (ea & 0x0f)));
		break;
	case 2:
		vm_write_memory_2(ea & ~1, VS::get_element(vS, ((ea >> 1) & 0x07)));
		break;
	case 4:
		vm_write_memory_4(ea & ~3, VS::get_element(vS, ((ea >> 2) & 0x03)));
		break;
	case 8:
		ea &= ~15;
		vm_write_memory_4(ea +  0, vS.w[0]);
		vm_write_memory_4(ea +  4, vS.w[1]);
		vm_write_memory_4(ea +  8, vS.w[2]);
		vm_write_memory_4(ea + 12, vS.w[3]);
		break;
	}
	increment_pc(4);
}

/**
 *	Vector arithmetic
 *
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 *		Rc		Predicate to record CR6
 *		C1		If recording CR6, do we check for '1' bits in vD?
 **/

template< class OP, class VD, class VA, class VB, class VC, class Rc, int C1 >
void powerpc_cpu::execute_vector_arith(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VC::type const & vC = VC::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	for (int i = 0; i < n_elements; i++) {
		const typename VA::element_type a = VA::get_element(vA, i);
		const typename VB::element_type b = VB::get_element(vB, i);
		const typename VC::element_type c = VC::get_element(vC, i);
		typename VD::element_type d = op_apply<typename VD::element_type, OP, VA, VB, VC>::apply(a, b, c);
		if (VD::saturate(d))
			vscr().set_sat(1);
		VD::set_element(vD, i, d);
	}

	// Propagate all conditions to CR6
	if (Rc::test(opcode))
		record_cr6(vD, C1);

	increment_pc(4);
}

/**
 *	Vector mixed arithmetic
 *
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 **/

template< class OP, class VD, class VA, class VB, class VC >
void powerpc_cpu::execute_vector_arith_mixed(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VC::type const & vC = VC::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;
	const int n_sub_elements = 4 / VA::element_size;

	for (int i = 0; i < n_elements; i++) {
		const typename VC::element_type c = VC::get_element(vC, i);
		typename VD::element_type d = c;
		for (int j = 0; j < n_sub_elements; j++) {
			const typename VA::element_type a = VA::get_element(vA, i * n_sub_elements + j);
			const typename VB::element_type b = VB::get_element(vB, i * n_sub_elements + j);
			d += op_apply<typename VD::element_type, OP, VA, VB, null_vector_operand>::apply(a, b, c);
		}
		if (VD::saturate(d))
			vscr().set_sat(1);
		VD::set_element(vD, i, d);
	}

	increment_pc(4);
}

/**
 *	Vector odd/even arithmetic
 *
 *		ODD		Flag: are we computing every odd element?
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 **/

template< int ODD, class OP, class VD, class VA, class VB, class VC >
void powerpc_cpu::execute_vector_arith_odd(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VC::type const & vC = VC::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	for (int i = 0; i < n_elements; i++) {
		const typename VA::element_type a = VA::get_element(vA, (i * 2) + ODD);
		const typename VB::element_type b = VB::get_element(vB, (i * 2) + ODD);
		const typename VC::element_type c = VC::get_element(vC, (i * 2) + ODD);
		typename VD::element_type d = op_apply<typename VD::element_type, OP, VA, VB, VC>::apply(a, b, c);
		if (VD::saturate(d))
			vscr().set_sat(1);
		VD::set_element(vD, i, d);
	}

	increment_pc(4);
}

/**
 *	Vector merge instructions
 *
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 *		LO		Flag: use lower part of element
 **/

template< class VD, class VA, class VB, int LO >
void powerpc_cpu::execute_vector_merge(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	for (int i = 0; i < n_elements; i += 2) {
		VD::set_element(vD, i    , VA::get_element(vA, (i / 2) + LO * (n_elements / 2)));
		VD::set_element(vD, i + 1, VB::get_element(vB, (i / 2) + LO * (n_elements / 2)));
	}

	increment_pc(4);
}

/**
 *	Vector pack/unpack instructions
 *
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 *		LO		Flag: use lower part of element
 **/

template< class VD, class VA, class VB >
void powerpc_cpu::execute_vector_pack(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;
	const int n_pivot = n_elements / 2;

	for (int i = 0; i < n_elements; i++) {
		typename VD::element_type d;
		if (i < n_pivot)
			d = VA::get_element(vA, i);
		else
			d = VB::get_element(vB, i - n_pivot);
		if (VD::saturate(d))
			vscr().set_sat(1);
		VD::set_element(vD, i, d);
	}

	increment_pc(4);
}

template< int LO, class VD, class VA >
void powerpc_cpu::execute_vector_unpack(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	for (int i = 0; i < n_elements; i++)
		VD::set_element(vD, i, VA::get_element(vA, i + LO * n_elements));

	increment_pc(4);
}

void powerpc_cpu::execute_vector_pack_pixel(uint32 opcode)
{
	powerpc_vr const & vA = vr(vA_field::extract(opcode));
	powerpc_vr const & vB = vr(vB_field::extract(opcode));
	powerpc_vr & vD = vr(vD_field::extract(opcode));

	for (int i = 0; i < 4; i++) {
		const uint32 a = vA.w[i];
		vD.h[ev_mixed::half_element(i)] = ((a >> 9) & 0xfc00) | ((a >> 6) & 0x03e0) | ((a >> 3) & 0x001f);
		const uint32 b = vB.w[i];
		vD.h[ev_mixed::half_element(i + 4)] = ((b >> 9) & 0xfc00) | ((b >> 6) & 0x03e0) | ((b >> 3) & 0x001f);
	}

	increment_pc(4);
}

template< int LO >
void powerpc_cpu::execute_vector_unpack_pixel(uint32 opcode)
{
	powerpc_vr const & vB = vr(vB_field::extract(opcode));
	powerpc_vr & vD = vr(vD_field::extract(opcode));

	for (int i = 0; i < 4; i++) {
		const uint32 h = vB.h[ev_mixed::half_element(i + LO * 4)];
		vD.w[i] = (((h & 0x8000) ? 0xff000000 : 0) |
				   ((h & 0x7c00) << 6) |
				   ((h & 0x03e0) << 3) |
				   (h & 0x001f));
	}

	increment_pc(4);
}

/**
 *	Vector shift instructions
 *
 *		SD		Shift direction: left (-1), right (+1)
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		VC		Input operand vector (optional: operand_NONE)
 *		SH		Shift count operand
 **/

template< int SD >
void powerpc_cpu::execute_vector_shift(uint32 opcode)
{
	powerpc_vr const & vA = vr(vA_field::extract(opcode));
	powerpc_vr const & vB = vr(vB_field::extract(opcode));
	powerpc_vr & vD = vr(vD_field::extract(opcode));

	// The contents of the low-order three bits of all byte
	// elements in vB must be identical to vB[125-127]; otherwise
	// the value placed into vD is undefined.
	const int sh = vB.b[ev_mixed::byte_element(15)] & 7;
	if (sh == 0) {
		for (int i = 0; i < 4; i++)
			vD.w[i] = vA.w[i];
	}
	else {
		uint32 prev_bits = 0;
		if (SD < 0) {
			for (int i = 3; i >= 0; i--) {
				uint32 next_bits = vA.w[i] >> (32 - sh);
				vD.w[i] = ((vA.w[i] << sh) | prev_bits);
				prev_bits = next_bits;
			}
		}
		else if (SD > 0) {
			for (int i = 0; i < 4; i++) {
				uint32 next_bits = vA.w[i] << (32 - sh);
				vD.w[i] = ((vA.w[i] >> sh) | prev_bits);
				prev_bits = next_bits;
			}
		}
	}

	increment_pc(4);
}

template< int SD, class VD, class VA, class VB, class SH >
void powerpc_cpu::execute_vector_shift_octet(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	const int sh = SH::get(this, opcode);
	if (SD < 0) {
		for (int i = 0; i < 16; i++) {
			if (i + sh < 16)
				VD::set_element(vD, i, VA::get_element(vA, i + sh));
			else
				VD::set_element(vD, i, VB::get_element(vB, i - (16 - sh)));
		}
	}
	else if (SD > 0) {
		for (int i = 0; i < 16; i++) {
			if (i < sh)
				VD::set_element(vD, i, VB::get_element(vB, 16 - (i - sh)));
			else
				VD::set_element(vD, i, VA::get_element(vA, i - sh));
		}
	}

	increment_pc(4);
}

/**
 *	Vector splat instructions
 *
 *		OP		Operation to perform on element
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 *		IM		Immediate value to replicate
 **/

template< class OP, class VD, class VB, bool IM >
void powerpc_cpu::execute_vector_splat(uint32 opcode)
{
	typename VD::type & vD = VD::ref(this, opcode);
	const int n_elements = 16 / VD::element_size;

	uint32 value;
	if (IM)
		value = OP::apply(vUIMM_field::extract(opcode));
	else {
		typename VB::type const & vB = VB::const_ref(this, opcode);
		const int n = vUIMM_field::extract(opcode) & (n_elements - 1);
		value = OP::apply(VB::get_element(vB, n));
	}

	for (int i = 0; i < n_elements; i++)
		VD::set_element(vD, i, value);

	increment_pc(4);
}

/**
 *	Vector sum instructions
 *
 *		SZ		Size of destination vector elements
 *		VD		Output operand vector
 *		VA		Input operand vector
 *		VB		Input operand vector (optional: operand_NONE)
 **/

template< int SZ, class VD, class VA, class VB >
void powerpc_cpu::execute_vector_sum(uint32 opcode)
{
	typename VA::type const & vA = VA::const_ref(this, opcode);
	typename VB::type const & vB = VB::const_ref(this, opcode);
	typename VD::type & vD = VD::ref(this, opcode);
	typename VD::element_type d;
	
	switch (SZ) {
	case 1: // vsum
		d = VB::get_element(vB, 3);
		for (int j = 0; j < 4; j++)
			d += VA::get_element(vA, j);
		if (VD::saturate(d))
			vscr().set_sat(1);
		VD::set_element(vD, 0, 0);
		VD::set_element(vD, 1, 0);
		VD::set_element(vD, 2, 0);
		VD::set_element(vD, 3, d);
		break;

	case 2: // vsum2
		for (int i = 0; i < 4; i += 2) {
			d = VB::get_element(vB, i + 1);
			for (int j = 0; j < 2; j++)
				d += VA::get_element(vA, i + j);
			if (VD::saturate(d))
				vscr().set_sat(1);
			VD::set_element(vD, i + 0, 0);
			VD::set_element(vD, i + 1, d);
		}
		break;

	case 4: // vsum4
		for (int i = 0; i < 4; i += 1) {
			d = VB::get_element(vB, i);
			const int n_elements = 4 / VA::element_size;
			for (int j = 0; j < n_elements; j++)
				d += VA::get_element(vA, i * n_elements + j);
			if (VD::saturate(d))
				vscr().set_sat(1);
			VD::set_element(vD, i, d);
		}
		break;
	}

	increment_pc(4);
}

/**
 *		Misc vector instructions
 **/

void powerpc_cpu::execute_vector_permute(uint32 opcode)
{
	powerpc_vr const & vA = vr(vA_field::extract(opcode));
	powerpc_vr const & vB = vr(vB_field::extract(opcode));
	powerpc_vr const & vC = vr(vC_field::extract(opcode));
	powerpc_vr & vD = vr(vD_field::extract(opcode));

	for (int i = 0; i < 16; i++) {
		const int ei = ev_mixed::byte_element(i);
		const int n  = vC.b[ei] & 0x1f;
		const int en = ev_mixed::byte_element(n & 0xf);
		vD.b[ei] = (n & 0x10) ? vB.b[en] : vA.b[en];
	}

	increment_pc(4);
}

void powerpc_cpu::execute_mfvscr(uint32 opcode)
{
	const int vD = vD_field::extract(opcode);
	vr(vD).w[0] = 0;
	vr(vD).w[1] = 0;
	vr(vD).w[2] = 0;
	vr(vD).w[3] = vscr().get();
	increment_pc(4);
}

void powerpc_cpu::execute_mtvscr(uint32 opcode)
{
	const int vB = vB_field::extract(opcode);
	vscr().set(vr(vB).w[3]);
	increment_pc(4);
}

/**
 *		Explicit template instantiations
 **/

#include "ppc-execute-impl.cpp"
