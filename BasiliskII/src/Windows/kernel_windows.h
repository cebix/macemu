/*
 *  kernel_windows.h
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

#ifndef _KERNEL_WINDOWS_H_
#define _KERNEL_WINDOWS_H_

extern UINT (WINAPI *pfnGetWriteWatch) (DWORD,PVOID,SIZE_T,PVOID *,LPDWORD,LPDWORD);
extern BOOL (WINAPI *pfnInitializeCriticalSectionAndSpinCount) (LPCRITICAL_SECTION,DWORD);
extern BOOL (WINAPI *pfnCancelIo) (HANDLE);
extern BOOL (WINAPI *pfnGETCDSECTORS) (BYTE,DWORD,WORD,LPBYTE);
extern UINT (WINAPI *pfnSendInput) (UINT,LPVOID,int);
extern BOOL (WINAPI *pfnGetDiskFreeSpaceEx) (LPCSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);

void KernelInit( void );
void KernelExit( void );

#endif // _KERNEL_WINDOWS_H_
