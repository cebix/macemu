/*
 *  ppc-dyngen-ops.hpp - PowerPC synthetic instructions
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

#include "sysdeps.h"
#include "cpu/vm.hpp"
#include "cpu/jit/dyngen-exec.h"
#define NO_DEFINE_ALIAS 1
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-registers.hpp"
#include "cpu/ppc/ppc-operations.hpp"

// We need at least 4 general purpose registers
#ifdef REG_CPU
register struct powerpc_cpu *CPU asm(REG_CPU);
#else
#define CPU ((powerpc_cpu *)CPUPARAM)
#endif
register uint32 A0 asm(REG_A0);
register uint32 T0 asm(REG_T0);
register uint32 T1 asm(REG_T1);
#ifdef REG_T2
register uint32 CC_LHS asm(REG_T2);
#else
#define CC_LHS powerpc_dyngen_helper::cc_lhs()
#endif
#ifdef REG_T3
register uint32 CC_RHS asm(REG_T3);
#else
#define CC_RHS powerpc_dyngen_helper::cc_rhs()
#endif

// Semantic action templates
#define DYNGEN_OPS
#include "ppc-execute.hpp"


/**
 *		Helper class to access protected CPU context
 **/

struct powerpc_dyngen_helper {
	static inline uint32 get_pc()				{ return CPU->pc(); }
	static inline void set_pc(uint32 value)		{ CPU->pc() = value; }
	static inline void inc_pc(int32 offset)		{ CPU->pc() += offset; }
	static inline uint32 get_lr()				{ return CPU->lr(); }
	static inline void set_lr(uint32 value)		{ CPU->lr() = value; }
	static inline uint32 get_ctr()				{ return CPU->ctr(); }
	static inline void set_ctr(uint32 value)	{ CPU->ctr() = value; }
	static inline uint32 get_cr()				{ return CPU->cr().get(); }
	static inline void set_cr(uint32 value)		{ CPU->cr().set(value); }
	static inline uint32 get_xer()				{ return CPU->xer().get(); }
	static inline void set_xer(uint32 value)	{ CPU->xer().set(value); }
	static inline void record(int crf, int32 v)	{ CPU->record_cr(crf, v); }
	static inline powerpc_cr_register & cr()	{ return CPU->cr(); }
	static inline powerpc_xer_register & xer()	{ return CPU->xer(); }
	static inline uint32 & cc_lhs() { return CPU->codegen_ptr()->rc_cache.cc_lhs; }
	static inline uint32 & cc_rhs() { return CPU->codegen_ptr()->rc_cache.cc_rhs; }
};


/**
 *		Load/store general purpose registers
 **/

#define DEFINE_OP(REG, N)						\
void OPPROTO op_load_##REG##_GPR##N(void)		\
{												\
	REG = CPU->gpr(N);							\
}												\
void OPPROTO op_store_##REG##_GPR##N(void)		\
{												\
	CPU->gpr(N) = REG;							\
}
#define DEFINE_REG(N)							\
DEFINE_OP(A0,N);								\
DEFINE_OP(T0,N);								\
DEFINE_OP(T1,N);

DEFINE_REG(0);
DEFINE_REG(1);
DEFINE_REG(2);
DEFINE_REG(3);
DEFINE_REG(4);
DEFINE_REG(5);
DEFINE_REG(6);
DEFINE_REG(7);
DEFINE_REG(8);
DEFINE_REG(9);
DEFINE_REG(10);
DEFINE_REG(11);
DEFINE_REG(12);
DEFINE_REG(13);
DEFINE_REG(14);
DEFINE_REG(15);
DEFINE_REG(16);
DEFINE_REG(17);
DEFINE_REG(18);
DEFINE_REG(19);
DEFINE_REG(20);
DEFINE_REG(21);
DEFINE_REG(22);
DEFINE_REG(23);
DEFINE_REG(24);
DEFINE_REG(25);
DEFINE_REG(26);
DEFINE_REG(27);
DEFINE_REG(28);
DEFINE_REG(29);
DEFINE_REG(30);
DEFINE_REG(31);

#undef DEFINE_REG
#undef DEFINE_OP


/**
 *		Condition Registers
 **/

void OPPROTO op_load_T0_CR(void)
{
	T0 = powerpc_dyngen_helper::get_cr();
}

