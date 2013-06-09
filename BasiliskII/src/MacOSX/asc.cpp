#include <stdio.h>
#include <asc.h>
#include "MacOSX_sound_if.h"

static OSXsoundOutput *soundOutput = NULL;

static int audioInt(void);

bool asc_init(int32 sample_rate) {
	if(soundOutput != NULL) {
		delete soundOutput;
		soundOutput = NULL;
	}

	soundOutput = new OSXsoundOutput();
	soundOutput->start(8, 1, sample_rate);
	soundOutput->setCallback(audioInt);

	return true;
}

bool asc_process_samples(const uae_u8 *samples, int count) {
	if(soundOutput == NULL) {
		return false;
	}

	if(soundOutput->sendAudioBuffer((const void *)samples, count) != 0) {
		return false;
	}

	return true;
}

static int audioInt(void) {
	asc_callback();

	return 0;
}

int32 asc_get_buffer_size() {
	if(soundOutput == NULL) {
		return -1;
	}

	return (int32) soundOutput->bufferSizeFrames();
}

void asc_stop() {
	delete soundOutput;
	soundOutput = NULL;
}
