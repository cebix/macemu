/*
 *  video_defs.h - MacOS types and structures for video
 *
 *  SheepShaver (C) 1997-2005 Marc Hellwig and Christian Bauer
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

#include "macos_util.h"
#include <stddef.h>


/* 
 * Definitions for Display Manager
 */
 
/* csMode values describing pixel depth in VDSwitchInfo */
enum {
	firstVidMode=128,						// first depth mode, representing lowest supported
											// pixel depth
	secondVidMode, thirdVidMode, fourthVidMode, fifthVidMode, sixthVidMode
											// following modes represent pixel depths in ascending
											// order
};

/* csDisplayType values in VDDisplayConnectInfoRec */
enum {
	kUnknownConnect=1,						// reserved
	kPanelTFTConnect,						// fixed-in-place LCS (TFT, aka "active matrix") panels
	kFixedModeCRTConnect,					// very limited displays
	kMultiModeCRT1Connect,					// 12" optional, 13" default, 16" required
	kMultiModeCRT2Connect,					// 12" optional, 13" req., 16" def., 19" req.
	kMultiModeCRT3Connect,					// 12" optional, 13" req., 16" req., 19" req.,21" def. 
	kMultiModeCRT4Connect,					// expansion to large multimode (not yet implemented)
	kModelessConnect,						// expansion to modeless model (not yet implemented)
	kFullPageConnect,						// 640x818 (to get 8bpp in 512K case) and
											// 640x870 (nothing else supported)
	kVGAConnect,							// 640x480 VGA default -- nothing else req.
	kNTSCConnect,							// NTSC ST(default), FF, STconv, FFconv
	kPALConnect,							// PAL ST(default), FF, STconv, FFconv
	kHRConnect,								// 640x400 (to get 8bpp in 256K case) and
											// 640x480 (nothing else supported)
	kPanelFSTNConnect						// fixed-in-place LCD FSTN (aka "supertwist") panels
};

/* csConnectFlags values in VDDisplayConnectInfoRec */
enum {
	kAllModesValid=0,						// all display modes not deleted by PrimaryInit code 
											// are optional
	kAllModesSafe,							// all display modes not deleted by PrimaryInit code
											// are required; is you set this bit, set the
											// kAllModesValid bit, too
	kHasDirectConnect=3,					// for future expansions, setting this bit means that
											// your driver can talk directly to the display
											// (e.g. there is a serial data link via sense lines)
	kIsMonoDev,								// this display does not support color
	kUncertainConnect						// there may not be a display; Monitors control panel
											// makes the user confirm some operations--like moving
											// the menu bar-- when this bit is set
};

/* csTimingFormat value in VDTimingInfoRec */
#define kDeclROMtables FOURCC('d','e','c','l')	// use information in this record instead of looking
												// in the decl. ROM for timing info; used for patching
												// existing card without updating declaration ROM
										
/* csTimingData values in VDTimingInfoRec */
enum {
	timingUnknown = 0,						// unknown timing
	timingApple_512x384_60hz	= 130,							/*  512x384  (60 Hz) Rubik timing. */
	timingApple_560x384_60hz	= 135,							/*  560x384  (60 Hz) Rubik-560 timing. */
	timingApple_640x480_67hz	= 140,							/*  640x480  (67 Hz) HR timing. */
	timingApple_640x400_67hz	= 145,							/*  640x400  (67 Hz) HR-400 timing. */
	timingVESA_640x480_60hz		= 150,							/*  640x480  (60 Hz) VGA timing. */
	timingVESA_640x480_72hz		= 152,							/*  640x480  (72 Hz) VGA timing. */
	timingVESA_640x480_75hz		= 154,							/*  640x480  (75 Hz) VGA timing. */
	timingVESA_640x480_85hz		= 158,							/*  640x480  (85 Hz) VGA timing. */
	timingGTF_640x480_120hz		= 159,							/*  640x480  (120 Hz) VESA Generalized Timing Formula */
	timingApple_640x870_75hz	= 160,							/*  640x870  (75 Hz) FPD timing.*/
	timingApple_640x818_75hz	= 165,							/*  640x818  (75 Hz) FPD-818 timing.*/
	timingApple_832x624_75hz	= 170,							/*  832x624  (75 Hz) GoldFish timing.*/
	timingVESA_800x600_56hz		= 180,							/*  800x600  (56 Hz) SVGA timing. */
	timingVESA_800x600_60hz		= 182,							/*  800x600  (60 Hz) SVGA timing. */
	timingVESA_800x600_72hz		= 184,							/*  800x600  (72 Hz) SVGA timing. */
	timingVESA_800x600_75hz		= 186,							/*  800x600  (75 Hz) SVGA timing. */
	timingVESA_800x600_85hz		= 188,							/*  800x600  (85 Hz) SVGA timing. */
	timingVESA_1024x768_60hz	= 190,							/* 1024x768  (60 Hz) VESA 1K-60Hz timing. */
	timingVESA_1024x768_70hz	= 200,							/* 1024x768  (70 Hz) VESA 1K-70Hz timing. */
	timingVESA_1024x768_75hz	= 204,							/* 1024x768  (75 Hz) VESA 1K-75Hz timing (very similar to timingApple_1024x768_75hz). */
	timingVESA_1024x768_85hz	= 208,							/* 1024x768  (85 Hz) VESA timing. */
	timingApple_1024x768_75hz	= 210,							/* 1024x768  (75 Hz) Apple 19" RGB. */
	timingApple_1152x870_75hz	= 220,							/* 1152x870  (75 Hz) Apple 21" RGB. */
	timingVESA_1280x960_75hz	= 250,							/* 1280x960  (75 Hz) */
	timingVESA_1280x960_60hz	= 252,							/* 1280x960  (60 Hz) */
	timingVESA_1280x960_85hz	= 254,							/* 1280x960  (85 Hz) */
	timingVESA_1280x1024_60hz	= 260,							/* 1280x1024 (60 Hz) */
	timingVESA_1280x1024_75hz	= 262,							/* 1280x1024 (75 Hz) */
	timingVESA_1280x1024_85hz	= 268,							/* 1280x1024 (85 Hz) */
	timingVESA_1600x1200_60hz	= 280,							/* 1600x1200 (60 Hz) VESA proposed timing. */
	timingVESA_1600x1200_65hz	= 282,							/* 1600x1200 (65 Hz) VESA proposed timing. */
	timingVESA_1600x1200_70hz	= 284,							/* 1600x1200 (70 Hz) VESA proposed timing. */
	timingVESA_1600x1200_75hz	= 286,							/* 1600x1200 (75 Hz) VESA proposed timing. */
	timingVESA_1600x1200_80hz	= 288,							/* 1600x1200 (80 Hz) VESA proposed timing (pixel clock is 216 Mhz dot clock). */
	timingSMPTE240M_60hz		= 400,							/* 60Hz V, 33.75KHz H, interlaced timing, 16:9 aspect, typical resolution of 1920x1035. */
	timingFilmRate_48hz			= 410							/* 48Hz V, 25.20KHz H, non-interlaced timing, typical resolution of 640x480. */
};

