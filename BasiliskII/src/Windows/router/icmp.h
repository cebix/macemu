/*
 *  icmp.h - ip router
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

#ifndef _ICMP_H_
#define _ICMP_H_

void start_icmp_listen();
void stop_icmp_listen();

void write_icmp( icmp_t *icmp, int len );

void CALLBACK icmp_read_completion(
	DWORD error,
	DWORD bytes_read,
	LPWSAOVERLAPPED lpOverlapped,
	DWORD flags
);

#endif // _ICMP_H_
