dnl Process this file with autoconf to produce a configure script.
dnl Written in 2002 by Christian Bauer et al.

AC_INIT([Basilisk II], 1.0, [Christian.Bauer@uni-mainz.de], BasiliskII)
AC_CONFIG_SRCDIR(main_windows.cpp)
AC_CONFIG_AUX_DIR(../Unix)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(config.h)

dnl Aliases for PACKAGE and VERSION macros.
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE_NAME", [Define this program name.])
AC_DEFINE_UNQUOTED(VERSION, "$PACKAGE_VERSION", [Define this program version.])

dnl SDL options.
AC_ARG_ENABLE(sdl-static,    [  --enable-sdl-static     use SDL static libraries for linking [default=no]], [WANT_SDL_STATIC=$enableval], [WANT_SDL_STATIC=no])

dnl JIT compiler options.
AC_ARG_ENABLE(jit-compiler,  [  --enable-jit-compiler   enable JIT compiler [default=yes]], [WANT_JIT=$enableval], [WANT_JIT=yes])
AC_ARG_ENABLE(jit-debug,     [  --enable-jit-debug      activate native code disassemblers [default=no]], [WANT_JIT_DEBUG=$enableval], [WANT_JIT_DEBUG=no])

dnl FPU emulation core.
AC_ARG_ENABLE(fpe,
[  --enable-fpe=FPE        specify which fpu emulator to use [default=auto]],
[ case "$enableval" in
    dnl default is always ieee, if architecture has this fp format
    auto)	FPE_CORE_TEST_ORDER="ieee uae";;
    ieee)	FPE_CORE_TEST_ORDER="ieee";;
    uae)	FPE_CORE_TEST_ORDER="uae";;
    x86)	FPE_CORE_TEST_ORDER="x86";;
	*)		AC_MSG_ERROR([--enable-fpe takes only one of the following values: auto, x86, ieee, uae]);;
  esac
],
[ FPE_CORE_TEST_ORDER="ieee uae"
])

dnl External packages.
AC_ARG_WITH(gtk,             [  --with-gtk              use GTK user interface [default=yes]], [WANT_GTK=$withval], [WANT_GTK=yes])

dnl Addressing modes.
AC_ARG_ENABLE(addressing,
[  --enable-addressing=AM  specify the addressing mode to use [default=fastest]],
[ case "$enableval" in
    direct)	ADDRESSING_TEST_ORDER="direct";;
    banks)	ADDRESSING_TEST_ORDER="banks";;
    fastest)ADDRESSING_TEST_ORDER="direct banks";;
    *)		AC_MSG_ERROR([--enable-addressing takes only one of the following values: fastest, direct, banks]);;
  esac
],
[ ADDRESSING_TEST_ORDER="direct banks"
])

dnl Canonical system information.
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Target CPU type.
HAVE_I386=no
HAVE_POWERPC=no
HAVE_X86_64=no
case "$target_cpu" in
  i386* | i486* | i586* | i686* | i786* ) HAVE_I386=yes;;
  powerpc* ) HAVE_POWERPC=yes;;
  x86_64* ) HAVE_X86_64=yes;;
esac

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_EGREP
AC_PROG_LN_S
AC_CHECK_TOOL(WINDRES, windres)

dnl We use GTK+ if possible.
if [[ "x$WANT_GTK" = "xyes" ]]; then
  AM_PATH_GTK_2_0(1.3.15, [], [
    AC_MSG_WARN([Could not find GTK+ 2.0, disabling user interface.])
    WANT_GTK=no
  ])
fi
AC_SUBST(WANT_GTK)

dnl We use 64-bit file size support if possible.
AC_SYS_LARGEFILE

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(float, 4)
AC_CHECK_SIZEOF(double, 8)
AC_CHECK_SIZEOF(long double, 12)
AC_CHECK_SIZEOF(void *, 4)
AC_TYPE_OFF_T
AC_CHECK_TYPES(loff_t)
AC_CHECK_TYPES(caddr_t)
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror)

