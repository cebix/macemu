/*
 *  extfs_unix.cpp - MacOS file system for access native file system access, Unix specific stuff
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "sysdeps.h"
#include "extfs.h"
#include "extfs_defs.h"

#define DEBUG 0
#include "debug.h"


// Default Finder flags
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;


/*
 *  Initialization
 */

void extfs_init(void)
{
}


/*
 *  Deinitialization
 */

void extfs_exit(void)
{
}


/*
 *  Add component to path name
 */

void add_path_component(char *path, const char *component, int max_len)
{
	int l = strlen(path);
	if (l < max_len-1 && path[l-1] != '/') {
		path[l] = '/';
		path[l+1] = 0;
	}
	strncat(path, component, max_len-1);
}


/*
 *  Get/set finder type/creator for file specified by full path
 */

struct ext2type {
	const char *ext;
	uint32 type;
	uint32 creator;
};

static const ext2type e2t_translation[] = {
	{".Z", 'ZIVM', 'LZIV'},
	{".gz", 'Gzip', 'Gzip'},
	{".hqx", 'TEXT', 'SITx'},
	{".pdf", 'PDF ', 'CARO'},
	{".ps", 'TEXT', 'ttxt'},
	{".sit", 'SIT!', 'SITx'},
	{".tar", 'TARF', 'TAR '},
	{".uu", 'TEXT', 'SITx'},
	{".uue", 'TEXT', 'SITx'},
	{".zip", 'ZIP ', 'ZIP '},
	{".8svx", '8SVX', 'SNDM'},
	{".aifc", 'AIFC', 'TVOD'},
	{".aiff", 'AIFF', 'TVOD'},
	{".au", 'ULAW', 'TVOD'},
	{".mid", 'MIDI', 'TVOD'},
	{".midi", 'MIDI', 'TVOD'},
	{".mp2", 'MPG ', 'TVOD'},
	{".mp3", 'MPG ', 'TVOD'},
	{".wav", 'WAVE', 'TVOD'},
	{".bmp", 'BMPf', 'ogle'},
	{".gif", 'GIFf', 'ogle'},
	{".lbm", 'ILBM', 'GKON'},
	{".ilbm", 'ILBM', 'GKON'},
	{".jpg", 'JPEG', 'ogle'},
	{".jpeg", 'JPEG', 'ogle'},
	{".pict", 'PICT', 'ogle'},
	{".png", 'PNGf', 'ogle'},
	{".sgi", '.SGI', 'ogle'},
	{".tga", 'TPIC', 'ogle'},
	{".tif", 'TIFF', 'ogle'},
	{".tiff", 'TIFF', 'ogle'},
	{".html", 'TEXT', 'MOSS'},
	{".txt", 'TEXT', 'ttxt'},
	{".rtf", 'TEXT', 'MSWD'},
	{".c", 'TEXT', 'R*ch'},
	{".C", 'TEXT', 'R*ch'},
	{".cc", 'TEXT', 'R*ch'},
	{".cpp", 'TEXT', 'R*ch'},
	{".cxx", 'TEXT', 'R*ch'},
	{".h", 'TEXT', 'R*ch'},
	{".hh", 'TEXT', 'R*ch'},
	{".hpp", 'TEXT', 'R*ch'},
	{".hxx", 'TEXT', 'R*ch'},
	{".s", 'TEXT', 'R*ch'},
	{".S", 'TEXT', 'R*ch'},
	{".i", 'TEXT', 'R*ch'},
	{".mpg", 'MPEG', 'TVOD'},
	{".mpeg", 'MPEG', 'TVOD'},
	{".mov", 'MooV', 'TVOD'},
	{".fli", 'FLI ', 'TVOD'},
	{".avi", 'VfW ', 'TVOD'},
	{NULL, 0, 0}	// End marker
};

void get_finder_type(const char *path, uint32 &type, uint32 &creator)
{
	type = 0;
	creator = 0;

	// Translate file name extension to MacOS type/creator
	int path_len = strlen(path);
	for (int i=0; e2t_translation[i].ext; i++) {
		int ext_len = strlen(e2t_translation[i].ext);
		if (path_len < ext_len)
			continue;
		if (!strcmp(path + path_len - ext_len, e2t_translation[i].ext)) {
			type = e2t_translation[i].type;
			creator = e2t_translation[i].creator;
			break;
		}
	}
}

void set_finder_type(const char *path, uint32 type, uint32 creator)
{
}


/*
 *  Get/set finder flags for file/dir specified by full path (MACOS:HFS_FLAGS attribute)
 */

void get_finder_flags(const char *path, uint16 &flags)
{
	flags = DEFAULT_FINDER_FLAGS;	// Default
}

void set_finder_flags(const char *path, uint16 flags)
{
}


/*
 *  Resource fork emulation functions
 */

uint32 get_rfork_size(const char *path)
{
	return 0;
}

int open_rfork(const char *path, int flag)
{
	return -1;
}

void close_rfork(const char *path, int fd)
{
}


/*
 *  Read "length" bytes from file to "buffer",
 *  returns number of bytes read (or 0)
 */

size_t extfs_read(int fd, void *buffer, size_t length)
{
	errno = 0;
	return read(fd, buffer, length);
}


/*
 *  Write "length" bytes from "buffer" to file,
 *  returns number of bytes written (or 0)
 */

size_t extfs_write(int fd, void *buffer, size_t length)
{
	errno = 0;
	return write(fd, buffer, length);
}
