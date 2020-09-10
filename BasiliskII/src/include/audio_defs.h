/*
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

#include "macos_util.h"


// Error codes
enum {
	badComponentSelector = (int32)0x80008002,
	noHardwareErr = -200,
	badChannel = -205,
	siInvalidSampleRate	= -225,
	siInvalidSampleSize	= -226,
	siDeviceBusyErr = -227
};

// General component dispatch selector codes
enum {
	kComponentOpenSelect = -1,
	kComponentCloseSelect = -2,
	kComponentCanDoSelect = -3,
	kComponentVersionSelect = -4,
	kComponentRegisterSelect = -5
};

// Sound component dispatch selector codes
enum {
	kSoundComponentInitOutputDeviceSelect = 1,
	kSoundComponentSetSourceSelect = 2,
	kSoundComponentGetSourceSelect = 3,
	kSoundComponentGetSourceDataSelect = 4,
	kSoundComponentSetOutputSelect = 5,
	kDelegatedSoundComponentSelectors = 0x0100,
	kSoundComponentAddSourceSelect = kDelegatedSoundComponentSelectors + 1,
	kSoundComponentRemoveSourceSelect = kDelegatedSoundComponentSelectors + 2,
	kSoundComponentGetInfoSelect = kDelegatedSoundComponentSelectors + 3,
	kSoundComponentSetInfoSelect = kDelegatedSoundComponentSelectors + 4,
	kSoundComponentStartSourceSelect = kDelegatedSoundComponentSelectors + 5,
	kSoundComponentStopSourceSelect = kDelegatedSoundComponentSelectors + 6,
	kSoundComponentPauseSourceSelect = kDelegatedSoundComponentSelectors + 7,
	kSoundComponentPlaySourceBufferSelect = kDelegatedSoundComponentSelectors + 8
};

// Sound information selectors
const uint32 siNumberChannels		= FOURCC('c','h','a','n');	// current number of channels
const uint32 siChannelAvailable		= FOURCC('c','h','a','v');	// number of channels available
const uint32 siSampleRate			= FOURCC('s','r','a','t');	// current sample rate
const uint32 siSampleRateAvailable	= FOURCC('s','r','a','v');	// sample rates available
const uint32 siSampleSize			= FOURCC('s','s','i','z');	// current sample size
const uint32 siSampleSizeAvailable	= FOURCC('s','s','a','v');	// sample sizes available
const uint32 siHardwareMute			= FOURCC('h','m','u','t');	// mute state of all hardware
const uint32 siHardwareVolume		= FOURCC('h','v','o','l');	// volume level of all hardware
const uint32 siHardwareVolumeSteps	= FOURCC('h','s','t','p');	// number of volume steps for hardware
const uint32 siHardwareBusy			= FOURCC('h','w','b','s');	// sound hardware is in use
const uint32 siHardwareFormat       = FOURCC('h','w','f','m');  // hardware format
const uint32 siHeadphoneMute		= FOURCC('p','m','u','t');	// mute state of headphone
const uint32 siHeadphoneVolume		= FOURCC('p','v','o','l');	// volume level of headphone
const uint32 siHeadphoneVolumeSteps	= FOURCC('h','d','s','t');	// number of volume steps for headphone
const uint32 siSpeakerMute			= FOURCC('s','m','u','t');	// mute state of all built-in speakers
const uint32 siSpeakerVolume		= FOURCC('s','v','o','l');	// volume level of built-in speaker
const uint32 siDeviceName			= FOURCC('n','a','m','e');  // sound input name
const uint32 siDeviceIcon			= FOURCC('i','c','o','n');  // sound input icon resource location
const uint32 siInputSourceNames     = FOURCC('s','n','a','m');  // sound input source names
const uint32 siInputSource          = FOURCC('s','o','u','r');  // sound input source selector
const uint32 siInputGain            = FOURCC('g','a','i','n');  // sound input gain
const uint32 siOptionsDialog        = FOURCC('o','p','t','d');  // display options dialog box
const uint32 siPlayThruOnOff        = FOURCC('p','l','t','h');  // play-through state
const uint32 siInitializeDriver     = FOURCC('i','n','i','t');  // open sound input device
const uint32 siCloseDriver          = FOURCC('c','l','o','s');  // close sound input device

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

// Component feature flags
enum {
	k8BitRawIn				= (1 << 0),
	k8BitTwosIn				= (1 << 1),
	k16BitIn				= (1 << 2),
	kStereoIn				= (1 << 3),
	k8BitRawOut				= (1 << 8),
	k8BitTwosOut			= (1 << 9),
	k16BitOut				= (1 << 10),
	kStereoOut				= (1 << 11),
	kReverse				= (1L << 16),
	kRateConvert			= (1L << 17),
	kCreateSoundSource		= (1L << 18),
	kHighQuality			= (1L << 22),
	kNonRealTime			= (1L << 23),
	cmpWantsRegisterMessage	= (1L << 31)
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
