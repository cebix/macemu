/*
 *  timer.cpp - Time Manager emulation
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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
 * TODO: Prime(0)
 */

#include "sysdeps.h"
#include "timer.h"
#include "macos_util.h"
#include "main.h"
#include "cpu_emulation.h"

#define DEBUG 0
#include "debug.h"


#if __BEOS__
#define PRECISE_TIMING 1
#else
#define PRECISE_TIMING 0
#endif

#define TM_QUEUE 0			// Enable TMQueue management (doesn't work)


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

#if PRECISE_TIMING
static thread_id timer_thread = -1;
static bool thread_active = true;
static volatile tm_time_t wakeup_time = 0x7fffffffffffffff;
static sem_id wakeup_time_sem = -1;
static int32 timer_func(void *arg);
#endif


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

#if PRECISE_TIMING
	// Start timer thread
	wakeup_time_sem = create_sem(1, "Wakeup Time");
	timer_thread = spawn_thread(timer_func, "Time Manager", B_REAL_TIME_PRIORITY, NULL);
	resume_thread(timer_thread);
#endif
}


/*
 *  Exit Time Manager
 */

void TimerExit(void)
{
#if PRECISE_TIMING
	// Quit timer thread
	if (timer_thread > 0) {
		status_t l;
		thread_active = false;
		suspend_thread(timer_thread);
		resume_thread(timer_thread);
		wait_for_thread(timer_thread, &l);
		delete_sem(wakeup_time_sem);
	}
#endif
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
	WriteMacInt16((uint32)tm + qType, ReadMacInt16((uint32)tm + qType) & 0x1fff | (trap << 4) & 0x6000);
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
		printf("WARNING: RmvTime(%08lx): Descriptor not found\n", tm);
		return 0;
	}

	// Task active?
#if PRECISE_TIMING
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
#endif
	if (ReadMacInt16(tm + qType) & 0x8000) {

		// Yes, make task inactive and remove it from the Time Manager queue
		WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) & 0x7fff);
		dequeue_tm(tm);
#if PRECISE_TIMING
		// Look for next task to be called and set wakeup_time
		wakeup_time = 0x7fffffffffffffff;
		for (int j=0; j<NUM_DESCS; j++) {
			if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
				if (desc[j].wakeup < wakeup_time)
					wakeup_time = desc[j].wakeup;
		}
#endif

		// Compute remaining time
		tm_time_t remaining, current;
		timer_current_time(current);
		timer_sub_time(remaining, desc[i].wakeup, current);
		WriteMacInt32(tm + tmCount, timer_host2mac_time(remaining));
	} else
		WriteMacInt32(tm + tmCount, 0);
	D(bug(" tmCount %ld\n", ReadMacInt32(tm + tmCount)));
#if PRECISE_TIMING
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif

	// Free descriptor
	free_desc(i);
	return 0;
}


/*
 *  Start timer task
 */

int16 PrimeTime(uint32 tm, int32 time)
{
	D(bug("PrimeTime %08lx, time %ld\n", tm, time));

	// Find descriptor
	int i = find_desc(tm);
	if (i < 0) {
		printf("FATAL: PrimeTime(): Descriptor not found\n");
		return 0;
	}

	// Convert delay time
	tm_time_t delay;
	timer_mac2host_time(delay, time);

	// Extended task?
	if (ReadMacInt16(tm + qType) & 0x4000) {

		// Yes, tmWakeUp set?
		if (ReadMacInt32(tm + tmWakeUp)) {

			//!! PrimeTime(0) means continue previous delay
			// (save wakeup time in RmvTime?)
			if (time == 0) {
				printf("FATAL: Unsupported PrimeTime(0)\n");
				return 0;
			}

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
		tm_time_t now;
		timer_current_time(now);
		timer_add_time(desc[i].wakeup, now, delay);
	}

	// Make task active and enqueue it in the Time Manager queue
#if PRECISE_TIMING
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
#endif
	WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) | 0x8000);
	enqueue_tm(tm);
#if PRECISE_TIMING
	// Look for next task to be called and set wakeup_time
	wakeup_time = 0x7fffffffffffffff;
	for (int j=0; j<NUM_DESCS; j++) {
		if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
			if (desc[j].wakeup < wakeup_time)
				wakeup_time = desc[j].wakeup;
	}
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif
	return 0;
}


#if PRECISE_TIMING
/*
 *  Time Manager thread
 */

static int32 timer_func(void *arg)
{
	while (thread_active) {

		// Wait until time specified by wakeup_time
		snooze_until(wakeup_time, B_SYSTEM_TIMEBASE);

		while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
		if (wakeup_time < system_time()) {

			// Timer expired, trigger interrupt
			wakeup_time = 0x7fffffffffffffff;
			SetInterruptFlag(INTFLAG_TIMER);
			TriggerInterrupt();
		}
		release_sem(wakeup_time_sem);
	}
	return 0;
}
#endif


/*
 *  Timer interrupt function (executed as part of 60Hz interrupt)
 */

void TimerInterrupt(void)
{
//	D(bug("TimerIRQ\n"));

	// Look for active TMTasks that have expired
	tm_time_t now;
	timer_current_time(now);
	for (int i=0; i<NUM_DESCS; i++)
		if (desc[i].in_use) {
			uint32 tm = desc[i].task;
			if ((ReadMacInt16(tm + qType) & 0x8000) && timer_cmp_time(desc[i].wakeup, now) <= 0) {

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
					Execute68k(r.a[0], &r);
					D(bug(" returned from TimeTask\n"));
				}
			}
		}

#if PRECISE_TIMING
	// Look for next task to be called and set wakeup_time
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
	wakeup_time = 0x7fffffffffffffff;
	for (int j=0; j<NUM_DESCS; j++) {
		if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
			if (desc[j].wakeup < wakeup_time)
				wakeup_time = desc[j].wakeup;
	}
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif
}
