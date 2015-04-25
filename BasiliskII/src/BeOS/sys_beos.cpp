/*
 *  sys_beos.cpp - System dependent routines, BeOS implementation
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

#include <StorageKit.h>
#include <InterfaceKit.h>
#include <kernel/fs_info.h>
#include <drivers/Drivers.h>
#include <device/scsi.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "sysdeps.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "sys.h"

#define DEBUG 0
#include "debug.h"

#ifdef __HAIKU__
#include <fs_volume.h>
#define unmount(x) fs_unmount_volume(x, 0)
#endif


// File handles are pointers to these structures
struct file_handle {
	file_handle *next;	// Pointer to next file handle (must be first in struct!)
	const char *name;	// File/device name (copied, for mount menu)
	int fd;				// fd of file/device
	bool is_file;		// Flag: plain file or /dev/something?
	bool read_only;		// Copy of Sys_open() flag
	loff_t start_byte;	// Size of file header (if any)
	loff_t file_size;	// Size of file data (only valid if is_file is true)
};

// Linked list of file handles
static file_handle *first_file_handle;

// Temporary buffer for transfers from/to kernel space
const int TMP_BUF_SIZE = 0x10000;
static uint8 *tmp_buf;

// For B_SCSI_PREVENT_ALLOW
static const int32 PREVENT = 1;
static const int32 ALLOW = 0;


/*
 *  Check if device is a mounted HFS volume, get mount name
 */

static bool is_drive_mounted(const char *dev_name, char *mount_name)
{
	int32 i = 0;
	dev_t d;
	fs_info info;
	while ((d = next_dev(&i)) >= 0) {
		fs_stat_dev(d, &info);
		if (strcmp(dev_name, info.device_name) == 0) {
			status_t err = -1;
			BPath mount;
			BDirectory dir;
			BEntry entry;
			node_ref node;
			node.device = info.dev;
			node.node = info.root;
			err = dir.SetTo(&node);
			if (!err)
				err = dir.GetEntry(&entry);
			if (!err)
				err = entry.GetPath(&mount);
			if (!err) {
				strcpy(mount_name, mount.Path());
				return true;
			}
		}
	}
	return false;
}


/*
 *  Initialization
 */

void SysInit(void)
{
	first_file_handle = NULL;

	// Allocate temporary buffer
	tmp_buf = new uint8[TMP_BUF_SIZE];
}


/*
 *  Deinitialization
 */

void SysExit(void)
{
	delete[] tmp_buf;
}


/*
 *  Create menu of used volumes (for "mount" menu)
 */

void SysCreateVolumeMenu(BMenu *menu, uint32 msg)
{
	for (file_handle *fh=first_file_handle; fh; fh=fh->next)
		if (!SysIsFixedDisk(fh))
			menu->AddItem(new BMenuItem(fh->name, new BMessage(msg)));
}


/*
 *  Mount volume given name from mount menu
 */

void SysMountVolume(const char *name)
{
	file_handle *fh;
	for (fh=first_file_handle; fh && strcmp(fh->name, name); fh=fh->next) ;
	if (fh)
		MountVolume(fh);
}


/*
 *  This gets called when no "floppy" prefs items are found
 *  It scans for available floppy drives and adds appropriate prefs items
 */

void SysAddFloppyPrefs(void)
{
	// Only one floppy drive under BeOS
	PrefsAddString("floppy", "/dev/disk/floppy/raw");
}


/*
 *  This gets called when no "disk" prefs items are found
 *  It scans for available HFS volumes and adds appropriate prefs items
 */

void SysAddDiskPrefs(void)
{
	// Let BeOS scan for HFS drives
	D(bug("Looking for Mac volumes...\n"));
	system("mountvolume -allhfs");

	// Add all HFS volumes
	int32 i = 0;
	dev_t d;
	fs_info info;
	while ((d = next_dev(&i)) >= 0) {
		fs_stat_dev(d, &info);
		status_t err = -1;
		BPath mount;
		if (!strcmp(info.fsh_name, "hfs")) {
			BDirectory dir;
			BEntry entry;
			node_ref node;
			node.device = info.dev;
			node.node = info.root;
			err = dir.SetTo(&node);
			if (!err)
				err = dir.GetEntry(&entry);
			if (!err)
				err = entry.GetPath(&mount);
		}
		if (!err)
			err = unmount(mount.Path());
		if (!err) {
			char dev_name[B_FILE_NAME_LENGTH];
			if (info.flags & B_FS_IS_READONLY) {
				dev_name[0] = '*';
				dev_name[1] = 0;
			} else
				dev_name[0] = 0;
			strcat(dev_name, info.device_name);
			PrefsAddString("disk", dev_name);
		}
	}
}


