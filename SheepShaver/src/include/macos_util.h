/*
 *  macos_util.h - MacOS definitions/utility functions
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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
#include "thunks.h"
#include <stddef.h>


/*
 *  General definitions
 */

struct Point {
	int16 v;
	int16 h;
};

struct Rect {
	int16 top;
	int16 left;
	int16 bottom;
	int16 right;
};


/*
 *  Queues
 */

enum {	// Queue types
	dummyType	= 0,
	vType		= 1,
	ioQType		= 2,
	drvQType	= 3,
	evType		= 4,
	fsQType		= 5,
	sIQType		= 6,
	dtQType		= 7,
	nmType		= 8
};

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
 *  Definitions for Deferred Task Manager
 */

enum {	// DeferredTask struct
	dtFlags = 6,
	dtAddr = 8,
	dtParam = 12,
	dtReserved = 16
};


/*
 *  Definitions for Device Manager
 */

// Error codes
enum {
	noErr			= 0,
	controlErr		= -17,			/* I/O System Errors */
	statusErr		= -18,			/* I/O System Errors */
	readErr			= -19,			/* I/O System Errors */
	writErr			= -20,			/* I/O System Errors */
	badUnitErr		= -21,			/* I/O System Errors */
	unitEmptyErr	= -22,			/* I/O System Errors */
	openErr			= -23,			/* I/O System Errors */
	closErr			= -24,			/* I/O System Errors */
	abortErr		= -27,			/* IO call aborted by KillIO */
	notOpenErr		= -28,			/* Driver not open */
	dskFulErr		= -34,			/* disk full */
	nsvErr			= -35,			/* no such volume */
	ioErr			= -36,			/* I/O error (bummers) */
	bdNamErr		= -37,			/* bad name */
	fnOpnErr		= -38,			/* file not open */
	eofErr			= -39,			/* End-of-file encountered */
	posErr			= -40,			/* tried to position to before start of file (r/w) */
	tmfoErr			= -42,			/* too many files open */
	fnfErr			= -43,			/* file not found */
	wPrErr			= -44,			/* diskette is write protected. */
	fLckdErr		= -45,			/* file is locked */
	fBsyErr			= -47,			/* file busy, dir not empty */
	dupFNErr		= -48,			/* duplicate filename already exists */
	paramErr		= -50,			/* error in user parameter list */
	rfNumErr		= -51,			/* bad ioRefNum */
	permErr			= -54,			/* permission error */
	nsDrvErr		= -56,			/* no such driver number */
	extFSErr		= -58,			/* external file system */
	noDriveErr		= -64,			/* drive not installed */
	offLinErr		= -65,			/* r/w requested for an off-line drive */
	noNybErr		= -66,			/* couldn't find 5 nybbles in 200 tries */
	noAdrMkErr		= -67,			/* couldn't find valid addr mark */
	dataVerErr		= -68,			/* read verify compare failed */
	badCksmErr		= -69,			/* addr mark checksum didn't check */
	badBtSlpErr		= -70,			/* bad addr mark bit slip nibbles */
	noDtaMkErr		= -71,			/* couldn't find a data mark header */
	badDCksum		= -72,			/* bad data mark checksum */
	badDBtSlp		= -73,			/* bad data mark bit slip nibbles */
	wrUnderrun		= -74,			/* write underrun occurred */
	cantStepErr		= -75,			/* step handshake failed */
	tk0BadErr		= -76,			/* track 0 detect doesn't change */
	initIWMErr		= -77,			/* unable to initialize IWM */
	twoSideErr		= -78,			/* tried to read 2nd side on a 1-sided drive */
	spdAdjErr		= -79,			/* unable to correctly adjust disk speed */
	seekErr			= -80,			/* track number wrong on address mark */
	sectNFErr		= -81,			/* sector number never found on a track */
	fmt1Err			= -82,			/* can't find sector 0 after track format */
	fmt2Err			= -83,			/* can't get enough sync */
	verErr			= -84,			/* track failed to verify */
	memFullErr		= -108,
	dirNFErr		= -120			/* directory not found */
};

// Misc constants
enum {
	goodbye			= -1,			/* heap being reinitialized */

	ioInProgress	= 1,			/* predefined value of ioResult while I/O is pending */
	aRdCmd			= 2,			/* low byte of ioTrap for Read calls */
	aWrCmd			= 3,			/* low byte of ioTrap for Write calls */
	asyncTrpBit		= 10,			/* trap word modifier */
	noQueueBit		= 9,			/* trap word modifier */

	dReadEnable		= 0,			/* set if driver responds to read requests */
	dWritEnable		= 1,			/* set if driver responds to write requests */
	dCtlEnable		= 2,			/* set if driver responds to control requests */
	dStatEnable		= 3,			/* set if driver responds to status requests */
	dNeedGoodBye	= 4,			/* set if driver needs time for performing periodic tasks */
	dNeedTime		= 5,			/* set if driver needs time for performing periodic tasks */
	dNeedLock		= 6,			/* set if driver must be locked in memory as soon as it is opened */

	dOpened			= 5,			/* driver is open */
	dRAMBased		= 6,			/* dCtlDriver is a handle (1) or pointer (0) */
	drvrActive		= 7,			/* driver is currently processing a request */

	rdVerify		= 64,

	fsCurPerm		= 0,			// Whatever is currently allowed
	fsRdPerm		= 1,			// Exclusive read
	fsWrPerm		= 2,			// Exclusive write
	fsRdWrPerm		= 3,			// Exclusive read/write
	fsRdWrShPerm	= 4,			// Shared read/write

