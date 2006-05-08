/*
 *  sys_unix.cpp - System dependent routines, Unix implementation
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_AVAILABILITYMACROS_H
#include <AvailabilityMacros.h>
#endif

#ifdef __linux__
#include <sys/mount.h>
#include <linux/cdrom.h>
#include <linux/fd.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <dirent.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/cdio.h>
#endif

#if defined __APPLE__ && defined __MACH__
#include <sys/disk.h>
#if (defined AQUA || defined HAVE_FRAMEWORK_COREFOUNDATION)
#ifndef __MACOSX__
#define __MACOSX__ MAC_OS_X_VERSION_MIN_REQUIRED
#endif
#endif
#endif

#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "sys.h"

#define DEBUG 0
#include "debug.h"


// File handles are pointers to these structures
struct file_handle {
	char *name;			// Copy of device/file name
	int fd;
	bool is_file;		// Flag: plain file or /dev/something?
	bool is_floppy;		// Flag: floppy device
	bool is_cdrom;		// Flag: CD-ROM device
	bool read_only;		// Copy of Sys_open() flag
	loff_t start_byte;	// Size of file header (if any)
	loff_t file_size;	// Size of file data (only valid if is_file is true)
	bool is_media_present;		// Flag: media is inserted and available

#if defined(__linux__)
	int cdrom_cap;		// CD-ROM capability flags (only valid if is_cdrom is true)
#elif defined(__FreeBSD__)
	struct ioc_capability cdrom_cap;
#elif defined(__APPLE__) && defined(__MACH__)
	char	*ioctl_name;	// For CDs on OS X - a device for special ioctls
	int		ioctl_fd;
#endif
};

// Open file handles
struct open_file_handle {
	file_handle *fh;
	open_file_handle *next;
};
static open_file_handle *open_file_handles = NULL;

// File handle of first floppy drive (for SysMountFirstFloppy())
static file_handle *first_floppy = NULL;

// Prototypes
static void cdrom_close(file_handle *fh);
static bool cdrom_open(file_handle *fh, const char *path = NULL);


/*
 *  Initialization
 */

void SysInit(void)
{
#if defined __MACOSX__
	extern void DarwinSysInit(void);
	DarwinSysInit();
#endif
}


/*
 *  Deinitialization
 */

void SysExit(void)
{
#if defined __MACOSX__
	extern void DarwinSysExit(void);
	DarwinSysExit();
#endif
}


/*
 *  Manage open file handles
 */

static void sys_add_file_handle(file_handle *fh)
{
	open_file_handle *p = new open_file_handle;
	p->fh = fh;
	p->next = open_file_handles;
	open_file_handles = p;
}

static void sys_remove_file_handle(file_handle *fh)
{
	open_file_handle *p = open_file_handles;
	open_file_handle *q = NULL;

	while (p) {
		if (p->fh == fh) {
			if (q)
				q->next = p->next;
			else
				open_file_handles = p->next;
			delete p;
			break;
		}
		q = p;
		p = p->next;
	}
}


/*
 *  Account for media that has just arrived
 */

void SysMediaArrived(const char *path, int type)
{
	// Replace the "cdrom" entry (we are polling, it's unique)
	if (type == MEDIA_CD && !PrefsFindBool("nocdrom"))
		PrefsReplaceString("cdrom", path);

	// Wait for media to be available for reading
	if (open_file_handles) {
		const int MAX_WAIT = 5;
		for (int i = 0; i < MAX_WAIT; i++) {
			if (access(path, R_OK) == 0)
				break;
			switch (errno) {
			case ENOENT: // Unlikely
			case EACCES: // MacOS X is mounting the media
				sleep(1);
				continue;
			}
			printf("WARNING: Cannot access %s (%s)\n", path, strerror(errno));
			return;
		}
	}

	for (open_file_handle *p = open_file_handles; p != NULL; p = p->next) {
		file_handle * const fh = p->fh;

		// Re-open CD-ROM device
		if (fh->is_cdrom && type == MEDIA_CD) {
			cdrom_close(fh);
			if (cdrom_open(fh, path)) {
				fh->is_media_present = true;
				MountVolume(fh);
			}
		}
	}
}


/*
 *  Account for media that has just been removed
 */

