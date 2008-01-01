/*
 *  timer_beos.cpp - Time Manager emulation, BeOS specific stuff
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

#include <KernelKit.h>

#include "sysdeps.h"
#include "macos_util.h"
#include "timer.h"

#define DEBUG 0
#include "debug.h"


// From main_beos.cpp
extern thread_id emul_thread;


/*
 *  Return microseconds since boot (64 bit)
 */

void Microseconds(uint32 &hi, uint32 &lo)
{
	D(bug("Microseconds\n"));
	bigtime_t time = system_time();
	hi = time >> 32;
	lo = time;
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
	t = system_time();
}


/*
 *  Add times
 */

void timer_add_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
	res = a + b;
}


/*
 *  Subtract times
 */

void timer_sub_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
	res = a - b;
}


/*
 *  Compare times (<0: a < b, =0: a = b, >0: a > b)
 */

int timer_cmp_time(tm_time_t a, tm_time_t b)
{
	tm_time_t r = a - b;
	return r < 0 ? -1 : (r > 0 ? 1 : 0);
}


/*
 *  Convert Mac time value (>0: microseconds, <0: microseconds) to tm_time_t
 */

void timer_mac2host_time(tm_time_t &res, int32 mactime)
{
	if (mactime > 0)
		res = mactime * 1000;	// Time in milliseconds
	else
		res = -mactime;			// Time in negative microseconds
}


/*
 *  Convert positive tm_time_t to Mac time value (>0: microseconds, <0: microseconds)
 *  A negative input value for hosttime results in a zero return value
 *  As long as the microseconds value fits in 32 bit, it must not be converted to milliseconds!
 */

int32 timer_host2mac_time(tm_time_t hosttime)
{
	if (hosttime < 0)
		return 0;
	else if (hosttime > 0x7fffffff)
		return hosttime / 1000;	// Time in milliseconds
	else
		return -hosttime;		// Time in negative microseconds
}


/*
 *  Delay by specified number of microseconds (<1 second)
 */

void Delay_usec(uint32 usec)
{
	snooze(usec);
}


/*
 *  Suspend emulator thread, virtual CPU in idle mode
 */

void idle_wait(void)
{
#if 0
	/*
	  FIXME: add a semaphore (counter) to avoid a B_BAD_THREAD_STATE
	  return if we call idle_resume() when thread is not suspended?

	  Sorry, I can't test -- gb.
	 */
	suspend_thread(emul_thread);
#endif
}


/*
 *  Resume execution of emulator thread, events just arrived
 */

void idle_resume(void)
{
#if 0
	resume_thread(emul_thread);
#endif
}
