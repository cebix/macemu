dnl Process this file with autoconf to produce a configure script.
dnl Written in 2002 by Christian Bauer et al.

AC_INIT([Basilisk II], 1.0, [Christian.Bauer@uni-mainz.de], BasiliskII)
AC_CONFIG_SRCDIR(main_unix.cpp)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(config.h)

AC_USE_SYSTEM_EXTENSIONS

dnl Aliases for PACKAGE and VERSION macros.
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE_NAME", [Define this program name.])
AC_DEFINE_UNQUOTED(VERSION, "$PACKAGE_VERSION", [Define this program version.])

dnl Some systems do not put corefiles in the currect directory, avoid saving
dnl cores for the configure tests since some are intended to dump core.
ulimit -c 0

dnl Video options.
AC_ARG_ENABLE(vosf,          [  --enable-vosf           enable video on SEGV signals [default=yes]], [WANT_VOSF=$enableval], [WANT_VOSF=yes])

dnl SDL options.
AC_ARG_ENABLE(sdl-static,    [  --enable-sdl-static     use SDL static libraries for linking [default=no]], [WANT_SDL_STATIC=$enableval], [WANT_SDL_STATIC=no])
AC_ARG_ENABLE(sdl-video,     [  --enable-sdl-video      use SDL for video graphics [default=no]], [WANT_SDL_VIDEO=$enableval], [WANT_SDL_VIDEO=no])
AC_ARG_ENABLE(sdl-audio,     [  --enable-sdl-audio      use SDL for audio [default=no]], [WANT_SDL_AUDIO=$enableval], [WANT_SDL_AUDIO=no])

dnl FPU emulation core.
AC_ARG_ENABLE(fpe,
[  --enable-fpe=FPE        specify which fpu emulator to use [default=auto]],
[ case "$enableval" in
    dnl default is always ieee, if architecture has this fp format
    uae)	FPE_CORE_TEST_ORDER="uae";;
	*)		AC_MSG_ERROR([--enable-fpe takes only one of the following values: auto, x86, ieee, uae]);;
  esac
],
[ FPE_CORE_TEST_ORDER="ieee uae"
])

dnl Addressing modes.
AC_ARG_ENABLE(addressing,
[  --enable-addressing=AM  specify the addressing mode to use [default=fastest]],
[ case "$enableval" in
    real) 	ADDRESSING_TEST_ORDER="real";;
    direct)	ADDRESSING_TEST_ORDER="direct";;
    banks)	ADDRESSING_TEST_ORDER="banks";;
    fastest)ADDRESSING_TEST_ORDER="direct banks";;
    *)		AC_MSG_ERROR([--enable-addressing takes only one of the following values: fastest, real, direct, banks]);;
  esac
],
[ ADDRESSING_TEST_ORDER="direct banks"
])

dnl Canonical system information.
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Target OS type (target is host if not cross-compiling).
case "$target_os" in
  darwin*)	OS_TYPE=darwin;;
  *)		OS_TYPE=`echo $target_os | sed -e 's/-/_/g' | sed -e 's/\./_/g'`;;
esac
DEFINES="$DEFINES -DOS_$OS_TYPE"

dnl Target CPU type.
HAVE_I386=no
HAVE_X86_64=no
case "$target_cpu" in
  i386* | i486* | i586* | i686* | i786* ) HAVE_I386=yes;;
  x86_64* | amd64* ) HAVE_X86_64=yes;;
esac

dnl Check if we should really be assuming x86_64 even if we detected HAVE_I386 above.
if [[ "x$HAVE_I386" = "xyes" ]]; then
  AC_TRY_RUN([
    int main(void) {
        #if defined(__x86_64__)
                return 0;
        #else
                return 1;
        #endif
    }
  ], [
    HAVE_I386=no
    HAVE_X86_64=yes
  ])
fi

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_EGREP

dnl Checks for libraries.
AC_CHECK_LIB(posix4, sem_init)
AC_CHECK_LIB(rt, timer_create)
AC_CHECK_LIB(rt, shm_open)
AC_CHECK_LIB(m, cos)


dnl Do we need SDL?
WANT_SDL=no
if [[ "x$WANT_SDL_VIDEO" = "xyes" ]]; then
  WANT_SDL=yes
  SDL_SUPPORT="$SDL_SUPPORT video"
fi
if [[ "x$WANT_SDL_AUDIO" = "xyes" ]]; then
  WANT_SDL=yes
  SDL_SUPPORT="$SDL_SUPPORT audio"