void SysMediaRemoved(const char *path, int type)
{
	if ((type & MEDIA_REMOVABLE) != MEDIA_CD)
		return;

	for (open_file_handle *p = open_file_handles; p != NULL; p = p->next) {
		file_handle * const fh = p->fh;

		// Mark media as not available
		if (!fh->is_cdrom || !fh->is_media_present)
			continue;
		if (fh->name && strcmp(fh->name, path) == 0) {
			fh->is_media_present = false;
			break;
		}
#if defined __MACOSX__
		if (fh->ioctl_name && strcmp(fh->ioctl_name, path) == 0) {
			fh->is_media_present = false;
			break;
		}
#endif
	}
}


/*
 *  Mount first floppy disk
 */

void SysMountFirstFloppy(void)
{
	if (first_floppy)
		MountVolume(first_floppy);
}


/*
 *  This gets called when no "floppy" prefs items are found
 *  It scans for available floppy drives and adds appropriate prefs items
 */

void SysAddFloppyPrefs(void)
{
#if defined(__linux__)
	DIR *fd_dir = opendir("/dev/floppy");
	if (fd_dir) {
		struct dirent *floppy_dev;
		while ((floppy_dev = readdir(fd_dir)) != NULL) {
			if (strstr(floppy_dev->d_name, "u1440") != NULL) {
				char fd_dev[20];
				sprintf(fd_dev, "/dev/floppy/%s", floppy_dev->d_name);
				PrefsAddString("floppy", fd_dev);
			}
		}
		closedir(fd_dir);
	} else {
		PrefsAddString("floppy", "/dev/fd0");
		PrefsAddString("floppy", "/dev/fd1");
	}
#elif defined(__NetBSD__)
	PrefsAddString("floppy", "/dev/fd0a");
	PrefsAddString("floppy", "/dev/fd1a");
#elif defined(__APPLE__) && defined(__MACH__)
  #if defined(AQUA) || defined(HAVE_FRAMEWORK_COREFOUNDATION)
	extern	void DarwinAddFloppyPrefs(void);

	DarwinAddFloppyPrefs();
  #else
	// Until I can convince the other guys that my Darwin code is useful,
	// we just add something safe (a non-existant device):
	PrefsAddString("floppy", "/dev/null");
  #endif
#else
	PrefsAddString("floppy", "/dev/fd0");
	PrefsAddString("floppy", "/dev/fd1");
#endif
}


/*
 *  This gets called when no "disk" prefs items are found
 *  It scans for available HFS volumes and adds appropriate prefs items
 *	On OS X, we could do the same, but on an OS X machine I think it is
 *	very unlikely that any mounted volumes would contain a system which
 *	is old enough to boot a 68k Mac, so we just do nothing here for now.
 */

void SysAddDiskPrefs(void)
{
#ifdef __linux__
	FILE *f = fopen("/etc/fstab", "r");
	if (f) {
		char line[256];
		while(fgets(line, 255, f)) {
			// Read line
			int len = strlen(line);
			if (len == 0 || line[0] == '#')
				continue;
			line[len-1] = 0;

			// Parse line
			char *dev, *mnt_point, *fstype;
			if (sscanf(line, "%as %as %as", &dev, &mnt_point, &fstype) == 3) {
				if (strcmp(fstype, "hfs") == 0)
					PrefsAddString("disk", dev);
			}
			free(dev); free(mnt_point); free(fstype);
		}
		fclose(f);
	}
#endif
}


/*
 *  This gets called when no "cdrom" prefs items are found
 *  It scans for available CD-ROM drives and adds appropriate prefs items
 */

void SysAddCDROMPrefs(void)
{
	// Don't scan for drives if nocdrom option given
	if (PrefsFindBool("nocdrom"))
		return;

#if defined(__linux__)
	if (access("/dev/.devfsd", F_OK) < 0)
		PrefsAddString("cdrom", "/dev/cdrom");
	else {
		DIR *cd_dir = opendir("/dev/cdroms");
		if (cd_dir) {
			struct dirent *cdrom_dev;
			while ((cdrom_dev = readdir(cd_dir)) != NULL) {
				if (strcmp(cdrom_dev->d_name, ".") != 0 && strcmp(cdrom_dev->d_name, "..") != 0) {
					char cd_dev[20];
					sprintf(cd_dev, "/dev/cdroms/%s", cdrom_dev->d_name);
					PrefsAddString("cdrom", cd_dev);
				}
			}
			closedir(cd_dir);
		}
	}
#elif defined __MACOSX__
	// There is no predefined path for CD-ROMs on MacOS X. Rather, we
	// define a single fake CD-ROM entry for the emulated MacOS.
	// XXX this means we handle only CD-ROM drive at a time, wherever
	// the disk is, the latest one is used.
	PrefsAddString("cdrom", "/dev/poll/cdrom");
#elif defined(__FreeBSD__) || defined(__NetBSD__)
	PrefsAddString("cdrom", "/dev/cd0c");
#endif
}


