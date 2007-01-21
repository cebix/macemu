/*
 *  disk.cpp - Generic disk driver
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Devices, chapter 1 "Device Manager"
 *    Technote DV 05: "Drive Queue Elements"
 *    Technote DV 23: "Driver Education"
 *    Technote FL 24: "Don't Look at ioPosOffset for Devices"
 */

#include "sysdeps.h"

#include <string.h>
#include <vector>

#ifndef NO_STD_NAMESPACE
using std::vector;
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "sys.h"
#include "prefs.h"
#include "disk.h"

#define DEBUG 0
#include "debug.h"


// .Disk Disk/drive icon
const uint8 DiskIcon[258] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe,
	0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0x8c, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0, 0
};


// Struct for each drive
struct disk_drive_info {
	disk_drive_info() : num(0), fh(NULL), start_byte(0), read_only(false), status(0) {}
	disk_drive_info(void *fh_, bool ro) : num(0), fh(fh_), read_only(ro), status(0) {}

	void close_fh(void) { Sys_close(fh); }

	int num;			// Drive number
	void *fh;			// File handle
	loff_t start_byte;	// Start of HFS partition on disk
	uint32 num_blocks;	// Size in 512-byte blocks
	bool to_be_mounted;	// Flag: drive must be mounted in accRun
	bool read_only;		// Flag: force write protection
	uint32 status;		// Mac address of drive status record
};

// List of drives handled by this driver
typedef vector<disk_drive_info> drive_vec;
static drive_vec drives;

// Icon address (Mac address space, set by PatchROM())
uint32 DiskIconAddr;

// Flag: Control(accRun) has been called, interrupt routine is now active
static bool acc_run_called = false;


/*
 *  Get pointer to drive info or drives.end() if not found
 */

static drive_vec::iterator get_drive_info(int num)
{
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info) {
		if (info->num == num)
			return info;
	}
	return info;
}


/*
 *  Find HFS partition, set info->start_byte and info->num_blocks
 *  (0 = no partition map or HFS partition found, assume flat disk image)
 */

static void find_hfs_partition(disk_drive_info &info)
{
	info.start_byte = 0;
	info.num_blocks = 0;
	uint8 *map = new uint8[512];

	// Search first 64 blocks for HFS partition
	for (int i=0; i<64; i++) {
		if (Sys_read(info.fh, map, i * 512, 512) != 512)
			break;

		// Not a partition map block? Then look at next block
		uint16 sig = (map[0] << 8) | map[1];
		if (sig != 0x504d)
			continue;

		// Partition map block found, Apple HFS partition?
		if (strcmp((char *)(map + 48), "Apple_HFS") == 0) {
			info.start_byte = (loff_t)((map[8] << 24) | (map[9] << 16) | (map[10] << 8) | map[11]) << 9;
			info.num_blocks = (map[12] << 24) | (map[13] << 16) | (map[14] << 8) | map[15];
			D(bug(" HFS partition found at %d, %d blocks\n", info.start_byte, info.num_blocks));
			break;
		}
	}
	delete[] map;
}


/*
 *  Initialization
 */

void DiskInit(void)
{
	// No drives specified in prefs? Then add defaults
	if (PrefsFindString("disk", 0) == NULL)
		SysAddDiskPrefs();

	// Add drives specified in preferences
	int index = 0;
	const char *str;
	while ((str = PrefsFindString("disk", index++)) != NULL) {
		bool read_only = false;
		if (str[0] == '*') {
			read_only = true;
			str++;
		}
		void *fh = Sys_open(str, read_only);
		if (fh)
			drives.push_back(disk_drive_info(fh, SysIsReadOnly(fh)));
	}
}


/*
 *  Deinitialization
 */

void DiskExit(void)
{
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info)
		info->close_fh();
	drives.clear();
}


/*
 *  Disk was inserted, flag for mounting
 */