/* csTimingFlags values in VDTimingInfoRec */
enum {
	kModeValid=0,							// this display mode is optional
	kModeSafe,								// this display mode is required; if you set this
											// bit, you should also set the kModeValid bit 
	kModeDefault,							// this display mode is the default for the attached 
											// display; if you set this bit, you should also set
											// the kModeSafe and kModeValid bits
	kShowModeNow,							// show this mode in Monitors control panel; useful
											// for SVGA modes
	kModeNotResize,
	kModeRequiresPan
};

/* code for Display Manager control request */
enum {
	cscReset=0,
	cscKillIO,
	cscSetMode,
	cscSetEntries,
	cscSetGamma,
	cscGrayPage,
	cscGrayScreen=5,
	cscSetGray,
	cscSetInterrupt,
	cscDirectSetEntries,
	cscSetDefaultMode,
	cscSwitchMode,						// switch to another display mode
	cscSetSync,
	cscSavePreferredConfiguration=16,
	cscSetHardwareCursor=22,
	cscDrawHardwareCursor,
	cscSetConvolution,
	cscSetPowerState,
	cscPrivateControlCall,				// Takes a VDPrivateSelectorDataRec
	cscSetMultiConnect,					// From a GDI point of view, this call should be implemented completely in the HAL and not at all in the core.
	cscSetClutBehavior,					// Takes a VDClutBehavior 
	cscUnusedCall=127					// This call used to expend the scrn resource.  Its imbedded data contains more control info 
};

/* Constants for the GetNextResolution call */

enum {
	kDisplayModeIDCurrent		= 0x00,							/* Reference the Current DisplayModeID */
	kDisplayModeIDInvalid		= (long)0xFFFFFFFF,				/* A bogus DisplayModeID in all cases */
	kDisplayModeIDFindFirstResolution = (long)0xFFFFFFFE,		/* Used in cscGetNextResolution to reset iterator */
	kDisplayModeIDNoMoreResolutions = (long)0xFFFFFFFD			/* Used in cscGetNextResolution to indicate End Of List */
};

/* codes for Display Manager status requests */
enum {
	cscGetMode=2,
	cscGetEntries,
	cscGetPageCnt,
	cscGetPages=4,					// This is what C&D 2 calls it. 
	cscGetPageBase,
	cscGetBaseAddr=5,				// This is what C&D 2 calls it. 
	cscGetGray,
	cscGetInterrupt,
	cscGetGamma,
	cscGetDefaultMode,
	cscGetCurMode,					// save the current display mode
	cscGetSync,
	cscGetConnection,				// return information about display capabilities of
									// connected display
	cscGetModeTiming,				// return scan timings data for a display mode
	cscGetModeBaseAddress,			// Return base address information about a particular mode 
	cscGetScanProc,					// QuickTime scan chasing routine 
	cscGetPreferredConfiguration,
	cscGetNextResolution,
	cscGetVideoParameters,
	cscGetGammaInfoList	=20,
	cscRetrieveGammaTable,
	cscSupportsHardwareCursor,
	cscGetHardwareCursorDrawState,
	cscGetConvolution,
	cscGetPowerState,
	cscPrivateStatusCall,			// Takes a VDPrivateSelectorDataRec
	cscGetDDCBlock,					// Takes a VDDDCBlockRec  
	cscGetMultiConnect,				// From a GDI point of view, this call should be implemented completely in the HAL and not at all in the core.
	cscGetClutBehavior				// Takes a VDClutBehavior 
};	

