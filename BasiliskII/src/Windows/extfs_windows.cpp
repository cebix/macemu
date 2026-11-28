/*
 *  extfs_windows.cpp - MacOS file system for access native file system access, Windows specific stuff
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
#include "main.h"
#include "extfs.h"
#include "extfs_defs.h"
#include "posix_emu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


#define DEBUG 0
#include "debug.h"


// Constants
#define HOST_DIRSEP_CHAR '\\'
#define HOST_DIRSEP_STR  "\\"


// Default Finder flags
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;


/*
 *  Initialization
 */

void extfs_init(void)
{
	init_posix_emu();
}


/*
 *  Deinitialization
 */

void extfs_exit(void)
{
	final_posix_emu();
}


/*
 *  Add component to path name
 */

void add_path_component(char *path, const char *component)
{
	size_t l = strlen(path);
	// On Windows, RootPath is "" so a leading '\' is required to distinguish host drives from Virtual Desktop items
	if (((l > 0 && path[l - 1] != HOST_DIRSEP_CHAR) || (l == 0 && component[0] != 0 && component[0] != HOST_DIRSEP_CHAR)) && l < MAX_PATH_LENGTH - 1) {
		path[l] = HOST_DIRSEP_CHAR;
		path[l + 1] = 0;
		l++;
	}
	if (l < MAX_PATH_LENGTH - 1)
		strncat(path, component, MAX_PATH_LENGTH - 1 - l);
}


/*
 *  Finder info and resource forks are kept in helper files
 *
 *  Finder info:
 *    /path/.finf/file
 *  Resource fork:
 *    /path/.rsrc/file
 *
 *  The .finf files store a FInfo/DInfo, followed by a FXInfo/DXInfo
 *  (16+16 bytes)
 */

static void make_helper_path(const char *src, char *dest, const char *add, bool only_dir = false)
{
	dest[0] = 0;

	// Get pointer to last component of path
	const char *last_part = strrchr(src, HOST_DIRSEP_CHAR);
	if (last_part)
		last_part++;
	else
		last_part = src;

	// Copy everything before
	size_t prefix_len = (size_t)(last_part - src);
	if (prefix_len >= MAX_PATH_LENGTH)
		prefix_len = MAX_PATH_LENGTH - 1;
	memcpy(dest, src, prefix_len);
	dest[prefix_len] = 0;

	// Add additional component
	size_t cur_len = strlen(dest);
	if (cur_len < MAX_PATH_LENGTH - 1)
		strncat(dest, add, MAX_PATH_LENGTH - 1 - cur_len);

	// Add last component
	if (!only_dir) {
		cur_len = strlen(dest);
		if (cur_len < MAX_PATH_LENGTH - 1)
			strncat(dest, last_part, MAX_PATH_LENGTH - 1 - cur_len);
	}
}

static int create_helper_dir(const char *path, const char *add)
{
	char helper_dir[MAX_PATH_LENGTH];
	make_helper_path(path, helper_dir, add, true);
	size_t len = strlen(helper_dir);
	if (len > 0 && helper_dir[len - 1] == HOST_DIRSEP_CHAR)	// Remove trailing "\"
		helper_dir[len - 1] = 0;
	return mkdir(helper_dir, 0777);
}

