/*
 *  scsi_dummy.cpp - SCSI Manager, dummy implementation
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

#include "sysdeps.h"
#include "scsi.h"

#define DEBUG 0
#include "debug.h"


/*
 *  Initialization
 */

void SCSIInit(void)
{
	// Reset SCSI bus
	SCSIReset();
}


/*
 *  Deinitialization
 */

void SCSIExit(void)
{
}


/*
 *  Set SCSI command to be sent by scsi_send_cmd()
 */

void scsi_set_cmd(int cmd_length, uint8 *cmd)
{
}


/*
 *  Check for presence of SCSI target
 */

bool scsi_is_target_present(int id)
{
	return false;
}


/*
 *  Set SCSI target (returns false on error)
 */

bool scsi_set_target(int id, int lun)
{
	return false;
}


/*
 *  Send SCSI command to active target (scsi_set_command() must have been called),
 *  read/write data according to S/G table (returns false on error)
 */

bool scsi_send_cmd(size_t data_length, bool reading, int sg_size, uint8 **sg_ptr, uint32 *sg_len, uint16 *stat, uint32 timeout)
{
	return false;
}

