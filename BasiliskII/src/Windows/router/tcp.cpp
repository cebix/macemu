/*
 *  tcp.cpp - ip router
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
 *  Features implemented:
 *		state machine, flow control, sequence numbers, RST/SYN/FIN/ACK/PSH
 *  
 *  Features not implemented:
 *		oob data, urgent pointer, window sliding, some options
 *		"Half-Nagle" implementation is a bit weird (mac-router interface; winsock has it on by default)
 *  
 *  
 *  All possible tcp state machine transitions:
 *  
 *		CLOSED ->	LISTEN						passive open
 *		CLOSED ->	SYN_SENT					active open				SYN->
 *  
 *		LISTEN ->	SYN_SENT					send data					SYN->
 *		LISTEN ->	SYN_RCVD					->SYN							SYN+ACK->
 *  
 *		SYN_SENT ->	SYN_RCVD				->SYN							SYN+ACK->
 *		SYN_SENT ->	ESTABLISHED			->SYN+ACK					ACK->
 *		SYN_SENT ->	CLOSED					close/timeout
 *  
 *		SYN_RCVD ->	CLOSED					timeout						RST->
 *		SYN_RCVD ->	LISTEN					->RST
 *		SYN_RCVD ->	ESTABLISHED			->ACK
 *		SYN_RCVD ->	FINWAIT_1				close							FIN->
 *  
 *		ESTABLISHED -> FINWAIT_1		close							FIN->
 *		ESTABLISHED -> CLOSE_WAIT		->FIN							ACK->
 *  
 *		CLOSE_WAIT -> LAST_ACK			close							FIN->
 *  
 *		LAST_ACK -> CLOSED					->ACK
 *  
 *		FINWAIT_1 -> CLOSING				->FIN							ACK->
 *		FINWAIT_1 -> FINWAIT_2			->ACK
 *		FINWAIT_1 -> TIME_WAIT			->FIN+ACK					ACK->
 *  
 *		FINWAIT_2 -> TIME_WAIT			->FIN							ACK->
 *  
 *		CLOSING -> TIME_WAIT				->ACK
 *  
 *		TIME_WAIT -> CLOSED					timeout (2*msl)
 *  
 */

#include "sysdeps.h"
#include "main.h"

#include <process.h>

#include "cpu_emulation.h"
#include "ws2tcpip.h"
#include "ether_windows.h"
#include "ether.h"
#include "prefs.h"
#include "router.h"
#include "router_types.h"
#include "dynsockets.h"
#include "iphelp.h"
#include "tcp.h"
#include "dump.h"
#include "mib/interfaces.h"
#include "ftp.h"

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"

// If you need more, use multiple threads.
#define MAX_SOCKETS MAXIMUM_WAIT_OBJECTS

// If true, always sends the PSH tcp flag with data.
// Otherwise only when a full buffer was received.
#define PUSH_ALWAYS 0

// In milliseconds. A TCP implementation should implement
// this dynamically, adapting the timeout value to match to the
// averaged packet round-trip time.
#define RESEND_TIMEOUT 750

// Just time out incoming connections after 5 secs if Mac has no time to reply
// No backlogs.
#define SYN_FLOOD_PROTECTION_TIMEOUT 5000

const int MAX_SEGMENT_SIZE = 1460;

// Shorthands
#define ISSET(f,x) ( ((f) & (x)) != 0 )
#define ISCLEAR(f,x) ( ((f) & (x)) == 0 )

// Local aliases
#define URG tcp_flags_URG
#define ACK tcp_flags_ACK
#define PSH tcp_flags_PSH
#define RST tcp_flags_RST
#define SYN tcp_flags_SYN
#define FIN tcp_flags_FIN

// Local aliases
#define CLOSED 			tcp_state_closed
#define LISTEN 			tcp_state_listen
#define SYN_SENT 		tcp_state_syn_sent
#define SYN_RCVD 		tcp_state_syn_rcvd
#define ESTABLISHED tcp_state_established
#define CLOSE_WAIT 	tcp_state_close_wait
#define LAST_ACK 		tcp_state_last_ack
#define FINWAIT_1 	tcp_state_finwait_1
#define FINWAIT_2 	tcp_state_finwait_2
#define CLOSING 		tcp_state_closing
#define TIME_WAIT 	tcp_state_time_wait

// For debugging only
static const char *_tcp_state_name[] = {
	"CLOSED",
	"LISTEN",
	"SYN_SENT",
	"SYN_RCVD",
	"ESTABLISHED",
	"CLOSE_WAIT",
	"LAST_ACK",
	"FINWAIT_1",
	"FINWAIT_2",
	"CLOSING",
	"TIME_WAIT"
};
#define STATENAME(i) _tcp_state_name[i]

static CRITICAL_SECTION tcp_section;

typedef struct {
	SOCKET s;
	int state;

	uint32 ip_src;											// "source" is the mac, dest is the remote host,
	uint32 ip_dest;											// no matter who opened the connection.
	uint16 src_port;										// all in host byte order.
	uint16 dest_port;

	struct sockaddr_in from;						// remote host address, network byte order.
	int from_len;

	// note: no true windows sliding, only one buffer.
	WSABUF buffers_read[1];							// data from remote host to Mac
	DWORD buffer_count_read;
	DWORD bytes_received;
	DWORD flags_read;
	WSAOVERLAPPED overlapped_read;

	WSABUF buffers_write[1];						// data from Mac to remote host
	DWORD buffer_count_write;
	DWORD bytes_written;
	DWORD flags_write;
	WSAOVERLAPPED overlapped_write;

	bool remote_closed;									// remote will not send any more data
	bool accept_more_data_from_mac;			// are we ready to accept more data from mac

	uint32 seq_in;											// will ack this mac sequence number
	uint32 seq_out;											// next sequence number to mac (unless a resend is needed)
	uint32 mac_ack;											// mac has acked this byte count. can be used to determined when to send some more data

	uint32 bytes_to_send;								// total send block size
	uint32 bytes_remaining_to_send;			// unsent byte count

	uint16 mac_window;									// mac tcp receive window, slides according to the window principle
	uint16 our_window;									// not really used
	uint16 mac_mss;											// maximum segment size that mac reported at SYN handshaking

	// resend info
	uint32 last_seq_out;								// remember last packet seq number if a resend is needed
	uint32 resend_timeout;							// currently set t0 0.75 secs but not updated
	uint32 stream_to_mac_stalled_until;	// tick count indicating resend time

	DWORD time_wait;										// do a graceful close after MSL*2
	DWORD msl;

	int child;

	WSAEVENT ev;												// used to signal remote-initiated close and host-initiated connect.

	bool in_use;
} tcp_socket_t;

static tcp_socket_t sockets[MAX_SOCKETS];

typedef struct {
	SOCKET s;
	uint16 port;
	uint32 ip;
	uint32 iface;
	bool once;
	int parent;
	WSAEVENT ev;
} tcp_listening_socket_t;

static tcp_listening_socket_t l_sockets[MAX_SOCKETS];

static void CALLBACK tcp_read_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
);

static void CALLBACK tcp_write_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
);

