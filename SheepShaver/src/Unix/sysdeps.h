/*
 *  sysdeps.h - System dependent definitions for Linux
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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
#include "user_strings_unix.h"

#ifndef STDC_HEADERS
#error "You don't have ANSI C header files."
#endif

#ifdef HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

// Define for external components
#define SHEEPSHAVER 1

// Mac and host address space are the same
#define REAL_ADDRESSING 1

#define POWERPC_ROM 1

#if EMULATED_PPC
// Handle interrupts asynchronously?
#define ASYNC_IRQ 0
// Mac ROM is write protected when banked memory is used
#if REAL_ADDRESSING || DIRECT_ADDRESSING
# define ROM_IS_WRITE_PROTECTED 0
# define USE_SCRATCHMEM_SUBTERFUGE 1
#else
# define ROM_IS_WRITE_PROTECTED 1
#endif
// Configure PowerPC emulator
#define PPC_NO_LAZY_PC_UPDATE 1
#define PPC_FLIGHT_RECORDER 1
#else
// Mac ROM is write protected
#define ROM_IS_WRITE_PROTECTED 1
#define USE_SCRATCHMEM_SUBTERFUGE 0
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

// Helper functions to byteswap data
#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#ifndef bswap_16
#define bswap_16 generic_bswap_16
#endif

static inline uint16 generic_bswap_16(uint16 x)
{
  return ((x & 0xff) << 8) | ((x >> 8) & 0xff);
}

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

// spin locks
#ifdef __GNUC__

#ifdef __powerpc__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
{
	int ret;
	__asm__ __volatile__("0:    lwarx %0,0,%1 ;"
						 "      xor. %0,%3,%0;"
						 "      bne 1f;"
						 "      stwcx. %2,0,%1;"
						 "      bne- 0b;"
						 "1:    "
						 : "=&r" (ret)
						 : "r" (p), "r" (1), "r" (0)
						 : "cr0", "memory");
	return ret;
}
#endif

#ifdef __i386__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
{
	char ret;
	long int readval;
	
	__asm__ __volatile__("lock; cmpxchgl %3, %1; sete %0"
						 : "=q" (ret), "=m" (*p), "=a" (readval)
						 : "r" (1), "m" (*p), "a" (0)
						 : "memory");
	return ret;
}
#endif

#ifdef __s390__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
{
	int ret;

	__asm__ __volatile__("0: cs    %0,%1,0(%2)\n"
						 "   jl    0b"
						 : "=&d" (ret)
						 : "r" (1), "a" (p), "0" (*p) 
						 : "cc", "memory" );
	return ret;
}
#endif

#ifdef __alpha__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
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

#ifdef __sparc__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
{
	int ret;

	__asm__ __volatile__("ldstub	[%1], %0"
						 : "=r" (ret)
						 : "r" (p)
						 : "memory");

	return (ret ? 1 : 0);
}
#endif

#ifdef __arm__
#define HAVE_TEST_AND_SET 1
static inline int testandset(int *p)
{
	register unsigned int ret;
	__asm__ __volatile__("swp %0, %1, [%2]"
						 : "=r"(ret)
						 : "0"(1), "r"(p));
	
	return ret;
}
#endif

#endif /* __GNUC__ */

#if HAVE_TEST_AND_SET
#define HAVE_SPINLOCKS 1
typedef int spinlock_t;

static const spinlock_t SPIN_LOCK_UNLOCKED = 0;

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
#endif

// Time data type for Time Manager emulation
#ifdef HAVE_CLOCK_GETTIME
typedef struct timespec tm_time_t;
#else
typedef struct timeval tm_time_t;
#endif

// Setup pthread attributes
extern void Set_pthread_attr(pthread_attr_t *attr, int priority);

// Various definitions
typedef struct rgb_color {
	uint8		red;
	uint8		green;
	uint8		blue;
	uint8		alpha;
} rgb_color;

// Macro for calling MacOS routines
#define CallMacOS(type, tvect) call_macos((uint32)tvect)
#define CallMacOS1(type, tvect, arg1) call_macos1((uint32)tvect, (uint32)arg1)
#define CallMacOS2(type, tvect, arg1, arg2) call_macos2((uint32)tvect, (uint32)arg1, (uint32)arg2)
#define CallMacOS3(type, tvect, arg1, arg2, arg3) call_macos3((uint32)tvect, (uint32)arg1, (uint32)arg2, (uint32)arg3)
#define CallMacOS4(type, tvect, arg1, arg2, arg3, arg4) call_macos4((uint32)tvect, (uint32)arg1, (uint32)arg2, (uint32)arg3, (uint32)arg4)
#define CallMacOS5(type, tvect, arg1, arg2, arg3, arg4, arg5) call_macos5((uint32)tvect, (uint32)arg1, (uint32)arg2, (uint32)arg3, (uint32)arg4, (uint32)arg5)
#define CallMacOS6(type, tvect, arg1, arg2, arg3, arg4, arg5, arg6) call_macos6((uint32)tvect, (uint32)arg1, (uint32)arg2, (uint32)arg3, (uint32)arg4, (uint32)arg5, (uint32)arg6)
#define CallMacOS7(type, tvect, arg1, arg2, arg3, arg4, arg5, arg6, arg7) call_macos7((uint32)tvect, (uint32)arg1, (uint32)arg2, (uint32)arg3, (uint32)arg4, (uint32)arg5, (uint32)arg6, (uint32)arg7)

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

#endif
