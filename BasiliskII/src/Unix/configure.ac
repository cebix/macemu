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

AC_ARG_ENABLE(standalone-gui,[  --enable-standalone-gui enable a standalone GUI prefs editor [default=no]], [WANT_STANDALONE_GUI=$enableval], [WANT_STANDALONE_GUI=no])

dnl Mac OS X GUI.
AC_ARG_ENABLE(macosx-gui,    [  --enable-macosx-gui     enable Mac OS X GUI [default=no]], [WANT_MACOSX_GUI=$enableval], [WANT_MACOSX_GUI=no])

dnl Mac OS X Sound
AC_ARG_ENABLE(macosx-sound,  [  --enable-macosx-sound   enable Mac OS X Sound [default=no]], [WANT_MACOSX_SOUND=$enableval], [WANT_MACOSX_SOUND=no])

dnl Video options.
AC_ARG_ENABLE(xf86-dga,      [  --enable-xf86-dga       use the XFree86 DGA extension [default=yes]], [WANT_XF86_DGA=$enableval], [WANT_XF86_DGA=yes])
AC_ARG_ENABLE(xf86-vidmode,  [  --enable-xf86-vidmode   use the XFree86 VidMode extension [default=yes]], [WANT_XF86_VIDMODE=$enableval], [WANT_XF86_VIDMODE=yes])
AC_ARG_ENABLE(fbdev-dga,     [  --enable-fbdev-dga      use direct frame buffer access via /dev/fb [default=yes]], [WANT_FBDEV_DGA=$enableval], [WANT_FBDEV_DGA=yes])
AC_ARG_ENABLE(vosf,          [  --enable-vosf           enable video on SEGV signals [default=yes]], [WANT_VOSF=$enableval], [WANT_VOSF=yes])

dnl SDL options.
AC_ARG_ENABLE(sdl-static,    [  --enable-sdl-static     use SDL static libraries for linking [default=no]], [WANT_SDL_STATIC=$enableval], [WANT_SDL_STATIC=no])
AC_ARG_ENABLE(sdl-video,     [  --enable-sdl-video      use SDL for video graphics [default=no]], [WANT_SDL_VIDEO=$enableval], [WANT_SDL_VIDEO=no])
AC_ARG_ENABLE(sdl-audio,     [  --enable-sdl-audio      use SDL for audio [default=no]], [WANT_SDL_AUDIO=$enableval], [WANT_SDL_AUDIO=no])
AC_ARG_ENABLE(sdl-framework,    [  --enable-sdl-framework  use SDL framework [default=no]], [WANT_SDL_FRAMEWORK=$enableval], [WANT_SDL_FRAMEWORK=no])
AC_ARG_ENABLE(sdl-framework-prefix,    [  --enable-sdl-framework-prefix=PFX    default=/Library/Frameworks], [SDL_FRAMEWORK="$enableval"], [SDL_FRAMEWORK=/Library/Frameworks])
AC_ARG_WITH(sdl1,            [  --with-sdl1             use SDL 1.x, rather than SDL 2.x [default=no]], [WANT_SDL_VERSION_MAJOR=1], [])

dnl JIT compiler options.
AC_ARG_ENABLE(jit-compiler,  [  --enable-jit-compiler   enable JIT compiler [default=no]], [WANT_JIT=$enableval], [WANT_JIT=no])
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

dnl External packages.
AC_ARG_WITH(esd,             [  --with-esd              support ESD for sound under Linux/FreeBSD [default=yes]], [WANT_ESD=$withval], [WANT_ESD=yes])
AC_ARG_WITH(gtk,             [  --with-gtk              use GTK user interface [default=yes]],
  [case "$withval" in
   gtk1)      WANT_GTK="gtk";;
   gtk|gtk2)  WANT_GTK="$withval";;
   yes)       WANT_GTK="gtk2 gtk";;
   *)         WANT_GTK="no";;
   esac],
  [WANT_GTK="gtk2 gtk"])
AC_ARG_WITH(mon,             [  --with-mon              use mon as debugger [default=yes]], [WANT_MON=$withval], [WANT_MON=yes])

AC_ARG_WITH(bincue,   
  AS_HELP_STRING([--with-bincue], [Allow cdrom image files in bin/cue mode]))

AC_ARG_WITH(libvhd,   
  AS_HELP_STRING([--with-libvhd], [Enable VHD disk images]))

  
dnl Cross Compiling results in 'guesses' being made about the target system. These defaults are oftetimes incorrect.
dnl The following Environment variables allow you to configure the default guess value for each option in the configure script.
AC_ARG_VAR(BII_CROSS_SOCKLEN_T, [  Whether sys/socket.h defines type socklen_t. [default=guessing no]])
if [[ "x$BII_CROSS_SOCKLEN_T" = "x" ]]; then
	BII_CROSS_SOCKLEN_T="guessing no"
fi

AC_ARG_VAR(BII_CROSS_BYTE_BITFIELDS, [  Whether non-GCC compilers support byte bit-fields. [default=guessing no]])
if [[ "x$BII_CROSS_BYTE_BITFIELDS" = "x" ]]; then
	BII_CROSS_BYTE_BITFIELDS="guessing no"
fi

AC_ARG_VAR(BII_CROSS_VM_PROTECT, [  Whether vm_protect works on the target system [default=guessing no]])
if [[ "x$BII_CROSS_VM_PROTECT" = "x" ]]; then
	BII_CROSS_VM_PROTECT="guessing no"
fi

AC_ARG_VAR(BII_CROSS_MMAP_ANON, [  Whether anonymous mmap() works on the target system [default=guessing no]])
if [[ "x$BII_CROSS_MMAP_ANON" = "x" ]]; then
	BII_CROSS_MMAP_ANON="guessing no"
fi