// socket utilities assume that the critical section has already been entered.
static void free_socket( const int t )
{
	_WSAResetEvent( sockets[t].ev );
	if(sockets[t].s != INVALID_SOCKET) {
		_closesocket( sockets[t].s );
		sockets[t].s = INVALID_SOCKET;
	}
	sockets[t].state = CLOSED;
	sockets[t].stream_to_mac_stalled_until = 0;
	sockets[t].in_use = false;
	sockets[t].time_wait = 0;

	// if there was an attached listening socket (ftp), close it.
	int lst = sockets[t].child;
	if( lst >= 0 ) {
		if(l_sockets[lst].s != INVALID_SOCKET) {
		  D(bug("  closing listening socket %d\r\n", lst));
			_closesocket( l_sockets[lst].s );
			l_sockets[lst].s = INVALID_SOCKET;
		}
		l_sockets[lst].port = 0;
		l_sockets[lst].parent = -1;
	}
	sockets[t].child = -1;
}

static int alloc_socket()
{
	static int last_allocated_socket = -1;

	int i = last_allocated_socket;
	for( int j=0; j<MAX_SOCKETS; j++ ) {
		if( ++i >= MAX_SOCKETS ) i = 0;
		if( !sockets[i].in_use ) {
			D(bug("<%d> Socket allocated\r\n", i));

			last_allocated_socket = i;
			sockets[i].in_use = true;

			sockets[i].s = INVALID_SOCKET;
			sockets[i].state = CLOSED;
			sockets[i].remote_closed = false;

			sockets[i].accept_more_data_from_mac = false;

			sockets[i].ip_src = sockets[i].ip_dest = 0;
			// sockets[i].src_port = sockets[i].dest_port = 0;

			memset( &sockets[i].overlapped_read, 0, sizeof(sockets[i].overlapped_read) );
			sockets[i].overlapped_read.hEvent = (HANDLE)i;
			memset( &sockets[i].overlapped_write, 0, sizeof(sockets[i].overlapped_write) );
			sockets[i].overlapped_write.hEvent = (HANDLE)i;

			sockets[i].bytes_received = 0;
			sockets[i].bytes_written = 0;

			sockets[i].flags_read = 0;
			sockets[i].flags_write = 0;

			// sockets[i].from_len = sizeof(struct sockaddr_in);
			// memset( &sockets[i].from, 0, sizeof(sockets[i].from) );
			// sockets[i].from.sin_family = AF_INET;

			sockets[i].buffer_count_read = 1;
			sockets[i].buffers_read[0].len = MAX_SEGMENT_SIZE;
			if(!sockets[i].buffers_read[0].buf) {
				sockets[i].buffers_read[0].buf = new char [sockets[i].buffers_read[0].len];
			}
			
			sockets[i].buffer_count_write = 1;
			sockets[i].buffers_write[0].len = MAX_SEGMENT_SIZE;
			if(!sockets[i].buffers_write[0].buf) {
				sockets[i].buffers_write[0].buf = new char [sockets[i].buffers_write[0].len];
			}

			sockets[i].mac_window = MAX_SEGMENT_SIZE; // updated for all mac datagrams
			sockets[i].our_window = MAX_SEGMENT_SIZE; // should use about 8-16 kB, really
			sockets[i].mac_mss = 0;			// not known yet

			sockets[i].time_wait = 0;
			sockets[i].msl = 5000L;			// The round-trip time can be hard to estimate.

			sockets[i].seq_in = 0;
			sockets[i].seq_out = 0x00000001;
			sockets[i].mac_ack = 0;
			sockets[i].stream_to_mac_stalled_until = 0;

			sockets[i].resend_timeout = RESEND_TIMEOUT;

			sockets[i].child = -1;

			break;
		}
	}
	if(i == MAX_SOCKETS) {
		D(bug("Out of free sockets\r\n"));
		i = -1;
	}
	return i;
}

static int alloc_new_socket( const uint16 src_port, const uint16 dest_port, const uint32 ip_dest )
{
	int t = alloc_socket();

	if(t >= 0) {
		sockets[t].s = _socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if(sockets[t].s == INVALID_SOCKET) {
			free_socket( t );
			t = -1;
		} else {
			sockets[t].src_port = src_port;
			sockets[t].dest_port = dest_port;
			
			sockets[t].from_len = sizeof(sockets[t].from);
			memset( &sockets[t].from, 0, sockets[t].from_len );
			sockets[t].from.sin_family = AF_INET;
			sockets[t].from.sin_port = htons(dest_port);
			sockets[t].from.sin_addr.s_addr = htonl(ip_dest);

			struct sockaddr_in to;
			memset( &to, 0, sizeof(to) );
			to.sin_family = AF_INET;

			if(	_bind ( sockets[t].s, (const struct sockaddr *)&to, sizeof(to) ) == 0 ) {
				D(bug("<%d> socket bound\r\n", t));
			} else {
				if( _WSAGetLastError() == WSAEINPROGRESS ) {
					D(bug("<%d> bind: a blocking call is in progress.\r\n", t));
				} else {
					D(bug("<%d> bind failed with error code %d\r\n", t, _WSAGetLastError()));
				}
				free_socket( t );
				t = -1;
			}
		}
	}
	return t;
}

static int get_socket_index( const uint16 src_port, const uint16 dest_port )
{
	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(sockets[i].in_use && sockets[i].src_port == src_port && sockets[i].dest_port == dest_port ) {
			return i;
		}
	}
	return -1;
}

static int get_socket_index( const uint16 src_port )
{
	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(sockets[i].in_use && sockets[i].src_port == src_port ) {
			return i;
		}
	}
	return -1;
}

static int find_socket( const uint16 src_port, const uint16 dest_port )
{
	int i = get_socket_index( src_port, dest_port );
	if( i < 0 ) {
		i = get_socket_index( src_port );
		if( i >= 0 ) {
			if( sockets[i].s == INVALID_SOCKET ) {
			  D(bug("find_socket reusing slot %d...\r\n", i));
				sockets[i].in_use = false;
			} else {
			  D(bug("find_socket forcing close %d...\r\n", i));
				free_socket( i );
			}
			i = -1;
		}
	}

  D(bug("<%d> find_socket(%d,%d): %s\r\n", i, src_port, dest_port, i>=0 ? "found" : "not found"));

	return i;
}

static int alloc_listen_socket( const uint16 port, const uint32 ip, const uint32 iface, const bool once )
{
	static int last_allocated_socket = -1;

	int i = last_allocated_socket;

	for( int j=0; j<MAX_SOCKETS; j++ ) {
		if( ++i >= MAX_SOCKETS ) i = 0;
		if( l_sockets[i].port == 0 ) {
			D(bug("[%d] Slot allocated for listening port %d\r\n", i, port));
			l_sockets[i].port = port;
			l_sockets[i].ip = ip;
			l_sockets[i].iface = iface;
			l_sockets[i].once = once;
			l_sockets[i].parent = -1;
			last_allocated_socket = i;
			_WSAResetEvent( l_sockets[i].ev );
			return i;
		}
	}
	return -1;
}

