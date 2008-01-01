/*
 *  audio_solaris.cpp - Audio support, Solaris implementation
 *
 *  Adapted from Frodo's Solaris sound routines by Marc Chabanas
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
#include <sys/audioio.h>
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

#define DEBUG 0
#include "debug.h"


// Global variables
static int fd = -1;							// fd of /dev/audio
static sem_t audio_irq_done_sem;			// Signal from interrupt to streaming thread: data block read
static pthread_t stream_thread;				// Audio streaming thread
static pthread_attr_t stream_thread_attr;	// Streaming thread attributes
static bool stream_thread_active = false;
static int sound_buffer_size;				// Size of sound buffer in bytes

// Prototypes
static void *stream_func(void *arg);


/*
 *  Initialization
 */

// Set AudioStatus to reflect current audio stream format
static void set_audio_status_format(void)
{
	AudioStatus.sample_rate = audio_sample_rates[0];
	AudioStatus.sample_size = audio_sample_sizes[0];
	AudioStatus.channels = audio_channel_counts[0];
}

void AudioInit(void)
{
	char str[256];

	// Init audio status and feature flags
	audio_sample_rates.push_back(44100 << 16);
	audio_sample_sizes.push_back(16);
	audio_channel_counts.push_back(2);
	set_audio_status_format();
	AudioStatus.mixer = 0;
	AudioStatus.num_sources = 0;
	audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

	// Sound disabled in prefs? Then do nothing
	if (PrefsFindBool("nosound"))
		return;

	// Init semaphore
	if (sem_init(&audio_irq_done_sem, 0, 0) < 0)
		return;

	// Open /dev/audio
	fd = open("/dev/audio", O_WRONLY | O_NDELAY);
	if (fd < 0) {
		sprintf(str, GetString(STR_NO_AUDIO_DEV_WARN), "/dev/audio", strerror(errno));
		WarningAlert(str);
		sem_destroy(&audio_irq_done_sem);
		return;
	}

	// Set audio parameters
	struct audio_info info;
	AUDIO_INITINFO(&info);
	info.play.sample_rate = AudioStatus.sample_rate >> 16;
	info.play.channels = AudioStatus.channels;
	info.play.precision = AudioStatus.sample_size;
	info.play.encoding = AUDIO_ENCODING_LINEAR;
	info.play.port = AUDIO_SPEAKER;
	if (ioctl(fd, AUDIO_SETINFO, &info)) {
		WarningAlert(GetString(STR_AUDIO_FORMAT_WARN));
		close(fd);
		fd = -1;
		sem_destroy(&audio_irq_done_sem);
		return;
	}

	// 2048 frames per buffer
	audio_frames_per_block = 2048;
	sound_buffer_size = (AudioStatus.sample_size>>3) * AudioStatus.channels * audio_frames_per_block;

	// Start audio thread
	Set_pthread_attr(&stream_thread_attr, 0);
	stream_thread_active = (pthread_create(&stream_thread, &stream_thread_attr, stream_func, NULL) == 0);

	// Everything OK
	audio_open = true;
}


/*
 *  Deinitialization
 */

void AudioExit(void)
{
	// Stop audio thread
	if (stream_thread_active) {
		pthread_cancel(stream_thread);
		pthread_join(stream_thread, NULL);
		sem_destroy(&audio_irq_done_sem);
		stream_thread_active = false;
	}

	// Close /dev/audio
	if (fd > 0) {
		ioctl(fd, AUDIO_DRAIN);
		close(fd);
	}
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

static uint32 apple_stream_info;	// Mac address of SoundComponentData struct describing next buffer

static void *stream_func(void *arg)
{
	int16 *silent_buffer = new int16[sound_buffer_size / 2];
	int16 *last_buffer = new int16[sound_buffer_size / 2];
	memset(silent_buffer, 0, sound_buffer_size);

	uint_t sent = 0, delta;
	struct audio_info status;

	for (;;) {
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

				// Send data to audio port
				if (work_size == sound_buffer_size)
					write(fd, Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer)), sound_buffer_size);
				else {
					// Last buffer
					Mac2Host_memcpy(last_buffer, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
					memset((uint8 *)last_buffer + work_size, 0, sound_buffer_size - work_size);
					write(fd, last_buffer, sound_buffer_size);
				}
				D(bug("stream: data written\n"));
			} else
				goto silence;

		} else {

			// Audio not active, play silence
silence:	write(fd, silent_buffer, sound_buffer_size);
		}

		// We allow a maximum of three buffers to be sent
		sent += audio_frames_per_block;
		ioctl(fd, AUDIO_GETINFO, &status);
		while ((delta = sent - status.play.samples) > (audio_frames_per_block * 3)) {
			unsigned int sl = 1000000 * (delta - audio_frames_per_block * 3) / (AudioStatus.sample_rate >> 16);
			usleep(sl);
			ioctl(fd, AUDIO_GETINFO, &status);
		}
	}
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
 *  "index" is an index into the audio_sample_rates[] etc. arrays
 *  It is guaranteed that AudioStatus.num_sources == 0
 */

bool audio_set_sample_rate(int index)
{
	return true;
}

bool audio_set_sample_size(int index)
{
	return true;
}

bool audio_set_channels(int index)
{
	return true;
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

bool audio_get_speaker_mute(void)
{
	return false;
}

uint32 audio_get_speaker_volume(void)
{
	return 0x01000100;
}

void audio_set_main_mute(bool mute)
{
}

void audio_set_main_volume(uint32 vol)
{
}

void audio_set_speaker_mute(bool mute)
{
}

void audio_set_speaker_volume(uint32 vol)
{
}
