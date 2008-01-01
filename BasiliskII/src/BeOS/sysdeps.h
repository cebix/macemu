/*
 *  sysdeps.h - System dependent definitions for BeOS
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#ifndef SYSDEPS_H
#define SYSDEPS_H

#ifdef __POWERPC__
#define NO_STD_NAMESPACE
#endif

#include <assert.h>
#include <support/SupportDefs.h>
#include <support/ByteOrder.h>

#include "user_strings_beos.h"

// Are the Mac and the host address space the same?
#ifdef __i386__
#define REAL_ADDRESSING 0
#undef WORDS_BIGENDIAN
#else
#define REAL_ADDRESSING 1
#define WORDS_BIGENDIAN 1
#endif

// Using 68k emulator
#define EMULATED_68K 1

// Mac ROM is write protected
#define ROM_IS_WRITE_PROTECTED 1

// ExtFS is supported
#define SUPPORTS_EXTFS 1

// BSD socket API is supported
#define SUPPORTS_UDP_TUNNEL 1

// mon is not supported
#undef ENABLE_MON

// Time data type for Time Manager emulation
typedef bigtime_t tm_time_t;

// 64 bit file offsets
typedef off_t loff_t;

// Networking types
#define PF_INET AF_INET
typedef int socklen_t;

// UAE CPU data types
#define uae_s8 int8
#define uae_u8 uint8
#define uae_s16 int16
#define uae_u16 uint16
#define uae_s32 int32
#define uae_u32 uint32
#define uae_s64 int64
#define uae_u64 uint64
typedef uae_u32 uaecptr;
#define VAL64(a) (a ## LL)
#define UVAL64(a) (a ## uLL)
typedef uint32 uintptr;
typedef int32 intptr;

/* Timing functions */
extern void Delay_usec(uint32 usec);

// UAE CPU defines
#ifdef __i386__

// Intel x86 assembler optimizations
#define X86_PPRO_OPT
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint32 retval; __asm__ ("bswap %0" : "=r" (retval) : "0" (*a) : "cc"); return retval;}
#ifdef X86_PPRO_OPT
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint32 retval; __asm__ ("movzwl %w1,%k0\n\tshll $16,%k0\n\tbswap %k0\n" : "=&r" (retval) : "m" (*a) : "cc"); return retval;}
#else
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint32 retval; __asm__ ("xorl %k0,%k0\n\tmovw %w1,%w0\n\trolw $8,%w0" : "=&r" (retval) : "m" (*a) : "cc"); return retval;}
#endif
#define HAVE_GET_WORD_UNSWAPPED
#define do_get_mem_word_unswapped(a) ((uae_u32)*((uae_u16 *)(a)))
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v) : "cc"); *a = v;}
#ifdef X86_PPRO_OPT
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {__asm__ ("bswap %0" : "=&r" (v) : "0" (v << 16) : "cc"); *a = v;}
#else
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {__asm__ ("rolw $8,%0" : "=r" (v) : "0" (v) : "cc"); *a = v;}
#endif

#define X86_ASSEMBLY
#define UNALIGNED_PROFITABLE
#define OPTIMIZED_FLAGS
#define ASM_SYM_FOR_FUNC(a) __asm__(a)
#define REGPARAM __attribute__((regparm(3)))

#else

// PowerPC (memory.cpp not used, so no optimization neccessary)
static inline uae_u32 do_get_mem_long(uae_u32 *a) {return *a;}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {return *a;}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {*a = v;}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {*a = v;}

#undef X86_ASSEMBLY
#define UNALIGNED_PROFITABLE
#undef OPTIMIZED_FLAGS
#define ASM_SYM_FOR_FUNC(a)
#define REGPARAM
#endif

#define do_get_mem_byte(a) ((uae_u32)*((uae_u8 *)(a)))
#define do_put_mem_byte(a, v) (*(uae_u8 *)(a) = (v))

#define call_mem_get_func(func, addr) ((*func)(addr))
#define call_mem_put_func(func, addr, v) ((*func)(addr, v))
#define __inline__ inline
#define CPU_EMU_SIZE 0
#undef NO_INLINE_MEMORY_ACCESS
#undef MD_HAVE_MEM_1_FUNCS
#undef USE_COMPILER
#define REGPARAM2
#define ENUMDECL typedef enum
#define ENUMNAME(name) name
#define write_log printf

#endif
