/*
 *  sys_amiga.cpp - System dependent routines, Amiga implementation
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

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/newstyle.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <resources/disk.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/disk.h>

#include "sysdeps.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "sys.h"

#define DEBUG 0
#include "debug.h"


// File handles are pointers to these structures
struct file_handle {
	bool is_file;			// Flag: plain file or /dev/something?
	bool read_only;			// Copy of Sys_open() flag
	loff_t start_byte;		// Size of file header (if any)
	loff_t size;			// Size of file/device (minus header)

	BPTR f;					// AmigaDOS file handle (if is_file == true)

	struct IOStdReq *io;	// Pointer to IORequest (if is_file == false)
	ULONG block_size;		// Block size of device (must be a power of two)
	bool is_nsd;			// New style device?
	bool does_64bit;		// Supports 64 bit trackdisk commands?
};


// FileInfoBlock (must be global because it has to be on a longword boundary)
static struct FileInfoBlock FIB;

// Message port for device communication
static struct MsgPort *the_port = NULL;

// Temporary buffer in chip memory
const int TMP_BUF_SIZE = 0x10000;
static UBYTE *tmp_buf = NULL;


/*
 *  Initialization
 */

void SysInit(void)
{
	// Create port and temporary buffer
	the_port = CreateMsgPort();
	tmp_buf = (UBYTE *)AllocMem(TMP_BUF_SIZE, MEMF_CHIP | MEMF_PUBLIC);
	if (the_port == NULL || tmp_buf == NULL) {
		ErrorAlert(GetString(STR_NO_MEM_ERR));
		QuitEmulator();
	}
}


/*
 *  Deinitialization
 */

void SysExit(void)
{
	// Delete port and temporary buffer
	if (the_port)
		DeleteMsgPort(the_port);
	if (tmp_buf)
		FreeMem(tmp_buf, TMP_BUF_SIZE);
}


/*
 *  This gets called when no "floppy" prefs items are found
 *  It scans for available floppy drives and adds appropriate prefs items
 */

void SysAddFloppyPrefs(void)
{
#if 0
	for (int i=0; i<4; i++) {
		ULONG id = GetUnitID(i);
		if (id == DRT_150RPM) {	// We need an HD drive
			char str[256];
			sprintf(str, "/dev/mfm.device/%d/0/0/1474560/512", i);
			PrefsAddString("floppy", str);
		}
	}
#endif
}


/*
 *  This gets called when no "disk" prefs items are found
 *  It scans for available HFS volumes and adds appropriate prefs items
 */

void SysAddDiskPrefs(void)
{
	// AmigaOS doesn't support MacOS partitioning, so this probably doesn't make much sense...
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

	//!!
}


/*
 *  Add default serial prefs (must be added, even if no ports present)
 */

void SysAddSerialPrefs(void)
{
	PrefsAddString("seriala", "serial.device/0");
	PrefsAddString("serialb", "*parallel.device/0");
}


/*
 *  Open file/device, create new file handle (returns NULL on error)
 *
 *  Format for device names: /dev/<name>/<unit>/<open flags>/<start block>/<size (blocks)>/<block size>
 */

