/*
 *  fpu/mathlib.cpp - Floating-point math support library
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *  
 *  Original UAE FPU, copyright 1996 Herman ten Brugge
 *  Rewrite for x86, copyright 1999-2001 Lauri Pesonen
 *  New framework, copyright 2000-2001 Gwenole Beauchesne
 *  Adapted for JIT compilation (c) Bernd Meyer, 2000-2001
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

/* NOTE: this file shall be included only from fpu/fpu_*.cpp */
#undef	PRIVATE
#define	PRIVATE static

#undef	PUBLIC
#define	PUBLIC	/**/

#undef	FFPU
#define	FFPU	/**/

#undef	FPU
#define	FPU		fpu.

#if defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY)

PRIVATE fpu_extended fp_do_pow(fpu_extended x, fpu_extended y)
{
	fpu_extended value, exponent;
	uae_s64 p = (uae_s64)y;
	
	if (x == 0.0) {
		if (y > 0.0)
			return (y == (double) p && (p & 1) != 0 ? x : 0.0);
		else if (y < 0.0)
			return (y == (double) p && (-p & 1) != 0 ? 1.0 / x : 1.0 / fp_fabs (x));
    }
	
	if (y == (double) p) {
		fpu_extended r = 1.0;
		if (p == 0)
			return 1.0;
		if (p < 0) {
			p = -p;
			x = 1.0 / x;
		}
		while (1) {
			if (p & 1)
				r *= x;
			p >>= 1;
			if (p == 0)
				return r;
			x *= x;
		}
    }
	
	__asm__ __volatile__("fyl2x" : "=t" (value) : "0" (x), "u" (1.0) : "st(1)");
	__asm__ __volatile__("fmul		%%st(1)		# y * log2(x)\n\t"
				 "fst		%%st(1)\n\t"
				 "frndint				# int(y * log2(x))\n\t"
				 "fxch\n\t"
				 "fsub		%%st(1)		# fract(y * log2(x))\n\t"
				 "f2xm1					# 2^(fract(y * log2(x))) - 1\n\t"
				 : "=t" (value), "=u" (exponent) : "0" (y), "1" (value));
	value += 1.0;
	__asm__ __volatile__("fscale" : "=t" (value) : "0" (value), "u" (exponent));
	return value;
}

PRIVATE fpu_extended fp_do_log1p(fpu_extended x)
{
	// TODO: handle NaN and +inf/-inf
	fpu_extended value;
	// The fyl2xp1 can only be used for values in
	//   -1 + sqrt(2) / 2 <= x <= 1 - sqrt(2) / 2
	// 0.29 is a safe value.
	if (fp_fabs(x) <= 0.29)
		__asm__ __volatile__("fldln2; fxch; fyl2xp1" : "=t" (value) : "0" (x));
	else
		__asm__ __volatile__("fldln2; fxch; fyl2x" : "=t" (value) : "0" (x + 1.0));
	return value;
}

#endif