AC_ARG_VAR(BII_CROSS_MMAP_SUPPORTS_MAP_ANONYMOUS, [  Whether <sys/mman.h> defines MAP_ANON and mmap()'ing with MAP_ANON works on the target system [default=guessing no]])
if [[ "x$BII_CROSS_MMAP_SUPPORTS_MAP_ANONYMOUS" = "x" ]]; then
	BII_CROSS_MMAP_SUPPORTS_MAP_ANONYMOUS="guessing no"
fi

AC_ARG_VAR(BII_CROSS_MPROTECT_WORKS, [  Whether mprotect works on the target system [default=guessing no]])
if [[ "x$BII_CROSS_MPROTECT_WORKS" = "x" ]]; then
	BII_CROSS_MPROTECT_WORKS="guessing no"
fi

AC_ARG_VAR(BII_CROSS_MAP_LOW_AREA, [  Whether the target system can map 0x2000 bytes from 0x0000 [default=guessing no]])
if [[ "x$BII_CROSS_MAP_LOW_AREA" = "x" ]]; then
	BII_CROSS_MAP_LOW_AREA="guessing no"
fi

AC_ARG_VAR(BII_CROSS_SIGNAL_NEED_REINSTALL, [  Whether the target system needs signal handlers to be reinstalled [default=guessing yes]])
if [[ "x$BII_CROSS_SIGNAL_NEED_REINSTALL" = "x" ]]; then
	BII_CROSS_SIGNAL_NEED_REINSTALL="guessing yes"
fi

AC_ARG_VAR(BII_CROSS_SIGACTION_NEED_REINSTALL, [  Whether the target system needs signal action handlers to be reinstalled [default=guessing yes]])
if [[ "x$BII_CROSS_SIGACTION_NEED_REINSTALL" = "x" ]]; then
	BII_CROSS_SIGACTION_NEED_REINSTALL="guessing yes"
fi

AC_ARG_VAR(BII_CROSS_HAVE_MACH_EXCEPTIONS, [  Whether the target system has mach exceptions [default=no]])
if [[ "x$BII_CROSS_HAVE_MACH_EXCEPTIONS" = "x" ]]; then
	BII_CROSS_HAVE_MACH_EXCEPTIONS="no"
fi

AC_ARG_VAR(BII_CROSS_HAVE_WIN32_EXCEPTIONS, [  Whether the target system has win32 exceptions [default=no]])
if [[ "x$BII_CROSS_HAVE_WIN32_EXCEPTIONS" = "x" ]]; then
	BII_CROSS_HAVE_WIN32_EXCEPTIONS="no"
fi

AC_ARG_VAR(BII_CROSS_HAVE_ASM_EXTENDED_SIGNALS, [  Whether the target system has extended signals supported with <asm/ucontext.h> [default=no]])
if [[ "x$BII_CROSS_HAVE_ASM_EXTENDED_SIGNALS" = "x" ]]; then
	BII_CROSS_HAVE_ASM_EXTENDED_SIGNALS="no"
fi

AC_ARG_VAR(BII_CROSS_HAVE_EXTENDED_SIGNALS, [  Whether the target system supports extended signal handlers [default=no]])
if [[ "x$BII_CROSS_HAVE_EXTENDED_SIGNALS" = "x" ]]; then
	BII_CROSS_HAVE_EXTENDED_SIGNALS="no"
fi

AC_ARG_VAR(BII_CROSS_HAVE_SIGCONTEXT_SUBTERFUGE, [  Whether there is a sigcontext subterfuge for the target system [default=no]])
if [[ "x$BII_CROSS_HAVE_SIGCONTEXT_SUBTERFUGE" = "x" ]]; then
	BII_CROSS_HAVE_SIGCONTEXT_SUBTERFUGE="no"
fi

AC_ARG_VAR(BII_CROSS_SIGSEGV_SKIP_INSTRUCTION, [  Whether the target system can skip instruction in SIGSEGV handler [default=no]])
if [[ "x$BII_CROSS_SIGSEGV_SKIP_INSTRUCTION" = "x" ]]; then
	BII_CROSS_SIGSEGV_SKIP_INSTRUCTION="no"
fi

dnl Canonical system information.
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Target OS type (target is host if not cross-compiling).
case "$target_os" in
  linux*)	OS_TYPE=linux;;
  netbsd*)	OS_TYPE=netbsd;;
  freebsd*)	OS_TYPE=freebsd;;
  solaris*)	OS_TYPE=solaris;;
  darwin*)	OS_TYPE=darwin;;
  *)		OS_TYPE=`echo $target_os | sed -e 's/-/_/g' | sed -e 's/\./_/g'`;;
esac
DEFINES="$DEFINES -DOS_$OS_TYPE"

dnl Target CPU type.
HAVE_I386=no
HAVE_M68K=no
HAVE_SPARC=no
HAVE_POWERPC=no
HAVE_X86_64=no
case "$target_cpu" in
  i386* | i486* | i586* | i686* | i786* ) HAVE_I386=yes;;
  m68k* ) HAVE_M68K=yes;;
  sparc* ) HAVE_SPARC=yes;;
  powerpc* ) HAVE_POWERPC=yes;;
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

dnl We use mon if possible.
MONSRCS=
if [[ "x$WANT_MON" = "xyes" ]]; then
  AC_MSG_CHECKING(for mon)
  mon_srcdir=../../../cxmon/src
  if grep mon_init $mon_srcdir/mon.h >/dev/null 2>/dev/null; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(ENABLE_MON, 1, [Define if using "mon".])
    MONSRCS="$mon_srcdir/mon.cpp $mon_srcdir/mon_6502.cpp $mon_srcdir/mon_z80.cpp $mon_srcdir/mon_cmd.cpp $mon_srcdir/mon_disass.cpp $mon_srcdir/mon_ppc.cpp $mon_srcdir/mon_lowmem.cpp $mon_srcdir/disass/floatformat.c $mon_srcdir/disass/i386-dis.c $mon_srcdir/disass/m68k-dis.c $mon_srcdir/disass/m68k-opc.c"
    CXXFLAGS="$CXXFLAGS -I$mon_srcdir -I$mon_srcdir/disass"
    AC_CHECK_LIB(ncurses, tgetent, ,
      [AC_CHECK_LIB(termcap, tgetent, ,
        [AC_CHECK_LIB(termlib, tgetent, ,
          [AC_CHECK_LIB(terminfo, tgetent, ,
            [AC_CHECK_LIB(Hcurses, tgetent, ,
              [AC_CHECK_LIB(curses, tgetent)])])])])])
    AC_CHECK_LIB(readline, readline)
  else
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Could not find mon, ignoring --with-mon.])
    WANT_MON=no
  fi
fi

