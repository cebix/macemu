#! /bin/sh
aclocal
autoheader
autoconf
./configure $*
