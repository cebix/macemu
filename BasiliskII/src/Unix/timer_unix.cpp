/*
 *  timer_unix.cpp - Time Manager emulation, Unix specific stuff
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
#include "timer.h"

#define DEBUG 0
#include "debug.h"


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
	time_t utc_now = time(NULL);
#if defined(__linux__) || defined(__SVR4)
	long tz = timezone;
	time_t local_now = utc_now - tz;
	if (daylight)
		local_now += 3600;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
	time_t local_now = utc_now + localtime(&utc_now)->tm_gmtoff;
#else
	time_t local_now = utc_now;
#endif
	return (uint32)local_now + TIME_OFFSET;
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
