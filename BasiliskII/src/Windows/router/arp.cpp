/*
 *  arp.cpp - ip router
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
#include "cpu_emulation.h"
#include "prefs.h"
#include "ether_windows.h"
#include "ether.h"
#include "router.h"
#include "router_types.h"
#include "iphelp.h"
#include "arp.h"
#include "icmp.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


// ARP queries can be replied immediately.

bool write_arp( arp_t *req, int len )
{
  D(bug("write_arp() len=%d, htype=%d, ptype=%04x, opcode=%d, halen=%d, palen=%d\r\n",len, ntohs(req->htype), ntohs(req->ptype), ntohs(req->opcode), req->halen, req->palen));

	start_icmp_listen();

	bool result = false;

	if( len >= sizeof(arp_t) &&
			req->htype == htons(arp_hwtype_enet) &&
			req->ptype == htons(mac_type_ip4) &&
			req->opcode == htons(arp_request) &&	
			req->halen == 6 &&
			req->palen == 4
	)
	{
		if(memcmp( req->srcp, req->dstp, 4 ) == 0) {
			// No reply. MacOS is making sure that there are no duplicate ip addresses.
			// Update localhost (==Mac) ip address (needed by incoming icmp)
			macos_ip_address = ntohl( *((uint32 *)&req->srcp[0]) );
			D(bug("Mac ip: %02x %02x %02x %02x\r\n", req->srcp[0], req->srcp[1], req->srcp[2], req->srcp[3]));
		} else {
			arp_t arp;

			D(bug("Source NIC: %02x %02x %02x %02x\r\n", req->srcp[0], req->srcp[1], req->srcp[2], req->srcp[3]));
			D(bug("Dest   NIC: %02x %02x %02x %02x\r\n", req->dstp[0], req->dstp[1], req->dstp[2], req->dstp[3]));

			// memcpy( arp.mac.dest, req->mac.src, 6 );
			memcpy( arp.mac.dest, ether_addr, 6 );
			memcpy( arp.mac.src, router_mac_addr, 6 );
			arp.mac.type = htons(mac_type_arp);
			arp.htype = htons(arp_hwtype_enet);
			arp.ptype = htons(mac_type_ip4);
			arp.halen = 6;
			arp.palen = 4;
			arp.opcode = htons(arp_reply);
			memcpy( arp.srch, router_mac_addr, 6 );
			memcpy( arp.srcp, req->dstp, 4 );
			// memcpy( arp.dsth, req->srch, 6 );
			memcpy( arp.dsth, ether_addr, 6 );
			memcpy( arp.dstp, req->srcp, 4 );

			// Update here, too, just in case.
			macos_ip_address = ntohl( *((uint32 *)&req->srcp[0]) );

			enqueue_packet( (uint8 *)&arp, sizeof(arp) );
		}
		result = true;
	}
	return result;
}
