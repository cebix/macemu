/*
 *  interfaces.cpp - ip router
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
#include "interfaces.h"
#include "../dump.h"
#include "mibaccess.h"

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"

static UINT ip_array[100];
static UINT ip_array_sz = 0;

void init_interfaces()
{
	MibII _mibs(false);

	ip_array_sz = sizeof(ip_array) / sizeof(ip_array[0]);

	if(_mibs.Init()) {
		_mibs.GetIPAddress( ip_array, ip_array_sz );
	}

	if(ip_array_sz == 0) {
		ip_array_sz = 1;
		ip_array[0] = 0; // localhost
	}

	D(bug("init_interfaces() found %d interfaces.\r\n", ip_array_sz));
}

void final_interfaces()
{
	// Currently nothing to do.
}

int get_ip_count()
{
	return ip_array_sz;
}

uint32 get_ip_by_index( int index )
{
	return index >= 0 && index < (int)ip_array_sz ? ip_array[index] : 0;
}
