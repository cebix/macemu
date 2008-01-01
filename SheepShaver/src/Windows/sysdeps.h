/*
 *  sysdeps.h - System dependent definitions for Windows
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#ifdef __WIN32__
#include <windows.h>
#endif
#include <sys/types.h>


// Define for external components
#define SHEEPSHAVER 1
#define POWERPC_ROM 1
#define EMULATED_PPC 1
#define CONFIG_WIN32 1

// Use Direct Addressing mode
#define DIRECT_ADDRESSING 1
#define NATMEM_OFFSET 0x11000000

// Always use the complete (non-stubs based) Ethernet driver
#if DIRECT_ADDRESSING
#define USE_ETHER_FULL_DRIVER 1
#endif

// Mac ROM is write protected when banked memory is used
#if REAL_ADDRESSING || DIRECT_ADDRESSING
# define ROM_IS_WRITE_PROTECTED 0
# define USE_SCRATCHMEM_SUBTERFUGE 1
#else
# define ROM_IS_WRITE_PROTECTED 1
#endif
// Configure PowerPC emulator
#define PPC_REENTRANT_JIT 1
#define PPC_CHECK_INTERRUPTS 1
#define PPC_DECODE_CACHE 1
#define PPC_FLIGHT_RECORDER 1
#define PPC_PROFILE_COMPILE_TIME 0
#define PPC_PROFILE_GENERIC_CALLS 0
#define KPX_MAX_CPUS 1
#if ENABLE_DYNGEN
#define PPC_ENABLE_JIT 1
#endif
#if defined(__i386__)
#define DYNGEN_ASM_OPTS 1
#endif

// Data types
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

// Define if the host processor supports fast unaligned load/stores
#if defined __i386__ || defined __x86_64__
#define UNALIGNED_PROFITABLE 1
#endif


/**
 *		Helper functions to byteswap data
 **/

#if defined(__GNUC__)
#if defined(__x86_64__) || defined(__i386__)
// Linux/AMD64 currently has no asm optimized bswap_32() in <byteswap.h>
#define opt_bswap_32 do_opt_bswap_32
static inline uint32 do_opt_bswap_32(uint32 x)
{
  uint32 v;
  __asm__ __volatile__ ("bswap %0" : "=r" (v) : "0" (x));
  return v;
}
#endif
#endif

#ifdef  opt_bswap_16
#undef  bswap_16
#define bswap_16 opt_bswap_16
#endif
#ifndef bswap_16
#define bswap_16 generic_bswap_16
#endif

static inline uint16 generic_bswap_16(uint16 x)
{
  return ((x & 0xff) << 8) | ((x >> 8) & 0xff);
}

#ifdef  opt_bswap_32
#undef  bswap_32
#define bswap_32 opt_bswap_32
#endif
#ifndef bswap_32
#define bswap_32 generic_bswap_32
#endif

static inline uint32 generic_bswap_32(uint32 x)
{
  return (((x & 0xff000000) >> 24) |
		  ((x & 0x00ff0000) >>  8) |
		  ((x & 0x0000ff00) <<  8) |
		  ((x & 0x000000ff) << 24) );
}

#if defined(__i386__)
#define opt_bswap_64 do_opt_bswap_64
static inline uint64 do_opt_bswap_64(uint64 x)
{
  return (bswap_32(x >> 32) | (((uint64)bswap_32((uint32)x)) << 32));
}
#endif

#ifdef  opt_bswap_64
#undef  bswap_64
#define bswap_64 opt_bswap_64
#endif
#ifndef bswap_64
#define bswap_64 generic_bswap_64
#endif

static inline uint64 generic_bswap_64(uint64 x)
{
  return (((x & UVAL64(0xff00000000000000)) >> 56) |
		  ((x & UVAL64(0x00ff000000000000)) >> 40) |
		  ((x & UVAL64(0x0000ff0000000000)) >> 24) |
		  ((x & UVAL64(0x000000ff00000000)) >>  8) |
		  ((x & UVAL64(0x00000000ff000000)) <<  8) |
		  ((x & UVAL64(0x0000000000ff0000)) << 24) |
		  ((x & UVAL64(0x000000000000ff00)) << 40) |
		  ((x & UVAL64(0x00000000000000ff)) << 56) );
}

