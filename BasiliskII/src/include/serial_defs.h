/*
 *  serial_defs.h - Definitions for MacOS serial drivers
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

#ifndef SERIAL_DEFS_H
#define SERIAL_DEFS_H

// Error codes
enum {
	rcvrErr		= -89,
	breakRecd	= -90
};

// Serial port configuration
enum {
	baud150		= 763,
	baud300		= 380,
	baud600		= 189,
	baud1200	= 94,
	baud1800	= 62,
	baud2400	= 46,
	baud3600	= 30,
	baud4800	= 22,
	baud7200	= 14,
	baud9600	= 10,
	baud14400	= 6,
	baud19200	= 4,
	baud28800	= 2,
	baud38400	= 1,
	baud57600	= 0
};

enum {
	stop10		= 0x4000,
	stop15		= 0x8000,
	stop20		= 0xc000
};

enum {
	noParity	= 0,
	oddParity	= 0x1000,
	evenParity	= 0x3000
};

enum {
	data5		= 0,
	data6		= 0x0800,
	data7		= 0x0400,
	data8		= 0x0c00
};

// Serial events
enum {
	dsrEvent	= 2,
	riEvent		= 4,
	dcdEvent	= 8,
	ctsEvent	= 32,
	breakEvent	= 128
};

// Flags for SerStaRec.xOffSent
enum {
	xOffWasSent	= 128,
	dtrNegated	= 64,
	rtsNegated	= 32
};

// Serial driver error masks
enum {
	swOverrunErr	= 1,
	breakErr		= 8,
	parityErr		= 16,
	hwOverrunErr	= 32,
	framingErr		= 64
};

// Option for kSERDMiscOptions
enum {
	kOptionPreserveDTR	= 128,
	kOptionClockX1CTS	= 64
};

// Flags for SerShk.fCTS
enum {
	kUseCTSOutputFlowControl	= 128,
	kUseDSROutputFlowControl	= 64,
	kUseRTSInputFlowControl		= 128,
	kUseDTRInputFlowControl		= 64
};

// Control codes
enum {
	kSERDConfiguration		= 8,
	kSERDInputBuffer		= 9,
	kSERDSerHShake			= 10,
	kSERDClearBreak			= 11,
	kSERDSetBreak			= 12,
	kSERDBaudRate			= 13,
	kSERDHandshake			= 14,
	kSERDClockMIDI			= 15,
	kSERDMiscOptions		= 16,
	kSERDAssertDTR			= 17,
	kSERDNegateDTR			= 18,
	kSERDSetPEChar			= 19,
	kSERDSetPEAltChar		= 20,
	kSERDSetXOffFlag		= 21,
	kSERDClearXOffFlag		= 22,
	kSERDSendXOn			= 23,
	kSERDSendXOnOut			= 24,
	kSERDSendXOff			= 25,
	kSERDSendXOffOut		= 26,
	kSERDResetChannel		= 27,
	kSERDHandshakeRS232		= 28,
	kSERDStickParity		= 29,
	kSERDAssertRTS			= 30,
	kSERDNegateRTS			= 31,
	kSERD115KBaud			= 115,
	kSERD230KBaud			= 230,
	kSERDSetHighSpeed		= 0x4a46,	// 'JF'
	kSERDSetPollWrite		= 0x6a66	// 'jf'
};

// Status codes
enum {
	kSERDInputCount	= 2,
	kSERDStatus		= 8,
	kSERDVersion	= 9,
	kSERDGetDCD		= 256
};

enum {	// SerShk struct
	shkFXOn = 0,
	shkFCTS = 1,
	shkXOn = 2,
	shkXOff = 3,
	shkErrs = 4,
	shkEvts = 5,
	shkFInX = 6,
	shkFDTR = 7
};

enum {	 // SerSta struct
	staCumErrs = 0,
	staXOffSent = 1,
	staRdPend = 2,
	staWrPend = 3,
	staCtsHold = 4,
	staXOffHold = 5,
	staDsrHold = 6,
	staModemStatus = 7
};

#endif