dnl Define a macro that translates a yesno-variable into a C macro definition
dnl to be put into the config.h file
dnl $1 -- the macro to define
dnl $2 -- the value to translate
dnl $3 -- template name
AC_DEFUN([AC_TRANSLATE_DEFINE], [
    if [[ "x$2" = "xyes" -o "x$2" = "xguessing yes" ]]; then
        AC_DEFINE($1, 1, $3)
    fi
])

dnl Check that VirtualAlloc(), VirtualProtect() work
AC_CACHE_CHECK([whether VirtualProtect works],
  ac_cv_VirtualProtect_works, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_cv_VirtualProtect_works=yes
  dnl First the tests that should segfault
  for test_def in NONE_READ NONE_WRITE READ_WRITE; do
    AC_TRY_RUN([
      #define HAVE_WIN32_VM 1
      #define CONFIGURE_TEST_VM_MAP
      #define TEST_VM_PROT_$test_def
      #include "../Unix/vm_alloc.cpp"
    ], ac_cv_VirtualProtect_works=no, rm -f core,
    dnl When cross-compiling, assume it works
    ac_cv_VirtualProtect_works="yes"
    )
  done
  AC_TRY_RUN([
    #define HAVE_WIN32_VM 1
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_PROT_RDWR_WRITE
    #include "../Unix/vm_alloc.cpp"
  ], , ac_cv_VirtualProtect_works=no,
  dnl When cross-compiling, assume it works
  ac_cv_VirtualProtect_works="yes"
  )
  AC_LANG_RESTORE
  ]
)
if [[ "x$ac_cv_VirtualProtect_works" = "xyes" ]]; then
  AC_DEFINE(HAVE_WIN32_VM, 1, [Define if your system has a working Win32-based memory allocator.])
else
  AC_MSG_ERROR([Sorry, Windows VM functions don't work as expected on your system.])
fi

dnl Check if Windows exceptions are supported.
AC_CACHE_CHECK([whether your system supports Windows exceptions],
  ac_cv_have_win32_exceptions, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_WIN32_EXCEPTIONS 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../Unix/vm_alloc.cpp"
    #include "../Unix/sigsegv.cpp"
  ],
  ac_cv_have_win32_exceptions=yes,
  ac_cv_have_win32_exceptions=no,
  dnl When cross-compiling, assume it works
  ac_cv_have_win32_exceptions="yes"
  )
  AC_LANG_RESTORE
  ]
)
if [[ "x$ac_cv_have_win32_exceptions" = "xyes" ]]; then
  AC_DEFINE(HAVE_WIN32_EXCEPTIONS, 1, [Define if your system supports Windows exceptions.])
else
  AC_MSG_ERROR([Sorry, Windows exceptions don't work as expected on your system.])
fi

dnl Check if we can ignore the fault (instruction skipping in SIGSEGV handler)
AC_CACHE_CHECK([whether we can skip instruction in SIGSEGV handler],
  ac_cv_have_skip_instruction, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_SIGSEGV_SKIP_INSTRUCTION 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../Unix/vm_alloc.cpp"
    #include "../Unix/sigsegv.cpp"
  ], ac_cv_have_skip_instruction=yes, ac_cv_have_skip_instruction=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_skip_instruction=no
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_SIGSEGV_SKIP_INSTRUCTION, "$ac_cv_have_skip_instruction",
  [Define if we can ignore the fault (instruction skipping in SIGSEGV handler).])

dnl We really want VOSF (Video on SEGV Signals) screen updates acceleration
AC_DEFINE(ENABLE_VOSF, 1, [Define if using video enabled on SEGV signals.])

dnl Determine the addressing mode to use
ADDRESSING_MODE=""
AC_MSG_CHECKING([for the addressing mode to use])
for am in $ADDRESSING_TEST_ORDER; do
  case $am in
  direct)
    dnl Direct addressing mode (constant offset)
    ADDRESSING_MODE="direct"
    DEFINES="$DEFINES -DDIRECT_ADDRESSING"
    break
    ;;
  banks)
    dnl Default addressing mode
    ADDRESSING_MODE="memory banks"
    break
    ;;
  *)
    AC_MSG_ERROR([Internal configure.ac script error for $am addressing mode])
  esac
