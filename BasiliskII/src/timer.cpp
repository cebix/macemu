/*
 *  timer.cpp - Time Manager emulation
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Processes, chapter 3 "Time Manager"
 *    Technote 1063: "Inside Macintosh: Processes: Time Manager Addenda"
 */

#include <stdio.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "timer.h"

#define DEBUG 0
#include "debug.h"


// Set this to 1 to enable TMQueue management (doesn't work)
#define TM_QUEUE 0


// Definitions for Time Manager
enum {	// TMTask struct
	tmAddr = 6,
	tmCount = 10,
	tmWakeUp = 14,
	tmReserved = 18
};


// Array of additional info for each installed TMTask
struct TMDesc {
	uint32 task;		// Mac address of associated TMTask
	tm_time_t wakeup;	// Time this task is scheduled for execution
	bool in_use;		// Flag: descriptor in use
};

const int NUM_DESCS = 64;		// Maximum number of descriptors
static TMDesc desc[NUM_DESCS];


/*
 *  Allocate descriptor for given TMTask in list
 */

static int alloc_desc(uint32 tm)
{
	// Search for first free descriptor
	for (int i=0; i<NUM_DESCS; i++)
		if (!desc[i].in_use) {
			desc[i].task = tm;
			desc[i].in_use = true;
			return i;
		}
	return -1;
}


/*
 *  Free descriptor in list
 */

inline static void free_desc(int i)
{
	desc[i].in_use = false;
}


/*
 *  Find descriptor associated with given TMTask
 */

inline static int find_desc(uint32 tm)
{
	for (int i=0; i<NUM_DESCS; i++)
		if (desc[i].in_use && desc[i].task == tm)
			return i;
	return -1;
}


/*
 *  Enqueue task in Time Manager queue
 */

static void enqueue_tm(uint32 tm)
{
#if TM_QUEUE
	uint32 tm_var = ReadMacInt32(0xb30);
	WriteMacInt32(tm + qLink, ReadMacInt32(tm_var));
	WriteMacInt32(tm_var, tm);
#endif
}


/*
 *  Remove task from Time Manager queue
 */

static void dequeue_tm(uint32 tm)
{
#if TM_QUEUE
	uint32 p = ReadMacInt32(0xb30);
	while (p) {
		uint32 next = ReadMacInt32(p + qLink);
		if (next == tm) {
			WriteMacInt32(p + qLink, ReadMacInt32(next + qLink));
			return;
		}
	}
#endif
}


/*
 *  Initialize Time Manager
 */

void TimerInit(void)
{
	// Mark all descriptors as inactive
	for (int i=0; i<NUM_DESCS; i++)
		free_desc(i);
}


/*
 *  Exit Time Manager
 */

void TimerExit(void)
{
}


/*
 *  Emulator reset, remove all timer tasks
 */

void TimerReset(void)
{
	// Mark all descriptors as inactive
	for (int i=0; i<NUM_DESCS; i++)
		free_desc(i);
}


/*
 *  Insert timer task
 */

int16 InsTime(uint32 tm, uint16 trap)
{
	D(bug("InsTime %08lx, trap %04x\n", tm, trap));
	WriteMacInt16(tm + qType, (ReadMacInt16(tm + qType) & 0x1fff) | ((trap << 4) & 0x6000));
	if (find_desc(tm) >= 0)
		printf("WARNING: InsTime(): Task re-inserted\n");
	else {
		int i = alloc_desc(tm);
		if (i < 0)
			printf("FATAL: InsTime(): No free Time Manager descriptor\n");
	}
	return 0;
}


/*
 *  Remove timer task
 */

int16 RmvTime(uint32 tm)
{
	D(bug("RmvTime %08lx\n", tm));

	// Find descriptor
	int i = find_desc(tm);
	if (i < 0) {
		printf("WARNING: RmvTime(%08x): Descriptor not found\n", tm);
		return 0;
	}

	// Task active?
	if (ReadMacInt16(tm + qType) & 0x8000) {

		// Yes, make task inactive and remove it from the Time Manager queue
		WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) & 0x7fff);
		dequeue_tm(tm);

		// Compute remaining time
		tm_time_t remaining, current;
		timer_current_time(current);
		timer_sub_time(remaining, desc[i].wakeup, current);
		WriteMacInt32(tm + tmCount, timer_host2mac_time(remaining));
	} else
		WriteMacInt32(tm + tmCount, 0);
	D(bug(" tmCount %d\n", ReadMacInt32(tm + tmCount)));

	// Free descriptor
	free_desc(i);
	return 0;
}


/*
 *  Start timer task
 */

int16 PrimeTime(uint32 tm, int32 time)
{
	D(bug("PrimeTime %08x, time %d\n", tm, time));

	// Find descriptor
	int i = find_desc(tm);
	if (i < 0) {
		printf("FATAL: PrimeTime(): Descriptor not found\n");
		return 0;
	}

	// Extended task?
	if (ReadMacInt16(tm + qType) & 0x4000) {

		// Convert delay time
		tm_time_t delay;
		timer_mac2host_time(delay, time);

		// Yes, tmWakeUp set?
		if (ReadMacInt32(tm + tmWakeUp)) {

			//!! PrimeTime(0) means continue previous delay
			// (save wakeup time in RmvTime?)
			if (time == 0)
				printf("WARNING: Unsupported PrimeTime(0)\n");

			// Yes, calculate wakeup time relative to last scheduled time
			tm_time_t wakeup;
			timer_add_time(wakeup, desc[i].wakeup, delay);
			desc[i].wakeup = wakeup;

		} else {

			// No, calculate wakeup time relative to current time
			tm_time_t now;
			timer_current_time(now);
			timer_add_time(desc[i].wakeup, now, delay);
		}

		// Set tmWakeUp to indicate that task was scheduled
		WriteMacInt32(tm + tmWakeUp, 0x12345678);

	} else {

		// Not extended task, calculate wakeup time relative to current time
		tm_time_t delay;
		timer_mac2host_time(delay, time);
		timer_current_time(desc[i].wakeup);
		timer_add_time(desc[i].wakeup, desc[i].wakeup, delay);
	}

	// Make task active and enqueue it in the Time Manager queue
	WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) | 0x8000);
	enqueue_tm(tm);
	return 0;
}


/*
 *  Timer interrupt function (executed as part of 60Hz interrupt)
 */

void TimerInterrupt(void)
{
	// Look for active TMTasks that have expired
	tm_time_t now;
	timer_current_time(now);
	for (int i=0; i<NUM_DESCS; i++)
		if (desc[i].in_use) {
			uint32 tm = desc[i].task;
			if ((ReadMacInt16(tm + qType) & 0x8000) && timer_cmp_time(desc[i].wakeup, now) < 0) {

				// Found one, mark as inactive and remove it from the Time Manager queue
				WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) & 0x7fff);
				dequeue_tm(tm);

				// Call timer function
				uint32 addr = ReadMacInt32(tm + tmAddr);
				if (addr) {
					D(bug("Calling TimeTask %08lx, addr %08lx\n", tm, addr));
					M68kRegisters r;
					r.a[0] = addr;
					r.a[1] = tm;
					Execute68k(addr, &r);
				}
			}
		}
}
