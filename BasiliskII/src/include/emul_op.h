/*
 *  emul_op.h - 68k opcodes for ROM patches
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

#ifndef EMUL_OP_H
#define EMUL_OP_H

// 68k opcodes
const uint16 M68K_ILLEGAL = 0x4afc;
const uint16 M68K_NOP = 0x4e71;
const uint16 M68K_RTS = 0x4e75;
const uint16 M68K_RTD = 0x4e74;
const uint16 M68K_RTR = 0x4e77;
const uint16 M68K_JMP = 0x4ef9;
const uint16 M68K_JMP_A0 = 0x4ed0;
const uint16 M68K_JSR = 0x4eb9;
const uint16 M68K_JSR_A0 = 0x4e90;

// Extended opcodes
enum {
	M68K_EXEC_RETURN = 0x7100,		// Extended opcodes (illegal moveq form)
	M68K_EMUL_BREAK,
	M68K_EMUL_OP_SHUTDOWN,
	M68K_EMUL_OP_RESET,
	M68K_EMUL_OP_CLKNOMEM,
	M68K_EMUL_OP_READ_XPRAM,
	M68K_EMUL_OP_READ_XPRAM2,
	M68K_EMUL_OP_PATCH_BOOT_GLOBS,
	M68K_EMUL_OP_FIX_BOOTSTACK,		// 0x7108
	M68K_EMUL_OP_FIX_MEMSIZE,
	M68K_EMUL_OP_INSTALL_DRIVERS,
	M68K_EMUL_OP_SERD,
	M68K_EMUL_OP_SONY_OPEN,
	M68K_EMUL_OP_SONY_PRIME,
	M68K_EMUL_OP_SONY_CONTROL,
	M68K_EMUL_OP_SONY_STATUS,
	M68K_EMUL_OP_DISK_OPEN,			// 0x7110
	M68K_EMUL_OP_DISK_PRIME,
	M68K_EMUL_OP_DISK_CONTROL,
	M68K_EMUL_OP_DISK_STATUS,
	M68K_EMUL_OP_CDROM_OPEN,
	M68K_EMUL_OP_CDROM_PRIME,
	M68K_EMUL_OP_CDROM_CONTROL,
	M68K_EMUL_OP_CDROM_STATUS,
	M68K_EMUL_OP_VIDEO_OPEN,		// 0x7118
	M68K_EMUL_OP_VIDEO_CONTROL,
	M68K_EMUL_OP_VIDEO_STATUS,
	M68K_EMUL_OP_SERIAL_OPEN,
	M68K_EMUL_OP_SERIAL_PRIME,
	M68K_EMUL_OP_SERIAL_CONTROL,
	M68K_EMUL_OP_SERIAL_STATUS,
	M68K_EMUL_OP_SERIAL_CLOSE,
	M68K_EMUL_OP_ETHER_OPEN,		// 0x7120
	M68K_EMUL_OP_ETHER_CONTROL,
	M68K_EMUL_OP_ETHER_READ_PACKET,
	M68K_EMUL_OP_ADBOP,
	M68K_EMUL_OP_INSTIME,
	M68K_EMUL_OP_RMVTIME,
	M68K_EMUL_OP_PRIMETIME,
	M68K_EMUL_OP_MICROSECONDS,
	M68K_EMUL_OP_SCSI_DISPATCH,		// 0x7128
	M68K_EMUL_OP_IRQ,
	M68K_EMUL_OP_PUT_SCRAP,
	M68K_EMUL_OP_GET_SCRAP,
	M68K_EMUL_OP_CHECKLOAD,
	M68K_EMUL_OP_AUDIO,
	M68K_EMUL_OP_EXTFS_COMM,
	M68K_EMUL_OP_EXTFS_HFS,
	M68K_EMUL_OP_BLOCK_MOVE,		// 0x7130
	M68K_EMUL_OP_SOUNDIN_OPEN,
	M68K_EMUL_OP_SOUNDIN_PRIME,
	M68K_EMUL_OP_SOUNDIN_CONTROL,
	M68K_EMUL_OP_SOUNDIN_STATUS,
	M68K_EMUL_OP_SOUNDIN_CLOSE,
	M68K_EMUL_OP_DEBUGUTIL,
	M68K_EMUL_OP_IDLE_TIME,
	M68K_EMUL_OP_SUSPEND,
	M68K_EMUL_OP_MAX				// highest number
};

// Functions
extern void EmulOp(uint16 opcode, struct M68kRegisters *r);	// Execute EMUL_OP opcode (called by 68k emulator or Line-F trap handler)

#endif
