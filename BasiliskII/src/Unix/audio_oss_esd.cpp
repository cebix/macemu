/*
 *  audio_oss_esd.cpp - Audio support, implementation for OSS and ESD (Linux and FreeBSD)
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

#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#ifdef __linux__
#include <linux/soundcard.h>
#endif

#ifdef __FreeBSD__
#include <machine/soundcard.h>
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#if ENABLE_ESD
#include <esd.h>
#endif

#define DEBUG 0
#include "debug.h"


// Supported sample rates, sizes and channels (defaults)
int audio_num_sample_rates = 1;
uint32 audio_sample_rates[] = {44100 << 16};
int audio_num_sample_sizes = 1;
uint16 audio_sample_sizes[] = {16};
int audio_num_channel_counts = 1;
uint16 audio_channel_counts[] = {2};

// Constants
#define DSP_NAME "/dev/dsp"

// Global variables
static int audio_fd = -1;							// fd of /dev/dsp or ESD
static int mixer_fd = -1;							// fd of /dev/mixer
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
	AudioStatus.sample_rate = audio_sample_rates[0];
	AudioStatus.sample_size = audio_sample_sizes[0];
	AudioStatus.channels = audio_channel_counts[0];
}

// Init using /dev/dsp, returns false on error
bool audio_init_dsp(void)
{
	printf("Using " DSP_NAME " audio output\n");

	// Get supported sample formats
	unsigned long format;
	ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &format);
	if ((format & (AFMT_U8 | AFMT_S16_BE | AFMT_S16_LE)) == 0) {
		WarningAlert(GetString(STR_AUDIO_FORMAT_WARN));
		close(audio_fd);
		audio_fd = -1;
		return false;
	}
	if (format & (AFMT_S16_BE | AFMT_S16_LE)) {
		audio_sample_sizes[0] = 16;
		silence_byte = 0;
	} else {
		audio_sample_sizes[0] = 8;
		silence_byte = 0x80;
	}
	if (!(format & AFMT_S16_BE))
		little_endian = true;

	// Set DSP parameters
	format = audio_sample_sizes[0] == 8 ? AFMT_U8 : (little_endian ? AFMT_S16_LE : AFMT_S16_BE);
	ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
	int frag = 0x0004000c;		// Block size: 4096 frames
	ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag);
	int stereo = (audio_channel_counts[0] == 2);
	ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo);
	int rate = audio_sample_rates[0] >> 16;
	ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate);
	audio_sample_rates[0] = rate << 16;

	// Set AudioStatus again because we now know more about the sound
	// system's capabilities
	set_audio_status_format();

	// Get sound buffer size
	ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &audio_frames_per_block);
	D(bug("DSP_GETBLKSIZE %d\n", audio_frames_per_block));
	sound_buffer_size = (AudioStatus.sample_size >> 3) * AudioStatus.channels * audio_frames_per_block;
	return true;
}

// Init using ESD, returns false on error
bool audio_init_esd(void)
{
#if ENABLE_ESD
	printf("Using ESD audio output\n");

	// ESD audio format
	esd_format_t format = ESD_STREAM | ESD_PLAY;
	if (AudioStatus.sample_size == 8)
		format |= ESD_BITS8;
	else
		format |= ESD_BITS16;
	if (AudioStatus.channels == 1)
		format |= ESD_MONO;
	else
		format |= ESD_STEREO;

#if WORDS_BIGENDIAN
	little_endian = false;
#else
	little_endian = true;
#endif
	silence_byte = 0;	// Is this correct for 8-bit mode?

	// Open connection to ESD server
	audio_fd = esd_play_stream(format, AudioStatus.sample_rate >> 16, NULL, NULL);
	if (audio_fd < 0) {
		char str[256];
		sprintf(str, GetString(STR_NO_ESD_WARN), strerror(errno));
		WarningAlert(str);
		return false;
	}

	// Sound buffer size = 4096 frames
	audio_frames_per_block = 4096;
	sound_buffer_size = (AudioStatus.sample_size >> 3) * AudioStatus.channels * audio_frames_per_block;
	return true;
#else
	ErrorAlert("Basilisk II has been compiled with ESD support disabled.");
	return false;
#endif
}

void AudioInit(void)
{
	char str[256];

	// Init audio status (defaults) and feature flags
	set_audio_status_format();
	AudioStatus.mixer = 0;
	AudioStatus.num_sources = 0;
	audio_component_flags = cmpWantsRegisterMessage | kStereoOut | k16BitOut;

	// Sound disabled in prefs? Then do nothing
	if (PrefsFindBool("nosound"))
		return;

	// Try to open /dev/dsp
	audio_fd = open(DSP_NAME, O_WRONLY);
	if (audio_fd < 0) {
#if ENABLE_ESD
		if (!audio_init_esd())
			return;
#else
		sprintf(str, GetString(STR_NO_AUDIO_DEV_WARN), DSP_NAME, strerror(errno));
		WarningAlert(str);
		return;
#endif
	} else
		if (!audio_init_dsp())
			return;

	// Try to open /dev/mixer
	mixer_fd = open("/dev/mixer", O_RDWR);
	if (mixer_fd < 0)
		printf("WARNING: Cannot open /dev/mixer (%s)", strerror(errno));

	// Init semaphore
	if (sem_init(&audio_irq_done_sem, 0, 0) < 0)
		return;
	sem_inited = true;

	// Start streaming thread
	pthread_attr_init(&stream_thread_attr);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	if (geteuid() == 0) {
		pthread_attr_setinheritsched(&stream_thread_attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&stream_thread_attr, SCHED_FIFO);
		struct sched_param fifo_param;
		fifo_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + sched_get_priority_max(SCHED_FIFO)) / 2;
		pthread_attr_setschedparam(&stream_thread_attr, &fifo_param);
	}
#endif
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

	// Close /dev/dsp
	if (audio_fd > 0)
		close(audio_fd);

	// Close /dev/mixer
	if (mixer_fd > 0)
		close(mixer_fd);
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

static uint32 apple_stream_info;	// Mac address of SoundComponentData struct describing next buffer

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
