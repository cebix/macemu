/*
 *  dyngen-ops.hpp - Synthetic opcodes
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

#include <math.h>

// We need at least 5 general purpose registers
struct basic_cpu;
#ifdef REG_CPU
register basic_cpu *CPU asm(REG_CPU);
#else
#define CPU ((basic_cpu *)CPUPARAM)
#endif
#if SIZEOF_VOID_P == 8
#define REG32(X) ((uint32)X)
#else
#define REG32(X) X
#endif
#define A0 REG32(reg_A0)
register uintptr reg_A0 asm(REG_A0);
#define T0 REG32(reg_T0)
register uintptr reg_T0 asm(REG_T0);
#define T1 REG32(reg_T1)
register uintptr reg_T1 asm(REG_T1);
#ifdef REG_T2
#define T2 REG32(reg_T2)
register uintptr reg_T2 asm(REG_T2);
#endif
#ifdef REG_T3
#define T3 REG32(reg_T3)
register uintptr reg_T3 asm(REG_T3);
#endif


/**
 *		Native ALU operations optimization
 **/

#ifndef do_udiv_32
#define do_udiv_32(x, y)		((uint32)x / (uint32)y)
#endif
#ifndef do_sdiv_32
#define do_sdiv_32(x, y)		((int32)x / (int32)y)
#endif
#ifndef do_rol_32
#define do_rol_32(x, y)			((x << y) | (x >> (32 - y)))
#endif
#ifndef do_ror_32
#define do_ror_32(x, y)			((x >> y) | (x << (32 - y)))
#endif


/**
 *		ALU operations
 **/

#define DEFINE_OP(NAME, CODE)					\
void OPPROTO op_##NAME(void)					\
{												\
	CODE;										\
}

// Register moves
DEFINE_OP(mov_32_T0_im, T0 = PARAM1);
DEFINE_OP(mov_32_T0_T1, T0 = T1);
DEFINE_OP(mov_32_T0_A0, T0 = A0);
DEFINE_OP(mov_32_T1_im, T1 = PARAM1);
DEFINE_OP(mov_32_T1_T0, T1 = T0);
DEFINE_OP(mov_32_T1_A0, T1 = A0);
DEFINE_OP(mov_32_A0_im, A0 = PARAM1);
DEFINE_OP(mov_32_A0_T0, A0 = T0);
DEFINE_OP(mov_32_A0_T1, A0 = T1);

void OPPROTO op_mov_ad_A0_im(void)
{
#if SIZEOF_VOID_P == 8
#if defined(__x86_64__)
	asm volatile ("movabsq $__op_param1,%" REG_A0);
#else
#error "unsupported 64-bit value move in"
#endif
#else
	A0 = PARAM1;
#endif
}

