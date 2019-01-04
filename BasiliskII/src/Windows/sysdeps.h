/*
 *  sysdeps.h - System dependent definitions for Windows
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

#if !defined _MSC_VER && !defined __STDC__
#error "Your compiler is not ANSI. Get a real one."
#endif

#include "config.h"
#include "user_strings_windows.h"

#ifndef STDC_HEADERS
#error "You don't have ANSI C header files."
#endif

#ifndef WIN32
#define WIN32
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <time.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <sys/types.h>


/* Mac and host address space are distinct */
#ifndef REAL_ADDRESSING
#define REAL_ADDRESSING 0
#endif
#if REAL_ADDRESSING
#error "Real Addressing mode can't work without proper kernel support"
#endif

/* Using 68k emulator */
#define EMULATED_68K 1

/* The m68k emulator uses a prefetch buffer ? */
#define USE_PREFETCH_BUFFER 0

/* Mac ROM is write protected when banked memory is used */
#if REAL_ADDRESSING || DIRECT_ADDRESSING
# define ROM_IS_WRITE_PROTECTED 0
# define USE_SCRATCHMEM_SUBTERFUGE 1
#else
# define ROM_IS_WRITE_PROTECTED 1
#endif

/* Direct Addressing requires Video on SEGV signals in plain X11 mode */
#if DIRECT_ADDRESSING && (!ENABLE_VOSF && !USE_SDL_VIDEO)
# undef  ENABLE_VOSF
# define ENABLE_VOSF 1
#endif

/* ExtFS is supported */
#define SUPPORTS_EXTFS 1

/* POSIX data types missing from Microsoft's CRT */
#ifdef _MSC_VER
typedef ptrdiff_t ssize_t;
#endif

/* Data types */
typedef unsigned char uint8;
typedef signed char int8;
#if SIZEOF_SHORT == 2
typedef unsigned short uint16;
typedef short int16;
#elif SIZEOF_INT == 2
typedef unsigned int uint16;
typedef int int16;
#else
#error "No 2 byte type, you lose."
#endif
#if SIZEOF_INT == 4
typedef unsigned int uint32;
typedef int int32;
#elif SIZEOF_LONG == 4
typedef unsigned long uint32;
typedef long int32;
#else
#error "No 4 byte type, you lose."
#endif
#if SIZEOF_LONG == 8
typedef unsigned long uint64;
typedef long int64;
#define VAL64(a) (a ## l)
#define UVAL64(a) (a ## ul)
#elif SIZEOF_LONG_LONG == 8
typedef unsigned long long uint64;
typedef long long int64;
#define VAL64(a) (a ## LL)
#define UVAL64(a) (a ## uLL)
#else
#error "No 8 byte type, you lose."
#endif
#if SIZEOF_VOID_P == 4
typedef uint32 uintptr;
typedef int32 intptr;
#elif SIZEOF_VOID_P == 8
typedef uint64 uintptr;
typedef int64 intptr;
#else
#error "Unsupported size of pointer"
#endif

#ifdef _WIN32
typedef int64 loff_t;
#endif
#ifndef HAVE_CADDR_T
typedef char * caddr_t;
#endif

#ifdef _MSC_VER
#ifdef _M_IX86
#define __i386__
#elif defined _M_AMD64
#define __x86_64__
#endif
#endif

/* Time data type for Time Manager emulation */
typedef int64 tm_time_t;

/* Define codes for all the float formats that we know of.
 * Though we only handle IEEE format.  */
#define UNKNOWN_FLOAT_FORMAT 0
#define IEEE_FLOAT_FORMAT 1
#define VAX_FLOAT_FORMAT 2
#define IBM_FLOAT_FORMAT 3
#define C4X_FLOAT_FORMAT 4

/* UAE CPU data types */
#define uae_s8 int8
#define uae_u8 uint8
#define uae_s16 int16
#define uae_u16 uint16
#define uae_s32 int32
#define uae_u32 uint32
#define uae_s64 int64
#define uae_u64 uint64
typedef uae_u32 uaecptr;

/* Timing functions */
extern void timer_init(void);
extern uint64 GetTicks_usec(void);
extern void Delay_usec(uint32 usec);

/* Spinlocks */
#ifdef __GNUC__
#define HAVE_TEST_AND_SET 1
static inline int testandset(volatile int *p)
{
	long int ret;
	/* Note: the "xchg" instruction does not need a "lock" prefix */
	__asm__ __volatile__("xchgl %k0, %1"
						 : "=r" (ret), "=m" (*p)
						 : "0" (1), "m" (*p)
						 : "memory");
	return ret;
}
#endif /* __GNUC__ */

typedef volatile int spinlock_t;

static const spinlock_t SPIN_LOCK_UNLOCKED = 0;