static int open_helper(const char *path, const char *add, int flag)
{
	char helper_path[MAX_PATH_LENGTH];
	make_helper_path(path, helper_path, add);

	switch (flag & (_O_RDONLY | _O_WRONLY | _O_RDWR)) {
	case _O_WRONLY:
	case _O_RDWR:
		flag |= O_CREAT;
		break;
	}

	int fd = open(helper_path, flag, 0666);
	if (fd < 0) {
		if (/*errno == ENOENT &&*/ (flag & O_CREAT)) {
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
	return open_helper(path, ".finf" HOST_DIRSEP_STR, flag);
}

static int open_rsrc(const char *path, int flag)
{
	return open_helper(path, ".rsrc" HOST_DIRSEP_STR, flag);
}


/*
 *  Get/set finder info for file/directory specified by full path
 */

struct ext2type {
	const char *ext;
	uint32 type;
	uint32 creator;
};

static const ext2type e2t_translation[] = {
	{".Z", FOURCC('Z','I','V','M'), FOURCC('L','Z','I','V')},
	{".gz", FOURCC('G','z','i','p'), FOURCC('G','z','i','p')},
	{".hqx", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".bin", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".pdf", FOURCC('P','D','F',' '), FOURCC('C','A','R','O')},
	{".ps", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".sit", FOURCC('S','I','T','!'), FOURCC('S','I','T','x')},
	{".tar", FOURCC('T','A','R','F'), FOURCC('T','A','R',' ')},
	{".uu", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".uue", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".zip", FOURCC('Z','I','P',' '), FOURCC('Z','I','P',' ')},
	{".8svx", FOURCC('8','S','V','X'), FOURCC('S','N','D','M')},
	{".aifc", FOURCC('A','I','F','C'), FOURCC('T','V','O','D')},
	{".aiff", FOURCC('A','I','F','F'), FOURCC('T','V','O','D')},
	{".au", FOURCC('U','L','A','W'), FOURCC('T','V','O','D')},
	{".mid", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".midi", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".mp2", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".mp3", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".wav", FOURCC('W','A','V','E'), FOURCC('T','V','O','D')},
	{".bmp", FOURCC('B','M','P','f'), FOURCC('o','g','l','e')},
	{".gif", FOURCC('G','I','F','f'), FOURCC('o','g','l','e')},
	{".lbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".ilbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".jpg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".jpeg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".pict", FOURCC('P','I','C','T'), FOURCC('o','g','l','e')},
	{".png", FOURCC('P','N','G','f'), FOURCC('o','g','l','e')},
	{".sgi", FOURCC('.','S','G','I'), FOURCC('o','g','l','e')},
	{".tga", FOURCC('T','P','I','C'), FOURCC('o','g','l','e')},
	{".tif", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".tiff", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".htm", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".html", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".txt", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".rtf", FOURCC('T','E','X','T'), FOURCC('M','S','W','D')},
	{".c", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".C", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cc", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".h", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hh", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".s", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".S", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".i", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".mpg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mpeg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mov", FOURCC('M','o','o','V'), FOURCC('T','V','O','D')},
	{".fli", FOURCC('F','L','I',' '), FOURCC('T','V','O','D')},
	{".avi", FOURCC('V','f','W',' '), FOURCC('T','V','O','D')},
	{".qxd", FOURCC('X','D','O','C'), FOURCC('X','P','R','3')},
	{".hfv", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".dsk", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".img", FOURCC('r','o','h','d'), FOURCC('d','d','s','k')},
	{NULL, 0, 0}	// End marker
};

void get_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	// Set default finder info (folders must keep DInfo/DXInfo zeroed by default)
	Mac_memset(finfo, 0, SIZEOF_FInfo);
	if (fxinfo)
		Mac_memset(fxinfo, 0, SIZEOF_FXInfo);
	if (!is_dir) {
		WriteMacInt16(finfo + fdFlags, DEFAULT_FINDER_FLAGS);
		WriteMacInt32(finfo + fdLocation, (uint32)-1);
	}

	// Read Finder info file
	int fd = open_finf(path, O_RDONLY);
	if (fd >= 0) {
		ssize_t actual = read(fd, Mac2HostAddr(finfo), SIZEOF_FInfo);
		if (fxinfo)
			actual += read(fd, Mac2HostAddr(fxinfo), SIZEOF_FXInfo);
		close(fd);
		if (actual >= SIZEOF_FInfo) {
			if (is_dir)
				WriteMacInt16(finfo + frFlags, ReadMacInt16(finfo + frFlags) & kFolderValidFlagsMask);
			return;
		}
	}

	// No Finder info file, translate file name extension to MacOS type/creator
	if (!is_dir) {
		int path_len = strlen(path);
		for (int i=0; e2t_translation[i].ext; i++) {
			int ext_len = strlen(e2t_translation[i].ext);
			if (path_len < ext_len)
				continue;
			if (!_stricmp(path + path_len - ext_len, e2t_translation[i].ext)) {
				WriteMacInt32(finfo + fdType, e2t_translation[i].type);
				WriteMacInt32(finfo + fdCreator, e2t_translation[i].creator);
				break;
			}
		}
	}
}

void set_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	struct my_utimbuf times;
	times.actime = MacTimeToTime(ReadMacInt32(finfo - ioFlFndrInfo + ioFlCrDat));
	times.modtime = MacTimeToTime(ReadMacInt32(finfo - ioFlFndrInfo + ioFlMdDat));

	if (utime(path, &times) < 0) {
		D(bug("utime failed on %s, error %d\n", path, GetLastError()));
	}

	// Open Finder info file
	int fd = open_finf(path, O_RDWR);
	if (fd < 0)
		return;

	uint8 clean_finfo[SIZEOF_FInfo];
	Mac2Host_memcpy(clean_finfo, finfo, SIZEOF_FInfo);
	if (is_dir) {
		uint16 flags = (((uint16)clean_finfo[frFlags] << 8) | clean_finfo[frFlags + 1]) & kFolderValidFlagsMask;
		clean_finfo[frFlags] = (uint8)(flags >> 8);
		clean_finfo[frFlags + 1] = (uint8)(flags & 0xff);
	}

	// Write file
	write(fd, clean_finfo, SIZEOF_FInfo);
	if (fxinfo)
		write(fd, Mac2HostAddr(fxinfo), SIZEOF_FXInfo);
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
	if (size < 0)
		return 0;
	return size > (off_t)0x7fffffffU ? 0x7fffffffU : (uint32)size;
}

