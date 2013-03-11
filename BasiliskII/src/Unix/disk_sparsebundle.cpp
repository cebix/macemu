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
#include "tinyxml2.h"

#include <errno.h>
#include <limits.h>
#include <algorithm>

#if defined __APPLE__ && defined __MACH__
#define __MACOSX__ 1
#endif

struct disk_sparsebundle : disk_generic {
	disk_sparsebundle(const char *bands, int fd, bool read_only,
		loff_t band_size, loff_t total_size)
	: token_fd(fd), read_only(read_only), band_size(band_size),
		total_size(total_size), band_dir(strdup(bands)),
		band_cur(-1), band_fd(-1), band_alloc(-1) {
	}
	
	virtual ~disk_sparsebundle() {
		if (band_fd != -1)
			close(band_fd);
		close(token_fd);
		free(band_dir);
	}
	
	virtual bool is_read_only() { return read_only; }
	virtual loff_t size() { return total_size; }
	
	virtual size_t read(void *buf, loff_t offset, size_t length) {
		return band_do(&disk_sparsebundle::band_read, buf, offset, length);
	}
	
	virtual size_t write(void *buf, loff_t offset, size_t length) {
		return band_do(&disk_sparsebundle::band_write, buf, offset, length);
	}
	
protected:
	int token_fd;			// lockfile
	bool read_only;
	loff_t band_size, total_size;
	char *band_dir;			// directory containing band files
	
	// Currently open band
	loff_t band_cur;		// index of the band
	int band_fd;			// -1 if not open
	loff_t band_alloc;		// how much space is already used?
	
	typedef ssize_t (disk_sparsebundle::*band_func)(char *buf, loff_t band,
		size_t offset, size_t len);
	
