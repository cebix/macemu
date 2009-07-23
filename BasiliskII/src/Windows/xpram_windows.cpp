/*
 *  xpram_windows.cpp - XPRAM handling, Windows specific stuff
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
using std::string;

#include "xpram.h"


// XPRAM file name and path
#if POWERPC_ROM
const char XPRAM_FILE_NAME[] = "SheepShaver_nvram.dat";
#else
const char XPRAM_FILE_NAME[] = "BasiliskII_xpram.dat";
#endif
static string xpram_path;


/*
 *  Construct XPRAM path
 */

static void build_xpram_path(void)
{
	xpram_path.clear();
	int pwd_len = GetCurrentDirectory(0, NULL);
	char *pwd = new char[pwd_len];
	if (GetCurrentDirectory(pwd_len, pwd) == pwd_len - 1)
		xpram_path = string(pwd) + '\\';
	delete[] pwd;
	xpram_path += XPRAM_FILE_NAME;
}


/*
 *  Load XPRAM from settings file
 */

void LoadXPRAM(const char *vmdir)
{
	// Construct XPRAM path
	build_xpram_path();

	// Load XPRAM from settings file
	HANDLE fh = CreateFile(xpram_path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (fh != INVALID_HANDLE_VALUE) {
		DWORD bytesRead;
		ReadFile(fh, XPRAM, XPRAM_SIZE, &bytesRead, NULL);
		CloseHandle(fh);
	}
}


/*
 *  Save XPRAM to settings file
 */

void SaveXPRAM(void)
{
	HANDLE fh = CreateFile(xpram_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (fh != INVALID_HANDLE_VALUE) {
		DWORD bytesWritten;
		WriteFile(fh, XPRAM, XPRAM_SIZE, &bytesWritten, NULL);
		CloseHandle(fh);
	}
}


/*
 *  Delete PRAM file
 */

void ZapPRAM(void)
{
	// Construct PRAM path
	build_xpram_path();

	// Delete file
	DeleteFile(xpram_path.c_str());
}
