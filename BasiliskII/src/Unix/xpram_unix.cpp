/*
 *  xpram_unix.cpp - XPRAM handling, Unix specific stuff
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

#include <string>
using std::string;

#include "xpram.h"

// XPRAM file name, set by LoadPrefs() in prefs_unix.cpp
string xpram_name;

/*
 *  Load XPRAM from settings file
 */

void LoadXPRAM(const char* vmdir)
{
	assert(!xpram_name.empty());
	int fd;
	if ((fd = open(xpram_name.c_str(), O_RDONLY)) >= 0)
	{
		read(fd, XPRAM, XPRAM_SIZE);
		close(fd);
	}
}

/*
 *  Save XPRAM to settings file
 */

void SaveXPRAM(void)
{
	assert(!xpram_name.empty());
	int fd;
	if ((fd = open(xpram_name.c_str(), O_WRONLY | O_CREAT, 0666)) >= 0)
	{
		write(fd, XPRAM, XPRAM_SIZE);
		close(fd);
	}
	else
	{
		fprintf(stderr, "WARNING: Unable to save %s (%s)\n",
		        xpram_name.c_str(), strerror(errno));
	}
}

/*
 *  Delete PRAM file
 */

void ZapPRAM(void)
{
	// Delete file
	assert(!xpram_name.empty());
	unlink(xpram_name.c_str());
}
