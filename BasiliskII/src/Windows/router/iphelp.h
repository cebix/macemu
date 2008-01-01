/*
 *  iphelp.h - ip router
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

#ifndef _IPHELP_H_
#define _IPHELP_H_

// Fake ttl exceeded code.
#define WSAETTLEXCEEDED  (WSABASEERR + 1999 + 17)

void error_winsock_2_icmp( int err, ip_t *ip_err, int dlen_err );

void make_icmp_checksum( icmp_t *icmp, int len );
void make_ip4_checksum( ip_t *ip );
void make_udp_checksum( udp_t *udp );
void make_tcp_checksum( tcp_t *tcp, int len );

#endif // _IPHELP_H_
