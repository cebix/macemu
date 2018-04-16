/*
 * fpu/flags.cpp - Floating-point flags
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

/* NOTE: this file shall be included only from fpu/fpu_*.cpp */
#undef	PRIVATE
#define	PRIVATE /**/

#undef	PUBLIC
#define	PUBLIC	/**/

#undef	FFPU
#define	FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* -------------------------------------------------------------------------- */
/* --- Native X86 floating-point flags                                    --- */
/* -------------------------------------------------------------------------- */

#ifdef FPU_USE_X86_FLAGS

/* Initialization */
void FFPU fpu_init_native_fflags(void)
{
	// Adapted from fpu_x86.cpp
	#define SW_Z_I_NAN_MASK			(SW_C0|SW_C2|SW_C3)
	#define SW_Z					(SW_C3)
	#define SW_I					(SW_C0|SW_C2)
	#define SW_NAN					(SW_C0)
	#define SW_FINITE				(SW_C2)
	#define SW_EMPTY_REGISTER		(SW_C0|SW_C3)
	#define SW_DENORMAL				(SW_C2|SW_C3)
	#define SW_UNSUPPORTED			(0)
	#define SW_N					(SW_C1)
	
	// Sanity checks
	#if (SW_Z != NATIVE_FFLAG_ZERO)
	#error "Incorrect X86 Z fflag"
	#endif
	#if (SW_I != NATIVE_FFLAG_INFINITY)
	#error "Incorrect X86 I fflag"
	#endif
	#if (SW_N != NATIVE_FFLAG_NEGATIVE)
	#error "Incorrect X86 N fflag"
	#endif
	#if (SW_NAN != NATIVE_FFLAG_NAN)
	#error "Incorrect X86 NAN fflag"
	#endif
	
	// Native status word to m68k mappings
	for (uae_u32 i = 0; i < 0x48; i++) {
		to_m68k_fpcond[i] = 0;
		const uae_u32 native_fpcond = i << 8;
		switch (native_fpcond & SW_Z_I_NAN_MASK) {
#ifndef FPU_UAE
//			gb-- enabling it would lead to incorrect drawing of digits
//			in Speedometer Performance Test
			case SW_UNSUPPORTED:
#endif
			case SW_NAN:
			case SW_EMPTY_REGISTER:
				to_m68k_fpcond[i] |= FPSR_CCB_NAN;
				break;
			case SW_FINITE:
			case SW_DENORMAL:
				break;
			case SW_I:
				to_m68k_fpcond[i] |= FPSR_CCB_INFINITY;
				break;
			case SW_Z:
				to_m68k_fpcond[i] |= FPSR_CCB_ZERO;
				break;
		}
		if (native_fpcond & SW_N)
			to_m68k_fpcond[i] |= FPSR_CCB_NEGATIVE;
	}

	// m68k to native status word mappings
	for (uae_u32 i = 0; i < 0x10; i++) {
		const uae_u32 m68k_fpcond = i << 24;
		if (m68k_fpcond & FPSR_CCB_NAN)
			to_host_fpcond[i] = SW_NAN;
		else if (m68k_fpcond & FPSR_CCB_ZERO)
			to_host_fpcond[i] = SW_Z;
		else if (m68k_fpcond & FPSR_CCB_INFINITY)
			to_host_fpcond[i] = SW_I;
		else
			to_host_fpcond[i] = SW_FINITE;
		if (m68k_fpcond & FPSR_CCB_NEGATIVE)
			to_host_fpcond[i] |= SW_N;
	}
	
	// truth-table for FPU conditions
	for (uae_u32 host_fpcond = 0; host_fpcond < 0x08; host_fpcond++) {
		// host_fpcond: C3 on bit 2, C1 and C0 are respectively on bits 1 and 0
		const uae_u32 real_host_fpcond = ((host_fpcond & 4) << 12) | ((host_fpcond & 3) << 8);
		const bool N = ((real_host_fpcond & NATIVE_FFLAG_NEGATIVE) == NATIVE_FFLAG_NEGATIVE);
		const bool Z = ((real_host_fpcond & NATIVE_FFLAG_ZERO) == NATIVE_FFLAG_ZERO);
		const bool NaN = ((real_host_fpcond & NATIVE_FFLAG_NAN) == NATIVE_FFLAG_NAN);
		
		int value;
		for (uae_u32 m68k_fpcond = 0; m68k_fpcond < 0x20; m68k_fpcond++) {
			switch (m68k_fpcond) {
			case 0x00:	value = 0;					break; // False
			case 0x01:	value = Z;					break; // Equal
			case 0x02:	value = !(NaN || Z || N);	break; // Ordered Greater Than
			case 0x03:	value = Z || !(NaN || N);	break; // Ordered Greater Than or Equal
			case 0x04:	value = N && !(NaN || Z);	break; // Ordered Less Than
			case 0x05:	value = Z || (N && !NaN);	break; // Ordered Less Than or Equal
			case 0x06:	value = !(NaN || Z);		break; // Ordered Greater or Less Than
			case 0x07:	value = !NaN;				break; // Ordered
			case 0x08:	value = NaN;				break; // Unordered
			case 0x09:	value = NaN || Z;			break; // Unordered or Equal
			case 0x0a:	value = NaN || !(N || Z);	break; // Unordered or Greater Than
			case 0x0b:	value = NaN || Z || !N;		break; // Unordered or Greater or Equal
			case 0x0c:	value = NaN || (N && !Z);	break; // Unordered or Less Than
			case 0x0d:	value = NaN || Z || N;		break; // Unordered or Less or Equal
			case 0x0e:	value = !Z;					break; // Not Equal
			case 0x0f:	value = 1;					break; // True
			case 0x10:	value = 0;					break; // Signaling False
			case 0x11:	value = Z;					break; // Signaling Equal
			case 0x12:	value = !(NaN || Z || N);	break; // Greater Than
			case 0x13:	value = Z || !(NaN || N);	break; // Greater Than or Equal
			case 0x14:	value = N && !(NaN || Z);	break; // Less Than
			case 0x15:	value = Z || (N && !NaN);	break; // Less Than or Equal
			case 0x16:	value = !(NaN || Z);		break; // Greater or Less Than
			case 0x17:	value = !NaN;				break; // Greater, Less or Equal
			case 0x18:	value = NaN;				break; // Not Greater, Less or Equal
			case 0x19:	value = NaN || Z;			break; // Not Greater or Less Than
			case 0x1a:	value = NaN || !(N || Z);	break; // Not Less Than or Equal
			case 0x1b:	value = NaN || Z || !N;		break; // Not Less Than
			case 0x1c:	value =  NaN || (N && !Z);	break; // Not Greater Than or Equal
//			case 0x1c:	value = !Z && (NaN || N);	break; // Not Greater Than or Equal
			case 0x1d:	value = NaN || Z || N;		break; // Not Greater Than
			case 0x1e:	value = !Z;					break; // Signaling Not Equal
			case 0x1f:	value = 1;					break; // Signaling True
			default:	value = -1;
			}
			fpcond_truth_table[m68k_fpcond][host_fpcond] = value;
		}
	}
}

#endif
