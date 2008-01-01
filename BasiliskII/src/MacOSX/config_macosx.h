/*
 *	config_macosx.h - MacOS X macros determined at compile-time
 *
 *	$Id$
 *
 *	Basilisk II (C) 1997-2008 Christian Bauer
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Supported platforms are: ppc, ppc64, x86, x86_64 */
#if ! defined __ppc__ && ! defined __ppc64__ && ! defined __i386__ && ! defined __x86_64__
# error "Unsupported architecture. Please fix arch-specific macros"
#endif

/* Size of data types */
#define SIZEOF_FLOAT 4
#define SIZEOF_DOUBLE 8
#if defined __ppc__ || defined __ppc64__
# if defined __LONG_DOUBLE_128__
#  define SIZEOF_LONG_DOUBLE 16
# else
#  define SIZEOF_LONG_DOUBLE 8
# endif
#else
# define SIZEOF_LONG_DOUBLE 16
#endif
#define SIZEOF_SHORT 2
#define SIZEOF_INT 4
#if defined __ppc64__ || defined __x86_64__
# define SIZEOF_LONG 8
#else
# define SIZEOF_LONG 4
#endif
#define SIZEOF_LONG_LONG 8
#define SIZEOF_VOID_P SIZEOF_LONG /* ILP32 or LP64 */

/* Endian-ness of data types */
#if ! defined __LITTLE_ENDIAN__
# define WORDS_BIGENDIAN 1
#endif

/* Define to the floating point format of the host machine. */
#define HOST_FLOAT_FORMAT IEEE_FLOAT_FORMAT

/* Define to 1 if the host machine stores floating point numbers in memory
   with the word containing the sign bit at the lowest address, or to 0 if it
   does it the other way around. This macro should not be defined if the
   ordering is the same as for multi-word integers. */
/* #undef HOST_FLOAT_WORDS_BIG_ENDIAN */

/* Define if your system supports Mach exceptions. */
#define HAVE_MACH_EXCEPTIONS 1

/* Define to 1 if you have the <mach/mach.h> header file. */
#define HAVE_MACH_MACH_H 1

/* Define to 1 if you have the `mach_task_self' function. */
#define HAVE_MACH_TASK_SELF 1

/* Define if your system has a working vm_allocate()-based memory allocator. */
#define HAVE_MACH_VM 1

/* Define if the __PAGEZERO Mach-O Low Memory Globals hack works. */
#define PAGEZERO_HACK 1
