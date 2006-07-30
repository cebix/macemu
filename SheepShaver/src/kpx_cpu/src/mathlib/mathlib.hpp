
/*
 *  mathlib.hpp - Math library wrapper
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

#ifndef MATHLIB_H
#define MATHLIB_H

#include <math.h>
#include "mathlib/ieeefp.hpp"

// Broken MacOS X headers
#if defined(__APPLE__) && defined(__MACH__)
// ... the following exist but are not macro-defined ...
#ifndef FP_NAN
#define FP_NAN			FP_NAN
#define FP_INFINITE		FP_INFINITE
#define FP_ZERO			FP_ZERO
#define FP_NORMAL		FP_NORMAL
#define FP_SUBNORMAL	FP_SUBNORMAL
#endif
#endif

// GCC fixes for IRIX/mips
#if defined __GNUC__ && defined __sgi__ && defined __mips__
#define mathlib_generic_1(func, x) \
		(sizeof(x) == sizeof(float) ? _##func##f(x) : _##func(x))

#define fpclassify(x)		mathlib_generic_1(fpclassify, x)
#define isnormal(x)		mathlib_generic_1(isnormal, x)
#define isfinite(x)		mathlib_generic_1(isfinite, x)
#define isnan(x)		mathlib_generic_1(isnan, x)
#define isinf(x)		mathlib_generic_1(isinf, x)
#define signbit(x)		mathlib_generic_1(signbit, x)

#define mathlib_generic_2(func, x, y) \
		((sizeof(x) == sizeof(float) && sizeof(x) == sizeof(y)) ? _##func##f(x, y) : _##func(x, y))

#define isless(x,y)		mathlib_generic_2(isless, x, y)
#define isgreater(x,y)	mathlib_generic_2(isgreater, x, y)
#endif

// C++ exception specifications
#if defined __GLIBC__ && defined __THROW
#define MATHLIB_THROW __THROW
#else
#define MATHLIB_THROW
#endif

// 7.12  Mathematics <math.h> [#6]
#ifndef FP_NAN
enum {
    FP_NAN,
# define FP_NAN FP_NAN
	FP_INFINITE,
# define FP_INFINITE FP_INFINITE
	FP_ZERO,
# define FP_ZERO FP_ZERO
	FP_SUBNORMAL,
# define FP_SUBNORMAL FP_SUBNORMAL
	FP_NORMAL
# define FP_NORMAL FP_NORMAL
};
#endif

// Arch-dependent definitions
#if defined(__i386__)
#include "mathlib/mathlib-i386.hpp"
#endif
#if defined(__x86_64__)
#include "mathlib/mathlib-x86_64.hpp"
#endif
#if defined(__powerpc__) || defined(__ppc__)
#include "mathlib/mathlib-ppc.hpp"
#endif

// Floating-Point Multiply Add/Subtract functions
#if (SIZEOF_LONG_DOUBLE > SIZEOF_DOUBLE) && (SIZEOF_DOUBLE > SIZEOF_FLOAT)
// FIXME: this is wrong for underflow conditions
#ifndef mathlib_fmadd
static inline double mathlib_fmadd(double x, double y, double z)
{
	return ((long double)x * (long double)y) + z;
}
static inline float mathlib_fmadd(float x, float y, float z)
{
	return ((double)x * (double)y) + z;
}
#define mathlib_fmadd(x, y, z) (mathlib_fmadd)(x, y, z)
#endif
#ifndef mathlib_fmsub
static inline double mathlib_fmsub(double x, double y, double z)
{
	return ((long double)x * (long double)y) - z;
}
static inline float mathlib_fmsub(float x, float y, float z)
{
	return ((double)x * (double)y) - z;
}
#define mathlib_fmsub(x, y, z) (mathlib_fmsub)(x, y, z)
#endif
#endif
#ifndef mathlib_fmadd
#define mathlib_fmadd(x, y, z) (((x) * (y)) + (z))
#endif
#ifndef mathlib_fmsub
#define mathlib_fmsub(x, y, z) (((x) * (y)) - (z))
#endif

// 7.12.6.2  The exp2 functions
#ifdef HAVE_EXP2F
extern "C" float exp2f(float x) MATHLIB_THROW;
#else
#ifdef HAVE_EXP2
extern "C" double exp2(double x) MATHLIB_THROW;
#define exp2f(x) (float)exp2(x)
#else
#ifndef exp2f
#define exp2f(x) powf(2.0, (x))
#endif
#endif
#endif

// 7.12.6.10  The log2 functions
#ifdef HAVE_LOG2F
extern "C" float log2f(float x) MATHLIB_THROW;
#else
#ifdef HAVE_LOG2
extern "C" double log2(double x) MATHLIB_THROW;
#define log2f(x) (float)log2(x)
#else
#ifndef M_LN2
#define M_LN2 logf(2.0)
#endif
#ifndef log2f
#define log2f(x) logf(x) / M_LN2
#endif
#endif
#endif

// 7.12.9.1  The ceil functions
#ifdef HAVE_CEILF
extern "C" float ceilf(float x) MATHLIB_THROW;
#else
#ifdef HAVE_CEIL
extern "C" double ceil(double x) MATHLIB_THROW;
#define ceilf(x) (float)ceil(x)
#endif
#endif

// 7.12.9.2  The floor functions
#ifdef HAVE_FLOORF
extern "C" float floorf(float x) MATHLIB_THROW;
#else
#ifdef HAVE_FLOOR
extern "C" double floor(double x) MATHLIB_THROW;
#define floorf(x) (float)floor(x)
#endif
#endif

// 7.12.9.5  The lrint and llrint functions
#ifdef HAVE_LRINT
extern "C" long lrint(double x) MATHLIB_THROW;
#else
#ifndef mathlib_lrint
extern long mathlib_lrint(double);
#endif
#define lrint(x) mathlib_lrint(x)
#endif

// 7.12.9.6  The round functions
#ifdef HAVE_ROUNDF
extern "C" float roundf(float x) MATHLIB_THROW;
#else
#ifdef HAVE_ROUND
extern "C" double round(double x) MATHLIB_THROW;
#define roundf(x) (float)round(x)
#else
extern float mathlib_roundf(float);
#define roundf(x) mathlib_roundf(x)
#endif
#endif

// 7.12.9.8  The trunc functions
#ifdef HAVE_TRUNCF
extern "C" float truncf(float x) MATHLIB_THROW;
#else
#ifdef HAVE_TRUNC
extern "C" double trunc(double x) MATHLIB_THROW;
#define truncf(x) (float)trunc(x)
#endif
#endif

// 7.12.3.1  The fpclassify macro
#ifndef fpclassify
#ifndef mathlib_fpclassifyf
extern int mathlib_fpclassifyf(float x);
#endif
#ifndef mathlib_fpclassify
extern int mathlib_fpclassify(double x);
#endif
#ifndef mathlib_fpclassifyl
extern int mathlib_fpclassifyl(long double x);
#endif
#define fpclassify(x)											\
		(sizeof (x) == sizeof (float)							\
		 ? mathlib_fpclassifyf (x)								\
		 : sizeof (x) == sizeof (double)						\
		 ? mathlib_fpclassify (x) : mathlib_fpclassifyl (x))
#endif

// 7.12.3.2  The isfinite macro
static inline int mathlib_isfinite(float x)
{
	int32 ix;

	MATHLIB_GET_FLOAT_WORD(ix, x);
	return (int)((uint32)((ix & 0x7fffffff) - 0x7f800000) >> 31);
}

static inline int mathlib_isfinite(double x)
{
	int32 hx;

	MATHLIB_GET_HIGH_WORD(hx, x);
	return (int)((uint32)((hx & 0x7fffffff) - 0x7ff00000) >> 31);
}

#ifndef isfinite
#define isfinite(x) mathlib_isfinite(x)
#endif

// 7.12.3.3  The isinf macro
static inline int mathlib_isinf(float x)
{
	int32 ix, t;

	MATHLIB_GET_FLOAT_WORD(ix, x);
	t = ix & 0x7fffffff;
	t ^= 0x7f800000;
	t |= -t;
	return ~(t >> 31) & (ix >> 30);
}

static inline int mathlib_isinf(double x)
{
	int32 hx, lx;

	MATHLIB_EXTRACT_WORDS(hx, lx, x);
	lx |= (hx & 0x7fffffff) ^ 0x7ff00000;
	lx |= -lx;
	return ~(lx >> 31) & (hx >> 30);
}

#ifndef isinf
#if defined __sgi && defined __mips
// specialized implementation for IRIX mips compilers
extern "C" int _isinf(double);
extern "C" int _isinff(float);
static inline int isinf(double x) { return _isinf(x); }
static inline int isinf(float x) { return _isinff(x); }
#else
#define isinf(x) mathlib_isinf(x)
#endif
#endif

// 7.12.3.4  The isnan macro
static inline int mathlib_isnan(float x)
{
	int32 ix;

	MATHLIB_GET_FLOAT_WORD(ix, x);
	ix &= 0x7fffffff;
	ix = 0x7f800000 - ix;
	return (int)(((uint32)ix) >> 31);
}

static inline int mathlib_isnan(double x)
{
	int32 hx, lx;

	MATHLIB_EXTRACT_WORDS(hx, lx, x);
	hx &= 0x7fffffff;
	hx |= (uint32)(lx|(-lx)) >> 31;
	hx = 0x7ff00000 - hx;
	return (int)(((uint32)hx) >> 31);
}

#ifndef isnan
#define isnan(x) mathlib_isnan(x)
#endif

// 7.12.3.6  The signbit macro
#ifndef signbit
#ifndef mathlib_signbitf
extern int mathlib_signbitf(float x);
#endif
#ifndef mathlib_signbit
extern int mathlib_signbit(double x);
#endif
#ifndef mathlib_signbitl
extern int mathlib_signbitl(long double x);
#endif
#define signbit(x)										\
		(sizeof (x) == sizeof (float)					\
		 ? mathlib_signbitf (x)							\
		 : sizeof (x) == sizeof (double)				\
		 ? mathlib_signbit (x) : mathlib_signbitl (x))
#endif

// 7.12.14.1  The isgreater macro
// FIXME: this is wrong for unordered values
#ifndef isgreater
#define isgreater(x, y) ((x) > (y))
#endif

// 7.12.14.3  The isless macro
// FIXME: this is wrong for unordered values
#ifndef isless
#define isless(x, y) ((x) < (y))
#endif

#endif /* MATHLIB_H */
