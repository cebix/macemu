/*
 *  audio.cpp - Audio support
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *  Portions written by Marc Hellwig
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Sound, chapter 5 "Sound Components"
 */

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "macos_util.h"
#include "emul_op.h"
#include "main.h"
#include "audio.h"
#include "audio_defs.h"

#define DEBUG 0
#include "debug.h"


// Supported sample rates, sizes and channels
vector<uint32> audio_sample_rates;
vector<uint16> audio_sample_sizes;
vector<uint8> audio_channel_counts;

// Global variables
struct audio_status AudioStatus;	// Current audio status (sample rate etc.)
bool audio_open = false;			// Flag: audio is initialized and ready
int audio_frames_per_block;			// Number of audio frames per block
uint32 audio_component_flags;		// Component feature flags
uint32 audio_data = 0;				// Mac address of global data area
static int open_count = 0;			// Open/close nesting count

bool AudioAvailable = false;		// Flag: audio output available (from the software point of view)


/*
 *  Reset audio emulation
 */

void AudioReset(void)
{
	audio_data = 0;
}


/*
 *  Get audio info
 */

static int32 AudioGetInfo(uint32 infoPtr, uint32 selector, uint32 sourceID)
{
	D(bug(" AudioGetInfo %c%c%c%c, infoPtr %08lx, source ID %08lx\n", selector >> 24, (selector >> 16) & 0xff, (selector >> 8) & 0xff, selector & 0xff, infoPtr, sourceID));
	M68kRegisters r;

	switch (selector) {
		case siSampleSize:
			WriteMacInt16(infoPtr, AudioStatus.sample_size);
			break;

		case siSampleSizeAvailable: {
			r.d[0] = audio_sample_sizes.size() * 2;
			Execute68kTrap(0xa122, &r);	// NewHandle()
			uint32 h = r.a[0];
			if (h == 0)
				return memFullErr;
			WriteMacInt16(infoPtr + sil_count, uint16(audio_sample_sizes.size()));
			WriteMacInt32(infoPtr + sil_infoHandle, h);
			uint32 sp = ReadMacInt32(h);
			for (unsigned i=0; i<audio_sample_sizes.size(); i++)
				WriteMacInt16(sp + i*2, audio_sample_sizes[i]);
			break;
		}

		case siNumberChannels:
			WriteMacInt16(infoPtr, AudioStatus.channels);
			break;

		case siChannelAvailable: {
			r.d[0] = audio_channel_counts.size() * 2;
			Execute68kTrap(0xa122, &r);	// NewHandle()
			uint32 h = r.a[0];
			if (h == 0)
				return memFullErr;
			WriteMacInt16(infoPtr + sil_count, uint16(audio_channel_counts.size()));
			WriteMacInt32(infoPtr + sil_infoHandle, h);
			uint32 sp = ReadMacInt32(h);
			for (unsigned i=0; i<audio_channel_counts.size(); i++)
				WriteMacInt16(sp + i*2, audio_channel_counts[i]);
			break;
		}

		case siSampleRate:
			WriteMacInt32(infoPtr, AudioStatus.sample_rate);
			break;

		case siSampleRateAvailable: {
			r.d[0] = audio_sample_rates.size() * 4;
			Execute68kTrap(0xa122, &r);	// NewHandle()
			uint32 h = r.a[0];
			if (h == 0)
				return memFullErr;
			WriteMacInt16(infoPtr + sil_count, uint16(audio_sample_rates.size()));
			WriteMacInt32(infoPtr + sil_infoHandle, h);
			uint32 lp = ReadMacInt32(h);
			for (unsigned i=0; i<audio_sample_rates.size(); i++)
				WriteMacInt32(lp + i*4, audio_sample_rates[i]);
			break;
		}

		case siSpeakerMute:
			WriteMacInt16(infoPtr, audio_get_speaker_mute());
			break;

		case siSpeakerVolume:
			WriteMacInt32(infoPtr, audio_get_speaker_volume());
			break;

		case siHardwareMute:
			WriteMacInt16(infoPtr, audio_get_main_mute());
			break;

		case siHardwareVolume:
			WriteMacInt32(infoPtr, audio_get_main_volume());
			break;

		case siHardwareVolumeSteps:
			WriteMacInt16(infoPtr, 7);
			break;

		case siHardwareBusy:
			WriteMacInt16(infoPtr, AudioStatus.num_sources != 0);
			break;

		case siHardwareFormat:
			WriteMacInt32(infoPtr + scd_flags, 0);
			WriteMacInt32(infoPtr + scd_format, AudioStatus.sample_size == 16 ? FOURCC('t','w','o','s') : FOURCC('r','a','w',' '));
			WriteMacInt16(infoPtr + scd_numChannels, AudioStatus.channels);
			WriteMacInt16(infoPtr + scd_sampleSize, AudioStatus.sample_size);
			WriteMacInt32(infoPtr + scd_sampleRate, AudioStatus.sample_rate);
			WriteMacInt32(infoPtr + scd_sampleCount, audio_frames_per_block);
			WriteMacInt32(infoPtr + scd_buffer, 0);
			WriteMacInt32(infoPtr + scd_reserved, 0);
			break;

		default:	// Delegate to Apple Mixer
			if (AudioStatus.mixer == 0)
				return badComponentSelector;
			M68kRegisters r;
			r.a[0] = infoPtr;
			r.d[0] = selector;
			r.a[1] = sourceID;
			r.a[2] = AudioStatus.mixer;
			Execute68k(audio_data + adatGetInfo, &r);
			D(bug("  delegated to Apple Mixer, returns %08lx\n", r.d[0]));
			return r.d[0];
	}
	return noErr;
}


