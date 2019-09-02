/*
 * fpu/mathlib.h - Floating-point math support library
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

#ifndef FPU_MATHLIB_H
#define FPU_MATHLIB_H

/* NOTE: this file shall be included only from fpu/fpu_*.cpp */
#undef	PUBLIC
#define PUBLIC	extern

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

// Define the following macro if branches are expensive. If so,
// integer-based isnan() and isinf() functions are implemented.
// TODO: move to Makefile.in
#define BRANCHES_ARE_EXPENSIVE 1

// Use ISO C99 extended-precision math functions (glibc 2.1+)
#define FPU_USE_ISO_C99 1

#if defined(FPU_USE_ISO_C99)
// NOTE: no prior <math.h> shall be included at this point
#define __USE_ISOC99 1 // for glibc 2.2.X and newer
#define __USE_ISOC9X 1 // for glibc 2.1.X
#include <math.h>
#else
#include <cmath>
using namespace std;
#endif

/* -------------------------------------------------------------------------- */
/* --- Floating-point register types                                      --- */
/* -------------------------------------------------------------------------- */

// Single : S 8*E 23*F
#define FP_SINGLE_EXP_MAX		0xff
#define FP_SINGLE_EXP_BIAS		0x7f

// Double : S 11*E 52*F
#define FP_DOUBLE_EXP_MAX		0x7ff
#define FP_DOUBLE_EXP_BIAS		0x3ff

// Extended : S 15*E 64*F
#define FP_EXTENDED_EXP_MAX		0x7fff
#define FP_EXTENDED_EXP_BIAS	0x3fff

// Zeroes						: E = 0 & F = 0
// Infinities					: E = MAX & F = 0
// Not-A-Number					: E = MAX & F # 0

/* -------------------------------------------------------------------------- */
/* --- Floating-point type shapes (IEEE-compliant)                        --- */
/* -------------------------------------------------------------------------- */

// Taken from glibc 2.2.x: ieee754.h

// IEEE-754 float format
union fpu_single_shape {
	
	fpu_single value;

	/* This is the IEEE 754 single-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:8;
		unsigned int mantissa:23;
#else
		unsigned int mantissa:23;
		unsigned int exponent:8;
		unsigned int negative:1;
#endif
	} ieee;

	/* This format makes it easier to see if a NaN is a signalling NaN.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:8;
		unsigned int quiet_nan:1;
		unsigned int mantissa:22;
#else
		unsigned int mantissa:22;
		unsigned int quiet_nan:1;
		unsigned int exponent:8;
		unsigned int negative:1;
#endif
	} ieee_nan;
};

// IEEE-754 double format
union fpu_double_shape {
	fpu_double value;
	
	/* This is the IEEE 754 double-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:11;
		/* Together these comprise the mantissa.  */
		unsigned int mantissa0:20;
		unsigned int mantissa1:32;
#else
#	if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int mantissa0:20;
		unsigned int exponent:11;
		unsigned int negative:1;
		unsigned int mantissa1:32;
#	else
		/* Together these comprise the mantissa.  */
		unsigned int mantissa1:32;
		unsigned int mantissa0:20;
		unsigned int exponent:11;
		unsigned int negative:1;
#	endif
#endif
	} ieee;

	/* This format makes it easier to see if a NaN is a signalling NaN.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:11;
		unsigned int quiet_nan:1;
		/* Together these comprise the mantissa.  */
		unsigned int mantissa0:19;
		unsigned int mantissa1:32;
#else
#	if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int mantissa0:19;
		unsigned int quiet_nan:1;
		unsigned int exponent:11;
		unsigned int negative:1;
		unsigned int mantissa1:32;
#	else
		/* Together these comprise the mantissa.  */
		unsigned int mantissa1:32;
		unsigned int mantissa0:19;
		unsigned int quiet_nan:1;
		unsigned int exponent:11;
		unsigned int negative:1;
#	endif
#endif
	} ieee_nan;

	/* This format is used to extract the sign_exponent and mantissa parts only */
	struct {
#if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int msw:32;
		unsigned int lsw:32;
#else
		unsigned int lsw:32;
		unsigned int msw:32;
#endif
	} parts;
};

#ifdef USE_LONG_DOUBLE
// IEEE-854 long double format
union fpu_extended_shape {
	fpu_extended value;
	
