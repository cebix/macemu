/*
 *  scsi_linux.cpp - SCSI Manager, Linux specific stuff
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

#include <sys/ioctl.h>
#include <linux/param.h>
#include <linux/../scsi/sg.h>	// workaround for broken RedHat 6.0 /usr/include/scsi
#include <unistd.h>
#include <errno.h>

#define DRIVER_SENSE 0x08

#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "scsi.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static int fds[8];				// fd's for 8 units
static int fd;					// Active fd (selected target)

static uint32 buffer_size;		// Size of data buffer
static uint8 *buffer = NULL;	// Pointer to data buffer

static uint8 the_cmd[12];		// Active SCSI command
static int the_cmd_len;


/*
 *  Initialization
 */

void SCSIInit(void)
{
	int id;

	// Allocate buffer
	buffer = (uint8 *)malloc(buffer_size = 0x10000);

	// Open generic SCSI driver for all 8 units
    for (id=0; id<8; id++) {
		char prefs_name[16];
		sprintf(prefs_name, "scsi%d", id);
		const char *str = PrefsFindString(prefs_name);
		if (str) {
			int fd = fds[id] = open(str, O_RDWR | O_EXCL);
			if (fd > 0) {
				// Is it really a Generic SCSI device?
				int timeout = ioctl(fd, SG_GET_TIMEOUT);
				if (timeout < 0) {
					// Error with SG_GET_TIMEOUT, doesn't seem to be a Generic SCSI device
					char msg[256];
					sprintf(msg, GetString(STR_SCSI_DEVICE_NOT_SCSI_WARN), str);
					WarningAlert(msg);
					close(fd);
					fds[id] = -1;
				} else {
					// Flush unwanted garbage from previous use of device
					uint8 reply[256];
					int old_fl = fcntl(fd, F_GETFL);
					fcntl(fd, F_SETFL, old_fl | O_NONBLOCK);
					while (read(fd, reply, sizeof(reply)) != -1 || errno != EAGAIN) ;
					fcntl(fd, F_SETFL, old_fl);
				}
			} else {
				char msg[256];
				sprintf(msg, GetString(STR_SCSI_DEVICE_OPEN_WARN), str, strerror(errno));
				WarningAlert(msg);
			}
		}
    }

	// Reset SCSI bus
	SCSIReset();
}


/*
 *  Deinitialization
 */

void SCSIExit(void)
{
	// Close all devices
	for (int i=0; i<8; i++) {
		int fd = fds[i];
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

static bool try_buffer(uint32 size)
{
	size += sizeof(sg_header) + 12;
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
	memcpy(the_cmd, cmd, the_cmd_len = cmd_length);
}


/*
 *  Check for presence of SCSI target
 */

bool scsi_is_target_present(int id)
{
	return fds[id] > 0;
}


/*
 *  Set SCSI target (returns false on error)
 */

bool scsi_set_target(int id, int lun)
{
	int new_fd = fds[id];
	if (new_fd < 0)
		return false;
	if (new_fd != fd) {
		// New target, clear autosense data
		sg_header *h = (sg_header *)buffer;
		h->driver_status &= ~DRIVER_SENSE;
	}
	fd = new_fd;
	return true;
}


/*
 *  Send SCSI command to active target (scsi_set_command() must have been called),
 *  read/write data according to S/G table (returns false on error); timeout is in 1/60 sec
 */

bool scsi_send_cmd(size_t data_length, bool reading, int sg_size, uint8 **sg_ptr, uint32 *sg_len, uint16 *stat, uint32 timeout)
{
	static int pack_id = 0;

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
		uint8 *buffer_ptr = buffer + sizeof(sg_header) + the_cmd_len;
		for (int i=0; i<sg_size; i++) {
			uint32 len = sg_len[i];
			D(bug("  %d bytes from %08lx\n", len, sg_ptr[i]));
			memcpy(buffer_ptr, sg_ptr[i], len);
			buffer_ptr += len;
		}
	}

	// Request Sense and autosense data valid?
	sg_header *h = (sg_header *)buffer;
	int res;
	if (reading && the_cmd[0] == 0x03 && (h->target_status & DRIVER_SENSE)) {

		// Yes, fake command
		D(bug(" autosense\n"));
		memcpy(buffer + sizeof(sg_header), h->sense_buffer, 16);
		h->target_status &= ~DRIVER_SENSE;
		res = 0;
		*stat = 0;

	} else {

		// No, send regular command
		if (timeout) {
			int to = timeout * HZ / 60;
			ioctl(fd, SG_SET_TIMEOUT, &to);
		}
		ioctl(fd, SG_NEXT_CMD_LEN, &the_cmd_len);

		D(bug(" sending command, length %d\n", data_length));

		int request_size, reply_size;
		if (reading) {
			h->pack_len = request_size = sizeof(sg_header) + the_cmd_len;
			h->reply_len = reply_size = sizeof(sg_header) + data_length;
		} else {
			h->pack_len = request_size = sizeof(sg_header) + the_cmd_len + data_length;
			h->reply_len = reply_size = sizeof(sg_header);
		}
		h->pack_id = pack_id++;
		h->result = 0;
		h->twelve_byte = (the_cmd_len == 12);
		h->target_status = 0;
		h->host_status = 0;
		h->driver_status = 0;
		h->other_flags = 0;
		memcpy(buffer + sizeof(sg_header), the_cmd, the_cmd_len);

		res = write(fd, buffer, request_size);
		D(bug(" request sent, actual %d, result %d\n", res, h->result));
		if (res >= 0) {
			res = read(fd, buffer, reply_size);
			D(bug(" reply read, actual %d, result %d, status %02x\n", res, h->result, h->target_status << 1));
		}

		*stat = h->target_status << 1;
	}

	// Process S/G table when reading
	if (reading && h->result == 0) {
		D(bug(" reading from buffer\n"));
		uint8 *buffer_ptr = buffer + sizeof(sg_header);
		for (int i=0; i<sg_size; i++) {
			uint32 len = sg_len[i];
			D(bug("  %d bytes to %08lx\n", len, sg_ptr[i]));
			memcpy(sg_ptr[i], buffer_ptr, len);
			buffer_ptr += len;
		}
	}
	return res >= 0;
}
