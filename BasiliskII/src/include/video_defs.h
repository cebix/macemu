/*
 *  video_defs.h - Definitions for MacOS video drivers
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

#ifndef VIDEO_DEFS_H
#define VIDEO_DEFS_H

// Video driver control codes
enum {
	cscSetMode						= 2,
	cscSetEntries					= 3,
	cscSetGamma						= 4,
	cscGrayPage						= 5,
	cscSetGray						= 6,
	cscSetInterrupt					= 7,
	cscDirectSetEntries				= 8,
	cscSetDefaultMode				= 9,
	cscSwitchMode					= 10,
	cscSetSync						= 11,
	cscSavePreferredConfiguration	= 16,
	cscSetHardwareCursor			= 22,
	cscDrawHardwareCursor			= 23,
	cscSetConvolution				= 24,
	cscSetPowerState				= 25,
	cscPrivateControlCall			= 26,
	cscSetMultiConnect				= 27,
	cscSetClutBehavior				= 28
};

// Video driver status codes
enum {
	cscGetMode						= 2,
	cscGetEntries					= 3,
	cscGetPages						= 4,
	cscGetBaseAddress				= 5,
	cscGetGray						= 6,
	cscGetInterrupt					= 7,
	cscGetGamma						= 8,
	cscGetDefaultMode				= 9,
	cscGetCurrentMode				= 10,
	cscGetSync						= 11,
	cscGetConnection				= 12,
	cscGetModeTiming				= 13,
	cscGetModeBaseAddress			= 14,
	cscGetScanProc					= 15,
	cscGetPreferredConfiguration	= 16,
	cscGetNextResolution			= 17,
	cscGetVideoParameters			= 18,
	cscGetGammaInfoList				= 20,
	cscRetrieveGammaTable			= 21,
	cscSupportsHardwareCursor		= 22,
	cscGetHardwareCursorDrawState	= 23,
	cscGetConvolution				= 24,
	cscGetPowerState				= 25,
	cscPrivateStatusCall			= 26,
	cscGetDDCBlock					= 27,
	cscGetMultiConnect				= 28,
	cscGetClutBehavior				= 29
};	

enum {	// VDSwitchInfo struct
	csMode = 0,
	csData = 2,
	csPage = 6,
	csBaseAddr = 8,
	csReserved = 12
};

enum {	// VDSetEntry struct
	csTable = 0,
	csStart = 4,
	csCount = 6
};

enum {	// VDGammaRecord
	csGTable = 0
};

enum {	// VDDisplayConnectInfo struct
	csDisplayType = 0,
	csConnectTaggedType = 2,
	csConnectTaggedData = 3,
	csConnectFlags = 4,
	csDisplayComponent = 8,
	csConnectReserved = 12
};

enum {	// VDTimingInfo struct
	csTimingMode = 0,
	csTimingReserved = 4,
	csTimingFormat = 8,
	csTimingData = 12,
	csTimingFlags = 16
};

enum {	// VDResolutionInfo struct
	csPreviousDisplayModeID = 0,
	csRIDisplayModeID = 4,
	csHorizontalPixels = 8,
	csVerticalLines = 12,
	csRefreshRate = 16,
	csMaxDepthMode = 20,
	csResolutionFlags = 22
};

enum {	// VDVideoParametersInfo struct
	csDisplayModeID = 0,
	csDepthMode = 4,
	csVPBlockPtr = 6,
	csPageCount = 10,
	csDeviceType = 14
};

enum {	// VPBlock struct
	vpBaseOffset = 0,
	vpRowBytes = 4,
	vpBounds = 6,
	vpVersion = 14,
	vpPackType = 16,
	vpPackSize = 18,
	vpHRes = 22,
	vpVRes = 26,
	vpPixelType = 30,
	vpPixelSize = 32,
	vpCmpCount = 34,
	vpCmpSize = 36,
	vpPlaneBytes = 38
};

enum {	// SPBlock struct
	spResult = 0,
	spPointer = 4,
	spSize = 8,
	spOffsetData = 12,
	spIOFileName = 16,
	spExecPBlk = 20,
	spParamData = 24,
	spMisc = 28,
	spReserved = 32,
	spIOReserved = 36,
	spRefNum = 38,
	spCategory = 40,
	spCType = 42,
	spDrvrSW = 44,
	spDrvrHW = 46,
	spTBMask = 48,
	spSlot = 49,
	spID = 50,
	spExtDev = 51,
	spHwDev = 52,
	spByteLanes = 53,
	spFlags = 54,
	spKey = 55,
	SIZEOF_SPBlock = 56
};

enum {	// struct GammaTbl
	gVersion = 0,
	gType = 2,
	gFormulaSize = 4,
	gChanCnt = 6,
	gDataCnt = 8,
	gDataWidth = 10,
	gFormulaData = 12, // variable size
	SIZEOF_GammaTbl = 12
};

enum {	// struct VDMultiConnectInfo
	csDisplayCountOrNumber = 0,
	csConnectInfo = 4
};

#endif
