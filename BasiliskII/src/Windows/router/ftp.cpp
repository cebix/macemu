/*
 *  ftp.cpp - ip router
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
#include <ctype.h>
#include "dump.h"
#include "prefs.h"
#include "ftp.h"

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"

static int m_ftp_port_count = 0;
#define MAX_FTP_PORTS 100
static uint16 m_ftp_ports[MAX_FTP_PORTS];

bool ftp_is_ftp_port( uint16 port )
{
	for( int i=0; i<m_ftp_port_count; i++ ) {
		if( m_ftp_ports[i] == port ) return true;
	}
	return false;
}

void init_ftp()
{
	const char *str = PrefsFindString("ftp_port_list");

	if(str) {
		char *ftp = new char [ strlen(str) + 1 ];
		if(ftp) {
			strcpy( ftp, str );
			char *p = ftp;
			while( p && *p ) {
				char *pp = strchr( p, ',' );
				if(pp) *pp++ = 0;
				if( m_ftp_port_count < MAX_FTP_PORTS ) {
					m_ftp_ports[m_ftp_port_count++] = (uint16)strtoul(p,0,0);
				}
				p = pp;
			}
			delete [] ftp;
		}
	}
}

void ftp_modify_port_command( 
	char *buf,
	int &count,
	const uint32 max_size,
	const uint32 ip,
	const uint16 port,
	const bool is_pasv
)
{
	if( max_size < 100 ) {
		// impossible
		return;
	}

	sprintf( 
		buf, 
		(is_pasv ? "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).%c%c" : "PORT %d,%d,%d,%d,%d,%d%c%c"),
		ip >> 24,
		(ip >> 16) & 0xFF,
		(ip >> 8) & 0xFF,
		ip & 0xFF,
		(port >> 8) & 0xFF,
		port & 0xFF,
		0x0d, 0x0a
	);

	count = strlen(buf);

	D(bug("ftp_modify_port_command: \"%s\"\r\n", buf ));
}

// this should be robust. rather skip it than do anything dangerous.
void ftp_parse_port_command(
	char *buf,
	uint32 count,
	uint16 &ftp_data_port,
	bool is_pasv
)
{
	ftp_data_port = 0;

	if( !count ) return;

	uint8 b[100];
	uint32 ftp_ip = 0;

	// make it a c-string
	if( count >= sizeof(b) ) count = sizeof(b)-1;
	memcpy( b, buf, count );
	b[ count ] = 0;

	for( uint32 i=0; i<count; i++ ) {
		if( b[i] < ' ' || b[i] > 'z' ) {
			b[i] = ' ';
		} else {
			b[i] = tolower(b[i]);
		}
	}

	// D(bug("FTP: \"%s\"\r\n", b ));

	char *s = (char *)b;

	while( *s == ' ' ) s++;

	if(is_pasv) {
		/*
		LOCAL SERVER: ..227 Entering Passive Mode (192,168,0,2,6,236). 0d 0a
		*/
		if( atoi(s) == 227 && strstr(s,"passive") ) {
			while( *s && *s != '(' ) s++;
			if( *s++ == 0 ) s = 0;
		} else {
			s = 0;
		}
	} else {
		/*
		LOCAL CLIENT: PORT 192,168,0,1,14,147 0d 0a
		*/
		if( strncmp(s,"port ",5) == 0 ) {
			s += 5;
		} else {
			s = 0;
		}
	}

	if(s && *s) {
		// get remote ip (used only for verification)
		for( uint32 i=0; i<4; i++ ) {
			while( *s == ' ' ) s++;
			if(!isdigit(*s)) {
				ftp_ip = 0;
				break;
			}
			ftp_ip = (ftp_ip << 8) + atoi(s);
			while( *s && *s != ',' ) s++;
			if(!*s) {
				ftp_ip = 0;
				break;
			}
			s++;
		}

		if(ftp_ip) {
			// get local port
			for( uint32 i=0; i<2; i++ ) {
				while( *s == ' ' ) s++;
				if(!isdigit(*s)) {
					ftp_data_port = 0;
					break;
				}
				ftp_data_port = (ftp_data_port << 8) + atoi(s);
				while( *s && *s != ',' && *s != ')' ) s++;
				if(!*s) 
					break; 
				else 
					s++;
			}
		}
	}
	if(ftp_data_port) {
		D(bug("ftp_parse_port_command: \"%s\"; port is %d\r\n", b, ftp_data_port ));
	}
}
