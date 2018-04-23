/*
 *  sysdeps.h - System dependent definitions for Unix
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

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif


#if 1
//Too lazy to port the checks to cmake right now
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define if your system supports TUN/TAP devices. */
/* #undef ENABLE_TUNTAP */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define if your system has <asm/ucontext.h> header. */
/* #undef HAVE_ASM_UCONTEXT */

/* Define to 1 if you have the `atanh' function. */
#define HAVE_ATANH 1

/* Define to 1 if you have the <AvailabilityMacros.h> header file. */
#define HAVE_AVAILABILITYMACROS_H 1

/* Define to 1 if the system has the type `caddr_t'. */
#define HAVE_CADDR_T 1

/* Define to 1 if you have the `cfmakeraw' function. */
#define HAVE_CFMAKERAW 1

/* Define to 1 if you have the `clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `finite' function. */
#define HAVE_FINITE 1

/* Define to 1 if you have the <floatingpoint.h> header file. */
/* #undef HAVE_FLOATINGPOINT_H */

/* Define if framework AppKit is available. */
#define HAVE_FRAMEWORK_APPKIT 1

/* Define if framework Carbon is available. */
#define HAVE_FRAMEWORK_CARBON 1

/* Define if framework CoreFoundation is available. */
#define HAVE_FRAMEWORK_COREFOUNDATION 1

/* Define if framework IOKit is available. */
#define HAVE_FRAMEWORK_IOKIT 1

/* Define to 1 if you have the <history.h> header file. */
/* #undef HAVE_HISTORY_H */

/* Define to 1 if you have the <ieee754.h> header file. */
/* #undef HAVE_IEEE754_H */

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <IOKit/storage/IOBlockStorageDevice.h> header
   file. */
#define HAVE_IOKIT_STORAGE_IOBLOCKSTORAGEDEVICE_H 1

/* Define to 1 if you have the `isinf' function. */
#define HAVE_ISINF 1

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 1

/* Define to 1 if you have the `isnormal' function. */
/* #undef HAVE_ISNORMAL */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `posix4' library (-lposix4). */
/* #undef HAVE_LIBPOSIX4 */

/* Define to 1 if you have the `pthread' library (-lpthread). */
#define HAVE_LIBPTHREAD 1

/* Define to 1 if you have the `PTL' library (-lPTL). */
/* #undef HAVE_LIBPTL */

/* Define to 1 if you have the `rt' library (-lrt). */
/* #undef HAVE_LIBRT */

/* Define if there is a linker script to relocate the executable above
   0x70000000. */
/* #undef HAVE_LINKER_SCRIPT */

/* Define to 1 if the system has the type `loff_t'. */
/* #undef HAVE_LOFF_T */

/* Define if your system supports Mach exceptions. */
#define HAVE_MACH_EXCEPTIONS 1

/* Define to 1 if you have the <mach/mach.h> header file. */
#define HAVE_MACH_MACH_H 1

/* Define to 1 if you have the `mach_task_self' function. */
#define HAVE_MACH_TASK_SELF 1

/* Define if your system has a working vm_allocate()-based memory allocator.
   */
#define HAVE_MACH_VM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define if <sys/mman.h> defines MAP_ANON and mmap()'ing with MAP_ANON works.
   */
/* #undef HAVE_MMAP_ANON */

/* Define if <sys/mman.h> defines MAP_ANONYMOUS and mmap()'ing with
   MAP_ANONYMOUS works. */
/* #undef HAVE_MMAP_ANONYMOUS */

/* Define if your system has a working mmap()-based memory allocator. */
/* #undef HAVE_MMAP_VM */

/* Define to 1 if you have the `mprotect' function. */
#define HAVE_MPROTECT 1

/* Define to 1 if you have the `munmap' function. */
#define HAVE_MUNMAP 1

/* Define to 1 if you have the <nan.h> header file. */
/* #undef HAVE_NAN_H */

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define if pthreads are available. */
#define HAVE_PTHREADS 1

/* Define to 1 if you have the `pthread_cancel' function. */
#define HAVE_PTHREAD_CANCEL 1

/* Define to 1 if you have the `pthread_cond_init' function. */
#define HAVE_PTHREAD_COND_INIT 1

/* Define to 1 if you have the `pthread_mutexattr_setprotocol' function. */
#define HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL 1

/* Define to 1 if you have the `pthread_mutexattr_setpshared' function. */
#define HAVE_PTHREAD_MUTEXATTR_SETPSHARED 1

/* Define to 1 if you have the `pthread_mutexattr_settype' function. */
#define HAVE_PTHREAD_MUTEXATTR_SETTYPE 1

/* Define to 1 if you have the `pthread_testcancel' function. */
#define HAVE_PTHREAD_TESTCANCEL 1

/* Define to 1 if you have the <readline.h> header file. */
/* #undef HAVE_READLINE_H */

/* Define to 1 if you have the <readline/history.h> header file. */
#define HAVE_READLINE_HISTORY_H 1

/* Define to 1 if you have the <readline/readline.h> header file. */
#define HAVE_READLINE_READLINE_H 1

/* Define to 1 if you have the `sem_init' function. */
#define HAVE_SEM_INIT 1

/* Define to 1 if you have the `sigaction' function. */
#define HAVE_SIGACTION 1

/* Define if we know a hack to replace siginfo_t->si_addr member. */
/* #undef HAVE_SIGCONTEXT_SUBTERFUGE */

/* Define if your system supports extended signals. */
/* #undef HAVE_SIGINFO_T */