int open_rfork(const char *path, int flag)
{
	return open_rsrc(path, flag);
}

void close_rfork(const char *path, int fd)
{
	if (fd >= 0)
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
	char helper_path[MAX_PATH_LENGTH];

	// Remove file or directory first so failed removals never destroy metadata
	if (remove(path) < 0) {
		if (errno == EISDIR || errno == ENOTEMPTY) {
			strncpy(helper_path, path, MAX_PATH_LENGTH - 1);
			helper_path[MAX_PATH_LENGTH - 1] = 0;
			add_path_component(helper_path, ".finf");
			rmdir(helper_path);
			strncpy(helper_path, path, MAX_PATH_LENGTH - 1);
			helper_path[MAX_PATH_LENGTH - 1] = 0;
			add_path_component(helper_path, ".rsrc");
			rmdir(helper_path);
			if (rmdir(path) < 0)
				return false;
		} else {
			return false;
		}
	}

	// Remove helpers if present
	make_helper_path(path, helper_path, ".finf" HOST_DIRSEP_STR, false);
	remove(helper_path);
	make_helper_path(path, helper_path, ".rsrc" HOST_DIRSEP_STR, false);
	remove(helper_path);
	return true;
}


/*
 *  Rename/move file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	// Rename file/directory first
	if (rename(old_path, new_path) < 0)
		return false;

	// Migrate helpers if present
	char old_helper_path[MAX_PATH_LENGTH], new_helper_path[MAX_PATH_LENGTH];
	make_helper_path(old_path, old_helper_path, ".finf" HOST_DIRSEP_STR, false);
	if (access(old_helper_path, F_OK) == 0) {
		make_helper_path(new_path, new_helper_path, ".finf" HOST_DIRSEP_STR, false);
		create_helper_dir(new_path, ".finf" HOST_DIRSEP_STR);
		rename(old_helper_path, new_helper_path);
	}
	make_helper_path(old_path, old_helper_path, ".rsrc" HOST_DIRSEP_STR, false);
	if (access(old_helper_path, F_OK) == 0) {
		make_helper_path(new_path, new_helper_path, ".rsrc" HOST_DIRSEP_STR, false);
		create_helper_dir(new_path, ".rsrc" HOST_DIRSEP_STR);
		rename(old_helper_path, new_helper_path);
	}
	return true;
}


// Convert from the host OS filename encoding to MacRoman
const char *host_encoding_to_macroman(const char *filename)
{
	return filename;
}

// Convert from MacRoman to host OS filename encoding
const char *macroman_to_host_encoding(const char *filename)
{
	return filename;
}

