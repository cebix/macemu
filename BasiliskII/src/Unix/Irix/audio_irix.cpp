/*
 *  audio_irix.cpp - Audio support, SGI Irix implementation
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


// The currently selected audio parameters (indices in audio_sample_rates[]
// etc. vectors)
static int audio_sample_rate_index = 0;
static int audio_sample_size_index = 0;
static int audio_channel_count_index = 0;

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

static bool current_main_mute = false;				// Flag: output muted
static bool current_speaker_mute = false;			// Flag: speaker muted
static uint32 current_main_volume = 0;				// Output volume
static uint32 current_speaker_volume = 0;			// Speaker volume

// IRIX libaudio control structures
static ALconfig config;
static ALport   port;


// Prototypes
static void *stream_func(void *arg);
static uint32 read_volume(void);
static bool read_mute(void);
static void set_mute(bool mute);


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

bool open_audio(void)
{
	ALpv     pv[2];

	printf("Using libaudio audio output\n");

	// Get supported sample formats

	if (audio_sample_sizes.empty()) {
		// All sample sizes are supported
		audio_sample_sizes.push_back(8);
		audio_sample_sizes.push_back(16);

		// Assume at least two channels are supported.  Some IRIX boxes
		// can do 4 or more...  MacOS only handles up to 2.
		audio_channel_counts.push_back(1);
		audio_channel_counts.push_back(2);

		if (audio_sample_sizes.empty() || audio_channel_counts.empty()) {
			WarningAlert(GetString(STR_AUDIO_FORMAT_WARN));
			alClosePort(port);
			audio_fd = -1;
			return false;
		}

		audio_sample_rates.push_back( 8000 << 16);
		audio_sample_rates.push_back(11025 << 16);
		audio_sample_rates.push_back(22050 << 16);
		audio_sample_rates.push_back(44100 << 16);

		// Default to highest supported values
		audio_sample_rate_index = audio_sample_rates.size() - 1;
		audio_sample_size_index = audio_sample_sizes.size() - 1;
		audio_channel_count_index = audio_channel_counts.size() - 1;
	}

	// Set the sample format

	D(bug("Size %d, channels %d, rate %d\n",
		  audio_sample_sizes[audio_sample_size_index],
		  audio_channel_counts[audio_channel_count_index],
		  audio_sample_rates[audio_sample_rate_index] >> 16));
	config = alNewConfig();
	alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP);
	if (audio_sample_sizes[audio_sample_size_index] == 8) {
		alSetWidth(config, AL_SAMPLE_8);
	}
	else {
		alSetWidth(config, AL_SAMPLE_16);
	}
	alSetChannels(config, audio_channel_counts[audio_channel_count_index]);
	alSetDevice(config, AL_DEFAULT_OUTPUT); // Allow selecting via prefs?
	
	// Try to open the audio library

	port = alOpenPort("BasiliskII", "w", config);
	if (port == NULL) {
		fprintf(stderr, "ERROR: Cannot open audio port: %s\n", 
				alGetErrorString(oserror()));
		WarningAlert(GetString(STR_NO_AUDIO_WARN));
		return false;
	}
	
	// Set the sample rate

	pv[0].param = AL_RATE;
	pv[0].value.ll = alDoubleToFixed(audio_sample_rates[audio_sample_rate_index] >> 16);
	pv[1].param = AL_MASTER_CLOCK;
	pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
	if (alSetParams(AL_DEFAULT_OUTPUT, pv, 2) < 0) {
		fprintf(stderr, "ERROR: libaudio setparams failed: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}

	// Compute sound buffer size and libaudio refill point

	config = alGetConfig(port);
	audio_frames_per_block = alGetQueueSize(config);
	if (audio_frames_per_block < 0) {
		fprintf(stderr, "ERROR: couldn't get queue size: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}
	D(bug("alGetQueueSize %d, width %d, channels %d\n",
		  audio_frames_per_block,
		  alGetWidth(config),
		  alGetChannels(config)));

	// Put a limit on the Mac sound buffer size, to decrease delay
#define AUDIO_BUFFER_MSEC 50	// milliseconds of sound to buffer
	int target_frames_per_block = 
		(audio_sample_rates[audio_sample_rate_index] >> 16) *
		AUDIO_BUFFER_MSEC / 1000;
	if (audio_frames_per_block > target_frames_per_block)
		audio_frames_per_block = target_frames_per_block;
	D(bug("frames per block %d\n", audio_frames_per_block));

	alZeroFrames(port, audio_frames_per_block);	// so we don't underflow

	// Try to keep the buffer pretty full
	sound_buffer_fill_point = alGetQueueSize(config) - 
		2 * audio_frames_per_block;
	if (sound_buffer_fill_point < 0)
		sound_buffer_fill_point = alGetQueueSize(config) / 3;
	D(bug("fill point %d\n", sound_buffer_fill_point));

	sound_buffer_size = (audio_sample_sizes[audio_sample_size_index] >> 3) *
		audio_channel_counts[audio_channel_count_index] * 
		audio_frames_per_block;
	set_audio_status_format();

	// Get a file descriptor we can select() on

	audio_fd = alGetFD(port);
	if (audio_fd < 0) {
		fprintf(stderr, "ERROR: couldn't get libaudio file descriptor: %s\n",
				alGetErrorString(oserror()));
		alClosePort(port);
		return false;
	}

	// Initialize volume, mute settings
	current_main_volume = current_speaker_volume = read_volume();
	current_main_mute = current_speaker_mute = read_mute();


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
		stream_thread_cancel = false;
	}

	// Close audio library
	alClosePort(port);

	audio_open = false;
}

void AudioExit(void)
{
	// Close audio device
	close_audio();

	// Delete semaphore
	if (sem_inited) {
		sem_destroy(&audio_irq_done_sem);
		sem_inited = false;
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
	int32 *last_buffer = new int32[sound_buffer_size / 4];
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

			// Get size of audio data
			uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
			if (!current_main_mute &&
				!current_speaker_mute &&
				apple_stream_info) {
				int work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
				D(bug("stream: work_size %d\n", work_size));
				if (work_size > sound_buffer_size)
					work_size = sound_buffer_size;
				if (work_size == 0)
					goto silence;

				// Send data to audio library.  Convert 8-bit data
				// unsigned->signed, using same algorithm as audio_amiga.cpp.
				// It works fine for 8-bit mono, but not stereo.
				if (AudioStatus.sample_size == 8) {
					uint32 *p = (uint32 *)Mac2HostAddr(ReadMacInt32(apple_stream_info + scd_buffer));
					uint32 *q = (uint32 *)last_buffer;
					int r = work_size >> 2;
					// XXX not quite right....
					while (r--)
						*q++ = *p++ ^ 0x80808080;
					if (work_size != sound_buffer_size)
						memset((uint8 *)last_buffer + work_size, silence_byte, sound_buffer_size - work_size);
					alWriteFrames(port, last_buffer, audio_frames_per_block);
				}
				else if (work_size == sound_buffer_size)
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
 * Read or set the current output volume using the audio library
 */