static void tcp_start_listen( const int i )
{
	if( l_sockets[i].port ) {
		uint32 iface = l_sockets[i].iface;

		D(bug("[%d] binding to interface 0x%08X\r\n", i, iface));

		l_sockets[i].s = _socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if(l_sockets[i].s != INVALID_SOCKET) {
			struct sockaddr_in to;
			memset( &to, 0, sizeof(to) );
			to.sin_family = AF_INET;
			to.sin_port = htons( l_sockets[i].port );
			to.sin_addr.s_addr = htonl( iface );

			if(	_bind ( l_sockets[i].s, (const struct sockaddr *)&to, sizeof(to) ) == 0 )
			{
				D(bug("[%d] socket bound to port %d on interface 0x%08X\r\n", i, l_sockets[i].port, iface));
				if( _listen( l_sockets[i].s, SOMAXCONN ) == SOCKET_ERROR  ) {
					D(bug("[%d] listen() failed with error code %d\r\n", i, _WSAGetLastError()));
				} else {
					D(bug("[%d] listening to port %d\r\n", i, l_sockets[i].port));
					_WSAResetEvent( l_sockets[i].ev );
					if( SOCKET_ERROR == _WSAEventSelect( l_sockets[i].s, l_sockets[i].ev, FD_ACCEPT ) ) {
						D(bug("[%d] WSAEventSelect() failed with error code %d\r\n", i, _WSAGetLastError()));
					}
				}
			} else {
				D(bug("[%d] bind to port %d failed with error code %d\r\n", i, l_sockets[i].port, _WSAGetLastError()));
			}
		} else {
			D(bug("[%d] could not create a socket for port %d, error = %d\r\n", i, l_sockets[i].port, _WSAGetLastError()));
		}
	}
}

static void set_ttl( const int t, const uint8 ttl )
{
	int _ttl = ttl; // defensive programming, I know VCx

	if(_setsockopt( sockets[t].s, IPPROTO_IP, IP_TTL, (const char *)&_ttl, sizeof(int) ) == SOCKET_ERROR  ) {
		D(bug("<%d> could not set ttl to %d, error=%d\r\n", t, ttl, _WSAGetLastError()));
	} else {
		D(bug("<%d> ttl set to %d.\r\n", t, ttl));
	}
}

static void tcp_reply( const int flags, const int t )
{
	int tcp_size = sizeof(tcp_t);

	tcp_t *tcp = (tcp_t *)malloc( tcp_size );
	if(tcp) {
		memcpy( tcp->ip.mac.dest, ether_addr, 6 );
		memcpy( tcp->ip.mac.src, router_mac_addr, 6 );
		tcp->ip.mac.type = htons(mac_type_ip4);

		tcp->ip.version = 4;
		tcp->ip.header_len = 5;
		tcp->ip.tos = 0;
		tcp->ip.total_len = htons(tcp_size - sizeof(mac_t));
		tcp->ip.ident = htons(next_ip_ident_number++);
		tcp->ip.flags_n_frag_offset = 0;
		tcp->ip.ttl = 128;
		tcp->ip.proto = ip_proto_tcp;
		tcp->ip.src = htonl(sockets[t].ip_dest);
		tcp->ip.dest = htonl(sockets[t].ip_src);
		make_ip4_checksum( (ip_t *)tcp );

		D(bug("<%d> Reply: Seq=%d, Ack=%d\r\n", t, sockets[t].seq_out, sockets[t].seq_in));

		tcp->src_port = htons(sockets[t].dest_port);
		tcp->dest_port = htons(sockets[t].src_port);
		tcp->seq = htonl(sockets[t].seq_out);
		tcp->ack = htonl(sockets[t].seq_in);
		tcp->header_len = (uint8)( 20 << 2 );
		tcp->flags = flags;
		tcp->window = htons( sockets[t].our_window );
		tcp->urgent_ptr = 0;
		make_tcp_checksum( tcp, tcp_size );

		// dump_bytes( (uint8 *)tcp, tcp_size );

		enqueue_packet( (uint8 *)tcp, tcp_size );
		free(tcp);
	}
}

static bool has_mac_read_space( const int t )
{
	uint32 pending_bytes = sockets[t].seq_out - sockets[t].mac_ack;
	uint32 mac_can_accept_bytes = sockets[t].mac_window - pending_bytes;

	D(bug("<%d> mac_can_accept_bytes = %d\r\n", t, mac_can_accept_bytes));

	// Modified Nagle, effectively disabling window sliding (which I don't support anyway):
	return pending_bytes == 0;

	// Use more of window bandwidth
	// Enabling this would require that the buffers seq numbers are stored somewhere
	// return mac_can_accept_bytes >= sockets[t].buffers_read[0].len;
}

static bool b_recfrom( const int t )
{
	bool result;

	if( !has_mac_read_space(t) ) {
		D(bug("<%d> read stalled, mac cannot accept any more data\r\n", t));

		sockets[t].stream_to_mac_stalled_until = GetTickCount() + sockets[t].resend_timeout;
		return true;
	}

	int ret = _WSARecv(
		sockets[t].s,
		sockets[t].buffers_read,
		sockets[t].buffer_count_read,
		&sockets[t].bytes_received,
		&sockets[t].flags_read,
		&sockets[t].overlapped_read,
		tcp_read_completion
	);

	if(ret == SOCKET_ERROR) {
		int socket_error = _WSAGetLastError();
		if(socket_error == WSA_IO_PENDING) {
			D(bug("<%d> WSARecv() i/o pending\r\n", t));
			result = true;
		} else {
			D(bug("<%d> WSARecv() returned error %d\r\n", t, socket_error));
			result = false;
		}
	} else /*if(ret == 0) */ {
		D(bug("<%d> WSARecv() ok\r\n", t));
		// Completion routine call is already scheduled.
		result = true;
	}
	return result;
}

static bool b_send( const int t )
{
	int ret = _WSASend(
		sockets[t].s,
		sockets[t].buffers_write,
		sockets[t].buffer_count_write,
		&sockets[t].bytes_written,
		sockets[t].flags_write,
		&sockets[t].overlapped_write,
		tcp_write_completion
	);

	bool result;
	if(ret == SOCKET_ERROR) {
		int socket_error = _WSAGetLastError();
		if(socket_error == WSA_IO_PENDING) {
			D(bug("<%d> WSASend() i/o pending\r\n", t));
			result = true;
		} else {
			D(bug("<%d> WSASend() returned %d\r\n", t, socket_error));
			result = false;
		}
	} else /*if(ret == 0) */ {
		D(bug("<%d> WSASend() ok\r\n", t));
		// Completion routine call is already scheduled.
		result = true;
	}
	return result;
}

