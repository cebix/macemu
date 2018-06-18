dnl Process this file with autoconf to produce a configure script.
dnl Written in 2002 by Christian Bauer

AC_INIT([SheepShaver], 2.4, [Christian.Bauer@uni-mainz.de], SheepShaver)
AC_CONFIG_SRCDIR(main_unix.cpp)
AC_PREREQ(2.52)
AC_CONFIG_HEADER(config.h)
AH_TOP(
#ifndef CONFIG_H
#define CONFIG_H
)
AH_BOTTOM(
#endif /* CONFIG_H */
)

dnl Canonical system information.
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

dnl Some systems do not put corefiles in the currect directory, avoid saving
dnl cores for the configure tests since some are intended to dump core.
ulimit -c 0

dnl Invite Cygwin users to build within the Windows/ directory
case $target_os in
*cygwin* | *mingw32*)
  AC_MSG_ERROR([You can only build the Windows version from its directory, Cygwin/X11 is not supported.])
  ;;
esac

dnl Options.
AC_ARG_ENABLE(jit,          [  --enable-jit            enable JIT compiler [default=yes]], [WANT_JIT=$enableval], [WANT_JIT=yes])
AC_ARG_ENABLE(ppc-emulator, [  --enable-ppc-emulator   use the selected PowerPC emulator [default=auto]], [WANT_EMULATED_PPC=$enableval], [WANT_EMULATED_PPC=auto])
AC_ARG_ENABLE(fbdev-dga,    [  --enable-fbdev-dga      use direct frame buffer access via /dev/fb0 [default=yes]], [WANT_FBDEV_DGA=$enableval], [WANT_FBDEV_DGA=yes])
AC_ARG_ENABLE(xf86-dga,     [  --enable-xf86-dga       use the XFree86 DGA extension [default=yes]], [WANT_XF86_DGA=$enableval], [WANT_XF86_DGA=yes])
AC_ARG_ENABLE(xf86-vidmode, [  --enable-xf86-vidmode   use the XFree86 VidMode extension [default=yes]], [WANT_XF86_VIDMODE=$enableval], [WANT_XF86_VIDMODE=yes])
AC_ARG_ENABLE(vosf,         [  --enable-vosf           enable video on SEGV signals [default=yes]], [WANT_VOSF=$enableval], [WANT_VOSF=yes])
AC_ARG_ENABLE(standalone-gui,[  --enable-standalone-gui enable a standalone GUI prefs editor [default=no]], [WANT_STANDALONE_GUI=$enableval], [WANT_STANDALONE_GUI=no])
AC_ARG_WITH(esd,            [  --with-esd              support ESD for sound under Linux/FreeBSD [default=yes]], [WANT_ESD=$withval], [WANT_ESD=yes])
AC_ARG_WITH(gtk,            [  --with-gtk              use GTK user interface [default=yes]],
  [case "$withval" in
   gtk1)      WANT_GTK="gtk";;
   gtk|gtk2)  WANT_GTK="$withval";;
   yes)       WANT_GTK="gtk2 gtk";;
   *)         WANT_GTK="no";;
   esac],
  [WANT_GTK="gtk2 gtk"])
AC_ARG_WITH(mon,            [  --with-mon              use mon as debugger [default=yes]], [WANT_MON=$withval], [WANT_MON=yes])
AC_ARG_WITH(dgcc,           [  --with-dgcc=COMPILER    use C++ COMPILER to compile synthetic opcodes or 'precompiled'], [DYNGEN_CC=$withval])

AC_ARG_WITH(bincue,   
  AS_HELP_STRING([--with-bincue], [Allow cdrom image files in bin/cue mode]))

AC_ARG_WITH(libvhd,   
  AS_HELP_STRING([--with-libvhd], [Enable VHD disk images]))


dnl Addressing mode
AC_ARG_ENABLE(addressing,
  [  --enable-addressing=AM  set the addressing mode to use [default=real]],
  [case "$enableval" in
   real)        WANT_ADDRESSING_MODE="real";;
   direct)      WANT_ADDRESSING_MODE="direct";;
   direct,0x*)  WANT_ADDRESSING_MODE="direct"; NATMEM_OFFSET=`echo "$enableval" | sed -n '/direct,\(0[[xX]][[0-9A-Fa-f]]*\([[UuLl]]\{1,2\}\)\?\)$/s//\1/p'`;;
   esac],
  [WANT_ADDRESSING_MODE="real"]
)

dnl SDL options.
AC_ARG_ENABLE(sdl-static,   [  --enable-sdl-static     use SDL static libraries for linking [default=no]], [WANT_SDL_STATIC=$enableval], [WANT_SDL_STATIC=no])
AC_ARG_ENABLE(sdl-video,    [  --enable-sdl-video      use SDL for video graphics [default=no]], [WANT_SDL_VIDEO=$enableval], [WANT_SDL_VIDEO=no])
AC_ARG_ENABLE(sdl-audio,    [  --enable-sdl-audio      use SDL for audio [default=no]], [WANT_SDL_AUDIO=$enableval], [WANT_SDL_AUDIO=no])
AC_ARG_ENABLE(sdl-framework,    [  --enable-sdl-framework  use SDL framework [default=no]], [WANT_SDL_FRAMEWORK=$enableval], [WANT_SDL_FRAMEWORK=no])
AC_ARG_ENABLE(sdl-framework-prefix,    [  --enable-sdl-framework-prefix=PFX    default=/Library/Frameworks], [SDL_FRAMEWORK="$enableval"], [SDL_FRAMEWORK=/Library/Frameworks])
AC_ARG_WITH(sdl1,            [  --with-sdl1             use SDL 1.x, rather than SDL 2.x [default=no]], [WANT_SDL_VERSION_MAJOR=1], [])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_EGREP
AC_PATH_PROGS(FILE, [file false])
AC_PATH_PROG(PERL, [perl])

