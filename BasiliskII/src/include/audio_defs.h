/*
 *  audio_defs.h - Definitions for MacOS audio components
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

#ifndef AUDIO_DEFS_H
#define AUDIO_DEFS_H

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
enum {
	siNumberChannels			= 'chan',		// current number of channels
	siChannelAvailable			= 'chav',		// number of channels available
	siSampleRate				= 'srat',		// current sample rate
	siSampleRateAvailable		= 'srav',		// sample rates available
	siSampleSize				= 'ssiz',		// current sample size
	siSampleSizeAvailable		= 'ssav',		// sample sizes available
	siHardwareMute				= 'hmut',		// mute state of all hardware
	siHardwareVolume			= 'hvol',		// volume level of all hardware
	siHardwareVolumeSteps		= 'hstp',		// number of volume steps for hardware
	siHardwareBusy				= 'hwbs',		// sound hardware is in use
	siHeadphoneMute				= 'pmut',		// mute state of headphone
	siHeadphoneVolume			= 'pvol',		// volume level of headphone
	siHeadphoneVolumeSteps		= 'hdst',		// number of volume steps for headphone
	siSpeakerMute				= 'smut',		// mute state of all built-in speakers
	siSpeakerVolume				= 'svol'		// volume level of built-in speaker
};

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
	componentIconFamily = 52
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
