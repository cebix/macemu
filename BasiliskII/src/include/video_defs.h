/*
 *  video_defs.h - Definitions for MacOS video drivers
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

#ifndef VIDEO_DEFS_H
#define VIDEO_DEFS_H

// Video driver control codes
enum {
	cscReset = 0,
	cscKillIO,
	cscSetMode,
	cscSetEntries,
	cscSetGamma,
	cscGrayPage,
	cscSetGray,
	cscSetInterrupt,
	cscDirectSetEntries,
	cscSetDefaultMode,
	cscSwitchMode,
	cscSetSync,
	cscSavePreferredConfiguration = 16,
	cscSetHardwareCursor = 22,
	cscDrawHardwareCursor,
	cscSetConvolution,
	cscSetPowerState,
	cscPrivateControlCall,
	cscSetMultiConnect,
	cscSetClutBehavior,
	cscUnusedCall = 127
};

// Video driver status codes
enum {
	cscGetMode = 2,
	cscGetEntries,
	cscGetPageCnt,
	cscGetPageBase,
	cscGetGray,
	cscGetInterrupt,
	cscGetGamma,
	cscGetDefaultMode,
	cscGetCurMode,
	cscGetSync,
	cscGetConnection,
	cscGetModeTiming,
	cscGetModeBaseAddress,
	cscGetScanProc,
	cscGetPreferredConfiguration,
	cscGetNextResolution,
	cscGetVideoParameters,
	cscGetGammaInfoList	= 20,
	cscRetrieveGammaTable,
	cscSupportsHardwareCursor,
	cscGetHardwareCursorDrawState,
	cscGetConvolution,
	cscGetPowerState,
	cscPrivateStatusCall,
	cscGetDDCBlock,
	cscGetMultiConnect,
	cscGetClutBehavior
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

#endif
