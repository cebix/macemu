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
#include "vhd_unix.h"
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

void *vhd_unix_open(const char *name, int *size, bool read_only)
{
	int amode = read_only ? R_OK : (R_OK | W_OK);
	int fid;
	vhd_context_t *vhd;

	D(bug("vhd open %s\n", name));

	if (access(name, amode)) {
		D(bug("vhd open -- incorrect permissions %s\n", name));
		return NULL;
	}
  
	if (! (fid = open(name, O_RDONLY))) { 
		D(bug("vhd open -- couldn't open file %s\n", name));
		return NULL;
	} 
	else {
		char buf[9];
		read(fid, buf, sizeof(buf)-1);
		buf[8] = 0;
		close(fid);
		if (strcmp("conectix", buf) != 0) {
			D(bug("vhd open -- not vhd magic = %s\n", buf));
			return NULL;
		}
		if (vhd = (vhd_context_t *) malloc(sizeof(vhd_context_t))) {
			int err;
			if (err = vhd_open(vhd, name, read_only ? 
								VHD_OPEN_RDONLY : VHD_OPEN_RDWR)) {
				D(bug("vhd_open failed (%d)\n", err));
				free(vhd);
				return NULL;
			} 
			else {
				*size = (int) vhd->footer.curr_size;
				printf("VHD Open %s\n", name);
				return (void *) vhd;
			}
		}
		else {
			D(bug("vhd open -- malloc failed\n"));
			return NULL;
		}
	}
}

int vhd_unix_read(void *arg, void *buffer, loff_t offset, size_t length)
{
	vhd_context_t *ctx = (vhd_context_t *) arg;
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

int vhd_unix_write(void *arg, void *buffer, loff_t offset, size_t length)
{
	int err;
	vhd_context_t *ctx = (vhd_context_t *) arg;

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

void vhd_unix_close(void *arg)
{
	D(bug("vhd close\n"));
	vhd_close((vhd_context_t *) arg);
	free(arg);
}