fi
if [[ "x$WANT_SDL" = "xyes" ]]; then

  AC_PATH_PROG(sdl_config, "sdl-config")
  if [[ -n "$sdl_config" ]]; then
    case $target_os in
    *)
      sdl_cflags=`$sdl_config --cflags`
      if [[ "x$WANT_SDL_STATIC" = "xyes" ]]; then
        sdl_libs=`$sdl_config --static-libs`
      else
        sdl_libs=`$sdl_config --libs`
      fi
      ;;
    esac
    CFLAGS="$CFLAGS $sdl_cflags"
    CXXFLAGS="$CXXFLAGS $sdl_cflags"
    LIBS="$LIBS $sdl_libs"
  else
    WANT_SDL=no
  fi

  SDL_SUPPORT=`echo "$SDL_SUPPORT" | sed -e "s/^ //"`
else
  SDL_SUPPORT="none"
fi

dnl We want pthreads. Try libpthread first, then PTL.
HAVE_PTHREADS=yes
AC_CHECK_LIB(pthread, pthread_create, , [
  AC_CHECK_LIB(PTL, pthread_create, , [
    HAVE_PTHREADS=no
  ])
])
if [[ "x$HAVE_PTHREADS" = "xyes" ]]; then
  AC_DEFINE(HAVE_PTHREADS, 1, [Define if pthreads are available.])
fi
AC_CHECK_FUNCS(pthread_cond_init)
AC_CHECK_FUNCS(pthread_cancel pthread_testcancel)
AC_CHECK_FUNCS(pthread_mutexattr_setprotocol)
AC_CHECK_FUNCS(pthread_mutexattr_settype)
AC_CHECK_FUNCS(pthread_mutexattr_setpshared)

dnl If POSIX.4 semaphores are not available, we emulate them with pthread mutexes.
SEMSRC=
AC_CHECK_FUNCS(sem_init, , [
  if test "x$HAVE_PTHREADS" = "xyes"; then
    SEMSRC=posix_sem.cpp
  fi
])

UISRCS=../dummy/prefs_editor_dummy.cpp

dnl We use 64-bit file size support if possible.
AC_SYS_LARGEFILE

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stdlib.h stdint.h)
AC_CHECK_HEADERS(unistd.h fcntl.h sys/types.h sys/time.h sys/mman.h mach/mach.h)
AC_CHECK_HEADERS(readline.h history.h readline/readline.h readline/history.h)
AC_CHECK_HEADERS(sys/socket.h sys/ioctl.h sys/filio.h sys/bitypes.h sys/wait.h)
AC_CHECK_HEADERS(sys/poll.h sys/select.h)
AC_CHECK_HEADERS(arpa/inet.h)
AC_CHECK_HEADERS(AvailabilityMacros.h)
AC_CHECK_HEADERS(IOKit/storage/IOBlockStorageDevice.h)
AC_CHECK_HEADERS(sys/stropts.h stropts.h)

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
AC_TYPE_SIGNAL
AC_HEADER_TIME
AC_STRUCT_TM

dnl Check whether sys/socket.h defines type socklen_t.
dnl (extracted from ac-archive/Miscellaneous)
AC_CACHE_CHECK([for socklen_t],
  ac_cv_type_socklen_t, [
  AC_TRY_COMPILE([
    #include <sys/types.h>
    #include <sys/socket.h>
  ], [socklen_t len = 42; return 0;],
  ac_cv_type_socklen_t=yes, ac_cv_type_socklen_t=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_type_socklen_t="guessing no"
  )
])
if [[ "x$ac_cv_type_socklen_t" != "xyes" ]]; then
  AC_DEFINE(socklen_t, int, [Define to 'int' if <sys/types.h> doesn't define.])
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror cfmakeraw)
AC_CHECK_FUNCS(clock_gettime timer_create)
AC_CHECK_FUNCS(sigaction signal)
AC_CHECK_FUNCS(mmap mprotect munmap)
AC_CHECK_FUNCS(vm_allocate vm_deallocate vm_protect)
AC_CHECK_FUNCS(poll inet_aton)

dnl Darwin seems to define mach_task_self() instead of task_self().
AC_CHECK_FUNCS(mach_task_self task_self)

dnl Check for systems where POSIX-style non-blocking I/O (O_NONBLOCK)
dnl doesn't work or is unimplemented. On these systems (mostly older
dnl ones), use the old BSD-style FIONBIO approach instead. [tcl.m4]
AC_CACHE_CHECK([FIONBIO vs. O_NONBLOCK for non-blocking I/O],
  ac_cv_nonblocking_io, [
  case "$host" in
  *-*-osf*)
    ac_cv_nonblocking_io=FIONBIO
    ;;
  *-*-sunos4*)
    ac_cv_nonblocking_io=FIONBIO
    ;;
  *-*-ultrix*)
    ac_cv_nonblocking_io=FIONBIO
    ;;
  *)
    ac_cv_nonblocking_io=O_NONBLOCK
    ;;
  esac
])
if [[ "$ac_cv_nonblocking_io" = "FIONBIO" ]]; then
  AC_DEFINE(USE_FIONBIO, 1, [Define if BSD-style non-blocking I/O is to be used])