dnl Checks for libraries.
AC_CHECK_LIB(posix4, sem_init)
AC_CHECK_LIB(rt, timer_create)
AC_CHECK_LIB(rt, shm_open)
AC_CHECK_LIB(m, cos)

dnl AC_CHECK_SDLFRAMEWORK($1=NAME, $2=INCLUDES, $3=ACTION_IF_SUCCESSFUL, $4=ACTION_IF_UNSUCCESSFUL)
dnl AC_TRY_LINK uses main() but SDL needs main to take args,
dnl therefore main is undefined with #undef.
dnl Framework can be in an custom location.
AC_DEFUN([AC_CHECK_SDLFRAMEWORK], [
  AS_VAR_PUSHDEF([ac_Framework], [ac_cv_framework_$1])
  AC_CACHE_CHECK([whether compiler supports framework $1],
    ac_Framework, [
    saved_LIBS="$LIBS"
    LIBS="$LIBS -framework $1"
    if [[ "x$SDL_FRAMEWORK" != "x/System/Library/Frameworks" ]]; then
      LIBS="$saved_LIBS -F$SDL_FRAMEWORK -framework $1"
    fi
    saved_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$SDL_FRAMEWORK/$1.framework/Headers"
    AC_TRY_LINK(
      [$2
#undef main], [],
      [AS_VAR_SET(ac_Framework, yes); $3], [AS_VAR_SET(ac_Framework, no);
LIBS="$saved_LIBS"; CPPFLAGS="$saved_CPPFLAGS"; $4]
    )
  ])
  AS_IF([test AS_VAR_GET(ac_Framework) = yes],
    [AC_DEFINE(AS_TR_CPP(HAVE_FRAMEWORK_$1), 1, [Define if framework $1 is available.])]
  )
  AS_VAR_POPDEF([ac_Framework])
])

dnl Do we need SDL?
WANT_SDL=no
if [[ "x$WANT_SDL_VIDEO" = "xyes" ]]; then
  WANT_SDL=yes
  WANT_XF86_DGA=no
  WANT_XF86_VIDMODE=no
  WANT_FBDEV_DGA=no
  SDL_SUPPORT="$SDL_SUPPORT video"
fi
if [[ "x$WANT_SDL_AUDIO" = "xyes" ]]; then
  WANT_SDL=yes
  SDL_SUPPORT="$SDL_SUPPORT audio"
fi
if [[ "x$WANT_SDL" = "xyes" ]]; then
  if [[ "x$WANT_SDL_FRAMEWORK" = "xyes" ]]; then
    TEMP_WANT_SDL_VERSION_MAJOR=$WANT_SDL_VERSION_MAJOR
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x" ]]; then
      TEMP_WANT_SDL_VERSION_MAJOR=2
    fi
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x2" ]]; then
      AC_CHECK_SDLFRAMEWORK(SDL2, [#include <SDL.h>], [
        WANT_SDL_VERSION_MAJOR=2
      ], [
        TEMP_WANT_SDL_VERSION_MAJOR=1
      ])
    fi
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x1" ]]; then
      AC_CHECK_SDLFRAMEWORK(SDL, [#include <SDL.h>], [
        WANT_SDL_VERSION_MAJOR=1
      ])
    fi
  else
    ac_cv_framework_SDL=no
  fi
  if [[ "x$ac_cv_framework_SDL" = "xno" ]]; then
    TEMP_WANT_SDL_VERSION_MAJOR=$WANT_SDL_VERSION_MAJOR
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x" ]]; then
      TEMP_WANT_SDL_VERSION_MAJOR=2
    fi

    dnl use PKG_PROG_PKG_CONFIG to declare PKG_CONFIG variables.  Otherwise,
    dnl PKG_* macros may fail, without much explanation.  The lack of this
    dnl was causing --with-sdl1 to fail, as SDL 1.x could not be detected,
    dnl as the 2nd call to PKG_CHECK_MODULES would fail, as $PKG_CONFIG
    dnl never got defined (bizarrely-enough).  -- dludwig@pobox.com
    PKG_PROG_PKG_CONFIG

    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x2" ]]; then
      PKG_CHECK_MODULES([sdl2], [sdl2 >= 2.0], [
        CFLAGS="$CFLAGS $sdl2_CFLAGS"
        CXXFLAGS="$CXXFLAGS $sdl2_CFLAGS"
        LIBS="$LIBS $sdl2_LIBS"
        WANT_SDL_VERSION_MAJOR=2
      ], [
        TEMP_WANT_SDL_VERSION_MAJOR=1
      ])
    fi
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x1" ]]; then
      PKG_CHECK_MODULES([sdl], [sdl >= 1.2], [
        CFLAGS="$CFLAGS $sdl_CFLAGS"
        CXXFLAGS="$CXXFLAGS $sdl_CFLAGS"
        LIBS="$LIBS $sdl_LIBS"
        WANT_SDL_VERSION_MAJOR=1
      ], [
        WANT_SDL=no
        WANT_SDL_VERSION_MAJOR=
      ])
    fi
  fi
  SDL_SUPPORT=`echo "$SDL_SUPPORT" | sed -e "s/^ //"`
else
  SDL_SUPPORT="none"
fi

dnl We need X11, if not using SDL or Mac GUI.
if [[ "x$WANT_SDL_VIDEO" = "xno" -a "x$WANT_MACOSX_GUI" = "xno" ]]; then
  AC_PATH_XTRA
  if [[ "x$no_x" = "xyes" ]]; then
    AC_MSG_ERROR([You need X11 to run Basilisk II.])
  fi
  CFLAGS="$CFLAGS $X_CFLAGS"
  CXXFLAGS="$CXXFLAGS $X_CFLAGS"
  LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lX11 -lXext $X_EXTRA_LIBS"
fi

dnl BINCUE
AS_IF([test  "x$with_bincue" = "xyes" ], [have_bincue=yes], [have_bincue=no])
AS_IF([test  "x$have_bincue" = "xyes" ], [
   if [[ "x$WANT_SDL_AUDIO" = "xyes" ]]; then
       DEFINES="$DEFINES -DBINCUE"
       AC_SUBST(USE_BINCUE, yes)
   else
       AC_MSG_ERROR([You need SDL Audio to use BINCUE support.])
       AC_SUBST(USE_BINCUE, no)
   fi
], [AC_SUBST(USE_BINCUE, no)])

dnl LIBVHD
AS_IF([test  "x$with_libvhd" = "xyes" ], [have_libvhd=yes], [have_libvhd=no])
AS_IF([test  "x$have_libvhd" = "xyes" ], [
       CPPFLAGS="$CPPFLAGS -DHAVE_LIBVHD"
	   LIBS="$LIBS -lvhd"
	   case $target_os in
	   linux*)
	   LIBS="$LIBS -luuid" 
	   esac
	   AC_CHECK_LIB(vhd, vhd_open)
	   AC_CHECK_LIB(vhd, vhd_io_read)
	   AC_CHECK_LIB(vhd, vhd_io_write)
	   AC_CHECK_LIB(vhd, vhd_close)
])



