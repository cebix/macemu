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
register basic_cpu *CPU asm(REG_CPU);
#define DYNGEN_DEFINE_GLOBAL_REGISTER(REG) \
register uintptr A##REG asm(REG_T##REG); \
register uint32  T##REG asm(REG_T##REG)
DYNGEN_DEFINE_GLOBAL_REGISTER(0);
DYNGEN_DEFINE_GLOBAL_REGISTER(1);
DYNGEN_DEFINE_GLOBAL_REGISTER(2);


/**
 *		Native ALU operations optimization
 **/

#if defined(__i386__) || defined(__x86_64__)
#define do_xchg_32(x, y)		asm volatile ("xchg %0,%1" : "+r" (x), "+r" (y))
#endif
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
#ifndef do_xchg_32
#define do_xchg_32(x, y)		do { uint32 t = x; x = y; y = t; } while (0)
#endif


/**
 *		ALU operations
 **/

// XXX update for new 64-bit arches
#if defined __x86_64__
#define MOV_AD_REG(PARAM, REG) asm volatile ("movabsq $__op_" #PARAM ",%0" : "=r" (REG))
#else
#define MOV_AD_REG(PARAM, REG) REG = PARAM
#endif

#define DEFINE_OP(REG)							\
void OPPROTO op_mov_ad_##REG##_im(void)			\
{												\
	MOV_AD_REG(PARAM1, REG);					\
}

DEFINE_OP(A0);
DEFINE_OP(A1);
DEFINE_OP(A2);

#undef DEFINE_OP

#define DEFINE_OP(NAME, CODE)					\
void OPPROTO op_##NAME(void)					\
{												\
	CODE;										\
}

// Register moves
DEFINE_OP(mov_32_T0_im, T0 = PARAM1);
DEFINE_OP(mov_32_T0_T1, T0 = T1);
DEFINE_OP(mov_32_T0_T2, T0 = T2);
DEFINE_OP(mov_32_T1_im, T1 = PARAM1);
DEFINE_OP(mov_32_T1_T0, T1 = T0);
DEFINE_OP(mov_32_T1_T2, T1 = T2);
DEFINE_OP(mov_32_T2_im, T2 = PARAM1);
DEFINE_OP(mov_32_T2_T1, T2 = T1);
DEFINE_OP(mov_32_T2_T0, T2 = T0);
DEFINE_OP(mov_32_T0_0,  T0 = 0);
DEFINE_OP(mov_32_T1_0,  T1 = 0);
DEFINE_OP(mov_32_T2_0,  T2 = 0);

