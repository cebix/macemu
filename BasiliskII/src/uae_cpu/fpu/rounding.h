/*
 *  fpu/rounding.h - system-dependant FPU rounding mode and precision
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *  
 *  Original UAE FPU, copyright 1996 Herman ten Brugge
 *  Rewrite for x86, copyright 1999-2000 Lauri Pesonen
 *  New framework, copyright 2000 Gwenole Beauchesne
 *  Adapted for JIT compilation (c) Bernd Meyer, 2000
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

#ifndef FPU_ROUNDING_H
#define FPU_ROUNDING_H

/* NOTE: this file shall be included from fpu/fpu_*.cpp */
#undef	PUBLIC
#define PUBLIC	extern

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU	fpu.

/* Defaults to generic rounding mode and precision handling */
#define FPU_USE_GENERIC_ROUNDING_MODE
#define FPU_USE_GENERIC_ROUNDING_PRECISION

/* -------------------------------------------------------------------------- */
/* --- Selection of floating-point rounding mode and precision            --- */
/* -------------------------------------------------------------------------- */

/* Optimized i386 fpu core must use native rounding mode */
#if defined(FPU_X86) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_ROUNDING_MODE
# define FPU_USE_X86_ROUNDING_MODE
#endif

/* Optimized i386 fpu core must use native rounding precision */
#if defined(FPU_X86) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_ROUNDING_PRECISION
# define FPU_USE_X86_ROUNDING_PRECISION
#endif

#if 0 // gb-- FIXME: that doesn't work
/* IEEE-based fpu core can have native rounding mode on i386 */
#if defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_ROUNDING_MODE
# define FPU_USE_X86_ROUNDING_MODE
#endif

/* IEEE-based fpu core can have native rounding precision on i386 */
#if defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_ROUNDING_PRECISION
# define FPU_USE_X86_ROUNDING_PRECISION
#endif
#endif

/* -------------------------------------------------------------------------- */
/* --- Sanity checks                                                      --- */
/* -------------------------------------------------------------------------- */

/* X86 rounding mode and precision work together */
#if defined(FPU_USE_X86_ROUNDING_MODE) && defined(FPU_USE_X86_ROUNDING_PRECISION)
# define FPU_USE_X86_ROUNDING
# define CW_INITIAL (CW_RESET|CW_X|CW_PC_EXTENDED|CW_RC_NEAR|CW_PM|CW_UM|CW_OM|CW_ZM|CW_DM|CW_IM)
  PRIVATE uae_u32 x86_control_word;
#endif

/* Control word -- rounding mode */
#ifdef FPU_USE_X86_ROUNDING_MODE
PUBLIC const uae_u32 x86_control_word_rm_mac2host[];
#endif

/* Control word -- rounding precision */
#ifdef FPU_USE_X86_ROUNDING_PRECISION
PUBLIC const uae_u32 x86_control_word_rp_mac2host[];
#endif

#if defined(FPU_USE_X86_ROUNDING_MODE) && defined(FPU_USE_X86_ROUNDING_PRECISION)
/* Set host control word for rounding mode and rounding precision */
PRIVATE inline void set_host_control_word(void)
{
	/*
		Exception enable byte is ignored, but the same value is returned
		that was previously set.
	*/
	x86_control_word
		= (x86_control_word & ~(X86_ROUNDING_MODE|X86_ROUNDING_PRECISION))
		| x86_control_word_rm_mac2host[(FPU fpcr.rounding_mode & FPCR_ROUNDING_MODE) >> 4]
		| x86_control_word_rp_mac2host[(FPU fpcr.rounding_precision & FPCR_ROUNDING_PRECISION) >> 6]
		;
	__asm__ __volatile__("fldcw %0" : : "m" (x86_control_word));
}
#endif

/* -------------------------------------------------------------------------- */
/* --- Generic rounding mode and precision                                --- */
/* -------------------------------------------------------------------------- */

#if defined(FPU_USE_GENERIC_ROUNDING_MODE) && defined(FPU_USE_GENERIC_ROUNDING_PRECISION)
/* Set host control word for rounding mode and rounding precision */
PRIVATE inline void set_host_control_word(void)
	{ }
#endif

/* -------------------------------------------------------------------------- */
/* --- Common rounding mode and precision                                 --- */
/* -------------------------------------------------------------------------- */

#if defined(FPU_USE_GENERIC_ROUNDING_MODE) || defined(FPU_USE_X86_ROUNDING_MODE)

/* Return the current rounding mode in m68k format */
static inline uae_u32 FFPU get_rounding_mode(void)
	{ return FPU fpcr.rounding_mode; }

/* Convert and set to native rounding mode */
static inline void FFPU set_rounding_mode(uae_u32 new_rounding_mode)
	{ FPU fpcr.rounding_mode = new_rounding_mode; }

#endif

#if defined(FPU_USE_GENERIC_ROUNDING_PRECISION) || defined(FPU_USE_X86_ROUNDING_PRECISION)

/* Return the current rounding precision in m68k format */
static inline uae_u32 FFPU get_rounding_precision(void)
	{ return FPU fpcr.rounding_precision; }

/* Convert and set to native rounding precision */
static inline void FFPU set_rounding_precision(uae_u32 new_rounding_precision)
	{ FPU fpcr.rounding_precision = new_rounding_precision; }

#endif

#endif /* FPU_ROUNDING_H */
