dnl Process this file with autoconf to produce a configure script.
dnl Written in 2000 by Christian Bauer

AC_INIT
AC_CONFIG_SRCDIR(src/mon.cpp)
AC_PREREQ(2.50)

dnl This defines PACKAGE and VERSION.
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([cxmon], [3.2])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h readline.h history.h readline/readline.h readline/history.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(long long, 8)
AC_CHECK_SIZEOF(void *, 4)

dnl Checks for libraries.
AC_CHECK_LIB(ncurses, tgetent, ,
  AC_CHECK_LIB(termcap, tgetent, ,
    AC_CHECK_LIB(termlib, tgetent, ,
      AC_CHECK_LIB(terminfo, tgetent, ,
        AC_CHECK_LIB(Hcurses, tgetent, ,
          AC_CHECK_LIB(curses, tgetent))))))
AC_CHECK_LIB(readline, readline)

dnl Generate Makefile.
AC_OUTPUT([
Makefile
cxmon.spec
src/Makefile
src/disass/Makefile
])

dnl Print summary.
echo "Configuration done. Now type \"make\"."
