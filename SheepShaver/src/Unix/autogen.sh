#! /bin/sh
# Run this to generate all the initial makefiles, etc.
# This was lifted from the Gimp, and adapted slightly by
# Christian Bauer.

DIE=0

PROG=SheepShaver

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $PROG."
        echo "Download the appropriate package for your distribution,"
        echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
        DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

if test -z "$*"; then
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

for dir in .
do 
  echo processing $dir
  (cd $dir; \
  aclocalinclude="$ACLOCAL_FLAGS"; \
  aclocal $aclocalinclude; \
  autoheader; autoconf)
done

if [ x"$NO_CONFIGURE" = "x" ]; then
    echo " + Running 'configure $@':"
    if [ -z "$*" ]; then
        echo "   ** If you wish to pass arguments to ./configure, please"
        echo "   ** specify them on the command line."
    fi
    ./configure "$@"
fi
