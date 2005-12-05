/*
 *  jit-config.hpp - JIT config utils
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

#ifndef JIT_CONFIG_H
#define JIT_CONFIG_H

/**
 *	ENABLE_DYNGEN
 *
 *		Define to enable the portable "JIT1" engine based on code
 *		inlining technique as implemented in QEMU.
 **/

#ifndef ENABLE_DYNGEN
#define ENABLE_DYNGEN 0
#endif

/**
 *	DYNGEN_ASM_OPTS
 *
 *		Define to permit host inline asm optimizations. This is
 *		particularly useful to compute emulated condition code
 *		registers.
 **/

#if ENABLE_DYNGEN
#ifndef DYNGEN_ASM_OPTS
#define DYNGEN_ASM_OPTS 0
#endif
#endif

/**
 *	DYNGEN_DIRECT_BLOCK_CHAINING
 *
 *		Define to enable direct block chaining on platforms supporting
 *		that feature. e.g. PowerPC.
 **/

#if ENABLE_DYNGEN
#ifndef DYNGEN_DIRECT_BLOCK_CHAINING
#define DYNGEN_DIRECT_BLOCK_CHAINING 1
#endif
#endif

#endif /* JIT_CONFIG_H */
