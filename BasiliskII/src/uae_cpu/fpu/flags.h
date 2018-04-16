/*
 * fpu/flags.h - Floating-point flags
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * MC68881/68040 fpu emulation
 *
 * Original UAE FPU, copyright 1996 Herman ten Brugge
 * Rewrite for x86, copyright 1999-2001 Lauri Pesonen
 * New framework, copyright 2000-2001 Gwenole Beauchesne
 * Adapted for JIT compilation (c) Bernd Meyer, 2000-2001
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef FPU_FLAGS_H
#define FPU_FLAGS_H

/* NOTE: this file shall be included only from fpu/fpu_*.cpp */
#undef	PUBLIC
#define PUBLIC	extern

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* Defaults to generic flags */
#define FPU_USE_GENERIC_FLAGS

/* -------------------------------------------------------------------------- */
/* --- Selection of floating-point flags handling mode                    --- */
/* -------------------------------------------------------------------------- */

/* Optimized i386 fpu core must use native flags */
#if defined(FPU_X86) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_FLAGS
# define FPU_USE_X86_FLAGS
#endif

/* Old UAE FPU core can use native flags */
#if defined(FPU_UAE) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_FLAGS
# define FPU_USE_X86_FLAGS
#endif

/* IEEE-based implementation must use lazy flag evaluation */
#if defined(FPU_IEEE)
# undef FPU_USE_GENERIC_FLAGS
# define FPU_USE_LAZY_FLAGS
#endif

/* JIT Compilation for FPU only works with lazy evaluation of FPU flags */
#if defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY) && defined(USE_JIT_FPU)
# undef FPU_USE_GENERIC_FLAGS
# define FPU_USE_LAZY_FLAGS
#endif

#ifdef FPU_IMPLEMENTATION

/* -------------------------------------------------------------------------- */
/* --- Native X86 Floating-Point Flags                                    --- */
/* -------------------------------------------------------------------------- */

/* FPU_X86 has its own set of lookup functions */

#ifdef FPU_USE_X86_FLAGS

#define FPU_USE_NATIVE_FLAGS

#define NATIVE_FFLAG_NEGATIVE	0x0200
#define NATIVE_FFLAG_ZERO		0x4000
#define NATIVE_FFLAG_INFINITY	0x0500
#define NATIVE_FFLAG_NAN		0x0100

/* Translation tables between native and m68k floating-point flags */
PRIVATE uae_u32 to_m68k_fpcond[0x48];
PRIVATE uae_u32 to_host_fpcond[0x10];

/* Truth table for floating-point condition codes */
PRIVATE uae_u32 fpcond_truth_table[32][8]; // 32 m68k conditions x 8 host condition codes

/* Initialization */
PUBLIC void FFPU fpu_init_native_fflags(void);

#ifdef FPU_UAE

/* Native to m68k floating-point condition codes */
PRIVATE inline uae_u32 FFPU get_fpccr(void)
	{ return to_m68k_fpcond[(FPU fpsr.condition_codes >> 8) & 0x47]; }

/* M68k to native floating-point condition codes */
PRIVATE inline void FFPU set_fpccr(uae_u32 new_fpcond)
	/* Precondition: new_fpcond is only valid for floating-point condition codes */
	{ FPU fpsr.condition_codes = to_host_fpcond[new_fpcond >> 24]; }

/* Make FPSR according to the value passed in argument */
PRIVATE inline void FFPU make_fpsr(fpu_register const & r)
	{ uae_u16 sw; __asm__ __volatile__ ("fxam\n\tfnstsw %0" : "=a" (sw) : "f" (r)); FPU fpsr.condition_codes = sw; }

/* Return the corresponding ID of the current floating-point condition codes */
/* NOTE: only valid for evaluation of a condition */
PRIVATE inline int FFPU host_fpcond_id(void)
	{ return ((FPU fpsr.condition_codes >> 12) & 4) | ((FPU fpsr.condition_codes >> 8) & 3); }

/* Return true if the floating-point condition is satisfied */
PRIVATE inline bool FFPU fpcctrue(int condition)
	{ return fpcond_truth_table[condition][host_fpcond_id()]; }

#endif /* FPU_UAE */

/* Return the address of the floating-point condition codes truth table */
static inline uae_u8 * const FFPU address_of_fpcond_truth_table(void)
	{ return ((uae_u8*)&fpcond_truth_table[0][0]); }

#endif /* FPU_X86_USE_NATIVE_FLAGS */

/* -------------------------------------------------------------------------- */
/* --- Use Original M68K FPU Mappings                                     --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_GENERIC_FLAGS

#undef FPU_USE_NATIVE_FLAGS

#define NATIVE_FFLAG_NEGATIVE	0x08000000
#define NATIVE_FFLAG_ZERO		0x04000000
#define NATIVE_FFLAG_INFINITY	0x02000000
#define NATIVE_FFLAG_NAN		0x01000000

/* Initialization - NONE */
PRIVATE inline void FFPU fpu_init_native_fflags(void)
	{ }

/* Native to m68k floating-point condition codes - SELF */
PRIVATE inline uae_u32 FFPU get_fpccr(void)
	{ return FPU fpsr.condition_codes; }

/* M68k to native floating-point condition codes - SELF */
PRIVATE inline void FFPU set_fpccr(uae_u32 new_fpcond)
	{ FPU fpsr.condition_codes = new_fpcond; }

#endif /* FPU_USE_GENERIC_FLAGS */

/* -------------------------------------------------------------------------- */
/* --- Use Lazy Flags Evaluation                                          --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_LAZY_FLAGS

#undef FPU_USE_NATIVE_FLAGS

#define NATIVE_FFLAG_NEGATIVE	0x08000000
#define NATIVE_FFLAG_ZERO		0x04000000
#define NATIVE_FFLAG_INFINITY	0x02000000
#define NATIVE_FFLAG_NAN		0x01000000

/* Initialization - NONE */
PRIVATE inline void FFPU fpu_init_native_fflags(void)
	{ }

/* Native to m68k floating-point condition codes - SELF */
PRIVATE inline uae_u32 FFPU get_fpccr(void)
{
	uae_u32 fpccr = 0;
	if (isnan(FPU result))
		fpccr |= FPSR_CCB_NAN;
	else if (FPU result == 0.0)
		fpccr |= FPSR_CCB_ZERO;
	else if (FPU result < 0.0)
		fpccr |= FPSR_CCB_NEGATIVE;
	if (isinf(FPU result))
		fpccr |= FPSR_CCB_INFINITY;
	return fpccr;
}

/* M68k to native floating-point condition codes - SELF */
PRIVATE inline void FFPU set_fpccr(uae_u32 new_fpcond)
{
	if (new_fpcond & FPSR_CCB_NAN)
		make_nan(FPU result);
	else if (new_fpcond & FPSR_CCB_ZERO)
		FPU result = 0.0;
	else if (new_fpcond & FPSR_CCB_NEGATIVE)
		FPU result = -1.0;
	else
		FPU result = +1.0;
	/* gb-- where is Infinity ? */
}

/* Make FPSR according to the value passed in argument */
PRIVATE inline void FFPU make_fpsr(fpu_register const & r)
	{ FPU result = r; }

#endif /* FPU_USE_LAZY_FLAGS */

#endif

/* -------------------------------------------------------------------------- */
/* --- Common methods                                                     --- */
/* -------------------------------------------------------------------------- */

/* Return the address of the floating-point condition codes register */
static inline uae_u32 * FFPU address_of_fpccr(void)
	{ return ((uae_u32 *)& FPU fpsr.condition_codes); }

#endif /* FPU_FLAGS_H */
