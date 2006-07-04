/*
 *  mathlib.cpp - Math library wrapper
 *  Code largely derived from GNU libc
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
#include "mathlib/mathlib.hpp"

#include <stdio.h>
#include <stdlib.h>


/**
 *		Arch-dependent optimizations
 **/

#if defined(__i386__)
#include "mathlib/mathlib-i386.cpp"
#endif


/**
 *		Helper functions
 **/

static void unimplemented(const char *function)
{
	fprintf(stderr, "MATHLIB: unimplemented function '%s', aborting execution\n", function);
	abort();
}


/**
 *		7.12.3.1  The fpclassify macro
 **/

int mathlib_fpclassifyf (float x)
{
	uint32 wx;
	int retval = FP_NORMAL;

	MATHLIB_GET_FLOAT_WORD (wx, x);
	wx &= 0x7fffffff;
	if (wx == 0)
		retval = FP_ZERO;
	else if (wx < 0x800000)
		retval = FP_SUBNORMAL;
	else if (wx >= 0x7f800000)
		retval = wx > 0x7f800000 ? FP_NAN : FP_INFINITE;

	return retval;
}

int mathlib_fpclassify (double x)
{
	uint32 hx, lx;
	int retval = FP_NORMAL;

	MATHLIB_EXTRACT_WORDS (hx, lx, x);
	lx |= hx & 0xfffff;
	hx &= 0x7ff00000;
	if ((hx | lx) == 0)
		retval = FP_ZERO;
	else if (hx == 0)
		retval = FP_SUBNORMAL;
	else if (hx == 0x7ff00000)
		retval = lx != 0 ? FP_NAN : FP_INFINITE;

	return retval;
}

int mathlib_fpclassifyl(long double x)
{
	unimplemented("fpclassifyl");
}


/**
 *		7.12.3.6  The signbit macro
 **/

int mathlib_signbitf (float x)
{
	int32 hx;

	MATHLIB_GET_FLOAT_WORD (hx, x);
	return hx & 0x80000000;
}

int mathlib_signbit (double x)
{
	int32 hx;

	MATHLIB_GET_HIGH_WORD (hx, x);
	return hx & 0x80000000;
}

int mathlib_signbitl(long double x)
{
	unimplemented("signbitl");
}


/**
 *		7.12.9.6  The round functions
 **/

float mathlib_roundf(float x)
{
	int32 i0, j0;
	static const float huge = 1.0e30;

	MATHLIB_GET_FLOAT_WORD (i0, x);
	j0 = ((i0 >> 23) & 0xff) - 0x7f;
	if (j0 < 23) {
		if (j0 < 0) {
			if (huge + x > 0.0F) {
				i0 &= 0x80000000;
				if (j0 == -1)
					i0 |= 0x3f800000;
			}
		}
		else {
			u_int32_t i = 0x007fffff >> j0;
			if ((i0 & i) == 0)
				/* X is integral.  */
				return x;
			if (huge + x > 0.0F) {
				/* Raise inexact if x != 0.  */
				i0 += 0x00400000 >> j0;
				i0 &= ~i;
			}
		}
    }
	else {
		if (j0 == 0x80)
			/* Inf or NaN.  */
			return x + x;
		else
			return x;
    }

	MATHLIB_SET_FLOAT_WORD (x, i0);
	return x;
}
