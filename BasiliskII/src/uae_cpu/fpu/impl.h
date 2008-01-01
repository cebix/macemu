/*
 *  fpu/impl.h - extra functions and inline implementations
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#ifndef FPU_IMPL_H
#define FPU_IMPL_H

/* NOTE: this file shall be included from fpu/core.h */
#undef	PUBLIC
#define PUBLIC	/**/

#undef	PRIVATE
#define PRIVATE	/**/

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* -------------------------------------------------------------------------- */
/* --- X86 assembly fpu specific methods                                  --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_X86

/* Return the floating-point status register in m68k format */
static inline uae_u32 FFPU get_fpsr(void)
{
	return	to_m68k_fpcond[(x86_status_word & 0x4700) >> 8]
		|	FPU fpsr.quotient
		|	exception_host2mac[x86_status_word & (SW_FAKE_BSUN|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)]
		|	accrued_exception_host2mac[x86_status_word_accrued & (SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)]
		;
}

/* Set the floating-point status register from an m68k format */
static inline void FFPU set_fpsr(uae_u32 new_fpsr)
{
	x86_status_word = to_host_fpcond[(new_fpsr & FPSR_CCB) >> 24 ]
		| exception_mac2host[(new_fpsr & FPSR_EXCEPTION_STATUS) >> 8];
	x86_status_word_accrued	= accrued_exception_mac2host[(new_fpsr & FPSR_ACCRUED_EXCEPTION) >> 3];
}

#endif

/* -------------------------------------------------------------------------- */
/* --- Original UAE and IEEE FPU core methods                             --- */
/* -------------------------------------------------------------------------- */

#ifndef FPU_X86

/* Return the floating-point status register in m68k format */
static inline uae_u32 FFPU get_fpsr(void)
{
	uae_u32 condition_codes		= get_fpccr();
	uae_u32 exception_status	= get_exception_status();
	uae_u32 accrued_exception	= get_accrued_exception();
	uae_u32 quotient			= FPU fpsr.quotient;
	return (condition_codes | quotient | exception_status | accrued_exception);
}

/* Set the floating-point status register from an m68k format */
static inline void FFPU set_fpsr(uae_u32 new_fpsr)
{
	set_fpccr					( new_fpsr & FPSR_CCB				);
	set_exception_status		( new_fpsr & FPSR_EXCEPTION_STATUS	);
	set_accrued_exception		( new_fpsr & FPSR_ACCRUED_EXCEPTION	);
	FPU fpsr.quotient			= new_fpsr & FPSR_QUOTIENT;
}

#endif

/* -------------------------------------------------------------------------- */
/* --- Common routines for control word                                   --- */
/* -------------------------------------------------------------------------- */

/* Return the floating-point control register in m68k format */
static inline uae_u32 FFPU get_fpcr(void)
{
	uae_u32 rounding_precision	= get_rounding_precision();
	uae_u32 rounding_mode		= get_rounding_mode();
	return (rounding_precision | rounding_mode);
}

/* Set the floating-point control register from an m68k format */
static inline void FFPU set_fpcr(uae_u32 new_fpcr)
{
	set_rounding_precision		( new_fpcr & FPCR_ROUNDING_PRECISION);
	set_rounding_mode			( new_fpcr & FPCR_ROUNDING_MODE		);
	set_host_control_word();
}

/* -------------------------------------------------------------------------- */
/* --- Specific part to X86 assembly FPU                                  --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_X86

/* Retrieve a floating-point register value and convert it to double precision */
static inline double FFPU fpu_get_register(int r)
{
	double f;
	__asm__ __volatile__("fldt %1\n\tfstpl %0" : "=m" (f) : "m" (FPU registers[r]));
	return f;
}

#endif

/* -------------------------------------------------------------------------- */
/* --- Specific to original UAE or new IEEE-based FPU core                --- */
/* -------------------------------------------------------------------------- */

#if defined(FPU_UAE) || defined(FPU_IEEE)

/* Retrieve a floating-point register value and convert it to double precision */
static inline double FFPU fpu_get_register(int r)
{
	return FPU registers[r];
}

#endif

#endif /* FPU_IMPL_H */