fi

dnl Check whether compiler supports byte bit-fields
AC_CACHE_CHECK([whether compiler supports byte bit-fields],
  ac_cv_have_byte_bitfields, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    struct A {
      unsigned char b1:4;
      unsigned char b2:4;
      unsigned char c;
      unsigned short s;
      unsigned char a[4];
    };

    int main(void) {
      A a;
      return ! (sizeof(A) == 8 && &a.c == ((unsigned char *)&a + 1));
    }],
    [ac_cv_have_byte_bitfields=yes],
    [ac_cv_have_byte_bitfields=no],
    dnl When cross-compiling, assume only GCC supports this
    [if [[ "$GCC" = "yes" ]]; then
      ac_cv_have_byte_bitfields="guessing yes"
    else
      ac_cv_have_byte_bitfields="guessing no"
    fi]
    )
  AC_LANG_RESTORE
])

dnl AC_CHECK_FRAMEWORK($1=NAME, $2=INCLUDES)
AC_DEFUN([AC_CHECK_FRAMEWORK], [
  AS_VAR_PUSHDEF([ac_Framework], [ac_cv_framework_$1])dnl
  AC_CACHE_CHECK([whether compiler supports framework $1],
    ac_Framework, [
    saved_LIBS="$LIBS"
    LIBS="$LIBS -framework $1"
    AC_TRY_LINK(
      [$2], [],
      [AS_VAR_SET(ac_Framework, yes)], [AS_VAR_SET(ac_Framework, no); LIBS="$saved_LIBS"]
    )
  ])
  AS_IF([test AS_VAR_GET(ac_Framework) = yes],
    [AC_DEFINE(AS_TR_CPP(HAVE_FRAMEWORK_$1), 1, [Define if framework $1 is available.])]
  )
  AS_VAR_POPDEF([ac_Framework])dnl
])