	/* This is the IEEE 854 double-extended-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:15;
		unsigned int empty:16;
		unsigned int mantissa0:32;
		unsigned int mantissa1:32;
#else
#	if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int exponent:15;
		unsigned int negative:1;
		unsigned int empty:16;
		unsigned int mantissa0:32;
		unsigned int mantissa1:32;
#	else
		unsigned int mantissa1:32;
		unsigned int mantissa0:32;
		unsigned int exponent:15;
		unsigned int negative:1;
		unsigned int empty:16;
#	endif
#endif
	} ieee;

	/* This is for NaNs in the IEEE 854 double-extended-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:15;
		unsigned int empty:16;
		unsigned int one:1;
		unsigned int quiet_nan:1;
		unsigned int mantissa0:30;
		unsigned int mantissa1:32;
#else
#	if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int exponent:15;
		unsigned int negative:1;
		unsigned int empty:16;
		unsigned int mantissa0:30;
		unsigned int quiet_nan:1;
		unsigned int one:1;
		unsigned int mantissa1:32;
#	else
		unsigned int mantissa1:32;
		unsigned int mantissa0:30;
		unsigned int quiet_nan:1;
		unsigned int one:1;
		unsigned int exponent:15;
		unsigned int negative:1;
		unsigned int empty:16;
#	endif
#endif
	} ieee_nan;
	
	/* This format is used to extract the sign_exponent and mantissa parts only */
	struct {
#if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
		unsigned int sign_exponent:16;
		unsigned int empty:16;
		unsigned int msw:32;
		unsigned int lsw:32;
#else
		unsigned int lsw:32;
		unsigned int msw:32;
		unsigned int sign_exponent:16;
		unsigned int empty:16;
#endif
	} parts;
};
#endif

#ifdef USE_QUAD_DOUBLE
// IEEE-854 quad double format
union fpu_extended_shape {
	fpu_extended value;
	
	/* This is the IEEE 854 quad-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:15;
		unsigned int mantissa0:16;
		unsigned int mantissa1:32;
		unsigned int mantissa2:32;
		unsigned int mantissa3:32;
#else
		unsigned int mantissa3:32;
		unsigned int mantissa2:32;
		unsigned int mantissa1:32;
		unsigned int mantissa0:16;
		unsigned int exponent:15;
		unsigned int negative:1;
#endif
	} ieee;

	/* This is for NaNs in the IEEE 854 quad-precision format.  */
	struct {
#ifdef WORDS_BIGENDIAN
		unsigned int negative:1;
		unsigned int exponent:15;
		unsigned int quiet_nan:1;
		unsigned int mantissa0:15;
		unsigned int mantissa1:32;
		unsigned int mantissa2:32;
		unsigned int mantissa3:32;
#else
		unsigned int mantissa3:32;
		unsigned int mantissa2:32;
		unsigned int mantissa1:32;
		unsigned int mantissa0:15;
		unsigned int quiet_nan:1;
		unsigned int exponent:15;
		unsigned int negative:1;
#endif
	} ieee_nan;

	/* This format is used to extract the sign_exponent and mantissa parts only */
#if defined(HOST_FLOAT_WORDS_BIG_ENDIAN) && HOST_FLOAT_WORDS_BIG_ENDIAN
	struct {
		uae_u64 msw;
		uae_u64 lsw;
	} parts64;
	struct {
		uae_u32 w0;
		uae_u32 w1;
		uae_u32 w2;
		uae_u32 w3;
	} parts32;
#else
	struct {
		uae_u64 lsw;
		uae_u64 msw;
	} parts64;
	struct {
		uae_u32 w3;
		uae_u32 w2;
		uae_u32 w1;
		uae_u32 w0;
	} parts32;
#endif
};
#endif

// Declare a shape of the requested FP type
#define fp_declare_init_shape(psvar, ftype) \
	fpu_ ## ftype ## _shape psvar

/* -------------------------------------------------------------------------- */
/* --- Extra Math Functions                                               --- */
/* --- (most of them had to be defined before including <fpu/flags.h>)    --- */
/* -------------------------------------------------------------------------- */

#undef isnan
#if 0 && defined(HAVE_ISNANL)
# define isnan(x) isnanl((x))
#else
# define isnan(x) fp_do_isnan((x))
#endif

