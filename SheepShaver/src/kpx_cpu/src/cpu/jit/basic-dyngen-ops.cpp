/*
 *  dyngen-ops.hpp - Synthetic opcodes
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
#define DYNGEN_DEFINE_GLOBAL_REGISTER(REG) \
register uintptr reg_##REG asm(REG_##REG); \
register uint32 REG asm(REG_##REG)
DYNGEN_DEFINE_GLOBAL_REGISTER(A0);
DYNGEN_DEFINE_GLOBAL_REGISTER(T0);
DYNGEN_DEFINE_GLOBAL_REGISTER(T1);
DYNGEN_DEFINE_GLOBAL_REGISTER(T2);
#ifdef REG_T3
DYNGEN_DEFINE_GLOBAL_REGISTER(T3);
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
DEFINE_OP(mov_32_T0_T2, T0 = T2);
DEFINE_OP(mov_32_T0_A0, T0 = A0);
DEFINE_OP(mov_32_T1_im, T1 = PARAM1);
DEFINE_OP(mov_32_T1_T0, T1 = T0);
DEFINE_OP(mov_32_T1_T2, T1 = T2);
DEFINE_OP(mov_32_T1_A0, T1 = A0);
DEFINE_OP(mov_32_T2_im, T2 = PARAM1);
DEFINE_OP(mov_32_T2_T1, T2 = T1);
DEFINE_OP(mov_32_T2_T0, T2 = T0);
DEFINE_OP(mov_32_T2_A0, T2 = A0);
DEFINE_OP(mov_32_A0_im, A0 = PARAM1);
DEFINE_OP(mov_32_A0_T0, A0 = T0);
DEFINE_OP(mov_32_A0_T1, A0 = T1);
DEFINE_OP(mov_32_A0_T2, A0 = T2);
DEFINE_OP(mov_32_T0_0,  T0 = 0);
DEFINE_OP(mov_32_T1_0,  T1 = 0);
DEFINE_OP(mov_32_T2_0,  T2 = 0);
DEFINE_OP(mov_32_A0_0,  A0 = 0);

#if SIZEOF_VOID_P == 8
#if defined(__x86_64__)
#define DEFINE_MOV_AD(REG) asm volatile ("movabsq $__op_param1,%" REG_##REG)
#else
#error "unsupported 64-bit value move in"
#endif
#else
#define DEFINE_MOV_AD(REG) REG = PARAM1
#endif

void OPPROTO op_mov_ad_T0_im(void) { DEFINE_MOV_AD(T0); }
void OPPROTO op_mov_ad_T1_im(void) { DEFINE_MOV_AD(T1); }
void OPPROTO op_mov_ad_T2_im(void) { DEFINE_MOV_AD(T2); }
void OPPROTO op_mov_ad_A0_im(void) { DEFINE_MOV_AD(A0); }

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
DEFINE_OP(se_16_32_T1, T1 = (int32)(int16)T1);
DEFINE_OP(ze_16_32_T0, T0 = (uint32)(uint16)T0);
DEFINE_OP(se_8_32_T0, T0 = (int32)(int8)T0);
DEFINE_OP(ze_8_32_T0, T0 = (uint32)(uint8)T0);

#undef DEFINE_OP


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
	asm volatile (ASM_DATA_SECTION);
	asm volatile (".global " ASM_NAME(op_exec_return_offset));
	asm volatile (ASM_NAME(op_exec_return_offset) ":");
	asm volatile (".long 1f-" ASM_NAME(op_execute));
	asm volatile (ASM_SIZE(op_exec_return_offset));
	asm volatile (ASM_PREVIOUS_SECTION);
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
#ifdef DYNGEN_FAST_DISPATCH
	DYNGEN_FAST_DISPATCH(__op_param1);
#else
	SLOW_DISPATCH(PARAM1);
#endif
}

void OPPROTO op_jmp_A0(void)
{
	SLOW_DISPATCH(reg_A0);
}

// Register calling conventions based arches don't need a stack frame
#if defined(__powerpc__) || defined(__x86_64__)
#define DEFINE_OP(NAME, CODE)											\
static void OPPROTO impl_##NAME(void) __attribute__((used));			\
void OPPROTO impl_##NAME(void)											\
{																		\
	asm volatile (#NAME ":");											\
	CODE;																\
	FORCE_RET();														\
	asm volatile ("." #NAME ":");										\
	asm volatile (ASM_SIZE(NAME));										\
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

DEFINE_OP(op_invoke_T0_T1, {
	typedef void (*func_t)(uint32, uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(T0, T1));
});

DEFINE_OP(op_invoke_T0_T1_T2, {
	typedef void (*func_t)(uint32, uint32, uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(T0, T1, T2));
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

DEFINE_OP(op_invoke_CPU_im_im, {
	typedef void (*func_t)(void *, uint32, uint32);
	func_t func = (func_t)reg_A0;
	CALL(func(CPU, PARAM1, PARAM2));
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

DEFINE_OP(op_invoke_direct_T0_T1, {
	typedef void (*func_t)(uint32, uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(T0, T1));
});

DEFINE_OP(op_invoke_direct_T0_T1_T2, {
	typedef void (*func_t)(uint32, uint32, uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(T0, T1, T2));
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

DEFINE_OP(op_invoke_direct_CPU_im_im, {
	typedef void (*func_t)(void *, uint32, uint32);
	func_t func = (func_t)PARAM1;
	CALL(func(CPU, PARAM2, PARAM3));
});

DEFINE_OP(op_invoke_CPU_T0_ret_A0, {
	typedef void *(*func_t)(void *, uintptr);
	func_t func = (func_t)reg_A0;
	reg_A0 = (uintptr)CALL(func(CPU, reg_T0));
});

DEFINE_OP(op_invoke_direct_CPU_T0_ret_A0, {
	typedef void *(*func_t)(void *, uintptr);
	func_t func = (func_t)PARAM1;
	reg_A0 = (uintptr)CALL(func(CPU, reg_T0));
});

#undef DEFINE_OP
