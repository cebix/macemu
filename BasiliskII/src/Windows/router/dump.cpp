/*
 *  dump.cpp - ip router
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
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

#include "sysdeps.h"
#include "main.h"
#include "dump.h"

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


void dump_bytes( uint8 *packet, int length )
{
#if DEBUG
	char buf[1000], sm[10];

	*buf = 0;

	if(length > 256) length = 256;

  for (int i=0; i<length; i++) {
    sprintf(sm,"%02x", (int)packet[i]);
		strcat( buf, sm );
  }
	strcat( buf, "\r\n" );
  bug(buf);
#endif
}