dnl We want pthreads. Try libpthread first, then npth, then libc_r (FreeBSD), then PTL.
HAVE_PTHREADS=no
AC_SEARCH_LIBS([pthread_create], [pthread npth c_r PTL], [
  HAVE_PTHREADS=yes
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

dnl We use DGA (XFree86 or fbdev) if possible.
if [[ "x$WANT_XF86_DGA" = "xyes" ]]; then
  AC_CHECK_LIB(Xxf86dga, XF86DGAQueryExtension, [
    AC_DEFINE(ENABLE_XF86_DGA, 1, [Define if using XFree86 DGA extension.])
    LIBS="$LIBS -lXxf86dga"
    if [[ "x$WANT_FBDEV_DGA" = "xyes" ]]; then
      AC_MSG_WARN([Cannot have both --enable-xf86-dga and --enable-fbdev-dga, ignoring --enable-fbdev-dga.])
      WANT_FBDEV_DGA=no
    fi
  ], [
    AC_MSG_WARN([Could not find XFree86 DGA extension, ignoring --enable-xf86-dga.])
    WANT_XF86_DGA=no
  ])
fi
if [[ "x$WANT_FBDEV_DGA" = "xyes" ]]; then
  AC_DEFINE(ENABLE_FBDEV_DGA, 1, [Define if using DGA with framebuffer device.])
fi

dnl We use XFree86 VidMode if possible.
if [[ "x$WANT_XF86_VIDMODE" = "xyes" ]]; then
  AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension, [
    AC_DEFINE(ENABLE_XF86_VIDMODE, 1, [Define if using XFree86 DGA extension.])
    LIBS="$LIBS -lXxf86vm"
  ], [
    AC_MSG_WARN([Could not find XFree86 VidMode extension, ignoring --enable-xf86-vidmode.])
    WANT_XF86_VIDMODE=no
  ])
fi

dnl We use GTK+ if possible.
UISRCS=../dummy/prefs_editor_dummy.cpp
case "x$WANT_GTK" in
xgtk2*)
  AM_PATH_GTK_2_0(1.3.15, [
    GUI_CFLAGS="$GTK_CFLAGS"
    GUI_LIBS="$GTK_LIBS"
    WANT_GTK=gtk2
  ], [
    case "x${WANT_GTK}x" in
    *gtkx)
      AC_MSG_WARN([Could not find GTK+ 2.0, trying with GTK+ 1.2.])
      WANT_GTK=gtk
      ;;
    *)
      AC_MSG_WARN([Could not find GTK+, disabling user interface.])
      WANT_GTK=no
      ;;
    esac
  ])
  ;;
esac
if [[ "x$WANT_GTK" = "xgtk" ]]; then
  AM_PATH_GTK(1.2.0, [
    GUI_CFLAGS="$GTK_CFLAGS"
    GUI_LIBS="$GTK_LIBS"
    dnl somehow, <gnome-i18n.h> would redefine gettext() to nothing if
    dnl ENABLE_NLS is not set, thusly conflicting with C++ <string> which
    dnl includes <libintl.h>
    AM_GNU_GETTEXT
    B2_PATH_GNOMEUI([
      AC_DEFINE(HAVE_GNOMEUI, 1, [Define if libgnomeui is available.])
      GUI_CFLAGS="$GUI_CFLAGS $GNOMEUI_CFLAGS"
      GUI_LIBS="$GUI_LIBS $GNOMEUI_LIBS"
    ], [])
  ], [
    AC_MSG_WARN([Could not find GTK+, disabling user interface.])
    WANT_GTK=no
  ])
fi
if [[ "x$WANT_GTK" != "xno" -a "x$WANT_STANDALONE_GUI" = "xno" ]]; then
  AC_DEFINE(ENABLE_GTK, 1, [Define if using GTK.])
  UISRCS=prefs_editor_gtk.cpp
fi
AC_SUBST(GUI_CFLAGS)
AC_SUBST(GUI_LIBS)

dnl Build external GUI if requested.
if [[ "$WANT_STANDALONE_GUI" != "yes" ]]; then
  WANT_STANDALONE_GUI=no
fi
if [[ "$WANT_GTK" = "no" ]]; then
  WANT_STANDALONE_GUI=no
fi
AC_SUBST(STANDALONE_GUI, [$WANT_STANDALONE_GUI])

dnl We use ESD if possible.
if [[ "x$WANT_ESD" = "xyes" ]]; then
  AM_PATH_ESD(0.2.8, [
    AC_DEFINE(ENABLE_ESD, 1, [Define is using ESD.])
    CFLAGS="$CFLAGS $ESD_CFLAGS"
    CXXFLAGS="$CXXFLAGS $ESD_CFLAGS"
    LIBS="$LIBS $ESD_LIBS"
  ], [
    AC_MSG_WARN([Could not find ESD, disabling ESD support.])
    WANT_ESD=no
  ])
fi

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
AC_CHECK_HEADERS(linux/if.h linux/if_tun.h net/if.h net/if_tun.h, [], [], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])
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
  ac_cv_type_socklen_t="$BII_CROSS_SOCKLEN_T"
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

dnl Check for headers and functions related to pty support (sshpty.c)
dnl From openssh-3.2.2p1 configure.ac

AC_CHECK_HEADERS(strings.h login.h sys/bsdtty.h sys/stat.h util.h pty.h)
AC_CHECK_FUNCS(_getpty vhangup strlcpy)