#if HAVE_TEST_AND_SET
#define HAVE_SPINLOCKS 1
static inline void spin_lock(spinlock_t *lock)
{
	while (testandset(lock));
}

static inline void spin_unlock(spinlock_t *lock)
{
	*lock = 0;
}

static inline int spin_trylock(spinlock_t *lock)
{
	return !testandset(lock);
}
#else
static inline void spin_lock(spinlock_t *lock)
{
}

static inline void spin_unlock(spinlock_t *lock)
{
}

static inline int spin_trylock(spinlock_t *lock)
{
	return 1;
}
#endif

#define HAVE_OPTIMIZED_BYTESWAP_32
#define HAVE_OPTIMIZED_BYTESWAP_16

#ifdef _MSC_VER
static inline uae_u32 do_get_mem_long(uae_u32 *a) {return _byteswap_ulong(*a);}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {return _byteswap_ushort(*a);}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {*a = _byteswap_ulong(v);}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {*a = _byteswap_ushort(v);}
static inline uae_u32 do_byteswap_32_g(uae_u32 v) {return _byteswap_ulong(v);}
static inline uae_u32 do_byteswap_16_g(uae_u32 v) {return _byteswap_ushort(v);}
#else
/* Intel x86 */
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint32 retval; __asm__ ("bswap %0" : "=r" (retval) : "0" (*a) : "cc"); return retval;}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint32 retval; __asm__ ("movzwl %w1,%k0\n\tshll $16,%k0\n\tbswapl %k0\n" : "=&r" (retval) : "m" (*a) : "cc"); return retval;}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v) : "cc"); *a = v;}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {__asm__ ("bswapl %0" : "=&r" (v) : "0" (v << 16) : "cc"); *a = v;}
/* bswap doesn't affect condition codes */
static inline uae_u32 do_byteswap_32_g(uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v)); return v;}
static inline uae_u32 do_byteswap_16_g(uae_u32 v) {__asm__ ("bswapl %0" : "=&r" (v) : "0" (v << 16) : "cc"); return v;}
#endif

#define HAVE_GET_WORD_UNSWAPPED
#define do_get_mem_word_unswapped(a) ((uae_u32)*((uae_u16 *)(a)))

#ifndef HAVE_OPTIMIZED_BYTESWAP_32
static inline uae_u32 do_byteswap_32_g(uae_u32 v)
	{ return (((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v & 0xff) << 24) | ((v & 0xff00) << 8)); }
#endif

#ifndef HAVE_OPTIMIZED_BYTESWAP_16
static inline uae_u32 do_byteswap_16_g(uae_u32 v)
	{ return (((v >> 8) & 0xff) | ((v & 0xff) << 8)); }
#endif

#define do_byteswap_16_c(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

#define do_byteswap_32_c(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#if defined(__GNUC__)
#define do_byteswap_16(x)						\
		(__extension__							\
		 ({ register uint16 __v, __x = (x);		\
		 if (__builtin_constant_p(__x))			\
		   __v = do_byteswap_16_c(__x);			\
		 else									\
		   __v = do_byteswap_16_g(__x);			\
		 __v; }))

#define do_byteswap_32(x)						\
		(__extension__							\
		 ({ register uint32 __v, __x = (x);		\
		 if (__builtin_constant_p(__x))			\
		   __v = do_byteswap_32_c(__x);			\
		 else									\
		   __v = do_byteswap_32_g(__x);			\
		 __v; }))
#else
#define do_byteswap_16(x) do_byteswap_16_g(x)
#define do_byteswap_32(x) do_byteswap_32_g(x)
#endif

/* Byte-swapping routines */
#if defined(__i386__) || defined(__x86_64__)
#define ntohl(x) do_byteswap_32(x)
#define ntohs(x) do_byteswap_16(x)
#define htonl(x) do_byteswap_32(x)
#define htons(x) do_byteswap_16(x)
#endif

#define do_get_mem_byte(a) ((uae_u32)*((uae_u8 *)(a)))
#define do_put_mem_byte(a, v) (*(uae_u8 *)(a) = (v))

#define call_mem_get_func(func, addr) ((*func)(addr))
#define call_mem_put_func(func, addr, v) ((*func)(addr, v))
#define __inline__ inline
#define CPU_EMU_SIZE 0
#undef NO_INLINE_MEMORY_ACCESS
#undef MD_HAVE_MEM_1_FUNCS
#define ENUMDECL typedef enum
#define ENUMNAME(name) name
#define write_log printf

#if defined(X86_ASSEMBLY) || defined(X86_64_ASSEMBLY)
#define ASM_SYM(a) __asm__(a)
#else
#define ASM_SYM(a)
#endif

#ifndef REGPARAM
# define REGPARAM
#endif
#define REGPARAM2

#ifdef _MSC_VER
#define ATTRIBUTE_PACKED
#endif

#endif
