/*
 *  ieee-mips.hpp - IEE754 Floating-Point Math library, mips specific code
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
 *  Code derived from the GNU C Library
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

#ifndef IEEEFP_MIPS_H
#define IEEEFP_MIPS_H

// 7.6  Floating-point environment <fenv.h>
#ifndef USE_FENV_H

// Exceptions
enum {
    FE_INEXACT = 0x04,
#define FE_INEXACT		FE_INEXACT
    FE_UNDERFLOW = 0x08,
#define FE_UNDERFLOW	FE_UNDERFLOW
    FE_OVERFLOW = 0x10,
#define FE_OVERFLOW		FE_OVERFLOW
    FE_DIVBYZERO = 0x20,
#define FE_DIVBYZERO	FE_DIVBYZERO
    FE_INVALID = 0x40,
#define FE_INVALID		FE_INVALID
};

#define FE_ALL_EXCEPT	(FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW | FE_INVALID)

// Rounding modes
enum {
  FE_TONEAREST = 0x0,
#define FE_TONEAREST	FE_TONEAREST
  FE_TOWARDZERO = 0x1,
#define FE_TOWARDZERO	FE_TOWARDZERO
  FE_UPWARD = 0x2,
#define FE_UPWARD		FE_UPWARD
    FE_DOWNWARD = 0x3
#define FE_DOWNWARD		FE_DOWNWARD
};

#endif

#endif /* IEEEFP_MIPS_H */