/*
 *  Add default serial prefs (must be added, even if no ports present)
 */

void SysAddSerialPrefs(void)
{
#if defined(__linux__)
	if (access("/dev/.devfsd", F_OK) < 0) {
		PrefsAddString("seriala", "/dev/ttyS0");
		PrefsAddString("serialb", "/dev/ttyS1");
	} else {
		PrefsAddString("seriala", "/dev/tts/0");
		PrefsAddString("serialb", "/dev/tts/1");
	}
#elif defined(__FreeBSD__)
	PrefsAddString("seriala", "/dev/cuaa0");
	PrefsAddString("serialb", "/dev/cuaa1");
#elif defined(__NetBSD__)
	PrefsAddString("seriala", "/dev/tty00");
	PrefsAddString("serialb", "/dev/tty01");
#elif defined(__APPLE__) && defined(__MACH__)
  #if defined(AQUA) || defined(HAVE_FRAMEWORK_COREFOUNDATION)
	extern	void DarwinAddSerialPrefs(void);

	DarwinAddSerialPrefs();
  #else
	// Until I can convince the other guys that my Darwin code is useful,
	// we just add something safe (non-existant devices):
	PrefsAddString("seriala", "/dev/null");
	PrefsAddString("serialb", "/dev/null");
  #endif
#endif
}


/*
 *  Open CD-ROM device and initialize internal data
 */

static bool cdrom_open_1(file_handle *fh)
{
#if defined __MACOSX__
	// In OS X, the device name is OK for sending ioctls to,
	// but not for reading raw CDROM data from.
	// (it seems to have extra data padded in)
	//
	// So, we keep the already opened file handle,
	// and open a slightly different file for CDROM data 
	//
	fh->ioctl_fd = fh->fd;
	fh->ioctl_name = fh->name;
	fh->fd = -1;
	fh->name = (char *)malloc(strlen(fh->ioctl_name) + 3);
	if (fh->name) {
		strcpy(fh->name, fh->ioctl_name);
		strcat(fh->name, "s1");
		fh->fd = open(fh->name, O_RDONLY, O_NONBLOCK);
	}
	if (fh->ioctl_fd < 0)
		return false;
#endif
	return true;
}

bool cdrom_open(file_handle *fh, const char *path)
{
	if (path)
		fh->name = strdup(path);
	fh->fd = open(fh->name, O_RDONLY, O_NONBLOCK);
	fh->start_byte = 0;
	if (!cdrom_open_1(fh))
		return false;
	return fh->fd >= 0;
}


/*
 *  Close a CD-ROM device
 */

void cdrom_close(file_handle *fh)
{
	if (fh->fd >= 0) {
		close(fh->fd);
		fh->fd = -1;
	}
	if (fh->name) {
		free(fh->name);
		fh->name = NULL;
	}
#if defined __MACOSX__
	if (fh->ioctl_fd >= 0) {
		close(fh->ioctl_fd);
		fh->ioctl_fd = -1;
	}
	if (fh->ioctl_name) {
		free(fh->ioctl_name);
		fh->ioctl_name = NULL;
	}
#endif
}


/*
 *  Check if device is a mounted HFS volume, get mount name
 */

static bool is_drive_mounted(const char *dev_name, char *mount_name)
{
#ifdef __linux__
	FILE *f = fopen("/proc/mounts", "r");
	if (f) {
		char line[256];
		while(fgets(line, 255, f)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len-1] = 0;

			// Parse line
			if (strncmp(line, dev_name, strlen(dev_name)) == 0) {
				mount_name[0] = 0;
				char *dummy;
				sscanf(line, "%as %s", &dummy, mount_name);
				free(dummy);
				fclose(f);
				return true;
			}
		}
		fclose(f);
	}
#endif
	return false;
}


/*
 *  Open file/device, create new file handle (returns NULL on error)
 */