case "$host" in
*-*-hpux10.26)
	disable_ptmx_check=yes
	;;
*-*-linux*)
	no_dev_ptmx=1
	;;
mips-sony-bsd|mips-sony-newsos4)
	AC_DEFINE(HAVE_NEWS4, 1, [Define if you are on NEWS-OS (additions from openssh-3.2.2p1, for sshpty.c).])
	;;
*-*-sco3.2v4*)
	no_dev_ptmx=1
	;;
*-*-sco3.2v5*)
	no_dev_ptmx=1
	;;
*-*-cygwin*)
	no_dev_ptmx=1
	;;
*-*-darwin*)
	no_dev_ptmx=1
	LIBS="$LIBS -lstdc++"
	;;
*-*-freebsd*)
	no_dev_ptmx=1
	;;
esac

if test ! -z "$cross_compiling" && test "x$cross_compiling" = "xyes"; then
	AC_MSG_WARN([cross compiling: Disabling /dev/ptmx test])
	disable_ptmx_check=yes
fi
if test -z "$no_dev_ptmx" ; then
	if test "x$disable_ptmx_check" != "xyes" ; then
		AC_CHECK_FILE(["/dev/ptmx"],
			[
				AC_DEFINE_UNQUOTED([HAVE_DEV_PTMX], [1],
					[Define if you have /dev/ptmx])
				have_dev_ptmx=1
			]
		)
	fi
fi

if test ! -z "$cross_compiling" && test "x$cross_compiling" != "xyes"; then
	AC_CHECK_FILE(["/dev/ptc"],
		[
			AC_DEFINE_UNQUOTED([HAVE_DEV_PTS_AND_PTC], [1],
				[Define if you have /dev/ptc])
			have_dev_ptc=1
		]
	)
else
	AC_MSG_WARN([cross compiling: Disabling /dev/ptc test])
fi
	
dnl (end of code from openssh-3.2.2p1 configure.ac)

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
      ac_cv_have_byte_bitfields="$BII_CROSS_BYTE_BITFIELDS"
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
SERIALSRC=serial_unix.cpp
ETHERSRC=../dummy/ether_dummy.cpp
SCSISRC=../dummy/scsi_dummy.cpp
AUDIOSRC=../dummy/audio_dummy.cpp
EXTFSSRC=extfs_unix.cpp
EXTRASYSSRCS=
CAN_NATIVE_M68K=no
case "$target_os" in
linux*)
  ETHERSRC=ether_unix.cpp
  AUDIOSRC=audio_oss_esd.cpp
  SCSISRC=Linux/scsi_linux.cpp
  ;;
freebsd*)
  ETHERSRC=ether_unix.cpp
  AUDIOSRC=audio_oss_esd.cpp
  DEFINES="$DEFINES -DBSD_COMP"
  CXXFLAGS="$CXXFLAGS -fpermissive"
  dnl Check for the CAM library
  AC_CHECK_LIB(cam, cam_open_btl, HAVE_LIBCAM=yes, HAVE_LIBCAM=no) 
  if [[ "x$HAVE_LIBCAM" = "xno" ]]; then
    AC_MSG_WARN([Cannot find libcam for SCSI management, disabling SCSI support.])
  else
    dnl Check for the sys kernel includes
    AC_CHECK_HEADER(camlib.h)
    if [[ "x$ac_cv_header_camlib_h" = "xno" ]]; then
      dnl In this case I should fix this thing including a "patch"
      dnl to access directly to the functions in the kernel :) --Orlando
      AC_MSG_WARN([Cannot find includes for CAM library, disabling SCSI support.])
    else
      SCSISRC=FreeBSD/scsi_freebsd.cpp
      LIBS="$LIBS -lcam"
      DEFINES="$DEFINES -DCAM"
    fi
  fi
  ;;
netbsd*)
  CAN_NATIVE_M68K=yes
  ETHERSRC=ether_unix.cpp
  ;;
solaris*)
  AUDIOSRC=Solaris/audio_solaris.cpp
  DEFINES="$DEFINES -DBSD_COMP -D_POSIX_PTHREAD_SEMANTICS"
  ;;
irix*)
  AUDIOSRC=Irix/audio_irix.cpp
  EXTRASYSSRCS=Irix/unaligned.c
  LIBS="$LIBS -laudio"
  WANT_ESD=no

  dnl Check if our compiler supports -IPA (MIPSPro)
  HAVE_IPA=no
  ocflags="$CFLAGS"
  CFLAGS=`echo "$CFLAGS -IPA" | sed -e "s/-g//g"`
  AC_MSG_CHECKING(if "-IPA" works)
  dnl Do a test compile of an empty function
  AC_TRY_COMPILE([#if defined __GNUC__
                  # error GCC does not support IPA yet
                  #endif],, [AC_MSG_RESULT(yes); HAVE_IPA=yes], AC_MSG_RESULT(no))
  CFLAGS="$ocflags"
  ;;
darwin*)
  ETHERSRC=ether_unix.cpp
  if [[ "x$ac_cv_framework_IOKit" = "xyes" -a "x$ac_cv_framework_CoreFoundation" = "xyes" ]]; then
    EXTRASYSSRCS="../MacOSX/sys_darwin.cpp"
  fi
  if [[ "x$ac_cv_framework_Carbon" = "xyes" ]]; then
    EXTFSSRC=../MacOSX/extfs_macosx.cpp
  fi
  ;;
cygwin*)
  SERIALSRC="../dummy/serial_dummy.cpp"
  EXTRASYSSRCS="../Windows/BasiliskII.rc"
  ;;
esac

dnl Is the slirp library supported?
case "$ac_cv_have_byte_bitfields" in
yes|"guessing yes")
  CAN_SLIRP=yes
  ETHERSRC=ether_unix.cpp
  ;;
esac
if [[ -n "$CAN_SLIRP" ]]; then
  AC_DEFINE(HAVE_SLIRP, 1, [Define if slirp library is supported])
  SLIRP_SRCS="\
    ../slirp/bootp.c     ../slirp/ip_output.c  ../slirp/tcp_input.c  \
    ../slirp/cksum.c     ../slirp/mbuf.c       ../slirp/tcp_output.c \
    ../slirp/debug.c     ../slirp/misc.c       ../slirp/tcp_subr.c   \
    ../slirp/if.c        ../slirp/sbuf.c       ../slirp/tcp_timer.c  \
    ../slirp/ip_icmp.c   ../slirp/slirp.c      ../slirp/tftp.c       \
    ../slirp/ip_input.c  ../slirp/socket.c     ../slirp/udp.c"
