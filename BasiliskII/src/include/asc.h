#ifndef ASC_H
#define ASC_H

#include "sysdeps.h"
#include "cpu_emulation.h"

bool asc_init(int32 sample_rate);
bool asc_process_samples(const uae_u8 *samples, int count);
int32 asc_get_buffer_size();
void asc_callback();
void asc_stop();

#endif