void *Sys_open(const char *name, bool read_only)
{
	bool is_file = (strstr(name, "/dev/") != name);

	D(bug("Sys_open(%s, %s)\n", name, read_only ? "read-only" : "read/write"));

	// File or device?
	if (is_file) {

		// File, open it and get stats
		BPTR f = Open((char *)name, MODE_OLDFILE);
		if (!f)
			return NULL;
		if (!ExamineFH(f, &FIB)) {
			Close(f);
			return NULL;
		}

		// Check if file is write protected
		if (FIB.fib_Protection & FIBF_WRITE)
			read_only = true;

		// Create file_handle
		file_handle *fh = new file_handle;
		fh->f = f;
		fh->is_file = true;
		fh->read_only = read_only;

		// Detect disk image file layout
		loff_t size = FIB.fib_Size;
		Seek(fh->f, 0, OFFSET_BEGINNING);
		Read(fh->f, tmp_buf, 256);
		FileDiskLayout(size, tmp_buf, fh->start_byte, fh->size);
		return fh;

	} else {

		// Device, parse string
		char dev_name[256];
		ULONG dev_unit = 0, dev_flags = 0, dev_start = 0, dev_size = 16, dev_bsize = 512;
		if (sscanf(name, "/dev/%[^/]/%ld/%ld/%ld/%ld/%ld", dev_name, &dev_unit, &dev_flags, &dev_start, &dev_size, &dev_bsize) < 2)
			return NULL;

		// Create IORequest
		struct IOStdReq *io = (struct IOStdReq *)CreateIORequest(the_port, sizeof(struct IOExtTD));
		if (io == NULL)
			return NULL;

		// Open device
		if (OpenDevice((UBYTE *)dev_name, dev_unit, (struct IORequest *)io, dev_flags)) {
			D(bug(" couldn't open device\n"));
			DeleteIORequest(io);
			return NULL;
		}

		// Check for new style device
		bool is_nsd = false, does_64bit = false;
		struct NSDeviceQueryResult nsdqr;
		nsdqr.DevQueryFormat = 0;
		nsdqr.SizeAvailable = 0;
		io->io_Command = NSCMD_DEVICEQUERY;
		io->io_Length = sizeof(nsdqr);
		io->io_Data = (APTR)&nsdqr;
		LONG error = DoIO((struct IORequest *)io);
		D(bug("DEVICEQUERY returned %ld (length %ld, actual %ld)\n", error, io->io_Length, io->io_Actual));
		if ((!error) && (io->io_Actual >= 16) && (io->io_Actual <= sizeof(nsdqr)) && (nsdqr.SizeAvailable == io->io_Actual)) {

			// Looks like an NSD
			is_nsd = true;
			D(bug(" new style device, type %ld\n", nsdqr.DeviceType));

			// We only work with trackdisk-like devices
			if (nsdqr.DeviceType != NSDEVTYPE_TRACKDISK) {
				CloseDevice((struct IORequest *)io);
				DeleteIORequest(io);
				return NULL;
			}

			// Check whether device is 64 bit capable
			UWORD *cmdcheck;
			for (cmdcheck = nsdqr.SupportedCommands; *cmdcheck; cmdcheck++) {
				if (*cmdcheck == NSCMD_TD_READ64) {
					D(bug(" supports 64 bit commands\n"));
					does_64bit = true;
				}
			}
		}

		// Create file_handle
		file_handle *fh = new file_handle;
		fh->io = io;
		fh->is_file = false;
		fh->read_only = read_only;
		fh->start_byte = (loff_t)dev_start * dev_bsize;
		fh->size = (loff_t)dev_size * dev_bsize;
		fh->block_size = dev_bsize;
		fh->is_nsd = is_nsd;
		fh->does_64bit = does_64bit;
		return fh;
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

	D(bug("Sys_close(%08lx)\n", arg));

	// File or device?
	if (fh->is_file) {

		// File, simply close it
		Close(fh->f);

	} else {

		// Device, close it and delete IORequest
		fh->io->io_Command = CMD_UPDATE;
		DoIO((struct IORequest *)fh->io);

		fh->io->io_Command = TD_MOTOR;
		fh->io->io_Length = 0;
		DoIO((struct IORequest *)fh->io);

		CloseDevice((struct IORequest *)fh->io);
		DeleteIORequest(fh->io);
	}
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

	// File or device?
	if (fh->is_file) {

		// File, seek to position
		if (Seek(fh->f, offset + fh->start_byte, OFFSET_BEGINNING) == -1)
			return 0;

		// Read data
		LONG actual = Read(fh->f, buffer, length);
		if (actual == -1)
			return 0;
		else
			return actual;

	} else {

		// Device, pre-read (partial read of first block) neccessary?
		loff_t pos = offset + fh->start_byte;
		size_t actual = 0;
		uint32 pre_offset = pos % fh->block_size;
		if (pre_offset) {

			// Yes, read one block
			fh->io->io_Command = CMD_READ;
			fh->io->io_Length = fh->block_size;
			fh->io->io_Offset = pos - pre_offset;
			fh->io->io_Data = tmp_buf;
			if (DoIO((struct IORequest *)fh->io) || fh->io->io_Actual != fh->block_size)
				return 0;

			// Copy data to destination buffer
			size_t pre_length = fh->block_size - pre_offset;
			if (pre_length > length)
				pre_length = length;
			memcpy(buffer, tmp_buf + pre_offset, pre_length);

			// Adjust data pointers
			buffer = (uint8 *)buffer + pre_length;
			pos += pre_length;
			length -= pre_length;
			actual += pre_length;
		}

		// Main read (complete reads of middle blocks) possible?
		if (length >= fh->block_size) {

			// Yes, read blocks
			size_t main_length = length & ~(fh->block_size - 1);
			fh->io->io_Command = CMD_READ;
			fh->io->io_Length = main_length;
			fh->io->io_Offset = pos;
			fh->io->io_Data = buffer;
			if (DoIO((struct IORequest *)fh->io) || fh->io->io_Actual != main_length)
				return 0;

			// Adjust data pointers
			buffer = (uint8 *)buffer + main_length;
			pos += main_length;
			length -= main_length;
			actual += main_length;
		}

		// Post-read (partial read of last block) neccessary?
		if (length) {

			// Yes, read one block
			fh->io->io_Command = CMD_READ;
			fh->io->io_Length = fh->block_size;
			fh->io->io_Offset = pos;
			fh->io->io_Data = tmp_buf;
			if (DoIO((struct IORequest *)fh->io) || fh->io->io_Actual != fh->block_size)
				return 0;

			// Copy data to destination buffer
			memcpy(buffer, tmp_buf, length);
			actual += length;
		}

		return actual;
	}
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

	// File or device?
	if (fh->is_file) {

		// File, seek to position if neccessary
		if (Seek(fh->f, offset + fh->start_byte, OFFSET_BEGINNING) == -1)
			return 0;

		// Write data
		LONG actual = Write(fh->f, buffer, length);
		if (actual == -1)
			return 0;
		else
			return actual;

	} else {

		// Device, write data
		fh->io->io_Command = CMD_WRITE;
		fh->io->io_Length = length;
		fh->io->io_Offset = offset + fh->start_byte;
		fh->io->io_Data = buffer;
		if (DoIO((struct IORequest *)fh->io))
			return 0;
		else
			return fh->io->io_Actual;
	}
}


/*
 *  Return size of file/device (minus header)
 */

loff_t SysGetFileSize(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return true;

	return fh->size;
}


/*
 *  Eject volume (if applicable)
 */

void SysEject(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file) {

		// Flush buffer, turn off the drive motor and eject volume
		fh->io->io_Command = CMD_UPDATE;
		DoIO((struct IORequest *)fh->io);

		fh->io->io_Command = TD_MOTOR;
		fh->io->io_Length = 0;
		DoIO((struct IORequest *)fh->io);

		fh->io->io_Command = TD_EJECT;
		fh->io->io_Length = 1;
		DoIO((struct IORequest *)fh->io);
	}
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

	if (fh->is_file) {

		// File, return flag given to Sys_open
		return fh->read_only;

	} else {

		// Device, check write protection
		fh->io->io_Flags = IOF_QUICK;
		fh->io->io_Command = TD_PROTSTATUS;
		BeginIO((struct IORequest *)fh->io);
		if (fh->io->io_Actual)
			return true;
		else
			return fh->read_only;
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
	else {

		// Check medium status
		fh->io->io_Flags = IOF_QUICK;
		fh->io->io_Command = TD_CHANGESTATE;
		fh->io->io_Actual = 0;
		BeginIO((struct IORequest *)fh->io);
		return fh->io->io_Actual == 0;
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

	if (!fh->is_file) {

		// Send PREVENT ALLOW MEDIUM REMOVAL SCSI command
		struct SCSICmd scsi;
		static const UBYTE the_cmd[6] = {0x1e, 0, 0, 0, 1, 0};
		scsi.scsi_Length = 0;
		scsi.scsi_Command = (UBYTE *)the_cmd;
		scsi.scsi_CmdLength = 6;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		DoIO((struct IORequest *)fh->io);
	}
}


/*
 *  Allow medium removal (if applicable)
 */

void SysAllowRemoval(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return;

	if (!fh->is_file) {

		// Send PREVENT ALLOW MEDIUM REMOVAL SCSI command
		struct SCSICmd scsi;
		static const UBYTE the_cmd[6] = {0x1e, 0, 0, 0, 0, 0};
		scsi.scsi_Length = 0;
		scsi.scsi_Command = (UBYTE *)the_cmd;
		scsi.scsi_CmdLength = 6;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		DoIO((struct IORequest *)fh->io);
	}
}


/*
 *  Read CD-ROM TOC (binary MSF format, 804 bytes max.)
 */

bool SysCDReadTOC(void *arg, uint8 *toc)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		// Send READ TOC MSF SCSI command
		struct SCSICmd scsi;
		static const UBYTE read_toc_cmd[10] = {0x43, 0x02, 0, 0, 0, 0, 0, 0x03, 0x24, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 804;
		scsi.scsi_Command = (UBYTE *)read_toc_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		memcpy(toc, tmp_buf, 804);
		return true;
	}
}


/*
 *  Read CD-ROM position data (Sub-Q Channel, 16 bytes, see SCSI standard)
 */

bool SysCDGetPosition(void *arg, uint8 *pos)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		// Send READ SUB-CHANNEL SCSI command
		struct SCSICmd scsi;
		static const UBYTE read_subq_cmd[10] = {0x42, 0x02, 0x40, 0x01, 0, 0, 0, 0, 0x10, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 16;
		scsi.scsi_Command = (UBYTE *)read_subq_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		memcpy(pos, tmp_buf, 16);
		return true;
	}
}