fi
AC_SUBST(SLIRP_SRCS)

if [[ "x$WANT_MACOSX_GUI" = "xyes" ]]; then
  CPPFLAGS="$CPPFLAGS -I../MacOSX"
  LIBS="$LIBS -framework CoreAudio -framework AudioUnit -framework AudioToolbox"

  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/Controller.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/Emulator.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/EmulatorView.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/PrefsEditor.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/NNThread.m"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/misc_macosx.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/clip_macosx.cpp"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/main_macosx.mm"
  EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/prefs_macosx.cpp"

  VIDEOSRCS="../MacOSX/video_macosx.mm"
else
  EXTRASYSSRCS="$EXTRASYSSRCS main_unix.cpp prefs_unix.cpp"
fi

if [[ "x$WANT_MACOSX_SOUND" = "xyes" ]]; then
  AUDIOSRC="../MacOSX/audio_macosx.cpp ../MacOSX/AudioBackEnd.cpp ../MacOSX/AudioDevice.cpp ../MacOSX/MacOSX_sound_if.cpp"
  LIBS="$LIBS -framework AudioToolbox -framework AudioUnit -framework CoreAudio"
fi

dnl SDL overrides
if [[ "x$WANT_SDL" = "xyes" ]]; then
  AC_DEFINE(USE_SDL, 1, [Define to enble SDL support])
  if [[ "x$WANT_SDL_FRAMEWORK" = "xyes" ]]; then
    EXTRASYSSRCS="$EXTRASYSSRCS ../SDL/SDLMain.m"
  fi
fi
if [[ "x$WANT_SDL_VIDEO" = "xyes" ]]; then
  AC_DEFINE(USE_SDL_VIDEO, 1, [Define to enable SDL video graphics support])
  VIDEOSRCS="../SDL/video_sdl.cpp ../SDL/video_sdl2.cpp"
  KEYCODES="../SDL/keycodes"
  if [[ "x$ac_cv_framework_Carbon" = "xyes" ]]; then
    AC_MSG_CHECKING([whether __LP64__ is defined])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if !defined(__LP64__)
                                         # error __LP64__ not defined
                                         #endif
                                       ]])],
                      [AC_MSG_RESULT(yes); LP64_DEFINED=yes],
                      [AC_MSG_RESULT(no)])
    if [[ "x$LP64_DEFINED" = "xyes" ]]; then
      EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/clip_macosx64.mm ../pict.c"
    else
      EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/clip_macosx.cpp"
    fi
    EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/utils_macosx.mm"
    CPPFLAGS="$CPPFLAGS -I../MacOSX"
  else
    case "$target_os" in
    cygwin*)
      EXTRASYSSRCS="$EXTRASYSSRCS ../Windows/clip_windows.cpp"
      ;;
    *)
      EXTRASYSSRCS="$EXTRASYSSRCS ../dummy/clip_dummy.cpp"
      ;;
    esac
  fi
elif [[ "x$WANT_MACOSX_GUI" != "xyes" ]]; then
  VIDEOSRCS="video_x.cpp"
  KEYCODES="keycodes"
  EXTRASYSSRCS="$EXTRASYSSRCS clip_unix.cpp"
fi
if [[ "x$WANT_SDL_AUDIO" = "xyes" ]]; then
  AC_DEFINE(USE_SDL_AUDIO, 1, [Define to enable SDL audio support])
  AUDIOSRC="../SDL/audio_sdl.cpp"
fi

dnl BINCUE overrides

if [[ "x$have_bincue" = "xyes" ]]; then
  EXTRASYSSRCS="$EXTRASYSSRCS bincue_unix.cpp"
fi

dnl libvhd overrides

if [[ "x$have_libvhd" = "xyes" ]]; then
  EXTRASYSSRCS="$EXTRASYSSRCS vhd_unix.cpp"
fi


dnl Use 68k CPU natively?
WANT_NATIVE_M68K=no
if [[ "x$HAVE_M68K" = "xyes" -a "x$CAN_NATIVE_M68K" = "xyes" ]]; then
  AC_DEFINE(ENABLE_NATIVE_M68K, 1, [Define if using native 68k mode.])
  WANT_NATIVE_M68K=yes
fi

