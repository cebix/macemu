/*
 *  dyngen defines for micro operation code
 *
 *  Copyright (c) 2003 Fabrice Bellard
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
#include JIT_TARGET_INCLUDE(dyngen-target-exec.h)

/* define virtual register set */
#define REG_A0			AREG0
#define REG_A0_ID		AREG0_ID
#define REG_T0			AREG1
#define REG_T0_ID		AREG1_ID
#define REG_T1			AREG2
#define REG_T1_ID		AREG2_ID
#ifdef  AREG3
#define REG_T2			AREG3
#define REG_T2_ID		AREG3_ID
#endif
#ifdef  AREG4
#define REG_T3			AREG4
#define REG_T3_ID		AREG4_ID
#endif
#ifdef  AREG5
#define REG_CPU			AREG5
#define REG_CPU_ID		AREG5_ID
#endif
#ifdef  FREG0
#define REG_F0			FREG0
#define REG_F0_ID		FREG0_ID
#endif
#ifdef  FREG1
#define REG_F1			FREG1
#define REG_F1_ID		FREG1_ID
#endif
#ifdef  FREG2
#define REG_F2			FREG2
#define REG_F2_ID		FREG2_ID
#endif
#ifdef  FREG3
#define REG_F3			FREG3
#define REG_F3_ID		FREG3_ID
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
        ldah $0,__op_param1($29)        !gprelhigh
        lda $0,__op_param1($0)          !gprellow
   We can then conveniently change $29 to $31 and adapt the offsets to
   emit the appropriate constant.  */
#define PARAM1 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param1)); _r; })
#define PARAM2 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param2)); _r; })
#define PARAM3 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param3)); _r; })
extern int __op_param1 __hidden;
extern int __op_param2 __hidden;
extern int __op_param3 __hidden;
#else
extern int __op_param1, __op_param2, __op_param3;
#define PARAM1 ((long)(&__op_param1))
#define PARAM2 ((long)(&__op_param2))
#define PARAM3 ((long)(&__op_param3))
#endif

#ifndef REG_CPU
extern int __op_cpuparam;
#define CPUPARAM ((long)(&__op_cpuparam))
#endif

extern int __op_jmp0, __op_jmp1;

#endif /* DYNGEN_EXEC_H */
