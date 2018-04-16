/*
 * fpu/rounding.cpp - system-dependant FPU rounding mode and precision
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

#undef	PRIVATE
#define	PRIVATE /**/

#undef	PUBLIC
#define	PUBLIC	/**/

#undef	FFPU
#define	FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* -------------------------------------------------------------------------- */
/* --- Native X86 Rounding Mode                                           --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_X86_ROUNDING_MODE
const uae_u32 FFPU x86_control_word_rm_mac2host[] = {
	CW_RC_NEAR,
	CW_RC_ZERO,
	CW_RC_DOWN,
	CW_RC_UP
};
#endif

/* -------------------------------------------------------------------------- */
/* --- Native X86 Rounding Precision                                      --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_X86_ROUNDING_PRECISION
const uae_u32 FFPU x86_control_word_rp_mac2host[] = {
	CW_PC_EXTENDED,
	CW_PC_SINGLE,
	CW_PC_DOUBLE,
	CW_PC_RESERVED
};
#endif
