/*
 *  sysdeps.h - System dependent definitions for AmigaOS
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SYSDEPS_H
#define SYSDEPS_H

#include <exec/types.h>
#include <devices/timer.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "user_strings_amiga.h"

// Mac and host address space are the same
#define REAL_ADDRESSING 1

// Using 68k natively
#define EMULATED_68K 0

// Mac ROM is not write protected
#define ROM_IS_WRITE_PROTECTED 0
#define USE_SCRATCHMEM_SUBTERFUGE 1

// ExtFS is supported
#define SUPPORTS_EXTFS 1

// mon is not supported
#undef ENABLE_MON

// Data types
typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned long uint32;
typedef signed long int32;
typedef unsigned long long uint64;
typedef signed long long int64;

typedef unsigned long long loff_t;

// Time data type for Time Manager emulation
typedef struct timeval tm_time_t;

// Endianess conversion (not needed)
#define ntohs(x) (x)
#define ntohl(x) (x)
#define htons(x) (x)
#define htonl(x) (x)

// Some systems don't define this (ExecBase->AttnFlags)
#ifndef AFF_68060
#define AFF_68060 (1L<<7)
#endif

#endif
