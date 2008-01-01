/*
 *  ntcd.h - Interface to cdenable.sys driver
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


/*
		Installs the driver, if not already installed.
		Starts the driver, if not already running.

		You can either always call "CdenableSysInstallStart" when your
		program fires up and "CdenableSysStopRemove" when it terminates,
		or just let the installation program call "CdenableSysInstallStart"
		and leave it always be present.

		I recommend the latter option. Calling "CdenableSysInstallStart"
		always doesn't hurt anything, it will immediately return
		with success if the service is running.

		Returns non-zero if installation/startup was succesfull,
		zero if anything failed.
		Returns non-zero also if the driver was already running.

		The file "cdenable.sys" must already have been copied to
		the directory "System32\Drivers"
*/

#ifndef _NT_CD_H_
#define _NT_CD_H_

#ifdef __cplusplus
extern "C" {
#endif


BOOL CdenableSysInstallStart(void);


/*
		Stops and removes the driver. See above.
		This must be called when new version of the driver is updated.
*/
void CdenableSysStopRemove(void);


/*
		HANDLE h: returned from CreateFile ( "\\\\.\\X:", GENERIC_READ, ... );
		Returns the bytes actually read (==count), 0 on failure.
		NOTE: in my code, start and count are always aligned to
		sector boundaries (2048 bytes).
		I cannot guarantee that this works if they are not.
		Max read is 64 kb.
		Synchronous read, but quite fast.
*/
int CdenableSysReadCdBytes( HANDLE h, DWORD start, DWORD count, char *buf );


/*
		Same as SysReadCdBytes, but "start" and "count" are in 2048 byte
		sectors.
*/
int CdenableSysReadCdSectors( HANDLE h, DWORD start, DWORD count, char *buf );


/*
	Ditto for writing stuff.
	Not a cd of course but removable & hd media are supported now.
*/
int CdenableSysWriteCdBytes( HANDLE h, DWORD start, DWORD count, char *buf );
int CdenableSysWriteCdSectors( HANDLE h, DWORD start, DWORD count, char *buf );


/*
		Returns CDENABLE_CURRENT_VERSION (of the driver).
*/
DWORD CdenableSysGetVersion( void );

#ifdef __cplusplus
} // extern "C"
#endif

#endif //_NT_CD_H_