dnl Check for PowerPC target CPU.
HAVE_PPC=no
AC_MSG_CHECKING(for PowerPC target CPU)
AC_EGREP_CPP(yes,
[
#ifdef __powerpc__
  yes
#endif
#ifdef __ppc__
  yes
#endif
], [AC_MSG_RESULT(yes); HAVE_PPC=yes], AC_MSG_RESULT(no))

dnl We use native CPU if possible.
EMULATED_PPC=yes
case $WANT_EMULATED_PPC in
  auto) [[ "x$HAVE_PPC" = "xyes" ]] && EMULATED_PPC=no;;
  no)   EMULATED_PPC=no;;
esac
if [[ "x$EMULATED_PPC" = "xyes" ]]; then
  AC_DEFINE(EMULATED_PPC, 1, [Define if using a PowerPC CPU emulator.])
fi

dnl We use mon if possible.
MONSRCS=
case "x$WANT_MON" in
x/* | x.*)
  mon_srcdir=$WANT_MON
  WANT_MON=yes
  ;;
xyes)
  mon_srcdir=../../../mon/src
  ;;
esac
if [[ "x$WANT_MON" = "xyes" ]]; then
  AC_MSG_CHECKING(for mon)
  if grep mon_init $mon_srcdir/mon.h >/dev/null 2>/dev/null; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(ENABLE_MON, 1, [Define if using "mon".])
    MONSRCS="$mon_srcdir/mon.cpp $mon_srcdir/mon_6502.cpp $mon_srcdir/mon_z80.cpp $mon_srcdir/mon_cmd.cpp $mon_srcdir/mon_lowmem.cpp $mon_srcdir/mon_disass.cpp $mon_srcdir/mon_ppc.cpp $mon_srcdir/disass/floatformat.c $mon_srcdir/disass/i386-dis.c $mon_srcdir/disass/m68k-dis.c $mon_srcdir/disass/m68k-opc.c $mon_srcdir/disass/mips-dis.c $mon_srcdir/disass/mips-opc.c $mon_srcdir/disass/mips16-opc.c"
    CXXFLAGS="$CXXFLAGS -I$mon_srcdir -I$mon_srcdir/disass"
    AC_CHECK_LIB(ncurses, tgetent, ,
      [AC_CHECK_LIB(termcap, tgetent, ,
        [AC_CHECK_LIB(termlib, tgetent, ,
          [AC_CHECK_LIB(terminfo, tgetent, ,
            [AC_CHECK_LIB(Hcurses, tgetent, ,
              [AC_CHECK_LIB(curses, tgetent)])])])])])
    AC_CHECK_LIB(readline, readline)
    AC_CHECK_HEADERS(readline.h history.h readline/readline.h readline/history.h)
  else
    AC_MSG_RESULT(no)
    AC_MSG_WARN([Could not find mon, ignoring --with-mon.])
    WANT_MON=no
  fi
fi
AC_SUBST(MONSRCS)

dnl Checks for libraries.
AC_CHECK_LIB(posix4, sem_init)
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
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x2" ]]; then
      AC_PATH_PROG(sdl2_config, "sdl2-config")
      if [[ -n "$sdl2_config" ]]; then
        sdl2_cflags=`$sdl2_config --cflags`
        if [[ "x$WANT_SDL_STATIC" = "xyes" ]]; then
          sdl2_libs=`$sdl2_config --static-libs`
        else
          sdl2_libs=`$sdl2_config --libs`
        fi
        CFLAGS="$CFLAGS $sdl2_cflags"
        CXXFLAGS="$CXXFLAGS $sdl2_cflags"
        LIBS="$LIBS $sdl2_libs"
        WANT_SDL_VERSION_MAJOR=2
      else
        TEMP_WANT_SDL_VERSION_MAJOR=1
      fi
    fi
    if [[ "x$TEMP_WANT_SDL_VERSION_MAJOR" = "x1" ]]; then
      AC_PATH_PROG(sdl_config, "sdl-config")
      if [[ -n "$sdl_config" ]]; then
        sdl_cflags=`$sdl_config --cflags`
        if [[ "x$WANT_SDL_STATIC" = "xyes" ]]; then
          sdl_libs=`$sdl_config --static-libs`
        else
          sdl_libs=`$sdl_config --libs`
        fi
        CFLAGS="$CFLAGS $sdl_cflags"
        CXXFLAGS="$CXXFLAGS $sdl_cflags"
        LIBS="$LIBS $sdl_libs"
        WANT_SDL_VERSION_MAJOR=1
      else
        WANT_SDL=no
        WANT_SDL_VIDEO=no
        WANT_SDL_AUDIO=no
      fi
    fi
  fi
  SDL_SUPPORT=`echo "$SDL_SUPPORT" | sed -e "s/^ //"`
else
  SDL_SUPPORT="none"
fi

dnl We need X11, if not using SDL.
if [[ "x$WANT_SDL_VIDEO" != "xyes" ]]; then
  AC_PATH_XTRA
  if [[ "x$no_x" = "xyes" ]]; then
    AC_MSG_ERROR([You need X11 to run SheepShaver.])
  fi
  CFLAGS="$CFLAGS $X_CFLAGS"
  CXXFLAGS="$CXXFLAGS $X_CFLAGS"
  LIBS="$LIBS $X_PRE_LIBS $X_LIBS -lX11 -lXext $X_EXTRA_LIBS"
fi



dnl We need pthreads on non-PowerPC systems. Try libpthread first, then libc_r (FreeBSD), then PTL.
HAVE_PTHREADS=yes
case $EMULATED_PPC:$target_os in
no:linux*|no:netbsd*)
  dnl We do have our own pthread_cancel() implementation
  AC_DEFINE(HAVE_PTHREAD_CANCEL, 1, [Define if you have the pthread_cancel function.])
  ;;
*:*)
  AC_CHECK_LIB(pthread, pthread_create, , [
    AC_CHECK_LIB(c_r, pthread_create, , [
      AC_CHECK_LIB(PTL, pthread_create, , [
        dnl XXX remove when no pthreads case is merged
        AC_MSG_ERROR([You need pthreads to run SheepShaver.])
        HAVE_PTHREADS=no
      ])
    ])
  ])
  AC_CHECK_FUNCS(pthread_cancel)
  AC_CHECK_FUNCS(pthread_cond_init pthread_testcancel)
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
  ;;
esac
if [[ "x$HAVE_PTHREADS" = "xyes" ]]; then
  AC_DEFINE(HAVE_PTHREADS, 1, [Define if pthreads are available.])
fi

dnl We use FBDev DGA if possible.
if [[ "x$WANT_FBDEV_DGA" = "xyes" ]]; then
  AC_CHECK_HEADER(linux/fb.h, [
    AC_DEFINE(ENABLE_FBDEV_DGA, 1, [Define if using Linux fbdev extension.])
  ], [
    AC_MSG_WARN([Could not find Linux FBDev extension, ignoring --enable-fbdev-dga.])
    WANT_FBDEV_DGA=no
  ])
fi

dnl We use XFree86 DGA if possible.
if [[ "x$WANT_XF86_DGA" = "xyes" ]]; then
  AC_CHECK_LIB(Xxf86dga, XF86DGAQueryExtension, [
    AC_DEFINE(ENABLE_XF86_DGA, 1, [Define if using XFree86 DGA extension.])
    LIBS="$LIBS -lXxf86dga"
  ], [
    AC_MSG_WARN([Could not find XFree86 DGA extension, ignoring --enable-xf86-dga.])
    WANT_XF86_DGA=no
  ])
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
  WANT_GTK=no
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
  WANT_GTK=no
  AM_PATH_GTK(1.2.0, [
    GUI_CFLAGS="$GTK_CFLAGS"
    GUI_LIBS="$GTK_LIBS"
    WANT_GTK=gtk
  ], [
    AC_MSG_WARN([Could not find GTK+, disabling user interface.])
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
  WANT_ESD=no
  AM_PATH_ESD(0.2.8, [
    AC_DEFINE(ENABLE_ESD, 1, [Define is using ESD.])
    CFLAGS="$CFLAGS $ESD_CFLAGS"
    CXXFLAGS="$CXXFLAGS $ESD_CFLAGS"
    LIBS="$LIBS $ESD_LIBS"
    WANT_ESD=yes
  ], [
    AC_MSG_WARN([Could not find ESD, disabling ESD support.])
  ])
fi

dnl We use 64-bit file size support if possible.
AC_SYS_LARGEFILE

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(malloc.h stdint.h)
AC_CHECK_HEADERS(mach/vm_map.h mach/mach_init.h sys/mman.h)
AC_CHECK_HEADERS(unistd.h fcntl.h byteswap.h dirent.h)
AC_CHECK_HEADERS(sys/socket.h sys/ioctl.h sys/filio.h sys/bitypes.h sys/wait.h)
AC_CHECK_HEADERS(sys/time.h sys/poll.h sys/select.h arpa/inet.h)
AC_CHECK_HEADERS(netinet/in.h linux/if.h linux/if_tun.h net/if.h net/if_tun.h, [], [], [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])
AC_CHECK_HEADERS(AvailabilityMacros.h)
AC_CHECK_HEADERS(IOKit/storage/IOBlockStorageDevice.h)
AC_CHECK_HEADERS(fenv.h)
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
AC_CHECK_SIZEOF(void *, 4)
AC_TYPE_OFF_T
AC_CHECK_TYPE(loff_t, off_t)
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

dnl Check whether struct sigaction has sa_restorer member.
AC_CACHE_CHECK([whether struct sigaction has sa_restorer],
  ac_cv_signal_sa_restorer, [
  AC_TRY_COMPILE([
    #include <signal.h>
  ], [struct sigaction sa; sa.sa_restorer = 0;],
  ac_cv_signal_sa_restorer=yes, ac_cv_signal_sa_restorer=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_signal_sa_restorer=no
  )
])
if [[ "x$ac_cv_signal_sa_restorer" = "xyes" ]]; then
  AC_DEFINE(HAVE_SIGNAL_SA_RESTORER, 1, [Define if sa_restorer is available in struct sigaction.])
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup strerror strlcpy cfmakeraw)
AC_CHECK_FUNCS(nanosleep)
AC_CHECK_FUNCS(sigaction signal)
AC_CHECK_FUNCS(mmap mprotect munmap)
AC_CHECK_FUNCS(vm_allocate vm_deallocate vm_protect)
AC_CHECK_FUNCS(exp2f log2f exp2 log2)
AC_CHECK_FUNCS(floorf roundf ceilf truncf floor round ceil trunc)
AC_CHECK_FUNCS(poll inet_aton)

dnl Darwin seems to define mach_task_self() instead of task_self().
AC_CHECK_FUNCS(mach_task_self task_self)

dnl We need clock_gettime() for better performance but it may drag
dnl libpthread in, which we don't want for native ppc mode
case $EMULATED_PPC:$target_os in
no:linux*)
  ;;
*:*)
  AC_SEARCH_LIBS(clock_gettime, [rt posix4])
  AC_CHECK_FUNCS(clock_gettime clock_nanosleep)
  ;;
esac

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
	;;
*-*-freebsd*)
	no_dev_ptmx=1
	;;
esac

if test -z "$no_dev_ptmx" ; then
	if test "x$disable_ptmx_check" != "xyes" ; then
		AC_CHECK_FILE([/dev/ptmx],
			[
				AC_DEFINE_UNQUOTED(HAVE_DEV_PTMX, 1, [Define if you have /dev/ptmx.])
				have_dev_ptmx=1
			]
		)
	fi
fi
AC_CHECK_FILE([/dev/ptc],
	[
		AC_DEFINE_UNQUOTED(HAVE_DEV_PTS_AND_PTC, 1, [Define if you have /dev/ptc.])
		have_dev_ptc=1
	]
)
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
AC_CHECK_FRAMEWORK(Carbon, [#include <Carbon/Carbon.h>])
AC_CHECK_FRAMEWORK(IOKit, [#include <IOKit/IOKitLib.h>])
AC_CHECK_FRAMEWORK(CoreFoundation, [#include <CoreFoundation/CoreFoundation.h>])
AC_CHECK_FRAMEWORK(CoreAudio, [#include <CoreAudio/CoreAudio.h>])
AC_CHECK_FRAMEWORK(AudioUnit, [#include <AudioUnit/AudioUnit.h>])
AC_CHECK_FRAMEWORK(AudioToolbox, [#include <AudioToolbox/AudioToolbox.h>])
AC_CHECK_FRAMEWORK(AppKit, [])

dnl Select system-dependant sources.
SERIALSRC=serial_unix.cpp
ETHERSRC=../dummy/ether_dummy.cpp
SCSISRC=../dummy/scsi_dummy.cpp
AUDIOSRC=../dummy/audio_dummy.cpp
PREFSSRC=../dummy/prefs_dummy.cpp
EXTFSSRC=extfs_unix.cpp
EXTRASYSSRCS=
case "$target_os" in
linux*)
  ETHERSRC=ether_unix.cpp
  AUDIOSRC=audio_oss_esd.cpp
  SCSISRC=Linux/scsi_linux.cpp
  if [[ "x$EMULATED_PPC" = "xno" ]]; then
    EXTRASYSSRCS="paranoia.cpp Linux/sheepthreads.c ppc_asm.S"
  fi
  ;;
freebsd*)
  ETHERSRC=ether_unix.cpp
  ;;
netbsd*)
  ETHERSRC=ether_unix.cpp
  if [[ "x$EMULATED_PPC" = "xno" ]]; then
    EXTRASYSSRCS="paranoia.cpp NetBSD/sheepthreads.c ppc_asm.S"
  fi
  ;;
darwin*)
  ETHERSRC=ether_unix.cpp
  if [[ "x$EMULATED_PPC" = "xno" ]]; then
    EXTRASYSSRCS="paranoia.cpp ppc_asm.S"
  fi
  if [[ "x$ac_cv_framework_IOKit" = "xyes" -a "x$ac_cv_framework_CoreFoundation" = "xyes" ]]; then
    EXTRASYSSRCS="$EXTRASYSSRCS ../MacOSX/sys_darwin.cpp"
  fi
  if [[ "x$ac_cv_framework_Carbon" = "xyes" ]]; then
    EXTFSSRC=../MacOSX/extfs_macosx.cpp
    if [[ "x$ac_cv_framework_AppKit" = "xyes" -a "x$WANT_GTK" = "xno" ]]; then
      PREFSSRC="../MacOSX/prefs_macosx.mm ../MacOSX/Launcher/VMSettingsController.mm ../MacOSX/Launcher/DiskType.m"
      CPPFLAGS="$CPPFLAGS -I../MacOSX/Launcher"
    fi
  fi
  if [[ "x$WANT_ESD" = "xno" -a "x$ac_cv_framework_CoreAudio" = "xyes" -a "x$WANT_SDL_AUDIO" = "xno" ]]; then
    AUDIOSRC="../MacOSX/audio_macosx.cpp ../MacOSX/AudioBackEnd.cpp ../MacOSX/AudioDevice.cpp ../MacOSX/MacOSX_sound_if.cpp"
    OSX_CORE_AUDIO="-DOSX_CORE_AUDIO"
  fi
  ;;
irix*)
  AUDIOSRC=Irix/audio_irix.cpp
  LIBS="$LIBS -laudio"
  WANT_ESD=no

  dnl Check if our compiler supports -IPA (MIPSPro)
  HAVE_IPA=no
  ocflags="$CFLAGS"
  CFLAGS=`echo " $CFLAGS -IPA" | sed -e "s/ -g //g"`
  AC_MSG_CHECKING(if "-IPA" works)
  dnl Do a test compile of an empty function
  AC_TRY_COMPILE([#if defined __GNUC__
                  # error GCC does not support IPA yet
                  #endif],, [AC_MSG_RESULT(yes); HAVE_IPA=yes], AC_MSG_RESULT(no))
  CFLAGS="$ocflags"
  ;;
esac

dnl BINCUE
AS_IF([test  "x$with_bincue" = "xyes" ], [have_bincue=yes], [have_bincue=no])
AS_IF([test  "x$have_bincue" = "xyes" ], [
   if [[ "xOSX_CORE_AUDIO" = "xno"  -a "x$WANT_SDL_AUDIO"="xno" ]]; then
       AC_MSG_ERROR([You need SDL or OSX Core Audio to use BINCUE support.])
       AC_SUBST(USE_BINCUE, no)
   else
       CPPFLAGS="$CPPFLAGS -DBINCUE $OSX_CORE_AUDIO"
       AC_SUBST(USE_BINCUE, yes)
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

dnl SDL overrides
if [[ "x$WANT_SDL" = "xyes" ]]; then
  AC_DEFINE(USE_SDL, 1, [Define to enble SDL support.])
fi
if [[ "x$WANT_SDL_VIDEO" = "xyes" ]]; then
  AC_DEFINE(USE_SDL_VIDEO, 1, [Define to enable SDL video graphics support.])
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
    EXTRASYSSRCS="$EXTRASYSSRCS ../dummy/clip_dummy.cpp"
  fi
else
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


SYSSRCS="$VIDEOSRCS $EXTFSSRC $PREFSSRC $SERIALSRC $ETHERSRC $SCSISRC $AUDIOSRC $SEMSRC $UISRCS $EXTRASYSSRCS"

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
	#if defined(HAVE_NETINET_IN_H)
	#include <netinet/in.h>
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

dnl Check if we can disable position-independent code
AC_CACHE_CHECK([how to disable position-independent code],
  ac_cv_no_pie, [
  ac_cv_no_pie='-Wl,-no_pie'
  saved_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $ac_cv_no_pie"
  AC_TRY_LINK(,,,[ac_cv_no_pie="cannot"])
  if [[ "$ac_cv_no_pie" = "cannot" ]]; then
    LDFLAGS="$saved_LDFLAGS"
  fi
])

dnl Check if we can modify the __PAGEZERO segment for use as Low Memory
AC_CACHE_CHECK([whether __PAGEZERO can be Low Memory area 0x0000-0x3000],
  ac_cv_pagezero_hack, [
  ac_cv_pagezero_hack=no 
  if AC_TRY_COMMAND([Darwin/testlmem.sh 0x3000]); then
    ac_cv_pagezero_hack=yes
    dnl might as well skip the test for mmap-able low memory
    ac_cv_can_map_lm=no
  fi
])
AC_TRANSLATE_DEFINE(PAGEZERO_HACK, "$ac_cv_pagezero_hack",
  [Define if the __PAGEZERO Mach-O Low Memory Globals hack works on this system.])

dnl Check if we can mmap 0x3000 bytes from 0x0000
AC_CACHE_CHECK([whether we can map Low Memory area 0x0000-0x3000],
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
    #define HAVE_MACH_EXCEPTIONS 1
    #define CONFIGURE_TEST_SIGSEGV_RECOVERY
    #include "../CrossPlatform/vm_alloc.cpp"
    #include "../CrossPlatform/sigsegv.cpp"
  ],
  ac_cv_have_mach_exceptions=yes,
  ac_cv_have_mach_exceptions=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_mach_exceptions=no
  )
  AC_LANG_RESTORE
  ]
)
if [[ "$ac_cv_have_mach_exceptions" = "yes" ]]; then
  sigsegv_recovery=mach
fi
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
  sigsegv_recovery=win32
  ac_cv_have_win32_exceptions=yes
  ],
  ac_cv_have_win32_exceptions=no,
  dnl When cross-compiling, do not assume anything.
  ac_cv_have_win32_exceptions=no
  )
  AC_LANG_RESTORE
  ]
)
AC_TRANSLATE_DEFINE(HAVE_WIN32_EXCEPTIONS, "$ac_cv_have_win32_exceptions",
  [Define if your system supports Windows exceptions.])

dnl Otherwise, check if extended signals are supported.
if [[ -z "$sigsegv_recovery" ]]; then
  AC_CACHE_CHECK([whether your system supports extended signal handlers],
    ac_cv_have_extended_signals, [
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_RUN([
      #define HAVE_SIGINFO_T 1
      #define CONFIGURE_TEST_SIGSEGV_RECOVERY
      #include "../CrossPlatform/vm_alloc.cpp"
      #include "../CrossPlatform/sigsegv.cpp"
    ],
    ac_cv_have_extended_signals=yes,
    ac_cv_have_extended_signals=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_extended_signals=no
    )
    AC_LANG_RESTORE
    ]
  )
  if [[ "$ac_cv_have_extended_signals" = "yes" ]]; then
    sigsegv_recovery=siginfo
  fi
  AC_TRANSLATE_DEFINE(HAVE_SIGINFO_T, "$ac_cv_have_extended_signals",
    [Define if your system support extended signals.])
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
    ],
    ac_cv_have_sigcontext_hack=yes,
    ac_cv_have_sigcontext_hack=no,
    dnl When cross-compiling, do not assume anything.
    ac_cv_have_sigcontext_hack=no
    )
    AC_LANG_RESTORE
  ])
  if [[ "$ac_cv_have_sigcontext_hack" = "yes" ]]; then
    sigsegv_recovery=sigcontext
  fi
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

dnl Enable VOSF screen updates with this feature is requested and feasible
if [[ "x$WANT_VOSF" = "xyes" -a "x$CAN_VOSF" = "xyes" ]]; then
    AC_DEFINE(ENABLE_VOSF, 1, [Define if using video enabled on SEGV signals.])
else
    WANT_VOSF=no
fi

dnl Check addressing mode to use
AC_MSG_CHECKING([for addressing mode to use])
if [[ "$EMULATED_PPC" != "yes" ]]; then
  if [[ "$WANT_ADDRESSING_MODE" != "real" ]]; then
    AC_MSG_WARN([Running in native PowerPC mode, force use of Real Addressing.])
    WANT_ADDRESSING_MODE="real"
  fi
fi
if [[ "$WANT_ADDRESSING_MODE" = "direct" ]]; then
  if [[ -n "$NATMEM_OFFSET" ]]; then
    NATMEM_OFFSET_DEF="-DNATMEM_OFFSET=$NATMEM_OFFSET"
  fi
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  cat confdefs.h > conftest.$ac_ext
  cat >> conftest.$ac_ext << EOF
#include <stdio.h>
#include <string.h>
#include "../CrossPlatform/vm_alloc.cpp"

int main(void)
{
	if (vm_init() < 0)
		return 1;

	static const struct {
		unsigned long base;
		unsigned int size;
	} ranges[[]] = {
		{ 0x00000000, 0x0003000 },
		{ 0x10000000, 0x2000000 },
		{ 0x40800000, 0x0400000 },
		{ 0x68070000, 0x0010000 },
		{ 0x69000000, 0x0080000 },
		{ 0x68ffe000, 0x0002000 },
		{ 0x5fffe000, 0x0002000 },
		{ 0x60000000, 0x0040000 },
	};
	const int n_ranges = sizeof(ranges)/ sizeof(ranges[[0]]);

#ifdef NATMEM_OFFSET
	unsigned long ofs = NATMEM_OFFSET;
#else
	for (unsigned long ofs = 0x10000000; ofs <= 0x90000000; ofs += 0x10000000)
#endif
	for (int i = 0; i < n_ranges; i++) {
		char *m = (char *)(ranges[[i]].base + ofs);
		if (vm_acquire_fixed(m, ranges[[i]].size) != 0)
			break;
		memset(m, 0, ranges[[i]].size);
		vm_release(m, ranges[[i]].size);
		if (i == n_ranges - 1) {
			if (sizeof(void *) == 8 && ofs > 0xffffffff)
				printf("0x%lxul\n", ofs);
			else
				printf("0x%08x\n", ofs);
			return 0;
		}
	}

	vm_exit();
	return 1;
}
EOF
  doit='$CXX conftest.$ac_ext -o conftest.$ac_exeext $CXXFLAGS $CPPFLAGS $LDFLAGS $LIBS $NATMEM_OFFSET_DEF >& AS_MESSAGE_LOG_FD'
  if AC_TRY_EVAL(doit); then
    NATMEM_OFFSET=`./conftest.$ac_exeext`
  else
    NATMEM_OFFSET=
  fi
  rm -f conftest*
  AC_LANG_RESTORE

  if [[ -z "$NATMEM_OFFSET" ]]; then
    AC_MSG_ERROR([could not determine a sensible NATMEM_OFFSET value])
  else
    WANT_ADDRESSING_MODE="direct,$NATMEM_OFFSET"
    AC_DEFINE_UNQUOTED(NATMEM_OFFSET, $NATMEM_OFFSET,
      [Define constant offset for Mac address translation])
  fi
fi
AC_MSG_RESULT($WANT_ADDRESSING_MODE)

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

dnl Check for host float format
gcc_AC_C_FLOAT_FORMAT

dnl Platform specific binary postprocessor
AC_PATH_PROG(BLESS, "true")
if [[ "x$ac_cv_pagezero_hack" = "xyes" ]]; then
  BLESS=Darwin/lowmem
  LDFLAGS="$LDFLAGS -pagezero_size 0x3000"
fi

dnl Check for GCC 2.7 or higher.
HAVE_GCC27=no
AC_MSG_CHECKING(for GCC 2.7 or higher)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if ! (__GNUC__ - 1 > 1 || __GNUC_MINOR__ - 1 > 5)
                                     # error gcc < 2.7
                                     typedef syntax error;
                                     #endif
                                   ]])],
                  [AC_MSG_RESULT(yes); HAVE_GCC27=yes],
                  [AC_MSG_RESULT(no)])

dnl Check for GCC 3.0 or higher.
HAVE_GCC30=no
AC_MSG_CHECKING(for GCC 3.0 or higher)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if ! (__GNUC__ >= 3)
                                     # error gcc < 3
                                     typedef syntax error;
                                     #endif
                                   ]])],
                  [AC_MSG_RESULT(yes); HAVE_GCC30=yes],
                  [AC_MSG_RESULT(no)])

dnl Check for GCC 4.0 or higher.
HAVE_GCC40=no
AC_MSG_CHECKING(for GCC 4.0 or higher)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if ! (__GNUC__ >= 4)
                                     # error gcc < 4
                                     typedef syntax error;
                                     #endif
                                   ]])],
                  [AC_MSG_RESULT(yes); HAVE_GCC40=yes],
                  [AC_MSG_RESULT(no)])

dnl Check for ICC.
AC_MSG_CHECKING(for ICC)
HAVE_ICC=no
if $CXX -V -v 2>&1 | grep -q "Intel(R) C++ Compiler"; then
  HAVE_ICC=yes
fi
AC_MSG_RESULT($HAVE_ICC)

dnl Determine the generated object format
AC_CACHE_CHECK([the format of compiler generated objects],
  ac_cv_object_format, [
  echo 'int i;' > conftest.$ac_ext
  ac_cv_object_format=no
  if AC_TRY_EVAL(ac_compile); then
    case `/usr/bin/file conftest.$ac_objext` in
    *"ELF"*)
      ac_cv_object_format=elf
      ;;
    *"Mach-O"*)
      ac_cv_object_format=mach
      ;;
    *)
      ac_cv_object_format=unknown
      ;;
    esac
  fi
  rm -rf conftest*
])

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

dnl CPU emulator sources
if [[ "x$EMULATED_PPC" = "xyes" ]]; then
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
  if [[ "x$WANT_JIT" = "xyes" ]]; then
    ac_cv_use_dyngen_precompiled=no
    AC_CACHE_CHECK([whether dyngen can be used],
      ac_cv_use_dyngen, [
      case $host_cpu:$ac_cv_object_format in
      powerpc:elf)
        ac_cv_use_dyngen=yes
        ;;
      x86_64:elf)
        ac_cv_use_dyngen=yes
        ;;
      i?86:elf)
        ac_cv_use_dyngen=yes
        ;;
      mips:elf)
        ac_cv_use_dyngen=yes
        ;;
      powerpc:mach)
        ac_cv_use_dyngen=yes
        ;;
      x86_64:mach)
        ac_cv_use_dyngen=yes
        ;;
      i?86:mach)
        ac_cv_use_dyngen=yes
        ;;
      *:*)
        ac_cv_use_dyngen=no
        ;;
      esac
      dnl Check for a suitable synthetic opcodes compiler (icc is faking itself as gcc 3.2.2)
      if [[ -z "$DYNGEN_CC" ]]; then
        if [[ "x$HAVE_GCC27" = "xyes" -a "x$HAVE_GCC40" = "xno" -a "x$HAVE_ICC" = "xno" ]]; then
          DYNGEN_CC=$CXX
        elif command -v g++ >/dev/null; then
          vers=`g++ -dumpversion`
          function version { echo "$@" | awk -F. '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }'; }
          if [[ $(version $vers) -ge $(version "2.7.0") ]] && [[ $(version $vers) -lt $(version "4.0.0") ]]; then
            DYNGEN_CC="$gxx"
          fi
        fi
      fi
      if [[ -z "$DYNGEN_CC" -o "x$DYNGEN_CC" = "xprecompiled" ]]; then
        case $host_cpu in
          i?86)
            ac_cv_use_dyngen_precompiled=yes
            ;;
          x86_64)
            ac_cv_use_dyngen_precompiled=yes
            ;;
          *)
            ac_cv_use_dyngen=no
            ;;
        esac
      fi
      if [[ "x$ac_cv_use_dyngen_precompiled" = "xyes" ]]; then DYNGEN_CC=precompiled; fi
    ])
    if [[ "x$ac_cv_use_dyngen" = "xyes" ]]; then
      case $host_cpu in
      i?86)
        DYNGEN_OP_FLAGS="-fomit-frame-pointer"
        ;;
      mips)
        DYNGEN_OP_FLAGS="-fno-delayed-branch -mno-abicalls"
        ;;
      powerpc)
        if [[ "x$ac_cv_object_format" = "xmach" ]]; then
          DYNGEN_OP_FLAGS="-mdynamic-no-pic"
        fi
        ;;
      esac
      have_dyngen_gcc3=no
      if [[ "$DYNGEN_CC" != "precompiled" ]]; then
        case "x`$DYNGEN_CC -dumpversion`" in
        x[12].*) ;;
        x*) have_dyngen_gcc3=yes ;;
        esac
      else
        case "x`$CXX -dumpversion`" in
        x[12].*) ;;
        x*) have_dyngen_gcc3=yes ;;
        esac
      fi
      if [[ "x$have_dyngen_gcc3" = "xyes" ]]; then
        DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -fno-align-functions"
      else
        DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -malign-functions=0"
      fi
      DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -finline-functions -finline-limit=10000 -fno-exceptions -g0"
      if [[ "x$have_dyngen_gcc3" = "xyes" ]]; then
        DYNGEN_OP_FLAGS="$DYNGEN_OP_FLAGS -fno-reorder-blocks -fno-optimize-sibling-calls"
      fi
      if [[ "x$DYNGEN_CC" != "x$CXX" ]]; then
        DYNGEN_CFLAGS="-O2 $CFLAGS"
        DYNGEN_CXXFLAGS="-O2 $CXXFLAGS"
      else
        DYNGEN_CFLAGS="\$(CFLAGS)"
        DYNGEN_CXXFLAGS="\$(CXXFLAGS)"
      fi
    else
      WANT_JIT=no
    fi
    AC_TRANSLATE_DEFINE(ENABLE_DYNGEN, $ac_cv_use_dyngen, [Define to enable dyngen engine])
    if [[ "x$WANT_JIT" = "xyes" ]]; then
      DYNGENSRCS="\
        ../kpx_cpu/src/cpu/jit/dyngen.c \
        ../kpx_cpu/src/cpu/jit/cxxdemangle.cpp"
      CPUSRCS="\
        ../kpx_cpu/src/cpu/jit/jit-cache.cpp \
        ../kpx_cpu/src/cpu/jit/basic-dyngen.cpp \
        ../kpx_cpu/src/cpu/ppc/ppc-dyngen.cpp \
        ../kpx_cpu/src/cpu/ppc/ppc-jit.cpp $CPUSRCS"
    fi
  fi
  CPUSRCS="$CPUSRCS ../kpx_cpu/sheepshaver_glue.cpp ../kpx_cpu/ppc-dis.c"
else
  WANT_JIT=no
fi
if [[ "x$WANT_JIT" = "xyes" ]]; then
  CPPFLAGS="$CPPFLAGS -DUSE_JIT"
fi

dnl Higher level optimizations with MIPSPro compilers are possible
if [[ "x$HAVE_IPA" = "xyes" ]]; then
  CFLAGS="`echo $CFLAGS | sed -e 's/-g//g'` -O3 -OPT:Olimit=0 -IPA"
  CXXFLAGS="`echo $CXXFLAGS | sed -e 's/-g//g'` -O3 -OPT:Olimit=0 -IPA"
  CXXFLAGS="-LANG:std $CXXFLAGS"
  LDFLAGS="$LDFLAGS -O3 -OPT:Olimit=0 -IPA"
fi

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

dnl Generate Makefile.
AC_SUBST(PERL)
AC_SUBST(USE_DYNGEN, [$ac_cv_use_dyngen])
AC_SUBST(USE_DYNGEN_PRECOMPILED, [$ac_cv_use_dyngen_precompiled])
AC_SUBST(DYNGENSRCS)
AC_SUBST(DYNGEN_CC)
AC_SUBST(DYNGEN_CFLAGS)
AC_SUBST(DYNGEN_CXXFLAGS)
AC_SUBST(DYNGEN_OP_FLAGS)
AC_SUBST(SYSSRCS)
AC_SUBST(CPUSRCS)
AC_SUBST(BLESS)
AC_SUBST(KEYCODES)
AC_OUTPUT([
Makefile
../MacOSX/Info.plist
])

dnl Print summary.
echo
echo SheepShaver configuration summary:
echo
echo SDL support ...................... : $SDL_SUPPORT
echo SDL major-version ................ : $WANT_SDL_VERSION_MAJOR
echo BINCUE support ................... : $have_bincue
echo LIBVHD support ................... : $have_libvhd
echo FBDev DGA support ................ : $WANT_FBDEV_DGA
echo XFree86 DGA support .............. : $WANT_XF86_DGA
echo XFree86 VidMode support .......... : $WANT_XF86_VIDMODE
echo Using PowerPC emulator ........... : $EMULATED_PPC
echo Enable JIT compiler .............. : $WANT_JIT
echo Enable video on SEGV signals ..... : $WANT_VOSF
echo ESD sound support ................ : $WANT_ESD
echo GTK user interface ............... : $WANT_GTK
echo mon debugger support ............. : $WANT_MON
echo Addressing mode .................. : $WANT_ADDRESSING_MODE
echo Bad memory access recovery type .. : $sigsegv_recovery
echo
echo "Configuration done. Now type \"make\"."