/*
 *  Play CD audio
 */

bool SysCDPlay(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, uint8 end_m, uint8 end_s, uint8 end_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		// Send PLAY AUDIO MSF SCSI command
		struct SCSICmd scsi;
		UBYTE play_cmd[10] = {0x47, 0, 0, start_m, start_s, start_f, end_m, end_s, end_f, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 0;
		scsi.scsi_Command = play_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		return true;
	}
}


/*
 *  Pause CD audio
 */

bool SysCDPause(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		// Send PAUSE RESUME SCSI command
		struct SCSICmd scsi;
		static const UBYTE pause_cmd[10] = {0x4b, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 0;
		scsi.scsi_Command = (UBYTE *)pause_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		return true;
	}
}


/*
 *  Resume paused CD audio
 */

bool SysCDResume(void *arg)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		// Send PAUSE RESUME SCSI command
		struct SCSICmd scsi;
		static const UBYTE resume_cmd[10] = {0x4b, 0, 0, 0, 0, 0, 0, 0, 1, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 0;
		scsi.scsi_Command = (UBYTE *)resume_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		return true;
	}
}


/*
 *  Stop CD audio
 */

bool SysCDStop(void *arg, uint8 lead_out_m, uint8 lead_out_s, uint8 lead_out_f)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	if (fh->is_file)
		return false;
	else {

		uint8 end_m = lead_out_m;
		uint8 end_s = lead_out_s;
		uint8 end_f = lead_out_f + 1;
		if (end_f >= 75) {
			end_f = 0;
			end_s++;
			if (end_s >= 60) {
				end_s = 0;
				end_m++;
			}
		}

		// Send PLAY AUDIO MSF SCSI command (play first frame of lead-out area)
		struct SCSICmd scsi;
		UBYTE play_cmd[10] = {0x47, 0, 0, lead_out_m, lead_out_s, lead_out_f, end_m, end_s, end_f, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 0;
		scsi.scsi_Command = play_cmd;
		scsi.scsi_CmdLength = 10;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return false;
		return true;
	}
}


