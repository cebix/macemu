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
#define AREG0 "r27"
  AREG0_ID = 27,

#define AREG1 "r24"
  AREG1_ID = 24,

#define AREG2 "r25"
  AREG2_ID = 25,

#define AREG3 "r26"
  AREG3_ID = 26,

#define AREG4 "r16"
  AREG4_ID = 16,

#define AREG5 "r17"
  AREG5_ID = 17,

#define AREG6 "r18"
  AREG6_ID = 18,

#define AREG7 "r19"
  AREG7_ID = 19,

#define AREG8 "r20"
  AREG8_ID = 20,

#define AREG9 "r21"
  AREG9_ID = 21,

#define AREG10 "r22"
  AREG10_ID = 22,

#define AREG11 "r23"
  AREG11_ID = 23,

#define FREG0 "f1"
  FREG0_ID = 1,

#define FREG1 "f2"
  FREG1_ID = 2,

#define FREG2 "f3"
  FREG2_ID = 3,
};

#endif /* DYNGEN_TARGET_EXEC_H */