dnl Check for some MacOS X frameworks
AC_CHECK_FRAMEWORK(AppKit, [])
AC_CHECK_FRAMEWORK(Carbon, [#include <Carbon/Carbon.h>])
AC_CHECK_FRAMEWORK(IOKit, [#include <IOKit/IOKitLib.h>])
AC_CHECK_FRAMEWORK(CoreFoundation, [#include <CoreFoundation/CoreFoundation.h>])

dnl Select system-dependant source files.
SERIALSRC=../dummy/serial_dummy.cpp
ETHERSRC=../dummy/ether_dummy.cpp
SCSISRC=../dummy/scsi_dummy.cpp
AUDIOSRC=../dummy/audio_dummy.cpp
EXTFSSRC=extfs_unix.cpp
EXTRASYSSRCS=
case "$target_os" in
darwin*)
  if [[ "x$ac_cv_framework_IOKit" = "xyes" -a "x$ac_cv_framework_CoreFoundation" = "xyes" ]]; then
    EXTRASYSSRCS="Darwin/sys_darwin.cpp"
  fi
  ;;
esac

EXTRASYSSRCS="$EXTRASYSSRCS main_unix.cpp prefs_unix.cpp"

dnl SDL overrides
if [[ "x$WANT_SDL" = "xyes" ]]; then
  AC_DEFINE(USE_SDL, 1, [Define to enble SDL support])
fi
if [[ "x$WANT_SDL_VIDEO" = "xyes" ]]; then
  AC_DEFINE(USE_SDL_VIDEO, 1, [Define to enable SDL video graphics support])
  VIDEOSRCS="../SDL/video_sdl.cpp"
  KEYCODES="../SDL/keycodes"
  case "$target_os" in
  *)
    EXTRASYSSRCS="$EXTRASYSSRCS ../dummy/clip_dummy.cpp"
    ;;
  esac
fi
if [[ "x$WANT_SDL_AUDIO" = "xyes" ]]; then
  AC_DEFINE(USE_SDL_AUDIO, 1, [Define to enable SDL audio support])
  AUDIOSRC="../SDL/audio_sdl.cpp"
fi

if [[ "x$HAVE_PTHREADS" = "xno" ]]; then
  dnl Serial, ethernet and audio support needs pthreads
  AC_MSG_WARN([You don't have pthreads, disabling serial, ethernet and audio support.])
  SERIALSRC=../dummy/serial_dummy.cpp
  ETHERSRC=../dummy/ether_dummy.cpp
  AUDIOSRC=../dummy/audio_dummy.cpp
fi
SYSSRCS="$VIDEOSRCS $EXTFSSRC $SERIALSRC $ETHERSRC $SCSISRC $AUDIOSRC $SEMSRC $UISRCS $EXTRASYSSRCS"

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

dnl Check that the host supports TUN/TAP devices
AC_CACHE_CHECK([whether TUN/TAP is supported],
  ac_cv_tun_tap_support, [
  AC_TRY_COMPILE([
    #if defined(HAVE_NET_IF_H) && defined(HAVE_NET_IF_TUN_H)
    #include <net/if.h>
    #include <net/if_tun.h>
    #endif
  ], [
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  ],
  ac_cv_tun_tap_support=yes, ac_cv_tun_tap_support=no
  )
])
AC_TRANSLATE_DEFINE(ENABLE_TUNTAP, "$ac_cv_tun_tap_support",
  [Define if your system supports TUN/TAP devices.])

dnl Various checks if the system supports vm_allocate() and the like functions.
have_mach_vm=no
if [[ "x$ac_cv_func_vm_allocate" = "xyes" -a "x$ac_cv_func_vm_deallocate" = "xyes" -a \
      "x$ac_cv_func_vm_protect" = "xyes" ]]; then
  have_mach_vm=yes
fi
AC_TRANSLATE_DEFINE(HAVE_MACH_VM, "$have_mach_vm",
  [Define if your system has a working vm_allocate()-based memory allocator.])

dnl Check that vm_allocate(), vm_protect() work
if [[ "x$have_mach_vm" = "xyes" ]]; then

AC_CACHE_CHECK([whether vm_protect works],
  ac_cv_vm_protect_works, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_cv_vm_protect_works=yes
  dnl First the tests that should segfault
  for test_def in NONE_READ NONE_WRITE READ_WRITE; do
    AC_TRY_RUN([
      #define CONFIGURE_TEST_VM_MAP
      #define TEST_VM_PROT_$test_def
      #include "../CrossPlatform/vm_alloc.cpp"
    ], ac_cv_vm_protect_works=no, rm -f core,
    dnl When cross-compiling, do not assume anything
    ac_cv_vm_protect_works="guessing no"
    )
  done
  AC_TRY_RUN([
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_PROT_RDWR_WRITE
    #include "../CrossPlatform/vm_alloc.cpp"
  ], , ac_cv_vm_protect_works=no,
  dnl When cross-compiling, do not assume anything
  ac_cv_vm_protect_works="guessing no"
  )
  AC_LANG_RESTORE
  ]
)

dnl Remove support for vm_allocate() if vm_protect() does not work
if [[ "x$have_mach_vm" = "xyes" ]]; then
  case $ac_cv_vm_protect_works in
    *yes) have_mach_vm=yes;;
    *no) have_mach_vm=no;;
  esac
fi
AC_TRANSLATE_DEFINE(HAVE_MACH_VM, "$have_mach_vm",
  [Define if your system has a working vm_allocate()-based memory allocator.])

fi dnl HAVE_MACH_VM

dnl Various checks if the system supports mmap() and the like functions.
dnl ... and Mach memory allocators are not supported
have_mmap_vm=no
if [[ "x$ac_cv_func_mmap" = "xyes" -a "x$ac_cv_func_munmap" = "xyes" -a \
      "x$ac_cv_func_mprotect" = "xyes" ]]; then
  if [[ "x$have_mach_vm" = "xno" ]]; then
    have_mmap_vm=yes
  fi
fi
AC_TRANSLATE_DEFINE(HAVE_MMAP_VM, "$have_mmap_vm",
  [Define if your system has a working mmap()-based memory allocator.])

dnl Check that mmap() and associated functions work.
if [[ "x$have_mmap_vm" = "xyes" ]]; then

dnl Check if we have a working anonymous mmap()
AC_CACHE_CHECK([whether mmap supports MAP_ANON],
  ac_cv_mmap_anon, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_MMAP_ANON
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_MMAP_ANON
    #include "../CrossPlatform/vm_alloc.cpp"
  ], ac_cv_mmap_anon=yes, ac_cv_mmap_anon=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_mmap_anon="guessing no"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_MMAP_ANON, "$ac_cv_mmap_anon",
  [Define if <sys/mman.h> defines MAP_ANON and mmap()'ing with MAP_ANON works.])

AC_CACHE_CHECK([whether mmap supports MAP_ANONYMOUS],
  ac_cv_mmap_anonymous, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_MMAP_ANONYMOUS
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_MMAP_ANON
    #include "../CrossPlatform/vm_alloc.cpp"
  ], ac_cv_mmap_anonymous=yes, ac_cv_mmap_anonymous=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_mmap_anonymous="guessing no"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_MMAP_ANONYMOUS, "$ac_cv_mmap_anonymous",
  [Define if <sys/mman.h> defines MAP_ANONYMOUS and mmap()'ing with MAP_ANONYMOUS works.])

AC_CACHE_CHECK([whether mprotect works],
  ac_cv_mprotect_works, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_cv_mprotect_works=yes
  dnl First the tests that should segfault
  for test_def in NONE_READ NONE_WRITE READ_WRITE; do
    AC_TRY_RUN([
      #define CONFIGURE_TEST_VM_MAP
      #define TEST_VM_PROT_$test_def
      #include "../CrossPlatform/vm_alloc.cpp"
    ], ac_cv_mprotect_works=no, rm -f core,
    dnl When cross-compiling, do not assume anything
    ac_cv_mprotect_works="guessing no"
    )
  done
  AC_TRY_RUN([
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_PROT_RDWR_WRITE
    #include "../CrossPlatform/vm_alloc.cpp"
  ], , ac_cv_mprotect_works=no,
  dnl When cross-compiling, do not assume anything
  ac_cv_mprotect_works="guessing no"
  )
  AC_LANG_RESTORE
  ]
)

dnl Remove support for mmap() if mprotect() does not work
if [[ "x$have_mmap_vm" = "xyes" ]]; then
  case $ac_cv_mprotect_works in
    *yes) have_mmap_vm=yes;;
    *no) have_mmap_vm=no;;
  esac
fi
AC_TRANSLATE_DEFINE(HAVE_MMAP_VM, $have_mmap_vm,
  [Define if your system has a working mmap()-based memory allocator.])

fi dnl HAVE_MMAP_VM

dnl Check if we can modify the __PAGEZERO segment for use as Low Memory
AC_CACHE_CHECK([whether __PAGEZERO can be Low Memory area 0x0000-0x2000],
  ac_cv_pagezero_hack, [
  ac_cv_pagezero_hack=no 
  if AC_TRY_COMMAND([Darwin/testlmem.sh 0x2000]); then
    ac_cv_pagezero_hack=yes
    dnl might as well skip the test for mmap-able low memory
    ac_cv_can_map_lm=no
  fi
])
AC_TRANSLATE_DEFINE(PAGEZERO_HACK, "$ac_cv_pagezero_hack",
  [Define if the __PAGEZERO Mach-O Low Memory Globals hack works on this system.])

dnl Check if we can mmap 0x2000 bytes from 0x0000
AC_CACHE_CHECK([whether we can map Low Memory area 0x0000-0x2000],
  ac_cv_can_map_lm, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #include "../CrossPlatform/vm_alloc.cpp"
    int main(void) { /* returns 0 if we could map the lowmem globals */
      volatile char * lm = 0;
      if (vm_init() < 0) exit(1);
      if (vm_acquire_fixed(0, 0x2000) < 0) exit(1);
      lm[0] = 'z';
      if (vm_release((char *)lm, 0x2000) < 0) exit(1);
      vm_exit(); exit(0);
    }
  ], ac_cv_can_map_lm=yes, ac_cv_can_map_lm=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_can_map_lm="guessing no"
  )
  AC_LANG_RESTORE
  ]
)