static void send_buffer( const int t, const bool resending )
{
	if(resending) {
		if(sockets[t].last_seq_out == 0) {
			D(bug("<%d> resend failure\r\n", t ));
			return;
		}
		sockets[t].seq_out = sockets[t].last_seq_out;
	} else {
		sockets[t].last_seq_out = sockets[t].seq_out;
	}

	D(bug("<%d> %s data to Mac: Seq=%d, Ack=%d\r\n", t, (resending ? "resending" : "sending"), sockets[t].seq_out, sockets[t].seq_in));

	uint32 bytes_read = sockets[t].bytes_received;

	if( sockets[t].mac_mss && bytes_read > sockets[t].mac_mss ) {
		D(bug("<%d> impossible: %d bytes to send, Mac mss is only %d\r\n", t, sockets[t].mac_mss && bytes_read, sockets[t].mac_mss));
	}

	int tcp_size = sizeof(tcp_t) + bytes_read;

	tcp_t *tcp = (tcp_t *)malloc( tcp_size );
	if(tcp) {
		// Build MAC
		// memcpy( tcp->ip.mac.dest, sockets[t].mac_src, 6 );
		memcpy( tcp->ip.mac.dest, ether_addr, 6 );
		memcpy( tcp->ip.mac.src, router_mac_addr, 6 );
		tcp->ip.mac.type = htons(mac_type_ip4);

		// Build IP
		tcp->ip.version = 4;
		tcp->ip.header_len = 5;
		tcp->ip.tos = 0;
		tcp->ip.total_len = htons(sizeof(tcp_t) - sizeof(mac_t) + bytes_read); // no options
		tcp->ip.ident = htons(next_ip_ident_number++);
		tcp->ip.flags_n_frag_offset = 0;
		tcp->ip.ttl = 128; // one hop actually!
		tcp->ip.proto = ip_proto_tcp;
		tcp->ip.src = htonl(sockets[t].ip_dest);
		tcp->ip.dest = htonl(sockets[t].ip_src);
		make_ip4_checksum( (ip_t *)tcp );

		// Copy payload (used by tcp checksum)
		memcpy( (char *)tcp + sizeof(tcp_t), sockets[t].buffers_read[0].buf, bytes_read );

		// Build tcp
		tcp->src_port = htons(sockets[t].dest_port);
		tcp->dest_port = htons(sockets[t].src_port);

		tcp->seq = htonl(sockets[t].seq_out);
		tcp->ack = htonl(sockets[t].seq_in);

		tcp->header_len = (uint8)( 20 << 2 );
#if PUSH_ALWAYS
		tcp->flags = ACK|PSH;
#else
		tcp->flags = (bytes_read == MAX_SEGMENT_SIZE) ? ACK : (ACK|PSH);
#endif
		tcp->window = htons( sockets[t].our_window );
		tcp->urgent_ptr = 0;
		make_tcp_checksum( tcp, tcp_size );

		sockets[t].seq_out += bytes_read;

		// dump_bytes( (uint8 *)tcp, tcp_size );

		enqueue_packet( (uint8 *)tcp, tcp_size );
		free(tcp);
	}
}

static void CALLBACK tcp_read_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
)
{
	EnterCriticalSection( &tcp_section );

	const int t = (int)lpOverlapped->hEvent;

	sockets[t].bytes_received = bytes_read;

  D(bug("<%d> tcp_read_completion(error=%d, bytes_read=%d)\r\n", t, error, bytes_read));

  D(bug("<%d> tcp_read_completion() start, old state = %s\r\n", t, STATENAME(sockets[t].state)));

	if(!sockets[t].in_use) {
	  D(bug("<%d> ignoring canceled read\r\n", t));
	} else {
		if( error != 0 ) {
		  D(bug("<%d> resetting after read error\r\n", t));
			tcp_reply( RST, t );
			free_socket(t);
		} else {
			if(bytes_read == 0) {
				_closesocket( sockets[t].s );
				sockets[t].s = INVALID_SOCKET;
			} else if( bytes_read > 0) {
				send_buffer( t, false );
			}

			switch( sockets[t].state ) {
				case SYN_RCVD:
					if( bytes_read == 0 ) {
						D(bug("<%d> Closing: SYN_RCVD -> FINWAIT_1\r\n", t));
						tcp_reply( ACK|FIN, t );
						sockets[t].seq_out++;
						sockets[t].state = FINWAIT_1;
					}
					break;
				case ESTABLISHED:
					if( bytes_read == 0 ) {
						D(bug("<%d> Closing: ESTABLISHED -> FINWAIT_1\r\n", t));
						tcp_reply( ACK|FIN, t );
						sockets[t].seq_out++;
						sockets[t].state = FINWAIT_1;
					} 
					break;
				case LISTEN:
					tcp_reply( SYN, t );
					sockets[t].seq_out++;
					sockets[t].state = SYN_SENT;
					sockets[t].time_wait = GetTickCount() + SYN_FLOOD_PROTECTION_TIMEOUT;
					D(bug("<%d> LISTEN -> SYN_SENT\r\n", t));
					break;
				case CLOSE_WAIT:
					if( bytes_read == 0) {
						tcp_reply( ACK|FIN, t );
						sockets[t].seq_out++;
						sockets[t].state = LAST_ACK;
						D(bug("<%d> Closing: CLOSE_WAIT -> LAST_ACK\r\n", t));
						if(sockets[t].remote_closed) {
							// Just in case that mac gets out of sync.
							_closesocket(sockets[t].s);
							sockets[t].s = INVALID_SOCKET;
						}
					}
					break;
				default:
					break;
			}

			if(!is_router_shutting_down && sockets[t].s != INVALID_SOCKET) {
				if(sockets[t].state != LISTEN) {
					b_recfrom(t);
				}
			}
		}
	}

	LeaveCriticalSection( &tcp_section );
}

static void CALLBACK tcp_write_completion(
	DWORD error,
	DWORD bytes_written,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
)
{
	EnterCriticalSection( &tcp_section );

	const int t = (int)lpOverlapped->hEvent;

	sockets[t].bytes_written = bytes_written;
	sockets[t].bytes_remaining_to_send -= bytes_written;

  D(bug("<%d> tcp_write_completion(error=%d, bytes_written=%d)\r\n", t, error, bytes_written));

	if(!sockets[t].in_use) {
	  D(bug("<%d> ignoring canceled write\r\n", t));
	} else {
		if(is_router_shutting_down || sockets[t].s == INVALID_SOCKET) {
		  D(bug("<%d> is not alive for sending.\r\n", t));
		} else {
			if( sockets[t].bytes_remaining_to_send <= 0 ) {
				D(bug("<%d> all data sent, accepting some more.\r\n", t));
				sockets[t].seq_in += sockets[t].bytes_to_send;
				sockets[t].bytes_to_send = sockets[t].bytes_remaining_to_send = 0; // superfluous
				tcp_reply( ACK, t );
				sockets[t].accept_more_data_from_mac = true;
			} else {
				D(bug("<%d> %d bytes (of %d total) remaining, sending.\r\n", t, sockets[t].bytes_remaining_to_send, sockets[t].bytes_to_send));
				sockets[t].buffers_write[0].len = sockets[t].bytes_remaining_to_send;
				char *p = sockets[t].buffers_write[0].buf;
				memmove( p, &p[bytes_written], sockets[t].bytes_remaining_to_send );
				if(!b_send(t)) {
				} else {
				}
			}
		}
	}

	LeaveCriticalSection( &tcp_section );
}

