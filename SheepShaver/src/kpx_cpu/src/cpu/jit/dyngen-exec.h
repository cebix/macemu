/*
 *  dyngen defines for micro operation code
 *
 *  Copyright (c) 2003-2004-2004 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DYNGEN_EXEC_H
#define DYNGEN_EXEC_H

#include "cpu/jit/jit-config.hpp"

#define _JIT_HEADER dyngen-target-exec.h
#include "cpu/jit/jit-target-dispatch.h"

/* define virtual register set */
#define REG_CPU			AREG0
#define REG_CPU_ID		AREG0_ID
#define REG_T0			AREG1
#define REG_T0_ID		AREG1_ID
#define REG_T1			AREG2
#define REG_T1_ID		AREG2_ID
#define REG_T2			AREG3
#define REG_T2_ID		AREG3_ID
#ifdef  AREG4
#define REG_T3			AREG4
#define REG_T3_ID		AREG4_ID
#endif
#ifdef  FREG3
#define REG_F0			FREG0
#define REG_F0_ID		FREG0_ID
#define REG_F1			FREG1
#define REG_F1_ID		FREG1_ID
#define REG_F2			FREG2
#define REG_F2_ID		FREG2_ID
#define REG_F3			FREG3
#define REG_F3_ID		FREG3_ID
#endif
#ifdef  VREG3
#define REG_V0			VREG0
#define REG_V0_ID		VREG0_ID
#define REG_V1			VREG1
#define REG_V1_ID		VREG1_ID
#define REG_V2			VREG2
#define REG_V2_ID		VREG2_ID
#define REG_V3			VREG3
#define REG_V3_ID		VREG3_ID
#endif

// Force only one return point
#define dyngen_barrier() asm volatile ("")

#ifndef OPPROTO
#define OPPROTO
#endif

#ifdef __alpha__
/* the symbols are considered non exported so a br immediate is generated */
#define __hidden __attribute__((visibility("hidden")))
#else
#define __hidden 
#endif

#ifdef __alpha__
/* Suggested by Richard Henderson. This will result in code like
        ldah $0,__op_PARAM1($29)        !gprelhigh
        lda $0,__op_PARAM1($0)          !gprellow
   We can then conveniently change $29 to $31 and adapt the offsets to
   emit the appropriate constant.  */
#define PARAM1 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_PARAM1)); _r; })
#define PARAM2 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_PARAM2)); _r; })
#define PARAM3 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_PARAM3)); _r; })
extern int __op_PARAM1 __hidden;
extern int __op_PARAM2 __hidden;
extern int __op_PARAM3 __hidden;
#elif defined __mips__
/* On MIPS, parameters to a C expression are passed via the global pointer.
 * We don't want that. */
#define PARAMN(index) ({ register int _r; \
		asm("lui %0,%%hi(__op_PARAM" #index ")\n\t" \
		    "ori %0,%0,%%lo(__op_PARAM" #index ")" \
		    : "=r"(_r)); _r; })
#define PARAM1 PARAMN(1)
#define PARAM2 PARAMN(2)
#define PARAM3 PARAMN(3)
#else
#if defined(__APPLE__) && defined(__MACH__)
static int __op_PARAM1, __op_PARAM2, __op_PARAM3;
#else
extern int __op_PARAM1, __op_PARAM2, __op_PARAM3;
#endif
#define PARAM1 ((long)(&__op_PARAM1))
#define PARAM2 ((long)(&__op_PARAM2))
#define PARAM3 ((long)(&__op_PARAM3))
#endif

// Direct block chaining support
#if defined(__powerpc__) || defined(__ppc__)
#define DYNGEN_FAST_DISPATCH(TARGET) asm volatile ("b " ASM_NAME(TARGET))
#endif
#if defined(__i386__) || defined(__x86_64__)
#define DYNGEN_FAST_DISPATCH(TARGET) asm volatile ("jmp " ASM_NAME(TARGET))
#endif

#define DYNGEN_SLOW_DISPATCH(TARGET) do {								\
	static const void __attribute__((unused)) *label1 = &&dummy_label1;	\
	static const void __attribute__((unused)) *label2 = &&dummy_label2;	\
	goto *((void *)TARGET);												\
  dummy_label1:															\
  dummy_label2:															\
	dyngen_barrier();													\
} while (0)

extern int __op_jmp0, __op_jmp1;

// Sections handling
#if defined(__CYGWIN__) || defined(_WIN32)
#define ASM_DATA_SECTION		".section .data\n"
#define ASM_PREVIOUS_SECTION	".section .text\n"
#define ASM_GLOBAL				".global"
#define ASM_NAME(NAME)			"_" #NAME
#define ASM_SIZE(NAME)			""
#elif defined(__APPLE__) && defined(__MACH__)
#define ASM_DATA_SECTION		".data\n"
#define ASM_PREVIOUS_SECTION	".text\n"
#define ASM_GLOBAL				".globl"
#define ASM_NAME(NAME)			"_" #NAME
#define ASM_SIZE(NAME)			""
#if defined(__ppc__)
#define ASM_OP_EXEC_RETURN_INSN	"0x18,0xde,0xad,0xff"
#endif
#if defined(__i386__)
#define ASM_OP_EXEC_RETURN_INSN "0x0f,0xa6,0xf0"
#endif
#elif defined __sgi && defined __mips
#define ASM_DATA_SECTION		".data\n"
#define ASM_PREVIOUS_SECTION	".text\n"
#define ASM_GLOBAL				".globl"
#define ASM_NAME(NAME)			#NAME
#define ASM_SIZE(NAME)			""
#define ASM_LONG				".word"
#else
#define ASM_DATA_SECTION		".section \".data\"\n"
#define ASM_PREVIOUS_SECTION	".previous\n"
#define ASM_GLOBAL				".global"
#define ASM_NAME(NAME)			#NAME
#define ASM_SIZE(NAME)			".size " ASM_NAME(NAME) ",.-" ASM_NAME(NAME)
#endif
#ifndef ASM_LONG
#define ASM_LONG				".long"
#endif

// Helper macros to annotate likely branch directions
#if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#ifndef likely
#define likely(x)		__builtin_expect((x),1)
#endif
#ifndef unlikely
#define unlikely(x)		__builtin_expect((x),0)
#endif
#endif
#ifndef likely
#define likely(x)		(x)
#endif
#ifndef unlikely
#define unlikely(x)		(x)
#endif

#endif /* DYNGEN_EXEC_H */
