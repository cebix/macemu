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

// Arch-dependent definitions
#if defined(__i386__)
#include "mathlib/ieeefp-i386.hpp"
#endif

#ifdef HAVE_FENV_H
#include <fenv.h>
#else

// Rounding control
extern "C" int fegetround(void);
extern "C" int fesetround(int);

#endif /* FENV_H */

#endif /* IEEEFP_H */
