/*
 *  audio_irix.cpp - Audio support, SGI Irix implementation
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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

#include <dmedia/audio.h>
#include <dmedia/dmedia.h>

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static int audio_fd = -1;							// fd from audio library
static sem_t audio_irq_done_sem;					// Signal from interrupt to streaming thread: data block read
static bool sem_inited = false;						// Flag: audio_irq_done_sem initialized
static int sound_buffer_size;						// Size of sound buffer in bytes
static int sound_buffer_fill_point;					// Fill buffer when this many frames are empty
static uint8 silence_byte = 0;						// Byte value to use to fill sound buffers with silence
static pthread_t stream_thread;						// Audio streaming thread
static pthread_attr_t stream_thread_attr;			// Streaming thread attributes
static bool stream_thread_active = false;			// Flag: streaming thread installed
static volatile bool stream_thread_cancel = false;	// Flag: cancel streaming thread

// IRIX libaudio control structures
static ALconfig config;
static ALport   port;


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

// Init libaudio, returns false on error
bool audio_init_al(void)
{
	ALpv     pv[2];

	printf("Using libaudio audio output\n");

	// Try to open the audio library

	config = alNewConfig();
	alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP);
	alSetWidth(config, AL_SAMPLE_16);
	alSetChannels(config, 2);	// stereo
	alSetDevice(config, AL_DEFAULT_OUTPUT); // Allow selecting via prefs?
	
	port = alOpenPort("BasiliskII", "w", config);
	if (port == NULL) {
		fprintf(stderr, "ERROR: Cannot open audio port: %s\n", 
				alGetErrorString(oserror()));
		return false;
	}

	// Set the sample rate

	pv[0].param = AL_RATE;
	pv[0].value.ll = alDoubleToFixed(audio_sample_rates[0] >> 16);
	pv[1].param = AL_MASTER_CLOCK;
	pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
	if (alSetParams(AL_DEFAULT_OUTPUT, pv, 2) < 0) {
		fprintf(stderr, "ERROR: libaudio setparams failed: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}

	// TODO:  list all supported sample formats?

	// Set AudioStatus again because we now know more about the sound
	// system's capabilities
	set_audio_status_format();

	// Compute sound buffer size and libaudio refill point

	config = alGetConfig(port);
	audio_frames_per_block = alGetQueueSize(config);
	if (audio_frames_per_block < 0) {
		fprintf(stderr, "ERROR: couldn't get queue size: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}
	D(bug("alGetQueueSize %d\n", audio_frames_per_block));

	alZeroFrames(port, audio_frames_per_block);	// so we don't underflow

	// Put a limit on the Mac sound buffer size, to decrease delay
	if (audio_frames_per_block > 2048)
		audio_frames_per_block = 2048;
	// Try to keep the buffer pretty full.  5000 samples of slack works well.
	sound_buffer_fill_point = alGetQueueSize(config) - 5000;
	if (sound_buffer_fill_point < 0)
		sound_buffer_fill_point = alGetQueueSize(config) / 3;
	D(bug("fill point %d\n", sound_buffer_fill_point));

	sound_buffer_size = (AudioStatus.sample_size >> 3) * AudioStatus.channels * audio_frames_per_block;

	// Get a file descriptor we can select() on

	audio_fd = alGetFD(port);
	if (audio_fd < 0) {
		fprintf(stderr, "ERROR: couldn't get libaudio file descriptor: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}

	return true;
}


/*
 *  Initialization
 */

void AudioInit(void)
{
	// Init audio status (defaults) and feature flags
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

	// Try to open audio library
	if (!audio_init_al())
			return;

	// Init semaphore
	if (sem_init(&audio_irq_done_sem, 0, 0) < 0)
		return;
	sem_inited = true;

	// Start streaming thread
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
	// Stop stream and delete semaphore
	if (stream_thread_active) {
		stream_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(stream_thread);
#endif
		pthread_join(stream_thread, NULL);
		stream_thread_active = false;
	}
	if (sem_inited)
		sem_destroy(&audio_irq_done_sem);

	// Close audio library
	alClosePort(port);
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
	int16 *last_buffer = new int16[sound_buffer_size / 2];
	fd_set audio_fdset;
	int    numfds, was_error;

	numfds = audio_fd + 1;
	FD_ZERO(&audio_fdset);

	while (!stream_thread_cancel) {
		if (AudioStatus.num_sources) {

			// Trigger audio interrupt to get new buffer
			D(bug("stream: triggering irq\n"));
			SetInterruptFlag(INTFLAG_AUDIO);
			TriggerInterrupt();
			D(bug("stream: waiting for ack\n"));
			sem_wait(&audio_irq_done_sem);
			D(bug("stream: ack received\n"));

			uint32 apple_stream_info;	// Mac address of SoundComponentData struct describing next buffer
			// Get size of audio data
			apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);

			if (apple_stream_info) {
				int work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
				D(bug("stream: work_size %d\n", work_size));
				if (work_size > sound_buffer_size)
					work_size = sound_buffer_size;
				if (work_size == 0)
					goto silence;

				// Send data to audio library
				if (work_size == sound_buffer_size)
					alWriteFrames(port, Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer)), audio_frames_per_block);
				else {
					// Last buffer
					Mac2Host_memcpy(last_buffer, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
					memset((uint8 *)last_buffer + work_size, silence_byte, sound_buffer_size - work_size);
					alWriteFrames(port, last_buffer, audio_frames_per_block);
				}
				D(bug("stream: data written\n"));
			} else
				goto silence;

		} else {

			// Audio not active, play silence
		silence:	// D(bug("stream: silence\n"));
			alZeroFrames(port, audio_frames_per_block);
		}

		// Wait for fill point to be reached (may be immediate)

		if (alSetFillPoint(port, sound_buffer_fill_point) < 0) {
			fprintf(stderr, "ERROR: alSetFillPoint failed: %s\n",
					alGetErrorString(oserror()));
			// Should stop the audio here....
		}

		do {
			errno = 0;
			FD_SET(audio_fd, &audio_fdset);
			was_error = select(numfds, NULL, &audio_fdset, NULL, NULL);
		} while(was_error < 0 && (errno == EINTR));
		if (was_error < 0) {
			fprintf(stderr, "ERROR: select returned %d, errno %d\n",
					was_error, errno);
			// Should stop audio here....
		}
	}
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