static void tcp_connect_callback( const int t )
{
  D(bug("<%d> tcp_connect_callback() start, old state = %s\r\n", t, STATENAME(sockets[t].state)));

	switch( sockets[t].state ) {
		case LISTEN:
			tcp_reply( SYN|ACK, t );
			sockets[t].seq_out++;
			sockets[t].state = SYN_RCVD;
		  D(bug("<%d> Connect: LISTEN -> SYN_RCVD\r\n", t));
			break;
		default:
			break;
	}
  D(bug("<%d> tcp_connect_callback() end, new state = %s\r\n", t, STATENAME(sockets[t].state)));
}

static void tcp_accept_callback( const int lst )
{
  D(bug("[%d] tcp_accept_callback()\r\n", lst));

	struct sockaddr_in to;
	memset( &to, 0, sizeof(to) );
	to.sin_family = AF_INET;
	int tolen = sizeof(to);

	SOCKET s = _accept( l_sockets[lst].s, (struct sockaddr *)&to, &tolen );
	if( s == INVALID_SOCKET ) {
		D(bug("[%d] connection not accepted, error code %d\r\n", lst, _WSAGetLastError()));
	} else {
		_WSAEventSelect( s, 0, 0 );

		uint16 src_port = l_sockets[lst].port;
		uint16 dest_port = ntohs(to.sin_port);
		uint32 ip_dest = ntohl(to.sin_addr.s_addr);

	  D(bug("[%d] connection accepted, local port:%d, remote %s:%d\r\n", lst, src_port, _inet_ntoa(to.sin_addr), dest_port));

		if( l_sockets[lst].ip != 0 && l_sockets[lst].ip != ip_dest ) {
			_closesocket( s );
		  D(bug("[%d] authorization failure. connection closed.\r\n", lst ));
		} else {
			int t = alloc_new_socket( src_port, dest_port, ip_dest );
			if( t < 0 ) {
				D(bug("<%d> out of slot space, connection dropped\r\n", t ));
				free_socket(t);
			} else {
				sockets[t].s = s;
				sockets[t].state = LISTEN;
				sockets[t].src_port = src_port;
				sockets[t].dest_port = dest_port;
				sockets[t].ip_src = macos_ip_address;
				sockets[t].ip_dest = ip_dest;

				sockets[t].seq_out = 0x00000001;
				sockets[t].seq_in = 0; // not known yet
				sockets[t].mac_ack = sockets[t].seq_out; // zero out pending bytes

				tcp_reply( SYN, t );
				sockets[t].seq_out++;
				sockets[t].state = SYN_SENT;
				sockets[t].time_wait = GetTickCount() + SYN_FLOOD_PROTECTION_TIMEOUT;
				D(bug("<%d> Connect: LISTEN -> SYN_SENT\r\n", t));

				_WSAResetEvent( sockets[t].ev );
				if( SOCKET_ERROR == _WSAEventSelect( sockets[t].s, sockets[t].ev, FD_CLOSE ) ) {
					D(bug("<%d> WSAEventSelect() failed with error code %d\r\n", t, _WSAGetLastError()));
				}

				// No data from the remote host is needed until the connection is established.
				// So don't initiate read yet.
			}
		}
	}
}

/*
	MSS is the only option I care about, and since I'm on ethernet
	I already pretty much know everything needed.

	AFAIK window scaling is not in effect unless both parties specify it,
	and I'm not doing it.
*/
static void process_options( const int t, const uint8 *opt, int len, uint32 &mss )
{
	mss = 0;

	while( len > 0 ) {
		switch( *opt ) {
			case 0:		// End of Option List
				D(bug("<%d> End of Option List\r\n", t));
				len = 0;
				break;
			case 1:		// No-Operation
				D(bug("<%d> No-Operation\r\n", t));
				len--;
				opt++;
				break;
			case 2:		// Maximum Segment Size
				{
					mss = ntohs( *((uint16 *)&opt[2]) );
					D(bug("<%d> Maximum Segment Size = %d\r\n", t, mss));
					len -= 4;
					opt += 4;
				}
				break;
			case 3:		// Window Scale
				{
					int wscale = opt[2];
					D(bug("<%d> Window Scale = %d\r\n", t, (int)wscale));
					len -= 3;
					opt += 3;
				}
				break;
			case 4:		// Sack-Permitted
				D(bug("<%d> Sack-Permitted option is set\r\n", t));
				len -= 2;
				opt += 2;
				break;
			case 5:		// Sack
				{
					int sack_len = opt[1];
					int hf = (sack_len-2) / 4;
					D(bug("<%d> Sack, %d half-blocks\r\n", t, hf));
					len -= sack_len;
					opt += sack_len;
				}
				break;
			case 8:		// Time Stamps
				{
					int valve = ntohl( *((uint32 *)&opt[2]) );
					int ereply = ntohl( *((uint32 *)&opt[6]) );
					D(bug("<%d> Time Stamps, TS valve = 0x%X, TS echo reply = 0x%X\r\n", t, valve, ereply));
					len -= 10;
					opt += 10;
				}
				break;
			default:
				D(bug("<%d> Unknown tcp header option 0x%02x, breaking out\r\n", t, (int)*opt));
				len = 0;
				break;
		}
	}
}

