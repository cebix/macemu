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

#ifndef DYNGEN_TARGET_EXEC_H
#define DYNGEN_TARGET_EXEC_H

enum {
  /* callee save registers */
#define AREG0 "rbp"
  AREG0_ID = 5,

#define AREG1 "rbx"
  AREG1_ID = 3,

#define AREG2 "r12"
  AREG2_ID = 12,

#define AREG3 "r13"
  AREG3_ID = 13,

#define AREG4 "r14"
  AREG4_ID = 14,

#define AREG5 "r15"
  AREG5_ID = 15,

  // NOTE: the following XMM registers definitions require to build
  // *-dyngen-ops.cpp with -ffixed-xmmN

  /* floating-point registers */
#define FREG0 "xmm4"
  FREG0_ID = 4,

#define FREG1 "xmm5"
  FREG1_ID = 5,

#define FREG2 "xmm6"
  FREG2_ID = 6,

#define FREG3 "xmm7"
  FREG3_ID = 7,

  /* vector registers -- aliased to FP registers, not intermixed */
#define VREG0			FREG0
#define VREG0_ID		FREG0_ID
#define VREG1			FREG1
#define VREG1_ID		FREG1_ID
#define VREG2			FREG2
#define VREG2_ID		FREG2_ID
#define VREG3			FREG3
#define VREG3_ID		FREG3_ID
};

#endif /* DYNGEN_TARGET_EXEC_H */
