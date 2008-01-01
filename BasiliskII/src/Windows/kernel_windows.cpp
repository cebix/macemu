/*
 *  kernel_windows.cpp
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
#include "prefs.h"
#include "kernel_windows.h"

// From main_windows.cpp
extern DWORD win_os;
extern DWORD win_os_major;

static HMODULE hKernel32 = 0;
static HMODULE hUser32 = 0;
static HMODULE hB2Win32 = 0;

UINT (WINAPI *pfnGetWriteWatch) (DWORD,PVOID,SIZE_T,PVOID *,LPDWORD,LPDWORD) = 0;
BOOL (WINAPI *pfnInitializeCriticalSectionAndSpinCount) (LPCRITICAL_SECTION,DWORD) = 0;
BOOL (WINAPI *pfnCancelIo) (HANDLE) = 0;
BOOL (WINAPI *pfnGETCDSECTORS) (BYTE,DWORD,WORD,LPBYTE) = 0;
UINT (WINAPI *pfnSendInput) (UINT,LPVOID,int) = 0;
BOOL (WINAPI *pfnGetDiskFreeSpaceEx) (LPCSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER) = 0;

void KernelInit( void )
{
	hKernel32 = LoadLibrary( "kernel32.dll" );
	hUser32 = LoadLibrary( "user32.dll" );
	if(hKernel32) {
		if(win_os == VER_PLATFORM_WIN32_WINDOWS) {
			// NT5 RC2 Kernel exports GetWriteWatch(), but VirtualAlloc(MEM_WRITE_WATCH) fails
			pfnGetWriteWatch = (UINT (WINAPI *)(DWORD,PVOID,SIZE_T,PVOID *,LPDWORD,LPDWORD))GetProcAddress( hKernel32, "GetWriteWatch" );
		}
		pfnInitializeCriticalSectionAndSpinCount = (BOOL (WINAPI *)(LPCRITICAL_SECTION,DWORD))GetProcAddress( hKernel32, "InitializeCriticalSectionAndSpinCount" );
		pfnCancelIo = (BOOL (WINAPI *)(HANDLE))GetProcAddress( hKernel32, "CancelIo" );
		pfnGetDiskFreeSpaceEx = (BOOL (WINAPI *)(LPCSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER))GetProcAddress( hKernel32, "GetDiskFreeSpaceExA" );
	}
	if(hUser32) {
		// Win98 has this one too.
		// if(win_os == VER_PLATFORM_WIN32_NT) {
			pfnSendInput = (UINT (WINAPI *)(UINT,LPVOID,int))GetProcAddress( hUser32, "SendInput" );
		// }
	}
	if(win_os == VER_PLATFORM_WIN32_WINDOWS) {
		hB2Win32 = LoadLibrary( "B2Win32.dll" );
		if(hB2Win32) {
			pfnGETCDSECTORS = (BOOL (WINAPI *)(BYTE,DWORD,WORD,LPBYTE))GetProcAddress( hB2Win32, "GETCDSECTORS" );
		}
	}
}

void KernelExit( void )
{
	if(hKernel32) {
		FreeLibrary( hKernel32 );
		hKernel32 = 0;
	}
	if(hUser32) {
		FreeLibrary( hUser32 );
		hUser32 = 0;
	}
	if(hB2Win32) {
		FreeLibrary( hB2Win32 );
		hB2Win32 = 0;
	}
	pfnGetWriteWatch = 0;
	pfnInitializeCriticalSectionAndSpinCount = 0;
	pfnCancelIo = 0;
	pfnSendInput = 0;
	pfnGETCDSECTORS = 0;
}
