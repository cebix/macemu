/*
 *  bfd.h - Dummy bfd library header file
 */

#include "sysdeps.h"
#include "ansidecl.h"

enum bfd_flavour {
	bfd_target_unknown_flavour
};

enum bfd_endian {
	BFD_ENDIAN_BIG,
	BFD_ENDIAN_LITTLE,
	BFD_ENDIAN_UNKNOWN
};

enum bfd_architecture {
	bfd_arch_unknown,
	bfd_arch_m68k,
#define bfd_mach_m68000 1
#define bfd_mach_m68008 2
#define bfd_mach_m68010 3
#define bfd_mach_m68020 4
#define bfd_mach_m68030 5
#define bfd_mach_m68040 6
#define bfd_mach_m68060 7
	bfd_arch_i386
#define bfd_mach_i386_i386 0
#define bfd_mach_i386_i8086 1
#define bfd_mach_i386_i386_intel_syntax 2
#define bfd_mach_x86_64 3
#define bfd_mach_x86_64_intel_syntax 4
};

typedef struct symbol_cache_entry {
	CONST char *name;
} asymbol;

typedef uint64 bfd_vma;
typedef int64 bfd_signed_vma;
typedef unsigned char bfd_byte;

typedef struct _bfd bfd;
struct _bfd;

#if SIZEOF_LONG == 8
#define BFD_HOST_64BIT_LONG 1
#endif

// 64-bit vma
#define BFD64

#ifndef fprintf_vma
#if BFD_HOST_64BIT_LONG
#define sprintf_vma(s,x) sprintf (s, "%016lx", x)
#define fprintf_vma(f,x) fprintf (f, "%016lx", x)
#else
#define _bfd_int64_low(x) ((unsigned long) (((x) & 0xffffffff)))
#define _bfd_int64_high(x) ((unsigned long) (((x) >> 32) & 0xffffffff))
#define fprintf_vma(s,x) \
  fprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
#define sprintf_vma(s,x) \
  sprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
#endif
#endif