void OPPROTO op_store_T0_CR(void)
{
	powerpc_dyngen_helper::set_cr(T0);
}

void OPPROTO op_load_T0_XER(void)
{
	T0 = powerpc_dyngen_helper::get_xer();
}

void OPPROTO op_store_T0_XER(void)
{
	powerpc_dyngen_helper::set_xer(T0);
}

#define DEFINE_OP(REG, N)											\
void OPPROTO op_load_##REG##_crb##N(void)							\
{																	\
	REG = bit_field<N,N>::extract(powerpc_dyngen_helper::get_cr());	\
}																	\
void OPPROTO op_store_##REG##_crb##N(void)							\
{																	\
	uint32 cr = powerpc_dyngen_helper::get_cr();					\
	bit_field<N,N>::insert(cr, REG);								\
	powerpc_dyngen_helper::set_cr(cr);								\
}
#define DEFINE_REG(N)							\
DEFINE_OP(T0, N);								\
DEFINE_OP(T1, N);

DEFINE_REG(0);
DEFINE_REG(1);
DEFINE_REG(2);
DEFINE_REG(3);
DEFINE_REG(4);
DEFINE_REG(5);
DEFINE_REG(6);
DEFINE_REG(7);
DEFINE_REG(8);
DEFINE_REG(9);
DEFINE_REG(10);
DEFINE_REG(11);
DEFINE_REG(12);
DEFINE_REG(13);
DEFINE_REG(14);
DEFINE_REG(15);
DEFINE_REG(16);
DEFINE_REG(17);
DEFINE_REG(18);
DEFINE_REG(19);
DEFINE_REG(20);
DEFINE_REG(21);
DEFINE_REG(22);
DEFINE_REG(23);
DEFINE_REG(24);
DEFINE_REG(25);
DEFINE_REG(26);
DEFINE_REG(27);
DEFINE_REG(28);
DEFINE_REG(29);
DEFINE_REG(30);
DEFINE_REG(31);

#undef DEFINE_REG
#undef DEFINE_OP

#define DEFINE_OP(CRF, REG)						\
void OPPROTO op_load_##REG##_cr##CRF(void)		\
{												\
	T0 = powerpc_dyngen_helper::cr().get(CRF);	\
}												\
void OPPROTO op_store_##REG##_cr##CRF(void)		\
{												\
	powerpc_dyngen_helper::cr().set(CRF, REG);	\
}

DEFINE_OP(0, T0);
DEFINE_OP(1, T0);
DEFINE_OP(2, T0);
DEFINE_OP(3, T0);
DEFINE_OP(4, T0);
DEFINE_OP(5, T0);
DEFINE_OP(6, T0);
DEFINE_OP(7, T0);

#undef DEFINE_OP

#define DEFINE_OP(CRF)								\
void OPPROTO op_commit_so_cache_cr##CRF(void)		\
{													\
	int so = powerpc_dyngen_helper::xer().get_so();	\
	powerpc_dyngen_helper::cr().set_so(CRF, so);	\
}

DEFINE_OP(0);
DEFINE_OP(1);
DEFINE_OP(2);
DEFINE_OP(3);
DEFINE_OP(4);
DEFINE_OP(5);
DEFINE_OP(6);
DEFINE_OP(7);

#undef DEFINE_OP

#define DEFINE_OP_1(NAME,TYPE,CRF)					\
void OPPROTO op_##NAME##_rc_cache_cr##CRF(void)		\
{													\
	uint32 cr = powerpc_dyngen_helper::get_cr();	\
	cr &= ~(CR_LT_field<CRF>::mask() |				\
			CR_GT_field<CRF>::mask() |				\
			CR_EQ_field<CRF>::mask());				\
													\
	if ((TYPE)CC_LHS < (TYPE)CC_RHS)				\
		cr |= CR_LT_field<CRF>::mask();				\
	else if ((TYPE)CC_LHS > (TYPE)CC_RHS)			\
		cr |= CR_GT_field<CRF>::mask();				\
	else											\
		cr |= CR_EQ_field<CRF>::mask();				\
													\
	powerpc_dyngen_helper::set_cr(cr);				\
	dyngen_barrier();								\
}
#define DEFINE_OP(CRF)							\
DEFINE_OP_1(commit,int32,CRF);					\
DEFINE_OP_1(commit_logical,uint32,CRF)

