/*
 *  ppc-config.hpp - PowerPC core emulator config
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
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

#ifndef PPC_CONFIG_H
#define PPC_CONFIG_H

/**
 *	PPC_NO_BASIC_CPU_BASE
 *
 *		Define to not inherit from basic_cpu, thus removing two
 *		vtables. Otherwise, access to registers require an extra
 *		offset from "this" because vtables are stored before other
 *		regular members.
 **/

#ifndef PPC_NO_BASIC_CPU_BASE
#undef  PPC_NO_BASIC_CPU_BASE
#endif


/**
 *	PPC_NO_STATIC_II_INDEX_TABLE
 *
 *		Define to make sure the ii_index_table[] is a non static
 *		member so that powerpc_cpu object size is reduced by 64
 *		KB. This is only supported for mono CPU configurations.
 **/

#ifndef PPC_NO_STATIC_II_INDEX_TABLE
#define PPC_NO_STATIC_II_INDEX_TABLE
#endif


/**
 *	PPC_OPCODE_HASH_XO_PRIMARY
 *
 *		Define to hash opcode hash (xo, primary opcode) instead of
 *		(primary opcode, xo). This simply reduces the computation
 *		index into instr_info[] table by one operation.
 **/

#ifndef PPC_OPCODE_HASH_XO_PRIMARY
#define PPC_OPCODE_HASH_XO_PRIMARY
#endif


/**
 *	PPC_NO_FPSCR_UPDATE
 *
 *		Define to not touch to FPSCR register. This is only useful for
 *		debugging purposes and side-by-side comparision with other
 *		PowerPC emulators that don't handle the FPSCR register.
 **/

#ifndef PPC_NO_FPSCR_UPDATE
#define PPC_NO_FPSCR_UPDATE
#endif


/**
 *	PPC_LAZY_PC_UPDATE
 *
 *		Define to update program counter lazily, i.e. update it only
 *		on branch instructions. On entry of a block, program counter
 *		is speculatively set to the last instruction of that block.
 **/

#ifndef PPC_LAZY_PC_UPDATE
#define PPC_LAZY_PC_UPDATE
#endif
#ifdef  PPC_NO_LAZY_PC_UPDATE
#undef  PPC_LAZY_PC_UPDATE
#endif


/**
 *	PPC_LAZY_CC_UPDATE
 *
 *		Define to update condition code register lazily, i.e. (LT, GT,
 *		EQ) fields will be computed on-demand from the last recorded
 *		operation result. (SO) is always copied from the XER register.
 *
 *		This implies PPC_HAVE_SPLIT_CR to be set. See below.
 **/

#ifndef PPC_LAZY_CC_UPDATE
#undef  PPC_LAZY_CC_UPDATE
#endif


/**
 *	PPC_HAVE_SPLIT_CR
 *
 *		Define to split condition register fields into 8 smaller
 *		aggregates. This is only useful for JIT backends where we
 *		don't want to bother shift-masking CR values.
 **/

#ifndef PPC_HAVE_SPLIT_CR
#undef  PPC_HAVE_SPLIT_CR
#endif


/**
 *	PPC_NO_DECODE_CACHE
 *
 *		Define to disable the decode cache. This is only useful for
 *		debugging purposes and side-by-side comparison with other
 *		PowerPC emulators.
 **/

#ifndef PPC_NO_DECODE_CACHE
#undef  PPC_NO_DECODE_CACHE
#endif


/**
 *	PPC_NO_DECODE_CACHE_UNROLL_EXECUTE
 *
 *		Define to disable decode_cache[] execute loop unrolling. This
 *		is a manual unrolling as a Duff's device makes things worse.
 **/

#ifndef PPC_NO_DECODE_CACHE_UNROLL_EXECUTE
#undef  PPC_NO_DECODE_CACHE_UNROLL_EXECUTE
#endif


/**
 *	PPC_EXECUTE_DUMP_STATE
 *
 *		Define to dump state after each instruction. This also
 *		disables the decode cache.
 **/

#ifndef PPC_EXECUTE_DUMP_STATE
#undef  PPC_EXECUTE_DUMP_STATE
#endif


/**
 *	PPC_FLIGHT_RECORDER
 *
 *		Define to enable the flight recorder. If set to 2, the
 *		complete register state will be recorder after each
 *		instruction execution.
 **/

#ifndef PPC_FLIGHT_RECORDER
#undef  PPC_FLIGHT_RECORDER
#endif


/**
 *		Sanity checks and features enforcements
 **/

#ifdef SHEEPSHAVER
#define PPC_NO_BASIC_CPU_BASE
#undef PPC_NO_STATIC_II_INDEX_TABLE
#endif

#if defined(PPC_FLIGHT_RECORDER) && !defined(PPC_NO_DECODE_CACHE)
#define PPC_NO_DECODE_CACHE
#endif

#if defined(PPC_EXECUTE_DUMP_STATE) && !defined(PPC_NO_DECODE_CACHE)
#define PPC_NO_DECODE_CACHE
#endif

#ifdef PPC_NO_DECODE_CACHE
#undef PPC_LAZY_PC_UPDATE
#endif

#if PPC_FLIGHT_RECORDER
#undef PPC_LAZY_PC_UPDATE
#endif

#if defined(PPC_LAZY_CC_UPDATE) && !defined(PPC_HAVE_SPLIT_CR)
#define PPC_HAVE_SPLIT_CR
#endif

#endif /* PPC_CONFIG_H */
