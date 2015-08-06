/*
 *  iphelp.cpp - ip router
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
#include "ether_windows.h"
#include "ether.h"
#include "router.h"
#include "router_types.h"
#include "tcp.h"
#include "icmp.h"
#include "udp.h"
#include "iphelp.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


void make_icmp_checksum( icmp_t *icmp, int len )
{
	icmp->checksum = 0;

	uint16 sz = (len-sizeof(ip_t))/2;
	uint16 *p = (uint16 *)( (uint8 *)icmp + sizeof(ip_t) );

	uint32 sum32 = 0;
	for( int i=0; i<sz; i++ ) {
		sum32 += ntohs(p[i]);
	}

	while( HIWORD(sum32) ) {
		sum32 = HIWORD(sum32) + LOWORD(sum32);
	}
	sum32 = ~sum32;
	icmp->checksum = htons((uint16)sum32);
}

void make_tcp_checksum( tcp_t *tcp, int len )
{
	tcp->checksum = 0;

	int tcp_len = len - sizeof(ip_t);
	uint16 sz = tcp_len/2;
	uint16 *p = (uint16 *)( (uint8 *)tcp + sizeof(ip_t) );

	uint32 sum32 = 0;
	for( int i=0; i<sz; i++ ) {
		sum32 += ntohs(p[i]);
	}

	if(len & 1) {
		uint8 *p8 = (uint8 *)p;
		sum32 += p8[tcp_len-1] << 8;
	}

	pseudo_ip_t pseudo;
	pseudo.src_lo = LOWORD(ntohl(tcp->ip.src));
	pseudo.src_hi = HIWORD(ntohl(tcp->ip.src));
	pseudo.dest_lo = LOWORD(ntohl(tcp->ip.dest));
	pseudo.dest_hi = HIWORD(ntohl(tcp->ip.dest));
	pseudo.proto = (uint16)tcp->ip.proto;
	pseudo.msg_len = tcp->header_len >> 2;

	int datalen = len - sizeof(tcp_t);
	pseudo.msg_len += datalen;

	p = (uint16 *)&pseudo;

	for( int i=0; i<sizeof(pseudo_ip_t)/2; i++ ) {
		sum32 += p[i];
	}

	while( HIWORD(sum32) ) {
		sum32 = HIWORD(sum32) + LOWORD(sum32);
	}
	sum32 = ~sum32;
	tcp->checksum = htons((uint16)sum32);
}

void make_ip4_checksum( ip_t *ip )
{
	ip->checksum = 0;
	uint16 sz = ip->header_len * 2;
	uint16 *p = (uint16 *)( (uint8 *)ip + sizeof(mac_t) );

	uint32 sum32 = 0;
	for( int i=0; i<sz; i++ ) {
		sum32 += ntohs(p[i]);
	}

	while( HIWORD(sum32) ) {
		sum32 = HIWORD(sum32) + LOWORD(sum32);
	}

	sum32 = ~sum32;
	ip->checksum = htons((uint16)sum32);
}

void make_udp_checksum( udp_t *udp )
{
	udp->checksum = 0;
	return;

	// UDP checksums are optional.

	/*
	uint16 sz = ntohs(udp->msg_len) / 2;
	uint16 *p = (uint16 *)( (uint8 *)udp + sizeof(ip_t) );

	uint32 sum32 = 0;
	for( int i=0; i<sz; i++ ) {
		sum32 += ntohs(p[i]);
	}

	// last byte??

	pseudo_ip_t pseudo;
	pseudo.src_lo = LOWORD(ntohl(udp->ip.src));
	pseudo.src_hi = HIWORD(ntohl(udp->ip.src));
	pseudo.dest_lo = LOWORD(ntohl(udp->ip.dest));
	pseudo.dest_hi = HIWORD(ntohl(udp->ip.dest));
	pseudo.proto = (uint16)udp->ip.proto;
	pseudo.msg_len = ntohs(udp->msg_len); // ???

	p = (uint16 *)&pseudo;

	for( i=0; i<sizeof(pseudo_ip_t)/2; i++ ) {
		sum32 += p[i];
	}

	while( HIWORD(sum32) ) {
		sum32 = HIWORD(sum32) + LOWORD(sum32);
	}
	sum32 = ~sum32;
	udp->checksum = htons((uint16)sum32);
	*/
}

void error_winsock_2_icmp( int err, ip_t *ip_err, int dlen_err )
{
	int type = -1, code = -1, msg_size = 0;

	switch( err ) {
		case WSAEHOSTUNREACH:
		case WSAETIMEDOUT:
			type = icmp_Destination_unreachable;
			code = 1;	// Host unreachable
			msg_size = (ip_err->header_len << 2) + 4 + 8; // ip header + unused + 64 msg bits
			break;
		case WSAENETDOWN:
		case WSAENETUNREACH:
			type = icmp_Destination_unreachable;
			code = 0;	// Network unreachable
			msg_size = (ip_err->header_len << 2) + 4 + 8; // ip header + unused + 64 msg bits
			break;
		case WSAETTLEXCEEDED:
			type = icmp_Time_exceeded;
			code = 0;		// Transit TTL exceeded
			msg_size = (ip_err->header_len << 2) + 4 + 8; // ip header + unused + 64 msg bits
			break;
	}

	if(type >= 0 && macos_ip_address != 0) {
	  D(bug("sending icmp error reply. type=%d, code=%d, msg_size=%d\r\n", type, code, msg_size));

		int icmp_size = sizeof(icmp_t) + msg_size;

		icmp_t *icmp = (icmp_t *)malloc( icmp_size );
		if(icmp) {
			mac_t *mac = (mac_t *)icmp;
			ip_t *ip = (ip_t *)icmp;

			memcpy( mac->dest, ether_addr, 6 );
			memcpy( mac->src, router_mac_addr, 6 );
			mac->type = htons(mac_type_ip4);

			ip->version = 4;
			ip->header_len = 5;
			ip->tos = 0;
			ip->total_len = htons(sizeof(icmp_t) - sizeof(mac_t) + msg_size);

			ip->ident = htons(next_ip_ident_number++);
			ip->flags_n_frag_offset = 0;
			ip->ttl = 128;
			ip->proto = ip_proto_icmp;
			ip->src = htonl(router_ip_address);
			ip->dest = htonl(macos_ip_address);
			make_ip4_checksum( ip );

			icmp->type = type;
			icmp->code = code;

			// zero out the unused field
			memset( (char *)icmp + sizeof(icmp_t), 0, sizeof(uint32) );

			// copy 64 bits of original message
			memcpy( 
				(char *)icmp + sizeof(icmp_t) + sizeof(uint32),
				(char *)ip_err + sizeof(mac_t),
				msg_size
			);

			make_icmp_checksum( icmp, icmp_size );

			dump_bytes( (uint8 *)icmp, icmp_size );

			enqueue_packet( (uint8 *)icmp, icmp_size );
			free(icmp);
		}
	}
}