dnl Check signal handlers need to be reinstalled
AC_CACHE_CHECK([whether signal handlers need to be reinstalled],
  ac_cv_signal_need_reinstall, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #include <stdlib.h>
    #ifdef HAVE_UNISTD_H
    #include <unistd.h>
    #endif
    #include <signal.h>
    static int handled_signal = 0;
    RETSIGTYPE sigusr1_handler(int) { handled_signal++; }
    int main(void) { /* returns 0 if signals need not to be reinstalled */
      signal(SIGUSR1, sigusr1_handler); raise(SIGUSR1); raise(SIGUSR1);
      exit(handled_signal == 2);
    }
  ], ac_cv_signal_need_reinstall=yes, ac_cv_signal_need_reinstall=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_signal_need_reinstall="guessing yes"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(SIGNAL_NEED_REINSTALL, "$ac_cv_signal_need_reinstall",
  [Define if your system requires signals to be reinstalled.])

dnl Check if sigaction handlers need to be reinstalled
AC_CACHE_CHECK([whether sigaction handlers need to be reinstalled],
  ac_cv_sigaction_need_reinstall, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #include <stdlib.h>
    #ifdef HAVE_UNISTD_H
    #include <unistd.h>
    #endif
    #include <signal.h>
    static int handled_signal = 0;
    RETSIGTYPE sigusr1_handler(int) { handled_signal++; }
    typedef RETSIGTYPE (*signal_handler)(int);
    static signal_handler mysignal(int sig, signal_handler handler) {
      struct sigaction old_sa;
      struct sigaction new_sa;
      new_sa.sa_handler = handler;
      return ((sigaction(sig,&new_sa,&old_sa) < 0) ? SIG_IGN : old_sa.sa_handler);
    }
    int main(void) { /* returns 0 if signals need not to be reinstalled */
      mysignal(SIGUSR1, sigusr1_handler); raise(SIGUSR1); raise(SIGUSR1);
      exit(handled_signal == 2);
    }
  ], ac_cv_sigaction_need_reinstall=yes, ac_cv_sigaction_need_reinstall=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_sigaction_need_reinstall="guessing yes"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(SIGACTION_NEED_REINSTALL, "$ac_cv_sigaction_need_reinstall",
  [Define if your system requires sigactions to be reinstalled.])

