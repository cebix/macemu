/*
 *  mathlib-i386.cpp - Math library wrapper, x86 specific code
 *  Code largely derived from GNU libc
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

// 7.12.9.8  The trunc functions
#ifndef HAVE_TRUNC
#define HAVE_TRUNC
double trunc(double x)
{
	volatile unsigned short int cw;
	volatile unsigned short int cwtmp;
	double value;

	__asm__ __volatile__("fnstcw %0" : "=m" (cw));
	cwtmp = (cw & 0xf3ff) | 0x0c00; /* toward zero */
	__asm__ __volatile__("fldcw %0" : : "m" (cwtmp));
	__asm__ __volatile__("frndint" : "=t" (value) : "0" (x));
	__asm__ __volatile__("fldcw %0" : : "m" (cw));
	return value;
}
#endif
