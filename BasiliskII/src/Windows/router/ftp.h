/*
 *  ftp.h - ip router
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

#ifndef _FTP_H_
#define _FTP_H_

// Read the preferences.
void init_ftp();

// Compares against a list provided by the user.
bool ftp_is_ftp_port( uint16 port );

// Determine whether this is a ftp client PORT command or ftp server entering to passive mode.
void ftp_parse_port_command(
	char *buf,
	uint32 count,
	uint16 &ftp_data_port,
	bool is_pasv
);

// Build a new command using ip and port.
void ftp_modify_port_command( 
	char *buf,
	int &count,
	const uint32 max_size,
	const uint32 ip,
	const uint16 port,
	const bool is_pasv
);

#endif // _FTP_H_