/*
 *  Perform CD audio fast-forward/fast-reverse operation starting from specified address
 */

bool SysCDScan(void *arg, uint8 start_m, uint8 start_s, uint8 start_f, bool reverse)
{
	file_handle *fh = (file_handle *)arg;
	if (!fh)
		return false;

	//!!
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

		// Send MODE SENSE (CD-ROM Audio Control Parameters Page) SCSI command
		struct SCSICmd scsi;
		static const UBYTE mode_sense_cmd[6] = {0x1a, 0x08, 0x0e, 0, 20, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 20;
		scsi.scsi_Command = (UBYTE *)mode_sense_cmd;
		scsi.scsi_CmdLength = 6;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return;

		tmp_buf[6] = 0x04;		// Immed
		tmp_buf[9] = 0;			// LBA/sec format
		tmp_buf[10] = 0;		// LBA/sec
		tmp_buf[11] = 0;
		tmp_buf[13] = left;		// Port 0 volume
		tmp_buf[15] = right;	// Port 1 volume

		// Send MODE SELECT (CD-ROM Audio Control Parameters Page) SCSI command
		static const UBYTE mode_select_cmd[6] = {0x15, 0x10, 0, 0, 20, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 20;
		scsi.scsi_Command = (UBYTE *)mode_select_cmd;
		scsi.scsi_CmdLength = 6;
		scsi.scsi_Flags = SCSIF_WRITE;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		DoIO((struct IORequest *)fh->io);
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

	if (!fh->is_file) {

		// Send MODE SENSE (CD-ROM Audio Control Parameters Page) SCSI command
		struct SCSICmd scsi;
		static const UBYTE mode_sense_cmd[6] = {0x1a, 0x08, 0x0e, 0, 20, 0};
		scsi.scsi_Data = (UWORD *)tmp_buf;
		scsi.scsi_Length = 20;
		scsi.scsi_Command = (UBYTE *)mode_sense_cmd;
		scsi.scsi_CmdLength = 6;
		scsi.scsi_Flags = SCSIF_READ;
		scsi.scsi_Status = 0;
		fh->io->io_Data = &scsi;
		fh->io->io_Length = sizeof(scsi);
		fh->io->io_Command = HD_SCSICMD;
		if (DoIO((struct IORequest *)fh->io) || scsi.scsi_Status)
			return;
		left = tmp_buf[13];		// Port 0 volume
		right = tmp_buf[15];	// Port 1 volume
	}
}
