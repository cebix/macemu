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

void add_path_component(char *path, const char *component)
{
	int l = strlen(path);
	if (l < MAX_PATH_LENGTH-1 && path[l-1] != '/') {
		path[l] = '/';
		path[l+1] = 0;
	}
	strncat(path, component, MAX_PATH_LENGTH-1);
}


/*
 *  Finder info and resource forks are kept in helper files
 *
 *  Finder info:
 *    /path/.finf/file
 *  Resource fork:
 *    /path/.rsrc/file
 */

// Layout of Finder info helper files (all fields big-endian)
struct finf_struct {
	uint32 type;
	uint32 creator;
	uint16 flags;
	uint8 pad0[22];	// total size: 32 bytes to match the size of FInfo+FXInfo
};

static void make_helper_path(const char *src, char *dest, const char *add, bool only_dir = false)
{
	dest[0] = 0;

	// Get pointer to last component of path
	const char *last_part = strrchr(src, '/');
	if (last_part)
		last_part++;
	else
		last_part = src;

	// Copy everything before
	strncpy(dest, src, last_part-src);
	dest[last_part-src] = 0;

	// Add additional component
	strncat(dest, add, MAX_PATH_LENGTH-1);

	// Add last component
	if (!only_dir)
		strncat(dest, last_part, MAX_PATH_LENGTH-1);
}

static int create_helper_dir(const char *path, const char *add)
{
	char helper_dir[MAX_PATH_LENGTH];
	make_helper_path(path, helper_dir, add, true);
	return mkdir(helper_dir, 0777);
}

static int open_helper(const char *path, const char *add, int flag)
{
	char helper_path[MAX_PATH_LENGTH];
	make_helper_path(path, helper_path, add);

	if ((flag & O_ACCMODE) == O_RDWR || (flag & O_ACCMODE) == O_WRONLY)
		flag |= O_CREAT;
	int fd = open(helper_path, flag, 0666);
	if (fd < 0) {
		if (errno == ENOENT && (flag & O_CREAT)) {
			// One path component was missing, probably the helper
			// directory. Try to create it and re-open the file.
			int ret = create_helper_dir(path, add);
			if (ret < 0)
				return ret;
			fd = open(helper_path, flag, 0666);
		}
	}
	return fd;
}

static int open_finf(const char *path, int flag)
{
	return open_helper(path, ".finf/", flag);
}

static int open_rsrc(const char *path, int flag)
{
	return open_helper(path, ".rsrc/", flag);
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

	// Open Finder info file
	int fd = open_finf(path, O_RDONLY);
	if (fd >= 0) {

		// Read file
		finf_struct finf;
		if (read(fd, &finf, sizeof(finf_struct)) >= 8) {

			// Type/creator are in Finder info file, return them
			type = ntohl(finf.type);
			creator = ntohl(finf.creator);
			close(fd);
			return;
		}
		close(fd);
	}

	// No Finder info file, translate file name extension to MacOS type/creator
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
	// Open Finder info file
	int fd = open_finf(path, O_RDWR);
	if (fd < 0)
		return;

	// Read file
	finf_struct finf;
	finf.flags = DEFAULT_FINDER_FLAGS;
	memset(&finf, 0, sizeof(finf_struct));
	read(fd, &finf, sizeof(finf_struct));

	// Set Finder flags
	finf.type = htonl(type);
	finf.creator = htonl(creator);

	// Update file
	lseek(fd, 0, SEEK_SET);
	write(fd, &finf, sizeof(finf_struct));
	close(fd);
}


/*
 *  Get/set finder flags for file/dir specified by full path
 */

void get_finder_flags(const char *path, uint16 &flags)
{
	flags = DEFAULT_FINDER_FLAGS;	// Default

	// Open Finder info file
	int fd = open_finf(path, O_RDONLY);
	if (fd < 0)
		return;

	// Read Finder flags
	finf_struct finf;
	if (read(fd, &finf, sizeof(finf_struct)) >= 10)
		flags = ntohs(finf.flags);

	// Close file
	close(fd);
}

void set_finder_flags(const char *path, uint16 flags)
{
	// Open Finder info file
	int fd = open_finf(path, O_RDWR);
	if (fd < 0)
		return;

	// Read file
	finf_struct finf;
	memset(&finf, 0, sizeof(finf_struct));
	finf.flags = DEFAULT_FINDER_FLAGS;
	read(fd, &finf, sizeof(finf_struct));

	// Set Finder flags
	finf.flags = htons(flags);

	// Update file
	lseek(fd, 0, SEEK_SET);
	write(fd, &finf, sizeof(finf_struct));
	close(fd);
}


/*
 *  Resource fork emulation functions
 */

uint32 get_rfork_size(const char *path)
{
	// Open resource file
	int fd = open_rsrc(path, O_RDONLY);
	if (fd < 0)
		return 0;

	// Get size
	off_t size = lseek(fd, 0, SEEK_END);
	
	// Close file and return size
	close(fd);
	return size < 0 ? 0 : size;
}

int open_rfork(const char *path, int flag)
{
	return open_rsrc(path, flag);
}

void close_rfork(const char *path, int fd)
{
	close(fd);
}


/*
 *  Read "length" bytes from file to "buffer",
 *  returns number of bytes read (or -1 on error)
 */

ssize_t extfs_read(int fd, void *buffer, size_t length)
{
	return read(fd, buffer, length);
}


/*
 *  Write "length" bytes from "buffer" to file,
 *  returns number of bytes written (or -1 on error)
 */

ssize_t extfs_write(int fd, void *buffer, size_t length)
{
	return write(fd, buffer, length);
}


/*
 *  Remove file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_remove(const char *path)
{
	// Remove helpers first, don't complain if this fails
	char helper_path[MAX_PATH_LENGTH];
	make_helper_path(path, helper_path, ".finf/", false);
	remove(helper_path);
	make_helper_path(path, helper_path, ".rsrc/", false);
	remove(helper_path);

	// Now remove file or directory (and helper directories in the directory)
	if (remove(path) < 0) {
		if (errno == EISDIR || errno == ENOTEMPTY) {
			helper_path[0] = 0;
			strncpy(helper_path, path, MAX_PATH_LENGTH-1);
			add_path_component(helper_path, ".finf");
			rmdir(helper_path);
			helper_path[0] = 0;
			strncpy(helper_path, path, MAX_PATH_LENGTH-1);
			add_path_component(helper_path, ".rsrc");
			rmdir(helper_path);
			return rmdir(path) == 0;
		} else
			return false;
	}
	return true;
}


/*
 *  Rename/move file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	// Rename helpers first, don't complain if this fails
	char old_helper_path[MAX_PATH_LENGTH], new_helper_path[MAX_PATH_LENGTH];
	make_helper_path(old_path, old_helper_path, ".finf/", false);
	make_helper_path(new_path, new_helper_path, ".finf/", false);
	create_helper_dir(new_path, ".finf/");
	rename(old_helper_path, new_helper_path);
	make_helper_path(old_path, old_helper_path, ".rsrc/", false);
	make_helper_path(new_path, new_helper_path, ".rsrc/", false);
	create_helper_dir(new_path, ".rsrc/");
	rename(old_helper_path, new_helper_path);

	// Now rename file
	return rename(old_path, new_path) == 0;
}