done
AC_MSG_RESULT($ADDRESSING_MODE)
if [[ "x$ADDRESSING_MODE" = "x" ]]; then
  AC_MSG_WARN([Sorry, no suitable addressing mode in $ADDRESSING_TEST_ORDER])
  ADDRESSING_MODE="memory banks"
fi

dnl Banked Memory Addressing mode is not supported by the JIT compiler
if [[ "x$WANT_JIT" = "xyes" -a "x$ADDRESSING_MODE" = "xmemory banks" ]]; then
  AC_MSG_ERROR([Sorry, the JIT Compiler requires Direct Addressing, at least])
fi

dnl Check for GAS.
HAVE_GAS=no
AC_MSG_CHECKING(for GAS .p2align feature)
cat >conftest.S << EOF
	.text
	.p2align 5
EOF
if $CC conftest.S -c -o conftest.o >/dev/null 2>&1 ; then HAVE_GAS=yes; fi
AC_MSG_RESULT($HAVE_GAS)

dnl Check for GCC 2.7 or higher.
HAVE_GCC27=no
AC_MSG_CHECKING(for GCC 2.7 or higher)
AC_EGREP_CPP(xyes,
[#if __GNUC__ - 1 > 1 || __GNUC_MINOR__ - 1 > 5
  xyes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC27=yes], AC_MSG_RESULT(no))

dnl Check for GCC 3.0 or higher.
HAVE_GCC30=no
AC_MSG_CHECKING(for GCC 3.0 or higher)
AC_EGREP_CPP(xyes,
[#if __GNUC__ >= 3
  xyes
#endif
], [AC_MSG_RESULT(yes); HAVE_GCC30=yes], AC_MSG_RESULT(no))

dnl Add -fno-strict-aliasing for slirp sources
if [[ "x$HAVE_GCC30" = "xyes" ]]; then
  SAVED_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -fno-strict-aliasing"
  AC_CACHE_CHECK([whether the compiler supports -fno-strict-aliasing],
    ac_cv_gcc_no_strict_aliasing, [
    AC_TRY_COMPILE([],[],
      [ac_cv_gcc_no_strict_aliasing=yes; AC_SUBST(SLIRP_CFLAGS, "-fno-strict-aliasing")],
      [ac_cv_gcc_no_strict_aliasing=no])
  ])
  CFLAGS="$SAVED_CFLAGS"
fi

dnl Select appropriate CPU source and REGPARAM define.
ASM_OPTIMIZATIONS=none
CPUSRCS="cpuemu1.cpp cpuemu2.cpp cpuemu3.cpp cpuemu4.cpp cpuemu5.cpp cpuemu6.cpp cpuemu7.cpp cpuemu8.cpp"

dnl JITSRCS will be emptied later if the JIT is not available
dnl Other platforms should define their own set of noflags file variants
CAN_JIT=no
JITSRCS="compemu1.cpp compemu2.cpp compemu3.cpp compemu4.cpp compemu5.cpp compemu6.cpp compemu7.cpp compemu8.cpp"

