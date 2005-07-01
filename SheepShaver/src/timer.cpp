/*
 *  timer.cpp - Time Manager emulation
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

#ifdef PRECISE_TIMING_POSIX
#include <pthread.h>
#include <semaphore.h>
#endif

#define DEBUG 0
#include "debug.h"


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
#ifdef PRECISE_TIMING_BEOS
static thread_id timer_thread = -1;
static bool thread_active = true;
static const tm_time_t wakeup_time_max = 0x7fffffffffffffff; 
static volatile tm_time_t wakeup_time = wakeup_time_max;
static sem_id wakeup_time_sem = -1;
static int32 timer_func(void *arg);
#endif
#ifdef PRECISE_TIMING_POSIX
static pthread_t timer_thread;
static bool timer_thread_active = false;
static volatile bool timer_thread_cancel = false;
static tm_time_t wakeup_time_max = { 0x7fffffff, 999999999 };
static tm_time_t wakeup_time = wakeup_time_max;
static pthread_mutex_t wakeup_time_lock = PTHREAD_MUTEX_INITIALIZER;
static void *timer_func(void *arg);
#endif
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
 *  Timer thread operations
 */

#ifdef PRECISE_TIMING_POSIX
const int SIGSUSPEND = SIGRTMIN + 6;
const int SIGRESUME  = SIGRTMIN + 7;
static struct sigaction sigsuspend_action;
static struct sigaction sigresume_action;

static int suspend_count = 0;
static pthread_mutex_t suspend_count_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t suspend_ack_sem;
static sigset_t suspend_handler_mask;

// Signal handler for suspended thread
static void sigsuspend_handler(int sig)
{
	sem_post(&suspend_ack_sem);
	sigsuspend(&suspend_handler_mask);
}

// Signal handler for resumed thread
static void sigresume_handler(int sig)
{
	/* simply trigger a signal to stop clock_nanosleep() */
}

// Initialize timer thread
static bool timer_thread_init(void)
{
	// Install suspend signal handler
	sigemptyset(&sigsuspend_action.sa_mask);
	sigaddset(&sigsuspend_action.sa_mask, SIGRESUME);
	sigsuspend_action.sa_handler = sigsuspend_handler;
	sigsuspend_action.sa_flags = SA_RESTART;
#ifdef HAVE_SIGNAL_SA_RESTORER
	sigsuspend_action.sa_restorer = NULL;
#endif
	if (sigaction(SIGSUSPEND, &sigsuspend_action, NULL) < 0)
		return false;

	// Install resume signal handler
	sigemptyset(&sigresume_action.sa_mask);
	sigresume_action.sa_handler = sigresume_handler;
	sigresume_action.sa_flags = SA_RESTART;
#ifdef HAVE_SIGNAL_SA_RESTORER
	sigresume_action.sa_restorer = NULL;
#endif
	if (sigaction(SIGRESUME, &sigresume_action, NULL) < 0)
		return false;

	// Initialize semaphore
	if (sem_init(&suspend_ack_sem, 0, 0) < 0)
		return false;

	// Initialize suspend_handler_mask, it excludes SIGRESUME
	if (sigfillset(&suspend_handler_mask) != 0)
		return false;
	if (sigdelset(&suspend_handler_mask, SIGRESUME) != 0)
		return false;

	// Create thread in running state
	suspend_count = 0;
	return (pthread_create(&timer_thread, NULL, timer_func, NULL) == 0);
}

// Kill timer thread
static void timer_thread_kill(void)
{
	timer_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
	pthread_cancel(timer_thread);
#endif
	pthread_join(timer_thread, NULL);
}

// Suspend timer thread
static void timer_thread_suspend(void)
{
	pthread_mutex_lock(&suspend_count_lock);
	if (suspend_count == 0) {
		suspend_count ++;
		if (pthread_kill(timer_thread, SIGSUSPEND) == 0)
			sem_wait(&suspend_ack_sem);
	}
	pthread_mutex_unlock(&suspend_count_lock);
}

// Resume timer thread
static void timer_thread_resume(void)
{
	pthread_mutex_lock(&suspend_count_lock);
	assert(suspend_count > 0);
	if (suspend_count == 1) {
		suspend_count = 0;
		pthread_kill(timer_thread, SIGRESUME);
	}
	pthread_mutex_unlock(&suspend_count_lock);
}
#endif


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
#ifdef PRECISE_TIMING_BEOS
	wakeup_time_sem = create_sem(1, "Wakeup Time");
	timer_thread = spawn_thread(timer_func, "Time Manager", B_REAL_TIME_PRIORITY, NULL);
	resume_thread(timer_thread);