PRIVATE inline bool FFPU fp_do_isnan(fpu_register const & r)
{
#ifdef BRANCHES_ARE_EXPENSIVE
#if !defined(USE_LONG_DOUBLE)
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	uae_s32 hx = sxp.parts.msw;
	uae_s32 lx = sxp.parts.lsw;
	hx &= 0x7fffffff;
	hx |= (uae_u32)(lx | (-lx)) >> 31;
	hx = 0x7ff00000 - hx;
	return (int)(((uae_u32)hx) >> 31);
#elif defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	uae_s64 hx = sxp.parts64.msw;
	uae_s64 lx = sxp.parts64.lsw;
	hx &= 0x7fffffffffffffffLL;
	hx |= (uae_u64)(lx | (-lx)) >> 63;
	hx = 0x7fff000000000000LL - hx;
	return (int)((uae_u64)hx >> 63);
#else
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	uae_s32 se = sxp.parts.sign_exponent;
	uae_s32 hx = sxp.parts.msw;
	uae_s32 lx = sxp.parts.lsw;
	se = (se & 0x7fff) << 1;
	lx |= hx & 0x7fffffff;
	se |= (uae_u32)(lx | (-lx)) >> 31;
	se = 0xfffe - se;
	return (int)(((uae_u32)(se)) >> 31);
#endif
#else
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	return	(sxp.ieee.exponent == FP_EXTENDED_EXP_MAX)
#else
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	return	(sxp.ieee.exponent == FP_DOUBLE_EXP_MAX)
#endif
		&& (sxp.ieee.mantissa0 & 0x7fffffff) != 0
		&& sxp.ieee.mantissa1 != 0
#ifdef USE_QUAD_DOUBLE
		&& sxp.ieee.mantissa2 != 0
		&& sxp.ieee.mantissa3 != 0
#endif
		;
#endif
}

#undef isinf
#if 0 && defined(HAVE_ISINFL)
# define isinf(x) isinfl((x))
#else
# define isinf(x) fp_do_isinf((x))
#endif

PRIVATE inline bool FFPU fp_do_isinf(fpu_register const & r)
{
#ifdef BRANCHES_ARE_EXPENSIVE
#if !defined(USE_LONG_DOUBLE)
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	uae_s32 hx = sxp.parts.msw;
	uae_s32 lx = sxp.parts.lsw;
	lx |= (hx & 0x7fffffff) ^ 0x7ff00000;
	lx |= -lx;
	return ~(lx >> 31) & (hx >> 30);
#elif defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	uae_s64 hx = sxp.parts64.msw;
	uae_s64 lx = sxp.parts64.lsw;
	lx |= (hx & 0x7fffffffffffffffLL) ^ 0x7fff000000000000LL;
	lx |= -lx;
	return ~(lx >> 63) & (hx >> 62);
#else
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	/* NOTE: This function should work for both m68k and native INFs. */
#if 0
	uae_s32 se = sxp.parts.sign_exponent;
	uae_s32 hx = sxp.parts.msw;
	uae_s32 lx = sxp.parts.lsw;
	/* This additional ^ 0x80000000 is necessary because in Intel's
	   internal representation of the implicit one is explicit.
	   NOTE: anyway, this is equivalent to & 0x7fffffff in that case.  */
#ifdef CPU_i386
	lx |= (hx ^ 0x80000000) | ((se & 0x7fff) ^ 0x7fff);
#else
	lx |= (hx & 0x7fffffff) | ((se & 0x7fff) ^ 0x7fff);
#endif
	lx |= -lx;
	se &= 0x8000;
	return ~(lx >> 31) & (1 - (se >> 14));
#else
	return sxp.ieee.exponent == FP_EXTENDED_EXP_MAX
		&& (sxp.ieee.mantissa0 & 0x7fffffff) == 0
		&& sxp.ieee.mantissa1 == 0;
#endif
#endif
#else
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	return	(sxp.ieee_nan.exponent == FP_EXTENDED_EXP_MAX)
#else
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	return	(sxp.ieee_nan.exponent == FP_DOUBLE_EXP_MAX)
#endif
		&& (sxp.ieee.mantissa0 & 0x7fffffff) == 0
		&& sxp.ieee.mantissa1 == 0
#ifdef USE_QUAD_DOUBLE
		&& sxp.ieee.mantissa2 == 0
		&& sxp.ieee.mantissa3 == 0
#endif
		;
#endif
}

#undef isneg
#define isneg(x) fp_do_isneg((x))

PRIVATE inline bool FFPU fp_do_isneg(fpu_register const & r)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
#else
	fp_declare_init_shape(sxp, double);