if [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_I386" = "xyes" ]]; then
  dnl i386 CPU
  DEFINES="$DEFINES -DUNALIGNED_PROFITABLE -DREGPARAM=\"__attribute__((regparm(3)))\""
  if [[ "x$HAVE_GAS" = "xyes" ]]; then
    ASM_OPTIMIZATIONS=i386
    DEFINES="$DEFINES -DX86_ASSEMBLY -DOPTIMIZED_FLAGS -DSAHF_SETO_PROFITABLE"
    JITSRCS="cpuemu1_nf.cpp cpuemu2_nf.cpp cpuemu3_nf.cpp cpuemu4_nf.cpp cpuemu5_nf.cpp cpuemu6_nf.cpp cpuemu7_nf.cpp cpuemu8_nf.cpp $JITSRCS"
    CAN_JIT=yes
  fi
elif [[ "x$HAVE_GCC30" = "xyes" -a "x$HAVE_X86_64" = "xyes" ]]; then
  dnl x86-64 CPU
  DEFINES="$DEFINES -DUNALIGNED_PROFITABLE"
  if [[ "x$HAVE_GAS" = "xyes" ]]; then
    ASM_OPTIMIZATIONS="x86-64"
    DEFINES="$DEFINES -DX86_64_ASSEMBLY -DOPTIMIZED_FLAGS"
    JITSRCS="cpuemu1_nf.cpp cpuemu2_nf.cpp cpuemu3_nf.cpp cpuemu4_nf.cpp cpuemu5_nf.cpp cpuemu6_nf.cpp cpuemu7_nf.cpp cpuemu8_nf.cpp $JITSRCS"
    CAN_JIT=yes
  fi
fi

dnl Enable JIT compiler, if possible.
if [[ "x$WANT_JIT" = "xyes" -a "x$CAN_JIT" ]]; then
  JITSRCS="$JITSRCS ../uae_cpu/compiler/compemu_support.cpp ../uae_cpu/compiler/compemu_fpp.cpp compstbl.o cpustbl_nf.o"
  DEFINES="$DEFINES -DUSE_JIT -DUSE_JIT_FPU"
  
  if [[ "x$WANT_JIT_DEBUG" = "xyes" ]]; then
    if [[ "x$WANT_MON" = "xyes" ]]; then
      DEFINES="$DEFINES -DJIT_DEBUG=1"
    else
      AC_MSG_WARN([cxmon not found, ignoring --enable-jit-debug])
      WANT_JIT_DEBUG=no
    fi
  fi

  dnl IEEE core is the only FPU emulator to use with the JIT compiler
  case $FPE_CORE_TEST_ORDER in
  ieee*) ;;
  *) AC_MSG_WARN([Forcing use of the IEEE FPU core, as the JIT compiler supports only this one.]) ;;
  esac
  FPE_CORE_TEST_ORDER="ieee"
else
  WANT_JIT=no
  WANT_JIT_DEBUG=no
  JITSRCS=""
fi

