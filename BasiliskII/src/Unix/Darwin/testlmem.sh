#!/bin/sh

#  testlmem.sh - test whether the Mach-O hack works
#
#  Basilisk II (C) 1997-2005 Christian Bauer
#
#  testlmem.sh Copyright (C) 2003 Michael Z. Sliczniak
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

PAGEZERO_SIZE=0x2000
[[ -n "$1" ]] && PAGEZERO_SIZE=$1
# You want all the output to go to stderr so that configure is quiet but
# config.log is verbose.
{ echo 'building lowmem utility' && \
make -f /dev/null Darwin/lowmem && \
echo 'building pagezero test' && \
make -f /dev/null LDFLAGS="-pagezero_size $PAGEZERO_SIZE" Darwin/pagezero && \
echo 'enabling low memory globals in pagezero' && \
Darwin/lowmem Darwin/pagezero && \
echo 'running pagezero test' && \
Darwin/pagezero; } 1>&2
