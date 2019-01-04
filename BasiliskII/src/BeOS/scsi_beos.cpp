/*
 *  scsi_beos.cpp - SCSI Manager, BeOS specific stuff
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <device/scsi.h>
#ifdef __HAIKU__
#include <CAM.h>
#else
#include <drivers/CAM.h>
#endif

#include "sysdeps.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "scsi.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static raw_device_command rdc;

static int fds[8*8];					// fd's for 8 units and 8 LUNs each
static int fd;							// Active fd (selected target)

static uint32 buffer_size;				// Size of data buffer
static uint8 *buffer = NULL;			// Pointer to data buffer

static uint8 sense_data[256];			// Buffer for autosense data


/*
 *  Initialization
 */

void SCSIInit(void)
{
	int id, lun;

	// Allocate buffer
	buffer = (uint8 *)malloc(buffer_size = 0x10000);

	// Open scsi_raw driver for all 8 units (and all 8 LUNs)
	char dev_name[256];
	for (id=0; id<8; id++) {
		for (lun=0; lun<8; lun++)
			fds[id*8+lun] = -1;
		char prefs_name[16];
		sprintf(prefs_name, "scsi%d", id);
		const char *str = PrefsFindString(prefs_name);
		if (str) {
			int bus, unit;
			if (sscanf(str, "%d/%d", &bus, &unit) == 2) {
				for (lun=0; lun<8; lun++) {
					sprintf(dev_name, "/dev/bus/scsi/%d/%d/%d/raw", bus, unit, lun);
					D(bug("SCSI %d: Opening %s\n", id, dev_name));
					fds[id*8+lun] = open(dev_name, O_RDWR);
				}
			}
		}
	}

	// Reset SCSI bus
	SCSIReset();

	// Init rdc
	memset(&rdc, 0, sizeof(rdc));
	rdc.data = buffer;
	rdc.sense_data = sense_data;
}


/*
 *  Deinitialization
 */

void SCSIExit(void)
{
	// Close all devices
	for (int i=0; i<8; i++)
		for (int j=0; j<8; j++) {
			int fd = fds[i*8+j];
			if (fd > 0)
				close(fd);
		}

	// Free buffer
	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
}


/*
 *  Check if requested data size fits into buffer, allocate new buffer if needed
 */

static bool try_buffer(int size)
{
	if (size <= buffer_size)
		return true;

	uint8 *new_buffer = (uint8 *)malloc(size);
	if (new_buffer == NULL)
		return false;
	free(buffer);
	buffer = new_buffer;
	buffer_size = size;
	return true;
}


/*
 *  Set SCSI command to be sent by scsi_send_cmd()
 */

void scsi_set_cmd(int cmd_length, uint8 *cmd)
{
	rdc.command_length = cmd_length;
	memcpy(rdc.command, cmd, cmd_length);
}


/*
 *  Check for presence of SCSI target
 */

bool scsi_is_target_present(int id)
{
	return fds[id * 8] > 0;
}


/*
 *  Set SCSI target (returns false on error)
 */

bool scsi_set_target(int id, int lun)
{
	int new_fd = fds[id * 8 + lun];
	if (new_fd < 0)
		return false;
	if (new_fd != fd)
		rdc.cam_status &= ~CAM_AUTOSNS_VALID;	// Clear sense data when selecting new target
	fd = new_fd;
	return true;
}


/*
 *  Send SCSI command to active target (scsi_set_command() must have been called),
 *  read/write data according to S/G table (returns false on error); timeout is in 1/60 sec
 */

bool scsi_send_cmd(size_t data_length, bool reading, int sg_size, uint8 **sg_ptr, uint32 *sg_len, uint16 *stat, uint32 timeout)
{
	// Check if buffer is large enough, allocate new buffer if needed
	if (!try_buffer(data_length)) {
		char str[256];
		sprintf(str, GetString(STR_SCSI_BUFFER_ERR), data_length);
		ErrorAlert(str);
		return false;
	}

	// Process S/G table when writing
	if (!reading) {
		D(bug(" writing to buffer\n"));
		uint8 *buffer_ptr = buffer;
		for (int i=0; i<sg_size; i++) {
			uint32 len = sg_len[i];
			D(bug("  %d bytes from %08lx\n", len, sg_ptr[i]));
			memcpy(buffer_ptr, sg_ptr[i], len);
			buffer_ptr += len;
		}
	}

	// Request Sense and autosense data valid?
	status_t res = B_NO_ERROR;
	if (rdc.command[0] == 0x03 && (rdc.cam_status & CAM_AUTOSNS_VALID)) {

		// Yes, fake command
		D(bug(" autosense\n"));
		memcpy(buffer, sense_data, 256 - rdc.sense_data_length);
		rdc.scsi_status = 0;
		rdc.cam_status = CAM_REQ_CMP;

	} else {

		// No, send regular command
		D(bug(" sending command, length %d\n", data_length));
		rdc.flags = (reading ? B_RAW_DEVICE_DATA_IN : 0) | B_RAW_DEVICE_REPORT_RESIDUAL | B_RAW_DEVICE_SHORT_READ_VALID;
		rdc.data_length = data_length;
		rdc.sense_data_length = 256;
		rdc.scsi_status = 0;
		rdc.cam_status = CAM_REQ_CMP;
		rdc.timeout = timeout * 16667;
		res = ioctl(fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof(rdc));
		D(bug(" command sent, res %08x, status %d, cam_status %02x\n", res, rdc.scsi_status, rdc.cam_status));
		*stat = rdc.scsi_status;
	}

	// Process S/G table when reading
	if (reading && rdc.cam_status == CAM_REQ_CMP) {
		D(bug(" reading from buffer\n"));
		uint8 *buffer_ptr = buffer;
		for (int i=0; i<sg_size; i++) {
			uint32 len = sg_len[i];
			D(bug("  %d bytes to %08lx\n", len, sg_ptr[i]));
			memcpy(sg_ptr[i], buffer_ptr, len);
			buffer_ptr += len;
		}
	}
	return res ? false : true;
}
