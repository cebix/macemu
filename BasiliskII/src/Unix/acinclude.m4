dnl Additional macros for Basilisk II


dnl Check for libgnomeui
dnl B2_PATH_GNOMEUI([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test to see if libgnomeui is installed, and define GNOMEUI_CFLAGS, LIBS
AC_DEFUN(B2_PATH_GNOMEUI,
[dnl
dnl Get the cflags and libraries from the gnome-config script
dnl
AC_ARG_WITH(gnome-config,
[  --with-gnome-config=GNOME_CONFIG  Location of gnome-config],
GNOME_CONFIG="$withval")

AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
AC_MSG_CHECKING(for libgnomeui)
if test "$GNOME_CONFIG" = "no"; then
  AC_MSG_RESULT(no)
  ifelse([$2], , :, [$2])
else
  AC_MSG_RESULT(yes)
  GNOMEUI_CFLAGS=`$GNOME_CONFIG --cflags gnomeui`
  GNOMEUI_LIBS=`$GNOME_CONFIG --libs gnomeui`
  ifelse([$1], , :, [$1])
fi  
AC_SUBST(GNOMEUI_CFLAGS)
AC_SUBST(GNOMEUI_LIBS)  
])


dnl Check for socklen_t type

AC_DEFUN(TYPE_SOCKLEN_T,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(ac_cv_type_socklen_t,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])socklen_t[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#include <sys/socket.h>], ac_cv_type_socklen_t=yes, ac_cv_type_socklen_t=no)])dnl
AC_MSG_RESULT($ac_cv_type_socklen_t)
if test $ac_cv_type_socklen_t = no; then
  AC_DEFINE(socklen_t, int)
fi
])
