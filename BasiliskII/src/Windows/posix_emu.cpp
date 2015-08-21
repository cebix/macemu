/*
 *  posix_emu.cpp -- posix and virtual desktop
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

// TODO: UNC names. Customizable "Virtual Desktop" location.

#include "sysdeps.h"
#define NO_POSIX_API_HOOK
#include "posix_emu.h"
#include "user_strings.h"
#include "util_windows.h"
#include "main.h"
#include "extfs_defs.h"
#include "prefs.h"
#include <ctype.h>


#define DEBUG_EXTFS 0

#if DEBUG_EXTFS

// This must be always on.
#define DEBUG 1
#undef OutputDebugString
#define OutputDebugString extfs_log_write
extern void extfs_log_write( char *s );
#define EXTFS_LOG_FILE_NAME "extfs.log"
#include "debug.h"

enum {
	DB_EXTFS_NONE=0,
	DB_EXTFS_NORMAL,
	DB_EXTFS_LOUD
};
static int16 debug_extfs = DB_EXTFS_NONE;
static HANDLE extfs_log_file = INVALID_HANDLE_VALUE;

static void extfs_log_open( char *path )
{
	if(debug_extfs == DB_EXTFS_NONE) return;

	DeleteFile( path );
	extfs_log_file = CreateFile(
			path,
			GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			// FILE_FLAG_WRITE_THROUGH|FILE_FLAG_NO_BUFFERING,
			FILE_FLAG_WRITE_THROUGH,
			NULL
	);
	if( extfs_log_file == INVALID_HANDLE_VALUE ) {
		ErrorAlert( "Could not create the EXTFS log file." );
	}
}

static void extfs_log_close( void )
{
	if(debug_extfs == DB_EXTFS_NONE) return;

	if( extfs_log_file != INVALID_HANDLE_VALUE ) {
		CloseHandle( extfs_log_file );
		extfs_log_file = INVALID_HANDLE_VALUE;
	}
}

static void extfs_log_write( char *s )
{
	DWORD bytes_written;

	// should have been checked already.
	if(debug_extfs == DB_EXTFS_NONE) return;

	if( extfs_log_file != INVALID_HANDLE_VALUE ) {

		DWORD count = strlen(s);
		if (0 == WriteFile(extfs_log_file, s, count, &bytes_written, NULL) ||
				(int)bytes_written != count)
		{
			extfs_log_close();
			ErrorAlert( "extfs log file write error (out of disk space?). Log closed." );
		} else {
			FlushFileBuffers( extfs_log_file );
		}
	}
}
#else

#define DEBUG 0
#include "debug.h"

#endif // DEBUG_EXTFS

int my_errno = 0;

#define VIRTUAL_ROOT_ID ((HANDLE)0xFFFFFFFE)

static LPCTSTR desktop_name = TEXT("Virtual Desktop");
static const char *custom_icon_name = "Icon\r";
#define my_computer GetString(STR_EXTFS_VOLUME_NAME)

static TCHAR lb1[MAX_PATH_LENGTH];
static TCHAR lb2[MAX_PATH_LENGTH];

#define MRP(path) translate(path,lb1)
#define MRP2(path) translate(path,lb2)

#define DISABLE_ERRORS UINT prevmode = SetErrorMode(SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS)
#define RESTORE_ERRORS SetErrorMode(prevmode);

static TCHAR host_drive_list[512];
static TCHAR virtual_root[248]; // Not _MAX_PATH

const uint8 my_comp_icon[2670] = {
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0xD8, 0x00, 0x00, 0x08, 0xD8, 0x00, 0x00, 0x00, 0x96,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x79, 0x79, 0x79, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0xC0, 0xCC, 0xCC, 0xCC,
	0xCC, 0xD7, 0x97, 0x97, 0x97, 0x97, 0x97, 0xC0, 0xC0, 0xC0, 0xC0, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
	0xCD, 0xDB, 0xD9, 0x79, 0x79, 0x7E, 0x79, 0x0C, 0xDD, 0xCD, 0xDD, 0xCD, 0xCD, 0xDC, 0xDD, 0xCD,
	0xCC, 0xED, 0xED, 0x97, 0x97, 0x97, 0x97, 0x0C, 0xE7, 0x78, 0x77, 0x97, 0x97, 0x97, 0x97, 0x97,
	0xDC, 0xED, 0xDE, 0x79, 0x79, 0x79, 0x99, 0x0C, 0xD9, 0x7E, 0x5E, 0x65, 0x5E, 0x65, 0xD9, 0x79,
	0xCD, 0xDE, 0xDD, 0x97, 0xE7, 0x9E, 0x77, 0xC0, 0x97, 0x9D, 0xCD, 0xCC, 0xC7, 0xCC, 0xE7, 0x97,
	0xCC, 0xED, 0xEE, 0x79, 0x79, 0x79, 0x7E, 0xCC, 0x57, 0xD5, 0xD7, 0xD5, 0xDD, 0x5D, 0xD9, 0x7E,
	0xCD, 0xDE, 0xDE, 0x79, 0x97, 0x97, 0x99, 0x0C, 0x87, 0xCD, 0x75, 0xC7, 0x5C, 0x7D, 0xD9, 0x79,
	0xCD, 0xDD, 0xED, 0xE7, 0x7E, 0x79, 0x77, 0xCC, 0xE7, 0xB0, 0x00, 0xC0, 0x0C, 0xCD, 0xE7, 0x97,
	0xDC, 0xED, 0xEE, 0x79, 0x97, 0x86, 0x79, 0xC0, 0xE7, 0xD0, 0x2C, 0xC1, 0xC2, 0xCD, 0xD9, 0x79,
	0xCD, 0xDE, 0xDD, 0x97, 0x99, 0x79, 0x97, 0x0C, 0xE7, 0xB0, 0xD0, 0xDC, 0xCC, 0xCD, 0xD6, 0x87,
	0xDD, 0xDE, 0xED, 0x79, 0x77, 0xE7, 0x79, 0x0C, 0x58, 0xDC, 0x0C, 0x0C, 0xCC, 0xCD, 0xE9, 0x79,
	0xCD, 0xDD, 0xD5, 0x99, 0x97, 0x99, 0x79, 0xC0, 0x87, 0xD0, 0xC0, 0xC0, 0xC0, 0xCD, 0xD7, 0xE7,
	0xDD, 0xDE, 0xD7, 0x97, 0x79, 0x77, 0xE7, 0x0C, 0xE7, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0x79, 0x79,
	0xCD, 0xDE, 0xD9, 0x79, 0x97, 0xE9, 0x79, 0x0C, 0x97, 0x79, 0x79, 0x79, 0x79, 0x79, 0x97, 0x97,
	0xDC, 0xED, 0xE7, 0x97, 0x79, 0x97, 0x97, 0x0C, 0xCD, 0xD7, 0xD7, 0xD7, 0xE7, 0xE7, 0x7E, 0x79,
	0xCD, 0xDE, 0x79, 0x79, 0x97, 0x7E, 0x79, 0xC0, 0xCC, 0xCC, 0x0C, 0xCC, 0x0D, 0xCC, 0xDC, 0xDC,
	0xDC, 0xED, 0x97, 0x97, 0x77, 0x99, 0x79, 0xCC, 0xCC, 0xCC, 0xDC, 0xCC, 0xDC, 0xCC, 0xCC, 0x8D,
	0xCD, 0xDE, 0x79, 0x79, 0x96, 0x77, 0x97, 0x97, 0x97, 0x90, 0xCC, 0xCD, 0xCD, 0xDD, 0xDD, 0xCC,
	0xDD, 0xD9, 0x76, 0x87, 0x97, 0x99, 0x7E, 0x7C, 0x0C, 0xCC, 0xDD, 0xDD, 0xED, 0xDE, 0xDD, 0xEE,
	0xDE, 0xD5, 0xBD, 0xDE, 0x79, 0x79, 0x9C, 0xC0, 0xCC, 0xDD, 0xDD, 0xDD, 0xDE, 0xDD, 0xED, 0xDE,
	0xDE, 0xDD, 0xDE, 0xDE, 0x79, 0x79, 0x70, 0xCD, 0xCC, 0xCC, 0xCC, 0xCC, 0xDC, 0xDD, 0xDD, 0xDD,
	0xDD, 0xDD, 0xED, 0xED, 0x97, 0x97, 0x90, 0xCC, 0x8D, 0xCC, 0xDC, 0xCD, 0xCC, 0xCC, 0xCC, 0xCC,
	0xCC, 0xEE, 0xDE, 0xDE, 0x79, 0x7E, 0x70, 0xCC, 0x88, 0xDC, 0xCC, 0xCC, 0xCD, 0xDD, 0xDD, 0xDC,
	0xCD, 0xDD, 0xED, 0xED, 0x97, 0x97, 0xEC, 0xCC, 0xCC, 0xCC, 0xDC, 0xCC, 0xCD, 0xDD, 0xED, 0xDD,
	0xDC, 0xED, 0xED, 0xEE, 0x79, 0x79, 0xDC, 0x0D, 0xCC, 0xDC, 0xCC, 0xCD, 0xCC, 0xCC, 0xCC, 0x0C,
	0xDC, 0xDE, 0xDE, 0xED, 0x97, 0xDC, 0xCC, 0xDC, 0xCD, 0xCC, 0xDC, 0xCD, 0xCC, 0xCC, 0xCD, 0xCC,
	0xCC, 0xED, 0xED, 0x79, 0xDD, 0xC0, 0xCD, 0xCC, 0xDC, 0xCD, 0xCC, 0xDC, 0xCC, 0xDC, 0xDD, 0xCD,
	0xCD, 0xED, 0x97, 0x97, 0xDD, 0xCC, 0xCC, 0x00, 0xC0, 0xDD, 0xCD, 0xCC, 0xCC, 0xCD, 0xD0, 0xDC,
	0xDD, 0xF7, 0x99, 0x79, 0x97, 0x9D, 0xDD, 0xDD, 0xCC, 0xC0, 0xCC, 0x0C, 0xDC, 0xDC, 0xCD, 0xCD,
	0xDF, 0x79, 0x77, 0x97, 0x79, 0x79, 0x79, 0x79, 0xDD, 0xDE, 0xDC, 0xCC, 0xCC, 0xC0, 0xC0, 0xDD,
	0xE9, 0x79, 0x97, 0x99, 0x97, 0xE7, 0xE7, 0x97, 0x97, 0x9D, 0x79, 0xDD, 0xDD, 0xDD, 0xCD, 0xDE,
	0x79, 0x79, 0x7E, 0x77, 0x00, 0x00, 0x04, 0x00, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0xF5,
	0xF5, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B,
	0x2B, 0x2B, 0xF9, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0xF6,
	0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF7, 0xF7, 0xF7,
	0xF7, 0xF8, 0x81, 0xFA, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56,
	0xF7, 0xF8, 0x81, 0x81, 0x81, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xF8, 0xF8, 0x81, 0xFA, 0xFB, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xC2, 0xFB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xFB, 0xC2, 0xC2, 0xC2,
	0xF7, 0xF8, 0x81, 0x81, 0xFB, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0x2B,
	0xA5, 0xC2, 0xC2, 0xFB, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x81, 0xC2, 0xC2, 0xC2,
	0xF7, 0xF8, 0x81, 0x81, 0xFB, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0x2B,
	0xA5, 0xC2, 0xF9, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF9, 0xFB, 0xC2, 0xC2, 0xC2,
	0xF7, 0xF8, 0x81, 0x81, 0xFB, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xF9, 0x81, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0xFA, 0x81, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0x2B, 0xF9, 0xFB, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0xFB, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF5, 0x0A, 0xF6, 0x2B, 0x0A, 0xF6, 0x0A, 0x2B, 0xF9, 0x81, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF5, 0xF8, 0xF6, 0x56, 0xF7, 0xF7, 0xF8, 0x2B, 0xF9, 0x81, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0xFB, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6, 0x2B, 0x2B, 0xF9, 0xFB, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0xFA, 0x81, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF6, 0xF6, 0xF5, 0xF6, 0xF6, 0xF6, 0xF6, 0x2B, 0xF9, 0x81, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xC2, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xA5, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0xF7, 0x56, 0xF8, 0x7A, 0x7A, 0x9E, 0x9E, 0x9E, 0x9E, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xF8, 0x56, 0x81, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B,
	0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF7, 0xF7, 0xF8, 0xF8,
	0xF8, 0x56, 0x81, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF7, 0xF7,
	0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0x56, 0xB9, 0xF8,
	0xF8, 0x56, 0x81, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xC2, 0xC2, 0xC2, 0xF6, 0x2B, 0x2B, 0xF7, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0xF8,
	0xF8, 0x56, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6,
	0xF6, 0xF6, 0x2B, 0xF8, 0x56, 0xFA, 0xF9, 0x81, 0x81, 0x81, 0xFA, 0x81, 0x81, 0x81, 0xFB, 0x81,
	0xFB, 0xFB, 0xFB, 0x81, 0xFA, 0xFA, 0xFA, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xF6, 0xF6,
	0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFA, 0xFA, 0xFA, 0xFA, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFB,
	0x81, 0xFB, 0xF9, 0xFA, 0xFA, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B, 0xF7,
	0xF8, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF8, 0xF8, 0x56, 0x56, 0xF9, 0xF9, 0xF9, 0xFA,
	0xFA, 0xFA, 0xFA, 0x81, 0x81, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B, 0xF7,
	0x93, 0xA0, 0xF7, 0xF7, 0xF8, 0xF7, 0xF7, 0xF8, 0xF7, 0xF7, 0xF7, 0xF7, 0x2B, 0x2B, 0x2B, 0x2B,
	0x2B, 0x2B, 0x81, 0xFB, 0x81, 0xFB, 0xFB, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B, 0xF7,
	0xA0, 0xA0, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0x56, 0x56, 0xF9, 0xF9, 0xF9, 0xF8, 0xF7,
	0xF7, 0xF7, 0xFB, 0xFB, 0x81, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF5, 0x2B, 0xF7,
	0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF9, 0xF9, 0xFB, 0xFB, 0xFB, 0xF8, 0xF9,
	0xF9, 0xF7, 0x81, 0xFB, 0xFB, 0x81, 0xFB, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xF8, 0x2B, 0x2B, 0x56,
	0x2B, 0x2B, 0xF9, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF8,
	0xF8, 0xF7, 0xFB, 0xFB, 0x81, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xF8, 0xF8, 0xF6, 0xF6, 0xF9, 0xF8,
	0x2B, 0xF9, 0xF8, 0x2B, 0xF9, 0x2B, 0x2B, 0xF9, 0x2B, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7,
	0xF7, 0xF7, 0x81, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xF9, 0xF8, 0xF6, 0xF6, 0xF7, 0xF9, 0xF8, 0x2B,
	0xF9, 0xF8, 0xF6, 0xF9, 0xF8, 0xF6, 0xF9, 0xF8, 0xF6, 0x2B, 0xF9, 0x2B, 0xF9, 0x56, 0x2B, 0xF9,
	0x2B, 0xF9, 0xAC, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xFA, 0xF9, 0xF8, 0x2B, 0x2B, 0x2B, 0xF5, 0xF5,
	0xF5, 0xF5, 0xF9, 0xF8, 0xF6, 0xF9, 0xF8, 0xF6, 0x2B, 0xF8, 0x2B, 0xF9, 0x56, 0x2B, 0x56, 0x2B,
	0x56, 0x81, 0xAC, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xFA, 0xFA, 0xFA, 0xF9, 0xF8,
	0xF8, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF5, 0x2B, 0xF9, 0xF6, 0xF9, 0xF8, 0xF7, 0xF9, 0x2B, 0xF9,
	0x81, 0xAC, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xFA, 0xFA, 0xFA, 0xFA, 0xF9, 0xF8, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0xF5, 0xF5, 0xF5, 0x56, 0x81,
	0xAC, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2,
	0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xFA, 0xF9, 0xFA, 0xFA, 0xF9, 0xF9, 0xF8, 0xF8, 0x81, 0x81,
	0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0x00, 0x00, 0x01, 0x00, 0x03, 0xFF, 0xFF, 0xE0,
	0x02, 0x00, 0x00, 0x38, 0x02, 0xFF, 0xFF, 0x3C, 0x02, 0xFF, 0xFF, 0x3C, 0x02, 0xFF, 0xFF, 0x3C,
	0x02, 0xF0, 0x0F, 0x3C, 0x02, 0xFF, 0xFF, 0x3C, 0x02, 0xFF, 0xFF, 0x7C, 0x02, 0xE0, 0x1F, 0x7C,
	0x02, 0xE0, 0x1F, 0x7C, 0x02, 0xE0, 0x1F, 0x7C, 0x02, 0xE0, 0x1F, 0x7C, 0x02, 0xE0, 0x1F, 0x78,
	0x02, 0xFF, 0xFF, 0x78, 0x02, 0xFF, 0xFF, 0x78, 0x02, 0x1F, 0xFF, 0x70, 0x02, 0x00, 0x00, 0x70,
	0x03, 0xFF, 0xFF, 0xF0, 0x00, 0x0F, 0xFF, 0xE0, 0x00, 0xFF, 0xFF, 0xFF, 0x01, 0x1F, 0xFF, 0xFF,
	0x02, 0x00, 0x3F, 0xFF, 0x02, 0x40, 0x00, 0x3F, 0x02, 0xC0, 0x7C, 0x3F, 0x02, 0x00, 0x7D, 0xBF,
	0x0F, 0x20, 0x00, 0x3F, 0x32, 0x49, 0x00, 0x3C, 0xC4, 0x92, 0x2D, 0x70, 0xE0, 0x24, 0x1A, 0xE0,
	0x1F, 0x00, 0xA5, 0xC0, 0x00, 0xFC, 0x03, 0x80, 0x00, 0x03, 0xFF, 0x00, 0x03, 0xFF, 0xFF, 0xE0,
	0x03, 0xFF, 0xFF, 0xF8, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC,
	0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC,
	0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xFC, 0x03, 0xFF, 0xFF, 0xF8,
	0x03, 0xFF, 0xFF, 0xF8, 0x03, 0xFF, 0xFF, 0xF8, 0x03, 0xFF, 0xFF, 0xF0, 0x03, 0xFF, 0xFF, 0xF0,
	0x03, 0xFF, 0xFF, 0xF0, 0x00, 0x1F, 0xFF, 0xE0, 0x01, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF,
	0x07, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0xFF,
	0x0F, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xE0,
	0x1F, 0xFF, 0xFF, 0xC0, 0x00, 0xFF, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x40,
	0x1F, 0xFC, 0x10, 0x06, 0x10, 0x06, 0x10, 0x06, 0x10, 0x06, 0x10, 0x06, 0x10, 0x06, 0x10, 0x04,
	0x1F, 0xFC, 0x0F, 0xFE, 0x0F, 0xFF, 0x18, 0x67, 0x34, 0x06, 0x69, 0x64, 0x72, 0xC8, 0x3F, 0xF0,
	0x1F, 0xFC, 0x1F, 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x1F, 0xFC,
	0x1F, 0xFC, 0x07, 0xFF, 0x1F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, 0x7F, 0xFE, 0xFF, 0xFC, 0x07, 0xF8,
	0x00, 0x00, 0x00, 0x80, 0x79, 0x7C, 0x0C, 0x0C, 0xCC, 0xCC, 0xCD, 0x97, 0x97, 0x90, 0xE7, 0x97,
	0x97, 0x97, 0xDD, 0xD9, 0x79, 0x7C, 0xE7, 0xD5, 0x5E, 0x58, 0xCE, 0xD7, 0x97, 0x9C, 0xDD, 0x5D,
	0x7D, 0xB7, 0xDD, 0x59, 0x79, 0x7C, 0x9D, 0x10, 0x1D, 0xD9, 0xCE, 0xD7, 0x97, 0x9C, 0xDD, 0x0C,
	0xCC, 0xE7, 0xDD, 0xD9, 0x79, 0x7C, 0xED, 0xDD, 0xDD, 0x79, 0xCE, 0xE7, 0xE7, 0x90, 0xE7, 0x77,
	0x97, 0x97, 0xDD, 0x79, 0x79, 0x7C, 0xCC, 0xDC, 0xCD, 0xC8, 0xDD, 0x97, 0x97, 0x99, 0x7C, 0xDD,
	0xDD, 0xDE, 0xDE, 0xDE, 0x7E, 0x7C, 0xCC, 0xCC, 0xCD, 0xCD, 0xDD, 0xDE, 0x99, 0x0C, 0x8C, 0xCC,
	0xCC, 0xCC, 0xCD, 0xED, 0x77, 0xCC, 0xCC, 0xCD, 0xDD, 0xED, 0xCE, 0xDE, 0x9C, 0xCD, 0xCD, 0xCD,
	0x0D, 0xCC, 0xCE, 0xE7, 0xDC, 0xDC, 0xDC, 0xDC, 0xDC, 0xCC, 0xDE, 0x99, 0x97, 0x97, 0x9D, 0xDD,
	0xDD, 0xDE, 0xE9, 0x77, 0x00, 0x00, 0x01, 0x00, 0xC2, 0xC2, 0xC2, 0xF5, 0xF5, 0xF5, 0xF6, 0xF6,
	0xF6, 0x2B, 0x2B, 0x2B, 0xF7, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xC2, 0xC2, 0xC2,
	0xC2, 0xC2, 0xC2, 0xC2, 0xF8, 0x81, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xC2, 0x81, 0xAA,
	0xAA, 0xAA, 0xFB, 0xC2, 0xF8, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xF9, 0x7F, 0x7F,
	0x7F, 0x56, 0x81, 0xC2, 0xF8, 0x81, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xF9, 0x0A, 0xF6,
	0x0A, 0x56, 0xFB, 0xC2, 0xF8, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xF9, 0xF6, 0xF6,
	0xF6, 0x56, 0x81, 0xC2, 0x56, 0x81, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xF9, 0xF9, 0xF9,
	0xF9, 0xF9, 0xC2, 0xC2, 0xF8, 0x81, 0xFB, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0xA5, 0xC2, 0xC2, 0xC2,
	0xC2, 0xC2, 0xC2, 0xC2, 0x56, 0xFA, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0x2B, 0x2B, 0x2B, 0xF7, 0xF7,
	0xF7, 0xF7, 0xF7, 0xB9, 0x56, 0x81, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF6, 0x56, 0xF9,
	0xFA, 0xFA, 0xFB, 0xFB, 0xFB, 0x81, 0xFA, 0x81, 0xC2, 0xC2, 0xC2, 0xF6, 0xF6, 0xF7, 0x2B, 0x2B,
	0x2B, 0xF8, 0xF8, 0xF8, 0xF9, 0xFA, 0x81, 0xFB, 0xC2, 0xC2, 0xF5, 0xF7, 0x93, 0xF7, 0xF7, 0xF7,
	0xF7, 0x2B, 0x2B, 0x2B, 0x2B, 0xFB, 0x81, 0xFB, 0xC2, 0xC2, 0xF5, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7,
	0xF9, 0xFB, 0xFB, 0xF9, 0xF7, 0xFB, 0x81, 0xFB, 0xC2, 0xF6, 0xF8, 0xF9, 0x2B, 0xF9, 0x2B, 0xF9,
	0xF6, 0xF8, 0x2B, 0xF8, 0xF7, 0xFB, 0xFB, 0xC2, 0xF9, 0xF7, 0xF9, 0xF7, 0xF9, 0xF7, 0xF9, 0x2B,
	0xF9, 0x2B, 0xF8, 0x2B, 0x81, 0xAC, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xC2, 0xF9, 0xF9, 0xF9,
	0xF9, 0xF9, 0xF9, 0x81, 0xAC, 0xC2, 0xC2, 0xC2, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0xD8,
	0x00, 0x00, 0x08, 0xD8, 0x00, 0x00, 0x00, 0x96, 0x02, 0x1C, 0xC1, 0xC4, 0x18, 0x9C, 0x00, 0x00,
	0x00, 0x1C, 0x00, 0x96, 0x00, 0x05, 0x69, 0x63, 0x6C, 0x34, 0x00, 0x00, 0x00, 0x32, 0x69, 0x63,
	0x6C, 0x38, 0x00, 0x00, 0x00, 0x3E, 0x49, 0x43, 0x4E, 0x23, 0x00, 0x00, 0x00, 0x4A, 0x69, 0x63,
	0x73, 0x23, 0x00, 0x00, 0x00, 0x56, 0x69, 0x63, 0x73, 0x34, 0x00, 0x00, 0x00, 0x62, 0x69, 0x63,
	0x73, 0x38, 0x00, 0x00, 0x00, 0x6E, 0xBF, 0xB9, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1C,
	0xE2, 0x10, 0xBF, 0xB9, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x04, 0x02, 0x1C, 0xE1, 0xAC, 0xBF, 0xB9,
	0xFF, 0xFF, 0x00, 0x00, 0x06, 0x08, 0x02, 0x1C, 0xE1, 0xA4, 0xBF, 0xB9, 0xFF, 0xFF, 0x00, 0x00,
	0x07, 0x0C, 0x02, 0x1C, 0xE1, 0xF8, 0xBF, 0xB9, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0x50, 0x02, 0x1C,
	0xE1, 0xDC, 0xBF, 0xB9, 0xFF, 0xFF, 0x00, 0x00, 0x07, 0xD4, 0x02, 0x1C, 0xE1, 0xD0
};

static bool use_streams[ 'Z'-'A'+1 ];

static bool is_ntfs_volume(LPCTSTR rootdir)
{
	bool ret = false;
	TCHAR tst_file[_MAX_PATH], tst_stream[_MAX_PATH];
	_sntprintf( tst_file, lengthof(tst_file), TEXT("%sb2query.tmp"), rootdir );
	_sntprintf( tst_stream, lengthof(tst_stream), TEXT("%s:AFP_AfpInfo"), tst_file );
	if(!exists(tst_file)) {
		if(create_file( tst_file, 0 )) {
			if(create_file( tst_stream, 0 )) {
				ret = true;
			}
			DeleteFile( tst_file );
		}
	}
	return ret;
}


// !!UNC
void init_posix_emu(void)
{
#if DEBUG_EXTFS
	debug_extfs = PrefsFindInt16("debugextfs");

	debug_extfs = DB_EXTFS_LOUD;

	if(debug_extfs != DB_EXTFS_NONE) {
		extfs_log_open( EXTFS_LOG_FILE_NAME );
	}
#endif

	// We cannot use ExtFS "RootPath" because of the virtual desktop.
	if(PrefsFindBool("enableextfs")) {
		PrefsReplaceString("extfs", "");
	} else {
		PrefsRemoveItem("extfs");
		D(bug("extfs disabled by user\n"));
#if DEBUG_EXTFS
		extfs_log_close();
#endif
		return;
	}

	const char *extdrives = PrefsFindString("extdrives");

	// Set up drive list.
	size_t outinx = 0;
	for( TCHAR letter = TEXT('A'); letter <= TEXT('Z'); letter++ ) {
		if(extdrives && !strchr(extdrives,letter)) continue;
		TCHAR rootdir[20];
		_sntprintf( rootdir, lengthof(rootdir), TEXT("%c:\\"), letter );
		use_streams[ letter - 'A' ] = false;
		switch(GetDriveType(rootdir)) {
			case DRIVE_FIXED:
			case DRIVE_REMOTE:
			case DRIVE_RAMDISK:
				// TODO: NTFS AFP?
				// fall
			case DRIVE_REMOVABLE:
			case DRIVE_CDROM:
				if(outinx < lengthof(host_drive_list)) {
					host_drive_list[outinx] = letter;
					outinx += 2;
				}
		}
	}

	// Set up virtual desktop root.
	// TODO: this should be customizable.
	GetModuleFileName( NULL, virtual_root, lengthof(virtual_root) );
	TCHAR *p = _tcsrchr( virtual_root, TEXT('\\') );
	if(p) {
		_tcscpy( ++p, desktop_name );
	} else {
		// should never happen
		_sntprintf( virtual_root, lengthof(virtual_root), TEXT("C:\\%s"), desktop_name );
	}
	CreateDirectory( virtual_root, 0 );

	// Set up an icon looking like "My Computer"
	// Can be overwritten just like any other folder custom icon.
	if(my_access(custom_icon_name,0) != 0) {
		int fd = my_creat( custom_icon_name, 0 );
		if(fd >= 0) {
			my_close(fd);
			fd = open_rfork( custom_icon_name, O_RDWR|O_CREAT );
			if(fd >= 0) {
				my_write( fd, my_comp_icon, sizeof(my_comp_icon) );
				my_close(fd);
				static uint8 host_finfo[SIZEOF_FInfo];
				uint32 finfo = Host2MacAddr(host_finfo);
				get_finfo(custom_icon_name, finfo, 0, false);
				WriteMacInt16(finfo + fdFlags, kIsInvisible);
				set_finfo(custom_icon_name, finfo, 0, false);
				get_finfo(my_computer, finfo, 0, true);
				WriteMacInt16(finfo + fdFlags, ReadMacInt16(finfo + fdFlags) | kHasCustomIcon);
				set_finfo(my_computer, finfo, 0, true);
			} else {
				my_remove(custom_icon_name);
			}
		}
	}
}

void final_posix_emu(void)
{
#if DEBUG_EXTFS
	extfs_log_close();
#endif
}

static void charset_host2mac( char *s )
{
	int i, len=strlen(s), code;

	for( i=len-3; i>=0; i-- ) {
		if( s[i] == '%' && isxdigit(s[i+1]) && isxdigit(s[i+2]) ) {
			sscanf( &s[i], "%%%02X", &code );
			memmove( &s[i], &s[i+2], strlen(&s[i+2])+1 );
			s[i] = code;
		}
	}
}

static void charset_mac2host( LPTSTR s )
{
	size_t len = _tcslen(s);

	D(bug(TEXT("charset_mac2host(%s)...\n"), s));

	for( size_t i=len; i-->0; ) {
		bool convert = false;
		switch( (unsigned char)s[i] ) {
			// case '\r': // handled by "default"
			// case '\n':
			// case '\t':
			case '/':
			// case '\\': // Backslash is tricky -- "s" is a full path!
			// case ':':
			case '*':
			case '?':
			case '"':
			case '<':
			case '>':
			case '|':
			case '%':
				convert = true;
				break;
			default:
				if((unsigned char)s[i] < ' ') convert = true;
				break;
		}
		if(convert) {
			TCHAR sml[10];
			_sntprintf( sml, lengthof(sml), TEXT("%%%02X"), s[i] );
			memmove( &s[i+2], &s[i], (_tcslen(&s[i])+1) * sizeof(TCHAR) );
			memmove( &s[i], sml, 3 * sizeof(TCHAR) );
		}
	}
	D(bug(TEXT("charset_mac2host = %s\n"), s));
}

static void make_mask(
	TCHAR *mask,
	LPCTSTR dir,
	LPCTSTR a1,
	LPCTSTR a2
)
{
	_tcscpy( mask, dir );

	size_t len = _tcslen(mask);
	if( len && mask[len-1] != '\\' ) _tcscat( mask, TEXT("\\") );

	if( a1 ) _tcscat( mask, a1 );
	if( a2 ) _tcscat( mask, a2 );
}

// !!UNC
static LPTSTR translate( LPCTSTR path, TCHAR *buffer )
{
	TCHAR *l = host_drive_list;
	const TCHAR *p = path;

	while(*l) {
		if(_totupper(p[1]) == _totupper(*l)) break;
		l += _tcslen(l) + 1;
	}

	if(p[0] == TEXT('\\') && *l && (p[2] == 0 || p[2] == TEXT(':') || p[2] == TEXT('\\'))) {
		p += 2;
		if(*p == TEXT(':')) p++;
		if(*p == TEXT('\\')) p++;
		_sntprintf( buffer, MAX_PATH_LENGTH, TEXT("%c:\\%s"), *l, p );
	} else {
		if(*path == TEXT('\\')) {
			_sntprintf( buffer, MAX_PATH_LENGTH, TEXT("%s%s"), virtual_root, path );
		} else {
			int len = _tcslen(path);
			if(len == 0 || path[len-1] == TEXT('\\')) {
				make_mask( buffer, virtual_root, path, tstr(my_computer).get() );
			} else {
				make_mask( buffer, virtual_root, path, 0 );
			}
		}
	}
	charset_mac2host( buffer );

	return buffer;
}

// helpers
static void strip_trailing_bs( LPTSTR path )
{
	size_t len = _tcslen(path);
	if(len > 0 && path[len-1] == TEXT('\\')) path[len-1] = 0;
}

#if 0 /* defined is util_windows.cpp */
static int exists( const char *p )
{
	WIN32_FIND_DATA fdata;

	int result = 0;

	HANDLE h = FindFirstFile( p, &fdata );
	if(h != INVALID_HANDLE_VALUE) {
		result = 1;
		FindClose( h );
	}

	D(bug("exists(%s) = %d\n", p, result));

	return result;
}
#endif