void *Sys_open(const char *name, bool read_only)
{
	bool is_file = strncmp(name, "/dev/", 5) != 0;
#if defined(__FreeBSD__)
	                // SCSI                             IDE
	bool is_cdrom = strncmp(name, "/dev/cd", 7) == 0 || strncmp(name, "/dev/acd", 8) == 0;
#else
	bool is_cdrom = strncmp(name, "/dev/cd", 7) == 0;
#endif
	bool is_floppy = strncmp(name, "/dev/fd", 7) == 0;

	bool is_polled_media = strncmp(name, "/dev/poll/", 10) == 0;
	if (is_floppy) // Floppy open fails if there's no disk inserted
		is_polled_media = true;

#if defined __MACOSX__
	// There is no set filename in /dev which is the cdrom,
	// so we have to see if it is any of the devices that we found earlier
	{
		int index = 0;
		const char *str;
		while ((str = PrefsFindString("cdrom", index++)) != NULL) {
			if (is_polled_media || strcmp(str, name) == 0) {
				is_cdrom = true;
				read_only = true;
				break;
			}
		}
	}
#endif

	D(bug("Sys_open(%s, %s)\n", name, read_only ? "read-only" : "read/write"));

	// Check if write access is allowed, set read-only flag if not
	if (!read_only && access(name, W_OK))
		read_only = true;

	// Print warning message and eventually unmount drive when this is an HFS volume mounted under Linux (double mounting will corrupt the volume)
	char mount_name[256];
	if (!is_file && !read_only && is_drive_mounted(name, mount_name)) {
		char str[512];
		sprintf(str, GetString(STR_VOLUME_IS_MOUNTED_WARN), mount_name);
		WarningAlert(str);
		sprintf(str, "umount %s", mount_name);
		if (system(str)) {
			sprintf(str, GetString(STR_CANNOT_UNMOUNT_WARN), mount_name, strerror(errno));
			WarningAlert(str);
			return NULL;
		}
	}

	// Open file/device
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__MACOSX__)
	int fd = open(name, (read_only ? O_RDONLY : O_RDWR) | (is_cdrom ? O_NONBLOCK : 0));
#else
	int fd = open(name, read_only ? O_RDONLY : O_RDWR);
#endif
	if (fd < 0 && !read_only) {
		// Read-write failed, try read-only
		read_only = true;
		fd = open(name, O_RDONLY);
	}
	if (fd >= 0 || is_polled_media) {
		file_handle *fh = new file_handle;
		fh->name = strdup(name);
		fh->fd = fd;
		fh->is_file = is_file;
		fh->read_only = read_only;
		fh->start_byte = 0;
		fh->is_floppy = is_floppy;
		fh->is_cdrom = is_cdrom;
		fh->is_media_present = false;
#if defined __MACOSX__
		fh->ioctl_fd = -1;
		fh->ioctl_name = NULL;
#endif
		if (fh->is_file) {
			fh->is_media_present = true;
			// Detect disk image file layout
			loff_t size = 0;
			size = lseek(fd, 0, SEEK_END);
			uint8 data[256];
			lseek(fd, 0, SEEK_SET);
			read(fd, data, 256);
			FileDiskLayout(size, data, fh->start_byte, fh->file_size);
		} else {
			struct stat st;
			if (fstat(fd, &st) == 0) {
				fh->is_media_present = true;
				if (S_ISBLK(st.st_mode)) {
					fh->is_cdrom = is_cdrom;
#if defined(__linux__)
					fh->is_floppy = (MAJOR(st.st_rdev) == FLOPPY_MAJOR);
#ifdef CDROM_GET_CAPABILITY
					if (is_cdrom) {
						fh->cdrom_cap = ioctl(fh->fd, CDROM_GET_CAPABILITY);
						if (fh->cdrom_cap < 0)
							fh->cdrom_cap = 0;
					}
#else
					fh->cdrom_cap = 0;
#endif
#elif defined(__FreeBSD__)
					fh->is_floppy = ((st.st_rdev >> 16) == 2);
#ifdef CDIOCCAPABILITY
					if (is_cdrom) {
						if (ioctl(fh->fd, CDIOCCAPABILITY, &fh->cdrom_cap) < 0)
							memset(&fh->cdrom_cap, 0, sizeof(fh->cdrom_cap));
					}
#else
					fh->cdrom_cap = 0;
#endif
#elif defined(__NetBSD__)
					fh->is_floppy = ((st.st_rdev >> 16) == 2);
#endif
				}
#if defined __MACOSX__
				if (is_cdrom) {
					fh->is_cdrom = true;
					fh->is_floppy = false;
					if (cdrom_open_1(fh))
						fh->is_media_present = true;
				}
#endif
			}
		}
		if (fh->is_floppy && first_floppy == NULL)
			first_floppy = fh;
		sys_add_file_handle(fh);
		return fh;
	} else {
		printf("WARNING: Cannot open %s (%s)\n", name, strerror(errno));
		return NULL;
	}
}


