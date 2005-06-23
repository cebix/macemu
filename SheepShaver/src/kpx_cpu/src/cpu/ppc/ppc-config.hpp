/*
 *  ppc-config.hpp - PowerPC core emulator config
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

#ifndef PPC_CONFIG_H
#define PPC_CONFIG_H

/**
 *	PPC_CHECK_INTERRUPTS
 *
 *		Define if interrupts need to be check after each instruction,
 *		in interpreted mode, or at the end of each block, in compiled
 *		mode.
 *
 *		NOTE: this only checks for user defined interrupts that are
 *		triggered by the program. This is not about OEA interrupts.
 */

#ifndef PPC_CHECK_INTERRUPTS
#define PPC_CHECK_INTERRUPTS 0
#endif


/**
 *	PPC_ENABLE_FPU_EXCEPTIONS
 *
 *		Define to enable a more precise FPU emulation with support for
 *		exception bits. This is slower and not fully correct yet.
 **/

#ifndef PPC_ENABLE_FPU_EXCEPTIONS
#define PPC_ENABLE_FPU_EXCEPTIONS 0
#endif


/**
 *	PPC_DECODE_CACHE
 *
 *		Define to 0 to disable the decode cache. This is only useful
 *		for debugging purposes and side-by-side comparison with other
 *		PowerPC emulators.
 **/

#ifndef PPC_DECODE_CACHE
#define PPC_DECODE_CACHE 1
#endif


/**
 *	PPC_ENABLE_JIT
 *
 *		Define to 1 if dynamic translation is used. This requires
 *		dyngen to be enabled first.
 **/

#ifndef PPC_ENABLE_JIT
#define PPC_ENABLE_JIT ENABLE_DYNGEN
#endif


/**
 *	PPC_REENTRANT_JIT
 *
 *		Define to 1 if we are guaranteed to be able to invoke the JIT
 *		compiler, and the generated code, recursively. Enable this
 *		only if you have necessary provisions to recover from possible
 *		cache invalidatation within inner calls.
 **/

#ifndef PPC_REENTRANT_JIT
#define PPC_REENTRANT_JIT 0
#endif


/**
 *	PPC_EXECUTE_DUMP_STATE
 *
 *		Define to dump state after each instruction. This also
 *		disables the decode cache.
 **/

#ifndef PPC_EXECUTE_DUMP_STATE
#define PPC_EXECUTE_DUMP_STATE 0
#endif


/**
 *	PPC_FLIGHT_RECORDER
 *
 *		Define to enable the flight recorder. If set to 2, the
 *		complete register state will be recorder after each
 *		instruction execution.
 **/

#ifndef PPC_FLIGHT_RECORDER
#define PPC_FLIGHT_RECORDER 0
#endif


/**
 *	PPC_PROFILE_COMPILE_TIME
 *
 *		Define to enable some compile time statistics. This concerns
 *		time spent into the decoder (PPC_DECODE_CACHE case) or total
 *		time spent into the dynamic translator (PPC_ENABLE_JIT case).
 **/

#ifndef PPC_PROFILE_COMPILE_TIME
#define PPC_PROFILE_COMPILE_TIME 0
#endif


/**
 *	PPC_PROFILE_GENERIC_CALLS
 *
 *		Define to enable some generic handler invocation statistics.
 **/

#ifndef PPC_PROFILE_GENERIC_CALLS
#define PPC_PROFILE_GENERIC_CALLS 0
#endif


/**
 *	PPC_PROFILE_REGS_USE
 *
 *		Define to enable some statistics about registers use.
 **/

#ifndef PPC_PROFILE_REGS_USE
#define PPC_PROFILE_REGS_USE 0
#endif

#endif /* PPC_CONFIG_H */
