/*
 *  sysdeps.h - System dependent definitions for Windows
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifndef __STDC__
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
#include <time.h>
#ifdef __WIN32__
#include <windows.h>
#endif
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

#ifdef __WIN32__
typedef int64 loff_t;
#endif
#ifndef HAVE_CADDR_T
typedef char * caddr_t;
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

/* Alignment restrictions */
#if defined(__i386__) || defined(__powerpc__) || defined(__m68k__) || defined(__x86_64__)
# define CPU_CAN_ACCESS_UNALIGNED
#endif

/* Timing functions */
extern void timer_init(void);
extern uint64 GetTicks_usec(void);
extern void Delay_usec(uint32 usec);

/* Spinlocks */
#ifdef __GNUC__

#if defined(__powerpc__) || defined(__ppc__)
#define HAVE_TEST_AND_SET 1
static inline int testandset(volatile int *p)
{
	int ret;
	__asm__ __volatile__("0:    lwarx	%0,0,%1\n"
						 "      xor.	%0,%3,%0\n"
						 "      bne		1f\n"
						 "      stwcx.	%2,0,%1\n"
						 "      bne-	0b\n"
						 "1:    "
						 : "=&r" (ret)
						 : "r" (p), "r" (1), "r" (0)
						 : "cr0", "memory");
	return ret;
}
#endif

#if defined(__i386__) || defined(__x86_64__)
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
#endif

#ifdef __alpha__
#define HAVE_TEST_AND_SET 1
static inline int testandset(volatile int *p)
{
	int ret;
	unsigned long one;

	__asm__ __volatile__("0:	mov 1,%2\n"
						 "	ldl_l %0,%1\n"
						 "	stl_c %2,%1\n"
						 "	beq %2,1f\n"
						 ".subsection 2\n"
						 "1:	br 0b\n"
						 ".previous"
						 : "=r" (ret), "=m" (*p), "=r" (one)
						 : "m" (*p));
	return ret;
}
#endif

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

/* UAE CPU defines */
#ifdef WORDS_BIGENDIAN

#ifdef CPU_CAN_ACCESS_UNALIGNED

/* Big-endian CPUs which can do unaligned accesses */
static inline uae_u32 do_get_mem_long(uae_u32 *a) {return *a;}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {return *a;}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {*a = v;}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {*a = v;}

#else /* CPU_CAN_ACCESS_UNALIGNED */

/* Big-endian CPUs which can not do unaligned accesses (this is not the most efficient way to do this...) */
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint8 *b = (uint8 *)a; return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint8 *b = (uint8 *)a; return (b[0] << 8) | b[1];}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {uint8 *b = (uint8 *)a; b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {uint8 *b = (uint8 *)a; b[0] = v >> 8; b[1] = v;}

#endif /* CPU_CAN_ACCESS_UNALIGNED */

#else /* WORDS_BIGENDIAN */

#if defined(__i386__) || defined(__x86_64__)

/* Intel x86 */
#define X86_PPRO_OPT
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint32 retval; __asm__ ("bswap %0" : "=r" (retval) : "0" (*a) : "cc"); return retval;}
#ifdef X86_PPRO_OPT
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint32 retval; __asm__ ("movzwl %w1,%k0\n\tshll $16,%k0\n\tbswapl %k0\n" : "=&r" (retval) : "m" (*a) : "cc"); return retval;}
#else
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint32 retval; __asm__ ("xorl %k0,%k0\n\tmovw %w1,%w0\n\trolw $8,%w0" : "=&r" (retval) : "m" (*a) : "cc"); return retval;}
#endif
#define HAVE_GET_WORD_UNSWAPPED
#define do_get_mem_word_unswapped(a) ((uae_u32)*((uae_u16 *)(a)))
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v) : "cc"); *a = v;}
#ifdef X86_PPRO_OPT
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {__asm__ ("bswapl %0" : "=&r" (v) : "0" (v << 16) : "cc"); *a = v;}
#else
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {__asm__ ("rolw $8,%0" : "=r" (v) : "0" (v) : "cc"); *a = v;}
#endif
#define HAVE_OPTIMIZED_BYTESWAP_32
/* bswap doesn't affect condition codes */
static inline uae_u32 do_byteswap_32_g(uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v)); return v;}
#define HAVE_OPTIMIZED_BYTESWAP_16
#ifdef X86_PPRO_OPT
static inline uae_u32 do_byteswap_16_g(uae_u32 v) {__asm__ ("bswapl %0" : "=&r" (v) : "0" (v << 16) : "cc"); return v;}
#else
static inline uae_u32 do_byteswap_16_g(uae_u32 v) {__asm__ ("rolw $8,%0" : "=r" (v) : "0" (v) : "cc"); return v;}
#endif

#elif defined(CPU_CAN_ACCESS_UNALIGNED)

/* Other little-endian CPUs which can do unaligned accesses */
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint32 x = *a; return (x >> 24) | (x >> 8) & 0xff00 | (x << 8) & 0xff0000 | (x << 24);}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint16 x = *a; return (x >> 8) | (x << 8);}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {*a = (v >> 24) | (v >> 8) & 0xff00 | (v << 8) & 0xff0000 | (v << 24);}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {*a = (v >> 8) | (v << 8);}

#else /* CPU_CAN_ACCESS_UNALIGNED */

/* Other little-endian CPUs which can not do unaligned accesses (this needs optimization) */
static inline uae_u32 do_get_mem_long(uae_u32 *a) {uint8 *b = (uint8 *)a; return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];}
static inline uae_u32 do_get_mem_word(uae_u16 *a) {uint8 *b = (uint8 *)a; return (b[0] << 8) | b[1];}
static inline void do_put_mem_long(uae_u32 *a, uae_u32 v) {uint8 *b = (uint8 *)a; b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;}
static inline void do_put_mem_word(uae_u16 *a, uae_u32 v) {uint8 *b = (uint8 *)a; b[0] = v >> 8; b[1] = v;}

#endif /* CPU_CAN_ACCESS_UNALIGNED */

#endif /* WORDS_BIGENDIAN */

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

#define ATTRIBUTE_PACKED __attribute__((packed))

#if defined(X86_ASSEMBLY) || defined(X86_64_ASSEMBLY)
#define ASM_SYM_FOR_FUNC(a) __asm__(a)
#else
#define ASM_SYM_FOR_FUNC(a)
#endif

#ifndef REGPARAM
# define REGPARAM
#endif
#define REGPARAM2

#endif