/*
 *  Close file/device, delete file handle
 */

void Sys_close(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	sys_remove_file_handle(fh);

	if (fh->is_cdrom)
		cdrom_close(fh);
	if (fh->fd >= 0)
		close(fh->fd);
	if (fh->name)
		free(fh->name);
	delete fh;
}


/*
 *  Read "length" bytes from file/device, starting at "offset", to "buffer",
 *  returns number of bytes read (or 0)
 */

size_t Sys_read(void *arg, void *buffer, loff_t offset, size_t length)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return 0;

	// Seek to position
	if (lseek(fh->fd, offset + fh->start_byte, SEEK_SET) < 0)
		return 0;

	// Read data
	return read(fh->fd, buffer, length);
}


/*
 *  Write "length" bytes from "buffer" to file/device, starting at "offset",
 *  returns number of bytes written (or 0)
 */

size_t Sys_write(void *arg, void *buffer, loff_t offset, size_t length)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return 0;

	// Seek to position
	if (lseek(fh->fd, offset + fh->start_byte, SEEK_SET) < 0)
		return 0;

	// Write data
	return write(fh->fd, buffer, length);
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
#if defined(__linux__)
		long blocks;
		if (ioctl(fh->fd, BLKGETSIZE, &blocks) < 0)
			return 0;
		D(bug(" BLKGETSIZE returns %d blocks\n", blocks));
		return (loff_t)blocks * 512;
#elif defined __MACOSX__
		uint32 block_size;
		if (ioctl(fh->ioctl_fd, DKIOCGETBLOCKSIZE, &block_size) < 0)
			return 0;
		D(bug(" DKIOCGETBLOCKSIZE returns %lu bytes\n", (unsigned long)block_size));
		uint64 block_count;
		if (ioctl(fh->ioctl_fd, DKIOCGETBLOCKCOUNT, &block_count) < 0)
			return 0;
		D(bug(" DKIOCGETBLOCKCOUNT returns %llu blocks\n", (unsigned long long)block_count));
		return block_count * block_size;
#else
		return lseek(fh->fd, 0, SEEK_END) - fh->start_byte;
#endif
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

#if defined(__linux__)
	if (fh->is_floppy) {
		if (fh->fd >= 0) {
			fsync(fh->fd);
			ioctl(fh->fd, FDFLUSH);
			ioctl(fh->fd, FDEJECT);
			close(fh->fd);	// Close and reopen so the driver will see the media change
		}
		fh->fd = open(fh->name, fh->read_only ? O_RDONLY : O_RDWR);
	} else if (fh->is_cdrom) {
		ioctl(fh->fd, CDROMEJECT);
		close(fh->fd);	// Close and reopen so the driver will see the media change
		fh->fd = open(fh->name, O_RDONLY | O_NONBLOCK);
	}
#elif defined(__FreeBSD__) || defined(__NetBSD__)
	if (fh->is_floppy) {
		fsync(fh->fd);
	} else if (fh->is_cdrom) {
		ioctl(fh->fd, CDIOCEJECT);
		close(fh->fd);	// Close and reopen so the driver will see the media change
		fh->fd = open(fh->name, O_RDONLY | O_NONBLOCK);
	}
#elif defined(__APPLE__) && defined(__MACH__)
	if (fh->is_cdrom && fh->is_media_present) {
		close(fh->fd);
		fh->fd = -1;
		if (ioctl(fh->ioctl_fd, DKIOCEJECT) < 0) {
			D(bug(" DKIOCEJECT failed on file %s: %s\n",
				   fh->ioctl_name, strerror(errno)));

			// If we are running MacOS X, the device may be in busy
			// state because the Finder has mounted the disk
			close(fh->ioctl_fd);
			fh->ioctl_fd = -1;

			// Try to use "diskutil eject" but it can take up to 5
			// seconds to complete
			static const char eject_cmd[] = "/usr/sbin/diskutil eject %s 2>&1 >/dev/null";
			char *cmd = (char *)alloca(strlen(eject_cmd) + strlen(fh->ioctl_name) + 1);
			sprintf(cmd, eject_cmd, fh->ioctl_name);
			system(cmd);
		}
		fh->is_media_present = false;
	}