static int is_dir( LPCTSTR p )
{
	WIN32_FIND_DATA fdata;

	int result = 0;

	HANDLE h = FindFirstFile( p, &fdata );
	if(h != INVALID_HANDLE_VALUE) {
		result = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		FindClose( h );
	}
	return result;
}

static int myRemoveDirectory( LPCTSTR source )
{
	HANDLE fh;
	WIN32_FIND_DATA FindFileData;
	int ok, result = 1;
	TCHAR mask[_MAX_PATH];

	D(bug(TEXT("removing folder %s\n"), source));

	make_mask( mask, source, TEXT("*.*"), 0 );

	fh = FindFirstFile( mask, &FindFileData );
	ok = fh != INVALID_HANDLE_VALUE;
	while(ok) {
		make_mask( mask, source, FindFileData.cFileName, 0 );
		D(bug(TEXT("removing item %s\n"), mask));
		int isdir = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if(isdir) {
			// must delete ".finf", ".rsrc" but not ".", ".."
			if(_tcscmp(FindFileData.cFileName,TEXT(".")) && _tcscmp(FindFileData.cFileName,TEXT(".."))) {
				result = myRemoveDirectory( mask );
				if(!result) break;
			}
		} else {
			D(bug(TEXT("DeleteFile %s\n"), mask));
			result = DeleteFile( mask );
			if(!result) break;
		}
		ok = FindNextFile( fh, &FindFileData );
	}
	if(fh != INVALID_HANDLE_VALUE) FindClose( fh );
	if(result) {
		D(bug(TEXT("RemoveDirectory %s\n"), source));
		result = RemoveDirectory( source );
	}
	return result;
}