/*
 *  Set audio info
 */

static int32 AudioSetInfo(uint32 infoPtr, uint32 selector, uint32 sourceID)
{
	D(bug(" AudioSetInfo %c%c%c%c, infoPtr %08lx, source ID %08lx\n", selector >> 24, (selector >> 16) & 0xff, (selector >> 8) & 0xff, selector & 0xff, infoPtr, sourceID));
	M68kRegisters r;

	switch (selector) {
		case siSampleSize:
			D(bug("  set sample size %08lx\n", infoPtr));
			if (AudioStatus.num_sources)
				return siDeviceBusyErr;
			if (infoPtr == AudioStatus.sample_size)
				return noErr;
			for (unsigned i=0; i<audio_sample_sizes.size(); i++)
				if (audio_sample_sizes[i] == infoPtr) {
					if (audio_set_sample_size(i))
						return noErr;
					else
						return siInvalidSampleSize;
				}
			return siInvalidSampleSize;

		case siSampleRate:
			D(bug("  set sample rate %08lx\n", infoPtr));
			if (AudioStatus.num_sources)
				return siDeviceBusyErr;
			if (infoPtr == AudioStatus.sample_rate)
				return noErr;
			for (unsigned i=0; i<audio_sample_rates.size(); i++)
				if (audio_sample_rates[i] == infoPtr) {
					if (audio_set_sample_rate(i))
						return noErr;
					else
						return siInvalidSampleRate;
				}
			return siInvalidSampleRate;

		case siNumberChannels:
			D(bug("  set number of channels %08lx\n", infoPtr));
			if (AudioStatus.num_sources)
				return siDeviceBusyErr;
			if (infoPtr == AudioStatus.channels)
				return noErr;
			for (unsigned i=0; i<audio_channel_counts.size(); i++)
				if (audio_channel_counts[i] == infoPtr) {
					if (audio_set_channels(i))
						return noErr;
					else
						return badChannel;
				}
			return badChannel;

		case siSpeakerMute:
			audio_set_speaker_mute(uint16(infoPtr) != 0);
			break;

		case siSpeakerVolume:
			D(bug("  set speaker volume %08lx\n", infoPtr));
			audio_set_speaker_volume(infoPtr);
			break;

		case siHardwareMute:
			audio_set_main_mute(uint16(infoPtr) != 0);
			break;

		case siHardwareVolume:
			D(bug("  set hardware volume %08lx\n", infoPtr));
			audio_set_main_volume(infoPtr);
			break;

		default:	// Delegate to Apple Mixer
			if (AudioStatus.mixer == 0)
				return badComponentSelector;
			r.a[0] = infoPtr;
			r.d[0] = selector;
			r.a[1] = sourceID;
			r.a[2] = AudioStatus.mixer;
			Execute68k(audio_data + adatSetInfo, &r);
			D(bug("  delegated to Apple Mixer, returns %08lx\n", r.d[0]));
			return r.d[0];
	}
	return noErr;
}


/*
 *  Sound output component dispatch
 */

