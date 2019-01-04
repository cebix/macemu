/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define if using video enabled on SEGV signals. */
#ifndef _DEBUG
#define ENABLE_VOSF 1
#endif

/* Define to 1 if you have the `acoshl' function. */
#define HAVE_ACOSHL 1

/* Define to 1 if you have the `acosl' function. */
#define HAVE_ACOSL 1

/* Define to 1 if you have the `asinhl' function. */
#define HAVE_ASINHL 1

/* Define to 1 if you have the `asinl' function. */
#define HAVE_ASINL 1

/* Define to 1 if you have the `atanh' function. */
#define HAVE_ATANH 1

/* Define to 1 if you have the `atanhl' function. */
#define HAVE_ATANHL 1

/* Define to 1 if you have the `atanl' function. */
#define HAVE_ATANL 1

/* Define to 1 if the system has the type `caddr_t'. */
/* #undef HAVE_CADDR_T */

/* Define to 1 if you have the `ceill' function. */
#define HAVE_CEILL 1

/* Define to 1 if you have the `coshl' function. */
#define HAVE_COSHL 1

/* Define to 1 if you have the `cosl' function. */
#define HAVE_COSL 1

/* Define to 1 if you have the `expl' function. */
/* #undef HAVE_EXPL */

/* Define to 1 if you have the `fabsl' function. */
/* #undef HAVE_FABSL */

/* Define to 1 if you have the `finite' function. */
#define HAVE_FINITE 1

/* Define to 1 if you have the <floatingpoint.h> header file. */
/* #undef HAVE_FLOATINGPOINT_H */

/* Define to 1 if you have the `floorl' function. */
#define HAVE_FLOORL 1

/* Define to 1 if you have the <ieee754.h> header file. */
/* #undef HAVE_IEEE754_H */

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef HAVE_IEEEFP_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isinf' function. */
/* #undef HAVE_ISINF */

/* Define to 1 if you have the `isinfl' function. */
/* #undef HAVE_ISINFL */

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 1

/* Define to 1 if you have the `isnanl' function. */
/* #undef HAVE_ISNANL */

/* Define to 1 if you have the `isnormal' function. */
/* #undef HAVE_ISNORMAL */

/* Define to 1 if the system has the type `loff_t'. */
/* #undef HAVE_LOFF_T */

/* Define to 1 if you have the `log10l' function. */
/* #undef HAVE_LOG10L */

/* Define to 1 if you have the `logl' function. */
/* #undef HAVE_LOGL */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <nan.h> header file. */
/* #undef HAVE_NAN_H */

/* Define to 1 if you have the `powl' function. */
#define HAVE_POWL 1

/* Define to 1 if you have the `signbit' function. */
/* #undef HAVE_SIGNBIT */

/* Define if we can ignore the fault (instruction skipping in SIGSEGV
   handler). */
#define HAVE_SIGSEGV_SKIP_INSTRUCTION 1

/* Define to 1 if you have the `sinhl' function. */
#define HAVE_SINHL 1

/* Define to 1 if you have the `sinl' function. */
#define HAVE_SINL 1

/* Define to 1 if you have the `sqrtl' function. */
#define HAVE_SQRTL 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `tanhl' function. */
#define HAVE_TANHL 1

/* Define to 1 if you have the `tanl' function. */
#define HAVE_TANL 1

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define if your system supports Windows exceptions. */
#define HAVE_WIN32_EXCEPTIONS 1

/* Define if your system has a working Win32-based memory allocator. */
#define HAVE_WIN32_VM 1

/* Define to the floating point format of the host machine. */
#define HOST_FLOAT_FORMAT IEEE_FLOAT_FORMAT

/* Define to 1 if the host machine stores floating point numbers in memory
   with the word containing the sign bit at the lowest address, or to 0 if it
   does it the other way around. This macro should not be defined if the
   ordering is the same as for multi-word integers. */
/* #undef HOST_FLOAT_WORDS_BIG_ENDIAN */

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

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 8

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to enble SDL support */
#define USE_SDL 1

/* Define to enable SDL audio support */
#define USE_SDL_AUDIO 1

/* Define to enable SDL video graphics support */
#define USE_SDL_VIDEO 1

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

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

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
