/*
 *  types.h - basic types for fpu registers
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

#ifndef FPU_TYPES_H
#define FPU_TYPES_H

#include "sysdeps.h"

/* Default behavior is *not* to use long doubles */
#undef USE_LONG_DOUBLE
#undef USE_QUAD_DOUBLE

/* -------------------------------------------------------------------------- */
/* --- Original UAE fpu core                                              --- */
/* -------------------------------------------------------------------------- */

#if defined(FPU_UAE)

/* 4-byte floats */
#if SIZEOF_FLOAT == 4
typedef float uae_f32;
#elif SIZEOF_DOUBLE == 4
typedef double uae_f32;
#else
#error "No 4 byte float type, you lose."
#endif

/* 8-byte floats */
#if SIZEOF_DOUBLE == 8
typedef double uae_f64;
#elif SIZEOF_LONG_DOUBLE == 8
typedef long double uae_f64;
#else
#error "No 8 byte float type, you lose."
#endif

/* Original UAE FPU registers are only 8 bytes long */
typedef uae_f64		fpu_register;
typedef fpu_register	fpu_extended;
typedef uae_f64		fpu_double;
typedef uae_f32		fpu_single;

/* -------------------------------------------------------------------------- */
/* --- Optimized core for x86                                             --- */
/* -------------------------------------------------------------------------- */

#elif defined(FPU_X86)

/* 4-byte floats */
#if SIZEOF_FLOAT == 4
typedef float uae_f32;
#elif SIZEOF_DOUBLE == 4
typedef double uae_f32;
#else
#error "No 4 byte float type, you lose."
#endif

/* 8-byte floats */
#if SIZEOF_DOUBLE == 8
typedef float uae_f64;
#elif SIZEOF_LONG_DOUBLE == 8
typedef double uae_f64;
#else
#error "No 8 byte float type, you lose."
#endif

/* At least 10-byte floats are required */
#if SIZEOF_LONG_DOUBLE >= 10
typedef long double fpu_register;
#else
#error "No float type at least 10 bytes long, you lose."
#endif

/* X86 FPU has a custom register type that maps to a native X86 register */
typedef fpu_register	fpu_extended;
typedef uae_f64		fpu_double;
typedef uae_f32		fpu_single;

/* -------------------------------------------------------------------------- */
/* --- C99 implementation                                                 --- */
/* -------------------------------------------------------------------------- */

#elif defined(FPU_IEEE)

#if HOST_FLOAT_FORMAT != IEEE_FLOAT_FORMAT
#error "No IEEE float format, you lose."
#endif

/* 4-byte floats */
#if SIZEOF_FLOAT == 4
typedef float uae_f32;
#elif SIZEOF_DOUBLE == 4
typedef double uae_f32;
#else
#error "No 4 byte float type, you lose."
#endif

/* 8-byte floats */
#if SIZEOF_DOUBLE == 8
typedef double uae_f64;
#elif SIZEOF_LONG_DOUBLE == 8
typedef long double uae_f64;
#else
#error "No 8 byte float type, you lose."
#endif

/* 12-byte or 16-byte floats */
#if SIZEOF_LONG_DOUBLE == 12
typedef long double uae_f96;
typedef uae_f96 fpu_register;
#define USE_LONG_DOUBLE 1
#elif SIZEOF_LONG_DOUBLE == 16 && defined(__x86_64__)
/* Long doubles on x86-64 are really held in old x87 FPU stack.  */
typedef long double uae_f128;
typedef uae_f128 fpu_register;
#define USE_LONG_DOUBLE 1
#elif 0
/* Disable for now and probably for good as (i) the emulator
   implementation is not correct, (ii) I don't know of any CPU which
   handles this kind of format *natively* with conformance to IEEE.  */
typedef long double uae_f128;
typedef uae_f128 fpu_register;
#define USE_QUAD_DOUBLE 1
#else
typedef uae_f64 fpu_register;
#endif

/* We need all those floating-point types */
typedef fpu_register	fpu_extended;
typedef uae_f64			fpu_double;
typedef uae_f32			fpu_single;

#endif

#endif /* FPU_TYPES_H */