#endif
	sxp.value = r;
	return sxp.ieee.negative;
}

#undef iszero
#define iszero(x) fp_do_iszero((x))

PRIVATE inline bool FFPU fp_do_iszero(fpu_register const & r)
{
	// TODO: BRANCHES_ARE_EXPENSIVE
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
#else
	fp_declare_init_shape(sxp, double);
#endif
	sxp.value = r;
	return	(sxp.ieee.exponent == 0)
		&&	(sxp.ieee.mantissa0 == 0)
		&&	(sxp.ieee.mantissa1 == 0)
#ifdef USE_QUAD_DOUBLE
		&&	(sxp.ieee.mantissa2 == 0)
		&&	(sxp.ieee.mantissa3 == 0)
#endif
		;
}

PRIVATE inline void FFPU get_dest_flags(fpu_register const & r)
{
	fl_dest.negative	= isneg(r);
	fl_dest.zero		= iszero(r);
	fl_dest.infinity	= isinf(r);
	fl_dest.nan			= isnan(r);
	fl_dest.in_range	= !fl_dest.zero && !fl_dest.infinity && !fl_dest.nan;
}

PRIVATE inline void FFPU get_source_flags(fpu_register const & r)
{
	fl_source.negative	= isneg(r);
	fl_source.zero		= iszero(r);
	fl_source.infinity	= isinf(r);
	fl_source.nan		= isnan(r);
	fl_source.in_range	= !fl_source.zero && !fl_source.infinity && !fl_source.nan;
}

PRIVATE inline void FFPU make_nan(fpu_register & r, bool negative)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.ieee.exponent	= FP_EXTENDED_EXP_MAX;
	sxp.ieee.empty		= 0;
	sxp.ieee.mantissa0	= 0xffffffff;
#else
	fp_declare_init_shape(sxp, double);
	sxp.ieee.exponent	= FP_DOUBLE_EXP_MAX;
	sxp.ieee.mantissa0	= 0xfffff;
#endif
	sxp.ieee.mantissa1	= 0xffffffff;
#ifdef USE_QUAD_DOUBLE
	sxp.ieee.mantissa2	= 0xffffffff;
	sxp.ieee.mantissa3	= 0xffffffff;
#endif
	sxp.ieee.negative   = negative;
	r = sxp.value;
}

PRIVATE inline void FFPU make_zero(fpu_register & r, bool negative)
{
#if 1
	r = negative ? -0.0 : +0.0;
#else
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.ieee.empty      = 0;
#else
	fp_declare_init_shape(sxp, double);
#endif
	sxp.ieee.negative	= negative;
	sxp.ieee.exponent	= 0;
	sxp.ieee.mantissa0	= 0;
	sxp.ieee.mantissa1	= 0;
#ifdef USE_QUAD_DOUBLE
	sxp.ieee.mantissa2	= 0;
	sxp.ieee.mantissa3	= 0;
#endif
	r = sxp.value;
#endif
}

PRIVATE inline void FFPU make_inf(fpu_register & r, bool negative)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.ieee.exponent	= FP_EXTENDED_EXP_MAX;
	sxp.ieee.mantissa0	= 0x80000000;
	sxp.ieee.empty		= 0;
#else
	fp_declare_init_shape(sxp, double);
	sxp.ieee.exponent	= FP_DOUBLE_EXP_MAX;
	sxp.ieee.mantissa0	= 0;
#endif
	sxp.ieee.negative	= negative;
	sxp.ieee.mantissa1	= 0;
#ifdef USE_QUAD_DOUBLE
	sxp.ieee.mantissa2 = 0;
	sxp.ieee.mantissa3 = 0;
#endif
	r = sxp.value;
}

PRIVATE inline fpu_register FFPU fast_fgetexp(fpu_register const & r)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	return ((int) sxp.ieee.exponent - FP_EXTENDED_EXP_BIAS);
#else
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	return ((int) sxp.ieee.exponent - FP_DOUBLE_EXP_BIAS);
#endif
}

// Normalize to range 1..2
PRIVATE inline void FFPU fast_remove_exponent(fpu_register & r)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = r;
	sxp.ieee.exponent = FP_EXTENDED_EXP_BIAS;
#else
	fp_declare_init_shape(sxp, double);
	sxp.value = r;
	sxp.ieee.exponent = FP_DOUBLE_EXP_BIAS;
#endif
	r = sxp.value;
}

