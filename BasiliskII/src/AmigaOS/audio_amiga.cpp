/*
 *  audio_amiga.cpp - Audio support, AmigaOS implementation using AHI
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

#include "sysdeps.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/ahi.h>

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#define DEBUG 0
#include "debug.h"


// Supported sample rates, sizes and channels
int audio_num_sample_rates = 1;
uint32 audio_sample_rates[] = {22050 << 16};
int audio_num_sample_sizes = 1;
uint16 audio_sample_sizes[] = {16};
int audio_num_channel_counts = 1;
uint16 audio_channel_counts[] = {2};


// Global variables
static ULONG ahi_id = AHI_DEFAULT_ID;			// AHI audio ID
static struct AHIAudioCtrl *ahi_ctrl = NULL;
static struct AHISampleInfo sample[2];			// Two sample infos for double-buffering
static struct Hook sf_hook;
static int play_buf = 0;						// Number of currently played buffer
static long sound_buffer_size;					// Size of one audio buffer in bytes
static int audio_block_fetched = 0;				// Number of audio blocks fetched by interrupt routine


// Prototypes
static __saveds __asm ULONG audio_callback(register __a0 struct Hook *hook, register __a2 struct AHIAudioCtrl *ahi_ctrl, register __a1 struct AHISoundMessage *msg);


/*
 *  Initialization
 */

void AudioInit(void)
{
	sample[0].ahisi_Address = sample[1].ahisi_Address = NULL;

	// Init audio status and feature flags
	AudioStatus.sample_rate = audio_sample_rates[0];
	AudioStatus.sample_size = audio_sample_sizes[0];
	AudioStatus.channels = audio_channel_counts[0];
	AudioStatus.mixer = 0;
	AudioStatus.num_sources = 0;
	audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

	// Sound disabled in prefs? Then do nothing
	if (PrefsFindBool("nosound"))
		return;

	// AHI available?
	if (AHIBase == NULL) {
		WarningAlert(GetString(STR_NO_AHI_WARN));
		return;
	}

	// Initialize callback hook
	sf_hook.h_Entry = (HOOKFUNC)audio_callback;

	// Read "sound" preferences
	const char *str = PrefsFindString("sound");
	if (str)
		sscanf(str, "ahi/%08lx", &ahi_id);

	// Open audio control structure
	if ((ahi_ctrl = AHI_AllocAudio(
		AHIA_AudioID, ahi_id,
		AHIA_MixFreq, AudioStatus.sample_rate >> 16,
		AHIA_Channels, 1,
		AHIA_Sounds, 2,
		AHIA_SoundFunc, (ULONG)&sf_hook,
		TAG_END)) == NULL) {
		WarningAlert(GetString(STR_NO_AHI_CTRL_WARN));
		return;
	}

	// 2048 frames per block
	audio_frames_per_block = 2048;
	sound_buffer_size = (AudioStatus.sample_size >> 3) * AudioStatus.channels * audio_frames_per_block;

	// Prepare SampleInfos and load sounds (two sounds for double buffering)
	sample[0].ahisi_Type = AudioStatus.sample_size == 16 ? AHIST_S16S : AHIST_S8S;
	sample[0].ahisi_Length = audio_frames_per_block;
	sample[0].ahisi_Address = AllocVec(sound_buffer_size, MEMF_PUBLIC | MEMF_CLEAR);
	sample[1].ahisi_Type = AudioStatus.sample_size == 16 ? AHIST_S16S : AHIST_S8S;
	sample[1].ahisi_Length = audio_frames_per_block;
	sample[1].ahisi_Address = AllocVec(sound_buffer_size, MEMF_PUBLIC | MEMF_CLEAR);
	if (sample[0].ahisi_Address == NULL || sample[1].ahisi_Address == NULL)
		return;
	AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample[0], ahi_ctrl);
	AHI_LoadSound(1, AHIST_DYNAMICSAMPLE, &sample[1], ahi_ctrl);

	// Set parameters
	play_buf = 0;
	AHI_SetVol(0, 0x10000, 0x8000, ahi_ctrl, AHISF_IMM);
	AHI_SetFreq(0, AudioStatus.sample_rate >> 16, ahi_ctrl, AHISF_IMM);
	AHI_SetSound(0, play_buf, 0, 0, ahi_ctrl, AHISF_IMM);

	// Everything OK
	audio_open = true;
}