dnl Check if Mach exceptions supported.
AC_CACHE_CHECK([whether your system supports Mach exceptions],
  ac_cv_have_mach_exceptions, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_UNISTD_H 1
    #define HAVE_MACH_VM 1
    #define HAVE_MACH_TASK_SELF 1
    #define HAVE_MACH_EXCEPTIONS 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../CrossPlatform/vm_alloc.cpp"
    #include "../CrossPlatform/sigsegv.cpp"
  ], [
  sigsegv_recovery=mach
  ac_cv_have_mach_exceptions=yes
  ],
  ac_cv_have_mach_exceptions=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_mach_exceptions=no
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_MACH_EXCEPTIONS, "$ac_cv_have_mach_exceptions",
  [Define if your system supports Mach exceptions.])

dnl Otherwise, check if extended signals are supported with <asm/ucontext.h>.
if [[ -z "$sigsegv_recovery" ]]; then
  AC_CACHE_CHECK([whether your system supports extended signal handlers via asm],
    ac_cv_have_asm_extended_signals, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_RUN([
      #ifdef HAVE_UNISTD_H
      #include <unistd.h>
      #endif
      #define HAVE_ASM_UCONTEXT 1
      #define HAVE_SIGINFO_T 1
      #define CONFIGURE_TEST_SIGSEGV_RECOVERY
      #include "../CrossPlatform/vm_alloc.cpp"
      #include "../CrossPlatform/sigsegv.cpp"
    ], [
    sigsegv_recovery=siginfo
    ac_cv_have_asm_extended_signals=yes
    ],
    ac_cv_have_asm_extended_signals=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_asm_extended_signals=no
    )
    AC_LANG_RESTORE
    ]
  )
  AC_TRANSLATE_DEFINE(HAVE_ASM_UCONTEXT, "$ac_cv_have_asm_extended_signals",
    [Define if your system has <asm/ucontext.h> header.])
  AC_TRANSLATE_DEFINE(HAVE_SIGINFO_T, "$ac_cv_have_asm_extended_signals",
    [Define if your system supports extended signals.])
fi

if [[ -z "$sigsegv_recovery" ]]; then
  AC_CACHE_CHECK([whether your system supports extended signal handlers],
    ac_cv_have_extended_signals, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_RUN([
      #ifdef HAVE_UNISTD_H
      #include <unistd.h>
      #endif
      #define HAVE_SIGINFO_T 1
      #define CONFIGURE_TEST_SIGSEGV_RECOVERY
      #include "../CrossPlatform/vm_alloc.cpp"
      #include "../CrossPlatform/sigsegv.cpp"
    ], [
    sigsegv_recovery=siginfo
    ac_cv_have_extended_signals=yes
    ],
    ac_cv_have_extended_signals=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_extended_signals=no
    )
    AC_LANG_RESTORE
    ]
  )
  AC_TRANSLATE_DEFINE(HAVE_SIGINFO_T, "$ac_cv_have_extended_signals",
    [Define if your system supports extended signals.])