static uint32 read_volume(void)
{
	ALpv x[2];
	ALfixed gain[8];
	double maxgain, mingain;
	ALparamInfo pi;
	uint32 ret = 0x01000100;	// default, maximum value
	int dev = alGetDevice(config);

	// Fetch the maximum and minimum gain settings

	alGetParamInfo(dev, AL_GAIN, &pi);
	maxgain = alFixedToDouble(pi.max.ll);
	mingain = alFixedToDouble(pi.min.ll);
//	printf("maxgain = %lf dB, mingain = %lf dB\n", maxgain, mingain);

	// Get the current gain values

	x[0].param = AL_GAIN;
	x[0].value.ptr = gain;
	x[0].sizeIn = sizeof(gain) / sizeof(gain[0]);
	x[1].param = AL_CHANNELS;
	if (alGetParams(dev, x, 2) < 0) {
		printf("alGetParams failed: %s\n", alGetErrorString(oserror()));
	}
	else {
		if (x[0].sizeOut < 0) {
			printf("AL_GAIN was an unrecognized parameter\n");
		}
		else {
			double v;
			uint32 left, right;

			// Left
			v = alFixedToDouble(gain[0]);
			if (v < mingain)
				v = mingain;	// handle gain == -inf
			v = (v - mingain) / (maxgain - mingain); // scale to 0..1
			left = (uint32)(v * (double)256); // convert to 8.8 fixed point

			// Right
			if (x[0].sizeOut <= 1) {	// handle a mono interface
				right = left;
			}
			else {
				v = alFixedToDouble(gain[1]);
				if (v < mingain)
					v = mingain; // handle gain == -inf
				v = (v - mingain) / (maxgain - mingain); // scale to 0..1
				right = (uint32)(v * (double)256); // convert to 8.8 fixed point
			}

			ret = (left << 16) | right;
		}
	}

	return ret;
}

