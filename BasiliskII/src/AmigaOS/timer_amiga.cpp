/*
 *  timer_amiga.cpp - Time Manager emulation, AmigaOS specific stuff
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

#include <exec/types.h>
#include <devices/timer.h>
#include <proto/timer.h>
#include <proto/intuition.h>

#include "sysdeps.h"
#include "timer.h"

#define DEBUG 0
#include "debug.h"


// Assembly functions
extern "C" ULONG TimevalTo64(struct timeval *, ULONG *);
extern "C" LONG TimevalToMacTime(struct timeval *);


/*
 *  Return microseconds since boot (64 bit)
 */

void Microseconds(uint32 &hi, uint32 &lo)
{
	D(bug("Microseconds\n"));
	struct timeval tv;
	GetSysTime(&tv);
	lo = TimevalTo64(&tv, &hi);
}


/*
 *  Return local date/time in Mac format (seconds since 1.1.1904)
 */

const uint32 TIME_OFFSET = 0x8b31ef80;	// Offset Mac->Amiga time in seconds

uint32 TimerDateTime(void)
{
	ULONG secs;
	ULONG mics;
	CurrentTime(&secs, &mics);
	return secs + TIME_OFFSET;
}


/*
 *  Get current time
 */

void timer_current_time(tm_time_t &t)
{
	GetSysTime(&t);
}


/*
 *  Add times
 */

void timer_add_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
	res = a;
	AddTime(&res, &b);
}


/*
 *  Subtract times
 */

void timer_sub_time(tm_time_t &res, tm_time_t a, tm_time_t b)
{
	res = a;
	SubTime(&res, &b);
}


/*
 *  Compare times (<0: a < b, =0: a = b, >0: a > b)
 */

int timer_cmp_time(tm_time_t a, tm_time_t b)
{
	return CmpTime(&b, &a);
}


/*
 *  Convert Mac time value (>0: microseconds, <0: microseconds) to tm_time_t
 */

void timer_mac2host_time(tm_time_t &res, int32 mactime)
{
	if (mactime > 0) {
		res.tv_secs = mactime / 1000;			// Time in milliseconds
		res.tv_micro = (mactime % 1000) * 1000;
	} else {
		res.tv_secs = -mactime / 1000000;		// Time in negative microseconds
		res.tv_micro = -mactime % 1000000;
	}
}


/*
 *  Convert positive tm_time_t to Mac time value (>0: microseconds, <0: microseconds)
 *  A negative input value for hosttime results in a zero return value
 *  As long as the microseconds value fits in 32 bit, it must not be converted to milliseconds!
 */

int32 timer_host2mac_time(tm_time_t hosttime)
{
	if (hosttime.tv_secs < 0)
		return 0;
	else
		return TimevalToMacTime(&hosttime);
}
