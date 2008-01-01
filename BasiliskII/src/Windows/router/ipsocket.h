/*
 *  ipsocket.h - ip router
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

#ifndef _IPSOCKET_H_
#define _IPSOCKET_H_

class socket_t {
public:
	socket_t( int _proto );
	~socket_t();
	bool b_recfrom();
	void set_ttl( uint8 ttl );

protected:
	int WSARecvFrom();

public:
	SOCKET s;				// Always a valid socket
	BOOL permanent; // T: a user-defined listening socket, 
	int proto;			// udp/icmp
	WSABUF buffers[1];
	WSABUF out_buffers[1];
	DWORD buffer_count;
	DWORD bytes_received;
	DWORD flags;
	struct sockaddr_in from;
	int from_len;
	WSAOVERLAPPED overlapped;
	uint32 ip_src;
	uint32 ip_dest;
	uint16 src_port;
	uint16 dest_port;
	DWORD socket_ttl;
};


int get_socket_index( uint16 src_port, uint16 dest_port, int proto );
int get_socket_index( uint16 src_port, int proto );
int get_socket_index( socket_t *cmpl );
void delete_socket( socket_t *cmpl );
socket_t *find_socket( uint16 src_port, uint16 dest_port, int proto );
void add_socket( socket_t *cmpl );
void close_old_sockets();
void close_all_sockets();


#endif // _IPSOCKET_H_