bool DiskMountVolume(void *fh)
{
	drive_vec::iterator info = drives.begin(), end = drives.end();
	while (info != end && info->fh != fh)
		++info;
	if (info != end) {
		if (SysIsDiskInserted(info->fh)) {
			info->read_only = SysIsReadOnly(info->fh);
			WriteMacInt8(info->status + dsDiskInPlace, 1);	// Inserted removable disk
			WriteMacInt8(info->status + dsWriteProt, info->read_only ? 0xff : 0);
			find_hfs_partition(*info);
			if (info->start_byte == 0)
				info->num_blocks = SysGetFileSize(info->fh) / 512;
			WriteMacInt16(info->status + dsDriveSize, info->num_blocks & 0xffff);
			WriteMacInt16(info->status + dsDriveS1, info->num_blocks >> 16);
			info->to_be_mounted = true;
		}
		return true;
	} else
		return false;
}


/*
 *  Mount volumes for which the to_be_mounted flag is set
 *  (called during interrupt time)
 */

static void mount_mountable_volumes(void)
{
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info) {

		// Disk in drive?
		if (!ReadMacInt8(info->status + dsDiskInPlace)) {

			// No, check if disk was inserted
			if (SysIsDiskInserted(info->fh))
				DiskMountVolume(info->fh);
		}

		// Mount disk if flagged
		if (info->to_be_mounted) {
			D(bug(" mounting drive %d\n", info->num));
			M68kRegisters r;
			r.d[0] = info->num;
			r.a[0] = 7;	// diskEvent
			Execute68kTrap(0xa02f, &r);		// PostEvent()
			info->to_be_mounted = false;
		}
	}
}


/*
 *  Driver Open() routine
 */

int16 DiskOpen(uint32 pb, uint32 dce)
{
	D(bug("DiskOpen\n"));

	// Set up DCE
	WriteMacInt32(dce + dCtlPosition, 0);
	acc_run_called = false;

	// Install drives
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info) {

		info->num = FindFreeDriveNumber(1);
		info->to_be_mounted = false;

		if (info->fh) {

			// Allocate drive status record
			M68kRegisters r;
			r.d[0] = SIZEOF_DrvSts;
			Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
			if (r.a[0] == 0)
				continue;
			info->status = r.a[0];
			D(bug(" DrvSts at %08lx\n", info->status));

			// Set up drive status
			WriteMacInt16(info->status + dsQType, hard20);
			WriteMacInt8(info->status + dsInstalled, 1);
			bool disk_in_place = false;
			if (SysIsFixedDisk(info->fh)) {
				WriteMacInt8(info->status + dsDiskInPlace, 8);	// Fixed disk
				disk_in_place = true;
			} else if (SysIsDiskInserted(info->fh)) {
				WriteMacInt8(info->status + dsDiskInPlace, 1);	// Inserted removable disk
				disk_in_place = true;
			}
			if (disk_in_place) {
				D(bug(" disk inserted\n"));
				WriteMacInt8(info->status + dsWriteProt, info->read_only ? 0x80 : 0);
				find_hfs_partition(*info);
				if (info->start_byte == 0)
					info->num_blocks = SysGetFileSize(info->fh) / 512;
				info->to_be_mounted = true;
			}
			D(bug(" %d blocks\n", info->num_blocks));
			WriteMacInt16(info->status + dsDriveSize, info->num_blocks & 0xffff);
			WriteMacInt16(info->status + dsDriveS1, info->num_blocks >> 16);

			// Add drive to drive queue
			D(bug(" adding drive %d\n", info->num));
			r.d[0] = (info->num << 16) | (DiskRefNum & 0xffff);
			r.a[0] = info->status + dsQLink;
			Execute68kTrap(0xa04e, &r);	// AddDrive()
		}
	}
	return noErr;
}


/*
 *  Driver Prime() routine
 */

