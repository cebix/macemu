/*
 *  router.cpp - ip router
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

/*
 *  This could be implemented by writing three (9x,nt,2k)
 *  NDIS filter drivers. No thanks.
 *  But this is not easy either.
 */

#include "sysdeps.h"
#include "main.h"

#include <process.h>

#include "cpu_emulation.h"
#include "prefs.h"
#include "ether_windows.h"
#include "ether.h"
#include "router.h"
#include "router_types.h"
#include "dynsockets.h"
#include "ipsocket.h"
#include "iphelp.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "ftp.h"
#include "mib/interfaces.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


uint16 next_ip_ident_number = 1;
uint32 macos_ip_address = 0;
const uint8 router_mac_addr[6] = { '4', '2', '6', '7', '7', '9' };
uint32 router_ip_address = 0;
bool raw_sockets_available = false;



// Protected data.
CRITICAL_SECTION router_section;
bool is_router_shutting_down = false;
static HANDLE r_handle = 0;
static unsigned int rh_tid = 0;


static void write_ip4( ip_t *ip, int len )
{
	if(len < sizeof(ip_t)) {
	  D(bug("Too small ip packet(%d), dropped\r\n", len));
	} else {
		uint8 proto = ip->proto;

		// This is a router, decrement the hop count
		if( --ip->ttl == 0 ) {
			// Most likely this is some Mac traceroute app
		  D(bug("ip packet ttl expired, proto=%d.\r\n", proto));
			error_winsock_2_icmp( WSAETTLEXCEEDED, ip, len );
		} else {
			switch( proto ) {
				case ip_proto_icmp:
					write_icmp( (icmp_t *)ip, len );
					break;
				case ip_proto_tcp:
					write_tcp( (tcp_t *)ip, len );
					break;
				case ip_proto_udp:
					write_udp( (udp_t *)ip, len );
					break;
				default:
				  D(bug("write_ip4() len=%d, proto=%d\r\n", len, proto));
					break;
			}
		}
	}
}

bool router_write_packet(uint8 *packet, int len)
{
	bool result = false;

	if( len >= 14 ) {
		switch( ntohs( ((mac_t *)packet)->type ) ) {
			case mac_type_ip4:
				write_ip4( (ip_t *)packet, len );
				result = true;
				break;
			case mac_type_ip6:
			  D(bug("write_ip6() len=%d; unsupported.\r\n", len));
				result = true;
				break;
			case mac_type_arp:
				result = write_arp( (arp_t *)packet, len );
				break;
		}
	}
	return result;
}

bool router_read_packet(uint8 *packet, int len)
{
	bool result = false;

	if( len >= 14 ) {
		switch( ntohs( ((mac_t *)packet)->type ) ) {
			case mac_type_ip4:
			case mac_type_ip6:
			case mac_type_arp:
				result = true;
				break;
		}
	}
	return result;
}

/*
	This has nothing to do with TCP TIME_WAITs or CLOSE_WAITs,
	the thread is needed to close down expired udp sockets.
	Arguably an ugly hack, but needed since there is no way to
	listen to all ports w/o writing another ndis filter driver
*/
static unsigned int WINAPI router_expire_thread(void *arg)
{
	while(!is_router_shutting_down) {
		close_old_sockets();
		Sleep(1000);
	}
	return 0;
}

bool router_init(void)
{
	InitializeCriticalSection( &router_section );

	if(dynsockets_init()) {
		char me[128];
		if( _gethostname(me, sizeof(me)) == SOCKET_ERROR ) {
			D(bug("gethostname() failed, error = %d\r\n", _WSAGetLastError()));
		} else {
			struct hostent *hent = _gethostbyname(me);
			if( hent == NULL ) {
				D(bug("gethostbyname() failed, error = %d\r\n", _WSAGetLastError()));
			} else {
				struct in_addr *ina = (struct in_addr *) *hent->h_addr_list;
				router_ip_address = ntohl(ina->s_addr);
			  D(bug("router protocol address seems to be %s (used only in icmp error messages)\r\n", _inet_ntoa(*ina)));
			} 
		}
		is_router_shutting_down = false;
		r_handle = (HANDLE)_beginthreadex( 0, 0, router_expire_thread, 0, 0, &rh_tid );
		init_interfaces();
		init_tcp();
		init_udp();
		init_ftp();
		return true;
	}

	return false;
}

void router_final(void)
{
	final_interfaces();
	stop_icmp_listen();
	close_all_sockets();
	if(r_handle) {
		is_router_shutting_down = true;
		WaitForSingleObject( r_handle, INFINITE );
		final_tcp();
		final_udp();
		dynsockets_final();
	}
	DeleteCriticalSection( &router_section );
}
