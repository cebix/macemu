/*
 *  sys_windows.cpp - System dependent routines, Windows implementation
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

#include <string>
using std::string;

#include <algorithm>
using std::min;

#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "sys.h"

#include "cd_defs.h"
#include "cdenable/ntcd.h"
#include "cdenable/cache.h"
#include "cdenable/eject_nt.h"

#define DEBUG 0
#include "debug.h"


// File handles are pointers to these structures
struct file_handle {
	char *name;			// Copy of device/file name
	HANDLE fh;
	bool is_file;		// Flag: plain file or physical device?
	bool is_floppy;		// Flag: floppy device
	bool is_cdrom;		// Flag: CD-ROM device
	bool read_only;		// Copy of Sys_open() flag
	loff_t start_byte;	// Size of file header (if any)
	loff_t file_size;	// Size of file data (only valid if is_file is true)
	cachetype cache;
	bool is_media_present;
};

// Open file handles
struct open_file_handle {
	file_handle *fh;
	open_file_handle *next;
};
static open_file_handle *open_file_handles = NULL;

// File handle of first floppy drive (for SysMountFirstFloppy())
static file_handle *first_floppy = NULL;

// CD-ROM variables
static const int CD_READ_AHEAD_SECTORS = 16;
static char *sector_buffer = NULL;

// Prototypes
static bool is_cdrom_readable(file_handle *fh);


/*
 *  Initialization
 */

void SysInit(void)
{
	// Initialize CD-ROM driver
	sector_buffer = (char *)VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	CdenableSysInstallStart();
}


/*
 *  Deinitialization
 */