fi

dnl Otherwise, check for subterfuges.
if [[ -z "$sigsegv_recovery" ]]; then
  AC_CACHE_CHECK([whether we then have a subterfuge for your system],
  ac_cv_have_sigcontext_hack, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_RUN([
      #define HAVE_SIGCONTEXT_SUBTERFUGE 1
      #define CONFIGURE_TEST_SIGSEGV_RECOVERY
      #include "../CrossPlatform/vm_alloc.cpp"
      #include "../CrossPlatform/sigsegv.cpp"
    ], [
    sigsegv_recovery=sigcontext
    ac_cv_have_sigcontext_hack=yes
    ],
    ac_cv_have_sigcontext_hack=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_sigcontext_hack=no
    )
    AC_LANG_RESTORE
  ])
  AC_TRANSLATE_DEFINE(HAVE_SIGCONTEXT_SUBTERFUGE, "$ac_cv_have_sigcontext_hack",
    [Define if we know a hack to replace siginfo_t->si_addr member.])
fi

dnl Check if we can ignore the fault (instruction skipping in SIGSEGV handler)
AC_CACHE_CHECK([whether we can skip instruction in SIGSEGV handler],
  ac_cv_have_skip_instruction, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_SIGSEGV_SKIP_INSTRUCTION 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../CrossPlatform/vm_alloc.cpp"
    #include "../CrossPlatform/sigsegv.cpp"
  ], ac_cv_have_skip_instruction=yes, ac_cv_have_skip_instruction=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_skip_instruction=no
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_SIGSEGV_SKIP_INSTRUCTION, "$ac_cv_have_skip_instruction",
  [Define if we can ignore the fault (instruction skipping in SIGSEGV handler).])

dnl Can we do Video on SEGV Signals ?
CAN_VOSF=no
if [[ -n "$sigsegv_recovery" ]]; then
  CAN_VOSF=yes
fi

dnl A dummy program that returns always true
AC_PATH_PROG([BLESS], "true")

dnl Check for linker script support
case $target_os:$target_cpu in
darwin*:*)      LINKER_SCRIPT_FLAGS="-Wl,-seg1addr,0x78048000";;
esac
if [[ -n "$LINKER_SCRIPT_FLAGS" ]]; then
  AC_CACHE_CHECK([whether linker script is usable],
    ac_cv_linker_script_works, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    saved_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $LINKER_SCRIPT_FLAGS"
    AC_TRY_RUN(
      [int main() {if ((char *)&main < (char *)0x70000000) return 1;}],
      [ac_cv_linker_script_works=yes],
      [ac_cv_linker_script_works=no],
      dnl When cross-compiling, assume it works
      [ac_cv_linker_script_works="guessing yes"]
    )
    AC_LANG_RESTORE
    if [[ "$ac_cv_linker_script_works" = "no" ]]; then
      LDFLAGS="$saved_LDFLAGS"
      LINKER_SCRIPT_FLAGS=""
    fi
  ])
fi
AC_TRANSLATE_DEFINE(HAVE_LINKER_SCRIPT, "$ac_cv_linker_script_works",
  [Define if there is a linker script to relocate the executable above 0x70000000.])

dnl Determine the addressing mode to use

ADDRESSING_MODE=""
AC_MSG_CHECKING([for the addressing mode to use])
for am in $ADDRESSING_TEST_ORDER; do
  case $am in
  real)
    dnl Requires ability to mmap() Low Memory globals
    if [[ "x$ac_cv_can_map_lm$ac_cv_pagezero_hack" = "xnono" ]]; then
      continue
    fi
    dnl Requires VOSF screen updates
    if [[ "x$CAN_VOSF" = "xno" ]]; then
      continue
    fi
    dnl Real addressing will probably work.
    ADDRESSING_MODE="real"
    WANT_VOSF=yes dnl we can use VOSF and we need it actually
    DEFINES="$DEFINES -DREAL_ADDRESSING"
    if [[ "x$ac_cv_pagezero_hack" = "xyes" ]]; then
      BLESS=Darwin/lowmem
      LDFLAGS="$LDFLAGS -pagezero_size 0x2000"
    fi
    break
    ;;
  direct)
    dnl Requires VOSF screen updates
    if [[ "x$CAN_VOSF" = "xyes" ]]; then
      ADDRESSING_MODE="direct"
      WANT_VOSF=yes dnl we can use VOSF and we need it actually
      DEFINES="$DEFINES -DDIRECT_ADDRESSING"
      break
    fi
    ;;
  banks)
    dnl Default addressing mode
    ADDRESSING_MODE="memory banks"
    break
    ;;
  *)
    AC_MSG_ERROR([Internal configure.in script error for $am addressing mode])
  esac
