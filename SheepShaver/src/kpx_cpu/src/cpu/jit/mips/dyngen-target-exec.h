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
#define AREG0 "s0"
  AREG0_ID = 16,

#define AREG1 "s1"
  AREG1_ID = 17,

#define AREG2 "s2"
  AREG2_ID = 18,

#define AREG3 "s3"
  AREG3_ID = 19,

#define AREG4 "s4"
  AREG4_ID = 20,

#define AREG5 "s5"
  AREG5_ID = 21,
};

#endif /* DYNGEN_TARGET_EXEC_H */
