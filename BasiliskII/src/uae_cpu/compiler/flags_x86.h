/*
 *  compiler/flags_x86.h - Native flags definitions for IA-32
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2005
 *    Gwenole Beauchesne
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

#ifndef NATIVE_FLAGS_X86_H
#define NATIVE_FLAGS_X86_H

/* Native integer code conditions */
enum {
	NATIVE_CC_HI = 7,
	NATIVE_CC_LS = 6,
	NATIVE_CC_CC = 3,
	NATIVE_CC_CS = 2,
	NATIVE_CC_NE = 5,
	NATIVE_CC_EQ = 4,
	NATIVE_CC_VC = 11,
	NATIVE_CC_VS = 10,
	NATIVE_CC_PL = 9,
	NATIVE_CC_MI = 8,
	NATIVE_CC_GE = 13,
	NATIVE_CC_LT = 12,
	NATIVE_CC_GT = 15,
	NATIVE_CC_LE = 14
};

#endif /* NATIVE_FLAGS_X86_H */