DEFINE_OP(0);
DEFINE_OP(1);
DEFINE_OP(2);
DEFINE_OP(3);
DEFINE_OP(4);
DEFINE_OP(5);
DEFINE_OP(6);
DEFINE_OP(7);

#undef DEFINE_OP
#undef DEFINE_OP_1


/**
 *		Special purpose registers
 **/

void OPPROTO op_load_T0_PC(void)
{
	T0 = powerpc_dyngen_helper::get_pc();
}

void OPPROTO op_store_T0_PC(void)
{
	powerpc_dyngen_helper::set_pc(T0);
}

void OPPROTO op_set_PC_im(void)
{
	powerpc_dyngen_helper::set_pc(PARAM1);
}

void OPPROTO op_set_PC_A0(void)
{
	powerpc_dyngen_helper::set_pc(A0);
}

void OPPROTO op_inc_PC(void)
{
	powerpc_dyngen_helper::inc_pc(PARAM1);
}

void OPPROTO op_load_T0_LR(void)
{
	T0 = powerpc_dyngen_helper::get_lr();
}

void OPPROTO op_store_T0_LR(void)
{
	powerpc_dyngen_helper::set_lr(T0);
}

void OPPROTO op_load_T0_CTR(void)
{
	T0 = powerpc_dyngen_helper::get_ctr();
}

void OPPROTO op_store_T0_CTR(void)
{
	powerpc_dyngen_helper::set_ctr(T0);
}

void OPPROTO op_store_T1_CTR(void)
{
	powerpc_dyngen_helper::set_ctr(T1);
}

void OPPROTO op_load_T1_PC(void)
{
	T1 = powerpc_dyngen_helper::get_pc();
}

void OPPROTO op_store_im_LR(void)
{
	powerpc_dyngen_helper::set_lr(PARAM1);
}

void OPPROTO op_load_A0_CTR(void)
{
	A0 = powerpc_dyngen_helper::get_ctr() & -4;
}

void OPPROTO op_load_A0_LR(void)
{
	A0 = powerpc_dyngen_helper::get_lr() & -4;
}


/**
 *		Branch instructions
 **/

void OPPROTO op_decrement_ctr_T0(void)
{
	T0 = powerpc_dyngen_helper::get_ctr() - 1;
	powerpc_dyngen_helper::set_ctr(T0);
}

void OPPROTO op_branch_A0_if_T0(void)
{
	if (T0)
		powerpc_dyngen_helper::set_pc(A0);
	else
		powerpc_dyngen_helper::set_pc(PARAM1);
	dyngen_barrier();
}

void OPPROTO op_branch_A0_if_not_T0(void)
{
	if (!T0)
		powerpc_dyngen_helper::set_pc(A0);
	else
		powerpc_dyngen_helper::set_pc(PARAM1);
	dyngen_barrier();
}

template< class branch_cond, class ctr_cond >
static inline void do_execute_branch(uint32 tpc, uint32 npc)
{
	if (branch_cond::test() && ctr_cond::test())
		powerpc_dyngen_helper::set_pc(tpc);
	else
		powerpc_dyngen_helper::set_pc(npc);
	dyngen_barrier();
}

template< int crb >
struct CR_comparator {
	static inline bool test() {
		return (T0 & crb);
	}
};

template< bool br_true, class comparator >
struct bool_condition {
	static inline bool test() {
		return comparator::test();
	}
};

template< class comparator >
struct bool_condition< false, comparator > {
	static inline bool test() {
		return !comparator::test();
	}
};

typedef bool_condition< true, CR_comparator<8> > blt_condition;
typedef bool_condition<false, CR_comparator<8> > bnlt_condition;
typedef bool_condition< true, CR_comparator<4> > bgt_condition;
typedef bool_condition<false, CR_comparator<4> > bngt_condition;
typedef bool_condition< true, CR_comparator<2> > beq_condition;
typedef bool_condition<false, CR_comparator<2> > bneq_condition;
typedef bool_condition< true, CR_comparator<1> > bso_condition;
typedef bool_condition<false, CR_comparator<1> > bnso_condition;

struct ctr_0x_condition {
	static inline bool test() {
		return true;
	}
};

struct ctr_10_condition {
	static inline bool test() {
		uint32 ctr = powerpc_dyngen_helper::get_ctr() - 1;
		powerpc_dyngen_helper::set_ctr(ctr);
		return ctr != 0;
	}
};