// Arithmetic operations
DEFINE_OP(add_32_T0_T2, T0 += T2);
DEFINE_OP(add_32_T0_T1, T0 += T1);
DEFINE_OP(add_32_T0_im, T0 += PARAM1);
DEFINE_OP(add_32_T0_1,  T0 += 1);
DEFINE_OP(add_32_T0_2,  T0 += 2);
DEFINE_OP(add_32_T0_4,  T0 += 4);
DEFINE_OP(add_32_T0_8,  T0 += 8);
DEFINE_OP(sub_32_T0_T2, T0 -= T2);
DEFINE_OP(sub_32_T0_T1, T0 -= T1);
DEFINE_OP(sub_32_T0_im, T0 -= PARAM1);
DEFINE_OP(sub_32_T0_1,  T0 -= 1);
DEFINE_OP(sub_32_T0_2,  T0 -= 2);
DEFINE_OP(sub_32_T0_4,  T0 -= 4);
DEFINE_OP(sub_32_T0_8,  T0 -= 8);
DEFINE_OP(add_32_T1_T2, T1 += T2);
DEFINE_OP(add_32_T1_T0, T1 += T0);
DEFINE_OP(add_32_T1_im, T1 += PARAM1);
DEFINE_OP(add_32_T1_1,  T1 += 1);
DEFINE_OP(add_32_T1_2,  T1 += 2);
DEFINE_OP(add_32_T1_4,  T1 += 4);
DEFINE_OP(add_32_T1_8,  T1 += 8);
DEFINE_OP(sub_32_T1_T2, T1 -= T2);
DEFINE_OP(sub_32_T1_T0, T1 -= T0);
DEFINE_OP(sub_32_T1_im, T1 -= PARAM1);
DEFINE_OP(sub_32_T1_1,  T1 -= 1);
DEFINE_OP(sub_32_T1_2,  T1 -= 2);
DEFINE_OP(sub_32_T1_4,  T1 -= 4);
DEFINE_OP(sub_32_T1_8,  T1 -= 8);
DEFINE_OP(umul_32_T0_T1, T0 = (uint32)T0 * (uint32)T1);
DEFINE_OP(smul_32_T0_T1, T0 = (int32)T0 * (int32)T1);
DEFINE_OP(udiv_32_T0_T1, T0 = do_udiv_32(T0, T1));
DEFINE_OP(sdiv_32_T0_T1, T0 = do_sdiv_32(T0, T1));
DEFINE_OP(xchg_32_T0_T1, do_xchg_32(T0, T1));
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
void OPPROTO op_load_u##BITS##_##REG##_T1_##OFFSET(void)			\
{																	\
	REG = (uint32)(uint##BITS)vm_read_memory_##SIZE(T1 + OFFSET);	\
}																	\
void OPPROTO op_load_s##BITS##_##REG##_T1_##OFFSET(void)			\
{																	\
	REG = (int32)(int##BITS)vm_read_memory_##SIZE(T1 + OFFSET);		\
}																	\
void OPPROTO op_store_##BITS##_##REG##_T1_##OFFSET(void)			\
{																	\
	vm_write_memory_##SIZE(T1 + OFFSET, REG);						\
}

DEFINE_OP(32,T0,4,0);
DEFINE_OP(32,T0,4,im);
DEFINE_OP(32,T0,4,T2);
DEFINE_OP(16,T0,2,0);
DEFINE_OP(16,T0,2,im);
DEFINE_OP(16,T0,2,T2);
DEFINE_OP(8,T0,1,0);
DEFINE_OP(8,T0,1,im);
DEFINE_OP(8,T0,1,T2);

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
#ifdef __ppc__
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

extern "C" void OPPROTO op_execute(uint8 *entry_point, basic_cpu *this_cpu);
void OPPROTO op_execute(uint8 *entry_point, basic_cpu *this_cpu)
{
	typedef void (*func_t)(void);
	func_t func = (func_t)entry_point;
	const int n_slots = 16 + 4; /* 16 stack slots + 4 VCPU registers */
	volatile uintptr stk[n_slots];
	stk[n_slots - 1] = (uintptr)CPU;
	stk[n_slots - 2] = A0;
	stk[n_slots - 3] = A1;
	stk[n_slots - 4] = A2;
	CPU = this_cpu;
	DYNGEN_SLOW_DISPATCH(entry_point);
	func(); // NOTE: never called, fake to make compiler save return point
#ifdef  ASM_OP_EXEC_RETURN_INSN
	asm volatile ("1: .byte " ASM_OP_EXEC_RETURN_INSN);
#else
	asm volatile (ASM_DATA_SECTION);
	asm volatile (ASM_GLOBAL " " ASM_NAME(op_exec_return_offset));
	asm volatile (ASM_NAME(op_exec_return_offset) ":");
	asm volatile (ASM_LONG " 1f-" ASM_NAME(op_execute));
	asm volatile (ASM_SIZE(op_exec_return_offset));
	asm volatile (ASM_PREVIOUS_SECTION);
	asm volatile ("1:");
#endif
	A2 = stk[n_slots - 4];
	A1 = stk[n_slots - 3];
	A0 = stk[n_slots - 2];
	CPU = (basic_cpu *)stk[n_slots - 1];
}

void OPPROTO op_jmp_slow(void)
{
	DYNGEN_SLOW_DISPATCH(PARAM1);
}

void OPPROTO op_jmp_fast(void)
{
#ifdef DYNGEN_FAST_DISPATCH
	DYNGEN_FAST_DISPATCH(__op_PARAM1);
#else
	DYNGEN_SLOW_DISPATCH(PARAM1);
#endif
}

void OPPROTO op_jmp_A0(void)
{
	DYNGEN_SLOW_DISPATCH(A0);
}

// Register calling conventions based arches don't need a stack frame
// XXX enable on x86 too because we allocated a frame in op_execute()
#if (defined __APPLE__ && defined __MACH__)
#define DEFINE_OP(NAME, CODE)									\
static void OPPROTO impl_##NAME(void) __attribute__((used));	\
void OPPROTO impl_##NAME(void)									\
{																\
	asm volatile (#NAME ":");									\
	CODE;														\
	FORCE_RET();												\
	asm volatile ("." #NAME ":");								\
	asm volatile (ASM_SIZE(NAME));								\
}																\
extern void OPPROTO NAME(void) __attribute__((weak_import));	\
asm(".set  helper_" #NAME "," #NAME);
#elif defined(__powerpc__) || ((defined(__x86_64__) || defined(__i386__)) && !defined(_WIN32))
// XXX there is a problem on Windows: coff_text_shndx != text_shndx
// The latter is found by searching for ".text" in all symbols and
// assigning its e_scnum.
#define DEFINE_OP(NAME, CODE)									\
static void OPPROTO impl_##NAME(void) __attribute__((used));	\
void OPPROTO impl_##NAME(void)									\
{																\
	asm volatile (#NAME ":");									\
	CODE;														\
	FORCE_RET();												\
	asm volatile ("." #NAME ":");								\
	asm volatile (ASM_SIZE(NAME));								\
}																\
asm(".weak " #NAME);											\
asm(".set  helper_" #NAME "," #NAME);
#else
#define DEFINE_OP(NAME, CODE)					\
extern "C" void OPPROTO NAME(void);				\
void OPPROTO NAME(void)							\
{												\
	CODE;										\
}
#endif

#if defined __mips__
#define CALL(FUNC, ARGS) ({										\
		register uintptr func asm("t9");						\
		asm volatile ("move %0,%1" : "=r" (func) : "r" (FUNC));	\
		((func_t)func) ARGS;									\
})
#else
#define CALL(FUNC, ARGS) ((func_t)FUNC) ARGS
#endif

DEFINE_OP(op_invoke, {
	typedef void (*func_t)(void);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, ());
});

DEFINE_OP(op_invoke_T0, {
	typedef void (*func_t)(uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (T0));
});

DEFINE_OP(op_invoke_T0_T1, {
	typedef void (*func_t)(uint32, uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (T0, T1));
});

DEFINE_OP(op_invoke_T0_T1_T2, {
	typedef void (*func_t)(uint32, uint32, uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (T0, T1, T2));
});

DEFINE_OP(op_invoke_T0_ret_T0, {
	typedef uint32 (*func_t)(uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	T0 = CALL(func, (T0));
});

DEFINE_OP(op_invoke_im, {
	typedef void (*func_t)(uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (PARAM2));
});

DEFINE_OP(op_invoke_CPU, {
	typedef void (*func_t)(void *);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (CPU));
});

DEFINE_OP(op_invoke_CPU_T0, {
	typedef void (*func_t)(void *, uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (CPU, T0));
});

DEFINE_OP(op_invoke_CPU_im, {
	typedef void (*func_t)(void *, uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (CPU, PARAM2));
});

DEFINE_OP(op_invoke_CPU_im_im, {
	typedef void (*func_t)(void *, uint32, uint32);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	CALL(func, (CPU, PARAM2, PARAM3));
});

DEFINE_OP(op_invoke_CPU_A0_ret_A0, {
	typedef void *(*func_t)(void *, uintptr);
	uintptr func;
	MOV_AD_REG(PARAM1, func);
	A0 = (uintptr)CALL(func, (CPU, A0));
});

DEFINE_OP(op_invoke_direct, {
	typedef void (*func_t)(void);
	CALL(PARAM1, ());
});

DEFINE_OP(op_invoke_direct_T0, {
	typedef void (*func_t)(uint32);
	CALL(PARAM1, (T0));
});

DEFINE_OP(op_invoke_direct_T0_T1, {
	typedef void (*func_t)(uint32, uint32);
	CALL(PARAM1, (T0, T1));
});

DEFINE_OP(op_invoke_direct_T0_T1_T2, {
	typedef void (*func_t)(uint32, uint32, uint32);
	CALL(PARAM1, (T0, T1, T2));
});

DEFINE_OP(op_invoke_direct_T0_ret_T0, {
	typedef uint32 (*func_t)(uint32);
	T0 = CALL(PARAM1, (T0));
});

DEFINE_OP(op_invoke_direct_im, {
	typedef void (*func_t)(uint32);
	CALL(PARAM1, (PARAM2));
});

DEFINE_OP(op_invoke_direct_CPU, {
	typedef void (*func_t)(void *);
	CALL(PARAM1, (CPU));
});

DEFINE_OP(op_invoke_direct_CPU_T0, {
	typedef void (*func_t)(void *, uint32);
	CALL(PARAM1, (CPU, T0));
});

DEFINE_OP(op_invoke_direct_CPU_im, {
	typedef void (*func_t)(void *, uint32);
	CALL(PARAM1, (CPU, PARAM2));
});

DEFINE_OP(op_invoke_direct_CPU_im_im, {
	typedef void (*func_t)(void *, uint32, uint32);
	CALL(PARAM1, (CPU, PARAM2, PARAM3));
});

DEFINE_OP(op_invoke_direct_CPU_A0_ret_A0, {
	typedef void *(*func_t)(void *, uintptr);
	A0 = (uintptr)CALL(PARAM1, (CPU, A0));
});

#undef DEFINE_OP