enum {	// VDSwitchInfo struct
	csMode = 0,
	csData = 2,
	csPage = 6,
	csBaseAddr = 8
};

enum {	// VDSetEntry struct
	csTable = 0,	// Pointer to ColorSpec[]
	csStart = 4,
	csCount = 6
};

enum {	// VDGammaRecord
	csGTable = 0
};

enum {	// ColorSpec table entry
	csValue = 0,
	csRed = 2,
	csGreen = 4,
	csBlue = 6
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

enum {	// VDDrawHardwareCursor/VDHardwareCursorDrawState struct
	csCursorX = 0,
	csCursorY = 4,
	csCursorVisible = 8,
	csCursorSet = 12
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

enum {
	kCursorImageMajorVersion	= 0x0001,
	kCursorImageMinorVersion	= 0x0000
};

enum {	// CursorImage struct
	ciMajorVersion = 0,
	ciMinorVersion = 2,
	ciCursorPixMap = 4,	// Handle to PixMap
	ciCursorBitMask = 8	// Handle to BitMap
};


/*
 *  Structures for graphics acceleration
 */

typedef uint32 CTabHandle;

// Parameter block passed to acceleration hooks
struct accl_params {
	uint32 unknown0[3];

	uint32 transfer_mode;
	uint32 pen_mode;

	uint32 unknown1[2];

	uint32 fore_pen;
	uint32 back_pen;

	uint32 unknown2[3];

	uint32 src_base_addr;
	int32 src_row_bytes;
	int16 src_bounds[4];
	uint32 src_unknown1;
	uint32 src_pixel_type;
	uint32 src_pixel_size;
	uint32 src_cmp_count;
	uint32 src_cmp_size;
	CTabHandle src_pm_table;
	uint32 src_unknown2;
	uint32 src_unknown3;
	uint32 src_unknown4;

	uint32 dest_base_addr;
	int32 dest_row_bytes;
	int16 dest_bounds[4];
	uint32 dest_unknown1;
	uint32 dest_pixel_type;
	uint32 dest_pixel_size;
	uint32 dest_cmp_count;
	uint32 dest_cmp_size;
	CTabHandle dest_pm_table;
	uint32 dest_unknown2;
	uint32 dest_unknown3;
	uint32 dest_unknown4;

	uint32 unknown3[13];

	int16 src_rect[4];
	int16 dest_rect[4];

	uint32 unknown4[38];

	uint32 draw_proc;
	// Argument for accl_sync_hook at offset 0x4f8
};

enum {
	acclTransferMode	= offsetof(accl_params, transfer_mode),
	acclPenMode			= offsetof(accl_params, pen_mode),
	acclForePen			= offsetof(accl_params, fore_pen),
	acclBackPen			= offsetof(accl_params, back_pen),
	acclSrcBaseAddr		= offsetof(accl_params, src_base_addr),
	acclSrcRowBytes		= offsetof(accl_params, src_row_bytes),
	acclSrcBoundsRect	= offsetof(accl_params, src_bounds),
	acclSrcPixelType	= offsetof(accl_params, src_pixel_type),
	acclSrcPixelSize	= offsetof(accl_params, src_pixel_size),
	acclSrcCmpCount		= offsetof(accl_params, src_cmp_count),
	acclSrcCmpSize		= offsetof(accl_params, src_cmp_size),
	acclSrcPMTable		= offsetof(accl_params, src_pm_table),
	acclDestBaseAddr	= offsetof(accl_params, dest_base_addr),
	acclDestRowBytes	= offsetof(accl_params, dest_row_bytes),
	acclDestBoundsRect	= offsetof(accl_params, dest_bounds),
	acclDestPixelType	= offsetof(accl_params, dest_pixel_type),
	acclDestPixelSize	= offsetof(accl_params, dest_pixel_size),
	acclDestCmpCount	= offsetof(accl_params, dest_cmp_count),
	acclDestCmpSize		= offsetof(accl_params, dest_cmp_size),
	acclDestPMTable		= offsetof(accl_params, dest_pm_table),
	acclSrcRect			= offsetof(accl_params, src_rect),
	acclDestRect		= offsetof(accl_params, dest_rect),
	acclDrawProc		= offsetof(accl_params, draw_proc)
};

// Hook info for NQDMisc
struct accl_hook_info {
	uint32 draw_func;
	uint32 sync_func;
	uint32 code;
};

// Hook function index
enum {
	ACCL_BITBLT,
	ACCL_BLTMASK,
	ACCL_FILLRECT,
	ACCL_FILLMASK
	// 4: bitblt
	// 5: lines
	// 6: fill
};

#endif
