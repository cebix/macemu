/*
 *  ieeefp.hpp - IEEE754 Floating-Point Math library
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

#ifndef IEEEFP_H
#define IEEEFP_H

// Can we use C99 extensions in C++ mode?
#ifdef HAVE_FENV_H
#if defined __GNUC__
#define USE_FENV_H 1
#endif
#endif

// Arch-dependent definitions
#if defined(__i386__)
#include "mathlib/ieeefp-i386.hpp"
#endif
#if defined(__mips__) || (defined(sgi) && defined(mips))
#include "mathlib/ieeefp-mips.hpp"
#endif

#ifdef USE_FENV_H
#include <fenv.h>
#else

// Rounding control
extern "C" int fegetround(void);
extern "C" int fesetround(int);

#endif /* FENV_H */

// Make sure previous instructions are executed first
// XXX this is most really a hint to the compiler so that is doesn't
// reorder calls to fe*() functions before the actual compuation...
#if defined __GNUC__
#define febarrier() __asm__ __volatile__ ("")
#endif
#ifndef febarrier
#define febarrier()
#endif

// HOST_FLOAT_WORDS_BIG_ENDIAN is a tristate:
//   yes (1) / no (0) / default (undefined)
#if HOST_FLOAT_WORDS_BIG_ENDIAN
#define FLOAT_WORD_ORDER_BIG_ENDIAN
#elif defined(WORDS_BIGENDIAN)
#define FLOAT_WORD_ORDER_BIG_ENDIAN
#endif

// Representation of an IEEE 754 float
union mathlib_ieee_float_shape_type {
	float value;
	uint32 word;
};

#define MATHLIB_GET_FLOAT_WORD(i,d)				\
do {											\
	mathlib_ieee_float_shape_type gf_u;			\
	gf_u.value = (d);							\
	(i) = gf_u.word;							\
} while (0)

#define MATHLIB_SET_FLOAT_WORD(d,i)				\
do {											\
	mathlib_ieee_float_shape_type sf_u;			\
	sf_u.word = (i);							\
	(d) = sf_u.value;							\
} while (0)

// Representation of an IEEE 754 double
union mathlib_ieee_double_shape_type {
	double value;
	struct {
#ifdef FLOAT_WORD_ORDER_BIG_ENDIAN
		uint32 msw;
		uint32 lsw;
#else
		uint32 lsw;
		uint32 msw;
#endif
	} parts;
};

#define MATHLIB_EXTRACT_WORDS(ix0,ix1,d)		\
do {											\
	mathlib_ieee_double_shape_type ew_u;		\
	ew_u.value = (d);							\
	(ix0) = ew_u.parts.msw;						\
	(ix1) = ew_u.parts.lsw;						\
} while (0)

#define MATHLIB_GET_HIGH_WORD(i,d)				\
do {											\
	mathlib_ieee_double_shape_type gh_u;		\
	gh_u.value = (d);							\
	(i) = gh_u.parts.msw;						\
} while (0)

#define MATHLIB_GET_LOW_WORD(i,d)				\
do {											\
	mathlib_ieee_double_shape_type gl_u;		\
	gl_u.value = (d);							\
	(i) = gl_u.parts.lsw;						\
} while (0)

#endif /* IEEEFP_H */