int32 AudioDispatch(uint32 params, uint32 globals)
{
	D(bug("AudioDispatch params %08lx (size %d), what %d\n", params, ReadMacInt8(params + cp_paramSize), (int16)ReadMacInt16(params + cp_what)));
	M68kRegisters r;
	uint32 p = params + cp_params;
	int16 selector = (int16)ReadMacInt16(params + cp_what);

	switch (selector) {

		// General component functions
		case kComponentOpenSelect:
			if (audio_data == 0) {

				// Allocate global data area
				r.d[0] = SIZEOF_adat;
				Execute68kTrap(0xa040, &r);	// ResrvMem()
				r.d[0] = SIZEOF_adat;
				Execute68kTrap(0xa31e, &r);	// NewPtrClear()
				if (r.a[0] == 0)
					return memFullErr;
				audio_data = r.a[0];
				D(bug(" global data at %08lx\n", audio_data));

				// Put in 68k routines
				int p = audio_data + adatDelegateCall;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x7024); p += 2;	// moveq	#$24,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatOpenMixer)
					goto adat_error;
				WriteMacInt16(p, 0x558f); p += 2;	// subq.l	#2,sp
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f00); p += 2;	// move.l	d0,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x203c); p += 2;	// move.l	#$06140018,d0
				WriteMacInt32(p, 0x06140018); p+= 4;
				WriteMacInt16(p, 0xa800); p += 2;	// SoundDispatch
				WriteMacInt16(p, 0x301f); p += 2;	// move.w	(sp)+,d0
				WriteMacInt16(p, 0x48c0); p += 2;	// ext.l	d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatCloseMixer)
					goto adat_error;
				WriteMacInt16(p, 0x558f); p += 2;	// subq.l	#2,sp
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x203c); p += 2;	// move.l	#$02180018,d0
				WriteMacInt32(p, 0x02180018); p+= 4;
				WriteMacInt16(p, 0xa800); p += 2;	// SoundDispatch
				WriteMacInt16(p, 0x301f); p += 2;	// move.w	(sp)+,d0
				WriteMacInt16(p, 0x48c0); p += 2;	// ext.l	d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatGetInfo)
					goto adat_error;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f0a); p += 2;	// move.l	a2,-(sp)
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f00); p += 2;	// move.l	d0,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x2f3c); p += 2;	// move.l	#$000c0103,-(sp)
				WriteMacInt32(p, 0x000c0103); p+= 4;
				WriteMacInt16(p, 0x7000); p += 2;	// moveq	#0,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatSetInfo)
					goto adat_error;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f0a); p += 2;	// move.l	a2,-(sp)
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f00); p += 2;	// move.l	d0,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x2f3c); p += 2;	// move.l	#$000c0104,-(sp)
				WriteMacInt32(p, 0x000c0104); p+= 4;
				WriteMacInt16(p, 0x7000); p += 2;	// moveq	#0,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatPlaySourceBuffer)
					goto adat_error;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f0a); p += 2;	// move.l	a2,-(sp)
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x2f00); p += 2;	// move.l	d0,-(sp)
				WriteMacInt16(p, 0x2f3c); p += 2;	// move.l	#$000c0108,-(sp)
				WriteMacInt32(p, 0x000c0108); p+= 4;
				WriteMacInt16(p, 0x7000); p += 2;	// moveq	#0,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatGetSourceData)
					goto adat_error;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x2f3c); p += 2;	// move.l	#$00040004,-(sp)
				WriteMacInt32(p, 0x00040004); p+= 4;
				WriteMacInt16(p, 0x7000); p += 2;	// moveq	#0,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatStartSource)
					goto adat_error;
				WriteMacInt16(p, 0x598f); p += 2;	// subq.l	#4,sp
				WriteMacInt16(p, 0x2f09); p += 2;	// move.l	a1,-(sp)
				WriteMacInt16(p, 0x3f00); p += 2;	// move.w	d0,-(sp)
				WriteMacInt16(p, 0x2f08); p += 2;	// move.l	a0,-(sp)
				WriteMacInt16(p, 0x2f3c); p += 2;	// move.l	#$00060105,-(sp)
				WriteMacInt32(p, 0x00060105); p+= 4;
				WriteMacInt16(p, 0x7000); p += 2;	// moveq	#0,d0
				WriteMacInt16(p, 0xa82a); p += 2;	// ComponentDispatch
				WriteMacInt16(p, 0x201f); p += 2;	// move.l	(sp)+,d0
				WriteMacInt16(p, M68K_RTS); p += 2;	// rts
				if (p - audio_data != adatData)
					goto adat_error;
			}
			AudioAvailable = true;
			if (open_count == 0)
				audio_enter_stream();
			open_count++;
			return noErr;