// The sign of the quotient is the exclusive-OR of the sign bits
// of the source and destination operands.
PRIVATE inline uae_u32 FFPU get_quotient_sign(fpu_register const & ra, fpu_register const & rb)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sap, extended);
	fp_declare_init_shape(sbp, extended);
#else
	fp_declare_init_shape(sap, double);
	fp_declare_init_shape(sbp, double);
#endif
	sap.value = ra;
	sbp.value = rb;
	return ((sap.ieee.negative ^ sbp.ieee.negative) ? FPSR_QUOTIENT_SIGN : 0);
}

/* -------------------------------------------------------------------------- */
/* --- Math functions                                                     --- */
/* -------------------------------------------------------------------------- */

#if defined(FPU_USE_ISO_C99) && (defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE))
# ifdef HAVE_LOGL
#  define fp_log	logl
# endif
# ifdef HAVE_LOG1PL
#  define fp_log1p	log1pl
# endif
# ifdef HAVE_EXPM1L
#  define fp_expm1	expm1l
# endif
# ifdef HAVE_LOG10L
#  define fp_log10	log10l
# endif
# ifdef HAVE_LOG2L
#  define fp_log2	log2l
# endif
# ifdef HAVE_EXPL
#  define fp_exp	expl
# endif
# ifdef HAVE_POWL
#  define fp_pow	powl
# endif
# if defined(HAVE_EXP10L)
#  define fp_pow10	exp10l
# elif defined(HAVE_POW10L)
#  define fp_pow10	pow10l
# else
#  define fp_pow10(x)	fp_pow(LD(10.0), x)
# endif
# if defined(HAVE_EXP2L)
#  define fp_pow2	exp2l
# elif defined(HAVE_POW2L)
#  define fp_pow2	pow2l
# else
#  define fp_pow2(x)	fp_pow(LD(2.0), x)
# endif
# ifdef HAVE_FABSL
#  define fp_fabs	fabsl
# endif
# ifdef HAVE_SQRTL
#  define fp_sqrt	sqrtl
# endif
# ifdef HAVE_SINL
#  define fp_sin	sinl
# endif
# ifdef HAVE_COSL
#  define fp_cos	cosl
# endif
# ifdef HAVE_TANL
#  define fp_tan	tanl
# endif
# ifdef HAVE_SINHL
#  define fp_sinh	sinhl
# endif
# ifdef HAVE_COSHL
#  define fp_cosh	coshl
# endif
# ifdef HAVE_TANHL
#  define fp_tanh	tanhl
# endif
# ifdef HAVE_ASINL
#  define fp_asin	asinl
# endif
# ifdef HAVE_ACOSL
#  define fp_acos	acosl
# endif
# ifdef HAVE_ATANL
#  define fp_atan	atanl
# endif
# ifdef HAVE_ASINHL
#  define fp_asinh	asinhl
# endif
# ifdef HAVE_ACOSHL
#  define fp_acosh	acoshl
# endif
# ifdef HAVE_ATANHL
#  define fp_atanh	atanhl
# endif
# ifdef HAVE_FLOORL
#  define fp_floor	floorl
# endif
# ifdef HAVE_CEILL
#  define fp_ceil	ceill
# endif
#endif

#ifndef fp_log
# define fp_log		log
#endif
#ifndef fp_log1p
# define fp_log1p	log1p
#endif
#ifndef fp_expm1
# define fp_expm1	expm1
#endif
#ifndef fp_log10
# define fp_log10	log10
#endif
#ifndef fp_log2
# define fp_log2	log2
#endif
#ifndef fp_exp
# define fp_exp		exp
#endif
#ifndef fp_pow
# define fp_pow		pow
#endif
#ifndef fp_pow10
# ifdef HAVE_POW10
#   define fp_pow10	pow10
# else
#   define fp_pow10	exp10
# endif
#endif
#ifndef fp_pow2
# ifdef HAVE_POW2
#   define fp_pow2	pow2
# else
#   define fp_pow2	exp2
# endif
#endif
#ifndef fp_fabs
# define fp_fabs	fabs
#endif
#ifndef fp_sqrt
# define fp_sqrt	sqrt
#endif
#ifndef fp_sin
# define fp_sin		sin
#endif
#ifndef fp_cos
# define fp_cos		cos
#endif
#ifndef fp_tan
# define fp_tan		tan
#endif
#ifndef fp_sinh
# define fp_sinh	sinh
#endif
#ifndef fp_cosh
# define fp_cosh	cosh
#endif
#ifndef fp_tanh
# define fp_tanh	tanh
#endif
#ifndef fp_asin
# define fp_asin	asin
#endif
#ifndef fp_acos
# define fp_acos	acos
#endif
#ifndef fp_atan
# define fp_atan	atan
#endif
#ifndef fp_asinh
# define fp_asinh	asinh
#endif
#ifndef fp_acosh
# define fp_acosh	acosh
#endif
#ifndef fp_atanh
# define fp_atanh	atanh
#endif
#ifndef fp_floor
# define fp_floor	floor
#endif
#ifndef fp_ceil
# define fp_ceil	ceil
#endif

