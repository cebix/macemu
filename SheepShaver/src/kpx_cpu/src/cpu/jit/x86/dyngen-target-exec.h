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
#define AREG0 "ebp"
  AREG0_ID = 5,

#define AREG1 "ebx"
  AREG1_ID = 3,

#define AREG2 "esi"
  AREG2_ID = 6,

#define AREG3 "edi"
  AREG3_ID = 7,

  // NOTE: the following XMM registers definitions require to build
  // *-dyngen-ops.cpp with -ffixed-xmmN

  /* vector registers */
#define VREG0 "xmm4"
  VREG0_ID = 4,

#define VREG1 "xmm5"
  VREG1_ID = 5,

#define VREG2 "xmm6"
  VREG2_ID = 6,

#define VREG3 "xmm7"
  VREG3_ID = 7,
};

#endif /* DYNGEN_TARGET_EXEC_H */
