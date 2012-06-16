dnl Process this file with autoconf to produce a configure script.
dnl Written in 2002 by Christian Bauer

AC_INIT([SheepShaver], 2.3, [Christian.Bauer@uni-mainz.de], SheepShaver)
AC_CONFIG_SRCDIR(main_windows.cpp)
AC_CONFIG_AUX_DIR(../Unix)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(config.h)

dnl Canonical system information.
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Options.
AC_ARG_ENABLE(jit,          [  --enable-jit            enable JIT compiler [default=yes]], [WANT_JIT=$enableval], [WANT_JIT=yes])
AC_ARG_WITH(gtk,          [  --with-gtk              use GTK user interface [default=yes]], [WANT_GTK=$withval], [WANT_GTK=yes])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_EGREP
AC_PROG_LN_S
AC_PATH_PROG(PERL, [perl])
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
AC_CHECK_SIZEOF(void *, 4)
AC_TYPE_OFF_T
AC_CHECK_TYPES(loff_t)
AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror)
AC_CHECK_FUNCS(exp2f log2f exp2 log2)
AC_CHECK_FUNCS(floorf roundf ceilf truncf)
AC_CHECK_FUNCS(floor round ceil trunc)

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
  dnl When cross-compiling, assume it works
  ac_cv_have_skip_instruction="yes"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_SIGSEGV_SKIP_INSTRUCTION, "$ac_cv_have_skip_instruction",
  [Define if we can ignore the fault (instruction skipping in SIGSEGV handler).])

dnl We really want VOSF (Video on SEGV Signals) screen updates acceleration
AC_DEFINE(ENABLE_VOSF, 1, [Define if using video enabled on SEGV signals.])

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

dnl CPU emulator sources
CPUSRCS="\
    ../kpx_cpu/src/mathlib/ieeefp.cpp \
    ../kpx_cpu/src/mathlib/mathlib.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-cpu.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-decode.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-execute.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-translate.cpp \
    ../kpx_cpu/src/utils/utils-cpuinfo.cpp"
CPPFLAGS="$CPPFLAGS -I../kpx_cpu/include -I../kpx_cpu/src"

dnl Enable JIT compiler, if possible
USE_DYNGEN="no"
if [[ "x$WANT_JIT" = "xyes" ]]; then
  case $host_cpu in
  i?86)
    DYNGEN_OP_FLAGS="-fomit-frame-pointer -mpreferred-stack-boundary=2"
    if [[ "x$HAVE_GCC30" = "xyes" ]]; then
      DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -falign-functions=0"
    else
      DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -malign-functions=0"
    fi
    ;;
  esac
  USE_DYNGEN="yes"
  DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -finline-limit=10000 -g0"
  if [[ "x$HAVE_GCC30" = "xyes" ]]; then
    DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -fno-reorder-blocks -fno-optimize-sibling-calls"
  fi
  AC_DEFINE(ENABLE_DYNGEN, 1, [Define to enable dyngen engine])
  DYNGENSRCS="\
    ../kpx_cpu/src/cpu/jit/dyngen.c \
    ../kpx_cpu/src/cpu/jit/cxxdemangle.cpp"
  CPUSRCS="\
    ../kpx_cpu/src/cpu/jit/jit-cache.cpp \
    ../kpx_cpu/src/cpu/jit/basic-dyngen.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-dyngen.cpp \
    ../kpx_cpu/src/cpu/ppc/ppc-jit.cpp $CPUSRCS"
  CPPFLAGS="$CPPFLAGS -DUSE_JIT"
fi
CPUSRCS="$CPUSRCS ../kpx_cpu/sheepshaver_glue.cpp ../kpx_cpu/ppc-dis.c"

dnl Use the dummy prefs file.
CPUSRCS="$CPUSRCS ../dummy/prefs_dummy.cpp"

dnl We really want SDL for now
AC_CHECK_TOOL(sdl_config, sdl-config, [AC_MSG_ERROR([Sorry, you currently need SDL for this port])])
SDL_CFLAGS=`$sdl_config --cflags`
AC_SUBST(SDL_CFLAGS)
SDL_LIBS=`$sdl_config --libs`
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
AC_SUBST(PERL)
AC_SUBST(USE_DYNGEN)
AC_SUBST(DYNGENSRCS)
AC_SUBST(DYNGEN_OP_FLAGS)
AC_SUBST(CPUSRCS)
AC_OUTPUT([Makefile])

dnl Print summary.
echo
echo SheepShaver configuration summary:
echo
echo Enable JIT compiler .............. : $WANT_JIT
echo GTK user interface ............... : $WANT_GTK
echo
echo "Configuration done. Now type \"make\"."
