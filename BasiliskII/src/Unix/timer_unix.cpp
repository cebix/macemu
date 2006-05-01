/*
 *  timer_unix.cpp - Time Manager emulation, Unix specific stuff
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
#include "macos_util.h"
#include "timer.h"

#include <errno.h>

#define DEBUG 0
#include "debug.h"

// For NetBSD with broken pthreads headers
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif


/*
 *  Return microseconds since boot (64 bit)
 */

void Microseconds(uint32 &hi, uint32 &lo)
{
	D(bug("Microseconds\n"));
#ifdef HAVE_CLOCK_GETTIME
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64 tl = (uint64)t.tv_sec * 1000000 + t.tv_nsec / 1000;
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	uint64 tl = (uint64)t.tv_sec * 1000000 + t.tv_usec;
#endif
	hi = tl >> 32;
	lo = tl;
}


/*
 *  Return local date/time in Mac format (seconds since 1.1.1904)
 */

uint32 TimerDateTime(void)
{
	return TimeToMacTime(time(NULL));
}


/*
 *  Get current time
 */

void timer_current_time(tm_time_t &t)
{
#ifdef HAVE_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &t);
#else
	gettimeofday(&t, NULL);
#endif
}


/*
 *  Add times
 */

void timer_add_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
#ifdef HAVE_CLOCK_GETTIME
	res.tv_sec = a.tv_sec + b.tv_sec;
	res.tv_nsec = a.tv_nsec + b.tv_nsec;
	if (res.tv_nsec >= 1000000000) {
		res.tv_sec++;
		res.tv_nsec -= 1000000000;
	}
#else
	res.tv_sec = a.tv_sec + b.tv_sec;
	res.tv_usec = a.tv_usec + b.tv_usec;
	if (res.tv_usec >= 1000000) {
		res.tv_sec++;
		res.tv_usec -= 1000000;
	}
#endif
}


/*
 *  Subtract times
 */

void timer_sub_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
#ifdef HAVE_CLOCK_GETTIME
	res.tv_sec = a.tv_sec - b.tv_sec;
	res.tv_nsec = a.tv_nsec - b.tv_nsec;
	if (res.tv_nsec < 0) {
		res.tv_sec--;
		res.tv_nsec += 1000000000;
	}
#else
	res.tv_sec = a.tv_sec - b.tv_sec;
	res.tv_usec = a.tv_usec - b.tv_usec;
	if (res.tv_usec < 0) {
		res.tv_sec--;
		res.tv_usec += 1000000;
	}
#endif
}


/*
 *  Compare times (<0: a < b, =0: a = b, >0: a > b)
 */

int timer_cmp_time(tm_time_t a, tm_time_t b)
{
#ifdef HAVE_CLOCK_GETTIME
	if (a.tv_sec == b.tv_sec)
		return a.tv_nsec - b.tv_nsec;
	else
		return a.tv_sec - b.tv_sec;
#else
	if (a.tv_sec == b.tv_sec)
		return a.tv_usec - b.tv_usec;
	else
		return a.tv_sec - b.tv_sec;
#endif
}


/*
 *  Convert Mac time value (>0: microseconds, <0: microseconds) to tm_time_t
 */

void timer_mac2host_time(tm_time_t &res, int32 mactime)
{
#ifdef HAVE_CLOCK_GETTIME
	if (mactime > 0) {
		// Time in milliseconds
		res.tv_sec = mactime / 1000;
		res.tv_nsec = (mactime % 1000) * 1000000;
	} else {
		// Time in negative microseconds
		res.tv_sec = -mactime / 1000000;
		res.tv_nsec = (-mactime % 1000000) * 1000;
	}
#else
	if (mactime > 0) {
		// Time in milliseconds
		res.tv_sec = mactime / 1000;
		res.tv_usec = (mactime % 1000) * 1000;
	} else {
		// Time in negative microseconds
		res.tv_sec = -mactime / 1000000;
		res.tv_usec = -mactime % 1000000;
	}
#endif
}


/*
 *  Convert positive tm_time_t to Mac time value (>0: microseconds, <0: microseconds)
 *  A negative input value for hosttime results in a zero return value
 *  As long as the microseconds value fits in 32 bit, it must not be converted to milliseconds!
 */

int32 timer_host2mac_time(tm_time_t hosttime)
{
	if (hosttime.tv_sec < 0)
		return 0;
	else {
#ifdef HAVE_CLOCK_GETTIME
		uint64 t = (uint64)hosttime.tv_sec * 1000000 + hosttime.tv_nsec / 1000;
#else
		uint64 t = (uint64)hosttime.tv_sec * 1000000 + hosttime.tv_usec;
#endif
		if (t > 0x7fffffff)
			return t / 1000;	// Time in milliseconds
		else
			return -t;			// Time in negative microseconds
	}
}


/*
 *  Get current value of microsecond timer
 */

uint64 GetTicks_usec(void)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint64)t.tv_sec * 1000000 + t.tv_nsec / 1000;
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return (uint64)t.tv_sec * 1000000 + t.tv_usec;
#endif
}


