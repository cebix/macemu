/*
 * vhd_unix.h -- support for disk images in vhd format 
 *
 *  (C) 2010 Geoffrey Brown
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

#ifndef VHD_H
#define VHD_H

void *vhd_unix_open(const char *name, int *size, bool read_only);
int vhd_unix_read(void *arg, void *buffer, loff_t offset, size_t length);
int vhd_unix_write(void *arg, void *buffer, loff_t offset, size_t length);
void vhd_unix_close(void *arg);

#endif
