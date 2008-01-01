/*
 *  audio_oss_esd.cpp - Audio support, implementation for OSS and ESD (Linux and FreeBSD)
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

#ifdef __linux__
#include <linux/soundcard.h>
#endif

#ifdef __FreeBSD__
#include <sys/soundcard.h>
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#ifdef ENABLE_ESD
#include <esd.h>
#endif

#define DEBUG 0
#include "debug.h"


// The currently selected audio parameters (indices in audio_sample_rates[] etc. vectors)
static int audio_sample_rate_index = 0;
static int audio_sample_size_index = 0;
static int audio_channel_count_index = 0;

// Global variables
static bool is_dsp_audio = false;					// Flag: is DSP audio
static int audio_fd = -1;							// fd of dsp or ESD
static int mixer_fd = -1;							// fd of mixer
static sem_t audio_irq_done_sem;					// Signal from interrupt to streaming thread: data block read
static bool sem_inited = false;						// Flag: audio_irq_done_sem initialized
static int sound_buffer_size;						// Size of sound buffer in bytes
static bool little_endian = false;					// Flag: DSP accepts only little-endian 16-bit sound data
static uint8 silence_byte;							// Byte value to use to fill sound buffers with silence
static pthread_t stream_thread;						// Audio streaming thread
static pthread_attr_t stream_thread_attr;			// Streaming thread attributes
static bool stream_thread_active = false;			// Flag: streaming thread installed
static volatile bool stream_thread_cancel = false;	// Flag: cancel streaming thread

// Prototypes
static void *stream_func(void *arg);


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

// Init using the dsp device, returns false on error
static bool open_dsp(void)
{
	// Open the device
	const char *dsp = PrefsFindString("dsp");
	audio_fd = open(dsp, O_WRONLY);
	if (audio_fd < 0) {
		fprintf(stderr, "WARNING: Cannot open %s (%s)\n", dsp, strerror(errno));
		return false;
	}

	printf("Using %s audio output\n", dsp);
	is_dsp_audio = true;

	// Get supported sample formats
	if (audio_sample_sizes.empty()) {
		unsigned long format;
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &format);
		if (format & AFMT_U8)
			audio_sample_sizes.push_back(8);
		if (format & (AFMT_S16_BE | AFMT_S16_LE))
			audio_sample_sizes.push_back(16);

		int stereo = 0;
		if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == 0 && stereo == 0)
			audio_channel_counts.push_back(1);
		stereo = 1;
		if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == 0 && stereo == 1)
			audio_channel_counts.push_back(2);

		if (audio_sample_sizes.empty() || audio_channel_counts.empty()) {
			WarningAlert(GetString(STR_AUDIO_FORMAT_WARN));
			close(audio_fd);
			audio_fd = -1;
			return false;
		}

		audio_sample_rates.push_back(11025 << 16);
		audio_sample_rates.push_back(22050 << 16);
		int rate = 44100;
		ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate);
		if (rate > 22050)
			audio_sample_rates.push_back(rate << 16);

		// Default to highest supported values
		audio_sample_rate_index = audio_sample_rates.size() - 1;
		audio_sample_size_index = audio_sample_sizes.size() - 1;
		audio_channel_count_index = audio_channel_counts.size() - 1;
	}

	// Set DSP parameters
	unsigned long format;
	if (audio_sample_sizes[audio_sample_size_index] == 8) {
		format = AFMT_U8;
		little_endian = false;
		silence_byte = 0x80;
	} else {
		unsigned long sup_format;
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &sup_format);
		if (sup_format & AFMT_S16_BE) {
			little_endian = false;
			format = AFMT_S16_BE;
		} else {
			little_endian = true;
			format = AFMT_S16_LE;
		}
		silence_byte = 0;
	}
	ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
	int frag = 0x0004000c;		// Block size: 4096 frames
	ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag);
	int stereo = (audio_channel_counts[audio_channel_count_index] == 2);
	ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo);
	int rate = audio_sample_rates[audio_sample_rate_index] >> 16;
	ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate);

	// Get sound buffer size
	ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &audio_frames_per_block);
	D(bug("DSP_GETBLKSIZE %d\n", audio_frames_per_block));
	return true;
}

// Init using ESD, returns false on error
static bool open_esd(void)
{
#ifdef ENABLE_ESD
	int rate;
	esd_format_t format = ESD_STREAM | ESD_PLAY;

	if (audio_sample_sizes.empty()) {

		// Default values
		rate = 44100;
		format |= (ESD_BITS16 | ESD_STEREO);

	} else {

		rate = audio_sample_rates[audio_sample_rate_index] >> 16;
		if (audio_sample_sizes[audio_sample_size_index] == 8)
			format |= ESD_BITS8;
		else
			format |= ESD_BITS16;
		if (audio_channel_counts[audio_channel_count_index] == 1)
			format |= ESD_MONO;
		else
			format |= ESD_STEREO;
	}

#if WORDS_BIGENDIAN
	little_endian = false;
#else
	little_endian = true;
#endif
	silence_byte = 0;	// Is this correct for 8-bit mode?

	// Open connection to ESD server
	audio_fd = esd_play_stream(format, rate, NULL, NULL);
	if (audio_fd < 0) {
		fprintf(stderr, "WARNING: Cannot open ESD connection\n");
		return false;
	}

	printf("Using ESD audio output\n");

	// ESD supports a variety of twisted little audio formats, all different
	if (audio_sample_sizes.empty()) {

		// The reason we do this here is that we don't want to add sample
		// rates etc. unless the ESD server connection could be opened
		// (if ESD fails, dsp might be tried next)
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

	// Sound buffer size = 4096 frames
	audio_frames_per_block = 4096;
	return true;
#else
	// ESD is not enabled, shut up the compiler
	return false;
#endif
}

static bool open_audio(void)
{
#ifdef ENABLE_ESD
	// If ESPEAKER is set, the user probably wants to use ESD, so try that first
	if (getenv("ESPEAKER"))
		if (open_esd())
			goto dev_opened;
#endif

	// Try to open dsp
	if (open_dsp())
		goto dev_opened;

#ifdef ENABLE_ESD
	// Hm, dsp failed so we try ESD again if ESPEAKER wasn't set
	if (!getenv("ESPEAKER"))
		if (open_esd())
			goto dev_opened;
#endif

	// No audio device succeeded
	WarningAlert(GetString(STR_NO_AUDIO_WARN));
	return false;

	// Device opened, set AudioStatus
dev_opened:
	sound_buffer_size = (audio_sample_sizes[audio_sample_size_index] >> 3) * audio_channel_counts[audio_channel_count_index] * audio_frames_per_block;
	set_audio_status_format();

	// Start streaming thread
	Set_pthread_attr(&stream_thread_attr, 0);
	stream_thread_active = (pthread_create(&stream_thread, &stream_thread_attr, stream_func, NULL) == 0);

	// Everything went fine
	audio_open = true;
	return true;
}

void AudioInit(void)
{
	// Init audio status (reasonable defaults) and feature flags
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
	if (sem_init(&audio_irq_done_sem, 0, 0) < 0)
		return;
	sem_inited = true;

	// Try to open the mixer device
	const char *mixer = PrefsFindString("mixer");
	mixer_fd = open(mixer, O_RDWR);
	if (mixer_fd < 0)
		printf("WARNING: Cannot open %s (%s)\n", mixer, strerror(errno));

	// Open and initialize audio device
	open_audio();
}


/*
 *  Deinitialization
 */

