#!/bin/sh
#
# $Id$
#
# Run this to generate all the initial makefiles, etc.

#
# Note that we actually don't need a config.guess
# We could instead do something like:
#	./configure --build=powerpc-apple-darwin6.1
#
if [ -e /usr/libexec/config.guess ]
then
	#
	# MacOS X 10.1
	#
	ln -sf /usr/libexec/config.guess .
	ln -sf /usr/libexec/config.sub   .
else
	#
	# MacOS X 10.2 (and later?)
	#
	ln -sf /usr/share/libtool/config.guess .
	ln -sf /usr/share/libtool/config.sub .
fi

ln -sf ../Unix/sys_unix.cpp .
ln -sf ../Unix/timer_unix.cpp .
ln -sf ../Unix/user_strings_unix.h .
ln -sf ../Unix/install-sh .
ln -sf ../../README README.txt

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

if test -z "$*"; then
   echo "*************************************************"
   echo "I am going to run ./configure with no arguments -"
   echo "  if you wish to pass any to it, please specify"
   echo "  them on the $0 command line."
   echo "*************************************************"
fi

./configure "$@"
