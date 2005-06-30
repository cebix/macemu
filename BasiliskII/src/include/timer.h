/*
 *  timer.h - Time Manager emulation
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

#ifndef TIMER_H
#define TIMER_H

extern void TimerInit(void);
extern void TimerExit(void);
extern void TimerReset(void);

extern void TimerInterrupt(void);

extern int16 InsTime(uint32 tm, uint16 trap);
extern int16 RmvTime(uint32 tm);
extern int16 PrimeTime(uint32 tm, int32 time);
extern void Microseconds(uint32 &hi, uint32 &lo);

extern uint32 TimerDateTime(void);

// System specific and internal functions/data
extern void timer_current_time(tm_time_t &t);
extern void timer_add_time(tm_time_t &res, tm_time_t a, tm_time_t b);
extern void timer_sub_time(tm_time_t &res, tm_time_t a, tm_time_t b);
extern int timer_cmp_time(tm_time_t a, tm_time_t b);
extern void timer_mac2host_time(tm_time_t &res, int32 mactime);
extern int32 timer_host2mac_time(tm_time_t hosttime);

// Suspend execution of emulator thread and resume it on events
extern void idle_wait(void);
extern void idle_resume(void);

#endif
