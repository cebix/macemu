/*
 *  extfs_defs.h - MacOS types and structures for external file system
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

#ifndef EXTFS_DEFS_H
#define EXTFS_DEFS_H

#include "macos_util.h"

// Gestalt selectors
enum {
	gestaltFSAttr				= FOURCC('f','s',' ',' '),
	gestaltFullExtFSDispatching	= 0,
	gestaltHasFSSpecCalls		= 1,
	gestaltHasFileSystemManager	= 2,
	gestaltFSMDoesDynamicLoad	= 3,
	gestaltFSSupports4GBVols	= 4,
	gestaltFSSupports2TBVols	= 5,
	gestaltHasExtendedDiskInit	= 6,
	gestaltDTMgrSupportsFSM		= 7
};

enum {
	gestaltFSMVersion			= FOURCC('f','s','m',' ')
};

// File attributes
enum {
	faLocked	= 0x01,
	faRFOpen	= 0x04,
	faDFOpen	= 0x08,
	faIsDir		= 0x10,
	faOpen		= 0x80
};

// Volume attributes
enum {
	vaBusy = 0x40,
	vaHardLock = 0x80,
	vaSoftLock = 0x8000
};

// vMAttrib (GetVolParms) constants
enum {
	kLimitFCBs					= 1 << 31,
	kLocalWList					= 1 << 30,
	kNoMiniFndr					= 1 << 29,
	kNoVNEdit					= 1 << 28,
	kNoLclSync					= 1 << 27,
	kTrshOffLine				= 1 << 26,
	kNoSwitchTo					= 1 << 25,
	kNoDeskItems				= 1 << 20,
	kNoBootBlks					= 1 << 19,
	kAccessCntl					= 1 << 18,
	kNoSysDir					= 1 << 17,
	kHasExtFSVol				= 1 << 16,
	kHasOpenDeny				= 1 << 15,
	kHasCopyFile				= 1 << 14,
	kHasMoveRename				= 1 << 13,
	kHasDesktopMgr				= 1 << 12,
	kHasShortName				= 1 << 11,
	kHasFolderLock				= 1 << 10,
	kHasPersonalAccessPrivileges = 1 << 9,
	kHasUserGroupList			= 1 << 8,
	kHasCatSearch				= 1 << 7,
	kHasFileIDs					= 1 << 6,
	kHasBTreeMgr				= 1 << 5,
	kHasBlankAccessPrivileges	= 1 << 4,
	kSupportsAsyncRequests		= 1 << 3
};

enum {
	fsUsrCNID					= 16,
	kHFSBit						= 9,
	kHFSMask					= 0x0200,
	kAsyncBit					= 10,
	kAsyncMask					= 0x0400
};

// HFSCIProc selectCode values
enum {
	kFSMOpen					= 0xA000,
	kFSMClose					= 0xA001,
	kFSMRead					= 0xA002,
	kFSMWrite					= 0xA003,
	kFSMGetVolInfo				= 0xA007,
	kFSMCreate					= 0xA008,
	kFSMDelete					= 0xA009,
	kFSMOpenRF					= 0xA00A,
	kFSMRename					= 0xA00B,
	kFSMGetFileInfo				= 0xA00C,
	kFSMSetFileInfo				= 0xA00D,
	kFSMUnmountVol				= 0xA00E,
	kFSMMountVol				= 0xA00F,
	kFSMAllocate				= 0xA010,
	kFSMGetEOF					= 0xA011,
	kFSMSetEOF					= 0xA012,
	kFSMFlushVol				= 0xA013,
	kFSMGetVol					= 0xA014,
	kFSMSetVol					= 0xA015,
	kFSMEject					= 0xA017,
	kFSMGetFPos					= 0xA018,
	kFSMOffline					= 0xA035,
	kFSMSetFilLock				= 0xA041,
	kFSMRstFilLock				= 0xA042,
	kFSMSetFilType				= 0xA043,
	kFSMSetFPos					= 0xA044,
	kFSMFlushFile				= 0xA045,
	kFSMOpenWD					= 0x0001,
	kFSMCloseWD					= 0x0002,
	kFSMCatMove					= 0x0005,
	kFSMDirCreate				= 0x0006,
	kFSMGetWDInfo				= 0x0007,
	kFSMGetFCBInfo				= 0x0008,
	kFSMGetCatInfo				= 0x0009,
	kFSMSetCatInfo				= 0x000A,
	kFSMSetVolInfo				= 0x000B,
	kFSMLockRng					= 0x0010,
	kFSMUnlockRng				= 0x0011,
	kFSMXGetVolInfo				= 0x0012,
	kFSMCreateFileIDRef			= 0x0014,
	kFSMDeleteFileIDRef			= 0x0015,
	kFSMResolveFileIDRef		= 0x0016,
	kFSMExchangeFiles			= 0x0017,
	kFSMCatSearch				= 0x0018,
	kFSMOpenDF					= 0x001A,
	kFSMMakeFSSpec				= 0x001B,
	kFSMDTGetPath				= 0x0020,
	kFSMDTCloseDown				= 0x0021,
	kFSMDTAddIcon				= 0x0022,
	kFSMDTGetIcon				= 0x0023,
	kFSMDTGetIconInfo			= 0x0024,
	kFSMDTAddAPPL				= 0x0025,
	kFSMDTRemoveAPPL			= 0x0026,
	kFSMDTGetAPPL				= 0x0027,
	kFSMDTSetComment			= 0x0028,
	kFSMDTRemoveComment			= 0x0029,
	kFSMDTGetComment			= 0x002A,
	kFSMDTFlush					= 0x002B,
	kFSMDTReset					= 0x002C,
	kFSMDTGetInfo				= 0x002D,
	kFSMDTOpenInform			= 0x002E,
	kFSMDTDelete				= 0x002F,
	kFSMGetVolParms				= 0x0030,
	kFSMGetLogInInfo			= 0x0031,
	kFSMGetDirAccess			= 0x0032,
	kFSMSetDirAccess			= 0x0033,
	kFSMMapID					= 0x0034,
	kFSMMapName					= 0x0035,
	kFSMCopyFile				= 0x0036,
	kFSMMoveRename				= 0x0037,
	kFSMOpenDeny				= 0x0038,
	kFSMOpenRFDeny				= 0x0039,
	kFSMGetXCatInfo				= 0x003A,
	kFSMGetVolMountInfoSize		= 0x003F,
	kFSMGetVolMountInfo			= 0x0040,
	kFSMVolumeMount				= 0x0041,
	kFSMShare					= 0x0042,
	kFSMUnShare					= 0x0043,
	kFSMGetUGEntry				= 0x0044,
	kFSMGetForeignPrivs			= 0x0060,
	kFSMSetForeignPrivs			= 0x0061
};

// UTDetermineVol status values
enum {
	dtmvError					= 0,
	dtmvFullPathname			= 1,
	dtmvVRefNum					= 2,
	dtmvWDRefNum				= 3,
	dtmvDriveNum				= 4,
	dtmvDefault					= 5	
};

// Miscellaneous constants used by FSM
enum {
	fsdVersion1					= 1,
	fsmIgnoreFSID				= 0xFFFE,
	fsmGenericFSID				= 0xFFFF
};

// compInterfMask bits common to all FSM components
enum {
	fsmComponentEnableBit		= 31,
	fsmComponentEnableMask		= (long)0x80000000,
	fsmComponentBusyBit			= 30,
	fsmComponentBusyMask		= 0x40000000
};

// compInterfMask bits specific to HFS component
enum {
	hfsCIDoesHFSBit				= 23,
	hfsCIDoesHFSMask			= 0x00800000,
	hfsCIDoesAppleShareBit		= 22,
	hfsCIDoesAppleShareMask		= 0x00400000,
	hfsCIDoesDeskTopBit			= 21,
	hfsCIDoesDeskTopMask		= 0x00200000,
	hfsCIDoesDynamicLoadBit		= 20,
	hfsCIDoesDynamicLoadMask	= 0x00100000,
	hfsCIResourceLoadedBit		= 19,
	hfsCIResourceLoadedMask		= 0x00080000,
	hfsCIHasHLL2PProcBit		= 18,
	hfsCIHasHLL2PProcMask		= 0x00040000,
	hfsCIWantsDTSupportBit		= 17,
	hfsCIWantsDTSupportMask		= 0x00020000
};

// FCBRec.fcbFlags bits
enum {
	fcbWriteBit					= 0,
	fcbWriteMask				= 0x01,
	fcbResourceBit				= 1,
	fcbResourceMask				= 0x02,
	fcbWriteLockedBit			= 2,
	fcbWriteLockedMask			= 0x04,
	fcbSharedWriteBit			= 4,
	fcbSharedWriteMask			= 0x10,
	fcbFileLockedBit			= 5,
	fcbFileLockedMask			= 0x20,
	fcbOwnClumpBit				= 6,
	fcbOwnClumpMask				= 0x40,
	fcbModifiedBit				= 7,
	fcbModifiedMask				= 0x80
};

// InformFSM messages
enum {
	fsmNopMessage				= 0,
	fsmDrvQElChangedMessage		= 1,
	fsmGetFSIconMessage			= 2
};

// Messages passed to the fileSystemCommProc
enum {
	ffsNopMessage				= 0,
	ffsGetIconMessage			= 1,
	ffsIDDiskMessage			= 2,
	ffsLoadMessage				= 3,
	ffsUnloadMessage			= 4,
	ffsIDVolMountMessage		= 5,
	ffsInformMessage			= 6,
	ffsGetIconInfoMessage		= 7
};

// Error codes from FSM functions
enum {
	fsmFFSNotFoundErr			= -431,
	fsmBusyFFSErr				= -432,
	fsmBadFFSNameErr			= -433,
	fsmBadFSDLenErr				= -434,
	fsmDuplicateFSIDErr			= -435,
	fsmBadFSDVersionErr			= -436,
	fsmNoAlternateStackErr		= -437,
	fsmUnknownFSMMessageErr		= -438
};

// paramBlock for ffsGetIconMessage and fsmGetFSIconMessage
enum {
	kLargeIcon = 1
};

enum {	// FSMGetIconRec struct
	iconBufferPtr = 2,
	requestSize = 6,
	actualSize = 10,
	iconType = 14,
	isEjectable = 15,
	driveQElemPtr = 16,
	fileSystemSpecPtr = 20
};

enum {	// VolumeMountInfoHeader struct
	vmiLength = 0,
	vmiMedia = 2,
	vmiFlags = 6,
	SIZEOF_VolumeMountInfoHeader = 8
};

enum {	// GetVolParmsInfoBuffer struct
	vMVersion = 0,
	vMAttrib = 2,
	vMLocalHand = 6,
	vMServerAdr = 10,
	vMVolumeGrade = 14,
	vMForeignPrivID = 18,
	SIZEOF_GetVolParmsInfoBuffer = 20
};

// Finder Flags
enum {
	kIsOnDesk					= 0x0001,
	kColor						= 0x000E,
	kIsShared					= 0x0040,
	kHasBeenInited				= 0x0100,
	kHasCustomIcon				= 0x0400,
	kIsStationery				= 0x0800,
	kNameLocked					= 0x1000,
	kHasBundle					= 0x2000,
	kIsInvisible				= 0x4000,
	kIsAlias					= 0x8000
};

enum {	// FInfo struct
	fdType = 0,
	fdCreator = 4,
	fdFlags = 8,
	fdLocation = 10,
	fdFldr = 14,
	SIZEOF_FInfo = 16
};

enum {	// FXInfo struct
	fdIconID = 0,
	fdUnused = 2,
	fdScript = 8,
	fdXFlags = 9,
	fdComment = 10,
	fdPutAway = 12,
	SIZEOF_FXInfo = 16
};

enum {	// HFileParam/HFileInfo struct
	ioFRefNum = 24,
	ioFVersNum = 26,
	ioFDirIndex = 28,
	ioFlAttrib = 30,
	ioACUser = 31,
	ioFlFndrInfo = 32,
	ioDirID = 48,
	ioFlStBlk = 52,
	ioFlLgLen = 54,
	ioFlPyLen = 58,
	ioFlRStBlk = 62,
	ioFlRLgLen = 64,
	ioFlRPyLen = 68,
	ioFlCrDat = 72,
	ioFlMdDat = 76,
	ioFlBkDat = 80,
	ioFlXFndrInfo = 84,
	ioFlParID = 100,
	ioFlClpSiz = 104
};

enum {	// DInfo struct
	frRect = 0,
	frFlags = 8,
	frLocation = 10,
	frView = 14,
	SIZEOF_DInfo = 16
};

enum {	// DXInfo struct
	frScroll = 0,
	frOpenChain = 4,
	frScript = 8,
	frXFlags = 9,
	frComment = 10,
	frPutAway = 12,
	SIZEOF_DXInfo = 16
};

enum {	// HDirParam/DirInfo struct
	ioDrUsrWds = 32,
	ioDrDirID = 48,
	ioDrNmFls = 52,
	ioDrCrDat = 72,
	ioDrMdDat = 76,
	ioDrBkDat = 80,
	ioDrFndrInfo = 84,
	ioDrParID = 100
};

enum {	// WDParam struct
	ioWDIndex = 26,
	ioWDProcID = 28,
	ioWDVRefNum = 32,
	ioWDDirID = 48,
	SIZEOF_WDParam = 52
};

enum {	// HVolumeParam struct
	ioVolIndex = 28,
	ioVCrDate = 30,
	ioVLsMod = 34,
	ioVAtrb = 38,
	ioVNmFls = 40,
	ioVBitMap = 42,
	ioAllocPtr = 44,
	ioVNmAlBlks = 46,
	ioVAlBlkSiz = 48,
	ioVClpSiz = 52,
	ioAlBlSt = 56,
	ioVNxtCNID = 58,
	ioVFrBlk = 62,
	ioVSigWord = 64,
	ioVDrvInfo = 66,
	ioVDRefNum = 68,
	ioVFSID = 70,
	ioVBkUp = 72,
	ioVSeqNum = 76,
	ioVWrCnt = 78,
	ioVFilCnt = 82,
	ioVDirCnt = 86,
	ioVFndrInfo = 90
};

enum {	// CMovePBRec struct
	ioNewName = 28,
	ioNewDirID = 36
};

enum {	// FCBPBRec struct
	ioFCBIndx = 28,
	ioFCBFlNm = 32,
	ioFCBFlags = 36,
	ioFCBStBlk = 38,
	ioFCBEOF = 40,
	ioFCBPLen = 44,
	ioFCBCrPs = 48,
	ioFCBVRefNum = 52,
	ioFCBClpSiz = 54,
	ioFCBParID = 58
};

// Volume control block
enum {	// VCB struct
	vcbFlags = 6,
	vcbSigWord = 8,
	vcbCrDate = 10,
	vcbLsMod = 14,
	vcbAtrb = 18,
	vcbNmFls = 20,
	vcbVBMSt = 22,
	vcbAllocPtr = 24,
	vcbNmAlBlks = 26,
	vcbAlBlkSiz = 28,
	vcbClpSiz = 32,
	vcbAlBlSt = 36,
	vcbNxtCNID = 38,
	vcbFreeBks = 42,
	vcbVN = 44,
	vcbDrvNum = 72,
	vcbDRefNum = 74,
	vcbFSID = 76,
	vcbVRefNum = 78,
	vcbMAdr = 80,
	vcbBufAdr = 84,
	vcbMLen = 88,
	vcbDirIndex = 90,
	vcbDirBlk = 92,
	vcbVolBkUp = 94,
	vcbVSeqNum = 98,
	vcbWrCnt = 100,
	vcbXTClpSiz = 104,
	vcbCTClpSiz = 108,
	vcbNmRtDirs = 112,
	vcbFilCnt = 114,
	vcbDirCnt = 118,
	vcbFndrInfo = 122,
	vcbVCSize = 154,
	vcbVBMCSiz = 156,
	vcbCtlCSiz = 158,
	vcbXTAlBlks = 160,
	vcbCTAlBlks = 162,
	vcbXTRef = 164,
	vcbCTRef = 166,
	vcbCtlBuf = 168,
	vcbDirIDM = 172,
	vcbOffsM = 176,
	SIZEOF_VCB = 178
};

// Working directory control block
enum {	// WDCBRec struct
	wdVCBPtr = 0,
	wdDirID = 4,
	wdCatHint = 8,
	wdProcID = 12,
	SIZEOF_WDCBRec = 16
};

// File control block
enum {	// FCBRec struct
	fcbFlNm = 0,
	fcbFlags = 4,
	fcbTypByt = 5,
	fcbSBlk = 6,
	fcbEOF = 8,
	fcbPLen = 12,
	fcbCrPs = 16,
	fcbVPtr = 20,
	fcbBfAdr = 24,
	fcbFlPos = 28,
	fcbClmpSize = 30,
	fcbBTCBPtr = 34,
	fcbExtRec = 38,
	fcbFType = 50,
	fcbCatPos = 54,
	fcbDirID = 58,
	fcbCName = 62
};

enum {	// ParsePathRec struct
	ppNamePtr = 0,
	ppStartOffset = 4,
	ppComponentLength = 6,
	ppMoreName = 8,
	ppFoundDelimiter = 9,
	SIZEOF_ParsePathRec = 10
};

enum {	// HFSCIRec struct
	compInterfMask = 0,
	compInterfProc = 4,
	log2PhyProc = 8,
	stackTop = 12,
	stackSize = 16,
	stackPtr = 20,
	idSector = 28,
	SIZEOF_HFSCIRec = 40
};

enum {	// DICIRec struct
	maxVolNameLength = 8,
	blockSize = 10,
	SIZEOF_DICIRec = 24
};

enum {	// FSDRec struct
	fsdLink = 0,
	fsdLength = 4,
	fsdVersion = 6,
	fileSystemFSID = 8,
	fileSystemName = 10,
	fileSystemSpec = 42,
	fileSystemGlobalsPtr = 112,
	fileSystemCommProc = 116,
	fsdHFSCI = 132,
	fsdDICI = 172,
	SIZEOF_FSDRec = 196
};

#endif
