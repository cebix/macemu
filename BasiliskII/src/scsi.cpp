/*
 *  scsi.cpp - SCSI Manager
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Devices, chapter 3 "SCSI Manager"
 *    Technote DV 24: "Fear No SCSI"
 */

#include <stdio.h>
#include <string.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "user_strings.h"
#include "scsi.h"

#define DEBUG 0
#include "debug.h"


// Error codes
enum {
	scCommErr = 2,
	scArbNBErr,
	scBadParmsErr,
	scPhaseErr,
	scCompareErr,
	scMgrBusyErr,
	scSequenceErr,
	scBusTOErr,
	scComplPhaseErr
};

// TIB opcodes
enum {
	scInc = 1,
	scNoInc,
	scAdd,
	scMove,
	scLoop,
	scNop,
	scStop,
	scComp
};

// Logical SCSI phases
enum {
	PH_FREE,		// Bus free
	PH_ARBITRATED,	// Bus arbitrated (after SCSIGet())
	PH_SELECTED,	// Target selected (after SCSISelect())
	PH_TRANSFER		// Command sent (after SCSICmd())
};

// Global variables
static int target_id;					// ID of active target
static int phase;						// Logical SCSI phase
static uint16 fake_status;				// Faked 5830 status word
static bool reading;					// Flag: reading from device

const int SG_TABLE_SIZE = 1024;
static int sg_index;					// Index of first unused entry in S/G table
static uint8 *sg_ptr[SG_TABLE_SIZE];	// Scatter/gather table data pointer (host address space)
static uint32 sg_len[SG_TABLE_SIZE];	// Scatter/gather table data length
static uint32 sg_total_length;			// Total data length


/*
 *  Execute TIB, constructing S/G table
 */

static int16 exec_tib(uint32 tib)
{
	for (;;) {

		// Read next opcode and parameters
		uint16 cmd = ReadMacInt16(tib); tib += 2;
		uint32 ptr = ReadMacInt32(tib); tib += 4;
		uint32 len = ReadMacInt32(tib); tib += 4;

#if DEBUG
		const char *cmd_str;
		switch (cmd) {
			case scInc:   cmd_str = "INC  "; break;
			case scNoInc: cmd_str = "NOINC"; break;
			case scAdd:   cmd_str = "ADD  "; break;
			case scMove:  cmd_str = "MOVE "; break;
			case scLoop:  cmd_str = "LOOP "; break;
			case scNop:   cmd_str = "NOP  "; break;
			case scStop:  cmd_str = "STOP "; break;
			case scComp:  cmd_str = "COMP "; break;
			default:      cmd_str = "???  "; break;
		}
		D(bug(" %s(%d) %08x %d\n", cmd_str, cmd, ptr, len));
#endif

		switch (cmd) {
			case scInc:
				WriteMacInt32(tib - 8, ptr + len);
			case scNoInc:
				if ((sg_index > 0) && (Mac2HostAddr(ptr) == sg_ptr[sg_index-1] + sg_len[sg_index-1]))
					sg_len[sg_index-1] += len;				// Merge to previous entry
				else {
					if (sg_index == SG_TABLE_SIZE) {
						ErrorAlert(GetString(STR_SCSI_SG_FULL_ERR));
						return -108;
					}
					sg_ptr[sg_index] = Mac2HostAddr(ptr);	// Create new entry
					sg_len[sg_index] = len;
					sg_index++;
				}
				sg_total_length += len;
				break;

			case scAdd:
				WriteMacInt32(ptr, ReadMacInt32(ptr) + len);
				break;

			case scMove:
				WriteMacInt32(len, ReadMacInt32(ptr));
				break;

			case scLoop:
				WriteMacInt32(tib - 4, len - 1);
				if (len - 1 > 0)
					tib += (int32)ptr - 10;
				break;

			case scNop:
				break;

			case scStop:
				return 0;

			case scComp:
				printf("WARNING: Unimplemented scComp opcode\n");
				return scCompareErr;

			default:
				printf("FATAL: Unrecognized TIB opcode %d\n", cmd);
				return scBadParmsErr;
		}
	}
}


/*
 *  Reset SCSI bus
 */