dnl Utility macro used by next two tests.
dnl AC_EXAMINE_OBJECT(C source code,
dnl	commands examining object file,
dnl	[commands to run if compile failed]):
dnl
dnl Compile the source code to an object file; then convert it into a
dnl printable representation.  All unprintable characters and
dnl asterisks (*) are replaced by dots (.).  All white space is
dnl deleted.  Newlines (ASCII 0x10) in the input are preserved in the
dnl output, but runs of newlines are compressed to a single newline.
dnl Finally, line breaks are forcibly inserted so that no line is
dnl longer than 80 columns and the file ends with a newline.  The
dnl result of all this processing is in the file conftest.dmp, which
dnl may be examined by the commands in the second argument.
dnl
AC_DEFUN([gcc_AC_EXAMINE_OBJECT],
[AC_LANG_SAVE
AC_LANG_C
dnl Next bit cribbed from AC_TRY_COMPILE.
cat > conftest.$ac_ext <<EOF
[#line __oline__ "configure"
#include "confdefs.h"
$1
]EOF
if AC_TRY_EVAL(ac_compile); then
  od -c conftest.o |
    sed ['s/^[0-7]*[ 	]*/ /
	  s/\*/./g
	  s/ \\n/*/g
	  s/ [0-9][0-9][0-9]/./g
	  s/  \\[^ ]/./g'] |
    tr -d '
 ' | tr -s '*' '
' | fold | sed '$a\
' > conftest.dmp
  $2
ifelse($3, , , else
  $3
)dnl
fi
rm -rf conftest*
AC_LANG_RESTORE])

dnl Floating point format probe.
dnl The basic concept is the same as the above: grep the object
dnl file for an interesting string.  We have to watch out for
dnl rounding changing the values in the object, however; this is
dnl handled by ignoring the least significant byte of the float.
dnl
dnl Does not know about VAX G-float or C4x idiosyncratic format.
dnl It does know about PDP-10 idiosyncratic format, but this is
dnl not presently supported by GCC.  S/390 "binary floating point"
dnl is in fact IEEE (but maybe we should have that in EBCDIC as well
dnl as ASCII?)
dnl
AC_DEFUN([gcc_AC_C_FLOAT_FORMAT],
[AC_CACHE_CHECK(floating point format, ac_cv_c_float_format,
[gcc_AC_EXAMINE_OBJECT(
[/* This will not work unless sizeof(double) == 8.  */
extern char sizeof_double_must_be_8 [sizeof(double) == 8 ? 1 : -1];

/* This structure must have no internal padding.  */
struct possibility {
  char prefix[8];
  double candidate;
  char postfix[8];
};

#define C(cand) { "\nformat:", cand, ":tamrof\n" }
struct possibility table [] =
{
  C( 3.25724264705901305206e+01), /* @@IEEEFP - IEEE 754 */
  C( 3.53802595280598432000e+18), /* D__float - VAX */
  C( 5.32201830133125317057e-19), /* D.PDP-10 - PDP-10 - the dot is 0x13a */
  C( 1.77977764695171661377e+10), /* IBMHEXFP - s/390 format, ascii */
  C(-5.22995989424860458374e+10)  /* IBMHEXFP - s/390 format, EBCDIC */
};],
 [if   grep 'format:.@IEEEF.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='IEEE (big-endian)'
  elif grep 'format:.I@@PFE.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='IEEE (big-endian)'
  elif grep 'format:.FEEEI@.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='IEEE (little-endian)'
  elif grep 'format:.EFP@@I.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='IEEE (little-endian)'
  elif grep 'format:.__floa.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='VAX D-float'
  elif grep 'format:..PDP-1.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='PDP-10'
  elif grep 'format:.BMHEXF.:tamrof' conftest.dmp >/dev/null 2>&1; then
    ac_cv_c_float_format='IBM 370 hex'
  else
    AC_MSG_ERROR(Unknown floating point format)
  fi],
  [AC_MSG_ERROR(compile failed)])
])
# IEEE is the default format.  If the float endianness isn't the same
# as the integer endianness, we have to set FLOAT_WORDS_BIG_ENDIAN
# (which is a tristate: yes, no, default).  This is only an issue with
# IEEE; the other formats are only supported by a few machines each,
# all with the same endianness.
format=IEEE_FLOAT_FORMAT
fbigend=
case $ac_cv_c_float_format in
    'IEEE (big-endian)' )
	if test $ac_cv_c_bigendian = no; then
	    fbigend=1
	fi
	;;
    'IEEE (little-endian)' )
	if test $ac_cv_c_bigendian = yes; then
	    fbigend=0
	fi
	;;
    'VAX D-float' )
	format=VAX_FLOAT_FORMAT
	;;
    'PDP-10' )
	format=PDP10_FLOAT_FORMAT
	;;
    'IBM 370 hex' )
	format=IBM_FLOAT_FORMAT
	;;
esac
AC_DEFINE_UNQUOTED(HOST_FLOAT_FORMAT, $format,
  [Define to the floating point format of the host machine.])
if test -n "$fbigend"; then
	AC_DEFINE_UNQUOTED(HOST_FLOAT_WORDS_BIG_ENDIAN, $fbigend,
  [Define to 1 if the host machine stores floating point numbers in
   memory with the word containing the sign bit at the lowest address,
   or to 0 if it does it the other way around.

   This macro should not be defined if the ordering is the same as for
   multi-word integers.])
fi
])

dnl Select appropriate FPU source.
gcc_AC_C_FLOAT_FORMAT
AC_CHECK_HEADERS(ieee754.h ieeefp.h floatingpoint.h nan.h)

