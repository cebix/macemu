dnl Additional macros for Basilisk II


dnl Check for libgnomeui
dnl B2_PATH_GNOMEUI([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test to see if libgnomeui is installed, and define GNOMEUI_CFLAGS, LIBS
AC_DEFUN([B2_PATH_GNOMEUI],
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