void write_tcp( tcp_t *tcp, int len )
{
	if(len < sizeof(tcp_t)) {
	  D(bug("<%d> Too small tcp packet(%d) on unknown slot, dropped\r\n", -1, len));
		return;
	}
	uint16 src_port = ntohs(tcp->src_port);
	uint16 dest_port = ntohs(tcp->dest_port);

	BOOL ok = true;
	BOOL handle_data = false;
	BOOL initiate_read = false;

	EnterCriticalSection( &tcp_section );

	int t = find_socket( src_port, dest_port );

	if(t < 0) {
		t = alloc_new_socket( src_port, dest_port, ntohl(tcp->ip.dest) );
		ok = t >= 0;
	}

	if(ok) {
		D(bug("<%d> write_tcp %d bytes from port %d to port %d\r\n", t, len, src_port, dest_port));
	} else {
		D(bug("<%d> FAILED write_tcp %d bytes from port %d to port %d\r\n", t, len, src_port, dest_port));
	}

	if( ok && ISSET(tcp->flags,RST) ) {
		D(bug("<%d> RST set, resetting socket\r\n", t));
		if( sockets[t].s != INVALID_SOCKET ) {
			D(bug("<%d> doing an extra shutdown (ie4)\r\n", t));
			_shutdown( sockets[t].s, SD_BOTH );
		}
		free_socket( t );
		ok = false;
	}

	if(ok) {
		D(bug("<%d> State machine start = %s\r\n", t, STATENAME(sockets[t].state)));

		// always update receive window
		sockets[t].mac_window = ntohs(tcp->window);

		int header_len = tcp->header_len >> 2;
		int option_bytes = header_len - 20;
		char *data = (char *)tcp + sizeof(tcp_t) + option_bytes;
		int dlen = len - sizeof(tcp_t) - option_bytes;

		if( !ISSET(tcp->flags,ACK) ) {
			D(bug("<%d> ACK not set\r\n", t));
		}
		if( ISSET(tcp->flags,SYN) ) {
			D(bug("<%d> SYN set\r\n", t));

			// Note that some options are valid even if there is no SYN.
			// I don't care about those however.

			uint32 new_mss;
			process_options( t, (uint8 *)data - option_bytes, option_bytes, new_mss );
			if(new_mss) {
				sockets[t].mac_mss = (int)new_mss;
				if( new_mss < sockets[t].buffers_read[0].len ) {
					sockets[t].buffers_read[0].len = new_mss;
				}
				D(bug("<%d> Max segment size set to %d\r\n", t, new_mss));
			}
		}
		if( ISSET(tcp->flags,FIN) ) {
			D(bug("<%d> FIN set\r\n", t));
		}

		// The sequence number Mac expects to see next time.
		sockets[t].mac_ack = ntohl(tcp->ack);

		D(bug("<%d> From Mac: Seq=%d, Ack=%d, window=%d, router Seq=%d\r\n", t, ntohl(tcp->seq), sockets[t].mac_ack, sockets[t].mac_window, sockets[t].seq_out));

		if( sockets[t].stream_to_mac_stalled_until && 
				sockets[t].mac_ack == sockets[t].seq_out &&
				(sockets[t].state == ESTABLISHED || sockets[t].state == CLOSE_WAIT) )
		{
			if( has_mac_read_space(t) ) {
				initiate_read = true;
				sockets[t].stream_to_mac_stalled_until = 0;
				D(bug("<%d> read resumed, mac can accept more data\r\n", t));
			}
		}

		switch( sockets[t].state ) {
			case CLOSED:
				sockets[t].src_port = src_port;
				sockets[t].dest_port = dest_port;
				sockets[t].ip_src = ntohl(tcp->ip.src);
				sockets[t].ip_dest = ntohl(tcp->ip.dest);

				if( ISSET(tcp->flags,SYN) ) {

					sockets[t].seq_out = 0x00000001;
					sockets[t].seq_in = ntohl(tcp->seq) + 1;

					_WSAResetEvent( sockets[t].ev );
					if( SOCKET_ERROR == _WSAEventSelect( sockets[t].s, sockets[t].ev, FD_CONNECT | FD_CLOSE ) ) {
						D(bug("<%d> WSAEventSelect() failed with error code %d\r\n", t, _WSAGetLastError()));
					}

				  D(bug("<%d> connecting local port %d to remote %s:%d\r\n", t, src_port, _inet_ntoa(sockets[t].from.sin_addr), dest_port));

					sockets[t].state = LISTEN;
					if( _WSAConnect(
						sockets[t].s,
						(const struct sockaddr *)&sockets[t].from,
						sockets[t].from_len,
						NULL, NULL,
						NULL, NULL
					) == SOCKET_ERROR )
					{
						int connect_error = _WSAGetLastError();
						if( connect_error == WSAEWOULDBLOCK ) {
							D(bug("<%d> WSAConnect() i/o pending.\r\n", t));
						} else {
							D(bug("<%d> WSAConnect() failed with error %d.\r\n", t, connect_error));
						}
					} else {
						D(bug("<%d> WSAConnect() ok.\r\n", t));
					}
				} else {
					if( ISSET(tcp->flags,FIN) ) {
						D(bug("<%d> No SYN but FIN on a closed socket.\r\n", t));
						free_socket(t);
					} else {
						D(bug("<%d> No SYN on a closed socket. resetting.\r\n", t));
						free_socket(t);
					}
				}
				break;
			case LISTEN:
				// handled in connect callback
				break;
			case SYN_SENT:
				if( ISSET(tcp->flags,SYN) && ISSET(tcp->flags,ACK) ) {
					sockets[t].seq_in = ntohl(tcp->seq) + 1;
					tcp_reply( ACK, t );
					sockets[t].state = ESTABLISHED;
					initiate_read = true;
					sockets[t].accept_more_data_from_mac = true;
					sockets[t].time_wait = 0;
				} else if( ISSET(tcp->flags,SYN) ) {
					sockets[t].seq_in = ntohl(tcp->seq) + 1;
					tcp_reply( ACK|SYN, t );
					sockets[t].seq_out++;
					sockets[t].state = SYN_RCVD;
					sockets[t].time_wait = 0;
				} else if( ISSET(tcp->flags,ACK) ) {
					// What was the bright idea here.
					D(bug("<%d> State is SYN_SENT, but got only ACK from Mac??\r\n", t));
					sockets[t].state = FINWAIT_2;
					sockets[t].time_wait = 0;
				}
				break;
			case SYN_RCVD:
				if( ISSET(tcp->flags,ACK) ) {
					sockets[t].state = ESTABLISHED;
					handle_data = true;
					initiate_read = true;
					sockets[t].accept_more_data_from_mac = true;
				}
				break;
			case ESTABLISHED:
				if( ISSET(tcp->flags,FIN) ) {
					sockets[t].seq_in++;
					tcp_reply( ACK, t );
					_shutdown( sockets[t].s, SD_SEND );
					sockets[t].state = CLOSE_WAIT;
				}
				handle_data = true;
				break;
			case CLOSE_WAIT:
				// handled in tcp_read_completion
				break;
			case LAST_ACK:
				if( ISSET(tcp->flags,ACK) ) {
					D(bug("<%d> LAST_ACK received, socket closed\r\n", t));
					free_socket( t );
				}
				break;
			case FINWAIT_1:
				if( ISSET(tcp->flags,FIN) && ISSET(tcp->flags,ACK) ) {
					sockets[t].seq_in++;
					tcp_reply( ACK, t );
					if(sockets[t].remote_closed) {
						_closesocket(sockets[t].s);
						sockets[t].s = INVALID_SOCKET;
					} else {
						_shutdown( sockets[t].s, SD_SEND );
					}
					sockets[t].state = TIME_WAIT;
					sockets[t].time_wait = GetTickCount() + 2 * sockets[t].msl;
				} else if( ISSET(tcp->flags,FIN) ) {
					sockets[t].seq_in++;
					tcp_reply( ACK, t );
					if(sockets[t].remote_closed) {
						_closesocket(sockets[t].s);
						sockets[t].s = INVALID_SOCKET;
					} else {
						_shutdown( sockets[t].s, SD_SEND );
					}
					sockets[t].state = CLOSING;
				} else if( ISSET(tcp->flags,ACK) ) {
					sockets[t].state = FINWAIT_2;
				}
				break;
			case FINWAIT_2:
				if( ISSET(tcp->flags,FIN) ) {
					sockets[t].seq_in++;
					tcp_reply( ACK, t );
					if(sockets[t].remote_closed) {
						_closesocket(sockets[t].s);
						sockets[t].s = INVALID_SOCKET;
					} else {
						_shutdown( sockets[t].s, SD_SEND );
					}
					sockets[t].state = TIME_WAIT;
					sockets[t].time_wait = GetTickCount() + 2 * sockets[t].msl;
				}
				break;
			case CLOSING:
				if( ISSET(tcp->flags,ACK) ) {
					sockets[t].state = TIME_WAIT;
					sockets[t].time_wait = GetTickCount() + 2 * sockets[t].msl;
				}
				break;
			case TIME_WAIT:
				// Catching stray packets: wait MSL * 2 seconds, -> CLOSED
				// Timer already set since we might not get here at all.
				// I'm using exceptionally low MSL value (5 secs).
				D(bug("<%d> time wait, datagram discarded\r\n", t));
				break;
		}

		// The "t" descriptor may already be freed. However, it's safe
		// to peek the state value inside the critical section.
		D(bug("<%d> State machine end = %s\r\n", t, STATENAME(sockets[t].state)));

		D(bug("<%d> handle_data=%d, initiate_read=%d\r\n", t, handle_data, initiate_read));

		if( handle_data && dlen && sockets[t].accept_more_data_from_mac ) {
			if( sockets[t].seq_in != ntohl(tcp->seq) ) {
				D(bug("<%d> dropping duplicate datagram seq=%d, expected=%d\r\n", t, ntohl(tcp->seq), sockets[t].seq_in));
			} else {
				set_ttl( t, tcp->ip.ttl );

				struct sockaddr_in to;
				memset( &to, 0, sizeof(to) );
				to.sin_family = AF_INET;
				to.sin_port = tcp->dest_port;
				to.sin_addr.s_addr = tcp->ip.dest;

				D(bug("<%d> sending %d bytes to remote host\r\n", t, dlen));

				sockets[t].accept_more_data_from_mac = false;

				if( dlen > MAX_SEGMENT_SIZE ) {
					D(bug("<%d> IMPOSSIBLE: b_send() dropped %d bytes! \r\n", t, dlen-MAX_SEGMENT_SIZE));
					dlen = MAX_SEGMENT_SIZE;
				}

				memcpy( sockets[t].buffers_write[0].buf, data, dlen );

				sockets[t].buffers_write[0].len = dlen;
				sockets[t].bytes_remaining_to_send = dlen;
				sockets[t].bytes_to_send = dlen;

				bool send_now = false;
				if( ISSET(tcp->flags,PSH) ) {
					send_now = true;
				} else {
					// todo -- delayed send
					send_now = true;
				}
				
				if(send_now) {

					// Patch ftp server or client address if needed.

					int lst = 1;
					bool is_pasv;
					uint16 ftp_data_port = 0;

					if(ftp_is_ftp_port(sockets[t].src_port)) {
						// Local ftp server may be entering to passive mode.
						is_pasv = true;
						ftp_parse_port_command( 
							sockets[t].buffers_write[0].buf,
							dlen,
							ftp_data_port,
							is_pasv
						);
					} else if(ftp_is_ftp_port(sockets[t].dest_port)) {
						// Local ftp client may be using port command.
						is_pasv = false;
						ftp_parse_port_command( 
							sockets[t].buffers_write[0].buf,
							dlen,
							ftp_data_port,
							is_pasv
						);
					}

					if(ftp_data_port) {
						D(bug("<%d> ftp %s command detected, port %d\r\n", t, (is_pasv ? "SERVER PASV REPLY" : "CLIENT PORT"), ftp_data_port ));

						// Note: for security reasons, only allow incoming connection from sockets[t].ip_dest
						lst = alloc_listen_socket( ftp_data_port, sockets[t].ip_dest, 0/*iface*/, true );

						if(lst < 0) {
							D(bug("<%d> no more free slots\r\n", t));
						} else {
							// First start listening (need to know the local name later)
							tcp_start_listen( lst );

							// When t is closed, lst must be closed too.
							sockets[t].child = lst;
							l_sockets[lst].parent = t;

							// Find out the local name
							struct sockaddr_in name;
							int namelen = sizeof(name);
							memset( &name, 0, sizeof(name) );
							if( _getsockname( sockets[t].s, (struct sockaddr *)&name, &namelen ) == SOCKET_ERROR ) {
								D(bug("_getsockname() failed, error=%d\r\n", _WSAGetLastError() ));
							}

							ftp_modify_port_command( 
								sockets[t].buffers_write[0].buf,
								dlen,
								MAX_SEGMENT_SIZE,
								ntohl(name.sin_addr.s_addr),
								ftp_data_port,
								is_pasv
							);

							sockets[t].buffers_write[0].len = dlen;
							sockets[t].bytes_remaining_to_send = dlen;
							// Do not change "bytes_to_send" field as it is used for ack calculation
						}
					} // end of ftp patch

					if(!b_send(t)) {
						// on error, close the ftp data listening socket if one was created
						if(lst >= 0) {
							D(bug("[%d] closing listening port %d after write error\r\n", t, l_sockets[lst].port));
							_closesocket( l_sockets[lst].s );
							l_sockets[lst].s = INVALID_SOCKET;
							l_sockets[lst].port = 0;
							l_sockets[lst].ip = 0;
							l_sockets[lst].parent = -1;
							sockets[t].child = -1;
						}
					}
				}
			}
		}

		if(initiate_read) {
			if(!b_recfrom(t)) {
				// post icmp error message
			}
		}
	}

	LeaveCriticalSection( &tcp_section );
}

