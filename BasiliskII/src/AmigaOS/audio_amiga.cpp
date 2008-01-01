/*
 *  audio_amiga.cpp - Audio support, AmigaOS implementation using AHI
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

#include "sysdeps.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/ahi.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/ahi.h>
#include <inline/exec.h>
#include <inline/ahi.h>

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#define DEBUG 0
#include "debug.h"

#define D1(x) ;


// Global variables
static ULONG ahi_id = AHI_DEFAULT_ID;			// AHI audio ID
static struct AHIAudioCtrl *ahi_ctrl = NULL;
static struct AHISampleInfo sample[2];			// Two sample infos for double-buffering
static struct Hook sf_hook;
static int play_buf = 0;						// Number of currently played buffer
static long sound_buffer_size;					// Size of one audio buffer in bytes
static int audio_block_fetched = 0;				// Number of audio blocks fetched by interrupt routine

static bool main_mute = false;
static bool speaker_mute = false;
static ULONG supports_volume_changes = false;
static ULONG supports_stereo_panning = false;
static ULONG current_main_volume;
static ULONG current_speaker_volume;


// Prototypes
static __saveds __attribute__((regparm(3))) ULONG audio_callback(struct Hook *hook /*a0*/, struct AHISoundMessage *msg /*a1*/, struct AHIAudioCtrl *ahi_ctrl /*a2*/);
void audio_set_sample_rate_byval(uint32 value);
void audio_set_sample_size_byval(uint32 value);
void audio_set_channels_byval(uint32 value);


/*
 *  Initialization
 */

// Set AudioStatus to reflect current audio stream format
static void set_audio_status_format(int sample_rate_index)
{
	AudioStatus.sample_rate = audio_sample_rates[sample_rate_index];
	AudioStatus.sample_size = audio_sample_sizes[0];
	AudioStatus.channels = audio_channel_counts[0];
}