#if defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY)
// Assembly optimized support functions. Taken from glibc 2.2.2

#undef fp_log
#define fp_log fp_do_log

PRIVATE inline fpu_extended fp_do_log(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fldln2; fxch; fyl2x" : "=t" (value) : "0" (x) : "st(1)");
	return value;
}

#undef fp_log10
#define fp_log10 fp_do_log10

PRIVATE inline fpu_extended fp_do_log10(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fldlg2; fxch; fyl2x" : "=t" (value) : "0" (x) : "st(1)");
	return value;
}

#if !defined(HAVE_EXPL)
#undef fp_exp
#define fp_exp fp_do_exp

PRIVATE inline fpu_extended fp_do_exp(fpu_extended x)
{
	fpu_extended value, exponent;
	if (isinf(x))
	{
		if(isneg(x))
			return 0.;
		else
			return x;
	}
	__asm__ __volatile__("fldl2e                    # e^x = 2^(x * log2(e))\n\t"
				 "fmul      %%st(1)         # x * log2(e)\n\t"
				 "fst       %%st(1)\n\t"
				 "frndint                   # int(x * log2(e))\n\t"
				 "fxch\n\t"
				 "fsub      %%st(1)         # fract(x * log2(e))\n\t"
				 "f2xm1                     # 2^(fract(x * log2(e))) - 1\n\t"
				 : "=t" (value), "=u" (exponent) : "0" (x));
	value += 1.0;
	__asm__ __volatile__("fscale" : "=t" (value) : "0" (value), "u" (exponent));
	return value;
}
#endif

#if !defined(HAVE_EXP10L) && !defined(HAVE_POW10L)
#undef fp_pow
#define fp_pow fp_do_pow

PRIVATE fpu_extended fp_do_pow(fpu_extended x, fpu_extended y);
#endif

#undef fp_fabs
#define fp_fabs fp_do_fabs

PRIVATE inline fpu_extended fp_do_fabs(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fabs" : "=t" (value) : "0" (x));
	return value;
}

#undef fp_sqrt
#define fp_sqrt fp_do_sqrt

PRIVATE inline fpu_extended fp_do_sqrt(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fsqrt" : "=t" (value) : "0" (x));
	return value;
}

#ifndef ACCURATE_SIN_COS_TAN
#undef fp_sin
#define fp_sin fp_do_sin

PRIVATE inline fpu_extended fp_do_sin(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fsin" : "=t" (value) : "0" (x));
	return value;
}

#undef fp_cos
#define fp_cos fp_do_cos

PRIVATE inline fpu_extended fp_do_cos(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fcos" : "=t" (value) : "0" (x));
	return value;
}

#undef fp_tan
#define fp_tan fp_do_tan

PRIVATE inline fpu_extended fp_do_tan(fpu_extended x)
{
	fpu_extended value, value2;
	__asm__ __volatile__("fptan" : "=t" (value2), "=u" (value) : "0" (x));
	return value;
}
#endif /* ACCURATE_SIN_COS_TAN */

#ifndef HAVE_EXPM1L
#undef fp_expm1
#define fp_expm1 fp_do_expm1