if [[ "x$HAVE_PTHREADS" = "xno" ]]; then
  dnl Serial, ethernet and audio support needs pthreads
  AC_MSG_WARN([You don't have pthreads, disabling serial, ethernet and audio support.])
  SERIALSRC=../dummy/serial_dummy.cpp
  ETHERSRC=../dummy/ether_dummy.cpp
  AUDIOSRC=../dummy/audio_dummy.cpp
fi
SYSSRCS="$VIDEOSRCS $EXTFSSRC $SERIALSRC $ETHERSRC $SCSISRC $AUDIOSRC $SEMSRC $UISRCS $MONSRCS $EXTRASYSSRCS"

dnl Define a macro that translates a yesno-variable into a C macro definition
dnl to be put into the config.h file
dnl $1 -- the macro to define
dnl $2 -- the value to translate
dnl $3 -- template name
AC_DEFUN([AC_TRANSLATE_DEFINE], [
    if [[ ""x$2"" = "xyes" -o ""x$2"" = "xguessing yes" ]]; then
        AC_DEFINE($1, 1, $3)
    fi
])

dnl Check that the host supports TUN/TAP devices
AC_CACHE_CHECK([whether TUN/TAP is supported],
  ac_cv_tun_tap_support, [
  AC_TRY_COMPILE([
    #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
    #endif
    #if defined(HAVE_LINUX_IF_H) && defined(HAVE_LINUX_IF_TUN_H)
    #include <linux/if.h>
    #include <linux/if_tun.h>
    #endif
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
    ac_cv_vm_protect_works="$BII_CROSS_VM_PROTECT"
    )
  done
  AC_TRY_RUN([
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_PROT_RDWR_WRITE
    #include "../CrossPlatform/vm_alloc.cpp"
  ], , ac_cv_vm_protect_works=no,
  dnl When cross-compiling, do not assume anything
  ac_cv_vm_protect_works="$BII_CROSS_VM_PROTECT"
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
  ac_cv_mmap_anon="$BII_CROSS_MMAP_ANON"
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
  ac_cv_mmap_anonymous="$BII_CROSS_MMAP_SUPPORTS_MAP_ANONYMOUS"
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
    ac_cv_mprotect_works="$BII_CROSS_MPROTECT_WORKS"
    )
  done
  AC_TRY_RUN([
    #define CONFIGURE_TEST_VM_MAP
    #define TEST_VM_PROT_RDWR_WRITE
    #include "../CrossPlatform/vm_alloc.cpp"
  ], , ac_cv_mprotect_works=no,
  dnl When cross-compiling, do not assume anything
  ac_cv_mprotect_works="$BII_CROSS_MPROTECT_WORKS"
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
AC_TRANSLATE_DEFINE(HAVE_MMAP_VM, "$have_mmap_vm",
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
  ac_cv_can_map_lm="$BII_CROSS_MAP_LOW_AREA"
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
  ac_cv_signal_need_reinstall="$BII_CROSS_SIGNAL_NEED_REINSTALL"
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
  ac_cv_sigaction_need_reinstall="$BII_CROSS_SIGACTION_NEED_REINSTALL"
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
  ac_cv_have_mach_exceptions=yes
  ],
  ac_cv_have_mach_exceptions=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_mach_exceptions="$BII_CROSS_HAVE_MACH_EXCEPTIONS"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_MACH_EXCEPTIONS, "$ac_cv_have_mach_exceptions",
  [Define if your system supports Mach exceptions.])

dnl Check if Windows exceptions are supported.
AC_CACHE_CHECK([whether your system supports Windows exceptions],
  ac_cv_have_win32_exceptions, [
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  AC_TRY_RUN([
    #define HAVE_WIN32_EXCEPTIONS 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../CrossPlatform/vm_alloc.cpp"
    #include "../CrossPlatform/sigsegv.cpp"
  ], [
  ac_cv_have_win32_exceptions=yes
  ],
  ac_cv_have_win32_exceptions=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_win32_exceptions="$BII_CROSS_HAVE_WIN32_EXCEPTIONS"
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_WIN32_EXCEPTIONS, "$ac_cv_have_win32_exceptions",
  [Define if your system supports Windows exceptions.])

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
    ac_cv_have_asm_extended_signals=yes
    ],
    ac_cv_have_asm_extended_signals=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_asm_extended_signals="$BII_CROSS_HAVE_ASM_EXTENDED_SIGNALS"
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
    ac_cv_have_extended_signals=yes
    ],
    ac_cv_have_extended_signals=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_extended_signals="$BII_CROSS_HAVE_EXTENDED_SIGNALS"
    )
    AC_LANG_RESTORE
    ]
  )
  AC_TRANSLATE_DEFINE(HAVE_SIGINFO_T, "$ac_cv_have_extended_signals",
    [Define if your system supports extended signals.])
fi

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
  ac_cv_have_sigcontext_hack=yes
  ],
  ac_cv_have_sigcontext_hack=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_sigcontext_hack="$BII_CROSS_HAVE_SIGCONTEXT_SUBTERFUGE"
  )
  AC_LANG_RESTORE
])
AC_TRANSLATE_DEFINE(HAVE_SIGCONTEXT_SUBTERFUGE, "$ac_cv_have_sigcontext_hack",
  [Define if we know a hack to replace siginfo_t->si_addr member.])


dnl Resolve and set the proper sigsegv_recovery method... 
if [[ "x$ac_cv_have_mach_exceptions" = "xyes" ]]; then
  sigsegv_recovery=mach
elif [[ "x$ac_cv_have_win32_exceptions" = "xyes" ]]; then
  sigsegv_recovery=win32
elif [[ "x$ac_cv_have_asm_extended_signals" = "xyes" ]]; then
  sigsegv_recovery=siginfo
elif [[ "x$ac_cv_have_extended_signals" = "xyes" ]]; then
  sigsegv_recovery=siginfo
elif [[ "x$ac_cv_have_sigcontext_hack" = "xyes" ]]; then
  sigsegv_recovery=sigcontext
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
  ac_cv_have_skip_instruction="$BII_CROSS_SIGSEGV_SKIP_INSTRUCTION"
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
linux*:i?86)    LINKER_SCRIPT_FLAGS="-Wl,-T,ldscripts/linux-i386.ld";;
linux*:x86_64)  LINKER_SCRIPT_FLAGS="-Wl,-T,ldscripts/linux-x86_64.ld";;
linux*:powerpc) LINKER_SCRIPT_FLAGS="-Wl,-T,ldscripts/linux-ppc.ld";;
netbsd*:i?86)   LINKER_SCRIPT_FLAGS="-Wl,-T,ldscripts/linux-i386.ld";;
freebsd*:i?86)  LINKER_SCRIPT_FLAGS="-Wl,-T,ldscripts/freebsd-i386.ld";;
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
if [[ "x$WANT_NATIVE_M68K" = "xyes" ]]; then
  ADDRESSING_MODE="real"
else
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
fi

dnl Banked Memory Addressing mode is not supported by the JIT compiler
if [[ "x$WANT_JIT" = "xyes" -a "x$ADDRESSING_MODE" = "xmemory banks" ]]; then
  AC_MSG_ERROR([Sorry, the JIT Compiler requires Direct Addressing, at least])
fi

dnl Enable VOSF screen updates with this feature is requested and feasible
if [[ "x$WANT_VOSF" = "xyes" -a "x$CAN_VOSF" = "xyes" ]]; then
    AC_DEFINE(ENABLE_VOSF, 1, [Define if using video enabled on SEGV signals.])
else
    WANT_VOSF=no
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

