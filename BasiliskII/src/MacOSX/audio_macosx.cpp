/*
 *  audio_macosx.cpp - Audio support, implementation Mac OS X
 *  Copyright (C) 2006, Daniel Sumorok
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

#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"
#include "MacOSX_sound_if.h"

#define DEBUG 0
#include "debug.h"


// The currently selected audio parameters (indices in
// audio_sample_rates[] etc. vectors)
static int audio_sample_rate_index = 0;
static int audio_sample_size_index = 0;
static int audio_channel_count_index = 0;

// Prototypes
static OSXsoundOutput *soundOutput = NULL;
static bool main_mute = false;
static bool speaker_mute = false;

/*
 *  Initialization
 */
static int audioInt(void);

static bool open_audio(void)
{
	AudioStatus.sample_rate = audio_sample_rates[audio_sample_rate_index];
	AudioStatus.sample_size = audio_sample_sizes[audio_sample_size_index];
	AudioStatus.channels = audio_channel_counts[audio_channel_count_index];

	if (soundOutput)
		delete soundOutput;

	soundOutput = new OSXsoundOutput();
	soundOutput->start(AudioStatus.sample_size, AudioStatus.channels, 
					   AudioStatus.sample_rate >> 16);
	soundOutput->setCallback(audioInt);
	audio_frames_per_block = soundOutput->bufferSizeFrames();

	audio_open = true;
	return true;
}

void AudioInit(void)
{
	// Sound disabled in prefs? Then do nothing
	if (PrefsFindBool("nosound"))
		return;

	//audio_sample_sizes.push_back(8);
	audio_sample_sizes.push_back(16);

	audio_channel_counts.push_back(1);
	audio_channel_counts.push_back(2);
	
	audio_sample_rates.push_back(11025 << 16);
	audio_sample_rates.push_back(22050 << 16);
	audio_sample_rates.push_back(44100 << 16);

	// Default to highest supported values
	audio_sample_rate_index   = audio_sample_rates.size() - 1;
	audio_sample_size_index   = audio_sample_sizes.size() - 1;
	audio_channel_count_index = audio_channel_counts.size() - 1;

	AudioStatus.mixer = 0;
	AudioStatus.num_sources = 0;
	audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;
	audio_component_flags = 0;

	open_audio();
}


/*
 *  Deinitialization
 */

static void close_audio(void)
{
	D(bug("Closing Audio\n"));

	if (soundOutput)
	{
		delete soundOutput;
		soundOutput = NULL;
	}
	
	audio_open = false;
}

void AudioExit(void)
{
	// Close audio device
	close_audio();
}


/*
 *  First source added, start audio stream
 */

void audio_enter_stream()
{
	// Streaming thread is always running to avoid clicking noises
}


/*
 *  Last source removed, stop audio stream
 */

void audio_exit_stream()
{
	// Streaming thread is always running to avoid clicking noises
}


/*
 *  MacOS audio interrupt, read next data block
 */

void AudioInterrupt(void)
{
	D(bug("AudioInterrupt\n"));
	uint32 apple_stream_info;
	uint32 numSamples;
	int16 *p;
	M68kRegisters r;

	if (!AudioStatus.mixer)
	{
		numSamples = 0;
		soundOutput->sendAudioBuffer((void *)p, (int)numSamples);
		D(bug("AudioInterrupt done\n"));
		return;
	}

	// Get data from apple mixer
	r.a[0] = audio_data + adatStreamInfo;
	r.a[1] = AudioStatus.mixer;
	Execute68k(audio_data + adatGetSourceData, &r);
	D(bug(" GetSourceData() returns %08lx\n", r.d[0]));

	apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
	if (apple_stream_info && (main_mute == false) && (speaker_mute == false))
	{
		numSamples = ReadMacInt32(apple_stream_info + scd_sampleCount);
		p = (int16 *)Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
	}
	else
	{
		numSamples = 0;
		p = NULL;
	}

	soundOutput->sendAudioBuffer((void *)p, (int)numSamples);

	D(bug("AudioInterrupt done\n"));
}


/*
 *  Set sampling parameters
 *  "index" is an index into the audio_sample_rates[] etc. vectors
 *  It is guaranteed that AudioStatus.num_sources == 0
 */

bool audio_set_sample_rate(int index)
{
	close_audio();
	audio_sample_rate_index = index;
	return open_audio();
}

bool audio_set_sample_size(int index)
{
	close_audio();
	audio_sample_size_index = index;
	return open_audio();
}

bool audio_set_channels(int index)
{
	close_audio();
	audio_channel_count_index = index;
	return open_audio();
}

/*
 *  Get/set volume controls (volume values received/returned have the
 *  left channel volume in the upper 16 bits and the right channel
 *  volume in the lower 16 bits; both volumes are 8.8 fixed point
 *  values with 0x0100 meaning "maximum volume"))
 */
bool audio_get_main_mute(void)
{
	return main_mute;
}

uint32 audio_get_main_volume(void)
{
	return 0x01000100;
}

bool audio_get_speaker_mute(void)
{
	return speaker_mute;
}

uint32 audio_get_speaker_volume(void)
{
	return 0x01000100;
}

void audio_set_main_mute(bool mute)
{
	main_mute = mute;
}

void audio_set_main_volume(uint32 vol)
{
}

void audio_set_speaker_mute(bool mute)
{
	speaker_mute = mute;
}

void audio_set_speaker_volume(uint32 vol)
{
}

static int audioInt(void)
{
	SetInterruptFlag(INTFLAG_AUDIO);
	TriggerInterrupt();
	return 0;
}