int16 SCSIReset(void)
{
	D(bug("SCSIReset\n"));

	phase = PH_FREE;
	fake_status = 0x0000;	// Bus free
	sg_index = 0;
	target_id = 8;
	return 0;
}


/*
 *  Arbitrate bus
 */

int16 SCSIGet(void)
{
	D(bug("SCSIGet\n"));
	if (phase != PH_FREE)
		return scMgrBusyErr;

	phase = PH_ARBITRATED;
	fake_status = 0x0040;	// Bus busy
	reading = false;
	sg_index = 0;			// Flush S/G table
	sg_total_length = 0;
	return 0;
}


/*
 *  Select SCSI device
 */

int16 SCSISelect(int id)
{
	D(bug("SCSISelect %d\n", id));
	if (phase != PH_ARBITRATED)
		return scSequenceErr;

	// ID valid?
	if (id >= 0 && id <= 7) {
		target_id = id;

		// Target present?
		if (scsi_is_target_present(target_id)) {
			phase = PH_SELECTED;
			fake_status = 0x006a;			// Target selected, command phase
			return 0;
		}
	}

	// Error
	phase = PH_FREE;
	fake_status = 0x0000;		// Bus free
	return scCommErr;
}


/*
 *  Send SCSI command
 */

int16 SCSICmd(int cmd_length, uint8 *cmd)
{
#if DEBUG
	switch (cmd_length) {
		case 6:
			D(bug("SCSICmd len 6, cmd %02x %02x %02x %02x %02x %02x\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]));
			break;
		case 10:
			D(bug("SCSICmd len 10, cmd %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]));
			break;
		case 12:
			D(bug("SCSICmd len 12, cmd %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11]));
			break;
		default:
			D(bug("SCSICmd bogus length %d\n", cmd_length));
			break;
	}
#endif

	if (phase != PH_SELECTED)
		return scPhaseErr;

	// Commdn length valid?
	if (cmd_length != 6 && cmd_length != 10 && cmd_length != 12)
		return scBadParmsErr;

	// Set command, extract LUN
	scsi_set_cmd(cmd_length, cmd);

	// Extract LUN, set target
	if (!scsi_set_target(target_id, (cmd[1] >> 5) & 7)) {
		phase = PH_FREE;
		fake_status = 0x0000;	// Bus free
		return scCommErr;
	}

	phase = PH_TRANSFER;
	fake_status = 0x006e;		// Target selected, data phase
	return 0;
}


/*
 *  Read data
 */

int16 SCSIRead(uint32 tib)
{
	D(bug("SCSIRead TIB %08lx\n", tib));
	if (phase != PH_TRANSFER)
		return scPhaseErr;

	// Execute TIB, fill S/G table
	reading = true;
	return exec_tib(tib);
}


/*
 *  Write data
 */

int16 SCSIWrite(uint32 tib)
{
	D(bug("SCSIWrite TIB %08lx\n", tib));
	if (phase != PH_TRANSFER)
		return scPhaseErr;

	// Execute TIB, fill S/G table
	return exec_tib(tib);
}


/*
 *  Wait for command completion (we're actually doing everything in here...)
 */

int16 SCSIComplete(uint32 timeout, uint32 message, uint32 stat)
{
	D(bug("SCSIComplete wait %d, msg %08lx, stat %08lx\n", timeout, message, stat));
	WriteMacInt16(message, 0);
	if (phase != PH_TRANSFER)
		return scPhaseErr;

	// Send command, process S/G table
	uint16 scsi_stat = 0;
	bool success = scsi_send_cmd(sg_total_length, reading, sg_index, sg_ptr, sg_len, &scsi_stat, timeout);
	WriteMacInt16(stat, scsi_stat);

	// Complete command
	phase = PH_FREE;
	fake_status = 0x0000;	// Bus free
	return success ? 0 : scCommErr;
}


/*
 *  Get bus status
 */

uint16 SCSIStat(void)
{
	D(bug("SCSIStat returns %04x\n", fake_status));
	return fake_status;
}


/*
 *  SCSI Manager busy?
 */

int16 SCSIMgrBusy(void)
{
//	D(bug("SCSIMgrBusy\n"));
	return phase != PH_FREE;
}