/*
 *  This gets called when no "cdrom" prefs items are found
 *  It scans for available CD-ROM drives and adds appropriate prefs items
 */

// Scan directory for CD-ROM drives, add them to prefs
static void scan_for_cdrom_drives(const char *directory)
{
	// Set directory
	BDirectory dir;
	dir.SetTo(directory);
	if (dir.InitCheck() != B_NO_ERROR)
		return;
	dir.Rewind();

	// Scan each entry
	BEntry entry;
	while (dir.GetNextEntry(&entry) >= 0) {

		// Get path and ref for entry
		BPath path;
		if (entry.GetPath(&path) != B_NO_ERROR)
			continue;
		const char *name = path.Path();
		entry_ref e;
		if (entry.GetRef(&e) != B_NO_ERROR)
			continue;

		// Recursively enter subdirectories (except for floppy)
		if (entry.IsDirectory()) {
			if (!strcmp(e.name, "floppy"))
				continue;
			scan_for_cdrom_drives(name);
		} else {

			D(bug(" checking '%s'\n", name));

			// Ignore partitions
			if (strcmp(e.name, "raw"))
				continue;

			// Open device
			int fd = open(name, O_RDONLY);
			if (fd < 0)
				continue;

			// Get geometry and device type
			device_geometry g;
			if (ioctl(fd, B_GET_GEOMETRY, &g, sizeof(g)) < 0) {
				close(fd);
				continue;
			}

			// Insert to list if it is a CD drive
			if (g.device_type == B_CD)
				PrefsAddString("cdrom", name);
			close(fd);
		}
	}
}

void SysAddCDROMPrefs(void)
{
	// Don't scan for drives if nocdrom option given
	if (PrefsFindBool("nocdrom"))
		return;

	// Look for CD-ROM drives and add prefs items
	D(bug("Looking for CD-ROM drives...\n"));
	scan_for_cdrom_drives("/dev/disk");
}


/*
 *  Add default serial prefs (must be added, even if no ports present)
 */

void SysAddSerialPrefs(void)
{
#ifdef __HAIKU__
	PrefsAddString("seriala", "serial1");
	PrefsAddString("serialb", "serial2");
#else
	system_info info;
	get_system_info(&info);
	switch (info.platform_type) {
		case B_BEBOX_PLATFORM:
		case B_AT_CLONE_PLATFORM:
			PrefsAddString("seriala", "serial1");
			PrefsAddString("serialb", "serial2");
			break;
		case B_MAC_PLATFORM:
			PrefsAddString("seriala", "modem");
			PrefsAddString("serialb", "printer");
			break;
		default:
			PrefsAddString("seriala", "none");
			PrefsAddString("serialb", "none");
			break;
	}
#endif
}


/*
 *  Open file/device, create new file handle (returns NULL on error)
 */

void *Sys_open(const char *name, bool read_only)
{
	static bool published_all = false;
	bool is_file = (strstr(name, "/dev/") != name);

	D(bug("Sys_open(%s, %s)\n", name, read_only ? "read-only" : "read/write"));

	// Print warning message and eventually unmount drive when this is an HFS volume mounted under BeOS (double mounting will corrupt the volume)
	char mount_name[B_FILE_NAME_LENGTH];
	if (!is_file && !read_only && is_drive_mounted(name, mount_name)) {
		char str[256 + B_FILE_NAME_LENGTH];
		sprintf(str, GetString(STR_VOLUME_IS_MOUNTED_WARN), mount_name);
		WarningAlert(str);
		if (unmount(mount_name) != 0) {
			sprintf(str, GetString(STR_CANNOT_UNMOUNT_WARN), mount_name);
			WarningAlert(str);
			return NULL;
		}
	}

	int fd = open(name, read_only ? O_RDONLY : O_RDWR);
	if (fd < 0 && !published_all) {
		// Open failed, create all device nodes and try again, but only the first time
		system("mountvolume -publishall");
		published_all = true;
		fd = open(name, read_only ? O_RDONLY : O_RDWR);
	}
	if (fd >= 0) {
		file_handle *fh = new file_handle;
		fh->name = strdup(name);
		fh->fd = fd;
		fh->is_file = is_file;
		fh->read_only = read_only;
		fh->start_byte = 0;
		if (fh->is_file) {
			// Detect disk image file layout
			loff_t size = lseek(fd, 0, SEEK_END);
			uint8 data[256];
			lseek(fd, 0, SEEK_SET);
			read(fd, data, 256);
			FileDiskLayout(size, data, fh->start_byte, fh->file_size);
		}

		// Enqueue file handle
		fh->next = NULL;
		file_handle *q = first_file_handle;
		if (q) {
			while (q->next)
				q = q->next;
			q->next = fh;
		} else
			first_file_handle = fh;
		return fh;
	} else
		return NULL;
}