// Arithmetic operations
DEFINE_OP(add_32_T0_T1, T0 += T1);
DEFINE_OP(add_32_T0_im, T0 += PARAM1);
DEFINE_OP(add_32_T0_1,  T0 += 1);
DEFINE_OP(add_32_T0_2,  T0 += 2);
DEFINE_OP(add_32_T0_4,  T0 += 4);
DEFINE_OP(add_32_T0_8,  T0 += 8);
DEFINE_OP(sub_32_T0_T1, T0 -= T1);
DEFINE_OP(sub_32_T0_im, T0 -= PARAM1);
DEFINE_OP(sub_32_T0_1,  T0 -= 1);
DEFINE_OP(sub_32_T0_2,  T0 -= 2);
DEFINE_OP(sub_32_T0_4,  T0 -= 4);
DEFINE_OP(sub_32_T0_8,  T0 -= 8);
DEFINE_OP(add_32_T1_T0, T1 += T0);
DEFINE_OP(add_32_T1_im, T1 += PARAM1);
DEFINE_OP(add_32_T1_1,  T1 += 1);
DEFINE_OP(add_32_T1_2,  T1 += 2);
DEFINE_OP(add_32_T1_4,  T1 += 4);
DEFINE_OP(add_32_T1_8,  T1 += 8);
DEFINE_OP(sub_32_T1_T0, T1 -= T0);
DEFINE_OP(sub_32_T1_im, T1 -= PARAM1);
DEFINE_OP(sub_32_T1_1,  T1 -= 1);
DEFINE_OP(sub_32_T1_2,  T1 -= 2);
DEFINE_OP(sub_32_T1_4,  T1 -= 4);
DEFINE_OP(sub_32_T1_8,  T1 -= 8);
DEFINE_OP(add_32_A0_T1, A0 += T1);
DEFINE_OP(add_32_A0_im, A0 += PARAM1);
DEFINE_OP(add_32_A0_1,  A0 += 1);
DEFINE_OP(add_32_A0_2,  A0 += 2);
DEFINE_OP(add_32_A0_4,  A0 += 4);
DEFINE_OP(add_32_A0_8,  A0 += 8);
DEFINE_OP(sub_32_A0_T1, A0 -= T1);
DEFINE_OP(sub_32_A0_im, A0 -= PARAM1);
DEFINE_OP(sub_32_A0_1,  A0 -= 1);
DEFINE_OP(sub_32_A0_2,  A0 -= 2);
DEFINE_OP(sub_32_A0_4,  A0 -= 4);
DEFINE_OP(sub_32_A0_8,  A0 -= 8);
DEFINE_OP(umul_32_T0_T1, T0 = (uint32)T0 * (uint32)T1);
DEFINE_OP(smul_32_T0_T1, T0 = (int32)T0 * (int32)T1);
DEFINE_OP(udiv_32_T0_T1, T0 = do_udiv_32(T0, T1));
DEFINE_OP(sdiv_32_T0_T1, T0 = do_sdiv_32(T0, T1));
DEFINE_OP(xchg_32_T0_T1, { uint32 tmp = T0; T0 = T1; T1 = tmp; });
DEFINE_OP(bswap_16_T0, T0 = bswap_16(T0));
DEFINE_OP(bswap_32_T0, T0 = bswap_32(T0));

// Logical operations
DEFINE_OP(neg_32_T0, T0 = -T0);
DEFINE_OP(not_32_T0, T0 = !T0);
DEFINE_OP(not_32_T1, T1 = !T1);
DEFINE_OP(and_32_T0_T1, T0 &= T1);
DEFINE_OP(and_32_T0_im, T0 &= PARAM1);
DEFINE_OP(or_32_T0_T1, T0 |= T1);
DEFINE_OP(or_32_T0_im, T0 |= PARAM1);
DEFINE_OP(xor_32_T0_T1, T0 ^= T1);
DEFINE_OP(xor_32_T0_im, T0 ^= PARAM1);
DEFINE_OP(orc_32_T0_T1, T0 |= ~T1);
DEFINE_OP(andc_32_T0_T1, T0 &= ~T1);
DEFINE_OP(nand_32_T0_T1, T0 = ~(T0 & T1));
DEFINE_OP(nor_32_T0_T1, T0 = ~(T0 | T1));
DEFINE_OP(eqv_32_T0_T1, T0 = ~(T0 ^ T1));
DEFINE_OP(and_logical_T0_T1, T0 = T0 && T1);
DEFINE_OP(or_logical_T0_T1, T0 = T0 || T1);

// Shift/Rotate operations
DEFINE_OP(lsl_32_T0_T1, T0 = T0 << T1);
DEFINE_OP(lsl_32_T0_im, T0 = T0 << PARAM1);
DEFINE_OP(lsr_32_T0_T1, T0 = T0 >> T1);
DEFINE_OP(lsr_32_T0_im, T0 = T0 >> PARAM1);
DEFINE_OP(asr_32_T0_T1, T0 = ((int32)T0) >> T1);
DEFINE_OP(asr_32_T0_im, T0 = ((int32)T0) >> PARAM1);
DEFINE_OP(rol_32_T0_T1, T0 = do_rol_32(T0, T1));
DEFINE_OP(rol_32_T0_im, T0 = do_rol_32(T0, PARAM1));
DEFINE_OP(ror_32_T0_T1, T0 = do_ror_32(T0, T1));
DEFINE_OP(ror_32_T0_im, T0 = do_ror_32(T0, PARAM1));

