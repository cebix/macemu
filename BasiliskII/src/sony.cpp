/*
 *  sony.cpp - Replacement .Sony driver (floppy drives)
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
 *    Technote DV 07: "Forcing Floppy Disk Size to be Either 400K or 800K"
 *    Technote DV 17: "Sony Driver: What Your Sony Drives For You"
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
#include "rom_patches.h"
#include "sys.h"
#include "prefs.h"
#include "sony.h"

#define DEBUG 0
#include "debug.h"


// Check for inserted disks by polling?
#ifdef AMIGA
#define DISK_INSERT_CHECK 1
#else
#define DISK_INSERT_CHECK 0
#endif


// Floppy disk icon
const uint8 SonyDiskIcon[258] = {
	0x7f, 0xff, 0xff, 0xf8, 0x81, 0x00, 0x01, 0x04, 0x81, 0x00, 0x71, 0x02, 0x81, 0x00, 0x89, 0x01,
	0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01,
	0x81, 0x00, 0x71, 0x01, 0x81, 0x00, 0x01, 0x01, 0x80, 0xff, 0xfe, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x87, 0xff, 0xff, 0xe1, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x7f, 0xff, 0xff, 0xfe,

	0x7f, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe,

	0, 0
};

// Floppy drive icon
const uint8 SonyDriveIcon[258] = {
	0x7f, 0xff, 0xff, 0xf8, 0x81, 0x00, 0x01, 0x04, 0x81, 0x00, 0x71, 0x02, 0x81, 0x00, 0x89, 0x01,
	0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01, 0x81, 0x00, 0x89, 0x01,
	0x81, 0x00, 0x71, 0x01, 0x81, 0x00, 0x01, 0x01, 0x80, 0xff, 0xfe, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x87, 0xff, 0xff, 0xe1, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11,
	0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x88, 0x00, 0x00, 0x11, 0x7f, 0xff, 0xff, 0xfe,

	0x7f, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xfe,

	0, 0
};


// Struct for each drive
struct sony_drive_info {
	sony_drive_info() : num(0), fh(NULL), read_only(false), status(0) {}
	sony_drive_info(void *fh_, bool ro) : num(0), fh(fh_), read_only(ro), status(0) {}

	void close_fh(void) { Sys_close(fh); }

	int num;			// Drive number
	void *fh;			// Floppy driver file handle
	bool to_be_mounted;	// Flag: drive must be mounted in accRun
	bool read_only;		// Flag: force write protection
	uint32 tag_buffer;	// Mac address of tag buffer
	uint32 status;		// Mac address of drive status record
};

// List of drives handled by this driver
typedef vector<sony_drive_info> drive_vec;
static drive_vec drives;

// Icon addresses (Mac address space, set by PatchROM())
uint32 SonyDiskIconAddr;
uint32 SonyDriveIconAddr;

// Flag: Control(accRun) has been called, interrupt routine is now active
static bool acc_run_called = false;


/*
 *  Get reference to drive info or drives.end() if not found
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
 *  Initialization
 */

void SonyInit(void)
{
	// No drives specified in prefs? Then add defaults
	if (PrefsFindString("floppy", 0) == NULL)
		SysAddFloppyPrefs();

	// Add drives specified in preferences
	int index = 0;
	const char *str;
	while ((str = PrefsFindString("floppy", index++)) != NULL) {
		bool read_only = false;
		if (str[0] == '*') {
			read_only = true;
			str++;
		}
		void *fh = Sys_open(str, read_only);
		if (fh)
			drives.push_back(sony_drive_info(fh, SysIsReadOnly(fh)));
	}
}


/*
 *  Deinitialization
 */

void SonyExit(void)
{
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info)
		info->close_fh();
	drives.clear();
}


/*
 *  Disk was inserted, flag for mounting
 */

