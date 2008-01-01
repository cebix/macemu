/*
 *	$Id$
 *
 *	macos_util_macosx.h - Work around clashes with the enums in <CarbonCore/OSUtils.h>
 *						  Based on:
 *
 *  macos_util.h - MacOS definitions/utility functions
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

#ifndef MACOS_UTIL_H
#define MACOS_UTIL_H

#include "cpu_emulation.h"

#import <Carbon/Carbon.h>

/*
 *  Queues
 */

enum {	// QElem struct
	qLink = 0,
	qType = 4,
	qData = 6
};

enum {	// QHdr struct
	qFlags = 0,
	qHead = 2,
	qTail = 6
};


/*
 *  Definitions for Device Manager
 */

// Misc constants

enum {	// IOParam struct
	ioTrap = 6,
	ioCmdAddr = 8,
	ioCompletion = 12,
	ioResult = 16,
	ioNamePtr = 18,
	ioVRefNum = 22,
	ioRefNum = 24,
	ioVersNum = 26,
	ioPermssn = 27,
	ioMisc = 28,
	ioBuffer = 32,
	ioReqCount = 36,
	ioActCount = 40,
	ioPosMode = 44,
	ioPosOffset = 46,
	ioWPosOffset = 46,	// Wide positioning offset when ioPosMode has kWidePosOffsetBit set
	SIZEOF_IOParam = 50
};

enum {	// CntrlParam struct
	csCode = 26,
	csParam = 28
};

enum {	// DrvSts struct
	dsTrack = 0,
	dsWriteProt = 2,
	dsDiskInPlace = 3,
	dsInstalled = 4,
	dsSides = 5,
	dsQLink = 6,
	dsQType = 10,
	dsQDrive = 12,
	dsQRefNum = 14,
	dsQFSID = 16,
	dsTwoSideFmt = 18,
	dsNewIntf = 19,
	dsDiskErrs = 20,
	dsMFMDrive = 22,
	dsMFMDisk = 23,
	dsTwoMegFmt = 24
};

enum {	// DrvSts2 struct
	dsDriveSize = 18,
	dsDriveS1 = 20,
	dsDriveType = 22,
	dsDriveManf = 24,
	dsDriveChar = 26,
	dsDriveMisc = 28,
	SIZEOF_DrvSts = 30
};

enum {	// DCtlEntry struct
	dCtlDriver = 0,
	dCtlFlags = 4,
	dCtlQHdr = 6,
	dCtlPosition = 16,
	dCtlStorage = 20,
	dCtlRefNum = 24,
	dCtlCurTicks = 26,
	dCtlWindow = 30,
	dCtlDelay = 34,
	dCtlEMask = 36,
	dCtlMenu = 38,
	dCtlSlot = 40,
	dCtlSlotId = 41,
	dCtlDevBase = 42,
	dCtlOwner = 46,
	dCtlExtDev = 50,
	dCtlFillByte = 51,
	dCtlNodeID = 52
};


/*
 *  Definitions for Deferred Task Manager
 */

enum {	// DeferredTask struct
	dtFlags = 6,
	dtAddr = 8,
	dtParam = 12,
	dtReserved = 16
};


// Definitions for DebugUtil() Selector
enum {
	duDebuggerGetMax = 0,
	duDebuggerEnter = 1,
	duDebuggerExit = 2,
	duDebuggerPoll = 3,
	duGetPageState = 4,
	duPageFaultFatal = 5,
	duDebuggerLockMemory = 6,
	duDebuggerUnlockMemory = 7,
	duEnterSupervisorMode = 8
};

// Functions
extern void EnqueueMac(uint32 elem, uint32 list);	// Enqueue QElem in list
extern int FindFreeDriveNumber(int num);			// Find first free drive number, starting at "num"
extern void MountVolume(void *fh);					// Mount volume with given file handle (see sys.h)
extern void FileDiskLayout(loff_t size, uint8 *data, loff_t &start_byte, loff_t &real_size);	// Calculate disk image file layout given file size and first 256 data bytes
extern uint32 DebugUtil(uint32 Selector);			// DebugUtil() Replacement
extern uint32 TimeToMacTime(time_t t);				// Convert time_t value to MacOS time

// Construct four-character-code
#define FOURCC(a,b,c,d) (((uint32)(a) << 24) | ((uint32)(b) << 16) | ((uint32)(c) << 8) | (uint32)(d))

// Emulator identification codes (4 and 2 characters)
const uint32 EMULATOR_ID_4 = 0x62617369;			// 'basi'
const uint16 EMULATOR_ID_2 = 0x6261;				// 'ba'

// Test if basic MacOS initializations (of the ROM) are done
static inline bool HasMacStarted(void)
{
	return ReadMacInt32(0xcfc) == FOURCC('W','L','S','C');	// Mac warm start flag
}

#endif