// Returns: exp(X) - 1.0
PRIVATE inline fpu_extended fp_do_expm1(fpu_extended x)
{
	fpu_extended value, exponent, temp, temp2;
	if (isinf(x))
	{
		if(isneg(x))
			return -1.;
		else
			return x;
	}
	__asm__ __volatile__("fldl2e                    # e^x - 1 = 2^(x * log2(e)) - 1\n\t"
				 "fmul      %%st(1)         # x * log2(e)\n\t"
				 "fst       %%st(1)\n\t"
				 "frndint                   # int(x * log2(e))\n\t"
				 "fxch\n\t"
				 "fsub      %%st(1)         # fract(x * log2(e))\n\t"
				 "f2xm1                     # 2^(fract(x * log2(e))) - 1\n\t"
				 "fscale                    # 2^(x * log2(e)) - 2^(int(x * log2(e)))\n\t"
				 : "=t" (value), "=u" (exponent) : "0" (x));
	__asm__ __volatile__("fld1    \n\t"
                         "fscale  \n\t"
                         : "=t" (temp), "=u" (temp2) : "0" (exponent));
	temp -= 1.0;
	return temp + value ? temp + value : x;
}
#endif

#undef fp_sgn1
#define fp_sgn1 fp_do_sgn1

PRIVATE inline fpu_extended fp_do_sgn1(fpu_extended x)
{
#if defined(USE_LONG_DOUBLE) || defined(USE_QUAD_DOUBLE)
	fp_declare_init_shape(sxp, extended);
	sxp.value = x;
	sxp.ieee_nan.exponent	= FP_EXTENDED_EXP_MAX>>1;
	sxp.ieee_nan.one		= 1;
#else
	fp_declare_init_shape(sxp, double);
	sxp.value = x;
	sxp.ieee_nan.exponent  = FP_DOUBLE_EXP_MAX>>1;
#endif
	sxp.ieee_nan.quiet_nan	= 0;
	sxp.ieee_nan.mantissa0	= 0;
	sxp.ieee_nan.mantissa1	= 0;
	x = sxp.value;
	return x;
}

#ifndef HAVE_SINHL
#undef fp_sinh
#define fp_sinh fp_do_sinh

PRIVATE inline fpu_extended fp_do_sinh(fpu_extended x)
{
	if (isinf(x)) return x;
	fpu_extended exm1 = fp_expm1(fp_fabs(x));
	return 0.5 * (exm1 / (exm1 + 1.0) + exm1) * fp_sgn1(x);
}
#endif

#ifndef HAVE_COSHL
#undef fp_cosh
#define fp_cosh fp_do_cosh

PRIVATE inline fpu_extended fp_do_cosh(fpu_extended x)
{
	fpu_extended ex = fp_exp(x);
	return 0.5 * (ex + 1.0 / ex);
}
#endif

#ifndef HAVE_TANHL
#undef fp_tanh
#define fp_tanh fp_do_tanh

PRIVATE inline fpu_extended fp_do_tanh(fpu_extended x)
{
	fpu_extended exm1 = fp_expm1(-fp_fabs(x + x));
	return exm1 / (exm1 + 2.0) * fp_sgn1(-x);
}
#endif

#undef fp_atan2
#define fp_atan2 fp_do_atan2

PRIVATE inline fpu_extended fp_do_atan2(fpu_extended y, fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fpatan" : "=t" (value) : "0" (x), "u" (y) : "st(1)");
	return value;
}

#ifndef HAVE_ASINL
#undef fp_asin
#define fp_asin fp_do_asin

PRIVATE inline fpu_extended fp_do_asin(fpu_extended x)
{
	return fp_atan2(x, fp_sqrt(1.0 - x * x));
}
#endif

#ifndef HAVE_ACOSL
#undef fp_acos
#define fp_acos fp_do_acos

PRIVATE inline fpu_extended fp_do_acos(fpu_extended x)
{
	return fp_atan2(fp_sqrt(1.0 - x * x), x);
}
#endif

#undef fp_atan
#define fp_atan fp_do_atan

PRIVATE inline fpu_extended fp_do_atan(fpu_extended x)
{
	fpu_extended value;
	__asm__ __volatile__("fld1; fpatan" : "=t" (value) : "0" (x) : "st(1)");
	return value;
}

#ifndef HAVE_LOG1PL
#undef fp_log1p
#define fp_log1p fp_do_log1p

// Returns: ln(1.0 + X)
PRIVATE fpu_extended fp_do_log1p(fpu_extended x);
#endif

#ifndef HAVE_ASINHL
#undef fp_asinh
#define fp_asinh fp_do_asinh

PRIVATE inline fpu_extended fp_do_asinh(fpu_extended x)
{
	fpu_extended y = fp_fabs(x);
	return (fp_log1p(y * y / (fp_sqrt(y * y + 1.0) + 1.0) + y) * fp_sgn1(x));
}
#endif

#ifndef HAVE_ACOSHL
#undef fp_acosh
#define fp_acosh fp_do_acosh

