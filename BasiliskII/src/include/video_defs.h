/*
 *  video_defs.h - Definitions for MacOS video drivers
 *
 *  Basilisk II (C) 1997-2000 Christian Bauer
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
	cscReset						= 0,
	cscKillIO						= 1,
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
	cscSetClutBehavior				= 28,
	cscUnusedCall = 127
};

// Video driver status codes
enum {
	cscGetMode						= 2,
	cscGetEntries					= 3,
	cscGetPageCnt					= 4,
	cscGetPageBase					= 5,
	cscGetGray						= 6,
	cscGetInterrupt					= 7,
	cscGetGamma						= 8,
	cscGetDefaultMode				= 9,
	cscGetCurMode					= 10,
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

enum {	// VDPageInfo struct
	csPageMode = 0,
	csPageData = 2,
	csPagePage = 6,
	csPageBaseAddr = 8

};

#endif