/*
 *  Close file/device, delete file handle
 */

void Sys_close(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	// Free device name and close file/device
	free((void *)fh->name);
	close(fh->fd);

	// Dequeue file handle
	file_handle *q = first_file_handle;
	if (q == fh) {
		first_file_handle = NULL;
		delete fh;
		return;
	}
	while (q) {
		if (q->next == fh) {
			q->next = fh->next;
			delete fh;
			return;
		}
		q = q->next;
	}
}


/*
 *  Read "length" bytes from file/device, starting at "offset", to "buffer",
 *  returns number of bytes read (or 0)
 */

static inline ssize_t sread(int fd, void *buf, size_t count)
{
	ssize_t res;
	while ((res = read(fd, buf, count)) == B_INTERRUPTED) ;
	return res;
}

size_t Sys_read(void *arg, void *buffer, loff_t offset, size_t length)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return 0;

//	D(bug("Sys_read(%08lx, %08lx, %Ld, %d)\n", fh, buffer, offset, length));

	// Seek to position
	if (lseek(fh->fd, offset + fh->start_byte, SEEK_SET) < 0)
		return 0;

	// Buffer in kernel space?
	size_t actual = 0;
	if ((uint32)buffer < 0x80000000) {

		// Yes, transfer via buffer
		while (length) {
			size_t transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
			if (sread(fh->fd, tmp_buf, transfer_size) != transfer_size)
				return actual;
			memcpy(buffer, tmp_buf, transfer_size);
			buffer = (void *)((uint8 *)buffer + transfer_size);
			length -= transfer_size;
			actual += transfer_size;
		}

	} else {

		// No, transfer directly
		actual = sread(fh->fd, buffer, length);
		if (actual < 0)
			actual = 0;
	}
	return actual;
}


/*
 *  Write "length" bytes from "buffer" to file/device, starting at "offset",
 *  returns number of bytes written (or 0)
 */

static inline ssize_t swrite(int fd, void *buf, size_t count)
{
	ssize_t res;
	while ((res = write(fd, buf, count)) == B_INTERRUPTED) ;
	return res;
}

size_t Sys_write(void *arg, void *buffer, loff_t offset, size_t length)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return 0;

//	D(bug("Sys_write(%08lx, %08lx, %Ld, %d)\n", fh, buffer, offset, length));

	// Seek to position
	if (lseek(fh->fd, offset + fh->start_byte, SEEK_SET) < 0)
		return 0;

	// Buffer in kernel space?
	size_t actual = 0;
	if ((uint32)buffer < 0x80000000) {

		// Yes, transfer via buffer
		while (length) {
			size_t transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
			memcpy(tmp_buf, buffer, transfer_size);
			if (swrite(fh->fd, tmp_buf, transfer_size) != transfer_size)
				return actual;
			buffer = (void *)((uint8 *)buffer + transfer_size);
			length -= transfer_size;
			actual += transfer_size;
		}

	} else {

		// No, transfer directly
		actual = swrite(fh->fd, buffer, length);
		if (actual < 0)
			actual = 0;
	}
	return actual;
}


/*
 *  Return size of file/device (minus header)
 */

loff_t SysGetFileSize(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

	if (fh->is_file)
		return fh->file_size;
	else {
		device_geometry g;
		if (ioctl(fh->fd, B_GET_GEOMETRY, &g, sizeof(g)) >= 0)
			return (loff_t)g.bytes_per_sector * g.sectors_per_track * g.cylinder_count * g.head_count;
		else
			return 0;
	}
}


/*
 *  Eject volume (if applicable)
 */

void SysEject(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file)
		ioctl(fh->fd, B_EJECT_DEVICE);
}


/*
 *  Format volume (if applicable)
 */

bool SysFormat(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file)
		return ioctl(fh->fd, B_FORMAT_DEVICE) >= 0;
	else
		return false;
}


/*
 *  Check if file/device is read-only (this includes the read-only flag on Sys_open())
 */

bool SysIsReadOnly(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

	if (fh->is_file) {

		// File, return flag given to Sys_open
		return fh->read_only;

	} else {

		// Device, check write protection
		device_geometry g;
		if (ioctl(fh->fd, B_GET_GEOMETRY, &g, sizeof(g)) >= 0)
			return g.read_only | fh->read_only;
		else
			return fh->read_only;	// Removable but not inserted
	}
}


/*
 *  Check if the given file handle refers to a fixed or a removable disk
 */

