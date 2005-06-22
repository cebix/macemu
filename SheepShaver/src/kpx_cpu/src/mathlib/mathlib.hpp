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

// 7.12.6.2  The exp2 functions
#ifdef HAVE_EXP2F
extern "C" float exp2f(float x);
#else
#ifdef HAVE_EXP2
extern "C" double exp2(double x);
#define exp2f(x) (float)exp2(x)
#else
#define exp2f(x) powf(2.0, (x))
#endif
#endif

// 7.12.6.10  The log2 functions
#ifdef HAVE_LOG2F
extern "C" float log2f(float x);
#else
#ifdef HAVE_LOG2
extern "C" double log2(double x);
#define log2f(x) (float)log2(x)
#else
#ifndef M_LN2
#define M_LN2 logf(2.0)
#endif
#define log2f(x) logf(x) / M_LN2
#endif
#endif

// 7.12.9.1  The ceil functions
#ifdef HAVE_CEILF
extern "C" float ceilf(float x);
#else
#ifdef HAVE_CEIL
extern "C" double ceil(double x);
#define ceilf(x) (float)ceil(x)
#endif
#endif

// 7.12.9.2  The floor functions
#ifdef HAVE_FLOORF
extern "C" float floorf(float x);
#else
#ifdef HAVE_FLOOR
extern "C" double floor(double x);
#define floorf(x) (float)floor(x)
#endif
#endif

// 7.12.9.6  The round functions
#ifdef HAVE_ROUNDF
extern "C" float roundf(float x);
#else
#ifdef HAVE_ROUND
extern "C" double round(double x);
#define roundf(x) (float)round(x)
#endif
#endif

// 7.12.9.8  The trunc functions
#ifdef HAVE_TRUNCF
extern "C" float truncf(float x);
#else
#ifdef HAVE_TRUNC
extern "C" double trunc(double x);
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
