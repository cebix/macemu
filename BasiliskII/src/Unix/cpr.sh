#!/bin/sh
# A script to copy recursively ignoring detritus.
# I based this off of a script I had that copied over ssh.
# source can be a file or directory.
# Mike Sliczniak 2009

# Don't copy resource forks or extended attributes on Mac OS X 10.4.
COPY_EXTENDED_ATTRIBUTES_DISABLE=true; export COPY_EXTENDED_ATTRIBUTES_DISABLE

# Don't copy resource forks or extended attributes on Mac OS X 10.5.
COPYFILE_DISABLE=true; export COPYFILE_DISABLE

case $# in
  2)
	;;
  *)
	echo "Usage: cpr source destdir" >&2
	exit 2
	;;
esac

# dir and base names of the source
d=`dirname "$1"` || exit
b=`basename "$1"` || exit

# handle relative and absolute destination dirs
case "$2" in
  /*)
	p=$2
	;;
  *)
	p="$PWD"/"$2"
	;;
esac

# cd into the source dir
cd "$d" || exit

# This is only for Mac OS X, but some systems do not have gtar, find
# sometimes lacks -f, and other systems use test -a.

# List all interesting files for tar to copy:
# The first clause skips directories used for revision control.
# The second clause ignores detritus files from revision control and OSs.
# The third clause ignores ._ style files created by Mac OS X on file systems
# that do not have native resource forks or extended attributes. It checks to
# see that the file it is associated with exists.
find -f "$b" \( \! \( -type d \( \
  -name CVS -o -name RCS -o -name SCCS -o -name .git -o -name .svn \
\) -prune \) \) \
\
\( \! \( -type f \( \
  -name .DS_Store -o -name Thumbs.db -o -name .cvsignore -o -name .gitignore \
\) \) \) \
\
\( \! \( \
  -type f -name '._*' -execdir /bin/sh -c \
    'f=`echo "$1" | sed "s:^\._:./:"`; [ -e "$f" ]' /bin/sh '{}' \; \
\) \) -print0 | tar -c -f - --null -T - --no-recursion | tar -x -C "$p" -f -