for fpe in $FPE_CORE_TEST_ORDER; do
  case $fpe in
  ieee)
    case $ac_cv_c_float_format in
    IEEE*)
      FPE_CORE="IEEE fpu core"
      DEFINES="$DEFINES -DFPU_IEEE"
      FPUSRCS="../uae_cpu/fpu/fpu_ieee.cpp"
      dnl Math functions not mandated by C99 standard
      AC_CHECK_FUNCS(isnanl isinfl)
      dnl Math functions required by C99 standard, but probably not
      dnl implemented everywhere. In that case, we fall back to the
      dnl regular variant for doubles.
      AC_CHECK_FUNCS(logl log10l expl powl fabsl sqrtl)
      AC_CHECK_FUNCS(sinl cosl tanl sinhl coshl tanhl)
      AC_CHECK_FUNCS(asinl acosl atanl asinhl acoshl atanhl)
      AC_CHECK_FUNCS(floorl ceill)
      break
      ;;
    esac
    ;;
  x86)
    if [[ ":$HAVE_GCC27:$HAVE_I386:$HAVE_GAS:" = ":yes:yes:yes:" ]]; then
      FPE_CORE="i387 fpu core"
      DEFINES="$DEFINES -DFPU_X86"
      FPUSRCS="../uae_cpu/fpu/fpu_x86.cpp"
      break
    fi
    ;;
  uae)
    FPE_CORE="uae fpu core"
    DEFINES="$DEFINES -DFPU_UAE"
    FPUSRCS="../uae_cpu/fpu/fpu_uae.cpp"
    break
    ;;
  *)
    AC_MSG_ERROR([Internal configure.in script error for $fpe fpu core])
    ;;
  esac
done
if [[ "x$FPE_CORE" = "x" ]]; then
  AC_MSG_ERROR([Sorry, no suitable FPU core found in $FPE_CORE_TEST_ORDER])
fi

dnl Check for certain math functions
AC_CHECK_FUNCS(atanh)
AC_CHECK_FUNCS(isnan isinf finite isnormal signbit)

dnl UAE CPU sources for all non-m68k-native architectures.
CPUINCLUDES="-I../uae_cpu"
CPUSRCS="../uae_cpu/basilisk_glue.cpp ../uae_cpu/memory.cpp ../uae_cpu/newcpu.cpp ../uae_cpu/readcpu.cpp $FPUSRCS cpustbl.cpp cpudefs.cpp $CPUSRCS $JITSRCS"

dnl We really want SDL for now
AC_CHECK_TOOL(sdl_config, sdl-config, [AC_MSG_ERROR([Sorry, you currently need SDL for this port])])
SDL_CFLAGS=`$sdl_config --cflags`
AC_SUBST(SDL_CFLAGS)
if [[ "x$WANT_SDL_STATIC" = "xyes" ]]; then
  SDL_LIBS=`$sdl_config --static-libs`
  sdl_prefix=`$sdl_config --exec-prefix`
  if [[ -n "$sdl_prefix" ]]; then
    SDL_LIBS=`echo "$SDL_LIBS" | sed -e "s,-l\(SDLmain\|SDL\),$sdl_prefix/lib/lib\1.a,g"`
  fi
  SDL_LIBS="$SDL_LIBS -lwinmm"
else
  SDL_LIBS=`$sdl_config --libs`
fi
AC_SUBST(SDL_LIBS)
AC_DEFINE(USE_SDL, 1, [Define to enble SDL support])
AC_DEFINE(USE_SDL_VIDEO, 1, [Define to enable SDL video graphics support])
AC_DEFINE(USE_SDL_AUDIO, 1, [Define to enable SDL audio support])

dnl Remove the "-g" option if set for GCC.
if [[ "x$HAVE_GCC27" = "xyes" ]]; then
  CFLAGS=`echo $CFLAGS | sed -e 's/-g\b//g'`
  CXXFLAGS=`echo $CXXFLAGS | sed -e 's/-g\b//g'`
fi

dnl Generate Makefile.
AC_SUBST(DEFINES)
AC_SUBST(CPUINCLUDES)
AC_SUBST(CPUSRCS)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl Print summary.
echo
echo Basilisk II configuration summary:
echo
echo Use JIT compiler ....................... : $WANT_JIT
echo JIT debug mode ......................... : $WANT_JIT_DEBUG
echo Floating-Point emulation core .......... : $FPE_CORE
echo Assembly optimizations ................. : $ASM_OPTIMIZATIONS
echo Addressing mode ........................ : $ADDRESSING_MODE
echo GTK user interface ..................... : $WANT_GTK
echo
echo "Configuration done. Now type \"make\" (or \"gmake\")."
