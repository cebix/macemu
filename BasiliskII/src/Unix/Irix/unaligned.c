/*
 *  Irix/unaligned.c - Optimized unaligned access for Irix
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifdef sgi
#include "sysdeps.h"

/* Tell the compiler to pack data on 1-byte boundaries 
 * (i.e. arbitrary alignment).  Requires SGI MIPSPro compilers. */
#pragma pack(1)

typedef struct _ual32 {
	uae_u32 v;
} ual32_t;

typedef struct _ual16 {
	uae_u16 v;
} ual16_t;

#pragma pack(0)

/* The compiler is smart enough to inline these when you build with "-ipa" */
uae_u32 do_get_mem_long(uae_u32 *a) {return ((ual32_t *)a)->v;}
uae_u32 do_get_mem_word(uae_u16 *a) {return ((ual16_t *)a)->v;}
void do_put_mem_long(uae_u32 *a, uae_u32 v) {((ual32_t *)a)->v = v;}
void do_put_mem_word(uae_u16 *a, uae_u32 v) {((ual16_t *)a)->v = v;}

#endif /* sgi */