adat_error:	printf("FATAL: audio component data block initialization error\n");
			QuitEmulator();
			return openErr;

		case kComponentCloseSelect:
			open_count--;
			if (open_count == 0) {
				if (audio_data) {
					if (AudioStatus.mixer) {
						// Close Apple Mixer
						r.a[0] = AudioStatus.mixer;
						Execute68k(audio_data + adatCloseMixer, &r);
						AudioStatus.mixer = 0;
						return r.d[0];
					}
					r.a[0] = audio_data;
					Execute68kTrap(0xa01f, &r);	// DisposePtr()
					audio_data = 0;
				}
				AudioStatus.num_sources = 0;
				audio_exit_stream();
			}
			return noErr;

		case kComponentCanDoSelect:
			switch ((int16)ReadMacInt16(p)) {
				case kComponentOpenSelect:
				case kComponentCloseSelect:
				case kComponentCanDoSelect:
				case kComponentVersionSelect:
				case kComponentRegisterSelect:
				case kSoundComponentInitOutputDeviceSelect:
				case kSoundComponentGetSourceSelect:
				case kSoundComponentGetInfoSelect:
				case kSoundComponentSetInfoSelect:
				case kSoundComponentStartSourceSelect:
					return 1;
				default:
					return 0;
			}

		case kComponentVersionSelect:
			return 0x00010003;

		case kComponentRegisterSelect:
			return noErr;

		// Sound component functions (not delegated)
		case kSoundComponentInitOutputDeviceSelect:
			D(bug(" InitOutputDevice\n"));
			if (!audio_open)
				return noHardwareErr;
			if (AudioStatus.mixer)
				return noErr;

			// Init sound component data
			WriteMacInt32(audio_data + adatData + scd_flags, 0);
			WriteMacInt32(audio_data + adatData + scd_format, AudioStatus.sample_size == 16 ? FOURCC('t','w','o','s') : FOURCC('r','a','w',' '));
			WriteMacInt16(audio_data + adatData + scd_numChannels, AudioStatus.channels);
			WriteMacInt16(audio_data + adatData + scd_sampleSize, AudioStatus.sample_size);
			WriteMacInt32(audio_data + adatData + scd_sampleRate, AudioStatus.sample_rate);
			WriteMacInt32(audio_data + adatData + scd_sampleCount, audio_frames_per_block);
			WriteMacInt32(audio_data + adatData + scd_buffer, 0);
			WriteMacInt32(audio_data + adatData + scd_reserved, 0);
			WriteMacInt32(audio_data + adatStreamInfo, 0);

			// Open Apple Mixer
			r.a[0] = audio_data + adatMixer;
			r.d[0] = 0;
			r.a[1] = audio_data + adatData;
			Execute68k(audio_data + adatOpenMixer, &r);
			AudioStatus.mixer = ReadMacInt32(audio_data + adatMixer);
			D(bug(" OpenMixer() returns %08lx, mixer %08lx\n", r.d[0], AudioStatus.mixer));
			return r.d[0];

		case kSoundComponentGetSourceSelect:
			D(bug(" GetSource source %08lx\n", ReadMacInt32(p)));
			WriteMacInt32(ReadMacInt32(p), AudioStatus.mixer);
			return noErr;

		// Sound component functions (delegated)
		case kSoundComponentAddSourceSelect:
			D(bug(" AddSource\n"));
			AudioStatus.num_sources++;
			goto delegate;

		case kSoundComponentRemoveSourceSelect:
			D(bug(" RemoveSource\n"));
			AudioStatus.num_sources--;
			goto delegate;

		case kSoundComponentGetInfoSelect:
			return AudioGetInfo(ReadMacInt32(p), ReadMacInt32(p + 4), ReadMacInt32(p + 8));

		case kSoundComponentSetInfoSelect:
			return AudioSetInfo(ReadMacInt32(p), ReadMacInt32(p + 4), ReadMacInt32(p + 8));

		case kSoundComponentStartSourceSelect:
			D(bug(" StartSource count %d\n", ReadMacInt16(p + 4)));
			D(bug(" starting Apple Mixer\n"));
			r.d[0] = ReadMacInt16(p + 4);
			r.a[0] = ReadMacInt32(p);
			r.a[1] = AudioStatus.mixer;
			Execute68k(audio_data + adatStartSource, &r);
			D(bug(" returns %08lx\n", r.d[0]));
			return noErr;

		case kSoundComponentStopSourceSelect:
			D(bug(" StopSource\n"));
			goto delegate;

		case kSoundComponentPauseSourceSelect:
			D(bug(" PauseSource\n"));
