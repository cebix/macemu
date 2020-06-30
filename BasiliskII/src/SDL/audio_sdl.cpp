/*
 *  audio_sdl.cpp - Audio support, SDL implementation
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
#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#include <SDL_mutex.h>
#include <SDL_audio.h>

#define DEBUG 0
#include "debug.h"

#if defined(BINCUE)
#include "bincue_unix.h"
#endif


#define MAC_MAX_VOLUME 0x0100

// The currently selected audio parameters (indices in audio_sample_rates[] etc. vectors)
static int audio_sample_rate_index = 0;
static int audio_sample_size_index = 0;
static int audio_channel_count_index = 0;

// Global variables
static SDL_sem *audio_irq_done_sem = NULL;			// Signal from interrupt to streaming thread: data block read
static uint8 silence_byte;							// Byte value to use to fill sound buffers with silence
static uint8 *audio_mix_buf = NULL;
static int audio_volume = SDL_MIX_MAXVOLUME;
static bool audio_mute = false;

// Prototypes
static void stream_func(void *arg, uint8 *stream, int stream_len);


/*
 *  Initialization
 */

// Set AudioStatus to reflect current audio stream format
static void set_audio_status_format(void)
{
	AudioStatus.sample_rate = audio_sample_rates[audio_sample_rate_index];
	AudioStatus.sample_size = audio_sample_sizes[audio_sample_size_index];
	AudioStatus.channels = audio_channel_counts[audio_channel_count_index];
}

// Init SDL audio system
static bool open_sdl_audio(void)
{
	// SDL supports a variety of twisted little audio formats, all different
	if (audio_sample_sizes.empty()) {
		audio_sample_rates.push_back(11025 << 16);
		audio_sample_rates.push_back(22050 << 16);
		audio_sample_rates.push_back(44100 << 16);
		audio_sample_sizes.push_back(8);
		audio_sample_sizes.push_back(16);
		audio_channel_counts.push_back(1);
		audio_channel_counts.push_back(2);

		// Default to highest supported values
		audio_sample_rate_index = audio_sample_rates.size() - 1;
		audio_sample_size_index = audio_sample_sizes.size() - 1;
		audio_channel_count_index = audio_channel_counts.size() - 1;
	}

	SDL_AudioSpec audio_spec;
	memset(&audio_spec, 0, sizeof(audio_spec));
	audio_spec.freq = audio_sample_rates[audio_sample_rate_index] >> 16;
	audio_spec.format = (audio_sample_sizes[audio_sample_size_index] == 8) ? AUDIO_U8 : AUDIO_S16MSB;
	audio_spec.channels = audio_channel_counts[audio_channel_count_index];
	audio_spec.samples = 4096;
	audio_spec.callback = stream_func;
	audio_spec.userdata = NULL;

	// Open the audio device, forcing the desired format
	if (SDL_OpenAudio(&audio_spec, NULL) < 0) {
		fprintf(stderr, "WARNING: Cannot open audio: %s\n", SDL_GetError());
		return false;
	}
	
#ifdef ENABLE_SDL2
	// HACK: workaround a bug in SDL pre-2.0.6 (reported via https://bugzilla.libsdl.org/show_bug.cgi?id=3710 )
	// whereby SDL does not update audio_spec.size
	if (audio_spec.size == 0) {
		audio_spec.size = (SDL_AUDIO_BITSIZE(audio_spec.format) / 8) * audio_spec.channels * audio_spec.samples;
	}
#endif

#if defined(BINCUE)
	OpenAudio_bincue(audio_spec.freq, audio_spec.format, audio_spec.channels,
	audio_spec.silence);
#endif

#ifdef ENABLE_SDL2
	const char * driver_name = SDL_GetCurrentAudioDriver();
#else
	char driver_name[32];
	SDL_AudioDriverName(driver_name, sizeof(driver_name) - 1);
#endif
	printf("Using SDL/%s audio output\n", driver_name ? driver_name : "");
	silence_byte = audio_spec.silence;
	SDL_PauseAudio(0);

	// Sound buffer size = 4096 frames
	audio_frames_per_block = audio_spec.samples;
	audio_mix_buf = (uint8*)malloc(audio_spec.size);
	return true;
}

