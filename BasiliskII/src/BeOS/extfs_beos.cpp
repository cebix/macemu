/*
 *  extfs_beos.cpp - MacOS file system for access native file system access, BeOS specific stuff
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

#include "sysdeps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <fs_attr.h>
#include <support/TypeConstants.h>
#include <storage/Mime.h>

#include "extfs.h"
#include "extfs_defs.h"

#define DEBUG 0
#include "debug.h"


// Default Finder flags
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;

// Temporary buffer for transfers from/to kernel space
const int TMP_BUF_SIZE = 0x10000;
static uint8 *tmp_buf = NULL;


/*
 *  Initialization
 */

void extfs_init(void)
{
	// Allocate temporary buffer
	tmp_buf = new uint8[TMP_BUF_SIZE];
}


/*
 *  Deinitialization
 */

void extfs_exit(void)
{
	// Delete temporary buffer
	delete[] tmp_buf;
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
 *  Get/set finder type/creator for file specified by full path
 */

struct mime2type {
	const char *mime;
	uint32 type;
	uint32 creator;
	bool reversible;	// type -> mime translation possible
};

static const mime2type m2t_translation[] = {
	{"application/x-compress", 'ZIVM', 'LZIV', true},
	{"application/x-gzip", 'Gzip', 'Gzip', true},
	{"application/x-macbinary", 'BINA', '????', false},
	{"application/mac-binhex40", 'TEXT', 'SITx', false},
	{"application/pdf", 'PDF ', 'CARO', true},
	{"application/postscript", 'TEXT', 'ttxt', false},
	{"application/x-stuffit", 'SIT!', 'SITx', true},
	{"application/x-tar", 'TARF', 'TAR ', true},
	{"application/x-uuencode", 'TEXT', 'SITx', false},
	{"application/zip", 'ZIP ', 'ZIP ', true},
	{"audio/x-8svx", '8SVX', 'SNDM', true},
	{"audio/x-aifc", 'AIFC', 'TVOD', true},
	{"audio/x-aiff", 'AIFF', 'TVOD', true},
	{"audio/basic", 'ULAW', 'TVOD', true},
	{"audio/x-midi", 'MIDI', 'TVOD', true},
	{"audio/x-mpeg", 'MPG ', 'TVOD', true},
	{"audio/x-wav", 'WAVE', 'TVOD', true},
	{"image/x-bmp", 'BMPf', 'ogle', true},
	{"image/gif", 'GIFf', 'ogle', true},
	{"image/x-ilbm", 'ILBM', 'GKON', true},
	{"image/jpeg", 'JPEG', 'ogle', true},
	{"image/jpeg", 'JFIF', 'ogle', true},
	{"image/x-photoshop", '8BPS', '8BIM', true},
	{"image/pict", 'PICT', 'ogle', true},
	{"image/png", 'PNGf', 'ogle', true},
	{"image/x-sgi", '.SGI', 'ogle', true},
	{"image/x-targa", 'TPIC', 'ogle', true},
	{"image/tiff", 'TIFF', 'ogle', true},
	{"text/html", 'TEXT', 'MOSS', false},
	{"text/plain", 'TEXT', 'ttxt', true},
	{"text/rtf", 'TEXT', 'MSWD', false},
	{"text/x-source-code", 'TEXT', 'R*ch', false},
	{"video/mpeg", 'MPEG', 'TVOD', true},
	{"video/quicktime", 'MooV', 'TVOD', true},
	{"video/x-flc", 'FLI ', 'TVOD', true},
	{"video/x-msvideo", 'VfW ', 'TVOD', true},
	{NULL, 0, 0, false}	// End marker
};

void get_finder_type(const char *path, uint32 &type, uint32 &creator)
{
	type = 0;
	creator = 0;

	// Open file
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return;

	// Read BeOS MIME type and close file
	char mime[256];
	ssize_t actual = fs_read_attr(fd, "BEOS:TYPE", B_MIME_STRING_TYPE, 0, mime, 256);
	mime[255] = 0;

	if (actual > 0) {

		// Translate MIME type to MacOS type/creator
		char mactype[4];
		if (sscanf(mime, "application/x-MacOS-%c%c%c%c", mactype, mactype+1, mactype+2, mactype+3) == 4) {

			// MacOS style type
			memcpy(&type, mactype, 4);

		} else {

			// MIME string, look in table
			for (int i=0; m2t_translation[i].mime; i++) {
				if (!strcmp(mime, m2t_translation[i].mime)) {
					type = m2t_translation[i].type;
					creator = m2t_translation[i].creator;
					break;
				}
			}
		}
	}

	// Override file type with MACOS:CREATOR attribute
	fs_read_attr(fd, "MACOS:CREATOR", B_UINT32_TYPE, 0, &creator, 4);

	// Close file
	close(fd);
}

void set_finder_type(const char *path, uint32 type, uint32 creator)
{
	// Open file
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return;

	// Set BEOS:TYPE attribute
	if (type) {
		bool written = false;
		for (int i=0; m2t_translation[i].mime; i++) {
			if (m2t_translation[i].type == type && m2t_translation[i].reversible) {
				fs_write_attr(fd, "BEOS:TYPE", B_MIME_STRING_TYPE, 0, m2t_translation[i].mime, strlen(m2t_translation[i].mime) + 1);
				written = true;
				break;
			}
		}
		if (!written) {
			char mime[256];
			sprintf(mime, "application/x-MacOS-%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
			fs_write_attr(fd, "BEOS:TYPE", B_MIME_STRING_TYPE, 0, mime, strlen(mime) + 1);
		}
	}

	// Set MACOS:CREATOR attribute
	if (creator)
		fs_write_attr(fd, "MACOS:CREATOR", B_UINT32_TYPE, 0, &creator, 4);

	// Close file
	close(fd);
}


/*
 *  Get/set finder flags for file/dir specified by full path (MACOS:HFS_FLAGS attribute)
 */

void get_finder_flags(const char *path, uint16 &flags)
{
	flags = DEFAULT_FINDER_FLAGS;	// Default

	// Open file
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return;

	// Read MACOS:HFS_FLAGS attribute
	fs_read_attr(fd, "MACOS:HFS_FLAGS", B_UINT16_TYPE, 0, &flags, 2);

	// Close file
	close(fd);
}

void set_finder_flags(const char *path, uint16 flags)
{
	// Open file
	int fd = open(path, O_WRONLY);
	if (fd < 0)
		return;

	// Write MACOS:HFS_FLAGS attribute
	if (flags != DEFAULT_FINDER_FLAGS)
		fs_write_attr(fd, "MACOS:HFS_FLAGS", B_UINT16_TYPE, 0, &flags, 2);
	else
		fs_remove_attr(fd, "MACOS:HFS_FLAGS");

	// Close file
	close(fd);
}


/*
 *  Resource fork emulation functions
 */

uint32 get_rfork_size(const char *path)
{
	// Open file
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return 0;

	// Get size of MACOS:RFORK attribute
	struct attr_info info;
	if (fs_stat_attr(fd, "MACOS:RFORK", &info) < 0)
		info.size = 0;

	// Close file and return size
	close(fd);
	return info.size;
}

int open_rfork(const char *path, int flag)
{
	// Open original file
	int fd = open(path, flag);
	if (fd < 0)
		return -1;

	// Open temporary file for resource fork
	char rname[L_tmpnam];
	tmpnam(rname);
	int rfd = open(rname, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (rfd < 0) {
		close(fd);
		return -1;
	}
	unlink(rname);	// File will be deleted when closed

	// Get size of MACOS:RFORK attribute
	struct attr_info info;
	if (fs_stat_attr(fd, "MACOS:RFORK", &info) < 0)
		info.size = 0;

	// Copy resource data from attribute to temporary file
	if (info.size > 0) {

		// Allocate buffer
		void *buf = malloc(info.size);
		if (buf == NULL) {
			close(rfd);
			close(fd);
			return -1;
		}

		// Copy data
		fs_read_attr(fd, "MACOS:RFORK", B_RAW_TYPE, 0, buf, info.size);
		write(rfd, buf, info.size);
		lseek(rfd, 0, SEEK_SET);

		// Free buffer
		if (buf)
			free(buf);
	}

	// Close original file
	close(fd);
	return rfd;
}

void close_rfork(const char *path, int fd)
{
	if (fd < 0)
		return;

	// Get size of temporary file
	struct stat st;
	if (fstat(fd, &st) < 0)
		st.st_size = 0;

	// Open original file
	int ofd = open(path, O_WRONLY);
	if (ofd > 0) {

		// Copy resource data to MACOS:RFORK attribute
		if (st.st_size > 0) {

			// Allocate buffer
			void *buf = malloc(st.st_size);
			if (buf == NULL) {
				close(ofd);
				close(fd);
				return;
			}

			// Copy data
			lseek(fd, 0, SEEK_SET);
			read(fd, buf, st.st_size);
			fs_write_attr(ofd, "MACOS:RFORK", B_RAW_TYPE, 0, buf, st.st_size);

			// Free buffer
			if (buf)
				free(buf);

		} else
			fs_remove_attr(ofd, "MACOS:RFORK");

		// Close original file
		close(ofd);
	}

	// Close temporary file
	close(fd);
}


/*
 *  Read "length" bytes from file to "buffer",
 *  returns number of bytes read (or -1 on error)
 */

static inline ssize_t sread(int fd, void *buf, size_t count)
{
	ssize_t res;
	while ((res = read(fd, buf, count)) == B_INTERRUPTED) ;
	return res;
}

ssize_t extfs_read(int fd, void *buffer, size_t length)
{
	// Buffer in kernel space?
	if ((uint32)buffer < 0x80000000) {

		// Yes, transfer via buffer
		ssize_t actual = 0;
		while (length) {
			size_t transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
			ssize_t res = sread(fd, tmp_buf, transfer_size);
			if (res < 0)
				return res;
			memcpy(buffer, tmp_buf, res);
			buffer = (void *)((uint8 *)buffer + res);
			length -= res;
			actual += res;
			if (res != transfer_size)
				return actual;
		}
		return actual;

	} else {

		// No, transfer directly
		return sread(fd, buffer, length);
	}
}


/*
 *  Write "length" bytes from "buffer" to file,
 *  returns number of bytes written (or -1 on error)
 */

static inline ssize_t swrite(int fd, void *buf, size_t count)
{
	ssize_t res;
	while ((res = write(fd, buf, count)) == B_INTERRUPTED) ;
	return res;
}

ssize_t extfs_write(int fd, void *buffer, size_t length)
{
	// Buffer in kernel space?
	if ((uint32)buffer < 0x80000000) {

		// Yes, transfer via buffer
		ssize_t actual = 0;
		while (length) {
			size_t transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
			memcpy(tmp_buf, buffer, transfer_size);
			ssize_t res = swrite(fd, tmp_buf, transfer_size);
			if (res < 0)
				return res;
			buffer = (void *)((uint8 *)buffer + res);
			length -= res;
			actual += res;
			if (res != transfer_size)
				return actual;
		}
		return actual;

	} else {

		// No, transfer directly
		return swrite(fd, buffer, length);
	}
}


/*
 *  Remove file/directory, returns false on error (and sets errno)
 */

bool extfs_remove(const char *path)
{
	if (remove(path) < 0) {
		if (errno == EISDIR)
			return rmdir(path) == 0;
		else
			return false;
	}
	return true;
}


/*
 *  Rename/move file/directory, returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	return rename(old_path, new_path) == 0;
}
