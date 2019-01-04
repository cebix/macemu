/*
 *  udp.cpp - ip router
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
#include "dynsockets.h"
#include "ipsocket.h"
#include "iphelp.h"
#include "udp.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


void CALLBACK udp_read_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
)
{
  D(bug("udp_read_completion(error=0x%x, bytes_read=%d, flags=0x%x)\r\n", error, bytes_read, flags));

	socket_t *cmpl = (socket_t *)lpOverlapped->hEvent;

	// It's not easy to know whether empty upd datagrams should be passed along. doh.
	if(error == 0 && bytes_read > 0) {

		if(bytes_read > 1460) {
		  D(bug("discarding oversized udp packet, size = \r\n", bytes_read));
		} else {
			struct sockaddr_in name;
			int namelen = sizeof(name);
			memset( &name, 0, sizeof(name) );
			if( _getsockname( cmpl->s, (struct sockaddr *)&name, &namelen ) == SOCKET_ERROR ) {
				D(bug("_getsockname() failed, error=%d\r\n", _WSAGetLastError() ));
			} else {
				D(bug("_getsockname(): port=%d\r\n", ntohs(name.sin_port) ));
			}

			int udp_size = sizeof(udp_t) + bytes_read;
			udp_t *udp = (udp_t *)malloc( udp_size );
			if(udp) {
				mac_t *mac = (mac_t *)udp;
				ip_t *ip = (ip_t *)udp;

				// Build MAC
				// memcpy( udp->ip.mac.dest, cmpl->mac_src, 6 );
				memcpy( mac->dest, ether_addr, 6 );
				memcpy( mac->src, router_mac_addr, 6 );
				mac->type = htons(mac_type_ip4);

				// Build IP
				ip->version = 4;
				ip->header_len = 5;
				ip->tos = 0;
				ip->total_len = htons(sizeof(udp_t) - sizeof(mac_t) + bytes_read); // no options
				ip->ident = htons(next_ip_ident_number++); // htons() might be a macro... but does not really matter here.
				ip->flags_n_frag_offset = 0;
				ip->ttl = 128; // one hop actually!
				ip->proto = ip_proto_udp;
				ip->src = htonl(cmpl->ip_dest);
				ip->dest = htonl(cmpl->ip_src);
				make_ip4_checksum( (ip_t *)udp );

				// Copy payload (used by UDP checksum)
				memcpy( (char *)udp + sizeof(udp_t), cmpl->buffers[0].buf, bytes_read );

				// Build UDP
				udp->src_port = htons(cmpl->dest_port);
				udp->dest_port = htons(cmpl->src_port);
				udp->msg_len = htons(sizeof(udp_t) - sizeof(ip_t) + bytes_read); // no options
				make_udp_checksum( udp );

				dump_bytes( (uint8 *)udp, udp_size );

				enqueue_packet( (uint8 *)udp, udp_size );
				free(udp);
			}
		}
	}

	if(!is_router_shutting_down && cmpl->s != INVALID_SOCKET && cmpl->b_recfrom()) {
		cmpl->socket_ttl = GetTickCount() + 60000L;
	} else {
		delete_socket( cmpl );
	}
}

void write_udp( udp_t *udp, int len )
{
	if( len < sizeof(udp_t) ) {
	  D(bug("Too small udp packet(%d), dropped\r\n", len));
		return;
	}

	uint16 src_port = ntohs(udp->src_port);
	uint16 dest_port = ntohs(udp->dest_port);

	BOOL ok = true;

	socket_t *cmpl = find_socket( src_port, dest_port, IPPROTO_UDP );

	BOOL old_socket_found = cmpl != 0;

	if(!cmpl) {
		cmpl = new socket_t(IPPROTO_UDP);
		if(cmpl) {
			cmpl->s = _socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
			if(cmpl->s == INVALID_SOCKET) {
				delete cmpl;
				cmpl = 0;
				ok = false;
			} else {
				cmpl->src_port = src_port;
				cmpl->dest_port = dest_port;
				add_socket( cmpl );
			}
		} else {
			ok = false;
		}
	}

	if(ok) {
		cmpl->src_port = src_port;
		cmpl->dest_port = dest_port;
		cmpl->ip_src = ntohl(udp->ip.src);
		cmpl->ip_dest = ntohl(udp->ip.dest);

		struct sockaddr_in to;
		memset( &to, 0, sizeof(to) );
		to.sin_family = AF_INET;
		to.sin_port = udp->dest_port;
		to.sin_addr.s_addr = udp->ip.dest;

		char *data = (char *)udp + sizeof(udp_t);
		int dlen = len - sizeof(udp_t);

		// ttl changed, update checksum
		make_udp_checksum( udp );

		cmpl->set_ttl( udp->ip.ttl );

		bool please_close = true;
		/*
			Note that broadcast messages fill fail, no setsockopt(SO_BROADCAST).
			That's exactly what I want.
		*/
		if(SOCKET_ERROR != _sendto( cmpl->s, data, dlen, 0, (struct sockaddr *)&to, sizeof(to) )) {
			if(old_socket_found) {
				// This socket is not overlapped.
				please_close = false;
			} else {
				if(cmpl->b_recfrom()) please_close = false;
			}
			cmpl->socket_ttl = GetTickCount() + 60000L;
		} else {
			int socket_error = _WSAGetLastError();
			D(bug("_sendto() completed with error %d\r\n", socket_error));
			// TODO: check this out: error_winsock_2_icmp() uses router_ip_address
			// as source ip; but it's probably allright
			error_winsock_2_icmp( socket_error, (ip_t *)udp, len );
		}
		if(please_close) {
			delete_socket(cmpl);
		}
	}
}

void init_udp()
{
}

void final_udp()
{
}