static void make_folders( LPCTSTR path )
{
	TCHAR local_path[_MAX_PATH], *p;
	_tcscpy( local_path, path );
	p = _tcsrchr( local_path, TEXT('\\') );
	if(p) {
		*p = 0;
		if(_tcslen(local_path) > 3) {
			make_folders(local_path);
			_tmkdir(local_path);
		}
	}
}

// !!UNC
static bool is_same_drive( LPCTSTR p1, LPCTSTR p2 )
{
	return _totupper(*p1) == _totupper(*p2);
}

// Used when the drives are known to be different.
// Can't use MoveFileEx() etc because of the Win9x limitations.
// It would simulate CopyFile*() -- DeleteFile*() anyway
static int file_move_copy( LPCTSTR src, LPCTSTR dst, bool delete_old )
{
	int result = 0;
	my_errno = 0;

	D(bug(TEXT("file_copy %s -> %s\n"), src, dst));

	// Fail if exists -- it's up to MacOS to move things to Trash
	if(CopyFile(src,dst,TRUE)) {
		if(delete_old && !DeleteFile(src)) {
			result = -1;
			my_errno = EACCES;
		}
	} else {
		result = -1;
		if(exists(src))
			my_errno = EACCES;
		else
			my_errno = ENOENT;
	}
	return result;
}