/* Define to 1 if you have the `signal' function. */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the `signbit' function. */
/* #undef HAVE_SIGNBIT */

/* Define if we can ignore the fault (instruction skipping in SIGSEGV
   handler). */
#define HAVE_SIGSEGV_SKIP_INSTRUCTION 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <stropts.h> header file. */
/* #undef HAVE_STROPTS_H */

/* Define to 1 if you have the <sys/bitypes.h> header file. */
/* #undef HAVE_SYS_BITYPES_H */

/* Define to 1 if you have the <sys/filio.h> header file. */
#define HAVE_SYS_FILIO_H 1

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/stropts.h> header file. */
/* #undef HAVE_SYS_STROPTS_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `task_self' function. */
/* #undef HAVE_TASK_SELF */

/* Define to 1 if you have the `timer_create' function. */
/* #undef HAVE_TIMER_CREATE */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vm_allocate' function. */
#define HAVE_VM_ALLOCATE 1

/* Define to 1 if you have the `vm_deallocate' function. */
#define HAVE_VM_DEALLOCATE 1

/* Define to 1 if you have the `vm_protect' function. */
#define HAVE_VM_PROTECT 1

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Define this program name. */
#define PACKAGE "Basilisk II"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "Christian.Bauer@uni-mainz.de"

/* Define to the full name of this package. */
#define PACKAGE_NAME "Basilisk II"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "Basilisk II 1.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "BasiliskII"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.0"

/* Define if the __PAGEZERO Mach-O Low Memory Globals hack works on this
   system. */
/* #undef PAGEZERO_HACK */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define if your system requires sigactions to be reinstalled. */
/* #undef SIGACTION_NEED_REINSTALL */

/* Define if your system requires signals to be reinstalled. */
/* #undef SIGNAL_NEED_REINSTALL */

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 16

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define if BSD-style non-blocking I/O is to be used */
/* #undef USE_FIONBIO */

/* Define to enble SDL support */
#define USE_SDL 1

/* Define to enable SDL audio support */
#define USE_SDL_AUDIO 1

/* Define to enable SDL video graphics support */
#define USE_SDL_VIDEO 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Define this program version. */
#define VERSION "1.0"

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to 'int' if <sys/types.h> doesn't define. */
/* #undef socklen_t */
#else
#include "config.h"
#endif
//Checks end
#ifndef NEED_CONFIG_H_ONLY

#include "user_strings_unix.h"

#ifdef HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

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

#if defined(__MACH__)
#include <mach/clock.h>
#endif

/* Using 68k emulator */
#define EMULATED_68K 1

/* The m68k emulator uses a prefetch buffer ? */
#define USE_PREFETCH_BUFFER 0

/* Mac ROM is write protected when banked memory is used */
#if DIRECT_ADDRESSING
# define ROM_IS_WRITE_PROTECTED 0
# define USE_SCRATCHMEM_SUBTERFUGE 1
#else
# define ROM_IS_WRITE_PROTECTED 1
#endif

/* ExtFS is supported */
#define SUPPORTS_EXTFS 1

/* BSD socket API supported */
#define SUPPORTS_UDP_TUNNEL 1

/* Use the CPU emulator to check for periodic tasks? */
#ifdef HAVE_PTHREADS
#define USE_PTHREADS_SERVICES
#endif
#ifdef USE_CPU_EMUL_SERVICES
#undef USE_PTHREADS_SERVICES
#endif


/* Data types */
typedef uint8_t uint8;
typedef int8_t int8;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint64_t uint64;
typedef int64_t int64;
#ifndef _UINT64
#define _UINT64
#endif
#define VAL64(a) (a ## LL)
#define UVAL64(a) (a ## uLL)
typedef uintptr_t uintptr;
typedef intptr_t intptr;

#ifndef HAVE_LOFF_T
typedef off_t loff_t;
#endif
#ifndef HAVE_CADDR_T
typedef char * caddr_t;
#endif

/* Time data type for Time Manager emulation */

#if defined(__MACH__)
typedef mach_timespec_t tm_time_t;
#elif defined(HAVE_CLOCK_GETTIME)
typedef struct timespec tm_time_t;
#else
typedef struct timeval tm_time_t;
#endif

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
extern uint64 GetTicks_usec(void);
extern void Delay_usec(uint32 usec);

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

#ifdef HAVE_PTHREADS
/* Centralized pthread attribute setup */
void Set_pthread_attr(pthread_attr_t *attr, int priority);
#endif

/* UAE CPU defines */

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
static inline uae_u32 do_byteswap_32(uae_u32 v) {__asm__ ("bswap %0" : "=r" (v) : "0" (v)); return v;}
#define HAVE_OPTIMIZED_BYTESWAP_16
#ifdef X86_PPRO_OPT
static inline uae_u32 do_byteswap_16(uae_u32 v) {__asm__ ("bswapl %0" : "=&r" (v) : "0" (v << 16) : "cc"); return v;}
#else
static inline uae_u32 do_byteswap_16(uae_u32 v) {__asm__ ("rolw $8,%0" : "=r" (v) : "0" (v) : "cc"); return v;}
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

#ifndef HAVE_OPTIMIZED_BYTESWAP_32
static inline uae_u32 do_byteswap_32(uae_u32 v)
	{ return (((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v & 0xff) << 24) | ((v & 0xff00) << 8)); }
#endif

#ifndef HAVE_OPTIMIZED_BYTESWAP_16
static inline uae_u32 do_byteswap_16(uae_u32 v)
	{ return (((v >> 8) & 0xff) | ((v & 0xff) << 8)); }
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


#if __GNUC__ < 3
# define __builtin_expect(foo,bar) (foo)
#endif
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)
#define ALWAYS_INLINE   inline __attribute__((always_inline))

#define memptr uint32


#endif /* NEED_CONFIG_H_ONLY */

#endif
