/*
 *  ether_defs.h - Definitions for MacOS Ethernet drivers
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

#ifndef ETHER_DEFS_H
#define ETHER_DEFS_H

// Error codes
enum {
	eMultiErr		= -91,
	eLenErr			= -92,
	lapProtErr		= -94,
	excessCollsns	= -95
};

// Control codes
enum {
	kENetSetGeneral	= 253,
	kENetGetInfo	= 252,
	kENetRdCancel	= 251,
	kENetRead		= 250,
	kENetWrite		= 249,
	kENetDetachPH	= 248,
	kENetAttachPH	= 247,
	kENetAddMulti	= 246,
	kENetDelMulti	= 245
};

enum {	// EParamBlock struct
	eProtType = 28,
	ePointer = 30,
	eBuffSize = 34,
	eDataSize = 36,
	eMultiAddr = 28
};

#endif