PRIVATE inline fpu_extended fp_do_acosh(fpu_extended x)
{
	return fp_log(x + fp_sqrt(x - 1.0) * fp_sqrt(x + 1.0));
}
#endif

#ifndef HAVE_ATANHL
#undef fp_atanh
#define fp_atanh fp_do_atanh

PRIVATE inline fpu_extended fp_do_atanh(fpu_extended x)
{
	fpu_extended y = fp_fabs(x);
	return -0.5 * fp_log1p(-(y + y) / (1.0 + y)) * fp_sgn1(x);
}
#endif


/*
 * LLVM 2.9 crashes on first definition,
 * clang with LLVM 3.x crashes on 2nd definition... sigh
 */
#if defined(__clang__) || !defined(__llvm__)
#define DEFINE_ROUND_FUNC(rounding_mode_str, rounding_mode)						\
PRIVATE inline fpu_extended fp_do_round_to_ ## rounding_mode_str(fpu_extended __x)	\
{ \
	register long double __value; \
	register int __ignore; \
	volatile unsigned short __cw; \
	volatile unsigned short __cwtmp; \
	__asm __volatile ("fnstcw %3\n\t" \
					  "movzwl %3, %1\n\t" \
					  "andl $0xf3ff, %1\n\t" \
					  "orl %5, %1\n\t" \
					  "movw %w1, %2\n\t" \
					  "fldcw %2\n\t" \
					  "frndint\n\t" \
					  "fldcw %3" \
					  : "=t" (__value), "=&q" (__ignore), "=m" (__cwtmp), \
					  "=m" (__cw) \
					  : "0" (__x), "i"(rounding_mode)); \
	return __value; \
}
#else
#define DEFINE_ROUND_FUNC(rounding_mode_str, rounding_mode)						\
PRIVATE inline fpu_extended fp_do_round_to_ ## rounding_mode_str(fpu_extended x)	\
{																				\
	volatile unsigned short cw;													\
	__asm__ __volatile__("fnstcw %0" : "=m" (cw));										\
	volatile unsigned short cw_temp = (cw & 0xf3ff) | (rounding_mode);			\
	__asm__ __volatile__("fldcw %0" : : "m" (cw_temp));									\
	fpu_extended value;															\
	__asm__ __volatile__("frndint" : "=t" (value) : "0" (x));							\
	__asm__ __volatile__("fldcw %0" : : "m" (cw));										\
	return value;																\
}
#endif

#undef fp_round_to_minus_infinity
#ifdef HAVE_FLOORL
#define fp_round_to_minus_infinity floorl
#else
#define fp_round_to_minus_infinity fp_do_round_to_minus_infinity
DEFINE_ROUND_FUNC(minus_infinity, CW_RC_DOWN)
#endif

#undef fp_round_to_plus_infinity
#ifdef HAVE_CEILL
#define fp_round_to_plus_infinity ceill
#else
#define fp_round_to_plus_infinity fp_do_round_to_plus_infinity
DEFINE_ROUND_FUNC(plus_infinity, CW_RC_UP)
#endif

#undef fp_round_to_zero
#ifdef HAVE_TRUNCL
#define fp_round_to_zero truncl
#else
#define fp_round_to_zero fp_do_round_to_zero
DEFINE_ROUND_FUNC(zero, CW_RC_ZERO)
#endif

#undef fp_round_to_nearest
#ifdef HAVE_ROUNDL
#define fp_round_to_nearest roundl
#else
#define fp_round_to_nearest fp_do_round_to_nearest
DEFINE_ROUND_FUNC(nearest, CW_RC_NEAR)
#endif

#undef fp_ceil
#define fp_ceil fp_do_round_to_plus_infinity

#undef fp_floor
#define fp_floor fp_do_round_to_minus_infinity


#endif /* USE_X87_ASSEMBLY */

#ifndef fp_round_to_minus_infinity
#define fp_round_to_minus_infinity(x) fp_floor(x)
#endif

#ifndef fp_round_to_plus_infinity
#define fp_round_to_plus_infinity(x) fp_ceil(x)
#endif

#ifndef fp_round_to_zero
#define fp_round_to_zero(x) ((int)(x))
#endif

#ifndef fp_round_to_nearest
#define fp_round_to_nearest(x) ((int)((x) + 0.5))
#endif

#endif /* FPU_MATHLIB_H */