/*
 *  Delay by specified number of microseconds (<1 second)
 *  (adapted from SDL_Delay() source; this function is designed to provide
 *  the highest accuracy possible)
 */

#if defined(linux)
// Linux select() changes its timeout parameter upon return to contain
// the remaining time. Most other unixen leave it unchanged or undefined.
#define SELECT_SETS_REMAINING
#elif defined(__FreeBSD__) || defined(__sun__) || (defined(__MACH__) && defined(__APPLE__))
#define USE_NANOSLEEP
#elif defined(HAVE_PTHREADS) && defined(sgi)
// SGI pthreads has a bug when using pthreads+signals+nanosleep,
// so instead of using nanosleep, wait on a CV which is never signalled.
#include <pthread.h>
#define USE_COND_TIMEDWAIT
#endif

void Delay_usec(uint32 usec)
{
	int was_error;

#if defined(USE_NANOSLEEP)
	struct timespec elapsed, tv;
#elif defined(USE_COND_TIMEDWAIT)
	// Use a local mutex and cv, so threads remain independent
	pthread_cond_t delay_cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t delay_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct timespec elapsed;
	uint64 future;
#else
	struct timeval tv;
#ifndef SELECT_SETS_REMAINING
	uint64 then, now, elapsed;
#endif
#endif

	// Set the timeout interval - Linux only needs to do this once
#if defined(SELECT_SETS_REMAINING)
    tv.tv_sec = 0;
    tv.tv_usec = usec;
#elif defined(USE_NANOSLEEP)
    elapsed.tv_sec = 0;
    elapsed.tv_nsec = usec * 1000;
#elif defined(USE_COND_TIMEDWAIT)
	future = GetTicks_usec() + usec;
	elapsed.tv_sec = future / 1000000;
	elapsed.tv_nsec = (future % 1000000) * 1000;
#else
    then = GetTicks_usec();
#endif

	do {
		errno = 0;
#if defined(USE_NANOSLEEP)
		tv.tv_sec = elapsed.tv_sec;
		tv.tv_nsec = elapsed.tv_nsec;
		was_error = nanosleep(&tv, &elapsed);
#elif defined(USE_COND_TIMEDWAIT)
		was_error = pthread_mutex_lock(&delay_mutex);
		was_error = pthread_cond_timedwait(&delay_cond, &delay_mutex, &elapsed);
		was_error = pthread_mutex_unlock(&delay_mutex);
#else
#ifndef SELECT_SETS_REMAINING
		// Calculate the time interval left (in case of interrupt)
		now = GetTicks_usec();
		elapsed = now - then;
		then = now;
		if (elapsed >= usec)
			break;
		usec -= elapsed;
		tv.tv_sec = 0;
		tv.tv_usec = usec;
#endif
		was_error = select(0, NULL, NULL, NULL, &tv);
#endif
	} while (was_error && (errno == EINTR));
}


/*
 *  Suspend emulator thread, virtual CPU in idle mode
 */

#ifdef HAVE_PTHREADS
#if defined(HAVE_PTHREAD_COND_INIT)
#define IDLE_USES_COND_WAIT 1
static pthread_mutex_t idle_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t idle_cond = PTHREAD_COND_INITIALIZER;
#elif defined(HAVE_SEM_INIT)
#define IDLE_USES_SEMAPHORE 1
#include <semaphore.h>
#ifdef HAVE_SPINLOCKS
static spinlock_t idle_lock = SPIN_LOCK_UNLOCKED;
#define LOCK_IDLE spin_lock(&idle_lock)
#define UNLOCK_IDLE spin_unlock(&idle_lock)
#else
static pthread_mutex_t idle_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_IDLE pthread_mutex_lock(&idle_lock)
#define UNLOCK_IDLE pthread_mutex_unlock(&idle_lock)
#endif
static sem_t idle_sem;
static int idle_sem_ok = -1;
#endif
#endif

void idle_wait(void)
{
#ifdef IDLE_USES_COND_WAIT
	pthread_mutex_lock(&idle_lock);
	pthread_cond_wait(&idle_cond, &idle_lock);
	pthread_mutex_unlock(&idle_lock);
#else
#ifdef IDLE_USES_SEMAPHORE
	LOCK_IDLE;
	if (idle_sem_ok < 0)
		idle_sem_ok = (sem_init(&idle_sem, 0, 0) == 0);
	if (idle_sem_ok > 0) {
		idle_sem_ok++;
		UNLOCK_IDLE;
		sem_wait(&idle_sem);
		return;
	}
	UNLOCK_IDLE;
#endif

	// Fallback: sleep 10 ms
	Delay_usec(10000);
#endif
}


/*
 *  Resume execution of emulator thread, events just arrived
 */

void idle_resume(void)
{
#ifdef IDLE_USES_COND_WAIT
	pthread_cond_signal(&idle_cond);
#else
#ifdef IDLE_USES_SEMAPHORE
	LOCK_IDLE;
	if (idle_sem_ok > 1) {
		idle_sem_ok--;
		UNLOCK_IDLE;
		sem_post(&idle_sem);
		return;
	}
	UNLOCK_IDLE;
#endif
#endif
}
