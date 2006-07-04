/*
 *  mathlib-ppc.hpp - Math library wrapper, ppc specific code
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

#ifndef MATHLIB_PPC_H
#define MATHLIB_PPC_H

// Floating-Point Multiply Add
#if defined __GNUC__
static inline double mathlib_fmadd(double x, double y, double z)
{
	double r;
	__asm__ __volatile__ ("fmadd %0,%1,%2,%3" : "=f" (r) : "f" (x), "f" (y) , "f" (z));
	return r;
}

static inline float mathlib_fmadd(float x, float y, float z)
{
	float r;
	__asm__ __volatile__ ("fmadds %0,%1,%2,%3" : "=f" (r) : "f" (x), "f" (y) , "f" (z));
	return r;
}

#define mathlib_fmadd(x, y, z) (mathlib_fmadd)(x, y, z)
#endif

// Floating-Point Multiply Subtract
#if defined __GNUC__
static inline double mathlib_fmsub(double x, double y, double z)
{
	double r;
	__asm__ __volatile__ ("fmsub %0,%1,%2,%3" : "=f" (r) : "f" (x), "f" (y) , "f" (z));
	return r;
}

static inline float mathlib_fmsub(float x, float y, float z)
{
	float r;
	__asm__ __volatile__ ("fmsubs %0,%1,%2,%3" : "=f" (r) : "f" (x), "f" (y) , "f" (z));
	return r;
}

#define mathlib_fmsub(x, y, z) (mathlib_fmsub)(x, y, z)
#endif

#endif /* MATHLIB_PPC_H */