	fsAtMark		= 0,			// At current mark
	fsFromStart		= 1,			// Set mark rel to BOF
	fsFromLEOF		= 2,			// Set mark rel to logical EOF
	fsFromMark		= 3,			// Set mark rel to current mark

	sony			= 0,
	hard20			= 1
};

enum {						/* Large Volume Constants */
	kWidePosOffsetBit			= 8,
	kMaximumBlocksIn4GB		= 0x007FFFFF
};

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
 *  Definitions for native Device Manager
 */

// Registry EntryID
struct RegEntryID {
	uint32 contents[4];
};

// Command codes
enum {
	kOpenCommand				= 0,
	kCloseCommand				= 1,
	kReadCommand				= 2,
	kWriteCommand				= 3,
	kControlCommand				= 4,
	kStatusCommand				= 5,
	kKillIOCommand				= 6,
	kInitializeCommand			= 7,							/* init driver and device*/
	kFinalizeCommand			= 8,							/* shutdown driver and device*/
	kReplaceCommand				= 9,							/* replace an old driver*/
	kSupersededCommand			= 10,							/* prepare to be replaced by a new driver*/
	kSuspendCommand				= 11,							/* prepare driver to go to sleep*/
	kResumeCommand				= 12							/* wake up sleeping driver*/
};

// Command kinds
enum {
	kSynchronousIOCommandKind	= 0x00000001,
	kAsynchronousIOCommandKind	= 0x00000002,
	kImmediateIOCommandKind		= 0x00000004
};


/* 
 *  Definitions for Mixed Mode Manager
 */

typedef uint32 ProcInfoType;
typedef int8 ISAType;
typedef uint16 RoutineFlagsType;
typedef uint32 ProcPtr;
typedef uint8 RDFlagsType;

struct RoutineRecord {
	ProcInfoType 					procInfo;					/* calling conventions */
	int8 							reserved1;					/* Must be 0 */
	ISAType 						ISA;						/* Instruction Set Architecture */
	RoutineFlagsType 				routineFlags;				/* Flags for each routine */
	ProcPtr 						procDescriptor;				/* Where is the thing we’re calling? */
	uint32 							reserved2;					/* Must be 0 */
	uint32 							selector;					/* For dispatched routines, the selector */
};

struct RoutineDescriptor {
	uint16 							goMixedModeTrap;			/* Our A-Trap */
	int8 							version;					/* Current Routine Descriptor version */
	RDFlagsType 					routineDescriptorFlags;		/* Routine Descriptor Flags */
	uint32 							reserved1;					/* Unused, must be zero */
	uint8 							reserved2;					/* Unused, must be zero */
	uint8 							selectorInfo;				/* If a dispatched routine, calling convention, else 0 */
	uint16 							routineCount;				/* Number of routines in this RD */
	RoutineRecord 					routineRecords[1];			/* The individual routines */
};

struct SheepRoutineDescriptor
	: public SheepVar
{
	SheepRoutineDescriptor(ProcInfoType procInfo, uint32 procedure)
		: SheepVar(sizeof(RoutineDescriptor))
	{
		const uintptr desc = addr();
		Mac_memset(desc, 0, sizeof(RoutineDescriptor));
		WriteMacInt16(desc + offsetof(RoutineDescriptor, goMixedModeTrap), 0xAAFE);
		WriteMacInt8 (desc + offsetof(RoutineDescriptor, version), 7);
		WriteMacInt32(desc + offsetof(RoutineDescriptor, routineRecords) + offsetof(RoutineRecord, procInfo), procInfo);
		WriteMacInt8 (desc + offsetof(RoutineDescriptor, routineRecords) + offsetof(RoutineRecord, ISA), 1);
		WriteMacInt16(desc + offsetof(RoutineDescriptor, routineRecords) + offsetof(RoutineRecord, routineFlags), 0 | 0 | 4);
		WriteMacInt32(desc + offsetof(RoutineDescriptor, routineRecords) + offsetof(RoutineRecord, procDescriptor), procedure);
	}
};


// Functions
extern void MacOSUtilReset(void);
extern void Enqueue(uint32 elem, uint32 list);			// Enqueue QElem to list
extern int FindFreeDriveNumber(int num);				// Find first free drive number, starting at "num"
extern void MountVolume(void *fh);						// Mount volume with given file handle (see sys.h)
extern void FileDiskLayout(loff_t size, uint8 *data, loff_t &start_byte, loff_t &real_size);	// Calculate disk image file layout given file size and first 256 data bytes
extern uint32 FindLibSymbol(char *lib, char *sym);		// Find symbol in shared library
extern void InitCallUniversalProc(void);				// Init CallUniversalProc()
extern long CallUniversalProc(void *upp, uint32 info);	// CallUniversalProc()
extern uint32 TimeToMacTime(time_t t);					// Convert time_t value to MacOS time
extern uint32 Mac_sysalloc(uint32 size);				// Allocate block in MacOS system heap zone
extern void Mac_sysfree(uint32 addr);					// Release block occupied by the nonrelocatable block p

// Construct four-character-code from string
#define FOURCC(a,b,c,d) (((uint32)(a) << 24) | ((uint32)(b) << 16) | ((uint32)(c) << 8) | (uint32)(d))

// Emulator identification codes (4 and 2 characters)
const uint32 EMULATOR_ID_4 = 0x62616168;			// 'baah'
const uint16 EMULATOR_ID_2 = 0x6261;				// 'ba'

// Test if basic MacOS initializations (of the ROM) are done
static inline bool HasMacStarted(void)
{
	return ReadMacInt32(0xcfc) == FOURCC('W','L','S','C');	// Mac warm start flag
}

#endif