#endif
}


/*
 *  Format volume (if applicable)
 */

bool SysFormat(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	//!!
	return true;
}


/*
 *  Check if file/device is read-only (this includes the read-only flag on Sys_open())
 */

bool SysIsReadOnly(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

#if defined(__linux__)
	if (fh->is_floppy) {
		if (fh->fd >= 0) {
			struct floppy_drive_struct stat;
			ioctl(fh->fd, FDGETDRVSTAT, &stat);
			return !(stat.flags & FD_DISK_WRITABLE);
		} else
			return true;
	} else
#endif
		return fh->read_only;
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
	else if (fh->is_floppy || fh->is_cdrom)
		return false;
	else
		return true;
}


/*
 *  Check if a disk is inserted in the drive (always true for files)
 */

bool SysIsDiskInserted(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file) {
		return true;

#if defined(__linux__)
	} else if (fh->is_floppy) {
		char block[512];
		lseek(fh->fd, 0, SEEK_SET);
		ssize_t actual = read(fh->fd, block, 512);
		if (actual < 0) {
			close(fh->fd);	// Close and reopen so the driver will see the media change
			fh->fd = open(fh->name, fh->read_only ? O_RDONLY : O_RDWR);
			actual = read(fh->fd, block, 512);
		}
		return actual == 512;
	} else if (fh->is_cdrom) {
#ifdef CDROM_MEDIA_CHANGED
		if (fh->cdrom_cap & CDC_MEDIA_CHANGED) {
			// If we don't do this, all attempts to read from a disc fail
			// once the tray has been opened (altough the TOC reads fine).
			// Can somebody explain this to me?
			if (ioctl(fh->fd, CDROM_MEDIA_CHANGED) == 1) {
				close(fh->fd);
				fh->fd = open(fh->name, O_RDONLY | O_NONBLOCK);
			}
		}
#endif
#ifdef CDROM_DRIVE_STATUS
		if (fh->cdrom_cap & CDC_DRIVE_STATUS) {
			return ioctl(fh->fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK;
		}
#endif
		cdrom_tochdr header;
		return ioctl(fh->fd, CDROMREADTOCHDR, &header) == 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
	} else if (fh->is_floppy) {
		return false;	//!!
	} else if (fh->is_cdrom) {
		struct ioc_toc_header header;
		return ioctl(fh->fd, CDIOREADTOCHEADER, &header) == 0;
#elif defined __MACOSX__
	} else if (fh->is_cdrom || fh->is_floppy) {
		return fh->is_media_present;
#endif

	} else
		return true;
}


/*
 *  Prevent medium removal (if applicable)
 */

void SysPreventRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

#if defined(__linux__) && defined(CDROM_LOCKDOOR)
	if (fh->is_cdrom)
		ioctl(fh->fd, CDROM_LOCKDOOR, 1);	
#endif
}


/*
 *  Allow medium removal (if applicable)
 */

void SysAllowRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

#if defined(__linux__) && defined(CDROM_LOCKDOOR)
	if (fh->is_cdrom)
		ioctl(fh->fd, CDROM_LOCKDOOR, 0);	
#endif
}


/*
 *  Read CD-ROM TOC (binary MSF format, 804 bytes max.)
 */