bool SysIsFixedDisk(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

	if (fh->is_file)
		return true;
	else {
		device_geometry g;
		if (ioctl(fh->fd, B_GET_GEOMETRY, &g, sizeof(g)) >= 0)
			return !g.removable;
		else
			return false;	// Removable but not inserted
	}
}


/*
 *  Check if a disk is inserted in the drive (always true for files)
 */

bool SysIsDiskInserted(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return true;
	else {
		status_t l;
		if (ioctl(fh->fd, B_GET_MEDIA_STATUS, &l, sizeof(l)) >= 0 && l == B_NO_ERROR)
			return true;
		else
			return false;
	}
}


/*
 *  Prevent medium removal (if applicable)
 */

void SysPreventRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file)
		ioctl(fh->fd, B_SCSI_PREVENT_ALLOW, &PREVENT, sizeof(PREVENT));
}


/*
 *  Allow medium removal (if applicable)
 */

void SysAllowRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file)
		ioctl(fh->fd, B_SCSI_PREVENT_ALLOW, &ALLOW, sizeof(ALLOW));
}


/*
 *  Read CD-ROM TOC (binary MSF format, 804 bytes max.)
 */

bool SysCDReadTOC(void *arg, uint8 *toc)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file) {
		memset(tmp_buf, 0, 804);
		if (ioctl(fh->fd, B_SCSI_GET_TOC, tmp_buf, 804) < 0)
			return false;
		memcpy(toc, tmp_buf, 804);
		return true;
	} else
		return false;
}


/*
 *  Read CD-ROM position data (Sub-Q Channel, 16 bytes, see SCSI standard)
 */

bool SysCDGetPosition(void *arg, uint8 *pos)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file) {
		if (ioctl(fh->fd, B_SCSI_GET_POSITION, tmp_buf, 16) < 0)
			return false;
		memcpy(pos, tmp_buf, 16);
		return true;
	} else
		return false;
}


/*
 *  Play CD audio
 */

bool SysCDPlay(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, uint8 end_m, uint8 end_s, uint8 end_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file) {
		scsi_play_position *p = (scsi_play_position *)tmp_buf;
		p->start_m = start_m;
		p->start_s = start_s;
		p->start_f = start_f;
		p->end_m = end_m;
		p->end_s = end_s;
		p->end_f = end_f;
		return ioctl(fh->fd, B_SCSI_PLAY_POSITION, p, sizeof(scsi_play_position)) == 0;
	} else
		return false;
}


/*
 *  Pause CD audio
 */

bool SysCDPause(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

	if (!fh->is_file)
		return ioctl(fh->fd, B_SCSI_PAUSE_AUDIO) == 0;
	else
		return false;
}


/*
 *  Resume paused CD audio
 */

bool SysCDResume(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file)
		return ioctl(fh->fd, B_SCSI_RESUME_AUDIO) == 0;
	else
		return false;
}


/*
 *  Stop CD audio
 */

bool SysCDStop(void *arg, uint8 lead_out_m, uint8 lead_out_s, uint8 lead_out_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file)
		return ioctl(fh->fd, B_SCSI_STOP_AUDIO) == 0;
	else
		return false;
}


/*
 *  Perform CD audio fast-forward/fast-reverse operation starting from specified address
 */

bool SysCDScan(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, bool reverse)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (!fh->is_file) {
		scsi_scan *p = (scsi_scan *)tmp_buf;
		p->speed = 0;
		p->direction = reverse ? -1 : 1;
		return ioctl(fh->fd, B_SCSI_SCAN, p, sizeof(scsi_scan)) == 0;
	} else
		return false;
}


/*
 *  Set CD audio volume (0..255 each channel)
 */

void SysCDSetVolume(void *arg, uint8 left, uint8 right)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file) {
		scsi_volume *p = (scsi_volume *)tmp_buf;
		p->flags = B_SCSI_PORT0_VOLUME | B_SCSI_PORT1_VOLUME;
		p->port0_volume = left;
		p->port1_volume = right;
		ioctl(fh->fd, B_SCSI_SET_VOLUME, p, sizeof(scsi_volume));
	}
}


/*
 *  Get CD audio volume (0..255 each channel)
 */

void SysCDGetVolume(void *arg, uint8 &left, uint8 &right)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	left = right = 0;
	if (!fh->is_file) {
		scsi_volume *p = (scsi_volume *)tmp_buf;
		p->flags = B_SCSI_PORT0_VOLUME | B_SCSI_PORT1_VOLUME;
		if (ioctl(fh->fd, B_SCSI_GET_VOLUME, p, sizeof(scsi_volume)) == 0) {
			left = p->port0_volume;
			right = p->port1_volume;
		}
	}
}
