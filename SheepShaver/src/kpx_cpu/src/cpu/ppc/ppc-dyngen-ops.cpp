/*
 *  ppc-dyngen-ops.hpp - PowerPC synthetic instructions
 *
 *  Kheperix (C) 2003-2004 Gwenole Beauchesne
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
#if SIZEOF_VOID_P == 8
#define REG32(X) ((uint32)X)
#else
#define REG32(X) X
#endif
#define FPREG(X) ((powerpc_fpr *)(X))
#define A0 REG32(reg_A0)
register uintptr reg_A0 asm(REG_A0);
#define T0 REG32(reg_T0)
#define F0 FPREG(reg_T0)->d
#define F0_dw FPREG(reg_T0)->j
register uintptr reg_T0 asm(REG_T0);
#define T1 REG32(reg_T1)
#define F1 FPREG(reg_T1)->d
#define F1_dw FPREG(reg_T1)->j
register uintptr reg_T1 asm(REG_T1);
#define T2 REG32(reg_T2)
#define F2 FPREG(reg_T2)->d
#define F2_dw FPREG(reg_T2)->j
register uintptr reg_T2 asm(REG_T2);
#define FD powerpc_dyngen_helper::fp_result()
#define FD_dw powerpc_dyngen_helper::fp_result_dw()

// Vector registers
#define VREG(X)			((powerpc_vr *)(X))[0]
#define VD				VREG(reg_VD)
#define reg_VD			reg_A0
#define V0				VREG(reg_V0)
#define reg_V0			reg_T0
#define V1				VREG(reg_V1)
#define reg_V1			reg_T1
#define V2				VREG(reg_V2)
#define reg_V2			reg_T2

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
	static inline uint32 get_fpscr()			{ return CPU->fpscr(); }
	static inline void set_fpscr(uint32 value)	{ CPU->fpscr() = value; }
	static inline uint32 get_xer()				{ return CPU->xer().get(); }
	static inline void set_xer(uint32 value)	{ CPU->xer().set(value); }
	static inline uint32 get_vrsave()			{ return CPU->vrsave(); }
	static inline void set_vrsave(uint32 value)	{ CPU->vrsave() = value; }
	static inline void record(int crf, int32 v)	{ CPU->record_cr(crf, v); }
	static inline powerpc_cr_register & cr()	{ return CPU->cr(); }
	static inline powerpc_xer_register & xer()	{ return CPU->xer(); }
	static inline powerpc_spcflags & spcflags()	{ return CPU->spcflags(); }
	static double & fp_result()					{ return CPU->fp_result(); }
	static uint64 & fp_result_dw()				{ return CPU->fp_result_dw(); }
	static inline void set_cr(int crfd, int v)	{ CPU->cr().set(crfd, v); }

	static inline powerpc_block_info *find_block(uint32 pc) { return CPU->block_cache.fast_find(pc); }
};

// Semantic action templates
#define DYNGEN_OPS
#include "ppc-execute.hpp"


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
DEFINE_OP(T1,N);								\
DEFINE_OP(T2,N);

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
 *		Load/store floating-point registers
 **/

#define DEFINE_OP(REG, N)						\
void OPPROTO op_load_F##REG##_FPR##N(void)		\
{												\
	reg_T##REG = (uintptr)&CPU->fpr(N);			\
}												\
void OPPROTO op_store_F##REG##_FPR##N(void)		\
{												\
	CPU->fpr(N) = F##REG;						\
}
#define DEFINE_REG(N)							\
DEFINE_OP(0,N);									\
DEFINE_OP(1,N);									\
DEFINE_OP(2,N);									\
void OPPROTO op_store_FD_FPR##N(void)			\
{												\
	CPU->fpr(N) = FD;							\
}

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
 *		Load/Store floating-point data
 **/

#if defined(__i386__)
#define do_load_double(REG, EA) do {			\
	uint32 *w = (uint32 *)&REG;					\
	w[1] = vm_read_memory_4(EA + 0);			\
	w[0] = vm_read_memory_4(EA + 4);			\
} while (0)
#define do_store_double(REG, EA) do {			\
	uint32 *w = (uint32 *)&REG;					\
	vm_write_memory_4(EA + 0, w[1]);			\
	vm_write_memory_4(EA + 4, w[0]);			\
} while (0)
#endif

#ifndef do_load_single
#define do_load_single(REG, EA) do {			\
	any_register *x = (any_register *)&FD;		\
	x->i = vm_read_memory_4(EA);				\
	REG = (double)x->f;							\
} while (0)
#endif

