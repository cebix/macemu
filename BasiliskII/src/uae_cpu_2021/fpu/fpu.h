/*
 * fpu/fpu.h - public header
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

#ifndef FPU_PUBLIC_HEADER_H
#define FPU_PUBLIC_HEADER_H

#ifndef FPU_DEBUG
#define FPU_DEBUG 0
#endif

#if FPU_DEBUG
#define fpu_debug(args)			printf args;
#define FPU_DUMP_REGISTERS		0
#define FPU_DUMP_FIRST_BYTES	0
#else
#define fpu_debug(args)	;
#undef FPU_DUMP_REGISTERS
#undef FPU_DUMP_FIRST_BYTES
#endif

#include "sysdeps.h"
#include "fpu/types.h"
#include "fpu/core.h"

void fpu_set_fpsr(uae_u32 new_fpsr);
uae_u32 fpu_get_fpsr(void);
void fpu_set_fpcr(uae_u32 new_fpcr);
uae_u32 fpu_get_fpcr(void);

#endif /* FPU_PUBLIC_HEADER_H */