#ifdef WORDS_BIGENDIAN
static inline uint16 tswap16(uint16 x) { return x; }
static inline uint32 tswap32(uint32 x) { return x; }
static inline uint64 tswap64(uint64 x) { return x; }
#else
static inline uint16 tswap16(uint16 x) { return bswap_16(x); }
static inline uint32 tswap32(uint32 x) { return bswap_32(x); }
static inline uint64 tswap64(uint64 x) { return bswap_64(x); }
#endif

#define do_byteswap_16_g bswap_16
#define do_byteswap_16_c(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

#define do_byteswap_32_g bswap_32
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

#if defined(__i386__) || defined(__x86_64__)
#define ntohl(x) do_byteswap_32(x)
#define ntohs(x) do_byteswap_16(x)
#define htonl(x) do_byteswap_32(x)
#define htons(x) do_byteswap_16(x)
#endif


/*
 *  Spin locks
 */

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

// Time data type for Time Manager emulation
typedef int64 tm_time_t;

// Timing functions
extern void timer_init(void);
extern uint64 GetTicks_usec(void);
extern void Delay_usec(uint32 usec);

// Various definitions
typedef struct rgb_color {
	uint8		red;
	uint8		green;
	uint8		blue;
	uint8		alpha;
} rgb_color;

// Macro for calling MacOS routines
#define CallMacOS(type, tvect) call_macos((uintptr)tvect)
#define CallMacOS1(type, tvect, arg1) call_macos1((uintptr)tvect, (uintptr)arg1)
#define CallMacOS2(type, tvect, arg1, arg2) call_macos2((uintptr)tvect, (uintptr)arg1, (uintptr)arg2)
#define CallMacOS3(type, tvect, arg1, arg2, arg3) call_macos3((uintptr)tvect, (uintptr)arg1, (uintptr)arg2, (uintptr)arg3)
#define CallMacOS4(type, tvect, arg1, arg2, arg3, arg4) call_macos4((uintptr)tvect, (uintptr)arg1, (uintptr)arg2, (uintptr)arg3, (uintptr)arg4)
#define CallMacOS5(type, tvect, arg1, arg2, arg3, arg4, arg5) call_macos5((uintptr)tvect, (uintptr)arg1, (uintptr)arg2, (uintptr)arg3, (uintptr)arg4, (uintptr)arg5)
#define CallMacOS6(type, tvect, arg1, arg2, arg3, arg4, arg5, arg6) call_macos6((uintptr)tvect, (uintptr)arg1, (uintptr)arg2, (uintptr)arg3, (uintptr)arg4, (uintptr)arg5, (uintptr)arg6)
#define CallMacOS7(type, tvect, arg1, arg2, arg3, arg4, arg5, arg6, arg7) call_macos7((uintptr)tvect, (uintptr)arg1, (uintptr)arg2, (uintptr)arg3, (uintptr)arg4, (uintptr)arg5, (uintptr)arg6, (uintptr)arg7)

#ifdef __cplusplus
extern "C" {
#endif
extern uint32 call_macos(uint32 tvect);
extern uint32 call_macos1(uint32 tvect, uint32 arg1);
extern uint32 call_macos2(uint32 tvect, uint32 arg1, uint32 arg2);
extern uint32 call_macos3(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3);
extern uint32 call_macos4(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4);
extern uint32 call_macos5(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5);
extern uint32 call_macos6(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6);
extern uint32 call_macos7(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6, uint32 arg7);
#ifdef __cplusplus
}
#endif

// Misc platform specific definitions
#ifdef __WIN32__
typedef int64 loff_t;
#endif
#define ATTRIBUTE_PACKED __attribute__((__packed__))

#endif