void SysExit(void)
{
	if (sector_buffer) {
		VirtualFree(sector_buffer, 0, MEM_RELEASE );
		sector_buffer = NULL;
	}
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
 *  Mount removable media now
 */

void mount_removable_media(int media)
{
	for (open_file_handle *p = open_file_handles; p != NULL; p = p->next) {
		file_handle * const fh = p->fh;

		if (fh->is_cdrom && (media & MEDIA_CD)) {
			cache_clear(&fh->cache);
			fh->start_byte = 0;

			if (fh->fh && fh->fh != INVALID_HANDLE_VALUE)
				CloseHandle(fh->fh);

			// Re-open device
			char device_name[MAX_PATH];
			sprintf(device_name, "\\\\.\\%c:", fh->name[0]);
			fh->fh = CreateFile(
				device_name,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if (fh->fh != INVALID_HANDLE_VALUE) {
				fh->is_media_present = is_cdrom_readable(fh);
				if (fh->is_media_present)
					MountVolume(fh);
			} else {
				fh->is_media_present = false;
			}
		}
	}
}


/*
 *  Account for media that has just arrived
 */

void SysMediaArrived(void)
{
	mount_removable_media(MEDIA_REMOVABLE);
}


/*
 *  Account for media that has just been removed
 */

void SysMediaRemoved(void)
{
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
}


/*
 *  This gets called when no "disk" prefs items are found
 *  It scans for available HFS volumes and adds appropriate prefs items
 */

void SysAddDiskPrefs(void)
{
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

	for (char letter = 'C'; letter <= 'Z'; letter++) {
		int i = (int)(letter - 'A');
		string rootdir = letter + ":\\";
		if (GetDriveType(rootdir.c_str()) == DRIVE_CDROM)
			PrefsAddString("cdrom", rootdir.c_str());
	}
}


/*
 *  Add default serial prefs (must be added, even if no ports present)
 */

void SysAddSerialPrefs(void)
{
	PrefsAddString("seriala", "COM1");
	PrefsAddString("serialb", "COM2");
}


/*
 *  Read CD-ROM
 *  Must give cd some time to settle
 *  Can't give too much however, would be annoying, this is difficult..
 */

static inline int cd_read_with_retry(file_handle *fh, ULONG LBA, int count, char *buf )
{
	if (!fh || !fh->fh)
		return 0;

	return CdenableSysReadCdBytes(fh->fh, LBA, count, buf);
}

static int cd_read(file_handle *fh, cachetype *cptr, ULONG LBA, int count, char *buf)
{
	ULONG l1, l2, cc;
	int i, c_count, got_bytes = 0, nblocks, s_inx, ss, first_block;
	int ok_bytes = 0;
	char *ptr, *ttptr = 0, *tmpbuf;

	if (count <= 0)
		return 0;

	if (!fh || !fh->fh)
		return 0;

	ss = 2048;
	l1 = (LBA / ss) * ss;
	l2 = ((LBA + count - 1 + ss) / ss) * ss;
	cc = l2 - l1;
	nblocks = cc / ss;
	first_block = LBA / ss;

	ptr = buf;
	s_inx = LBA - l1;
	c_count = ss - s_inx;
	if (c_count > count)
		c_count = count;

	for (i = 0; i < nblocks; i++) {
		if (!cache_get(cptr, first_block + i, sector_buffer))
			break;

		memcpy(ptr, sector_buffer + s_inx, c_count);
		ok_bytes += c_count;
		ptr += c_count;
		s_inx = 0;
		c_count = ss;
		if (c_count > count - ok_bytes)
			c_count = count - ok_bytes;
	}

	if (i != nblocks && count != ok_bytes) {
		int bytes_left = count - ok_bytes;
		int blocks_left = nblocks - i;
		int alignedleft;

		// NEW read ahead code:
		int ahead = CD_READ_AHEAD_SECTORS;
		if (blocks_left < ahead) {
			nblocks += (ahead - blocks_left);
			blocks_left = ahead;
		}

		alignedleft = blocks_left*ss;

		tmpbuf = (char *)VirtualAlloc(
			NULL, alignedleft,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (tmpbuf) {
			got_bytes = cd_read_with_retry(fh, (first_block + i) * ss, alignedleft, tmpbuf);
			if (got_bytes != alignedleft) {
				// should never happen
				// Yes it does ...
				if (got_bytes < 0)
					got_bytes = 0;
				if (c_count > got_bytes)
					c_count = got_bytes;
				if (c_count > 0) {
					ttptr = tmpbuf;
					memcpy(ptr, ttptr + s_inx, c_count);
					ok_bytes += c_count;
				}
				VirtualFree(tmpbuf, 0, MEM_RELEASE );
				return ok_bytes;
			}
			ttptr = tmpbuf;
			for ( ; i < nblocks; i++) {
				if (c_count > 0) {
					memcpy(ptr, ttptr + s_inx, c_count);
					ok_bytes += c_count;
					ptr += c_count;
				}
				s_inx = 0;
				c_count = ss;
				if (c_count > count - ok_bytes)
					c_count = count - ok_bytes;
				cache_put(cptr, first_block + i, ttptr, ss);
				ttptr += ss;
			}
			VirtualFree(tmpbuf, 0, MEM_RELEASE );
		}
	}

	return ok_bytes;
}


/*
 *  Check if file handle FH represents a readable CD-ROM
 */

static bool is_cdrom_readable(file_handle *fh)
{
	if (!fh || !fh->fh)
		return false;

	cache_clear(&fh->cache);

	DWORD dummy;
	bool result = (0 != DeviceIoControl(
		fh->fh,
		IOCTL_STORAGE_CHECK_VERIFY,
		NULL, 0,
		NULL, 0,
		&dummy,
		NULL));
	if (!result) {
		const size_t n_bytes = 2048;
		char *buffer = (char *)VirtualAlloc(NULL, n_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (buffer) {
			result = (cd_read_with_retry(fh, 0, n_bytes, buffer) == n_bytes);
			VirtualFree(buffer, 0, MEM_RELEASE);
		}
	}

	return result;
}


/*
 *  Check if NAME represents a read-only file
 */

static bool is_read_only_path(const char *name)
{
	DWORD attrib = GetFileAttributes((char *)name);
	return (attrib != INVALID_FILE_ATTRIBUTES && ((attrib & FILE_ATTRIBUTE_READONLY) != 0));
}


/*
 *  Open file/device, create new file handle (returns NULL on error)
 */

void *Sys_open(const char *path_name, bool read_only)
{
	file_handle * fh = NULL;

	// Parse path name and options
	char name[MAX_PATH];
	strcpy(name, path_name);

	// Normalize floppy / cd path
	int name_len = strlen(name);
	if (name_len == 1 && isalpha(name[0]))
		strcat(name, ":\\");
	if (name_len > 0 && name[name_len - 1] == ':')
		strcat(name, "\\");
	name_len = strlen(name);

	D(bug("Sys_open(%s, %s)\n", name, read_only ? "read-only" : "read/write"));
	if (name_len > 0 && name[name_len - 1] == '\\') {
		int type = GetDriveType(name);

		if (type == DRIVE_CDROM) {
			read_only = true;
			char device_name[MAX_PATH];
			sprintf(device_name, "\\\\.\\%c:", name[0]);

			// Open device
			HANDLE h = CreateFile(
				device_name,
				GENERIC_READ,
				0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if (h != INVALID_HANDLE_VALUE) {
				fh = new file_handle;
				fh->name = strdup(name);
				fh->fh = h;
				fh->is_file = false;
				fh->read_only = read_only;
				fh->start_byte = 0;
				fh->is_floppy = false;
				fh->is_cdrom = true;
				memset(&fh->cache, 0, sizeof(cachetype));
				cache_init(&fh->cache);
				cache_clear(&fh->cache);
				if (!PrefsFindBool("nocdrom"))
					fh->is_media_present = is_cdrom_readable(fh);
			}
		}
	}

	else { // Hard file

		// Check if write access is allowed, set read-only flag if not
		if (!read_only && is_read_only_path(name))
			read_only = true;

		// Open file
		HANDLE h = CreateFile(
			name,
			read_only ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (h == INVALID_HANDLE_VALUE && !read_only) {
			// Read-write failed, try read-only
			read_only = true;
			h = CreateFile(
				name,
				GENERIC_READ,
				0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}

		if (h != INVALID_HANDLE_VALUE) {
			fh = new file_handle;
			fh->name = strdup(name);
			fh->fh = h;
			fh->is_file = true;
			fh->read_only = read_only;
			fh->start_byte = 0;
			fh->is_floppy = false;
			fh->is_cdrom = false;

			// Detect disk image file layout
			loff_t size = GetFileSize(h, NULL);
			DWORD bytes_read;
			uint8 data[256];
			ReadFile(h, data, sizeof(data), &bytes_read, NULL);
			FileDiskLayout(size, data, fh->start_byte, fh->file_size);
		}
	}

	if (fh->is_floppy && first_floppy == NULL)
		first_floppy = fh;

	if (fh)
		sys_add_file_handle(fh);

	return fh;
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

	if (fh->is_cdrom) {
		cache_final(&fh->cache);
		SysAllowRemoval((void *)fh);
	}
	if (fh->fh != NULL) {
		CloseHandle(fh->fh);
		fh->fh = NULL;
	}
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

	DWORD bytes_read = 0;

	if (fh->is_file) {
		// Seek to position
		LONG lo = (LONG)offset;
		LONG hi = (LONG)(offset >> 32);
		DWORD r = SetFilePointer(fh->fh, lo, &hi, FILE_BEGIN);
		if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
			return 0;

		// Read data
		if (ReadFile(fh->fh, buffer, length, &bytes_read, NULL) == 0)
			bytes_read = 0;
	}
	else if (fh->is_cdrom) {
		int bytes_left, try_bytes, got_bytes;
		char *b = (char *)buffer;
		bytes_left = length;
		while (bytes_left) {
			try_bytes = min(bytes_left, 32768);
			if (fh->is_cdrom) {
				got_bytes = cd_read(fh, &fh->cache, (DWORD)offset, try_bytes, b);
				if (got_bytes != try_bytes && !PrefsFindBool("nocdrom"))
					fh->is_media_present = is_cdrom_readable(fh);
			}
			b += got_bytes;
			offset += got_bytes;
			bytes_read += got_bytes;
			bytes_left -= got_bytes;
			if (got_bytes != try_bytes)
				bytes_left = 0;
		}
	}
	// TODO: other media

	return bytes_read;
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

	DWORD bytes_written = 0;

	if (fh->is_file) {
		// Seek to position
		LONG lo = (LONG)offset;
		LONG hi = (LONG)(offset >> 32);
		DWORD r = SetFilePointer(fh->fh, lo, &hi, FILE_BEGIN);
		if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
			return 0;

		// Write data
		if (WriteFile(fh->fh, buffer, length, &bytes_written, NULL) == 0)
			bytes_written = 0;
	}
	// TODO: other media

	return bytes_written;
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
	else if (fh->is_cdrom)
		return 0x28A00000; // FIXME: get real CD-ROM size
	else {
		// TODO: other media
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

	if (fh->is_cdrom && fh->fh) {
		fh->is_media_present = false;
		// Commented out because there was some problems, but can't remember
		// exactly ... need to find out
		// EjectVolume(toupper(*fh->name),false);

		// Preventing is cumulative, try to make sure it's indeed released now
		for (int i = 0; i < 10; i++)
			PreventRemovalOfVolume(fh->fh, false);

		if (!PrefsFindBool("nocdrom")) {
			DWORD dummy;
			DeviceIoControl(
				fh->fh,
				IOCTL_STORAGE_EJECT_MEDIA,
				NULL, 0,
				NULL, 0,
				&dummy,
				NULL
				);
		}
		cache_clear(&fh->cache);
		fh->start_byte = 0;
	}
	// TODO: handle floppies
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

	if (fh->is_file)
		return true;
	else if (fh->is_cdrom && !PrefsFindBool("nocdrom")) {
		if (PrefsFindBool("pollmedia"))
			fh->is_media_present = is_cdrom_readable(fh);
		return fh->is_media_present;
	}
	else {
		// TODO: other media
	}

	return false;
}


/*
 *  Prevent medium removal (if applicable)
 */

void SysPreventRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (fh->is_cdrom && fh->fh)
		PreventRemovalOfVolume(fh->fh, true);
}


/*
 *  Allow medium removal (if applicable)
 */

void SysAllowRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (fh->is_cdrom && fh->fh)
		PreventRemovalOfVolume(fh->fh, false);
}


/*
 *  Read CD-ROM TOC (binary MSF format, 804 bytes max.)
 */

bool SysCDReadTOC(void *arg, uint8 *toc)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	DWORD dummy;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_READ_TOC,
						   NULL, 0,
						   toc, min((int)sizeof(CDROM_TOC), 804),
						   &dummy,
						   NULL);
}


/*
 *  Read CD-ROM position data (Sub-Q Channel, 16 bytes, see SCSI standard)
 */

bool SysCDGetPosition(void *arg, uint8 *pos)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	SUB_Q_CHANNEL_DATA q_data;

	CDROM_SUB_Q_DATA_FORMAT q_format;
	q_format.Format = IOCTL_CDROM_CURRENT_POSITION;
	q_format.Track = 0; // used only by ISRC reads

	DWORD dwBytesReturned = 0;
	bool ok = DeviceIoControl(fh->fh,
							  IOCTL_CDROM_READ_Q_CHANNEL,
							  &q_format, sizeof(CDROM_SUB_Q_DATA_FORMAT),
							  &q_data, sizeof(SUB_Q_CHANNEL_DATA),
							  &dwBytesReturned,
							  NULL);
	if (ok)
		memcpy(pos, &q_data.CurrentPosition, sizeof(SUB_Q_CURRENT_POSITION));

	return ok;
}


/*
 *  Play CD audio
 */

bool SysCDPlay(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, uint8 end_m, uint8 end_s, uint8 end_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	CDROM_PLAY_AUDIO_MSF msf;
	msf.StartingM = start_m;
	msf.StartingS = start_s;
	msf.StartingF = start_f;
	msf.EndingM = end_m;
	msf.EndingS = end_s;
	msf.EndingF = end_f;

	DWORD dwBytesReturned = 0;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_PLAY_AUDIO_MSF,
						   &msf, sizeof(CDROM_PLAY_AUDIO_MSF),
						   NULL, 0,
						   &dwBytesReturned,
						   NULL);
}


/*
 *  Pause CD audio
 */

bool SysCDPause(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	DWORD dwBytesReturned = 0;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_PAUSE_AUDIO,
						   NULL, 0,
						   NULL, 0,
						   &dwBytesReturned,
						   NULL);
}


/*
 *  Resume paused CD audio
 */

bool SysCDResume(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	DWORD dwBytesReturned = 0;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_RESUME_AUDIO,
						   NULL, 0,
						   NULL, 0,
						   &dwBytesReturned, NULL);
}