bool SysCDReadTOC(void *arg, uint8 *toc)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_cdrom) {
#if defined(__linux__)
		uint8 *p = toc + 2;

		// Header
		cdrom_tochdr header;
		if (ioctl(fh->fd, CDROMREADTOCHDR, &header) < 0)
			return false;
		*p++ = header.cdth_trk0;
		*p++ = header.cdth_trk1;

		// Tracks
		cdrom_tocentry entry;
		for (int i=header.cdth_trk0; i<=header.cdth_trk1; i++) {
			entry.cdte_track = i;
			entry.cdte_format = CDROM_MSF;
			if (ioctl(fh->fd, CDROMREADTOCENTRY, &entry) < 0)
				return false;
			*p++ = 0;
			*p++ = (entry.cdte_adr << 4) | entry.cdte_ctrl;
			*p++ = entry.cdte_track;
			*p++ = 0;
			*p++ = 0;
			*p++ = entry.cdte_addr.msf.minute;
			*p++ = entry.cdte_addr.msf.second;
			*p++ = entry.cdte_addr.msf.frame;
		}

		// Leadout track
		entry.cdte_track = CDROM_LEADOUT;
		entry.cdte_format = CDROM_MSF;
		if (ioctl(fh->fd, CDROMREADTOCENTRY, &entry) < 0)
			return false;
		*p++ = 0;
		*p++ = (entry.cdte_adr << 4) | entry.cdte_ctrl;
		*p++ = entry.cdte_track;
		*p++ = 0;
		*p++ = 0;
		*p++ = entry.cdte_addr.msf.minute;
		*p++ = entry.cdte_addr.msf.second;
		*p++ = entry.cdte_addr.msf.frame;

		// TOC size
		int toc_size = p - toc;
		*toc++ = toc_size >> 8;
		*toc++ = toc_size & 0xff;
		return true;
#elif defined __MACOSX__ && defined MAC_OS_X_VERSION_10_2
		if (fh->is_media_present) {
			extern bool DarwinCDReadTOC(char *name, uint8 *toc);
			return DarwinCDReadTOC(fh->name, toc);
		}
		return false;
#elif defined(__FreeBSD__)
		uint8 *p = toc + 2;

		// Header
		struct ioc_toc_header header;
		if (ioctl(fh->fd, CDIOREADTOCHEADER, &header) < 0)
			return false;
		*p++ = header.starting_track;
		*p++ = header.ending_track;

		// Tracks
		struct ioc_read_toc_single_entry entry;
		for (int i=header.starting_track; i<=header.ending_track; i++) {
			entry.track = i;
			entry.address_format = CD_MSF_FORMAT;
			if (ioctl(fh->fd, CDIOREADTOCENTRY, &entry) < 0)
				return false;
			*p++ = 0;
			*p++ = (entry.entry.addr_type << 4) | entry.entry.control;
			*p++ = entry.entry.track;
			*p++ = 0;
			*p++ = 0;
			*p++ = entry.entry.addr.msf.minute;
			*p++ = entry.entry.addr.msf.second;
			*p++ = entry.entry.addr.msf.frame;
		}

		// Leadout track
		entry.track = CD_TRACK_INFO;
		entry.address_format = CD_MSF_FORMAT;
		if (ioctl(fh->fd, CDIOREADTOCENTRY, &entry) < 0)
			return false;
		*p++ = 0;
		*p++ = (entry.entry.addr_type << 4) | entry.entry.control;
		*p++ = entry.entry.track;
		*p++ = 0;
		*p++ = 0;
		*p++ = entry.entry.addr.msf.minute;
		*p++ = entry.entry.addr.msf.second;
		*p++ = entry.entry.addr.msf.frame;

		// TOC size
		int toc_size = p - toc;
		*toc++ = toc_size >> 8;
		*toc++ = toc_size & 0xff;
		return true;
#elif defined(__NetBSD__)
		uint8 *p = toc + 2;

		// Header
		struct ioc_toc_header header;
		if (ioctl(fh->fd, CDIOREADTOCHEADER, &header) < 0)
			return false;
		*p++ = header.starting_track;
		*p++ = header.ending_track;

		// Tracks (this is nice... :-)
		struct ioc_read_toc_entry entries;
		entries.address_format = CD_MSF_FORMAT;
		entries.starting_track = 1;
		entries.data_len = 800;
		entries.data = (cd_toc_entry *)p;
		if (ioctl(fh->fd, CDIOREADTOCENTRIES, &entries) < 0)
			return false;

		// TOC size
		int toc_size = p - toc;
		*toc++ = toc_size >> 8;
		*toc++ = toc_size & 0xff;
		return true;
#else
		return false;
#endif
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

	if (fh->is_cdrom) {
#if defined(__linux__)
		cdrom_subchnl chan;
		chan.cdsc_format = CDROM_MSF;
		if (ioctl(fh->fd, CDROMSUBCHNL, &chan) < 0)
			return false;
		*pos++ = 0;
		*pos++ = chan.cdsc_audiostatus;
		*pos++ = 0;
		*pos++ = 12;	// Sub-Q data length
		*pos++ = 0;
		*pos++ = (chan.cdsc_adr << 4) | chan.cdsc_ctrl;
		*pos++ = chan.cdsc_trk;
		*pos++ = chan.cdsc_ind;
		*pos++ = 0;
		*pos++ = chan.cdsc_absaddr.msf.minute;
		*pos++ = chan.cdsc_absaddr.msf.second;
		*pos++ = chan.cdsc_absaddr.msf.frame;
		*pos++ = 0;
		*pos++ = chan.cdsc_reladdr.msf.minute;
		*pos++ = chan.cdsc_reladdr.msf.second;
		*pos++ = chan.cdsc_reladdr.msf.frame;
		return true;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		struct ioc_read_subchannel chan;
		chan.data_format = CD_MSF_FORMAT;
		chan.address_format = CD_MSF_FORMAT;
		chan.track = CD_CURRENT_POSITION;
		if (ioctl(fh->fd, CDIOCREADSUBCHANNEL, &chan) < 0)
			return false;
		*pos++ = 0;
		*pos++ = chan.data->header.audio_status;
		*pos++ = 0;
		*pos++ = 12;	// Sub-Q data length
		*pos++ = 0;
		*pos++ = (chan.data->what.position.addr_type << 4) | chan.data->what.position.control;
		*pos++ = chan.data->what.position.track_number;
		*pos++ = chan.data->what.position.index_number;
		*pos++ = 0;
		*pos++ = chan.data->what.position.absaddr.msf.minute;
		*pos++ = chan.data->what.position.absaddr.msf.second;
		*pos++ = chan.data->what.position.absaddr.msf.frame;
		*pos++ = 0;
		*pos++ = chan.data->what.position.reladdr.msf.minute;
		*pos++ = chan.data->what.position.reladdr.msf.second;
		*pos++ = chan.data->what.position.reladdr.msf.frame;
		return true;
#else
		return false;
#endif
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

	if (fh->is_cdrom) {
#if defined(__linux__)
		cdrom_msf play;
		play.cdmsf_min0 = start_m;
		play.cdmsf_sec0 = start_s;
		play.cdmsf_frame0 = start_f;
		play.cdmsf_min1 = end_m;
		play.cdmsf_sec1 = end_s;
		play.cdmsf_frame1 = end_f;
		return ioctl(fh->fd, CDROMPLAYMSF, &play) == 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		struct ioc_play_msf play;
		play.start_m = start_m;
		play.start_s = start_s;
		play.start_f = start_f;
		play.end_m = end_m;
		play.end_s = end_s;
		play.end_f = end_f;
		return ioctl(fh->fd, CDIOCPLAYMSF, &play) == 0;
#else
		return false;
#endif
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
		return false;

	if (fh->is_cdrom) {
#if defined(__linux__)
		return ioctl(fh->fd, CDROMPAUSE) == 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		return ioctl(fh->fd, CDIOCPAUSE) == 0;
#else
		return false;
#endif
	} else
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

	if (fh->is_cdrom) {
#if defined(__linux__)
		return ioctl(fh->fd, CDROMRESUME) == 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		return ioctl(fh->fd, CDIOCRESUME) == 0;
#else
		return false;
#endif
	} else
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

	if (fh->is_cdrom) {
#if defined(__linux__)
		return ioctl(fh->fd, CDROMSTOP) == 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		return ioctl(fh->fd, CDIOCSTOP) == 0;
#else
		return false;
#endif
	} else
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

	// Not supported under Linux
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

	if (fh->is_cdrom) {
#if defined(__linux__)
		cdrom_volctrl vol;
		vol.channel0 = vol.channel2 = left;
		vol.channel1 = vol.channel3 = right;
		ioctl(fh->fd, CDROMVOLCTRL, &vol);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		struct ioc_vol vol;
		vol.vol[0] = vol.vol[2] = left;
		vol.vol[1] = vol.vol[3] = right;
		ioctl(fh->fd, CDIOCSETVOL, &vol);
#endif
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
	if (fh->is_cdrom) {
#if defined(__linux__)
		cdrom_volctrl vol;
		ioctl(fh->fd, CDROMVOLREAD, &vol);
		left = vol.channel0;
		right = vol.channel1;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
		struct ioc_vol vol;
		ioctl(fh->fd, CDIOCGETVOL, &vol);
		left = vol.vol[0];
		right = vol.vol[1];
#endif
	}
}