delegate:	// Delegate call to Apple Mixer
			D(bug(" delegating call to Apple Mixer\n"));
			r.a[0] = AudioStatus.mixer;
			r.a[1] = params;
			Execute68k(audio_data + adatDelegateCall, &r);
			D(bug(" returns %08lx\n", r.d[0]));
			return r.d[0];

		case kSoundComponentPlaySourceBufferSelect:
			D(bug(" PlaySourceBuffer flags %08lx\n", ReadMacInt32(p)));
			r.d[0] = ReadMacInt32(p);
			r.a[0] = ReadMacInt32(p + 4);
			r.a[1] = ReadMacInt32(p + 8);
			r.a[2] = AudioStatus.mixer;
			Execute68k(audio_data + adatPlaySourceBuffer, &r);
			D(bug(" returns %08lx\n", r.d[0]));
			return r.d[0];

		default:
			if (selector >= 0x100)
				goto delegate;
			else
				return badComponentSelector;
	}
}


/*
 *  Sound input driver Open() routine
 */

int16 SoundInOpen(uint32 pb, uint32 dce)
{
	D(bug("SoundInOpen\n"));
	return noErr;
}


/*
 *  Sound input driver Prime() routine
 */

int16 SoundInPrime(uint32 pb, uint32 dce)
{
	D(bug("SoundInPrime\n"));
	//!!
	return paramErr;
}


/*
 *  Sound input driver Control() routine
 */

int16 SoundInControl(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SoundInControl %d\n", code));

	if (code == 1) {
		D(bug(" SoundInKillIO\n"));
		//!!
		return noErr;
	}

	if (code != 2)
		return -231;	// siUnknownInfoType

	uint32 *param = (uint32 *)Mac2HostAddr(pb + csParam);
	uint32 selector = param[0];
	D(bug(" selector %c%c%c%c\n", selector >> 24, selector >> 16, selector >> 8, selector));

	switch (selector) {
		default:
			return -231;	// siUnknownInfoType
	}
}


/*
 *  Sound input driver Status() routine
 */

int16 SoundInStatus(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SoundInStatus %d\n", code));
	if (code != 2)
		return -231;	// siUnknownInfoType

	uint32 *param = (uint32 *)Mac2HostAddr(pb + csParam);
	uint32 selector = param[0];
	D(bug(" selector %c%c%c%c\n", selector >> 24, selector >> 16, selector >> 8, selector));
	switch (selector) {
#if 0
		case siDeviceName: {
			const char *str = GetString(STR_SOUND_IN_NAME);
			param[0] = 0;
			memcpy((void *)param[1], str, strlen(str));
			return noErr;
		}

		case siDeviceIcon: {
			M68kRegisters r;
			static const uint8 proc[] = {
				0x55, 0x8f,							// 	subq.l	#2,sp
				0xa9, 0x94,							// 	CurResFile
				0x42, 0x67,							// 	clr.w	-(sp)
				0xa9, 0x98,							// 	UseResFile
				0x59, 0x8f,							// 	subq.l	#4,sp
				0x48, 0x79, 0x49, 0x43, 0x4e, 0x23,	// 	move.l	#'ICN#',-(sp)
				0x3f, 0x3c, 0xbf, 0x76,				// 	move.w	#-16522,-(sp)
				0xa9, 0xa0,							// 	GetResource
				0x24, 0x5f,							// 	move.l	(sp)+,a2
				0xa9, 0x98,							// 	UseResFile
				0x20, 0x0a,							// 	move.l	a2,d0
				0x66, 0x04,							// 	bne		1
				0x70, 0x00,							//  moveq	#0,d0
				M68K_RTS >> 8, M68K_RTS & 0xff,
				0x2f, 0x0a,							//1 move.l	a2,-(sp)
				0xa9, 0x92,							//  DetachResource
				0x20, 0x4a,							//  move.l	a2,a0
				0xa0, 0x4a,							//	HNoPurge
				0x70, 0x01,							//	moveq	#1,d0
				M68K_RTS >> 8, M68K_RTS & 0xff
			};
			Execute68k(Host2MacAddr((uint8 *)proc), &r);
			if (r.d[0]) {
				param[0] = 4;		// Length of returned data
				param[1] = r.a[2];	// Handle to icon suite
				return noErr;
			} else
				return -192;		// resNotFound
		}
#endif
		default:
			return -231;	// siUnknownInfoType
	}
}


/*
 *  Sound input driver Close() routine
 */

int16 SoundInClose(uint32 pb, uint32 dce)
{
	D(bug("SoundInClose\n"));
	return noErr;
}