bool SonyMountVolume(void *fh)
{
	drive_vec::iterator info = drives.begin(), end = drives.end();
	while (info != end && info->fh != fh)
		++info;
	if (info != end) {
		D(bug("Looking for disk in drive %d\n", info->num));
		if (SysIsDiskInserted(info->fh)) {
			info->read_only = SysIsReadOnly(info->fh);
			WriteMacInt8(info->status + dsDiskInPlace, 1);	// Inserted removable disk
			WriteMacInt8(info->status + dsWriteProt, info->read_only ? 0xff : 0);
			D(bug(" disk inserted, mounting\n"));
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

#if DISK_INSERT_CHECK
		// Disk in drive?
		if (ReadMacInt8(info->status + dsDiskInPlace) == 0) {

			// No, check if disk was inserted
			if (SysIsDiskInserted(info->fh))
				SonyMountVolume(info->fh);
		}
#endif

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
 *  Set error code in DskErr
 */

static int16 set_dsk_err(int16 err)
{
	D(bug("set_dsk_err(%d)\n", err));
	WriteMacInt16(0x142, err);
	return err;
}


/*
 *  Driver Open() routine
 */

int16 SonyOpen(uint32 pb, uint32 dce)
{
	D(bug("SonyOpen\n"));

	// Set up DCE
	WriteMacInt32(dce + dCtlPosition, 0);
	WriteMacInt16(dce + dCtlQHdr + qFlags, ReadMacInt16(dce + dCtlQHdr + qFlags) & 0xff00 | 3);	// Version number, must be >=3 or System 8 will replace us
	acc_run_called = false;

	// Install driver again with refnum -2 (HD20)
	uint32 utab = ReadMacInt32(0x11c);
	WriteMacInt32(utab + 4, ReadMacInt32(utab + 16));

	// Set up fake SonyVars
	WriteMacInt32(0x134, 0xdeadbeef);

	// Clear DskErr
	set_dsk_err(0);

	// Install drives
	drive_vec::iterator info, end = drives.end();
	for (info = drives.begin(); info != end; ++info) {

		info->num = FindFreeDriveNumber(1);
		info->to_be_mounted = false;
		info->tag_buffer = 0;

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
			WriteMacInt16(info->status + dsQType, sony);
			WriteMacInt8(info->status + dsInstalled, 1);
			WriteMacInt8(info->status + dsSides, 0xff);
			WriteMacInt8(info->status + dsTwoSideFmt, 0xff);
			WriteMacInt8(info->status + dsNewIntf, 0xff);
			WriteMacInt8(info->status + dsMFMDrive, 0xff);
			WriteMacInt8(info->status + dsMFMDisk, 0xff);
			WriteMacInt8(info->status + dsTwoMegFmt, 0xff);

			// Disk in drive?
			if (SysIsDiskInserted(info->fh)) {
				WriteMacInt8(info->status + dsDiskInPlace, 1);	// Inserted removable disk
				WriteMacInt8(info->status + dsWriteProt, info->read_only ? 0xff : 0);
				D(bug(" disk inserted, flagging for mount\n"));
				info->to_be_mounted = true;
			}

			// Add drive to drive queue
			D(bug(" adding drive %d\n", info->num));
			r.d[0] = (info->num << 16) | (SonyRefNum & 0xffff);
			r.a[0] = info->status + dsQLink;
			Execute68kTrap(0xa04e, &r);	// AddDrive()
		}
	}
	return noErr;
}


/*
 *  Driver Prime() routine
 */

int16 SonyPrime(uint32 pb, uint32 dce)
{
	WriteMacInt32(pb + ioActCount, 0);

	// Drive valid and disk inserted?
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	if (info == drives.end())
		return set_dsk_err(nsDrvErr);
	if (!ReadMacInt8(info->status + dsDiskInPlace))
		return set_dsk_err(offLinErr);
	WriteMacInt8(info->status + dsDiskInPlace, 2);	// Disk accessed

	// Get parameters
	void *buffer = Mac2HostAddr(ReadMacInt32(pb + ioBuffer));
	size_t length = ReadMacInt32(pb + ioReqCount);
	loff_t position = ReadMacInt32(dce + dCtlPosition);
	if ((length & 0x1ff) || (position & 0x1ff))
		return set_dsk_err(paramErr);

	size_t actual = 0;
	if ((ReadMacInt16(pb + ioTrap) & 0xff) == aRdCmd) {

		// Read
		actual = Sys_read(info->fh, buffer, position, length);
		if (actual != length)
			return set_dsk_err(readErr);

		// Clear TagBuf
		WriteMacInt32(0x2fc, 0);
		WriteMacInt32(0x300, 0);
		WriteMacInt32(0x304, 0);

	} else {

		// Write
		if (info->read_only)
			return set_dsk_err(wPrErr);
		actual = Sys_write(info->fh, buffer, position, length);
		if (actual != length)
			return set_dsk_err(writErr);
	}

	// Update ParamBlock and DCE
	WriteMacInt32(pb + ioActCount, actual);
	WriteMacInt32(dce + dCtlPosition, ReadMacInt32(dce + dCtlPosition) + actual);
	return set_dsk_err(noErr);
}


/*
 *  Driver Control() routine
 */

int16 SonyControl(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SonyControl %d\n", code));

	// General codes
	switch (code) {
		case 1:		// KillIO
			return set_dsk_err(-1);

		case 9:		// Track cache
			return set_dsk_err(noErr);

		case 65:	// Periodic action (accRun, "insert" disks on startup)
			mount_mountable_volumes();
			PatchAfterStartup();		// Install patches after system startup
			WriteMacInt16(dce + dCtlFlags, ReadMacInt16(dce + dCtlFlags) & ~0x2000);	// Disable periodic action
			acc_run_called = true;
			return noErr;
	}

	// Drive valid?
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	if (info == drives.end())
		return set_dsk_err(nsDrvErr);

	// Drive-specific codes
	int16 err = noErr;
	switch (code) {
		case 5:		// Verify disk
			if (ReadMacInt8(info->status + dsDiskInPlace) <= 0)
				err = verErr;
			break;

		case 6:		// Format disk
			if (info->read_only)
				err = wPrErr;
			else if (ReadMacInt8(info->status + dsDiskInPlace) > 0) {
				if (!SysFormat(info->fh))
					err = writErr;
			} else
				err = offLinErr;
			break;

		case 7:		// Eject
			if (ReadMacInt8(info->status + dsDiskInPlace) > 0) {
				SysEject(info->fh);
				WriteMacInt8(info->status + dsDiskInPlace, 0);
			}
			break;

		case 8:		// Set tag buffer
			info->tag_buffer = ReadMacInt32(pb + csParam);
			break;

		case 21:		// Get drive icon
			WriteMacInt32(pb + csParam, SonyDriveIconAddr);
			break;

		case 22:		// Get disk icon
			WriteMacInt32(pb + csParam, SonyDiskIconAddr);
			break;

		case 23:		// Get drive info
			if (info->num == 1)
				WriteMacInt32(pb + csParam, 0x0004);	// Internal drive
			else
				WriteMacInt32(pb + csParam, 0x0104);	// External drive
			break;

		case 0x5343:	// Format and write to disk ('SC'), used by DiskCopy
			if (!ReadMacInt8(info->status + dsDiskInPlace))
				err = offLinErr;
			else if (info->read_only)
				err = wPrErr;
			else {
				void *data = Mac2HostAddr(ReadMacInt32(pb + csParam + 2));
				size_t actual = Sys_write(info->fh, data, 0, 2880*512);
				if (actual != 2880*512)
					err = writErr;
			}
			break;

		default:
			printf("WARNING: Unknown SonyControl(%d)\n", code);
			err = controlErr;
			break;
	}

	return set_dsk_err(err);
}


/*
 *  Driver Status() routine
 */

int16 SonyStatus(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SonyStatus %d\n", code));

	// Drive valid?
	drive_vec::iterator info = get_drive_info(ReadMacInt16(pb + ioVRefNum));
	if (info == drives.end())
		return set_dsk_err(nsDrvErr);

	int16 err = noErr;
	switch (code) {
		case 6:		// Return format list
			if (ReadMacInt16(pb + csParam) > 0) {
				uint32 adr = ReadMacInt32(pb + csParam + 2);
				WriteMacInt16(pb + csParam, 1);		// 1 format
				WriteMacInt32(adr, 2880);			// 2880 sectors
				WriteMacInt32(adr + 4, 0xd2120050);	// 2 heads, 18 secs/track, 80 tracks
			} else
				err = paramErr;
			break;

		case 8:		// Get drive status
			Mac2Mac_memcpy(pb + csParam, info->status, 22);
			break;

		case 10:	// Get disk type
			WriteMacInt32(pb + csParam, ReadMacInt32(info->status + dsMFMDrive) & 0xffffff00 | 0xfe);
			break;

		case 0x4456: // Duplicator version supported ('DV')
			WriteMacInt16(pb + csParam, 0x0410);
			break;

		case 0x5343: // Get address header format byte ('SC')
			WriteMacInt8(pb + csParam, 0x22);	// 512 bytes/sector
			break;

		default:
			printf("WARNING: Unknown SonyStatus(%d)\n", code);
			err = statusErr;
			break;
	}

	return set_dsk_err(err);
}


/*
 *  Driver interrupt routine (1Hz) - check for volumes to be mounted
 */

void SonyInterrupt(void)
{
	if (!acc_run_called)
		return;

	mount_mountable_volumes();
}