int16 DiskPrime(uint32 pb, uint32 dce)
{
	WriteMacInt32(pb + ioActCount, 0);

	// Drive valid and disk inserted?
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	if (info == drives.end())
		return nsDrvErr;
	if (!ReadMacInt8(info->status + dsDiskInPlace))
		return offLinErr;

	// Get parameters
	void *buffer = Mac2HostAddr(ReadMacInt32(pb + ioBuffer));
	size_t length = ReadMacInt32(pb + ioReqCount);
	loff_t position = ReadMacInt32(dce + dCtlPosition);
	if (ReadMacInt16(pb + ioPosMode) & 0x100)	// 64 bit positioning
		position = ((loff_t)ReadMacInt32(pb + ioWPosOffset) << 32) | ReadMacInt32(pb + ioWPosOffset + 4);
	if ((length & 0x1ff) || (position & 0x1ff))
		return paramErr;

	size_t actual = 0;
	if ((ReadMacInt16(pb + ioTrap) & 0xff) == aRdCmd) {

		// Read
		actual = Sys_read(info->fh, buffer, position + info->start_byte, length);
		if (actual != length)
			return readErr;

	} else {

		// Write
		if (info->read_only)
			return wPrErr;
		actual = Sys_write(info->fh, buffer, position + info->start_byte, length);
		if (actual != length)
			return writErr;
	}

	// Update ParamBlock and DCE
	WriteMacInt32(pb + ioActCount, actual);
	WriteMacInt32(dce + dCtlPosition, ReadMacInt32(dce + dCtlPosition) + actual);
	return noErr;
}


/*
 *  Driver Control() routine
 */

int16 DiskControl(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("DiskControl %d\n", code));

	// General codes
	switch (code) {
		case 1:		// KillIO
			return noErr;

		case 65: {	// Periodic action (accRun, "insert" disks on startup)
			mount_mountable_volumes();
			WriteMacInt16(dce + dCtlFlags, ReadMacInt16(dce + dCtlFlags) & ~0x2000);	// Disable periodic action
			acc_run_called = true;
			return noErr;
		}
	}

	// Drive valid?
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	if (info == drives.end())
		return nsDrvErr;

	// Drive-specific codes
	switch (code) {
		case 5:		// Verify disk
			if (ReadMacInt8(info->status + dsDiskInPlace) > 0)
				return noErr;
			else
				return offLinErr;

		case 6:		// Format disk
			if (info->read_only)
				return wPrErr;
			else if (ReadMacInt8(info->status + dsDiskInPlace) > 0)
				return noErr;
			else
				return offLinErr;

		case 7:		// Eject disk
			if (ReadMacInt8(info->status + dsDiskInPlace) == 8) {
				// Fixed disk, re-insert
				M68kRegisters r;
				r.d[0] = info->num;
				r.a[0] = 7;	// diskEvent
				Execute68kTrap(0xa02f, &r);		// PostEvent()
			} else if (ReadMacInt8(info->status + dsDiskInPlace) > 0) {
				SysEject(info->fh);
				WriteMacInt8(info->status + dsDiskInPlace, 0);
			}
			return noErr;

		case 21:	// Get drive icon
		case 22:	// Get disk icon
			WriteMacInt32(pb + csParam, DiskIconAddr);
			return noErr;

		case 23:	// Get drive info
			if (ReadMacInt8(info->status + dsDiskInPlace) == 8)
				WriteMacInt32(pb + csParam, 0x0601);	// Unspecified fixed SCSI disk
			else
				WriteMacInt32(pb + csParam, 0x0201);	// Unspecified SCSI disk
			return noErr;

		case 24:	// Get partition size
			if (ReadMacInt8(info->status + dsDiskInPlace) > 0) {
				WriteMacInt32(pb + csParam, info->num_blocks);
				return noErr;
			} else
				return offLinErr;

		default:
			printf("WARNING: Unknown DiskControl(%d)\n", code);
			return controlErr;
	}
}


/*
 *  Driver Status() routine
 */

