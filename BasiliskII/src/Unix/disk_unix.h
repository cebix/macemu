/*
 *  disk_unix.h - Generic disk interface
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

#ifndef DISK_UNIX_H
#define DISK_UNIX_H

#include "sysdeps.h"

struct disk_generic {
	enum status {
		DISK_UNKNOWN,
		DISK_INVALID,
		DISK_VALID,
	};
	
	disk_generic() { }
	virtual ~disk_generic() { };
	
	virtual bool is_read_only() = 0;
	virtual size_t read(void *buf, loff_t offset, size_t length) = 0;
	virtual size_t write(void *buf, loff_t offset, size_t length) = 0;
	virtual loff_t size() = 0;
};

typedef disk_generic::status (disk_factory)(const char *path, bool read_only,
	disk_generic **disk);

extern disk_factory disk_sparsebundle_factory;
extern disk_factory disk_vhd_factory;

#endif