void AudioInit(void)
{
	sample[0].ahisi_Address = sample[1].ahisi_Address = NULL;

	// Init audio status and feature flags
	audio_channel_counts.push_back(2);
//	set_audio_status_format();
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

	ULONG max_channels, sample_rate, frequencies, sample_rate_index;

	AHI_GetAudioAttrs(ahi_id, ahi_ctrl,
		AHIDB_MaxChannels, (ULONG) &max_channels,
		AHIDB_Frequencies, (ULONG) &frequencies,
		TAG_END);

	D(bug("AudioInit: max_channels=%ld frequencies=%ld\n", max_channels, frequencies));

	for (int n=0; n<frequencies; n++)
		{
		AHI_GetAudioAttrs(ahi_id, ahi_ctrl,
			AHIDB_FrequencyArg, n,
			AHIDB_Frequency, (ULONG) &sample_rate,
			TAG_END);

		D(bug("AudioInit: f=%ld Hz\n", sample_rate));
		audio_sample_rates.push_back(sample_rate << 16);
		}

	ULONG sample_size_bits = 16;

	D(bug("AudioInit: sampe_rates=%ld\n", audio_sample_rates.size() ));

	// get index of sample rate closest to 22050 Hz
	AHI_GetAudioAttrs(ahi_id, ahi_ctrl,
		AHIDB_IndexArg, 22050,
		AHIDB_Bits, (ULONG) &sample_size_bits,
		AHIDB_Index, (ULONG) &sample_rate_index,
		AHIDB_Volume, (ULONG) &supports_volume_changes,
		AHIDB_Panning, (ULONG) &supports_stereo_panning,
		TAG_END);

	audio_sample_sizes.push_back(16);

	set_audio_status_format(sample_rate_index);

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
	current_main_volume = current_speaker_volume = 0x10000;
	AHI_SetVol(0, current_speaker_volume, 0x8000, ahi_ctrl, AHISF_IMM);

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

static __saveds __attribute__((regparm(3))) ULONG audio_callback(struct Hook *hook /*a0*/, struct AHISoundMessage *msg /*a1*/, struct AHIAudioCtrl *ahi_ctrl /*a2*/)
{
	play_buf ^= 1;

	// New buffer available?
	if (audio_block_fetched)
		{
		audio_block_fetched--;

		if (main_mute || speaker_mute)
			{
			memset(sample[play_buf].ahisi_Address, 0, sound_buffer_size);
			}
		else
			{
			// Get size of audio data
			uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
			if (apple_stream_info) {
				int32 sample_count = ReadMacInt32(apple_stream_info + scd_sampleCount);

				uint32 num_channels = ReadMacInt16(apple_stream_info + scd_numChannels);
				uint32 sample_size = ReadMacInt16(apple_stream_info + scd_sampleSize);
				uint32 sample_rate = ReadMacInt32(apple_stream_info + scd_sampleRate);

				D(bug("stream: sample_count=%ld  num_channels=%ld  sample_size=%ld  sample_rate=%ld\n", sample_count, num_channels, sample_size, sample_rate >> 16));

				// Yes, this can happen.
				if(sample_count != 0) {
					if(sample_rate != AudioStatus.sample_rate) {
						audio_set_sample_rate_byval(sample_rate);
					}
					if(num_channels != AudioStatus.channels) {
						audio_set_channels_byval(num_channels);
					}
					if(sample_size != AudioStatus.sample_size) {
						audio_set_sample_size_byval(sample_size);
					}
				}

				if (sample_count < 0)
					sample_count = 0;

				int work_size = sample_count * num_channels * (sample_size>>3);
				D(bug("stream: work_size=%ld  sound_buffer_size=%ld\n", work_size, sound_buffer_size));

				if (work_size > sound_buffer_size)
					work_size = sound_buffer_size;

				// Put data into AHI buffer (convert 8-bit data unsigned->signed)
				if (AudioStatus.sample_size == 16)
					Mac2Host_memcpy(sample[play_buf].ahisi_Address, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
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
		}

		}
	else
		memset(sample[play_buf].ahisi_Address, 0, sound_buffer_size);

	// Play next buffer
	AHI_SetSound(0, play_buf, 0, 0, ahi_ctrl, 0);

	// Trigger audio interrupt to get new buffer
	if (AudioStatus.num_sources) {
		D1(bug("stream: triggering irq\n"));
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
	D1(bug("AudioInterrupt\n"));

	// Get data from apple mixer
	if (AudioStatus.mixer) {
		M68kRegisters r;
		r.a[0] = audio_data + adatStreamInfo;
		r.a[1] = AudioStatus.mixer;
		Execute68k(audio_data + adatGetSourceData, &r);
		D1(bug(" GetSourceData() returns %08lx\n", r.d[0]));
	} else
		WriteMacInt32(audio_data + adatStreamInfo, 0);

	// Signal stream function
	audio_block_fetched++;
	D1(bug("AudioInterrupt done\n"));
}


/*
 *  Set sampling parameters
 *  "index" is an index into the audio_sample_rates[] etc. arrays
 *  It is guaranteed that AudioStatus.num_sources == 0
 */

void audio_set_sample_rate_byval(uint32 value)
{
	bool changed = (AudioStatus.sample_rate != value);
	if(changed)
		{
		ULONG sample_rate_index;

		// get index of sample rate closest to <value> Hz
		AHI_GetAudioAttrs(ahi_id, ahi_ctrl,
			AHIDB_IndexArg, value >> 16,
			AHIDB_Index, (ULONG) &sample_rate_index,
			TAG_END);

		D(bug(" audio_set_sample_rate_byval requested rate=%ld Hz\n", value >> 16));

		AudioStatus.sample_rate = audio_sample_rates[sample_rate_index];

		AHI_SetFreq(0, AudioStatus.sample_rate >> 16, ahi_ctrl, 0);
		}

	D(bug(" audio_set_sample_rate_byval rate=%ld Hz\n", AudioStatus.sample_rate >> 16));
}

void audio_set_sample_size_byval(uint32 value)
{
	bool changed = (AudioStatus.sample_size != value);
	if(changed) {
//		AudioStatus.sample_size = value;
//		update_sound_parameters();
//		WritePrivateProfileInt( "Audio", "SampleSize", AudioStatus.sample_size, ini_file_name );
	}
	D(bug(" audio_set_sample_size_byval %d\n", AudioStatus.sample_size));
}

void audio_set_channels_byval(uint32 value)
{
	bool changed = (AudioStatus.channels != value);
	if(changed) {
//		AudioStatus.channels = value;
//		update_sound_parameters();
//		WritePrivateProfileInt( "Audio", "Channels", AudioStatus.channels, ini_file_name );
	}
	D(bug(" audio_set_channels_byval %d\n", AudioStatus.channels));
}

bool audio_set_sample_rate(int index)
{
	if(index >= 0 && index < audio_sample_rates.size() ) {
		audio_set_sample_rate_byval( audio_sample_rates[index] );
		D(bug(" audio_set_sample_rate index=%ld rate=%ld\n", index, AudioStatus.sample_rate >> 16));
	}

	return true;
}

bool audio_set_sample_size(int index)
{
	if(index >= 0 && index < audio_sample_sizes.size()  ) {
		audio_set_sample_size_byval( audio_sample_sizes[index] );
		D(bug(" audio_set_sample_size %d,%d\n", index,AudioStatus.sample_size));
	}

	return true;
}

bool audio_set_channels(int index)
{
	if(index >= 0 && index < audio_channel_counts.size()   ) {
		audio_set_channels_byval( audio_channel_counts[index] );
		D(bug(" audio_set_channels %d,%d\n", index,AudioStatus.channels));
	}

	return true;
}


/*
 *  Get/set volume controls (volume values received/returned have the left channel
 *  volume in the upper 16 bits and the right channel volume in the lower 16 bits;
 *  both volumes are 8.8 fixed point values with 0x0100 meaning "maximum volume"))
 */

bool audio_get_main_mute(void)
{
	D(bug("audio_get_main_mute:  mute=%ld\n", main_mute));

	return main_mute;
}

uint32 audio_get_main_volume(void)
{
	D(bug("audio_get_main_volume\n"));

		ULONG volume = current_main_volume >> 8;	// 0x10000 => 0x100

		D(bug("audio_get_main_volume: volume=%08lx\n", volume));

		return (volume << 16) + volume;

	return 0x01000100;
}

bool audio_get_speaker_mute(void)
{
	D(bug("audio_get_speaker_mute:  mute=%ld\n", speaker_mute));

	return speaker_mute;
}

uint32 audio_get_speaker_volume(void)
{
	D(bug("audio_get_speaker_volume: \n"));

	if (audio_open)
		{
		ULONG volume = current_speaker_volume >> 8;	// 0x10000 => 0x100

		D(bug("audio_get_speaker_volume: volume=%08lx\n", volume));

		return (volume << 16) + volume;
		}

	return 0x01000100;
}

void audio_set_main_mute(bool mute)
{
	D(bug("audio_set_main_mute: mute=%ld\n", mute));

	if (mute != main_mute)
		{
		main_mute = mute;
		}
}

void audio_set_main_volume(uint32 vol)
{
	D(bug("audio_set_main_volume: vol=%08lx\n", vol));

	if (audio_open && supports_volume_changes)
		{
		ULONG volume = 0x80 * ((vol >> 16) + (vol & 0xffff));

		D(bug("audio_set_main_volume: volume=%08lx\n", volume));

		current_main_volume = volume;

		AHI_SetVol(0, volume, 0x8000, ahi_ctrl, AHISF_IMM);
		}
}

void audio_set_speaker_mute(bool mute)
{
	D(bug("audio_set_speaker_mute: mute=%ld\n", mute));

	if (mute != speaker_mute)
		{
		speaker_mute = mute;
		}
}

void audio_set_speaker_volume(uint32 vol)
{
	D(bug("audio_set_speaker_volume: vol=%08lx\n", vol));

	if (audio_open && supports_volume_changes)
		{
		ULONG volume = 0x80 * ((vol >> 16) + (vol & 0xffff));

		D(bug("audio_set_speaker_volume: volume=%08lx\n", volume));

		current_speaker_volume = volume;

		AHI_SetVol(0, volume, 0x8000, ahi_ctrl, AHISF_IMM);
		}
}
