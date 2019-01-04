dnl Process this file with autoconf to produce a configure script.
dnl Written by Christian Bauer

AC_PREREQ(2.69)
AC_INIT([cxmon], [3.2], [cb@cebix.net], [cxmon])
AC_CONFIG_SRCDIR([src/mon.cpp])
AM_INIT_AUTOMAKE([1.12 foreign])

AM_CONFIG_HEADER(config.h)

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
AC_SEARCH_LIBS([tgetent], [ncurses termcap termlib terminfo Hcurses curses], [], [
  AC_MSG_ERROR([unable to find the tgetent() function])
])
AC_SEARCH_LIBS([readline], [readline], [
  AC_DEFINE([HAVE_LIBREADLINE], 1, [Define if you have the Readline library])], [
  AC_MSG_ERROR([unable to find the readline() function])
])

dnl Generate Makefile.
AC_OUTPUT([
Makefile
cxmon.spec
src/Makefile
src/disass/Makefile
])

dnl Print summary.
echo "Configuration done. Now type \"make\"."