#ifndef do_store_single
#define do_store_single(REG, EA) do {			\
	any_register *x = (any_register *)&FD;		\
	x->f = (float)REG;							\
	vm_write_memory_4(EA, x->i);				\
} while (0)
#endif

#ifndef do_load_double
#define do_load_double(REG, EA) REG##_dw = vm_read_memory_8(EA)
#endif

#ifndef do_store_double
#define do_store_double(REG, EA) vm_write_memory_8(EA, REG##_dw)
#endif

#define im PARAM1
#define DEFINE_OP(OFFSET)							\
void OPPROTO op_load_double_FD_A0_##OFFSET(void)	\
{													\
	do_load_double(FD, A0 + OFFSET);				\
}													\
void OPPROTO op_load_single_FD_A0_##OFFSET(void)	\
{													\
	do_load_single(FD, A0 + OFFSET);				\
}													\
void OPPROTO op_store_double_F0_A0_##OFFSET(void)	\
{													\
	do_store_double(F0, A0 + OFFSET);				\
}													\
void OPPROTO op_store_single_F0_A0_##OFFSET(void)	\
{													\
	do_store_single(F0, A0 + OFFSET);				\
}

DEFINE_OP(0);
DEFINE_OP(im);
DEFINE_OP(T1);

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

#define DEFINE_OP(REG, N)											\
void OPPROTO op_load_##REG##_crb##N(void)							\
{																	\
	const uint32 cr = powerpc_dyngen_helper::get_cr();				\
	REG = (cr >> (31 - N)) & 1; 									\
}																	\
void OPPROTO op_store_##REG##_crb##N(void)							\
{																	\
	uint32 cr = powerpc_dyngen_helper::get_cr() & ~(1 << (31 - N));	\
	cr |= ((REG & 1) << (31 - N));									\
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
	REG = powerpc_dyngen_helper::cr().get(CRF);	\
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

void OPPROTO op_mtcrf_T0_im(void)
{
	const uint32 mask = PARAM1;
	const uint32 cr = powerpc_dyngen_helper::get_cr();
	powerpc_dyngen_helper::set_cr((cr & ~mask) | (T0 & mask));
}


/**
 *		Native FP operations optimization
 **/

#if defined(__i386__)
#define do_fabs(x)				({ double y; asm volatile ("fabs" : "=t" (y) : "0" (x)); y; })
#define do_fneg(x)				({ double y; asm volatile ("fchs" : "=t" (y) : "0" (x)); y; })
#endif

#ifndef do_fabs
#define do_fabs(x)				fabs(x)
#endif
#ifndef do_fadd
#define do_fadd(x, y)			(x) + (y)
#endif
#ifndef do_fdiv
#define do_fdiv(x, y)			(x) / (y)
#endif
#ifndef do_fmadd
#define do_fmadd(x, y, z)		(((x) * (y)) + (z))
#endif
#ifndef do_fmsub
#define do_fmsub(x, y, z)		(((x) * (y)) - (z))
#endif
#ifndef do_fmul
#define do_fmul(x, y)			((x) * (y))
#endif
#ifndef do_fneg
#define do_fneg(x)				-(x)
#endif
#ifndef do_fnabs
#define do_fnabs(x)				do_fneg(do_fabs(x))
#endif
#ifndef do_fnmadd
#define do_fnmadd(x, y, z)		do_fneg(((x) * (y)) + (z))
#endif
#ifndef do_fnmsub
#define do_fnmsub(x, y, z)		do_fneg(((x) * (y)) - (z))
#endif
#ifndef do_fsub
#define do_fsub(x, y)			(x) - (y)
#endif
#ifndef do_fmov
#define do_fmov(x)				(x)
#endif


/**
 *		Double-precision floating point operations
 **/

#define DEFINE_OP(NAME, CODE)					\
void OPPROTO op_##NAME(void)					\
{												\
	CODE;										\
}

DEFINE_OP(fmov_F0_F1, F0 = F1);
DEFINE_OP(fmov_F0_F2, F0 = F2);
DEFINE_OP(fmov_F1_F0, F1 = F0);
DEFINE_OP(fmov_F1_F2, F1 = F2);
DEFINE_OP(fmov_F2_F0, F2 = F0);
DEFINE_OP(fmov_F2_F1, F2 = F1);
DEFINE_OP(fmov_FD_F0, FD = F0);
DEFINE_OP(fmov_FD_F1, FD = F1);
DEFINE_OP(fmov_FD_F2, FD = F2);

DEFINE_OP(fabs_FD_F0, FD = do_fabs(F0));
DEFINE_OP(fneg_FD_F0, FD = do_fneg(F0));
DEFINE_OP(fnabs_FD_F0, FD = do_fnabs(F0));

DEFINE_OP(fadd_FD_F0_F1, FD = do_fadd(F0, F1));
DEFINE_OP(fsub_FD_F0_F1, FD = do_fsub(F0, F1));
DEFINE_OP(fmul_FD_F0_F1, FD = do_fmul(F0, F1));
DEFINE_OP(fdiv_FD_F0_F1, FD = do_fdiv(F0, F1));
DEFINE_OP(fmadd_FD_F0_F1_F2, FD = do_fmadd(F0, F1, F2));
DEFINE_OP(fmsub_FD_F0_F1_F2, FD = do_fmsub(F0, F1, F2));
DEFINE_OP(fnmadd_FD_F0_F1_F2, FD = do_fnmadd(F0, F1, F2));
DEFINE_OP(fnmsub_FD_F0_F1_F2, FD = do_fnmsub(F0, F1, F2));

#undef DEFINE_OP


/**
 *		Single-Precision floating point operations
 **/

#define DEFINE_OP(NAME, REG, OP)				\
void OPPROTO op_##NAME(void)					\
{												\
	float x = OP;								\
	REG = x;									\
}

DEFINE_OP(fadds_FD_F0_F1, FD, do_fadd(F0, F1));
DEFINE_OP(fsubs_FD_F0_F1, FD, do_fsub(F0, F1));
DEFINE_OP(fmuls_FD_F0_F1, FD, do_fmul(F0, F1));
DEFINE_OP(fdivs_FD_F0_F1, FD, do_fdiv(F0, F1));
DEFINE_OP(fmadds_FD_F0_F1_F2, FD, do_fmadd(F0, F1, F2));
DEFINE_OP(fmsubs_FD_F0_F1_F2, FD, do_fmsub(F0, F1, F2));
DEFINE_OP(fnmadds_FD_F0_F1_F2, FD, do_fnmadd(F0, F1, F2));
DEFINE_OP(fnmsubs_FD_F0_F1_F2, FD, do_fnmsub(F0, F1, F2));

#undef DEFINE_OP


/**
 *		Special purpose registers
 **/

void OPPROTO op_load_T0_VRSAVE(void)
{
	T0 = powerpc_dyngen_helper::get_vrsave();
}

void OPPROTO op_store_T0_VRSAVE(void)
{
	powerpc_dyngen_helper::set_vrsave(T0);
}

void OPPROTO op_load_T0_XER(void)
{
	T0 = powerpc_dyngen_helper::get_xer();
}

void OPPROTO op_store_T0_XER(void)
{
	powerpc_dyngen_helper::set_xer(T0);
}

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

void OPPROTO op_spcflags_init(void)
{
	powerpc_dyngen_helper::spcflags().set(PARAM1);
}

void OPPROTO op_spcflags_set(void)
{
	powerpc_dyngen_helper::spcflags().set(PARAM1);
}

void OPPROTO op_spcflags_clear(void)
{
	powerpc_dyngen_helper::spcflags().clear(PARAM1);
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

struct branch_if_T0_condition {
	static inline bool test() {
		return T0 != 0;
	}
};

struct branch_if_not_T0_condition {
	static inline bool test() {
		return T0 == 0;
	}
};

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
void OPPROTO op_##COND##_ctr_##CTR(void)									\
{																			\
	do_execute_branch<COND##_condition, ctr_##CTR##_condition>(A0, PARAM1);	\
}
#define DEFINE_OP(COND)							\
DEFINE_OP_CTR(COND,0x);							\
DEFINE_OP_CTR(COND,10);							\
DEFINE_OP_CTR(COND,11);

DEFINE_OP(branch_if_T0);
DEFINE_OP(branch_if_not_T0);

#undef DEFINE_OP
#undef DEFINE_OP_CTR

#ifdef DYNGEN_FAST_DISPATCH
#if defined(__x86_64__)
#define FAST_COMPARE_SPECFLAGS_DISPATCH(SPCFLAGS, TARGET) \
		asm volatile ("test %0,%0 ; jz " #TARGET : "+r" (SPCFLAGS))
#endif
#ifndef FAST_COMPARE_SPECFLAGS_DISPATCH
#define FAST_COMPARE_SPECFLAGS_DISPATCH(SPCFLAGS, TARGET) \
		if (SPCFLAGS == 0) DYNGEN_FAST_DISPATCH(TARGET)
#endif
#endif

template< int bo, bool chain >
static inline void do_execute_branch_bo(uint32 tpc, uint32 npc)
{
	bool ctr_ok = true;
	bool cond_ok = true;

	if (BO_CONDITIONAL_BRANCH(bo)) {
		if (BO_BRANCH_IF_TRUE(bo))
			cond_ok = T0;
		else
			cond_ok = !T0;
	}

	if (BO_DECREMENT_CTR(bo)) {
		T1 = powerpc_dyngen_helper::get_ctr() - 1;
		powerpc_dyngen_helper::set_ctr(T1);
		if (BO_BRANCH_IF_CTR_ZERO(bo))
			ctr_ok = T1 == 0;
		else
			ctr_ok = T1 != 0;
	}

#ifdef DYNGEN_FAST_DISPATCH
	if (chain) {
		T1 = powerpc_dyngen_helper::spcflags().get();
		if (ctr_ok && cond_ok) {
			FAST_COMPARE_SPECFLAGS_DISPATCH(T1, __op_jmp0);
			T0 = tpc;
		}
		else {
			FAST_COMPARE_SPECFLAGS_DISPATCH(T1, __op_jmp1);
			T0 = npc;
		}
	}
	else
#endif

	T0 = (ctr_ok && cond_ok) ? tpc : npc;
	powerpc_dyngen_helper::set_pc(T0);
	dyngen_barrier();
}

#define BO(A,B,C,D) (((A) << 4)| ((B) << 3) | ((C) << 2) | ((D) << 1))
#define DEFINE_OP1(BO_SUFFIX, BO_VALUE, CHAIN)				\
void OPPROTO op_branch_A0_bo_##BO_SUFFIX##_##CHAIN(void)	\
{															\
	do_execute_branch_bo<BO BO_VALUE, CHAIN>(A0, PARAM1);	\
}
#define DEFINE_OP(BO_SUFFIX, BO_VALUE)			\
DEFINE_OP1(BO_SUFFIX, BO_VALUE, 0)				\
DEFINE_OP1(BO_SUFFIX, BO_VALUE, 1)

DEFINE_OP(0000,(0,0,0,0));
DEFINE_OP(0001,(0,0,0,1));
DEFINE_OP(001x,(0,0,1,0));
DEFINE_OP(0100,(0,1,0,0));
DEFINE_OP(0101,(0,1,0,1));
DEFINE_OP(011x,(0,1,1,0));
DEFINE_OP(1x00,(1,0,0,0));
DEFINE_OP(1x01,(1,0,0,1));
// NOTE: the compiler is expected to optimize out the use of PARAM1
DEFINE_OP(1x1x,(1,0,1,0));

#undef DEFINE_OP
#undef BO


/**
 *		Compare & Record instructions
 **/

void OPPROTO op_record_cr0_T0(void)
{
	uint32 cr = powerpc_dyngen_helper::get_cr() & ~CR_field<0>::mask();
	cr |= powerpc_dyngen_helper::xer().get_so() << 28;
	if ((int32)T0 < 0)
		cr |= CR_LT_field<0>::mask();
	else if ((int32)T0 > 0)
		cr |= CR_GT_field<0>::mask();
	else
		cr |= CR_EQ_field<0>::mask();
	powerpc_dyngen_helper::set_cr(cr);
	dyngen_barrier();
}

void OPPROTO op_record_cr1(void)
{
	powerpc_dyngen_helper::set_cr((powerpc_dyngen_helper::get_cr() & ~CR_field<1>::mask()) |
								  ((powerpc_dyngen_helper::get_fpscr() >> 4) & 0x0f000000));
}

#define im PARAM1

#if DYNGEN_ASM_OPTS && defined(__powerpc__) && 0

#define DEFINE_OP(NAME, COMP, LHS, RHST, RHS)												\
void OPPROTO op_##NAME##_##LHS##_##RHS(void)												\
{																							\
	T0 = powerpc_dyngen_helper::xer().get_so();												\
	uint32 v;																				\
	asm volatile (COMP " 7,%1,%2 ; mfcr %0" : "=r" (v) : "r" (LHS), RHST (RHS) : "cr7");	\
	T0 |= (v & 0xe);																		\
}

DEFINE_OP(compare,"cmpw",T0,"r",T1);
DEFINE_OP(compare,"cmpw",T0,"r",im);
DEFINE_OP(compare,"cmpwi",T0,"i",0);
DEFINE_OP(compare_logical,"cmplw",T0,"r",T1);
DEFINE_OP(compare_logical,"cmplw",T0,"r",im);
DEFINE_OP(compare_logical,"cmplwi",T0,"i",0);

#else

#define DEFINE_OP(NAME, TYPE, LHS, RHS)							\
void OPPROTO op_##NAME##_##LHS##_##RHS(void)					\
{																\
	const uint32 SO = powerpc_dyngen_helper::xer().get_so();	\
	if ((TYPE)LHS < (TYPE)RHS)									\
		T0 = SO | standalone_CR_LT_field::mask();				\
	else if ((TYPE)LHS > (TYPE)RHS)								\
		T0 = SO | standalone_CR_GT_field::mask();				\
	else														\
		T0 = SO | standalone_CR_EQ_field::mask();				\
	dyngen_barrier();											\
}

DEFINE_OP(compare,int32,T0,T1);
DEFINE_OP(compare,int32,T0,im);
DEFINE_OP(compare,int32,T0,0);
DEFINE_OP(compare_logical,uint32,T0,T1);
DEFINE_OP(compare_logical,uint32,T0,im);
DEFINE_OP(compare_logical,uint32,T0,0);

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
	T1 &= 0x3f;
	if (T1 & 0x20) {
		const uint32 SB = T0 >> 31;
		powerpc_dyngen_helper::xer().set_ca(SB);
		T0 = -SB;
	}
	else {
		const uint32 RD = ((int32)T0) >> T1;
		const bool CA = (int32)T0 < 0 && (T0 & ~(0xffffffff << T1));
		powerpc_dyngen_helper::xer().set_ca(CA);
		T0 = RD;
	}
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
#if defined(__i386__)
	uint32 ov;
	asm volatile ("add %2,%0; seto %b1" : "=r" (T0), "=r" (ov) : "r" (T1) : "cc");
	powerpc_dyngen_helper::xer().set_ov(ov);
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
#if defined(__i386__)
	uint32 ca;
	asm volatile ("add %2,%0; setc %b1" : "=r" (T0), "=r" (ca) : "r" (T1) : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
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
#if defined(__i386__)
	uint32 ca, ov;
	asm volatile ("add %3,%0; setc %b1; seto %b2" : "=r" (T0), "=r" (ca), "=r" (ov) : "r" (T1) : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
	powerpc_dyngen_helper::xer().set_ov(ov);
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
#if defined(__i386__)
	uint32 ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc %2,%0; setc %b1" : "=r" (T0), "+r" (ca) : "r" (T1) : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
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
#if defined(__i386__)
	uint32 ov, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc %3,%0; setc %b1; seto %b2" : "=r" (T0), "+r" (ca), "=r" (ov) : "r" (T1) : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
	powerpc_dyngen_helper::xer().set_ov(ov);
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
#if defined(__i386__)
	uint32 ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc $-1,%0; setc %b1" : "=r" (T0), "+r" (ca) : : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
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
#if defined(__i386__)
	uint32 ov, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc $-1,%0; setc %b1; seto %b2" : "=r" (T0), "+r" (ca), "=r" (ov) : : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
	powerpc_dyngen_helper::xer().set_ov(ov);
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
#if defined(__i386__)
	uint32 ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc $0,%0; setc %b1" : "=r" (T0), "+r" (ca) : : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
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
#if defined(__i386__)
	uint32 ov, ca = powerpc_dyngen_helper::xer().get_ca();
	asm volatile ("neg %1; adc $0,%0; setc %b1; seto %b2" : "=r" (T0), "+r" (ca), "=r" (ov) : : "cc");
	powerpc_dyngen_helper::xer().set_ca(ca);
	powerpc_dyngen_helper::xer().set_ov(ov);
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

/**
 *		Misc synthetic instructions
 **/

void OPPROTO op_inc_32_mem(void)
{
	uint32 *m = (uint32 *)PARAM1;
	*m += 1;
}

void OPPROTO op_nego_T0(void)
{
	powerpc_dyngen_helper::xer().set_ov(T0 == 0x80000000);
	T0 = -T0;
}

/**
 *		Generate possible call to next basic block without going
 *		through register state restore & full cache lookup
 **/

void OPPROTO op_jump_next_A0(void)
{
	// Make sure there is no pending interrupt request
	if (powerpc_dyngen_helper::spcflags().empty()) {
		powerpc_block_info *bi = (powerpc_block_info *)reg_A0;
		uint32 pc = powerpc_dyngen_helper::get_pc();
		if (bi->pc == pc || (bi = powerpc_dyngen_helper::find_block(pc)) != NULL)
			goto *(bi->entry_point);
	}
	dyngen_barrier();
}

/**
 *		Load/store addresses to vector registers
 **/

#define reg_TD reg_A0
#define DEFINE_OP(REG, N)						\
void OPPROTO op_load_ad_V##REG##_VR##N(void)	\
{												\
	reg_T##REG = (uintptr)&CPU->vr(N);			\
}												
#define DEFINE_REG(N)							\
DEFINE_OP(D,N);									\
DEFINE_OP(0,N);									\
DEFINE_OP(1,N);									\
DEFINE_OP(2,N);									\

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
#undef reg_TD

void op_load_word_VD_T0(void)
{
	const uint32 ea = T0;
	VD.w[(ea >> 2) & 3] = vm_read_memory_4(ea & ~3);
}

void op_store_word_VD_T0(void)
{
	const uint32 ea = T0;
	vm_write_memory_4(ea & ~3, VD.w[(ea >> 2) & 3]);
}

void op_load_vect_VD_T0(void)
{
	const uint32 ea = T0 & ~15;
	VD.w[0] = vm_read_memory_4(ea +  0);
	VD.w[1] = vm_read_memory_4(ea +  4);
	VD.w[2] = vm_read_memory_4(ea +  8);
	VD.w[3] = vm_read_memory_4(ea + 12);
}

void op_store_vect_VD_T0(void)
{
	const uint32 ea = T0 & ~15;
	vm_write_memory_4(ea +  0, VD.w[0]);
	vm_write_memory_4(ea +  4, VD.w[1]);
	vm_write_memory_4(ea +  8, VD.w[2]);
	vm_write_memory_4(ea + 12, VD.w[3]);
}

/**
 *		Vector operations helpers
 **/

#define VNONE op_VNONE
struct op_VNONE {
	typedef null_operand type;
	static inline uint32 get(powerpc_vr const & v, int i) { return 0; }
	static inline void set(powerpc_vr const & v, int i, uint32) { }
};

#define V16QI op_V16QI
struct op_V16QI {
	typedef uint8 type;
	static inline type get(powerpc_vr const & v, int i) { return v.b[i]; }
	static inline void set(powerpc_vr & v, int i, type x) { v.b[i] = x; }
};

#define V8HI op_V8HI
struct op_V8HI {
	typedef uint16 type;
	static inline type get(powerpc_vr const & v, int i) { return v.h[i]; }
	static inline void set(powerpc_vr & v, int i, type x) { v.h[i] = x; }
};

#define V4SI op_V4SI
struct op_V4SI {
	typedef uint32 type;
	static inline type get(powerpc_vr const & v, int i) { return v.w[i]; }
	static inline void set(powerpc_vr & v, int i, type x) { v.w[i] = x; }
};

#define V2DI op_V2DI
struct op_V2DI {
	typedef uint64 type;
	static inline type get(powerpc_vr const & v, int i) { return v.j[i]; }
	static inline void set(powerpc_vr & v, int i, type x) { v.j[i] = x; }
};

#define V4SF op_V4SF
struct op_V4SF {
	typedef float type;
	static inline type get(powerpc_vr const & v, int i) { return v.f[i]; }
	static inline void set(powerpc_vr & v, int i, type x) { v.f[i] = x; }
};

template< class OP, class VX, class VA, class VB, class VC, int N >
struct do_vector_execute {
	static inline void apply() {
		do_vector_execute<OP, VX, VA, VB, VC, N - 1>::apply();
		VX::set(
			VD, N,
			op_apply<typename VX::type, OP, typename VA::type, typename VB::type, typename VC::type>::apply(
				VA::get(V0, N),
				VB::get(V1, N),
				VC::get(V2, N)));
	}
};

template< class OP, class VX, class VA, class VB, class VC >
struct do_vector_execute<OP, VX, VA, VB, VC, 0> {
	static inline void apply() {
		VX::set(
			VD, 0, op_apply<typename VX::type, OP, typename VA::type, typename VB::type, typename VC::type>::apply(
				VA::get(V0, 0),
				VB::get(V1, 0),
				VC::get(V2, 0)));
	}
};

template< class OP, class VX, class VA, class VB = VNONE, class VC = VNONE >
struct vector_execute {
	static inline void apply() {
		do_vector_execute<OP, VX, VA, VB, VC, (16 / sizeof(typename VX::type)) - 1>::apply();
	}
};


/**
 *		Vector synthetic operations
 **/

void op_vaddfp_VD_V0_V1(void)
{
	vector_execute<op_fadds, V4SF, V4SF, V4SF>::apply();
}

void op_vsubfp_VD_V0_V1(void)
{
	vector_execute<op_fsubs, V4SF, V4SF, V4SF>::apply();
}

void op_vmaddfp_VD_V0_V1_V2(void)
{
	vector_execute<op_vmaddfp, V4SF, V4SF, V4SF, V4SF>::apply();
}

#if defined(__i386__) && defined(__SSE__)
// Workaround gcc 3.2.2 miscompilation that inserts SSE instructions
struct op_do_vnmsubfp {
	static inline float apply(float x, float y, float z) {
//		return 0. - ((x * z) - y);
		return y - (x * z);
	}
};
#else
typedef op_vnmsubfp op_do_vnmsubfp;
#endif

void op_vnmsubfp_VD_V0_V1_V2(void)
{
	vector_execute<op_do_vnmsubfp, V4SF, V4SF, V4SF, V4SF>::apply();
}

void op_vmaxfp_VD_V0_V1(void)
{
	vector_execute<op_max<float>, V4SF, V4SF, V4SF>::apply();
}

void op_vminfp_VD_V0_V1(void)
{
	vector_execute<op_min<float>, V4SF, V4SF, V4SF>::apply();
}

void op_vand_VD_V0_V1(void)
{
	vector_execute<op_and_64, V2DI, V2DI, V2DI>::apply();
}

void op_vandc_VD_V0_V1(void)
{
	vector_execute<op_andc_64, V2DI, V2DI, V2DI>::apply();
}

void op_vnor_VD_V0_V1(void)
{
	vector_execute<op_nor_64, V2DI, V2DI, V2DI>::apply();
}

void op_vor_VD_V0_V1(void)
{
	vector_execute<op_or_64, V2DI, V2DI, V2DI>::apply();
}

void op_vxor_VD_V0_V1(void)
{
	vector_execute<op_xor_64, V2DI, V2DI, V2DI>::apply();
}

void op_record_cr6_VD(void)
{
	if (VD.j[0] == UVAL64(0xffffffffffffffff) &&
		VD.j[1] == UVAL64(0xffffffffffffffff))
		powerpc_dyngen_helper::cr().set(6, 8);
	else if (VD.j[0] == UVAL64(0) && VD.j[1] == UVAL64(0))
		powerpc_dyngen_helper::cr().set(6, 2);
	else
		powerpc_dyngen_helper::cr().set(6, 0);
	dyngen_barrier();
}

#undef VNONE
#undef V16QI
#undef V8HI
#undef V4SI
#undef V2DI
#undef V4SF

/**
 *		SSE optimizations
 **/

#if defined(__SSE__) && defined(HAVE_XMMINTRIN_H)
#include <xmmintrin.h>
#undef  VD
#define VD *((__m128 *)reg_VD)
#undef  V0
#define V0 *((__m128 *)reg_V0)
#undef  V1
#define V1 *((__m128 *)reg_V1)
#undef  V2
#define V2 *((__m128 *)reg_V2)

void op_sse_nop(void)
{
	asm volatile ("nop");
}

void op_sse_vcmpeqfp(void)
{
	VD = _mm_cmpeq_ps(V0, V1);
}

void op_sse_vcmpgefp(void)
{
	VD = _mm_cmpge_ps(V0, V1);
}

void op_sse_vcmpgtfp(void)
{
	VD = _mm_cmpgt_ps(V0, V1);
}

void op_sse_vaddfp(void)
{
	VD = _mm_add_ps(V0, V1);
}

void op_sse_vsubfp(void)
{
	VD = _mm_sub_ps(V0, V1);
}

void op_sse_vmaddfp(void)
{
	VD = _mm_add_ps(_mm_mul_ps(V0, V2), V1);
}

void op_sse_vnmsubfp(void)
{
	VD = _mm_sub_ps(_mm_setzero_ps(), _mm_sub_ps(_mm_mul_ps(V0, V2), V1));
}

void op_sse_vmaxfp(void)
{
	VD = _mm_max_ps(V0, V1);
}

void op_sse_vminfp(void)
{
	VD = _mm_min_ps(V0, V1);
}

void op_sse_vand(void)
{
	VD = _mm_and_ps(V0, V1);
}

void op_sse_vandc(void)
{
	VD = _mm_andnot_ps(V1, V0);
}

void op_sse_vor(void)
{
	VD = _mm_or_ps(V0, V1);
}

void op_sse_vxor(void)
{
	VD = _mm_xor_ps(V0, V1);
}
#endif

/**
 *		MMX optimizations
 **/

#if defined(__MMX__) && defined(HAVE_MMINTRIN_H)
#include <mmintrin.h>
#undef  VD
#define VD ((__m64 *)reg_VD)
#undef  V0
#define V0 ((__m64 *)reg_V0)
#undef  V1
#define V1 ((__m64 *)reg_V1)
#undef  V2
#define V2 ((__m64 *)reg_V2)

void op_mmx_nop(void)
{
	asm volatile ("nop");
}

void op_emms(void)
{
	_mm_empty();
}

void op_mmx_vcmpequb(void)
{
	VD[0] = _mm_cmpeq_pi8(V0[0], V1[0]);
	VD[1] = _mm_cmpeq_pi8(V0[1], V1[1]);
}

void op_mmx_vcmpequh(void)
{
	VD[0] = _mm_cmpeq_pi16(V0[0], V1[0]);
	VD[1] = _mm_cmpeq_pi16(V0[1], V1[1]);
}

void op_mmx_vcmpequw(void)
{
	VD[0] = _mm_cmpeq_pi32(V0[0], V1[0]);
	VD[1] = _mm_cmpeq_pi32(V0[1], V1[1]);
}

void op_mmx_vcmpgtsb(void)
{
	VD[0] = _mm_cmpgt_pi8(V0[0], V1[0]);
	VD[1] = _mm_cmpgt_pi8(V0[1], V1[1]);
}

void op_mmx_vcmpgtsh(void)
{
	VD[0] = _mm_cmpgt_pi16(V0[0], V1[0]);
	VD[1] = _mm_cmpgt_pi16(V0[1], V1[1]);
}

void op_mmx_vcmpgtsw(void)
{
	VD[0] = _mm_cmpgt_pi32(V0[0], V1[0]);
	VD[1] = _mm_cmpgt_pi32(V0[1], V1[1]);
}

void op_mmx_vaddubm(void)
{
	VD[0] = _mm_add_pi8(V0[0], V1[0]);
	VD[1] = _mm_add_pi8(V0[1], V1[1]);
}

void op_mmx_vadduhm(void)
{
	VD[0] = _mm_add_pi16(V0[0], V1[0]);
	VD[1] = _mm_add_pi16(V0[1], V1[1]);
}

void op_mmx_vadduwm(void)
{
	VD[0] = _mm_add_pi32(V0[0], V1[0]);
	VD[1] = _mm_add_pi32(V0[1], V1[1]);
}

void op_mmx_vsububm(void)
{
	VD[0] = _mm_sub_pi8(V0[0], V1[0]);
	VD[1] = _mm_sub_pi8(V0[1], V1[1]);
}

void op_mmx_vsubuhm(void)
{
	VD[0] = _mm_sub_pi16(V0[0], V1[0]);
	VD[1] = _mm_sub_pi16(V0[1], V1[1]);
}

void op_mmx_vsubuwm(void)
{
	VD[0] = _mm_sub_pi32(V0[0], V1[0]);
	VD[1] = _mm_sub_pi32(V0[1], V1[1]);
}

void op_mmx_vand(void)
{
	VD[0] = _mm_and_si64(V0[0], V1[0]);
	VD[1] = _mm_and_si64(V0[1], V1[1]);
}

void op_mmx_vandc(void)
{
	VD[0] = _mm_andnot_si64(V1[0], V0[0]);
	VD[1] = _mm_andnot_si64(V1[1], V0[1]);
}

void op_mmx_vor(void)
{
	VD[0] = _mm_or_si64(V0[0], V1[0]);
	VD[1] = _mm_or_si64(V0[1], V1[1]);
}

void op_mmx_vxor(void)
{
	VD[0] = _mm_xor_si64(V0[0], V1[0]);
	VD[1] = _mm_xor_si64(V0[1], V1[1]);
}

#if defined(__SSE__)
void op_mmx_vmaxub(void)
{
	VD[0] = _mm_max_pu8(V0[0], V1[0]);
	VD[1] = _mm_max_pu8(V0[1], V1[1]);
}

void op_mmx_vminub(void)
{
	VD[0] = _mm_min_pu8(V0[0], V1[0]);
	VD[1] = _mm_min_pu8(V0[1], V1[1]);
}

void op_mmx_vmaxsh(void)
{
	VD[0] = _mm_max_pi16(V0[0], V1[0]);
	VD[1] = _mm_max_pi16(V0[1], V1[1]);
}

void op_mmx_vminsh(void)
{
	VD[0] = _mm_min_pi16(V0[0], V1[0]);
	VD[1] = _mm_min_pi16(V0[1], V1[1]);
}
#endif
#endif
