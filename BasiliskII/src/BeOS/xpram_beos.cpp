/*
 *  xpram_beos.cpp - XPRAM handling, BeOS specific stuff
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

#include <StorageKit.h>
#include <unistd.h>

#include "sysdeps.h"
#include "xpram.h"


// XPRAM file name and path
#if POWERPC_ROM
const char XPRAM_FILE_NAME[] = "SheepShaver_NVRAM";
#else
const char XPRAM_FILE_NAME[] = "BasiliskII_XPRAM";
#endif
static BPath xpram_path;


/*
 *  Load XPRAM from settings file
 */

void LoadXPRAM(const char *vmdir)
{
	// Construct XPRAM path
	find_directory(B_USER_SETTINGS_DIRECTORY, &xpram_path, true);
	xpram_path.Append(XPRAM_FILE_NAME);

	// Load XPRAM from settings file
	int fd;
	if ((fd = open(xpram_path.Path(), O_RDONLY)) >= 0) {
		read(fd, XPRAM, XPRAM_SIZE);
		close(fd);
	}
}


/*
 *  Save XPRAM to settings file
 */

void SaveXPRAM(void)
{
	if (xpram_path.InitCheck() != B_NO_ERROR)
		return;
	int fd;
	if ((fd = open(xpram_path.Path(), O_WRONLY | O_CREAT, 0666)) >= 0) {
		write(fd, XPRAM, XPRAM_SIZE);
		close(fd);
	}
}


/*
 *  Delete PRAM file
 */

void ZapPRAM(void)
{
	// Construct PRAM path
	find_directory(B_USER_SETTINGS_DIRECTORY, &xpram_path, true);
	xpram_path.Append(XPRAM_FILE_NAME);

	// Delete file
	unlink(xpram_path.Path());
}
