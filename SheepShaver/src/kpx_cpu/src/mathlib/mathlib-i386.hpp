/*
 *  mathlib-i386.hpp - Math library wrapper, x86 specific code
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

#ifndef MATHLIB_I386_H
#define MATHLIB_I386_H

// 7.12.9.5  The lrint and llrint functions
#if defined(__GNUC__)
#define mathlib_lrint(x)													\
({ long int __result;														\
   __asm__ __volatile__ ("fistpl %0" : "=m" (__result) : "t" (x) : "st");	\
  __result; })
#endif

// 7.12.14  Comparison macros
#if defined(__GNUC__)
#ifndef isless
#define isless(x, y)														\
({ register char __result;													\
   __asm__ ("fucompp; fnstsw; testb $0x45, %%ah; setz %%al"					\
			: "=a" (__result) : "u" (x), "t" (y) : "cc", "st", "st(1)");	\
   __result; })
#endif

#ifndef isgreater
#define isgreater(x, y)														\
({ register char __result;													\
   __asm__ ("fucompp; fnstsw; testb $0x45, %%ah; setz %%al"					\
			: "=a" (__result) : "u" (y), "t" (x) : "cc", "st", "st(1)");	\
   __result; })
#endif
#endif

#endif /* MATHLIB_I386_H */