dnl (gb) Do not merge constants since it breaks fpu/fpu_x86.cpp.
dnl As of 2001/08/02, this affects the following compilers:
dnl Official: probably gcc-3.1 (mainline CVS)
dnl Mandrake: gcc-2.96 >= 0.59mdk, gcc-3.0.1 >= 0.1mdk
dnl Red Hat : gcc-2.96 >= 89, gcc-3.0 >= 1
if [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_ICC" = "xno" ]]; then
  SAVED_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -fno-merge-constants"
  AC_CACHE_CHECK([whether GCC supports constants merging], ac_cv_gcc_constants_merging, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_COMPILE([],[],[ac_cv_gcc_constants_merging=yes],[ac_cv_gcc_constants_merging=no])
    AC_LANG_RESTORE
  ])
  if [[ "x$ac_cv_gcc_constants_merging" != "xyes" ]]; then
    CXXFLAGS="$SAVED_CXXFLAGS"
  fi
fi

dnl Store motion was introduced in 3.3-hammer branch and any gcc >= 3.4
dnl However, there are some corner cases exposed on x86-64
if [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_ICC" = "xno" ]]; then
  SAVED_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -fno-gcse-sm"
  AC_CACHE_CHECK([whether GCC supports store motion], ac_cv_gcc_store_motion, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_COMPILE([],[],[ac_cv_gcc_store_motion=yes],[ac_cv_gcc_store_motion=no])
    AC_LANG_RESTORE
  ])
  if [[ "x$ac_cv_gcc_store_motion" != "xyes" ]]; then
    CXXFLAGS="$SAVED_CXXFLAGS"
  fi
fi

dnl Add -fno-strict-aliasing for slirp sources
if [[ "x$HAVE_GCC30" = "xyes" ]]; then
  SAVED_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -fno-strict-aliasing"
  AC_CACHE_CHECK([whether the compiler supports -fno-strict-aliasing],
    ac_cv_gcc_no_strict_aliasing, [
    AC_TRY_COMPILE([],[],[ac_cv_gcc_no_strict_aliasing=yes],[ac_cv_gcc_no_strict_aliasing=no])
  ])
  if [[ "x$ac_cv_gcc_no_strict_aliasing" = "xyes" ]]; then
    AC_SUBST(SLIRP_CFLAGS, "-fno-strict-aliasing")
  fi
  CFLAGS="$SAVED_CFLAGS"
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
ASM_OPTIMIZATIONS=none
CPUSRCS="cpuemu1.cpp cpuemu2.cpp cpuemu3.cpp cpuemu4.cpp cpuemu5.cpp cpuemu6.cpp cpuemu7.cpp cpuemu8.cpp"

dnl (gb) JITSRCS will be emptied later if the JIT is not available
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
elif [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_SPARC" = "xyes" -a "x$HAVE_GAS" = "xyes" ]]; then
  dnl SPARC CPU
  case "$target_os" in
  solaris*)
    AC_MSG_CHECKING(SPARC CPU architecture)
    SPARC_TYPE=`Solaris/which_sparc`
    AC_MSG_RESULT($SPARC_TYPE)
    case "$SPARC_TYPE" in
    SPARC_V8)
      ASM_OPTIMIZATIONS="SPARC V8 architecture"
      DEFINES="$DEFINES -DSPARC_V8_ASSEMBLY" dnl -DOPTIMIZED_FLAGS"
      CFLAGS="$CFLAGS -Wa,-Av8"
      CXXFLAGS="$CXXFLAGS -Wa,-Av8"
      ;;
    SPARC_V9)
      ASM_OPTIMIZATIONS="SPARC V9 architecture"
      DEFINES="$DEFINES -DSPARC_V9_ASSEMBLY" dnl -DOPTIMIZED_FLAGS"
      CFLAGS="$CFLAGS -Wa,-Av9"
      CXXFLAGS="$CXXFLAGS -Wa,-Av9"
      ;;
    esac
    ;;
  esac
elif [[ "x$WANT_NATIVE_M68K" = "xyes" ]]; then
  dnl Native m68k, no emulation
  CPUINCLUDES="-I../native_cpu"
  CPUSRCS="asm_support.s"
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
if [[ "x$WANT_NATIVE_M68K" = "xno" ]]; then
  CPUINCLUDES="-I../uae_cpu"
  CPUSRCS="../uae_cpu/basilisk_glue.cpp ../uae_cpu/memory.cpp ../uae_cpu/newcpu.cpp ../uae_cpu/readcpu.cpp $FPUSRCS cpustbl.cpp cpudefs.cpp $CPUSRCS $JITSRCS"
fi

dnl Or if we have -IPA (MIPSPro compilers)
if [[ "x$HAVE_IPA" = "xyes" ]]; then
  CFLAGS="`echo $CFLAGS | sed -e 's/-g//g'` -O3 -OPT:Olimit=0 -IPA"
  CXXFLAGS="`echo $CXXFLAGS | sed -e 's/-g//g'` -O3 -OPT:Olimit=0 -IPA"
  CXXFLAGS="-LANG:std $CXXFLAGS"
  LDFLAGS="$LDFLAGS -O3 -OPT:Olimit=0 -IPA"
fi

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
echo Mac OS X GUI ........................... : $WANT_MACOSX_GUI
echo Mac OS X Sound ......................... : $WANT_MACOSX_SOUND
echo SDL support ............................ : $SDL_SUPPORT
echo SDL major-version ...................... : $WANT_SDL_VERSION_MAJOR
echo BINCUE support ......................... : $have_bincue
echo LIBVHD support ......................... : $have_libvhd
echo XFree86 DGA support .................... : $WANT_XF86_DGA
echo XFree86 VidMode support ................ : $WANT_XF86_VIDMODE
echo fbdev DGA support ...................... : $WANT_FBDEV_DGA
echo Enable video on SEGV signals ........... : $WANT_VOSF
echo ESD sound support ...................... : $WANT_ESD
echo GTK user interface ..................... : $WANT_GTK
echo mon debugger support ................... : $WANT_MON
echo Running m68k code natively ............. : $WANT_NATIVE_M68K
echo Use JIT compiler ....................... : $WANT_JIT
echo JIT debug mode ......................... : $WANT_JIT_DEBUG
echo Floating-Point emulation core .......... : $FPE_CORE
echo Assembly optimizations ................. : $ASM_OPTIMIZATIONS
echo Addressing mode ........................ : $ADDRESSING_MODE
echo Bad memory access recovery type ........ : $sigsegv_recovery
echo
echo "Configuration done. Now type \"make\" (or \"gmake\")."