/*
 *  Deinitialization
 */

void AudioExit(void)
{
	// Free everything
	if (ahi_ctrl != NULL) {
		AHI_ControlAudio(ahi_ctrl, AHIC_Play, FALSE, TAG_END);
		AHI_FreeAudio(ahi_ctrl);
	}

	FreeVec(sample[0].ahisi_Address);
	FreeVec(sample[1].ahisi_Address);
}


/*
 *  First source added, start audio stream
 */

void audio_enter_stream()
{
	AHI_ControlAudio(ahi_ctrl, AHIC_Play, TRUE, TAG_END);
}


/*
 *  Last source removed, stop audio stream
 */

void audio_exit_stream()
{
	AHI_ControlAudio(ahi_ctrl, AHIC_Play, FALSE, TAG_END);
}


/*
 *  AHI sound callback, request next buffer
 */

static __saveds __asm ULONG audio_callback(register __a0 struct Hook *hook, register __a2 struct AHIAudioCtrl *ahi_ctrl, register __a1 struct AHISoundMessage *msg)
{
	play_buf ^= 1;

	// New buffer available?
	if (audio_block_fetched) {
		audio_block_fetched--;

		// Get size of audio data
		uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
		if (apple_stream_info) {
			int work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
			D(bug("stream: work_size %d\n", work_size));
			if (work_size > sound_buffer_size)
				work_size = sound_buffer_size;

			// Put data into AHI buffer (convert 8-bit data unsigned->signed)
			if (AudioStatus.sample_size == 16)
				memcpy(sample[play_buf].ahisi_Address, Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer)), work_size);
			else {
				uint32 *p = (uint32 *)Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
				uint32 *q = (uint32 *)sample[play_buf].ahisi_Address;
				int r = work_size >> 2;
				while (r--)
					*q++ = *p++ ^ 0x80808080;
			}
			if (work_size != sound_buffer_size)
				memset((uint8 *)sample[play_buf].ahisi_Address + work_size, 0, sound_buffer_size - work_size);
		}

	} else
		memset(sample[play_buf].ahisi_Address, 0, sound_buffer_size);

	// Play next buffer
	AHI_SetSound(0, play_buf, 0, 0, ahi_ctrl, 0);

	// Trigger audio interrupt to get new buffer
	if (AudioStatus.num_sources) {
		D(bug("stream: triggering irq\n"));
		SetInterruptFlag(INTFLAG_AUDIO);
		TriggerInterrupt();
	}
	return 0;
}


/*
 *  MacOS audio interrupt, read next data block
 */

void AudioInterrupt(void)
{
	D(bug("AudioInterrupt\n"));

	// Get data from apple mixer
	if (AudioStatus.mixer) {
		M68kRegisters r;
		r.a[0] = audio_data + adatStreamInfo;
		r.a[1] = AudioStatus.mixer;
		Execute68k(audio_data + adatGetSourceData, &r);
		D(bug(" GetSourceData() returns %08lx\n", r.d[0]));
	} else
		WriteMacInt32(audio_data + adatStreamInfo, 0);

	// Signal stream function
	audio_block_fetched++;
	D(bug("AudioInterrupt done\n"));
}


/*
 *  Set sampling parameters
 *  "index" is an index into the audio_sample_rates[] etc. arrays
 *  It is guaranteed that AudioStatus.num_sources == 0
 */

void audio_set_sample_rate(int index)
{
}

void audio_set_sample_size(int index)
{
}

void audio_set_channels(int index)
{
}


/*
 *  Get/set volume controls (volume values received/returned have the left channel
 *  volume in the upper 16 bits and the right channel volume in the lower 16 bits;
 *  both volumes are 8.8 fixed point values with 0x0100 meaning "maximum volume"))
 */

bool audio_get_main_mute(void)
{
	return false;
}

uint32 audio_get_main_volume(void)
{
	return 0x01000100;
}

bool audio_get_dac_mute(void)
{
	return false;
}

uint32 audio_get_dac_volume(void)
{
	return 0x01000100;
}

void audio_set_main_mute(bool mute)
{
}

void audio_set_main_volume(uint32 vol)
{
}

void audio_set_dac_mute(bool mute)
{
}

void audio_set_dac_volume(uint32 vol)
{
}