/*
	- Dispatch remote close and connect events.
	- Expire time-waits.
	- Handle resend timeouts.
*/
static unsigned int WINAPI tcp_connect_close_thread(void *arg)
{
	WSAEVENT wait_handles[MAX_SOCKETS];

	for( int i=0; i<MAX_SOCKETS; i++ ) {
		wait_handles[i] = sockets[i].ev;
	}

	while(!is_router_shutting_down) {
		DWORD ret = WaitForMultipleObjects(
			MAX_SOCKETS,
			wait_handles,
			FALSE,
			200
		);
		if(is_router_shutting_down) break;

		EnterCriticalSection( &tcp_section );
		if( ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + MAX_SOCKETS ) {
			const int t = ret - WAIT_OBJECT_0;

			D(bug("<%d> Event %d\r\n", t, ret));

			if(sockets[t].in_use) {
				WSANETWORKEVENTS what;

				if( _WSAEnumNetworkEvents( sockets[t].s, sockets[t].ev, &what ) != SOCKET_ERROR ) {
					if( what.lNetworkEvents & FD_CONNECT ) {
						if( what.iErrorCode[FD_CONNECT_BIT] == 0 ) {
							D(bug("<%d> Connect ok\r\n", t));
							tcp_connect_callback(t);
						} else {
							D(bug("<%d> Connect error=%d\r\n", t, what.iErrorCode[FD_CONNECT_BIT]));
							// Post icmp error
						}
					} else if( what.lNetworkEvents & FD_CLOSE ) {
						if( what.iErrorCode[FD_CLOSE_BIT] == 0 ) {
							D(bug("<%d> graceful close, state = %s\r\n", t, STATENAME(sockets[t].state)));
						} else {
							D(bug("<%d> abortive close, state = %s, code=%d\r\n", t, STATENAME(sockets[t].state), what.iErrorCode[FD_CLOSE_BIT]));
						}
						sockets[t].remote_closed = true;
					}
				} else {
					int err = _WSAGetLastError();
					if( err == WSAENOTSOCK ) {
						D(bug("<%d> WSAEnumNetworkEvents: socket is already closed\r\n", t));
					} else {
						D(bug("<%d> WSAEnumNetworkEvents failed with error code %d, freeing slot\r\n", t, err));
						free_socket( t );
					}
				}
			}
			_WSAResetEvent( sockets[t].ev );
		} else {
			static int interval = 5;
			if( !--interval ) {
				for( int i=0; i<MAX_SOCKETS; i++ ) {
					if(sockets[i].in_use) {
						DWORD tmw = sockets[i].time_wait;
						DWORD stl = sockets[i].stream_to_mac_stalled_until;
						if( tmw ) {
							if( GetTickCount() >= tmw ) {
								if( sockets[i].state == SYN_SENT ) {
									/*
										A very basic SYN flood protection. Note that watching
										SYN_SENT instead of SYN_RCVD, because the state codes are
										from the point of view of the Mac-Router interface, not Router-Remote.
									*/
									D(bug("<%d> SYN_SENT time-out expired\r\n", i));
								} else {
									D(bug("<%d> TIME_WAIT expired\r\n", i));
								}
								free_socket( i );
							}
						} else if( stl ) {
							if( sockets[i].state == ESTABLISHED ) {
								if( GetTickCount() >= stl ) {
									D(bug("<%d> RESEND timeout expired\r\n", i));
									sockets[i].stream_to_mac_stalled_until = GetTickCount() + sockets[i].resend_timeout;
									send_buffer( i, true );
								}
							} else {
								sockets[i].stream_to_mac_stalled_until = 0;
							}
						}
					}
				}
				interval = 5;
			}
		}
		LeaveCriticalSection( &tcp_section );
	}
	return 0;
}