// Sign-/Zero-extension
DEFINE_OP(se_16_32_T0, T0 = (int32)(int16)T0);
DEFINE_OP(ze_16_32_T0, T0 = (uint32)(uint16)T0);
DEFINE_OP(se_8_32_T0, T0 = (int32)(int8)T0);
DEFINE_OP(ze_8_32_T0, T0 = (uint32)(uint8)T0);

#undef DEFINE_OP


/**
 *		Native FP operations optimization
 **/

#ifndef do_fabs
#define do_fabs(x)				fabs(x)
#endif
#ifndef do_fadd
#define do_fadd(x, y)			x + y
#endif
#ifndef do_fdiv
#define do_fdiv(x, y)			x / y
#endif
#ifndef do_fmadd
#define do_fmadd(x, y, z)		((x * y) + z)
#endif
#ifndef do_fmsub
#define do_fmsub(x, y, z)		((x * y) - z)
#endif
#ifndef do_fmul
#define do_fmul(x, y)			(x * y)
#endif
#ifndef do_fnabs
#define do_fnabs(x)				-fabs(x)
#endif
#ifndef do_fneg
#define do_fneg(x)				-x
#endif
#ifndef do_fnmadd
#define do_fnmadd(x, y, z)		-((x * y) + z)
#endif
#ifndef do_fnmsub
#define do_fnmsub(x, y, z)		-((x * y) - z)
#endif
#ifndef do_fsub
#define do_fsub(x, y)			x - y
#endif
#ifndef do_fmov
#define do_fmov(x)				x
#endif


/**
 *		FP double operations
 **/

#if 0

double OPPROTO op_lfd(void)
{
	union { double d; uint64 j; } r;
	r.j = vm_do_read_memory_8((uint64 *)T1);
	return r.d;
}

float OPPROTO op_lfs(void)
{
	union { float f; uint32 i; } r;
	r.i = vm_do_read_memory_4((uint32 *)T1);
	return r.f;
}

#define DEFINE_OP(NAME, OP, ARGS)							\
double OPPROTO op_##NAME(double F0, double F1, double F2)	\
{															\
	return do_##OP ARGS;									\
}

DEFINE_OP(fmov_F1, fmov, (F1));
DEFINE_OP(fmov_F2, fmov, (F2));
DEFINE_OP(fabs, fabs, (F0));
DEFINE_OP(fadd, fadd, (F0, F1));
DEFINE_OP(fdiv, fdiv, (F0, F1));
DEFINE_OP(fmadd, fmadd, (F0, F1, F2));
DEFINE_OP(fmsub, fmsub, (F0, F1, F2));
DEFINE_OP(fmul, fmul, (F0, F1));
DEFINE_OP(fnabs, fnabs, (F0));
DEFINE_OP(fneg, fneg, (F0));
DEFINE_OP(fnmadd, fnmadd, (F0, F1, F2));
DEFINE_OP(fnmsub, fnmsub, (F0, F1, F2));
DEFINE_OP(fsub, fsub, (F0, F1));

#undef DEFINE_OP


/**
 *		FP single operations
 **/

#define DEFINE_OP(NAME, OP, ARGS)						\
float OPPROTO op_##NAME(float F0, float F1, float F2)	\
{														\
	return do_##OP ARGS;								\
}

