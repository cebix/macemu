#!/bin/sh
#
# $Id$
#
# Run this to generate all the initial makefiles, etc.

ln -sf ../Unix/config.guess .
ln -sf ../Unix/config.sub   .
ln -sf ../Unix/semaphore.h  .
cp -pf ../Unix/sys_unix.cpp .
cp -pf ../Unix/timer_unix.cpp .
ln -sf ../Unix/user_strings_unix.h .
ln -sf ../Unix/install-sh .
ln -sf ../Unix/Darwin .
cp -pr ../../INSTALL INSTALL.txt
cp -pr ../../README  README.txt

#
# This is how I generated the button images:
#
#T=/System/Library/CoreServices/loginwindow.app/Resources
#tiffutil -cat $T/resetH.tif	-out English.lproj/MainMenu.nib/resetH.tiff
#tiffutil -cat $T/resetN.tif	-out English.lproj/MainMenu.nib/resetN.tiff
#tiffutil -cat $T/shutdownH.tif	-out English.lproj/MainMenu.nib/shutdownH.tiff
#tiffutil -cat $T/shutdownN.tif	-out English.lproj/MainMenu.nib/shutdownN.tiff
#unset T

#
# Generate ./configure from configure.in
#
autoconf

#
# Generate config.h.in from configure.in
#
autoheader

#
# Build app which configure uses:
#


if test -z "$*"; then
   echo "*************************************************"
   echo "I am going to run ./configure with no arguments -"
   echo "  if you wish to pass any to it, please specify"
   echo "  them on the $0 command line."
   echo "*************************************************"
fi

# This mode isn't working yet - segfaults
#./configure "$@" --enable-addressing=real

./configure "$@"
