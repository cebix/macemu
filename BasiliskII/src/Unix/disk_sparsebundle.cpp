/*
 *  disk_sparsebundle.cpp - Apple sparse bundle implementation
 *
 *  Basilisk II (C) Dave Vasilevsky
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

#include "disk_unix.h"

#include <limits.h>
 
// TODO
// - Factory needs to actually check stuff
// - Add to basilisk build
// - Add to 'make links'

typedef ssize_t (band_func)(int fd, void *buf, size_t len);

static ssize_t band_read(int fd, void *buf, size_t len) {
	ssize_t err = (fd == -1 ? 0 : ::read(fd, buf, len));
	if (err == -1)
		return err;
	if (err < len)
		memset((char*)buf + err, 0, len - err);
	return len;
}

static ssize_t band_write(int fd, void *buf, size_t len) {
	return ::write(fd, buf, len);
}

struct disk_sparsebundle : disk_generic {
	disk_sparsebundle(char *bands, int fd, bool read_only, loff_t band_size,
			loff_t total_size)
	: token_fd(fd), read_only(read_only), band_size(band_size),
			total_size(total_size), band_dir(bands), band_cur(-1), band_fd(0) {
	}
	
	virtual ~disk_sparsebundle() {
		if (band_fd)
			close(band_fd);
		close(token_fd);
		free(band_dir);
	}
	
	virtual bool is_read_only() { return read_only; }
	virtual loff_t size() { return total_size; }
	
	virtual size_t read(void *buf, loff_t offset, size_t length) {
		return band_do(&band_read, buf, offset, length);
	}
	
	virtual size_t write(void *buf, loff_t offset, size_t length) {
		return band_do(&band_write, buf, offset, length);
	}
	
protected:
	int token_fd;
	bool read_only;
	loff_t band_size, total_size;
	char *band_dir;
	
	loff_t band_cur;
	int band_fd;
	
	size_t band_do(band_func func, void *buf, loff_t offset, size_t length) {
		char *b = (char*)buf;
		loff_t band = offset / band_size;
		size_t done = 0;
		while (length) {
			if (!open_band(band))
				break;
			
			size_t start = offset % band_size;
			size_t segment = band_size - start;
			if (segment > length)
				segment = length;
			
			if (band_fd != -1 && lseek(band_fd, start, SEEK_SET) == -1)
				return done;
			ssize_t err = func(band_fd, buf, segment);
			if (err > 0)
				done += err;
			if (err < segment)
				break;
			
			b += segment;
			offset += segment;
			length -= segment;
			++band;
		}
		return done;
	}
	
	// Open band index 'band', return false on error
	bool open_band(loff_t band) {
		if (band_cur == band)
			return true;
		
		char path[PATH_MAX + 1];
		if (snprintf(path, PATH_MAX, "%s/%lx", band_dir,
				(unsigned long)band) >= PATH_MAX) {
			return false;
		}
		
		int oflags = read_only ? O_RDONLY : (O_RDWR | O_CREAT);
		band_fd = open(path, oflags, 0644);
		if (band_fd == -1) {
			if (read_only) {
				band_cur == -1;
				return true;
			} else {
				return false;
			}
		}
		
		band_cur = band;
		return true;
	}
};

disk_generic *disk_sparsebundle_factory(const char *path, bool read_only) {
	if (strstr(path, ".sparsebundle") == NULL)
		return NULL; // FIXME: real test
	
	// FIXME: actually read these
	loff_t band_size = 1048576;
	loff_t total_size = 53687091200;
	
	// FIXME: check for double-mount, writable
	int token = 0;
	read_only = false;
	
	char buf[PATH_MAX + 1];
	if (snprintf(buf, PATH_MAX, "%s/%s", path, "bands") >= PATH_MAX)
		return NULL;
	char *bands = strdup(buf);
	
	return new disk_sparsebundle(bands, token, read_only, band_size,
		total_size);
}
