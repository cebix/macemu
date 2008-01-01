/*
 *	$Id$
 *
 *	audio_defs_macosx.h - Work around clashes with the enums in <CarbonCore/OSUtils.h>
 *						  Based on:
 *
 *  audio_defs.h - Definitions for MacOS audio components
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

#ifndef AUDIO_DEFS_H
#define AUDIO_DEFS_H

#include "macos_util_macosx.h"

enum {	// ComponentResource struct
	componentType = 0,
	componentSubType = 4,
	componentManufacturer = 8,
	componentFlags = 12,
	componentFlagsMask = 16,
	componentResType = 20,
	componentResID = 24,
	componentNameType = 26,
	componentNameID = 30,
	componentInfoType = 32,
	componentInfoID = 36,
	componentIconType = 38,
	componentIconID = 42,
	componentVersion = 44,
	componentRegisterFlags = 48,
	componentIconFamily = 52,
	componentPFCount = 54,
	componentPFFlags = 58,
	componentPFResType = 62,
	componentPFResID = 66,
	componentPFPlatform = 68
};


enum {	// ComponentParameters struct
	cp_flags = 0,		// call modifiers: sync/async, deferred, immed, etc
	cp_paramSize = 1,	// size in bytes of actual parameters passed to this call
	cp_what = 2,		// routine selector, negative for Component management calls
	cp_params = 4		// actual parameters for the indicated routine
};

enum {	// SoundComponentData struct
	scd_flags = 0,
	scd_format = 4,
	scd_numChannels = 8,
	scd_sampleSize = 10,
	scd_sampleRate = 12,
	scd_sampleCount = 16,
	scd_buffer = 20,
	scd_reserved = 24,
	SIZEOF_scd = 28
};

enum {	// SoundInfoList struct
	sil_count = 0,
	sil_infoHandle = 2
};

#endif