DEFINE_OP(fmovs_F1, fmov, (F1));
DEFINE_OP(fmovs_F2, fmov, (F2));
DEFINE_OP(fabss_F0, fabs, (F0));
DEFINE_OP(fadds_F0_F1, fadd, (F0, F1));
DEFINE_OP(fdivs_F0_F1, fdiv, (F0, F1));
DEFINE_OP(fmadds_F0_F1_F2, fmadd, (F0, F1, F2));
DEFINE_OP(fmsubs_F0_F1_F2, fmsub, (F0, F1, F2));
DEFINE_OP(fmuls_F0_F1, fmul, (F0, F1));
DEFINE_OP(fnabss_F0, fnabs, (F0));
DEFINE_OP(fnegs_F0, fneg, (F0));
DEFINE_OP(fnmadds_F0_F1_F2, fnmadd, (F0, F1, F2));
DEFINE_OP(fnmsubs_F0_F1_F2, fnmsub, (F0, F1, F2));
DEFINE_OP(fsubs_F0_F1, fsub, (F0, F1));

#undef DEFINE_OP

#endif


/**
 *		Load/Store instructions
 **/

#define im PARAM1
#define DEFINE_OP(BITS,REG,SIZE,OFFSET)								\
void OPPROTO op_load_u##BITS##_##REG##_A0_##OFFSET(void)			\
{																	\
	REG = (uint32)(uint##BITS)vm_read_memory_##SIZE(A0 + OFFSET);	\
}																	\
void OPPROTO op_load_s##BITS##_##REG##_A0_##OFFSET(void)			\
{																	\
	REG = (int32)(int##BITS)vm_read_memory_##SIZE(A0 + OFFSET);		\
}																	\
void OPPROTO op_store_##BITS##_##REG##_A0_##OFFSET(void)			\
{																	\
	vm_write_memory_##SIZE(A0 + OFFSET, REG);						\
}

DEFINE_OP(32,T0,4,0);
DEFINE_OP(32,T0,4,im);
DEFINE_OP(32,T0,4,T1);
DEFINE_OP(16,T0,2,0);
DEFINE_OP(16,T0,2,im);
DEFINE_OP(16,T0,2,T1);
DEFINE_OP(8,T0,1,0);
DEFINE_OP(8,T0,1,im);
DEFINE_OP(8,T0,1,T1);

#undef im
#undef DEFINE_OP


/**
 *		Control flow
 **/

#ifdef __i386__
#define FORCE_RET() asm volatile ("ret")
#endif
#ifdef __x86_64__
#define FORCE_RET() asm volatile ("ret")
#endif
#ifdef __powerpc__
#define FORCE_RET() asm volatile ("blr")
#endif
#ifdef __s390__
#define FORCE_RET() asm volatile ("br %r14")
#endif
#ifdef __alpha__
#define FORCE_RET() asm volatile ("ret")
#endif
#ifdef __ia64__
#define FORCE_RET() asm volatile ("br.ret.sptk.many b0;;")
#endif
#ifdef __sparc__
#define FORCE_RET() asm volatile ("jmpl %i0 + 8, %g0\n" \
                                  "nop")
#endif
#ifdef __arm__
#define FORCE_RET() asm volatile ("b exec_loop")
#endif
#ifdef __mc68000
#define FORCE_RET() asm volatile ("rts")
#endif

#define SLOW_DISPATCH(TARGET) do {										\
	static const void __attribute__((unused)) *label1 = &&dummy_label1;	\
	static const void __attribute__((unused)) *label2 = &&dummy_label2;	\
	goto *((void *)TARGET);												\
  dummy_label1:															\
  dummy_label2:															\
	dyngen_barrier();													\
} while (0)

#if defined(__powerpc__)
#define FAST_DISPATCH(TARGET) asm volatile ("b " #TARGET)
#endif
#if defined(__i386__) || defined(__x86_64__)
#define FAST_DISPATCH(TARGET) asm volatile ("jmp " #TARGET)
#endif