/*
 *  Stop CD audio
 */

bool SysCDStop(void *arg, uint8 lead_out_m, uint8 lead_out_s, uint8 lead_out_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	DWORD dwBytesReturned = 0;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_STOP_AUDIO,
						   NULL, 0,
						   NULL, 0,
						   &dwBytesReturned,
						   NULL);
}


/*
 *  Perform CD audio fast-forward/fast-reverse operation starting from specified address
 */

bool SysCDScan(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, bool reverse)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return false;

	CDROM_SEEK_AUDIO_MSF msf;
	msf.M = start_m;
	msf.S = start_s;
	msf.F = start_f;

	DWORD dwBytesReturned = 0;
	return DeviceIoControl(fh->fh,
						   IOCTL_CDROM_SEEK_AUDIO_MSF,
						   &msf, sizeof(CDROM_SEEK_AUDIO_MSF),
						   NULL, 0,
						   &dwBytesReturned,
						   NULL);
}


/*
 *  Set CD audio volume (0..255 each channel)
 */

void SysCDSetVolume(void *arg, uint8 left, uint8 right)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh || !fh->fh || !fh->is_cdrom)
		return;

	VOLUME_CONTROL vc;
	vc.PortVolume[0] = left;
	vc.PortVolume[1] = right;
	vc.PortVolume[2] = left;
	vc.PortVolume[3] = right;

	DWORD dwBytesReturned = 0;
	DeviceIoControl(fh->fh,
					IOCTL_CDROM_SET_VOLUME,
					&vc, sizeof(VOLUME_CONTROL),
					NULL, 0,
					&dwBytesReturned,
					NULL);
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
	if (!fh->fh || !fh->is_cdrom)
		return;

	VOLUME_CONTROL vc;
	memset(&vc, 0, sizeof(vc));

	DWORD dwBytesReturned = 0;
	if (DeviceIoControl(fh->fh,
						IOCTL_CDROM_GET_VOLUME,
						NULL, 0,
						&vc, sizeof(VOLUME_CONTROL),
						&dwBytesReturned,
						NULL))
	{
		left = vc.PortVolume[0];
		right = vc.PortVolume[1];
	}
}