static unsigned int WINAPI tcp_listen_thread(void *arg)
{
	WSAEVENT wait_handles[MAX_SOCKETS];

	for( int i=0; i<MAX_SOCKETS; i++ ) {
		wait_handles[i] = l_sockets[i].ev;
		tcp_start_listen( i );
	}

	while(!is_router_shutting_down) {
		DWORD ret = WaitForMultipleObjects(
			MAX_SOCKETS,
			wait_handles,
			FALSE,
			200
		);

		if(is_router_shutting_down) break;

		EnterCriticalSection( &tcp_section );
		if( ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + MAX_SOCKETS ) {
			const int lst = ret - WAIT_OBJECT_0;

			D(bug("[%d] connection attempt to port %d\r\n", lst, l_sockets[lst].port));

			WSANETWORKEVENTS what;

			if( _WSAEnumNetworkEvents( l_sockets[lst].s, l_sockets[lst].ev, &what ) != SOCKET_ERROR ) {
				if( what.lNetworkEvents & FD_ACCEPT ) {
					if( what.iErrorCode[FD_ACCEPT_BIT] == 0 ) {
						D(bug("[%d] Connect ok\r\n", lst));
						tcp_accept_callback(lst);
					} else {
						D(bug("[%d] Connect error=%d\r\n", lst, what.iErrorCode[FD_ACCEPT_BIT]));
						// Post icmp error
					}
				}
			}

			// close on errors too
			if(l_sockets[lst].once) {
				D(bug("[%d] once mode: closing listening socket on port %d\r\n", lst, l_sockets[lst].port));
				if( _closesocket( l_sockets[lst].s ) == SOCKET_ERROR ) {
					int err = _WSAGetLastError();
					D(bug("[%d] close error %d\r\n", lst, err));
				}

				l_sockets[lst].s = INVALID_SOCKET;
				l_sockets[lst].port = 0;
				l_sockets[lst].ip = 0;

				int t = l_sockets[lst].parent;
				if( t >= 0 ) {
					sockets[t].child = -1;
				}
				l_sockets[lst].parent = -1;
			}

			_WSAResetEvent( l_sockets[lst].ev );
		}
		LeaveCriticalSection( &tcp_section );
	}
	return 0;
}

/*
	tcp_port=<port> [,<interface to bind>]
	tcp_port=21,192.168.0.1
*/

static void init_tcp_listen_ports()
{
	int32 index = 0;
	const char *port_str;
	while ((port_str = PrefsFindString("tcp_port", index++)) != NULL) {
		uint32 iface = 0;
		const char *if_str = strchr(port_str,',');
		if(if_str) {
			if_str++;
			uint32 if_net = _inet_addr( if_str );
			if(if_net == INADDR_NONE) if_net = INADDR_ANY;
			iface = ntohl( if_net );
		}
		uint16 port = (uint16)strtoul( port_str, 0, 0 );
		if( port ) {
			uint32 ip = 0;
			bool once = false;
			alloc_listen_socket( port, ip, iface, once );
		}
	}
}

static HANDLE tcp_handle = 0;
static HANDLE tcp_l_handle = 0;

void init_tcp()
{
	InitializeCriticalSection( &tcp_section );

	for( int i=0; i<MAX_SOCKETS; i++ ) {
		memset( &sockets[i], 0, sizeof(tcp_socket_t) );
		sockets[i].s = INVALID_SOCKET;
		sockets[i].state = CLOSED;
		sockets[i].ev = _WSACreateEvent();
		sockets[i].child = -1;
	}

	for( int i=0; i<MAX_SOCKETS; i++ ) {
		memset( &l_sockets[i], 0, sizeof(tcp_listening_socket_t) );
		l_sockets[i].s = INVALID_SOCKET;
		l_sockets[i].ev = _WSACreateEvent();
		l_sockets[i].parent = -1;
		/*
		l_sockets[i].port = 0;
		l_sockets[i].ip = 0;
		l_sockets[i].iface = 0;
		l_sockets[i].once = false;
		*/
	}

	init_tcp_listen_ports();

	unsigned int tcp_tid;
	tcp_handle = (HANDLE)_beginthreadex( 0, 0, tcp_connect_close_thread, 0, 0, &tcp_tid );

	unsigned int tcp_l_tid;
	tcp_l_handle = (HANDLE)_beginthreadex( 0, 0, tcp_listen_thread, 0, 0, &tcp_l_tid );
}

void final_tcp()
{
  D(bug("closing all tcp sockets\r\n"));
	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(sockets[i].s != INVALID_SOCKET) {
		  D(bug("  closing socket %d\r\n", i));
		}
		free_socket( i );
		if(sockets[i].buffers_write[0].buf) {
			delete [] sockets[i].buffers_write[0].buf;
			sockets[i].buffers_write[0].buf = 0;
		}
		if(sockets[i].buffers_read[0].buf) {
			delete [] sockets[i].buffers_read[0].buf;
			sockets[i].buffers_read[0].buf = 0;
		}
	}

  D(bug("closing all tcp listening socket\r\n"));
	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(l_sockets[i].s != INVALID_SOCKET) {
		  D(bug("  closing listening socket %d\r\n", i));
			_closesocket( l_sockets[i].s );
			l_sockets[i].s = INVALID_SOCKET;
		}
	}

	// The router module has already set the shutdown flag.
	WaitForSingleObject( tcp_handle, INFINITE );
	WaitForSingleObject( tcp_l_handle, INFINITE );

	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(sockets[i].ev != WSA_INVALID_EVENT) {
			_WSACloseEvent(sockets[i].ev);
			sockets[i].ev = WSA_INVALID_EVENT;
		}
	}
	for( int i=0; i<MAX_SOCKETS; i++ ) {
		if(l_sockets[i].ev != WSA_INVALID_EVENT) {
			_WSACloseEvent(l_sockets[i].ev);
			l_sockets[i].ev = WSA_INVALID_EVENT;
		}
	}

	DeleteCriticalSection( &tcp_section );
}