extern "C" void OPPROTO op_execute(uint8 *entry_point, basic_cpu *this_cpu);
void OPPROTO op_execute(uint8 *entry_point, basic_cpu *this_cpu)
{
	typedef void (*func_t)(void);
	func_t func = (func_t)entry_point;
#ifdef REG_CPU
	volatile uintptr saved_CPU = (uintptr)CPU;
	CPU = this_cpu;
#endif
#ifdef REG_A0
	volatile uintptr saved_A0 = reg_A0;
#endif
#ifdef REG_T0
	volatile uintptr saved_T0 = reg_T0;
#endif
#ifdef REG_T1
	volatile uintptr saved_T1 = reg_T1;
#endif
#ifdef REG_T2
	volatile uintptr saved_T2 = reg_T2;
#endif
#ifdef REG_T3
	volatile uintptr saved_T3 = reg_T3;
#endif
	SLOW_DISPATCH(entry_point);
	func(); // NOTE: never called, fake to make compiler save return point
	asm volatile (".section \".data\"");
	asm volatile (".global op_exec_return_offset");
	asm volatile ("op_exec_return_offset:");
	asm volatile (".long 1f-op_execute");
	asm volatile (".size op_exec_return_offset,.-op_exec_return_offset");
	asm volatile (".previous");
	asm volatile ("1:");
#ifdef REG_T3
	reg_T3 = saved_T3;
#endif
#ifdef REG_T2
	reg_T2 = saved_T2;
#endif
#ifdef REG_T1
	reg_T1 = saved_T1;
#endif
#ifdef REG_T0
	reg_T0 = saved_T0;
#endif
#ifdef REG_A0
	reg_A0 = saved_A0;
#endif
#ifdef REG_CPU
	CPU = (basic_cpu *)saved_CPU;
#endif
}

void OPPROTO op_jmp_slow(void)
{
	SLOW_DISPATCH(PARAM1);
}

void OPPROTO op_jmp_fast(void)
{
#ifdef FAST_DISPATCH
	FAST_DISPATCH(__op_param1);
#else
	SLOW_DISPATCH(PARAM1);
#endif
}

// Register calling conventions based arches don't need a stack frame
#if defined(__powerpc__) || defined(__x86_64__)
#define DEFINE_OP(NAME, CODE)											\
static void OPPROTO impl_##NAME(void)									\
{																		\
	asm volatile (#NAME ":");											\
	CODE;																\
	FORCE_RET();														\
	asm volatile ("." #NAME ":");										\
	asm volatile (".size " #NAME ",." #NAME "-" #NAME);					\
}																		\
void OPPROTO helper_##NAME(void) __attribute__((weak, alias(#NAME)));
#else
#define DEFINE_OP(NAME, CODE)					\
void OPPROTO NAME(void)							\
{												\
	CODE;										\
}
#endif

#define CALL(CALL_CODE) CALL_CODE

DEFINE_OP(op_invoke, {
	typedef void (*func_t)(void);
	func_t func = (func_t)reg_A0;
	CALL(func());
});

DEFINE_OP(op_invoke_T0, {
	typedef void (*func_t)(uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(T0));
});

DEFINE_OP(op_invoke_im, {
	typedef void (*func_t)(uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(PARAM1));
});

DEFINE_OP(op_invoke_CPU, {
	typedef void (*func_t)(void *);
	func_t func = (func_t)reg_A0;
	CALL(func(CPU));
});

DEFINE_OP(op_invoke_CPU_T0, {
	typedef void (*func_t)(void *, uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(CPU, T0));
});

DEFINE_OP(op_invoke_CPU_im, {
	typedef void (*func_t)(void *, uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(CPU, PARAM1));
});

DEFINE_OP(op_invoke_direct, {
	typedef void (*func_t)(void);
	func_t func = (func_t)PARAM1;
	CALL(func());
});

DEFINE_OP(op_invoke_direct_T0, {
	typedef void (*func_t)(uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(T0));
});

DEFINE_OP(op_invoke_direct_im, {
	typedef void (*func_t)(uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(PARAM2));
});

DEFINE_OP(op_invoke_direct_CPU, {
	typedef void (*func_t)(void *);
	func_t func = (func_t)PARAM1;
	CALL(func(CPU));
});

DEFINE_OP(op_invoke_direct_CPU_T0, {
	typedef void (*func_t)(void *, uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(CPU, T0));
});

DEFINE_OP(op_invoke_direct_CPU_im, {
	typedef void (*func_t)(void *, uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(CPU, PARAM2));
});

#undef DEFINE_OP