static int file_move( LPCTSTR src, LPCTSTR dst )
{
	return file_move_copy( src, dst, true );
}

static int file_copy( LPCTSTR src, LPCTSTR dst )
{
	return file_move_copy( src, dst, false );
}

static int folder_copy( LPCTSTR folder_src, LPCTSTR folder_dst )
{
	HANDLE fh;
	WIN32_FIND_DATA FindFileData;
	int ok, result = 0;
	TCHAR mask[_MAX_PATH];

	D(bug(TEXT("copying folder %s -> \n"), folder_src, folder_dst));

	my_errno = 0;

	if(!CreateDirectory( folder_dst, 0 )) {
		my_errno = EACCES;
		return -1;
	}

	make_mask( mask, folder_src, TEXT("*.*"), 0 );

	fh = FindFirstFile( mask, &FindFileData );
	ok = fh != INVALID_HANDLE_VALUE;
	while(ok) {
		make_mask( mask, folder_src, FindFileData.cFileName, 0 );
		int isdir = (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		TCHAR target[_MAX_PATH];
		make_mask( target, folder_dst, FindFileData.cFileName, 0 );
		D(bug(TEXT("copying item %s -> %s\n"), mask, target));
		if(isdir) {
			if(_tcscmp(FindFileData.cFileName,TEXT(".")) && _tcscmp(FindFileData.cFileName,TEXT(".."))) {
				result = folder_copy( mask, target );
				if(result < 0) break;
			}
		} else {
			result = file_copy( mask, target );
			if(result < 0) break;
		}
		ok = FindNextFile( fh, &FindFileData );
	}
	if(fh != INVALID_HANDLE_VALUE) FindClose( fh );
	return result;
}

// dir enumeration
void closedir( struct DIR *d )
{
	DISABLE_ERRORS;
	if(d) {
		if(d->h != INVALID_HANDLE_VALUE && d->h != VIRTUAL_ROOT_ID) {
			FindClose( d->h );
		}
		delete d;
	}
	RESTORE_ERRORS;
}

static int make_dentry( struct DIR *d )
{
	int ok = 0;

	memset( &d->de, 0, sizeof(d->de) );
	if(d->h != INVALID_HANDLE_VALUE) {
		if( (d->FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
			   *d->FindFileData.cFileName == TEXT('.'))
		{
			ok = 0;
		} else {
			strlcpy( d->de.d_name, d->FindFileData.cFileName, lengthof(d->de.d_name) );
			charset_host2mac( d->de.d_name );
			ok = 1;
		}
	}
	return ok;
}

struct dirent *readdir( struct DIR *d )
{
	DISABLE_ERRORS;

	dirent *de = 0;

	if(d) {
		if(d->h != INVALID_HANDLE_VALUE) {
			if(d->h == VIRTUAL_ROOT_ID) {
				make_dentry(d);
				de = &d->de;
				d->vname_list += _tcslen(d->vname_list) + 1;
				if(*d->vname_list) {
					_tcscpy( d->FindFileData.cFileName, d->vname_list );
					d->FindFileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
				} else {
					// Out of static drive entries. Continue with other stuff.
					TCHAR mask[MAX_PATH_LENGTH];
					make_mask( mask, virtual_root, TEXT("*.*"), 0 );
					d->h = FindFirstFile( mask, &d->FindFileData );
				}
			} else {
				int done = 0;
				do {
					if(make_dentry(d)) {
						de = &d->de;
						done = 1;
					}
					if(!FindNextFile( d->h, &d->FindFileData )) {
						FindClose( d->h );
						d->h = INVALID_HANDLE_VALUE;
						done = 1;
					}
				} while(!done);
			}
		}
	}

	if(de) {
		D(bug("readdir found %s\n", de->d_name));
	}

	RESTORE_ERRORS;

	return de;
}

struct DIR *opendir( const char *path )
{
	DISABLE_ERRORS;
	auto tpath = tstr(path);
	DIR *d = new DIR;
	if(d) {
		memset( d, 0, sizeof(DIR) );
		if(*tpath.get() == 0) {
			d->vname_list = host_drive_list;
			if(d->vname_list) {
				d->h = VIRTUAL_ROOT_ID;
				_tcscpy( d->FindFileData.cFileName, d->vname_list );
				d->FindFileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			} else {
				d->h = INVALID_HANDLE_VALUE;
			}
		} else {
			TCHAR mask[MAX_PATH_LENGTH];
			make_mask( mask, MRP(tpath.get()), TEXT("*.*"), 0 );

			D(bug(TEXT("opendir path=%s, mask=%s\n"), tpath.get(), mask));

			d->h = FindFirstFile( mask, &d->FindFileData );
			if(d->h == INVALID_HANDLE_VALUE) {
				delete d;
				d = 0;
			}
		}
	}

	D(bug(TEXT("opendir(%s,%s) = %08x\n"), tpath.get(), MRP(tpath.get()), d));

	RESTORE_ERRORS;

	return d;
}

static void dump_stat( const struct my_stat *st )
{
	D(bug("stat: size = %ld, mode = %ld, a = %ld, m = %ld, c = %ld\n", st->st_size, st->st_mode, st->st_atime, st->st_mtime, st->st_ctime));
}



// Exported hook functions
int my_stat( const char *path, struct my_stat *st )
{
	DISABLE_ERRORS;

	auto tpath = tstr(path);
	int result;

	if(*tpath.get() == 0) {
		/// virtual root
		memset( st, 0, sizeof(struct my_stat) );
		st->st_mode = _S_IFDIR;
		result = 0;
		my_errno = 0;
	} else {
		result = _tstat( MRP(tpath.get()), (struct _stat *)st );
		if(result < 0) {
			my_errno = errno;
		} else {
			my_errno = 0;
		}
	}

	D(bug(TEXT("stat(%s,%s) = %d\n"), tpath.get(), MRP(tpath.get()), result));
	if(result >= 0) dump_stat( st );
	RESTORE_ERRORS;
	return result;
}

int my_fstat( int fd, struct my_stat *st )
{
	DISABLE_ERRORS;
	int result = _fstat( fd, (struct _stat *)st );
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	D(bug("fstat(%d) = %d\n", fd, result));
	if(result >= 0) dump_stat( st );
	RESTORE_ERRORS;
	return result;
}

int my_open( const char *path, int mode, ... )
{
	DISABLE_ERRORS;
	int result;
	auto tpath = tstr(path);
	LPCTSTR p = MRP(tpath.get());

	// Windows "open" does not handle _O_CREAT and _O_BINARY as it should
	if(mode & _O_CREAT) {
		if(exists(p)) {
			result = _topen( p, mode & ~_O_CREAT );
			D(bug(TEXT("open-nocreat(%s,%s,%d) = %d\n"), tpath.get(), p, mode, result));
		} else {
			result = _tcreat( p, _S_IWRITE|_S_IREAD );
			if(result < 0) {
				make_folders(p);
				result = _tcreat( p, _S_IWRITE|_S_IREAD );
			}
			D(bug(TEXT("open-creat(%s,%s,%d) = %d\n"), tpath.get(), p, mode, result));
		}
	} else {
		result = _topen( p, mode );
		D(bug(TEXT("open(%s,%s,%d) = %d\n"), tpath.get(), p, mode, result));
	}
	if(result < 0) {
		my_errno = errno;
	} else {
		setmode(result, _O_BINARY);
		my_errno = 0;
	}
	RESTORE_ERRORS;
	return result;
}

int my_rename( const char *old_path, const char *new_path )
{
	DISABLE_ERRORS;
	int result = -1;
	auto told_path = tstr(old_path);
	auto tnew_path = tstr(new_path);
	LPCTSTR p_old = MRP(told_path.get());
	LPCTSTR p_new = MRP2(tnew_path.get());

	result = my_access(old_path,0);
	if(result < 0) {
		// my_errno already set
	} else {
		if(is_same_drive(p_old,p_new)) {
			result = _trename( p_old, p_new );
			if(result != 0) { // by definition, rename may also return a positive value to indicate an error
				my_errno = errno;
			} else {
				my_errno = 0;
			}
		} else {
			if(is_dir(p_old)) {
				result = folder_copy( p_old, p_new );
				// my_errno already set
				if(result >= 0) {
					if(myRemoveDirectory( p_old )) {
						my_errno = 0;
						result = 0;
					} else {
						// there is no proper error code for this failure.
						my_errno = EACCES;
						result = -1;
					}
				}
			} else {
				result = file_move( p_old, p_new );
				// my_errno already set
			}
		}
	}
	D(bug(TEXT("rename(%s,%s,%s,%s) = %d\n"), told_path.get(), p_old, tnew_path.get(), p_new, result));
	RESTORE_ERRORS;
	return result;
}

int my_access( const char *path, int mode )
{
	DISABLE_ERRORS;
	auto tpath = tstr(path);
	LPCTSTR p = MRP(tpath.get());
	WIN32_FIND_DATA fdata;

	int result;

	if(is_dir(p)) {
		// access does not work for folders.
		HANDLE h = FindFirstFile( p, &fdata );
		if(h != INVALID_HANDLE_VALUE) {
			FindClose( h );
			if(mode == W_OK) {
				if( (fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0 ) {
					result = 0;
					my_errno = 0;
				} else {
					result = -1;
					my_errno = EACCES;
				}
			} else {
				result = 0;
				my_errno = 0;
			}
		} else {
			result = -1;
			my_errno = ENOENT;
		}
	} else {
		// W_OK, F_OK are ok.
		result = _taccess(p,mode);
		if(result < 0) {
			my_errno = errno;
		} else {
			my_errno = 0;
		}
	}

	D(bug(TEXT("access(%s,%s,%d) = %d\n"), tpath.get(), p, mode, result));
	RESTORE_ERRORS;
	return result;
}

int my_mkdir( const char *path, int mode )
{
	DISABLE_ERRORS;
	auto tpath = tstr(path);
	LPTSTR p = MRP(tpath.get());
	strip_trailing_bs(p);
	int result = _tmkdir( p );
	if(result < 0) {
		make_folders(p);
		result = _tmkdir( p );
	}
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	D(bug(TEXT("mkdir(%s,%s,%d) = %d\n"), tpath.get(), p, mode, result));
	RESTORE_ERRORS;
	return result;
}

int my_remove( const char *path )
{
	DISABLE_ERRORS;
	auto tpath = tstr(path);
	LPTSTR p = MRP(tpath.get());
	strip_trailing_bs(p);
	int result;
	if(is_dir(p)) {
		result = myRemoveDirectory( p );
	} else {
		D(bug(TEXT("DeleteFile %s\n"), p));
		result = DeleteFile( p );
	}
	if(result) {
		result = 0;
		my_errno = 0;
	} else {
		result = -1;
		if(exists(p)) {
			my_errno = EACCES;
		} else {
			my_errno = ENOENT;
		}
	}
	D(bug(TEXT("remove(%s,%s) = %d\n"), tpath.get(), p, result));
	RESTORE_ERRORS;
	return result;
}

int my_creat( const char *path, int mode )
{
	DISABLE_ERRORS;
	auto tpath = tstr(path);
	LPCTSTR p = MRP(tpath.get());
	int result = _tcreat( p, _S_IWRITE|_S_IREAD ); // note mode
	if(result < 0) {
		make_folders(p);
		result = _tcreat( p, _S_IWRITE|_S_IREAD ); // note mode
	}
	if(result < 0) {
		my_errno = errno;
	} else {
		setmode(result, _O_BINARY);
		my_errno = 0;
	}
	D(bug(TEXT("creat(%s,%s,%d) = %d\n"), tpath.get(), p, mode,result));
	RESTORE_ERRORS;
	return result;
}

int my_chsize( int fd, size_t sz )
{
	DISABLE_ERRORS;
	int result = chsize(fd,sz);
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	RESTORE_ERRORS;
	return result;
}

int my_close( int fd )
{
	DISABLE_ERRORS;
	int result = close(fd);
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	RESTORE_ERRORS;
	D(bug("close(%d) = %d\n", fd, result));
	return result;
}

long my_lseek( int fd, long offset, int origin )
{
	DISABLE_ERRORS;
	int result = lseek( fd, offset, origin );
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	RESTORE_ERRORS;
	return result;
}

int my_read( int fd, void *buffer, unsigned int count )
{
	DISABLE_ERRORS;
	int result = read( fd, buffer, count );
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	RESTORE_ERRORS;
	D(bug("read(%ld,%08x,%ld) = %d\n", fd, buffer, count, result));

	return result;
}

int my_write( int fd, const void *buffer, unsigned int count )
{
	DISABLE_ERRORS;
	int result = write( fd, buffer, count );
	if(result < 0) {
		my_errno = errno;
	} else {
		my_errno = 0;
	}
	RESTORE_ERRORS;
	D(bug("write(%ld,%08x,%ld) = %d\n", fd, buffer, count, result));
	return result;
}