static bool open_audio(void)
{
	// Try to open SDL audio
	if (!open_sdl_audio()) {
		WarningAlert(GetString(STR_NO_AUDIO_WARN));
		return false;
	}

	// Device opened, set AudioStatus
	set_audio_status_format();

	// Everything went fine
	audio_open = true;
	return true;
}

void AudioInit(void)
{
	// Init audio status and feature flags
	AudioStatus.sample_rate = 44100 << 16;
	AudioStatus.sample_size = 16;
	AudioStatus.channels = 2;
	AudioStatus.mixer = 0;
	AudioStatus.num_sources = 0;
	audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

	// Sound disabled in prefs? Then do nothing
	if (PrefsFindBool("nosound"))
		return;

	// Init semaphore
	audio_irq_done_sem = SDL_CreateSemaphore(0);

	// Open and initialize audio device
	open_audio();
}


/*
 *  Deinitialization
 */

static void close_audio(void)
{
	// Close audio device
	SDL_CloseAudio();
	free(audio_mix_buf);
	audio_mix_buf = NULL;
	audio_open = false;
}

void AudioExit(void)
{
	// Close audio device
	close_audio();

	// Delete semaphore
	if (audio_irq_done_sem)
		SDL_DestroySemaphore(audio_irq_done_sem);
}


/*
 *  First source added, start audio stream
 */

void audio_enter_stream()
{
}


/*
 *  Last source removed, stop audio stream
 */

void audio_exit_stream()
{
}


/*
 *  Streaming function
 */

static void stream_func(void *arg, uint8 *stream, int stream_len)
{
	if (AudioStatus.num_sources) {
		// Trigger audio interrupt to get new buffer
		D(bug("stream: triggering irq\n"));
		SetInterruptFlag(INTFLAG_AUDIO);
		TriggerInterrupt();
		D(bug("stream: waiting for ack\n"));
		SDL_SemWait(audio_irq_done_sem);
		D(bug("stream: ack received\n"));

		// Get size of audio data
		uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
		if (apple_stream_info && !audio_mute) {
			int work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
			D(bug("stream: work_size %d\n", work_size));
			if (work_size > stream_len)
				work_size = stream_len;
			if (work_size == 0)
				goto silence;

			// Send data to audio device
			Mac2Host_memcpy(audio_mix_buf, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
			memset((uint8 *)stream, silence_byte, stream_len);
			SDL_MixAudio(stream, audio_mix_buf, work_size, audio_volume);

			D(bug("stream: data written\n"));

		} else
			goto silence;

	} else {

		// Audio not active, play silence
silence: memset(stream, silence_byte, stream_len);
	}
#if defined(BINCUE)
	MixAudio_bincue(stream, stream_len);
#endif
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
	SDL_SemPost(audio_irq_done_sem);
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
 *  Get/set volume controls (volume values received/returned have the left channel
 *  volume in the upper 16 bits and the right channel volume in the lower 16 bits;
 *  both volumes are 8.8 fixed point values with 0x0100 meaning "maximum volume"))
 */

bool audio_get_main_mute(void)
{
	return audio_mute;
}

uint32 audio_get_main_volume(void)
{
	uint32 chan = (audio_volume * MAC_MAX_VOLUME / SDL_MIX_MAXVOLUME);
	return (chan << 16) + chan;
}

bool audio_get_speaker_mute(void)
{
	return audio_mute;
}

uint32 audio_get_speaker_volume(void)
{
	return audio_get_main_volume();
}

void audio_set_main_mute(bool mute)
{
	audio_mute = mute;
}

void audio_set_main_volume(uint32 vol)
{
	// We only have one-channel volume right now.
	uint32 avg = ((vol >> 16) + (vol & 0xffff)) / 2;
	if (avg > MAC_MAX_VOLUME)
		avg = MAC_MAX_VOLUME;
	audio_volume = avg * SDL_MIX_MAXVOLUME / MAC_MAX_VOLUME;
}

void audio_set_speaker_mute(bool mute)
{
}

void audio_set_speaker_volume(uint32 vol)
{
}
