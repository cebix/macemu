#!/bin/bash -e

#
# osx_generate_files.sh
#
# Generates files for 68k emulation, via UAE's virtual cpu, for use on Mac OS X hosts
#

SCRIPT_DIRNAME="$(cd $(dirname $0) && pwd)"
cd "$SCRIPT_DIRNAME"

echo "build68k: compiling"
clang build68k.c -o build68k -I ../MacOSX/ -I ../Unix/

echo "build68k: running"
cat table68k | ./build68k > ./defs68k.c

echo "gencpu: compiling"
clang gencpu.c ./defs68k.c readcpu.cpp -o gencpu -I . -I ../MacOSX/ -I ../Unix/

echo "gencpu: running"
./gencpu