int16 DiskStatus(uint32 pb, uint32 dce)
{
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("DiskStatus %d\n", code));

	// General codes (we can get these even if the drive was invalid)
	switch (code) {
		case 43: {	// Driver gestalt
			uint32 sel = ReadMacInt32(pb + csParam);
			D(bug(" driver gestalt %c%c%c%c\n", sel >> 24, sel >> 16,  sel >> 8, sel));
			switch (sel) {
				case FOURCC('v','e','r','s'):	// Version
					WriteMacInt32(pb + csParam + 4, 0x01008000);
					break;
				case FOURCC('d','e','v','t'):	// Device type
					if (info != drives.end()) {
						if (ReadMacInt8(info->status + dsDiskInPlace) == 8)
							WriteMacInt32(pb + csParam + 4, FOURCC('d','i','s','k'));
						else
							WriteMacInt32(pb + csParam + 4, FOURCC('r','d','s','k'));
					} else
						WriteMacInt32(pb + csParam + 4, FOURCC('d','i','s','k'));
					break;
				case FOURCC('i','n','t','f'):	// Interface type
					WriteMacInt32(pb + csParam + 4, EMULATOR_ID_4);
					break;
				case FOURCC('s','y','n','c'):	// Only synchronous operation?
					WriteMacInt32(pb + csParam + 4, 0x01000000);
					break;
				case FOURCC('b','o','o','t'):	// Boot ID
					if (info != drives.end())
						WriteMacInt16(pb + csParam + 4, info->num);
					else
						WriteMacInt16(pb + csParam + 4, 0);
					WriteMacInt16(pb + csParam + 6, (uint16)DiskRefNum);
					break;
				case FOURCC('w','i','d','e'):	// 64-bit access supported?
					WriteMacInt16(pb + csParam + 4, 0x0100);
					break;
				case FOURCC('p','u','r','g'):	// Purge flags
					WriteMacInt32(pb + csParam + 4, 0);
					break;
				case FOURCC('e','j','e','c'):	// Eject flags
					WriteMacInt32(pb + csParam + 4, 0x00030003);	// Don't eject on shutdown/restart
					break;
				case FOURCC('f','l','u','s'):	// Flush flags
					WriteMacInt16(pb + csParam + 4, 0);
					break;
				case FOURCC('v','m','o','p'):	// Virtual memory attributes
					WriteMacInt32(pb + csParam + 4, 0);	// Drive not available for VM
					break;
				default:
					return statusErr;
			}
			return noErr;
		}
	}

	// Drive valid?
	if (info == drives.end())
		return nsDrvErr;

	// Drive-specific codes
	switch (code) {
		case 8:		// Get drive status
			Mac2Mac_memcpy(pb + csParam, info->status, 22);
			return noErr;

		case 44: // get startup partition status: http://developer.apple.com/documentation/Hardware/DeviceManagers/ata/ata_ref/ATA.21.html
			printf("WARNING: DiskStatus(44:'get startup partition status') Not Implemented\n");
			return statusErr;

		case 45: // get partition write protect status: http://developer.apple.com/documentation/Hardware/DeviceManagers/ata/ata_ref/ATA.23.html
			printf("WARNING: DiskStatus(45:'get partition write protect status') Not Implemented\n");
			return statusErr;

		case 46: // get partition mount status: http://developer.apple.com/documentation/Hardware/DeviceManagers/ata/ata_ref/ATA.22.html
			printf("WARNING: DiskStatus(46:'get partition mount status') Not Implemented\n");
			return statusErr;

		case 70: // get power mode status: http://developer.apple.com/documentation/Hardware/DeviceManagers/ata/ata_ref/ATA.24.html
			printf("WARNING: DiskStatus(70:'get power mode status') Not Implemented\n");
			return statusErr;

		default:
			printf("WARNING: Unknown DiskStatus(%d)\n", code);
			return statusErr;
	}
}


/*
 *  Driver interrupt routine (1Hz) - check for volumes to be mounted
 */

void DiskInterrupt(void)
{
	if (!acc_run_called)
		return;

	mount_mountable_volumes();
}