struct ctr_11_condition {
	static inline bool test() {
		uint32 ctr = powerpc_dyngen_helper::get_ctr() - 1;
		powerpc_dyngen_helper::set_ctr(ctr);
		return ctr == 0;
	}
};

#define DEFINE_OP_CTR(COND,CTR)												\
void OPPROTO op_##COND##_##CTR(void)										\
{																			\
	do_execute_branch<COND##_condition, ctr_##CTR##_condition>(A0, PARAM1);	\
}
#define DEFINE_OP(COND)							\
DEFINE_OP_CTR(COND,0x);							\
DEFINE_OP_CTR(COND,10);							\
DEFINE_OP_CTR(COND,11)

DEFINE_OP(blt);
DEFINE_OP(bgt);
DEFINE_OP(beq);
DEFINE_OP(bso);
DEFINE_OP(bnlt);
DEFINE_OP(bngt);
DEFINE_OP(bneq);
DEFINE_OP(bnso);

#undef DEFINE_OP
#undef DEFINE_OP_CTR


/**
 *		Compare & Record instructions
 **/

void OPPROTO op_record_nego_T0(void)
{
	powerpc_dyngen_helper::xer().set_ov(T0 == 0x80000000);
	dyngen_barrier();
}

void OPPROTO op_record_cr0_T0(void)
{
	uint32 cr = powerpc_dyngen_helper::get_cr();
	cr &= ~(CR_LT_field<0>::mask() |
			CR_GT_field<0>::mask() |
			CR_EQ_field<0>::mask() |
			CR_SO_field<0>::mask());

#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 v;
	asm volatile ("cmpwi 0,%1,0 ; mfcr %0" : "=r" (v) : "r" (T0) : "cr0");
	cr |= (v & (0xe0000000));
	cr |= (powerpc_dyngen_helper::xer().get_so() << (31 - 3));
	goto end;
#endif
#endif

	if (powerpc_dyngen_helper::xer().get_so())
		cr |= CR_SO_field<0>::mask();
	if ((int32)T0 < 0)
		cr |= CR_LT_field<0>::mask();
	else if ((int32)T0 > 0)
		cr |= CR_GT_field<0>::mask();
	else
		cr |= CR_EQ_field<0>::mask();

  end:
	powerpc_dyngen_helper::set_cr(cr);
	dyngen_barrier();
}

#define im PARAM1

#define DEFINE_OP(LHS, RHS)						\
void OPPROTO op_compare_##LHS##_##RHS(void)		\
{												\
	CC_LHS = LHS;								\
	CC_RHS = RHS;								\
}
DEFINE_OP(T0,T1);
DEFINE_OP(T0,im);
DEFINE_OP(T0,0);
#undef DEFINE_OP

#if DYNGEN_ASM_OPTS && defined(__powerpc__)

#define DEFINE_OP(NAME, COMP, LHS, RHST, RHS)												\
void OPPROTO op_##NAME##_##LHS##_##RHS(void)												\
{																							\
	uint32 cr = powerpc_dyngen_helper::xer().get_so();										\
	uint32 v;																				\
	asm volatile (COMP " 7,%1,%2 ; mfcr %0" : "=r" (v) : "r" (LHS), RHST (RHS) : "cr7");	\
	T0 = cr | (v & 0xe);																	\
}

DEFINE_OP(do_compare,"cmpw",T0,"r",T1);
DEFINE_OP(do_compare,"cmpw",T0,"r",im);
DEFINE_OP(do_compare,"cmpwi",T0,"i",0);
DEFINE_OP(do_compare_logical,"cmplw",T0,"r",T1);
DEFINE_OP(do_compare_logical,"cmplw",T0,"r",im);
DEFINE_OP(do_compare_logical,"cmplwi",T0,"i",0);

#else

#define DEFINE_OP(NAME, TYPE, LHS, RHS)					\
void OPPROTO op_##NAME##_##LHS##_##RHS(void)			\
{														\
	uint32 cr = powerpc_dyngen_helper::xer().get_so();	\
	if ((TYPE)LHS < (TYPE)RHS)							\
		cr |= standalone_CR_LT_field::mask();			\
	else if ((TYPE)LHS > (TYPE)RHS)						\
		cr |= standalone_CR_GT_field::mask();			\
	else												\
		cr |= standalone_CR_EQ_field::mask();			\
	T0 = cr;											\
	dyngen_barrier();									\
}

