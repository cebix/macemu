#!/bin/bash -e

#
# run_build68k_for_xcode.sh
#
# Generates files for 68k emulation, via UAE's virtual cpu, for use on Mac OS X hosts
#

if [ ! -d "$BUILT_PRODUCTS_DIR" ] || [ ! "$PROJECT_DIR" ]; then
	echo "ERROR: $(basename $0) must be run from an Xcode 'External Build System' target"
	exit 1
fi

# Log some debugging information
echo "1=$1"
echo "BUILT_PRODUCTS_DIR=$BUILT_PRODUCTS_DIR"
echo "PROJECT_DIR=$PROJECT_DIR"

# Perform actions, given the passed-in build step
case "$1" in
	"clean")
		echo "Cleaning build68k output(s)"
		rm -rf "$BUILT_PRODUCTS_DIR/build68k_output"
		;;
	"")
		echo "Running build68k"
		cd "$BUILT_PRODUCTS_DIR"
		mkdir -p build68k_output
		cd build68k_output
		cat "$PROJECT_DIR/../uae_cpu/table68k" | "$BUILT_PRODUCTS_DIR/build68k" > "./defs68k.c"
		ls -al
		;;
esac
