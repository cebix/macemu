/*
 *  dynsockets.cpp - ip router
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
#include "dynsockets.h"
#include "dump.h"
#include "main.h"



#if DEBUG
#pragma optimize("",off)
#endif


#include "debug.h"


/*
	Cannot link statically to winsock. We need ws2, but there are
	Win95 b2 users who can't (or won't) upgrade.
*/

static LPCTSTR wslib = TEXT("WS2_32.DLL");

static HMODULE hWinsock32 = 0;
static WSADATA WSAData;

int (WSAAPI *_WSAStartup) (WORD, LPWSADATA) = 0;
int (WSAAPI *_WSACleanup) (void) = 0;
int (WSAAPI *_gethostname) (char *, int) = 0;
char * (WSAAPI *_inet_ntoa) (struct in_addr) = 0;
struct hostent * (WSAAPI *_gethostbyname) (const char *) = 0;
int (WSAAPI *_send) (SOCKET, const char *, int, int) = 0;
int (WSAAPI *_sendto) (SOCKET, const char *, int, int, const struct sockaddr *, int) = 0;
int (WSAAPI *_recv) (SOCKET, char *, int, int) = 0;
int (WSAAPI *_recvfrom) (SOCKET, char *, int, int, struct sockaddr *, int *) = 0;
int (WSAAPI *_listen) (SOCKET, int) = 0;
SOCKET (WSAAPI *_accept) (SOCKET, struct sockaddr *, int *) = 0;
SOCKET (WSAAPI *_socket) (int, int, int) = 0;
int (WSAAPI *_bind) (SOCKET, const struct sockaddr *, int) = 0;
int (WSAAPI *_WSAAsyncSelect) (SOCKET, HWND, u_int, long) = 0;
int (WSAAPI *_closesocket) (SOCKET) = 0;
int (WSAAPI *_getsockname) (SOCKET, struct sockaddr *, int *) = 0;
int (WSAAPI *_WSARecvFrom) (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, struct sockaddr *, LPINT, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE) = 0;
int (WSAAPI *_WSAGetLastError) (void) = 0;
int (WSAAPI *_WSAConnect) (SOCKET, const struct sockaddr *, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS) = 0;
int (WSAAPI *_setsockopt) (SOCKET, int, int, const char *, int) = 0;
int (WSAAPI *_WSAEventSelect) (SOCKET, WSAEVENT, long) = 0;
WSAEVENT (WSAAPI *_WSACreateEvent) (void) = 0;
BOOL (WSAAPI *_WSACloseEvent) (WSAEVENT) = 0;
BOOL (WSAAPI *_WSAResetEvent) (WSAEVENT) = 0;
int (WSAAPI *_WSAEnumNetworkEvents) (SOCKET, WSAEVENT, LPWSANETWORKEVENTS) = 0;
int (WSAAPI *_shutdown) (SOCKET, int) = 0;
int (WSAAPI *_WSASend) (SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE) = 0;
int (WSAAPI *_WSARecv) (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE) = 0;
unsigned long (WSAAPI *_inet_addr) (const char *) = 0;