DEFINE_OP(do_compare,int32,T0,T1);
DEFINE_OP(do_compare,int32,T0,im);
DEFINE_OP(do_compare,int32,T0,0);
DEFINE_OP(do_compare_logical,uint32,T0,T1);
DEFINE_OP(do_compare_logical,uint32,T0,im);
DEFINE_OP(do_compare_logical,uint32,T0,0);

#endif

#undef im
#undef DEFINE_OP


/**
 *		Divide instructions
 **/

#if DYNGEN_ASM_OPTS && defined(__powerpc__)
#define get_ov() ({ uint32 xer; asm volatile ("mfxer %0" : "=r" (xer)); XER_OV_field::extract(xer); })
#endif

void OPPROTO op_divw_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	asm volatile ("divw %0,%0,%1" : "=r" (T0) : "r" (T1));
	return;
#endif
#endif
	T0 = do_execute_divide<true, false>(T0, T1);
}

void OPPROTO op_divwo_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	asm volatile ("divwo %0,%0,%1" : "=r" (T0) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ov(get_ov());
	return;
#endif
#endif
	T0 = do_execute_divide<true, true>(T0, T1);
}

void OPPROTO op_divwu_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	asm volatile ("divwu %0,%0,%1" : "=r" (T0) : "r" (T1));
	return;
#endif
#endif
	T0 = do_execute_divide<false, false>(T0, T1);
}

void OPPROTO op_divwuo_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	asm volatile ("divwuo %0,%0,%1" : "=r" (T0) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ov(get_ov());
	return;
#endif
#endif
	T0 = do_execute_divide<false, true>(T0, T1);
}


/**
 *		Multiply instructions
 **/

void OPPROTO op_mulhw_T0_T1(void)
{
	T0 = (((int64)(int32)T0) * ((int64)(int32)T1)) >> 32;
}

void OPPROTO op_mulhwu_T0_T1(void)
{
	T0 = (((uint64)T0) * ((uint64)T1)) >> 32;
}

void OPPROTO op_mulli_T0_im(void)
{
	T0 = (int32)T0 * (int32)PARAM1;
}

void OPPROTO op_mullwo_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	asm volatile ("mullwo %0,%0,%1" : "=r" (T0) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ov(get_ov());
	return;
#endif
#endif
	int64 RD = (int64)(int32)T0 * (int64)(int32)T1;
	powerpc_dyngen_helper::xer().set_ov((int32)RD != RD);
	T0 = RD;
	dyngen_barrier();
}


/**
 *		Shift/Rotate instructions
 **/

void OPPROTO op_slw_T0_T1(void)
{
	T1 &= 0x3f;
	T0 = (T1 & 0x20) ? 0 : (T0 << T1);
	dyngen_barrier();
}

void OPPROTO op_srw_T0_T1(void)
{
	T1 &= 0x3f;
	T0 = (T1 & 0x20) ? 0 : (T0 >> T1);
	dyngen_barrier();
}

void OPPROTO op_sraw_T0_T1(void)
{
	const uint32 n = T1 & 0x3f;
	const uint32 RD = ((int32)T0) >> n;
	const bool ca = (((int32)T0) < 0) && (T0 & ~(0xffffffff << n));
	powerpc_dyngen_helper::xer().set_ca(ca);
	T0 = RD;
	dyngen_barrier();
}

void OPPROTO op_sraw_T0_im(void)
{
	const uint32 n = PARAM1;
	const uint32 RD = ((int32)T0) >> n;
	const bool ca = (((int32)T0) < 0) && (T0 & ~(0xffffffff << n));
	powerpc_dyngen_helper::xer().set_ca(ca);
	T0 = RD;
	dyngen_barrier();
}

void OPPROTO op_rlwimi_T0_T1(void)
{
	T0 = op_ppc_rlwimi::apply(T1, PARAM1, PARAM2, T0);
}

void OPPROTO op_rlwinm_T0_T1(void)
{
	T0 = op_rotl::apply(T0, PARAM1) & PARAM2;
}

void OPPROTO op_rlwnm_T0_T1(void)
{
	T0 = op_rotl::apply(T0, T1) & PARAM1;
}