	// Split an (offset, length) operation into bands.
	size_t band_do(band_func func, void *buf, loff_t offset, size_t length) {
		char *b = (char*)buf;
		loff_t band = offset / band_size;
		size_t done = 0;
		while (length) {
			if (offset >= total_size)
				break;
			size_t start = offset % band_size;
			size_t segment = std::min((size_t)band_size - start, length);
			
			ssize_t err = (this->*func)(b, band, start, segment);
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
		
	// Open a band by index. It's ok if the band is already open.
	enum open_ret {
		OPEN_FAILED = 0,
		OPEN_NOENT,		// Band doesn't exist yet
		OPEN_OK,
	};
	open_ret open_band(loff_t band, bool create) {
		if (band_cur == band)
			return OPEN_OK;
		
		char path[PATH_MAX + 1];
		if (snprintf(path, PATH_MAX, "%s/%lx", band_dir,
				(unsigned long)band) >= PATH_MAX) {
			return OPEN_FAILED;
		}
		
		if (band_fd != -1)
			close(band_fd);
		band_alloc = -1;
		band_cur = -1;
		
		int oflags = read_only ? O_RDONLY : O_RDWR;
		if (create)
			oflags |= O_CREAT;
		band_fd = open(path, oflags, 0644);
		if (band_fd == -1) {
			return (!create && errno == ENOENT) ? OPEN_NOENT : OPEN_FAILED;
		}
		
		// Get the allocated size
		if (!read_only) {
			band_alloc = lseek(band_fd, 0, SEEK_END);
			if (band_alloc == -1)
				band_alloc = band_size;
		}
		band_cur = band;
		return OPEN_OK;
	}
	
	ssize_t band_read(char *buf, loff_t band, size_t off, size_t len) {
		open_ret st = open_band(band, false);
		if (st == OPEN_FAILED)
			return -1;
		
		// Unallocated bytes 
		size_t want = (st == OPEN_NOENT || off >= band_alloc) ? 0
			: std::min(len, (size_t)band_alloc - off);
		if (want) {
			if (lseek(band_fd, off, SEEK_SET) == -1)
				return -1;
			ssize_t err = ::read(band_fd, buf, want);
			if (err < want)
				return err;
		}
		memset(buf + want, 0, len - want);
		return len;
	}

	ssize_t band_write(char *buf, loff_t band, size_t off, size_t len) {
		// If space is unused, don't needlessly fill it with zeros
		
		// Find min length such that all trailing chars are zero:
		size_t nz = len;
		for (; nz > 0 && !buf[nz-1]; --nz)
			; // pass
		
		open_ret st = open_band(band, nz);
		if (st != OPEN_OK)
			return st == OPEN_NOENT ? len : -1;

		if (lseek(band_fd, off, SEEK_SET) == -1)
			return -1;
		
		size_t space = (off >= band_alloc ? 0 : band_alloc - off);
		size_t want = std::max(nz, std::min(space, len));
		ssize_t err = ::write(band_fd, buf, want);
		if (err >= 0)
			band_alloc = std::max(band_alloc, loff_t(off + err));
		if (err < want)
			return err;
		return len;
	}
};



using tinyxml2::XML_NO_ERROR;
using tinyxml2::XMLElement;

// Simplistic plist parser
struct plist {
	plist() : doc(true, tinyxml2::COLLAPSE_WHITESPACE) { }
	
	bool open(const char *path) {
		if (doc.LoadFile(path) != XML_NO_ERROR)
			return false;
		tinyxml2::XMLHandle hnd(&doc);
		dict = hnd.FirstChildElement("plist").FirstChildElement("dict")
			.ToElement();
		return dict;
	}
	
	const char *str_val(const char *key) {
		return value(key, "string");
	}

	bool int_val(const char *key, loff_t *i) {
		const char *v = value(key, "integer");
		if (!v || !*v)
			return false;
		
		char *endp;
		long long ll = strtoll(v, &endp, 10);
		if (*endp)
			return false;
		*i = ll;
		return true;
	}
	
protected:
	tinyxml2::XMLDocument doc;
	XMLElement *dict;
	
	const char *value(const char *key, const char *type) {
		// Assume it's a flat plist
		XMLElement *cur = dict->FirstChildElement();
		bool found_key = false;
		while (cur) {
			if (found_key) {
				if (strcmp(cur->Name(), type) != 0)
					return NULL;
				return cur->GetText();
			}
			found_key = strcmp(cur->Name(), "key") == 0
				&& strcmp(cur->GetText(), key) == 0;
			cur = cur->NextSiblingElement();
		}
		return NULL;
	}
};


static int try_open(const char *path, bool read_only, bool *locked) {
	int oflags = (read_only ? O_RDONLY : O_RDWR);
	int lockflags = 0;
#if defined(__MACOSX__)
	lockflags = O_NONBLOCK | (read_only ? O_SHLOCK : O_EXLOCK);
#endif
	int fd = open(path, oflags | lockflags);
#if defined(__MACOSX__)
	if (fd == -1) {
		if (errno == EOPNOTSUPP) { // no locking support, try again
			fd = open(path, oflags);
		} else if (errno == EAGAIN) { // locked
			*locked = true;
		}
	}
#endif
	return fd;
}

disk_generic::status disk_sparsebundle_factory(const char *path,
		bool read_only, disk_generic **disk) {
	// Does it look like a sparsebundle?
	char buf[PATH_MAX + 1];
	if (snprintf(buf, PATH_MAX, "%s/%s", path, "Info.plist") >= PATH_MAX)
		return disk_generic::DISK_UNKNOWN;
	
	plist pl;
	if (!pl.open(buf))
		return disk_generic::DISK_UNKNOWN;
	
	const char *type;
	if (!(type = pl.str_val("diskimage-bundle-type")))
		return disk_generic::DISK_UNKNOWN;
	if (strcmp(type, "com.apple.diskimage.sparsebundle") != 0)
		return disk_generic::DISK_UNKNOWN;
	
	
	// Find the sparsebundle parameters
	loff_t version, band_size, total_size;
	if (!pl.int_val("bundle-backingstore-version", &version) || version != 1) {
		fprintf(stderr, "sparsebundle: Bad version\n");
		return disk_generic::DISK_UNKNOWN;
	}
	if (!pl.int_val("band-size", &band_size)
			|| !pl.int_val("size", &total_size)) {
		fprintf(stderr, "sparsebundle: Can't find size\n");
		return disk_generic::DISK_INVALID;
	}
	
	
	// Check if we can open it
	if (snprintf(buf, PATH_MAX, "%s/%s", path, "token") >= PATH_MAX)
		return disk_generic::DISK_INVALID;
	bool locked = false;
	int token = try_open(buf, read_only, &locked);
	if (token == -1 && !read_only) { // try again, read-only
		token = try_open(buf, true, &locked);
		if (token != -1 && !read_only)
			fprintf(stderr, "sparsebundle: Can only mount read-only\n");
		read_only = true;
	}
	if (token == -1) {
		if (locked)
			fprintf(stderr, "sparsebundle: Refusing to double-mount\n");
		else
			perror("sparsebundle: open failed:");
		return disk_generic::DISK_INVALID;
	}
	
	
	// We're good to go!
	if (snprintf(buf, PATH_MAX, "%s/%s", path, "bands") >= PATH_MAX)
		return disk_generic::DISK_INVALID;
	*disk = new disk_sparsebundle(buf, token, read_only, band_size,
		total_size);
	return disk_generic::DISK_VALID;
}
