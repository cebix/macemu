/*
 *  debug.h - Debugging utilities
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifndef DEBUG_H
#define DEBUG_H

#if defined(WIN32) && !defined(__CYGWIN__)

// Windows debugging goes where it's supposed to go
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

static void _cdecl inline winbug( char *s, ...)
{
	va_list vargs;
	char msg[1024], date[50], hours[50];
	struct _timeb tstruct;

	_ftime( &tstruct );
	_strtime( hours );
	_strdate( date );
	sprintf( msg, "B2: %s %s:%03u ", date, hours, tstruct.millitm );
	
	va_start( vargs, s );
	vsprintf( &msg[strlen(msg)], s, vargs );
	va_end( vargs );

	OutputDebugString(msg);
}
#define bug winbug

#elif defined(AMIGA)

// Amiga debugging info goes to serial port (or sushi)
#ifdef __cplusplus
extern "C" {
#endif
extern void kprintf(const char *, ...);
#ifdef __cplusplus
}
#endif
#define bug kprintf

#else

// Other systems just print it to stdout
#include <stdio.h>
#define bug printf

#endif

#if DEBUG
#define D(x) (x);
#else
#define D(x) ;
#endif

#endif