static void set_volume(uint32 vol)
{
	ALpv x[1];
	ALfixed gain[2];			// left and right
	double maxgain, mingain;
	ALparamInfo pi;
	int dev = alGetDevice(config);

	// Fetch the maximum and minimum gain settings

	alGetParamInfo(dev, AL_GAIN, &pi);
	maxgain = alFixedToDouble(pi.max.ll);
	mingain = alFixedToDouble(pi.min.ll);		

	// Set the new gain values

	x[0].param = AL_GAIN;
	x[0].value.ptr = gain;
	x[0].sizeIn = sizeof(gain) / sizeof(gain[0]);

	uint32 left = vol >> 16;
	uint32 right = vol & 0xffff;
  	double lv, rv;

	if (left == 0 && pi.specialVals & AL_NEG_INFINITY_BIT) {
		lv = AL_NEG_INFINITY;
	}
	else {
		lv = ((double)left / 256) * (maxgain - mingain) + mingain;
	}

	if (right == 0 && pi.specialVals & AL_NEG_INFINITY_BIT) {
		rv = AL_NEG_INFINITY;
	}
	else {
		rv = ((double)right / 256) * (maxgain - mingain) + mingain;
	}

	D(bug("set_volume:  left=%lf dB, right=%lf dB\n", lv, rv));

	gain[0] = alDoubleToFixed(lv);
	gain[1] = alDoubleToFixed(rv);

	if (alSetParams(dev, x, 1) < 0) {
		printf("alSetParams failed: %s\n", alGetErrorString(oserror()));
	}
}


/*
 * Read or set the mute setting using the audio library
 */

static bool read_mute(void)
{
	bool ret;
	int dev = alGetDevice(config);
	ALpv x;
	x.param = AL_MUTE;

	if (alGetParams(dev, &x, 1) < 0) {
		printf("alSetParams failed: %s\n", alGetErrorString(oserror()));
		return current_main_mute; // Or just return false?
	}

	ret = x.value.i;

	D(bug("read_mute:  mute=%d\n", ret));
	return ret;
}

static void set_mute(bool mute)
{
	D(bug("set_mute: mute=%ld\n", mute));

	int dev = alGetDevice(config);
	ALpv x;
	x.param = AL_MUTE;
	x.value.i = mute;

	if (alSetParams(dev, &x, 1) < 0) {
		printf("alSetParams failed: %s\n", alGetErrorString(oserror()));
	}
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
	D(bug("audio_get_main_mute:  mute=%ld\n", current_main_mute));

	return current_main_mute;
}

uint32 audio_get_main_volume(void)
{
	uint32 ret = current_main_volume;

	D(bug("audio_get_main_volume:  vol=0x%x\n", ret));

	return ret;
}

bool audio_get_speaker_mute(void)
{
	D(bug("audio_get_speaker_mute:  mute=%ld\n", current_speaker_mute));

	return current_speaker_mute;
}

uint32 audio_get_speaker_volume(void)
{
	uint32 ret = current_speaker_volume;

	D(bug("audio_get_speaker_volume:  vol=0x%x\n", ret));

	return ret;
}

void audio_set_main_mute(bool mute)
{
	D(bug("audio_set_main_mute: mute=%ld\n", mute));

	if (mute != current_main_mute) {
		current_main_mute = mute;
	}

	set_mute(current_main_mute);
}

void audio_set_main_volume(uint32 vol)
{

	D(bug("audio_set_main_volume:  vol=%x\n", vol));

	current_main_volume = vol;

	set_volume(vol);
}

void audio_set_speaker_mute(bool mute)
{
	D(bug("audio_set_speaker_mute: mute=%ld\n", mute));

	if (mute != current_speaker_mute) {
		current_speaker_mute = mute;
	}

	set_mute(current_speaker_mute);
}

void audio_set_speaker_volume(uint32 vol)
{
	D(bug("audio_set_speaker_volume:  vol=%x\n", vol));

	current_speaker_volume = vol;

	set_volume(vol);
}