static void close_audio(void)
{
	// Stop stream and delete semaphore
	if (stream_thread_active) {
		stream_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(stream_thread);
#endif
		pthread_join(stream_thread, NULL);
		stream_thread_active = false;
	}

	// Close dsp or ESD socket
	if (audio_fd >= 0) {
		close(audio_fd);
		audio_fd = -1;
	}

	audio_open = false;
}

void AudioExit(void)
{
	// Stop the device immediately. Otherwise, close() sends
	// SNDCTL_DSP_SYNC, which may hang
	if (is_dsp_audio)
		ioctl(audio_fd, SNDCTL_DSP_RESET, 0);

	// Close audio device
	close_audio();

	// Delete semaphore
	if (sem_inited) {
		sem_destroy(&audio_irq_done_sem);
		sem_inited = false;
	}

	// Close mixer device
	if (mixer_fd >= 0) {
		close(mixer_fd);
		mixer_fd = -1;
	}
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
 *  Streaming function
 */

static void *stream_func(void *arg)
{
	int16 *silent_buffer = new int16[sound_buffer_size / 2];
	int16 *last_buffer = new int16[sound_buffer_size / 2];
	memset(silent_buffer, silence_byte, sound_buffer_size);

	while (!stream_thread_cancel) {
		if (AudioStatus.num_sources) {

			// Trigger audio interrupt to get new buffer
			D(bug("stream: triggering irq\n"));
			SetInterruptFlag(INTFLAG_AUDIO);
			TriggerInterrupt();
			D(bug("stream: waiting for ack\n"));
			sem_wait(&audio_irq_done_sem);
			D(bug("stream: ack received\n"));

			// Get size of audio data
			uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
			if (apple_stream_info) {
				int work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
				D(bug("stream: work_size %d\n", work_size));
				if (work_size > sound_buffer_size)
					work_size = sound_buffer_size;
				if (work_size == 0)
					goto silence;

				// Send data to DSP
				if (work_size == sound_buffer_size && !little_endian)
					write(audio_fd, Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer)), sound_buffer_size);
				else {
					// Last buffer or little-endian DSP
					if (little_endian) {
						int16 *p = (int16 *)Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
						for (int i=0; i<work_size/2; i++)
							last_buffer[i] = ntohs(p[i]);
					} else
						Mac2Host_memcpy(last_buffer, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
					memset((uint8 *)last_buffer + work_size, silence_byte, sound_buffer_size - work_size);
					write(audio_fd, last_buffer, sound_buffer_size);
				}
				D(bug("stream: data written\n"));
			} else
				goto silence;

		} else {

			// Audio not active, play silence
silence:	write(audio_fd, silent_buffer, sound_buffer_size);
		}
	}
	delete[] silent_buffer;
	delete[] last_buffer;
	return NULL;
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
	sem_post(&audio_irq_done_sem);
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
	return false;
}