done
AC_MSG_RESULT($ADDRESSING_MODE)
if [[ "x$ADDRESSING_MODE" = "x" ]]; then
  AC_MSG_WARN([Sorry, no suitable addressing mode in $ADDRESSING_TEST_ORDER])
  ADDRESSING_MODE="memory banks"
fi

dnl Enable VOSF screen updates with this feature is requested and feasible
if [[ "x$WANT_VOSF" = "xyes" -a "x$CAN_VOSF" = "xyes" ]]; then
    AC_DEFINE(ENABLE_VOSF, 1, [Define if using video enabled on SEGV signals.])
else
    WANT_VOSF=no
fi

dnl Check for GCC 2.7 or higher.
HAVE_GCC27=no
AC_MSG_CHECKING(for GCC 2.7 or higher)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if ! (__GNUC__ - 1 > 1 || __GNUC_MINOR__ - 1 > 5)
                                     # error gcc < 2.7
                                     #endif
                                   ]])],
                  [AC_MSG_RESULT(yes); HAVE_GCC27=yes],
                  [AC_MSG_RESULT(no)])

dnl Check for GCC 3.0 or higher.
HAVE_GCC30=no
AC_MSG_CHECKING(for GCC 3.0 or higher)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if ! (__GNUC__ >= 3)
                                     # error gcc < 3
                                     #endif
                                   ]])],
                  [AC_MSG_RESULT(yes); HAVE_GCC30=yes],
                  [AC_MSG_RESULT(no)])

dnl Check for ICC.
AC_MSG_CHECKING(for ICC)
HAVE_ICC=no
if $CXX -V -v 2>&1 | grep -q "Intel(R) C++ Compiler"; then
  HAVE_ICC=yes
fi
AC_MSG_RESULT($HAVE_ICC)

dnl Set "-fomit-frame-pointer" on i386 GCC 2.7 or higher.
dnl Also set "-fno-exceptions" for C++ because exception handling requires
dnl the frame pointer.
if [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_I386" = "xyes" ]]; then
  CFLAGS="$CFLAGS -fomit-frame-pointer"
  CXXFLAGS="$CXXFLAGS -fomit-frame-pointer -fno-exceptions"
fi


dnl Add -mdynamic-no-pic for MacOS X (XXX icc10 will support MacOS X)
if [[ "x$HAVE_GCC30" = "xyes" -a "x$HAVE_ICC" = "xno" ]]; then
  SAVED_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -mdynamic-no-pic"
  AC_CACHE_CHECK([whether the compiler supports -mdynamic-no-pic],
    ac_cv_gcc_mdynamic_no_pic, [
    AC_TRY_COMPILE([],[],[ac_cv_gcc_mdynamic_no_pic=yes],[ac_cv_gcc_mdynamic_no_pic=no])
  ])
  if [[ "x$ac_cv_gcc_mdynamic_no_pic" = "xyes" ]]; then
    CXXFLAGS="$CXXFLAGS -mdynamic-no-pic"
  else
    CFLAGS="$SAVED_CFLAGS"
  fi
fi

dnl Select appropriate CPU source and REGPARAM define.
CPUSRCS="cpuemu.cpp"

dnl Select appropriate FPU source.
AC_CHECK_HEADERS(ieee754.h ieeefp.h floatingpoint.h nan.h)

for fpe in $FPE_CORE_TEST_ORDER; do
  case $fpe in
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
CPUSRCS="../uae_cpu/basilisk_glue.cpp ../uae_cpu/newcpu.cpp ../uae_cpu/readcpu.cpp $FPUSRCS cpustbl.cpp cpudefs.cpp $CPUSRCS"

dnl Generate Makefile.
AC_SUBST(DEFINES)
AC_SUBST(SYSSRCS)
AC_SUBST(CPUINCLUDES)
AC_SUBST(CPUSRCS)
AC_SUBST(BLESS)
AC_SUBST(KEYCODES)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl Print summary.
echo
echo Basilisk II configuration summary:
echo
echo SDL support ............................ : $SDL_SUPPORT
echo Enable video on SEGV signals ........... : $WANT_VOSF
echo Floating-Point emulation core .......... : $FPE_CORE
echo Addressing mode ........................ : $ADDRESSING_MODE
echo Bad memory access recovery type ........ : $sigsegv_recovery
echo
echo "Configuration done. Now type \"make\" (or \"gmake\")."
