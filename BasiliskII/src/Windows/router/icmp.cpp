/*
 *  icmp.cpp - ip router
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
#include "ws2tcpip.h"
#include "prefs.h"
#include "ether_windows.h"
#include "ether.h"
#include "router.h"
#include "router_types.h"
#include "dynsockets.h"
#include "ipsocket.h"
#include "iphelp.h"
#include "icmp.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


// Saved for cleanup.
static socket_t *icmp_incoming_s = 0;


void stop_icmp_listen()
{
	if(icmp_incoming_s) {
		delete icmp_incoming_s;
		icmp_incoming_s = 0;
	}
}

void start_icmp_listen()
{
	if(!icmp_incoming_s) {
		icmp_incoming_s = new socket_t(IPPROTO_ICMP);

		icmp_incoming_s->permanent = TRUE;
		icmp_incoming_s->s = _socket( AF_INET, SOCK_RAW, IPPROTO_ICMP );
		
		memset( &icmp_incoming_s->from, 0, icmp_incoming_s->from_len );
		icmp_incoming_s->from.sin_family = AF_INET;

		if(icmp_incoming_s->s == INVALID_SOCKET) {
			D(bug("Failed to create icmp listening socket (NT/no admin?)\r\n" ));
			delete icmp_incoming_s;
			icmp_incoming_s = 0;
		} else {
			D(bug("icmp listening socket created\r\n" ));
			raw_sockets_available = true;
			struct sockaddr_in to;
			memset( &to, 0, sizeof(to) );
			to.sin_family = AF_INET;
			if(	_bind ( icmp_incoming_s->s, (const struct sockaddr *)&to, sizeof(to) ) == SOCKET_ERROR ) {
				D(bug("Listening to inbound icmp failed, error code = %d\r\n", _WSAGetLastError() ));
				_closesocket( icmp_incoming_s->s );
				delete icmp_incoming_s;
				icmp_incoming_s = 0;
			} else {
				D(bug("icmp listening socket bound\r\n" ));
				if(!icmp_incoming_s->b_recfrom()) {
					D(bug("b_recfrom() from inbound icmp failed, error code = %d\r\n", _WSAGetLastError() ));
					// _closesocket( icmp_incoming_s->s );
					// delete icmp_incoming_s;
					// icmp_incoming_s = 0;
				}
			}
		}
	}
}

void CALLBACK icmp_read_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
)
{
  D(bug("icmp_read_completion(error=0x%x, bytes_read=%d, flags=0x%x)\r\n", error, bytes_read, flags));

	socket_t *cmpl = (socket_t *)lpOverlapped->hEvent;

	if(error == 0 && macos_ip_address != 0) {
		if(bytes_read > 1460) {
		  D(bug("discarding oversized icmp packet, size = \r\n", bytes_read));
		} else {
			int icmp_size = sizeof(mac_t) + bytes_read;
			icmp_t *icmp = (icmp_t *)malloc( icmp_size );
			if(icmp) {
				mac_t *mac = (mac_t *)icmp;
				ip_t *ip = (ip_t *)icmp;

				memcpy( mac->dest, ether_addr, 6 );
				memcpy( mac->src, router_mac_addr, 6 );
				mac->type = htons(mac_type_ip4);

				// Copy payload (used by ICMP checksum)
				memcpy( (char *)icmp + sizeof(mac_t), cmpl->buffers[0].buf, bytes_read );

				switch( icmp->type ) {
					// May need to patch the returned ip header.
					case icmp_Destination_unreachable:
					case icmp_Source_quench:
					case icmp_Redirect:
					case icmp_Time_exceeded:
					case icmp_Parameter_problem:
						ip_t *ip_if = (ip_t *)( (char *)icmp + sizeof(icmp_t) + sizeof(uint32) - sizeof(mac_t) );

						// This would be needed (traceroute)
						// ip_if->ident = ??;

						// Cannot fix some fields, this should be enough:
						ip_if->src = htonl(macos_ip_address);

						if(ip_if->proto == ip_proto_udp) {
							udp_t *udp_if = (udp_t *)ip_if;
							// udp_if->src_port = ... don't know!;
						} else if(ip_if->proto == ip_proto_tcp) {
							tcp_t *tcp_if = (tcp_t *)ip_if;
							// tcp_if->src_port = ... don't know!;
						}
						break;
				}

				make_icmp_checksum( icmp, icmp_size );

				// Replace the target ip address
				ip->dest = htonl(macos_ip_address);
				ip->ttl--;
				make_ip4_checksum( ip );

				dump_bytes( (uint8 *)icmp, icmp_size );

				if( ip->ttl == 0 ) {
					D(bug("icmp packet ttl expired\r\n"));
				} else {
					enqueue_packet( (uint8 *)icmp, icmp_size );
				}
				free(icmp);
			}
		}
	}

	memset( &cmpl->from, 0, cmpl->from_len );

	if(is_router_shutting_down) {
		delete cmpl;
	} else if(cmpl->s == INVALID_SOCKET || !cmpl->b_recfrom()) {
		// delete cmpl;
	}
}

void write_icmp( icmp_t *icmp, int len )
{
	struct in_addr ia;
	ia.s_addr = icmp->ip.dest;
	D(bug("write_icmp(%s)\r\n", _inet_ntoa(ia) ));

	if(!raw_sockets_available) {
		D(bug("write_icmp() cannot proceed, raw sockets not available\r\n" ));
		return;
	}

	if(len < sizeof(icmp_t)) {
	  D(bug("Too small icmp packet(%d), dropped\r\n", len));
		return;
	}

	// must be updated, ttl changed
	make_icmp_checksum( icmp, len );

	SOCKET s = _socket( AF_INET, SOCK_RAW, IPPROTO_ICMP );
	if(s != INVALID_SOCKET) {
		struct sockaddr_in to;
		memset( &to, 0, sizeof(to) );
		to.sin_family = AF_INET;
		to.sin_addr.s_addr = icmp->ip.dest;

		char *data = (char *)icmp + sizeof(ip_t);
		int dlen = len - sizeof(ip_t);

		int ttl = icmp->ip.ttl;
		if(_setsockopt( s, IPPROTO_IP, IP_TTL, (const char *)&ttl, sizeof(int) ) == SOCKET_ERROR  ) {
			D(bug("could not set ttl to %d.\r\n", ttl));
		} else {
			D(bug("ttl set to %d.\r\n", ttl));
		}

		if(SOCKET_ERROR == _sendto( s, data, dlen, 0, (struct sockaddr *)&to, sizeof(to) )) {
			D(bug("Failed to send icmp via raw socket\r\n" ));
		}
		_closesocket(s);
	} else {
		D(bug("Could not create raw socket for icmp\r\n" ));
	}
}
