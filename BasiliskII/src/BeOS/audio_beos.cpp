/*
 *  audio_beos.cpp - Audio support, BeOS implementation
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

#include "sysdeps.h"

#include <KernelKit.h>
#include <MediaKit.h>

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "audio.h"
#include "audio_defs.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static int audio_irq_done_sem = -1;	// Signal from interrupt to streaming thread: data block read
static BSoundPlayer *the_player;

// Prototypes
static void playbuffer_func(void *arg, void *buf, size_t size, const media_raw_audio_format &format);


/*
 *  Audio manager thread (for calling media kit functions;
 *  this is not safe under R4 when running on the MacOS stack in kernel space)
 */

// Message constants
const uint32 MSG_QUIT_AUDIO_MANAGER = 'quit';
const uint32 MSG_ENTER_STREAM = 'entr';
const uint32 MSG_EXIT_STREAM = 'exit';
const uint32 MSG_GET_VOLUME = 'getv';
const uint32 MSG_SET_VOLUME = 'setv';

static thread_id am_thread = -1;
static sem_id am_done_sem = -1;

static volatile float am_volume;

static status_t audio_manager(void *arg)
{
	for (;;) {

		// Receive message
		thread_id sender;
		uint32 code = receive_data(&sender, NULL, 0);
		D(bug("Audio manager received %08lx\n", code));
		switch (code) {
			case MSG_QUIT_AUDIO_MANAGER:
				return 0;

			case MSG_ENTER_STREAM:
				the_player->Start();
				break;

			case MSG_EXIT_STREAM:
				the_player->Stop();
				break;

			case MSG_GET_VOLUME:
				am_volume = the_player->Volume();
				break;

			case MSG_SET_VOLUME:
				the_player->SetVolume(am_volume);
				break;
		}

		// Acknowledge
		release_sem(am_done_sem);
	}
}


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

	// Init semaphores
	audio_irq_done_sem = create_sem(0, "Audio IRQ Done");
	am_done_sem = create_sem(0, "Audio Manager Done");

	// Start audio manager thread
	am_thread = spawn_thread(audio_manager, "Audio Manager", B_NORMAL_PRIORITY, NULL);
	resume_thread(am_thread);

	// Start stream
	media_raw_audio_format format;
	format.frame_rate = AudioStatus.sample_rate >> 16;
	format.channel_count = AudioStatus.channels;
	format.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_MEDIA_BIG_ENDIAN;
	audio_frames_per_block = 4096;
	size_t block_size = (AudioStatus.sample_size >> 3) * AudioStatus.channels * audio_frames_per_block;
	D(bug("AudioInit: block size %d\n", block_size));
	format.buffer_size = block_size;
	the_player = new BSoundPlayer(&format, "MacOS Audio", playbuffer_func, NULL, NULL);
	if (the_player->InitCheck() != B_NO_ERROR) {
		printf("FATAL: Cannot initialize BSoundPlayer\n");
		delete the_player;
		the_player = NULL;
		return;
	} else
		the_player->SetHasData(true);

	// Everything OK
	audio_open = true;
}


/*
 *  Deinitialization
 */

void AudioExit(void)
{
	// Stop stream
	if (the_player) {
		the_player->Stop();
		delete the_player;
		the_player = NULL;
	}

	// Stop audio manager
	if (am_thread > 0) {
		status_t l;
		send_data(am_thread, MSG_QUIT_AUDIO_MANAGER, NULL, 0);
		wait_for_thread(am_thread, &l);
	}

	// Delete semaphores
	delete_sem(am_done_sem);
	delete_sem(audio_irq_done_sem);
}


/*
 *  First source added, start audio stream
 */

void audio_enter_stream()
{
	while (send_data(am_thread, MSG_ENTER_STREAM, NULL, 0) == B_INTERRUPTED) ;
	while (acquire_sem(am_done_sem) == B_INTERRUPTED) ;
}


/*
 *  Last source removed, stop audio stream
 */

void audio_exit_stream()
{
	while (send_data(am_thread, MSG_EXIT_STREAM, NULL, 0) == B_INTERRUPTED) ;
	while (acquire_sem(am_done_sem) == B_INTERRUPTED) ;
}


/*
 *  Streaming function
 */

static uint32 apple_stream_info;	// Mac address of SoundComponentData struct describing next buffer

static void playbuffer_func(void *arg, void *buf, size_t size, const media_raw_audio_format &format)
{
	// Check if new buffer is available
	if (acquire_sem_etc(audio_irq_done_sem, 1, B_TIMEOUT, 0) == B_NO_ERROR) {

		// Get size of audio data
		D(bug("stream: new buffer present\n"));
		uint32 apple_stream_info = ReadMacInt32(audio_data + adatStreamInfo);
		if (apple_stream_info) {
			size_t work_size = ReadMacInt32(apple_stream_info + scd_sampleCount) * (AudioStatus.sample_size >> 3) * AudioStatus.channels;
			D(bug("stream: size %d, work_size %d\n", size, work_size));
			if (work_size > size)
				work_size = size;

			if (format.format != media_raw_audio_format::B_AUDIO_SHORT) {
				D(bug("Wrong audio format %04x\n", format.format));
				return;
			}

			// Place data into Media Kit buffer
			Mac2Host_memcpy(buf, ReadMacInt32(apple_stream_info + scd_buffer), work_size);
			if (work_size != size)
				memset((uint8 *)buf + work_size, 0, size - work_size);
		}

	} else
		memset(buf, 0, size);

	// Trigger audio interrupt to get new buffer
	if (AudioStatus.num_sources) {
		D(bug("stream: triggering irq\n"));
		SetInterruptFlag(INTFLAG_AUDIO);
		TriggerInterrupt();
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
	release_sem(audio_irq_done_sem);
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
 *  Get/set audio info
 */

bool audio_get_main_mute(void)
{
	return false;
}

uint32 audio_get_main_volume(void)
{
	if (audio_open) {
		while (send_data(am_thread, MSG_GET_VOLUME, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(am_done_sem) == B_INTERRUPTED) ;
		return int(am_volume * 256.0) * 0x00010001;
	} else
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
	if (audio_open) {
		am_volume = float((vol >> 16) + (vol & 0xffff)) / 512.0;
		while (send_data(am_thread, MSG_SET_VOLUME, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(am_done_sem) == B_INTERRUPTED) ;
	}
}

void audio_set_speaker_mute(bool mute)
{
}

void audio_set_speaker_volume(uint32 vol)
{
}