void OPPROTO op_cntlzw_32_T0(void)
{
	uint32 n;
	uint32 m = 0x80000000;
	for (n = 0; n < 32; n++, m >>= 1)
		if (T0 & m)
			break;
	T0 = n;
	dyngen_barrier();
}


/**
 *		Addition/Subtraction
 **/

void OPPROTO op_addo_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer;
	asm volatile ("addo %0,%0,%2 ; mfxer %1" : "=r" (T0), "=r" (xer) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ov(XER_OV_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<false, false, true>(T0, T1);
}

void OPPROTO op_addc_T0_im(void)
{
	T0 = do_execute_addition<false, true, false>(T0, PARAM1);
}

void OPPROTO op_addc_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer;
	asm volatile ("addc %0,%0,%2 ; mfxer %1" : "=r" (T0), "=r" (xer) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<false, true, false>(T0, T1);
}

void OPPROTO op_addco_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer;
	asm volatile ("addco %0,%0,%2 ; mfxer %1" : "=r" (T0), "=r" (xer) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	powerpc_dyngen_helper::xer().set_ov(XER_OV_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<false, true, true>(T0, T1);
}

void OPPROTO op_adde_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("adde %0,%0,%2 ; mfxer %1" : "=r" (T0), "=r" (xer) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, false>(T0, T1);
}

void OPPROTO op_addeo_T0_T1(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("addeo %0,%0,%2 ; mfxer %1" : "=r" (T0), "=r" (xer) : "r" (T1));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	powerpc_dyngen_helper::xer().set_ov(XER_OV_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, true>(T0, T1);
}

void OPPROTO op_addme_T0(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("addme %0,%0 ; mfxer %1" : "=r" (T0), "=r" (xer));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, false>(T0, 0xffffffff);
}

void OPPROTO op_addmeo_T0(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("addmeo %0,%0 ; mfxer %1" : "=r" (T0), "=r" (xer));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	powerpc_dyngen_helper::xer().set_ov(XER_OV_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, true>(T0, 0xffffffff);
}

void OPPROTO op_addze_T0(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("addze %0,%0 ; mfxer %1" : "=r" (T0), "=r" (xer));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, false>(T0, 0);
}

void OPPROTO op_addzeo_T0(void)
{
#if DYNGEN_ASM_OPTS
#if defined(__powerpc__)
	uint32 xer, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("li 0,-1 ; addc 0,%0,0" : : "r" (ca) : "r0");
	asm volatile ("addzeo %0,%0 ; mfxer %1" : "=r" (T0), "=r" (xer));
	powerpc_dyngen_helper::xer().set_ca(XER_CA_field::extract(xer));
	powerpc_dyngen_helper::xer().set_ov(XER_OV_field::extract(xer));
	return;
#endif
#endif
	T0 = do_execute_addition<true, false, true>(T0, 0);
}

void OPPROTO op_subf_T0_T1(void)
{
	T0 = T1 - T0;
}

void OPPROTO op_subfo_T0_T1(void)
{
	T0 = do_execute_subtract<false, true>(T0, T1);
}

void OPPROTO op_subfc_T0_im(void)
{
	T0 = do_execute_subtract<true, false>(T0, PARAM1);
}

void OPPROTO op_subfc_T0_T1(void)
{
	T0 = do_execute_subtract<true, false>(T0, T1);
}

void OPPROTO op_subfco_T0_T1(void)
{
	T0 = do_execute_subtract<true, true>(T0, T1);
}

void OPPROTO op_subfe_T0_T1(void)
{
	T0 = do_execute_subtract_extended<false>(T0, T1);
}

void OPPROTO op_subfeo_T0_T1(void)
{
	T0 = do_execute_subtract_extended<true>(T0, T1);
}

void OPPROTO op_subfme_T0(void)
{
	T0 = do_execute_subtract_extended<false>(T0, 0xffffffff);
}

void OPPROTO op_subfmeo_T0(void)
{
	T0 = do_execute_subtract_extended<true>(T0, 0xffffffff);
}

void OPPROTO op_subfze_T0(void)
{
	T0 = do_execute_subtract_extended<false>(T0, 0);
}

void OPPROTO op_subfzeo_T0(void)
{
	T0 = do_execute_subtract_extended<true>(T0, 0);
}
