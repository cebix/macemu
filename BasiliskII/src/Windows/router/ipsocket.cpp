/*
 *  ipsocket.cpp - ip router
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
#include "icmp.h"
#include "tcp.h"
#include "udp.h"
#include "dump.h"


#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


socket_t::socket_t( int _proto )
{
	s = INVALID_SOCKET;
	proto = _proto;

	ip_src = ip_dest = 0;
	src_port = dest_port = 0;

	memset( &overlapped, 0, sizeof(overlapped) );
	overlapped.hEvent = (HANDLE)this;

	bytes_received = 0;
	flags = 0;
	from_len = sizeof(struct sockaddr_in);
	memset( &from, 0, sizeof(from) );
	from.sin_family = AF_INET;

	buffer_count = 1;
	buffers[0].len = 1460;
	buffers[0].buf = new char [buffers[0].len];
	
	out_buffers[0].len = 1460;
	out_buffers[0].buf = new char [out_buffers[0].len];

	socket_ttl = GetTickCount() + 60000L;
	permanent = false;
}

socket_t::~socket_t()
{
	if(s != INVALID_SOCKET) {
		_closesocket( s ); // slam!
		s = INVALID_SOCKET;
	}
	delete [] out_buffers[0].buf;
	delete [] buffers[0].buf;
}

int socket_t::WSARecvFrom()
{
	return _WSARecvFrom(
		s,                                               
		buffers,
		buffer_count,
		&bytes_received,
		&flags,
		(struct sockaddr *)&from,
		&from_len,
		&overlapped,
		proto == IPPROTO_UDP ? udp_read_completion : icmp_read_completion
	);
}

bool socket_t::b_recfrom()
{
	bool result;

	int ret = WSARecvFrom();

	if(ret == SOCKET_ERROR) {
		int socket_error = _WSAGetLastError();
		if(socket_error == WSA_IO_PENDING) {
			D(bug("WSARecvFrom() i/o pending\r\n"));
			result = true;
		} else {
			D(bug("_WSAGetLastError() returned %d\r\n", socket_error));
			result = false;
		}
	} else /*if(ret == 0) */ {
		D(bug("WSARecvFrom() ok\r\n"));
		// Completion routine call is already scheduled.
		result = true;
	}
	return result;
}

void socket_t::set_ttl( uint8 ttl )
{
	int _ttl = ttl; // defensive programming, I know VCx

	if(_setsockopt( s, IPPROTO_IP, IP_TTL, (const char *)&_ttl, sizeof(int) ) == SOCKET_ERROR  ) {
		D(bug("could not set ttl to %d.\r\n", ttl));
	} else {
		D(bug("ttl set to %d.\r\n", ttl));
	}
}


#define MAX_OPEN_SOCKETS 1024
static socket_t *all_sockets[MAX_OPEN_SOCKETS];
static int open_sockets = 0;

int get_socket_index( uint16 src_port, uint16 dest_port, int proto )
{
	int result = -1;
	for( int i=0; i<open_sockets; i++ ) {
		socket_t *cmpl = all_sockets[i];
		if(cmpl->src_port == src_port && cmpl->dest_port == dest_port && cmpl->proto == proto ) {
			result = i;
			break;
		}
	}
	return result;
}

int get_socket_index( uint16 src_port, int proto )
{
	int result = -1;
	for( int i=0; i<open_sockets; i++ ) {
		socket_t *cmpl = all_sockets[i];
		if(cmpl->src_port == src_port && cmpl->proto == proto ) {
			result = i;
			break;
		}
	}
	return result;
}

int get_socket_index( socket_t *cmpl )
{
	int result = -1;
	for( int i=0; i<open_sockets; i++ ) {
		if(cmpl == all_sockets[i]) {
			result = i;
			break;
		}
	}
	return result;
}

void delete_socket( socket_t *cmpl )
{
  D(bug("deleting socket(%d,%d)\r\n", cmpl->src_port, cmpl->dest_port));

	EnterCriticalSection( &router_section );
	int i = get_socket_index( cmpl );
	if( i >= 0 ) {
		delete all_sockets[i];
		all_sockets[i] = all_sockets[--open_sockets];
	} else {
	  D(bug("Deleted socket not in table!\r\n"));
		// delete cmpl;
	}
	LeaveCriticalSection( &router_section );
}

socket_t *find_socket( uint16 src_port, uint16 dest_port, int proto )
{
	socket_t *result = 0;
	EnterCriticalSection( &router_section );
	int i = get_socket_index( src_port, dest_port, proto );
	if( i >= 0 ) {
		result = all_sockets[i];
	} else {
		i = get_socket_index( src_port, proto );
		if( i >= 0 ) {
			delete_socket( all_sockets[i] );
		}
	}
	LeaveCriticalSection( &router_section );

  D(bug("find_socket(%d,%d): %s\r\n", src_port, dest_port, result ? "found" : "not found"));

	return result;
}

void add_socket( socket_t *cmpl )
{
  D(bug("adding socket(%d,%d)\r\n", cmpl->src_port, cmpl->dest_port));

	EnterCriticalSection( &router_section );
	if( open_sockets < MAX_OPEN_SOCKETS ) {
		all_sockets[open_sockets++] = cmpl;
	} else {
		// Urgchiyuppijee! (that's finnish language, meaning "do something about this")
		delete all_sockets[0];
		all_sockets[0] = cmpl;
	}
	LeaveCriticalSection( &router_section );
}

void close_old_sockets()
{
	DWORD now = GetTickCount();

	EnterCriticalSection( &router_section );
	for( int i=open_sockets-1; i>=0; i-- ) {
		socket_t *cmpl = all_sockets[i];
		if( !cmpl->permanent && now >= cmpl->socket_ttl ) {
		  D(bug("expiring socket(%d,%d)\r\n", cmpl->src_port, cmpl->dest_port));
			if(cmpl->s == INVALID_SOCKET) {
				delete all_sockets[i];
				all_sockets[i] = all_sockets[--open_sockets];
			} else {
				// read completion will deallocate
				_closesocket( cmpl->s );
			}
		}
	}
	LeaveCriticalSection( &router_section );
}

void close_all_sockets()
{
  D(bug("closing all(%d) sockets\r\n", open_sockets));

	EnterCriticalSection( &router_section );
	for( int i=0; i<open_sockets; i++ ) {
		socket_t *cmpl = all_sockets[i];
	  D(bug("closing socket(%d,%d)\r\n", cmpl->src_port, cmpl->dest_port));
		if(cmpl->s == INVALID_SOCKET) {
			delete all_sockets[i];
			all_sockets[i] = all_sockets[--open_sockets];
		} else {
			// read completion will deallocate
			_closesocket( cmpl->s );
		}
	}
	LeaveCriticalSection( &router_section );
}
