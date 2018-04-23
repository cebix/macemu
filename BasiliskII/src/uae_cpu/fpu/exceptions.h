/*
 *  fpu/exceptions.h - system-dependant FPU exceptions management
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

#ifndef FPU_EXCEPTIONS_H
#define FPU_EXCEPTIONS_H

/* NOTE: this file shall be included only from fpu/fpu_*.cpp */
#undef	PUBLIC
#define PUBLIC	extern

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* Defaults to generic exceptions */
#define FPU_USE_GENERIC_EXCEPTIONS
#define FPU_USE_GENERIC_ACCRUED_EXCEPTIONS

/* -------------------------------------------------------------------------- */
/* --- Selection of floating-point exceptions handling mode               --- */
/* -------------------------------------------------------------------------- */

/* Optimized i386 fpu core must use native exceptions */
#if defined(FPU_X86) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_EXCEPTIONS
# define FPU_USE_X86_EXCEPTIONS
#endif

/* Optimized i386 fpu core must use native accrued exceptions */
#if defined(FPU_X86) && defined(USE_X87_ASSEMBLY)
# undef FPU_USE_GENERIC_ACCRUED_EXCEPTIONS
# define FPU_USE_X86_ACCRUED_EXCEPTIONS
#endif

/* -------------------------------------------------------------------------- */
/* --- Native X86 Exceptions                                              --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_X86_EXCEPTIONS

/* Extend the SW_* codes */
#define SW_FAKE_BSUN SW_SF

/* Shorthand */
#define SW_EXCEPTION_MASK (SW_ES|SW_SF|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)
// #define SW_EXCEPTION_MASK (SW_SF|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)

/* Lookup tables */
PRIVATE uae_u32 exception_host2mac[ 0x80 ];
PRIVATE uae_u32 exception_mac2host[ 0x100 ];

/* Initialize native exception management */
PUBLIC void FFPU fpu_init_native_exceptions(void);

/* Return m68k floating-point exception status */
PRIVATE inline uae_u32 FFPU get_exception_status(void)
	{ return exception_host2mac[FPU fpsr.exception_status & (SW_FAKE_BSUN|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)]; }

/* Set new exception status. Assumes mask against FPSR_EXCEPTION to be already performed */
PRIVATE inline void FFPU set_exception_status(uae_u32 new_status)
	{ FPU fpsr.exception_status = exception_mac2host[new_status >> 8]; }

#endif /* FPU_USE_X86_EXCEPTIONS */

#ifdef FPU_USE_X86_ACCRUED_EXCEPTIONS

/* Lookup tables */
PRIVATE uae_u32 accrued_exception_host2mac[ 0x40 ];
PRIVATE uae_u32 accrued_exception_mac2host[ 0x20 ];

/* Initialize native accrued exception management */
PUBLIC void FFPU fpu_init_native_accrued_exceptions(void);

/* Return m68k accrued exception byte */
PRIVATE inline uae_u32 FFPU get_accrued_exception(void)
	{ return accrued_exception_host2mac[FPU fpsr.accrued_exception & (SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)]; }

/* Set new accrued exception byte */
PRIVATE inline void FFPU set_accrued_exception(uae_u32 new_status)
	{ FPU fpsr.accrued_exception = accrued_exception_mac2host[(new_status & 0xF8) >> 3]; }

#endif /* FPU_USE_X86_ACCRUED_EXCEPTIONS */

/* -------------------------------------------------------------------------- */
/* --- Default Exceptions Handling                                        --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_GENERIC_EXCEPTIONS

/* Initialize native exception management */
static inline void FFPU fpu_init_native_exceptions(void)
	{ }

/* Return m68k floating-point exception status */
PRIVATE inline uae_u32 FFPU get_exception_status(void)
	{ return FPU fpsr.exception_status; }

/* Set new exception status. Assumes mask against FPSR_EXCEPTION to be already performed */
PRIVATE inline void FFPU set_exception_status(uae_u32 new_status)
	{ FPU fpsr.exception_status = new_status; }

#endif /* FPU_USE_GENERIC_EXCEPTIONS */

#ifdef FPU_USE_GENERIC_ACCRUED_EXCEPTIONS

/* Initialize native accrued exception management */
PRIVATE inline void FFPU fpu_init_native_accrued_exceptions(void)
	{ }

/* Return m68k accrued exception byte */
PRIVATE inline uae_u32 FFPU get_accrued_exception(void)
	{ return FPU fpsr.accrued_exception; }

/* Set new accrued exception byte */
PRIVATE inline void FFPU set_accrued_exception(uae_u32 new_status)
	{ FPU fpsr.accrued_exception = new_status; }

#endif /* FPU_USE_GENERIC_ACCRUED_EXCEPTIONS */

#endif /* FPU_EXCEPTIONS_H */
