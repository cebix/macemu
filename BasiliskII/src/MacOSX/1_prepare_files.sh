#!/bin/sh
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
	ln -s /usr/libexec/config.guess .
	ln -s /usr/libexec/config.sub   .
else
	#
	# MacOS X 10.2 (and later?)
	#
	ln -s /usr/share/libtool/config.guess .
	ln -s /usr/share/libtool/config.sub .
fi

ln -s ../Unix/user_strings_unix.h .
ln -s ../Unix/acconfig.h .
ln -s ../Unix/install-sh .
ln -s ../../README README.txt

autoconf

if test -z "$*"; then
   echo "I am going to run ./configure with no arguments - if you wish to pass"
   echo " any to it, please specify them on the $0 command line."
fi

./configure "$@"
