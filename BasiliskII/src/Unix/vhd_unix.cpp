/*
 * vhd_unix.cpp -- support for disk images in vhd format 
 *
 *	(C) 2010 Geoffrey Brown
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"
#include "disk_unix.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
extern "C" {
#include <libvhd.h>
}
// libvhd.h defines DEBUG
#undef DEBUG
#define DEBUG 0
#include "debug.h"

static disk_generic::status vhd_unix_open(const char *name, int *size,
	bool read_only, vhd_context_t **ctx)
{
	int amode = read_only ? R_OK : (R_OK | W_OK);
	int fid;
	vhd_context_t *vhd;

	D(bug("vhd open %s\n", name));

	if (access(name, amode)) {
		D(bug("vhd open -- incorrect permissions %s\n", name));
		return disk_generic::DISK_UNKNOWN;
	}
  
	if (! (fid = open(name, O_RDONLY))) { 
		D(bug("vhd open -- couldn't open file %s\n", name));
		return disk_generic::DISK_UNKNOWN;
	} 
	else {
		char buf[9];
		read(fid, buf, sizeof(buf)-1);
		buf[8] = 0;
		close(fid);
		if (strcmp("conectix", buf) != 0) {
			D(bug("vhd open -- not vhd magic = %s\n", buf));
			return disk_generic::DISK_UNKNOWN;
		}
		if (vhd = (vhd_context_t *) malloc(sizeof(vhd_context_t))) {
			int err;
			if (err = vhd_open(vhd, name, read_only ? 
								VHD_OPEN_RDONLY : VHD_OPEN_RDWR)) {
				D(bug("vhd_open failed (%d)\n", err));
				free(vhd);
				return disk_generic::DISK_INVALID;
			} 
			else {
				*size = (int) vhd->footer.curr_size;
				printf("VHD Open %s\n", name);
				*ctx = vhd;
				return disk_generic::DISK_VALID;
			}
		}
		else {
			D(bug("vhd open -- malloc failed\n"));
			return disk_generic::DISK_INVALID;
		}
	}
}

static int vhd_unix_read(vhd_context_t *ctx, void *buffer, loff_t offset,
	size_t length)
{
	int err;
	if ((offset % VHD_SECTOR_SIZE) || (length % VHD_SECTOR_SIZE)) {
		printf("vhd read only supported on sector boundaries (%d)\n",
				VHD_SECTOR_SIZE);
		return 0;
	}	
	if (err = vhd_io_read(ctx, (char *) buffer, offset / VHD_SECTOR_SIZE, 
							length / VHD_SECTOR_SIZE)){
		D(bug("vhd read error %d\n", err));	
		return err;
	}
	else 
		return length;
}

static int vhd_unix_write(vhd_context_t *ctx, void *buffer, loff_t offset,
	size_t length)
{
	int err;

	if ((offset % VHD_SECTOR_SIZE) || (length % VHD_SECTOR_SIZE)) {
		printf("vhd write only supported on sector boundaries (%d)\n",
				VHD_SECTOR_SIZE);
		return 0;
	}	
	if (err = vhd_io_write(ctx, (char *) buffer, offset/VHD_SECTOR_SIZE,
							length/VHD_SECTOR_SIZE)) {
		D(bug("vhd write error %d\n", err));
		return err;
	}
	else
		return length;
}


static void vhd_unix_close(vhd_context_t *ctx)
{
	D(bug("vhd close\n"));
	vhd_close(ctx);
	free(ctx);
}


struct disk_vhd : disk_generic {
	disk_vhd(vhd_context_t *ctx, bool read_only, loff_t size)
	: ctx(ctx), read_only(read_only), file_size(size) { }
	
	virtual ~disk_vhd() { vhd_unix_close(ctx); }
	virtual bool is_read_only() { return read_only; }
	virtual loff_t size() { return file_size; }
	
	virtual size_t read(void *buf, loff_t offset, size_t length) {
		return vhd_unix_read(ctx, buf, offset, length);
	}
	
	virtual size_t write(void *buf, loff_t offset, size_t length) {
		return vhd_unix_write(ctx, buf, offset, length);
	}

protected:
	vhd_context_t *ctx;
	bool read_only;
	loff_t file_size;
};

disk_generic::status disk_vhd_factory(const char *path,
		bool read_only, disk_generic **disk) {
	int size;
	vhd_context_t *ctx = NULL;
	disk_generic::status st = vhd_unix_open(path, &size, read_only, &ctx);
	if (st == disk_generic::DISK_VALID)
		*disk = new disk_vhd(ctx, read_only, size);
	return st;
}
