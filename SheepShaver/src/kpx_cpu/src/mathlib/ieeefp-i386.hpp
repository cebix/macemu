/*
 *  ieeefp-i386.hpp - IEEE754 Floating-Point Math library, x86 specific code
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

#ifndef IEEEFP_I386_H
#define IEEEFP_I386_H

// 7.6  Floating-point environment <fenv.h>
#ifndef HAVE_FENV_H

// Exceptions
enum {
    FE_INVALID = 0x01,
#define FE_INVALID	FE_INVALID
    FE_DIVBYZERO = 0x04,
#define FE_DIVBYZERO	FE_DIVBYZERO
    FE_OVERFLOW = 0x08,
#define FE_OVERFLOW	FE_OVERFLOW
    FE_UNDERFLOW = 0x10,
#define FE_UNDERFLOW	FE_UNDERFLOW
    FE_INEXACT = 0x20
#define FE_INEXACT	FE_INEXACT
};

#define FE_ALL_EXCEPT	(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

// Rounding modes
enum {
    FE_TONEAREST = 0,
#define FE_TONEAREST	FE_TONEAREST
    FE_DOWNWARD = 0x400,
#define FE_DOWNWARD	FE_DOWNWARD
    FE_UPWARD = 0x800,
#define FE_UPWARD	FE_UPWARD
    FE_TOWARDZERO = 0xc00
#define FE_TOWARDZERO	FE_TOWARDZERO
};

#endif

#endif /* IEEEFP_I386_H */
