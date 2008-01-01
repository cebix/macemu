/*
 *  dynsockets.h - ip router
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

#ifndef _DYNSOCKETS_H_
#define _DYNSOCKETS_H_

bool dynsockets_init(void);
void dynsockets_final(void);

extern int (WSAAPI *_WSAStartup) (WORD, LPWSADATA);
extern int (WSAAPI *_WSACleanup) (void);
extern int (WSAAPI *_gethostname) (char *, int);
extern char * (WSAAPI *_inet_ntoa) (struct in_addr);
extern struct hostent * (WSAAPI *_gethostbyname) (const char *);
extern int (WSAAPI *_send) (SOCKET, const char *, int, int);
extern int (WSAAPI *_sendto) (SOCKET, const char *, int, int, const struct sockaddr *, int);
extern int (WSAAPI *_recv) (SOCKET, char *, int, int);
extern int (WSAAPI *_recvfrom) (SOCKET, char *, int, int, struct sockaddr *, int *);
extern int (WSAAPI *_listen) (SOCKET, int);
extern SOCKET (WSAAPI *_accept) (SOCKET, struct sockaddr *, int *);
extern SOCKET (WSAAPI *_socket) (int, int, int);
extern int (WSAAPI *_bind) (SOCKET, const struct sockaddr *, int);
extern int (WSAAPI *_WSAAsyncSelect) (SOCKET, HWND, u_int, long);
extern int (WSAAPI *_closesocket) (SOCKET);
extern int (WSAAPI *_getsockname) (SOCKET, struct sockaddr *, int *);
extern int (WSAAPI *_WSARecvFrom) (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, struct sockaddr *, LPINT, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
extern int (WSAAPI *_WSAGetLastError) (void);
extern int (WSAAPI *_WSAConnect) (SOCKET, const struct sockaddr *, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS);
extern int (WSAAPI *_setsockopt) (SOCKET, int, int, const char *, int);
extern int (WSAAPI *_WSAEventSelect) (SOCKET, WSAEVENT, long);
extern WSAEVENT (WSAAPI *_WSACreateEvent) (void);
extern BOOL (WSAAPI *_WSACloseEvent) (WSAEVENT);
extern BOOL (WSAAPI *_WSAResetEvent) (WSAEVENT);
extern int (WSAAPI *_WSAEnumNetworkEvents) (SOCKET, WSAEVENT, LPWSANETWORKEVENTS);
extern int (WSAAPI *_shutdown) (SOCKET, int);
extern int (WSAAPI *_WSASend) (SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
extern int (WSAAPI *_WSARecv) (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
extern unsigned long (WSAAPI *_inet_addr) (const char *);

#endif // _DYNSOCKETS_H_
