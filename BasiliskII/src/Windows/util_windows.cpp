/*
 *  util_windows.cpp - Miscellaneous utilities for Win32
 *
 *  Basilisk II (C) 1997-2004 Christian Bauer
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
#include "util_windows.h"

BOOL exists( const char *path )
{
	HFILE h;
	bool ret = false;

	h = _lopen( path, OF_READ );
	if(h != HFILE_ERROR) {
		ret = true;
		_lclose(h);
	}
	return(ret);
}

BOOL create_file( const char *path, DWORD size )
{
	HANDLE h;
	bool ok = false;

	h = CreateFile( path,
		GENERIC_READ | GENERIC_WRITE,
		0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL
	);
	if(h != INVALID_HANDLE_VALUE) {
		if(size == 0) {
			ok = true;
		} else if(SetFilePointer( h, size, NULL, FILE_BEGIN) != 0xFFFFFFFF) {
			if(SetEndOfFile(h)) {
				ok = true;
				if(SetFilePointer( h, 0, NULL, FILE_BEGIN) != 0xFFFFFFFF) {
					DWORD written, zeroed_size = min(1024*1024,size);
					char *b = (char *)malloc(zeroed_size);
					if(b) {
						memset( b, 0, zeroed_size );
						WriteFile( h, b, zeroed_size, &written, NULL );
						free(b);
					}
				}
			}
		}
		CloseHandle(h);
	}
	if(!ok) DeleteFile(path);
	return(ok);
}

int32 get_file_size( const char *path )
{
	HANDLE h;
	DWORD size = 0;

	h = CreateFile( path,
		GENERIC_READ,
		0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
	);
	if(h != INVALID_HANDLE_VALUE) {
		size = GetFileSize( h, NULL );
		CloseHandle(h);
	}
	return(size);
}


/*
 *  Thread wrappers
 */

HANDLE create_thread(LPTHREAD_START_ROUTINE start_routine, void *arg)
{
	DWORD dwThreadId;
	return CreateThread(NULL, 0, start_routine, arg, 0, &dwThreadId);
}

void wait_thread(HANDLE thread)
{
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

void kill_thread(HANDLE thread)
{
	TerminateThread(thread, 0);
}