#endif
#ifdef PRECISE_TIMING_POSIX
	timer_thread_active = timer_thread_init();
#endif
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
#ifdef PRECISE_TIMING_BEOS
		status_t l;
		thread_active = false;
		suspend_thread(timer_thread);
		resume_thread(timer_thread);
		wait_for_thread(timer_thread, &l);
		delete_sem(wakeup_time_sem);
#endif
#ifdef PRECISE_TIMING_POSIX
		timer_thread_kill();
#endif
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
#if PRECISE_TIMING_BEOS
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
#endif
#if PRECISE_TIMING_POSIX
	timer_thread_suspend();
	pthread_mutex_lock(&wakeup_time_lock);
#endif
	if (ReadMacInt16(tm + qType) & 0x8000) {

		// Yes, make task inactive and remove it from the Time Manager queue
		WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) & 0x7fff);
		dequeue_tm(tm);
#if PRECISE_TIMING
		// Look for next task to be called and set wakeup_time
		wakeup_time = wakeup_time_max;
		for (int j=0; j<NUM_DESCS; j++) {
			if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
				if (timer_cmp_time(desc[j].wakeup, wakeup_time) < 0)
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
#if PRECISE_TIMING_BEOS
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif
#if PRECISE_TIMING_POSIX
	pthread_mutex_unlock(&wakeup_time_lock);
	timer_thread_resume();
	assert(suspend_count == 0);
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
#if PRECISE_TIMING_BEOS
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
#endif
#if PRECISE_TIMING_POSIX
	timer_thread_suspend();
	pthread_mutex_lock(&wakeup_time_lock);
#endif
	WriteMacInt16(tm + qType, ReadMacInt16(tm + qType) | 0x8000);
	enqueue_tm(tm);
#if PRECISE_TIMING
	// Look for next task to be called and set wakeup_time
	wakeup_time = wakeup_time_max;
	for (int j=0; j<NUM_DESCS; j++) {
		if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
			if (timer_cmp_time(desc[j].wakeup, wakeup_time) < 0)
				wakeup_time = desc[j].wakeup;
	}
#ifdef PRECISE_TIMING_BEOS
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif
#ifdef PRECISE_TIMING_POSIX
	pthread_mutex_unlock(&wakeup_time_lock);
	timer_thread_resume();
	assert(suspend_count == 0);
#endif
#endif
	return 0;
}


/*
 *  Time Manager thread
 */

#ifdef PRECISE_TIMING_BEOS
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

#ifdef PRECISE_TIMING_POSIX
static void *timer_func(void *arg)
{
	while (!timer_thread_cancel) {

		// Wait until time specified by wakeup_time
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &wakeup_time, NULL);

		tm_time_t system_time;
		timer_current_time(system_time);
		if (timer_cmp_time(wakeup_time, system_time) < 0) {

			// Timer expired, trigger interrupt
			pthread_mutex_lock(&wakeup_time_lock);
			wakeup_time = wakeup_time_max;
			pthread_mutex_unlock(&wakeup_time_lock);
			SetInterruptFlag(INTFLAG_TIMER);
			TriggerInterrupt();
		}
	}
	return NULL;
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
#if PRECISE_TIMING_BEOS
	while (acquire_sem(wakeup_time_sem) == B_INTERRUPTED) ;
	suspend_thread(timer_thread);
#endif
#if PRECISE_TIMING_POSIX
	timer_thread_suspend();
	pthread_mutex_lock(&wakeup_time_lock);
#endif
	wakeup_time = wakeup_time_max;
	for (int j=0; j<NUM_DESCS; j++) {
		if (desc[j].in_use && (ReadMacInt16(desc[j].task + qType) & 0x8000))
			if (timer_cmp_time(desc[j].wakeup, wakeup_time) < 0)
				wakeup_time = desc[j].wakeup;
	}
#if PRECISE_TIMING_BEOS
	release_sem(wakeup_time_sem);
	thread_info info;
	do {
		resume_thread(timer_thread);			// This will unblock the thread
		get_thread_info(timer_thread, &info);
	} while (info.state == B_THREAD_SUSPENDED);	// Sometimes, resume_thread() doesn't work (BeOS bug?)
#endif
#if PRECISE_TIMING_POSIX
	pthread_mutex_unlock(&wakeup_time_lock);
	timer_thread_resume();
	assert(suspend_count == 0);
#endif
#endif
}
