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

ln -sf ../Unix/user_strings_unix.h .
ln -sf ../Unix/install-sh .
ln -sf ../../README README.txt

autoconf

autoheader

if test -z "$*"; then
   echo "*************************************************"
   echo "I am going to run ./configure with no arguments -"
   echo "  if you wish to pass any to it, please specify"
   echo "  them on the $0 command line."
   echo "*************************************************"
fi

./configure "$@"
