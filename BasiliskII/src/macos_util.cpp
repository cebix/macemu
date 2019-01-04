/*
 *  macos_util.cpp - MacOS definitions/utility functions
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
#include "cpu_emulation.h"
#include "adb.h"
#include "main.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "macos_util.h"

#define DEBUG 0
#include "debug.h"


/*
 *  Enqueue QElem to list
 */

void EnqueueMac(uint32 elem, uint32 list)
{
	WriteMacInt32(elem + qLink, 0);
	if (!ReadMacInt32(list + qTail)) {
		WriteMacInt32(list + qHead, elem);
		WriteMacInt32(list + qTail, elem);
	} else {
		WriteMacInt32(ReadMacInt32(list + qTail) + qLink, elem);
		WriteMacInt32(list + qTail, elem);
	}
}


/*
 *  Find first free drive number, starting at num
 */

static bool is_drive_number_free(int num)
{
	uint32 e = ReadMacInt32(0x308 + qHead);
	while (e) {
		uint32 d = e - dsQLink;
		if ((int)ReadMacInt16(d + dsQDrive) == num)
			return false;
		e = ReadMacInt32(e + qLink);
	}
	return true;
}

int FindFreeDriveNumber(int num)
{
	while (!is_drive_number_free(num))
		num++;
	return num;
}


/*
 *  Mount volume with given file handle (call this function when you are unable to
 *  do automatic media change detection and the user has to press a special key
 *  or something to mount a volume; this function will check if there's really a
 *  volume in the drive with SysIsDiskInserted(); volumes which are present on startup
 *  are automatically mounted)
 */

void MountVolume(void *fh)
{
	SonyMountVolume(fh) || DiskMountVolume(fh) || CDROMMountVolume(fh);
}


/*
 *  Calculate disk image file layout given file size and first 256 data bytes
 */

void FileDiskLayout(loff_t size, uint8 *data, loff_t &start_byte, loff_t &real_size)
{
	if (size == 419284 || size == 838484) {
		// 400K/800K DiskCopy image, 84 byte header
		start_byte = 84;
		real_size = (size - 84) & ~0x1ff;
	} else {
		// 0..511 byte header
		start_byte = size & 0x1ff;
		real_size = size - start_byte;
	}
}


uint32 DebugUtil(uint32 Selector)
{
	switch (Selector) {
		case duDebuggerGetMax:
			return 3;
		case duDebuggerEnter:
			return 0;
		case duDebuggerExit:
			return 0;
		case duDebuggerPoll:
			ADBInterrupt();
			return 0;
		default:
			return (uint32) paramErr;
	}
}


/*
 *  Convert time_t value to MacOS time (seconds since 1.1.1904)
 */

uint32 TimeToMacTime(time_t t)
{
	// This code is taken from glibc 2.2

	// Convert to number of seconds elapsed since 1-Jan-1904
	struct tm *local = localtime(&t);
	const int TM_EPOCH_YEAR = 1900;
	const int MAC_EPOCH_YEAR = 1904;
	int a4 = ((local->tm_year + TM_EPOCH_YEAR) >> 2) - !(local->tm_year & 3);
	int b4 = (MAC_EPOCH_YEAR >> 2) - !(MAC_EPOCH_YEAR & 3);
	int a100 = a4 / 25 - (a4 % 25 < 0);
	int b100 = b4 / 25 - (b4 % 25 < 0);
	int a400 = a100 >> 2;
	int b400 = b100 >> 2;
	int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
	uint32 days = local->tm_yday + 365 * (local->tm_year - 4) + intervening_leap_days;
	return local->tm_sec + 60 * (local->tm_min + 60 * (local->tm_hour + 24 * days));
}

/*
 *  Convert MacOS time to time_t (seconds since 1.1.1970)
 */

time_t MacTimeToTime(uint32 t)
{
	// simply subtract number of seconds between 1.1.1904 and 1.1.1970
	return t - 2082826800;
}