static bool load_sockets()
{
	bool result = false;

	hWinsock32 = LoadLibrary( wslib );
	if(!hWinsock32) {
		ErrorAlert("Could not load Winsock libraries; router module is not available. Please install Windows sockets 2.");
	} else {
		_WSAStartup = (int (WSAAPI *)(WORD, LPWSADATA))GetProcAddress( hWinsock32, "WSAStartup" );
		_WSACleanup = (int (WSAAPI *)(void))GetProcAddress( hWinsock32, "WSACleanup" );
		_gethostname = (int (WSAAPI *)(char *, int))GetProcAddress( hWinsock32, "gethostname" );
		_inet_ntoa = (char * (WSAAPI *)(struct in_addr))GetProcAddress( hWinsock32, "inet_ntoa" );
		_gethostbyname = (struct hostent * (WSAAPI *)(const char *))GetProcAddress( hWinsock32, "gethostbyname" );
		_send = (int (WSAAPI *)(SOCKET, const char *, int, int))GetProcAddress( hWinsock32, "send" );
		_sendto = (int (WSAAPI *)(SOCKET, const char *, int, int, const struct sockaddr *, int))GetProcAddress( hWinsock32, "sendto" );
		_recv = (int (WSAAPI *)(SOCKET, char *, int, int))GetProcAddress( hWinsock32, "recv" );
		_recvfrom = (int (WSAAPI *)(SOCKET, char *, int, int, struct sockaddr *, int *))GetProcAddress( hWinsock32, "recvfrom" );
		_listen = (int (WSAAPI *)(SOCKET, int))GetProcAddress( hWinsock32, "listen" );
		_accept = (SOCKET (WSAAPI *)(SOCKET, struct sockaddr *, int *))GetProcAddress( hWinsock32, "accept" );
		_socket = (SOCKET (WSAAPI *)(int, int, int))GetProcAddress( hWinsock32, "socket" );
		_bind = (int (WSAAPI *)(SOCKET, const struct sockaddr *, int))GetProcAddress( hWinsock32, "bind" );
		_WSAAsyncSelect = (int (WSAAPI *)(SOCKET, HWND, u_int, long))GetProcAddress( hWinsock32, "WSAAsyncSelect" );
		_closesocket = (int (WSAAPI *)(SOCKET))GetProcAddress( hWinsock32, "closesocket" );
		_getsockname = (int (WSAAPI *)(SOCKET, struct sockaddr *, int *))GetProcAddress( hWinsock32, "getsockname" );
		_WSARecvFrom = (int (WSAAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, struct sockaddr *, LPINT, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE))GetProcAddress( hWinsock32, "WSARecvFrom" );
		_WSAGetLastError = (int (WSAAPI *)(void))GetProcAddress( hWinsock32, "WSAGetLastError" );
		_WSAConnect = (int (WSAAPI *)(SOCKET, const struct sockaddr *, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS))GetProcAddress( hWinsock32, "WSAConnect" );
		_setsockopt = (int (WSAAPI *)(SOCKET, int, int, const char *, int))GetProcAddress( hWinsock32, "setsockopt" );
		_WSAEventSelect = (int (WSAAPI *)(SOCKET, WSAEVENT, long))GetProcAddress( hWinsock32, "WSAEventSelect" );
		_WSACreateEvent = (WSAEVENT (WSAAPI *)(void))GetProcAddress( hWinsock32, "WSACreateEvent" );
		_WSACloseEvent = (BOOL (WSAAPI *)(WSAEVENT))GetProcAddress( hWinsock32, "WSACloseEvent" );
		_WSAResetEvent = (BOOL (WSAAPI *)(WSAEVENT))GetProcAddress( hWinsock32, "WSAResetEvent" );
		_WSAEnumNetworkEvents = (BOOL (WSAAPI *)(SOCKET, WSAEVENT, LPWSANETWORKEVENTS))GetProcAddress( hWinsock32, "WSAEnumNetworkEvents" );
 		_shutdown = (int (WSAAPI *)(SOCKET, int))GetProcAddress( hWinsock32, "shutdown" );
 		_WSASend = (int (WSAAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE))GetProcAddress( hWinsock32, "WSASend" );
 		_WSARecv = (int (WSAAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE))GetProcAddress( hWinsock32, "WSARecv" );
 		_inet_addr = (unsigned long (WSAAPI *)(const char *))GetProcAddress( hWinsock32, "inet_addr" );
 		
		if( _WSAStartup && _WSACleanup && _gethostname && _inet_ntoa && _gethostbyname &&
				_send && _sendto && _recv && _recvfrom && _listen && _accept && _socket && _bind &&
				_WSAAsyncSelect && _closesocket && _getsockname && _WSARecvFrom && _WSAGetLastError &&
				_WSAConnect && _setsockopt && _WSAEventSelect && _WSACreateEvent && _WSACloseEvent &&
				_WSAResetEvent && _WSAEnumNetworkEvents && _shutdown && _WSASend && _WSARecv && _inet_addr
		)
		{
			result = true;
		} else {
			ErrorAlert("Could not find required entry points; router module is not available. Please install Windows sockets 2.");
		}
	}

	return result;
}

bool dynsockets_init(void)
{
	bool result = false;
	if(load_sockets()) {
		if( (_WSAStartup(MAKEWORD(2,0), &WSAData)) != 0 ||
				LOBYTE( WSAData.wVersion ) != 2 ||
        HIBYTE( WSAData.wVersion ) != 0 )
		{
			ErrorAlert("Could not start Windows sockets version 2.");
		} else {
			result = true;
		}
	}
	return result;
}

void dynsockets_final(void)
{
	if(hWinsock32) {
		_WSACleanup();
		FreeLibrary( hWinsock32 );
		hWinsock32 = 0;
	}
	_WSAStartup = 0;
	_WSACleanup = 0;
	_gethostname = 0;
	_inet_ntoa = 0;
	_gethostbyname = 0;
	_send = 0;
	_sendto = 0;
	_recv = 0;
	_recvfrom = 0;
	_listen = 0;
	_accept = 0;
	_socket = 0;
	_bind = 0;
	_WSAAsyncSelect = 0;
	_closesocket = 0;
	_getsockname = 0;
	_WSARecvFrom = 0;
	_WSAGetLastError = 0;
	_WSAConnect = 0;
	_setsockopt = 0;
	_WSAEventSelect = 0;
	_WSACreateEvent = 0;
	_WSACloseEvent = 0;
	_WSAResetEvent = 0;
	_WSAEnumNetworkEvents = 0;
	_shutdown = 0;
	_WSASend = 0;
	_WSARecv = 0;
	_inet_addr = 0;
}
