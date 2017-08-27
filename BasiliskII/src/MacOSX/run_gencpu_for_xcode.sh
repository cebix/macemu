#!/bin/bash -e

#
# run_gemcpu_for_xcode.sh
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
		echo "Cleaning gencpu output(s)"
		rm -rf "$BUILT_PRODUCTS_DIR/gencpu_output"
		;;
	"")
		echo "Running gencpu"
		cd "$BUILT_PRODUCTS_DIR"
		mkdir -p gencpu_output
		cd gencpu_output
		"$BUILT_PRODUCTS_DIR/gencpu"
		ls -al
		;;
esac