uint32 audio_get_main_volume(void)
{
	if (mixer_fd >= 0) {
		int vol;
		if (ioctl(mixer_fd, SOUND_MIXER_READ_PCM, &vol) == 0) {
			int left = vol >> 8;
			int right = vol & 0xff;
			return ((left * 256 / 100) << 16) | (right * 256 / 100);
		}
	}
	return 0x01000100;
}

bool audio_get_speaker_mute(void)
{
	return false;
}

uint32 audio_get_speaker_volume(void)
{
	if (mixer_fd >= 0) {
		int vol;
		if (ioctl(mixer_fd, SOUND_MIXER_READ_VOLUME, &vol) == 0) {
			int left = vol >> 8;
			int right = vol & 0xff;
			return ((left * 256 / 100) << 16) | (right * 256 / 100);
		}
	}
	return 0x01000100;
}

void audio_set_main_mute(bool mute)
{
}

void audio_set_main_volume(uint32 vol)
{
	if (mixer_fd >= 0) {
		int left = vol >> 16;
		int right = vol & 0xffff;
		int p = ((left * 100 / 256) << 8) | (right * 100 / 256);
		ioctl(mixer_fd, SOUND_MIXER_WRITE_PCM, &p);
	}
}

void audio_set_speaker_mute(bool mute)
{
}

void audio_set_speaker_volume(uint32 vol)
{
	if (mixer_fd >= 0) {
		int left = vol >> 16;
		int right = vol & 0xffff;
		int p = ((left * 100 / 256) << 8) | (right * 100 / 256);
		ioctl(mixer_fd, SOUND_MIXER_WRITE_VOLUME, &p);
	}
}
