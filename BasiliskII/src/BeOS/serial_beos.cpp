/*
 *  serial_beos.cpp - Serial device driver, BeOS specific stuff
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
#include <stdio.h>
#include <unistd.h>
#include <DeviceKit.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "serial.h"
#include "serial_defs.h"

#define DEBUG 0
#include "debug.h"

#define MONITOR 0


// Buffer size for kernel-space transfers
const int TMP_BUF_SIZE = 2048;

// These packets are sent to the input/output threads
const uint32 CMD_READ = 'read';
const uint32 CMD_WRITE = 'writ';
const uint32 CMD_QUIT = 'quit';

struct ThreadPacket {
	uint32 pb;
};


// Driver private variables
class BeSERDPort : public SERDPort {
public:
	BeSERDPort(const char *dev)
	{
		device_name = dev;
		if (strstr(dev, "parallel")) {
			is_parallel = true;
			fd = -1;
			device = NULL;
		} else {
			is_parallel = false;
			device = new BSerialPort;	
		}
		device_sem = create_sem(1, "serial port");
		input_thread = output_thread = 0;
	}

	virtual ~BeSERDPort()
	{
		status_t l;
		if (input_thread > 0) {
			send_data(input_thread, CMD_QUIT, NULL, 0);
			suspend_thread(input_thread);	// Unblock thread
			snooze(1000);
			resume_thread(input_thread);
			while (wait_for_thread(input_thread, &l) == B_INTERRUPTED) ;
		}
		if (output_thread > 0) {
			send_data(output_thread, CMD_QUIT, NULL, 0);
			suspend_thread(output_thread);	// Unblock thread
			snooze(1000);
			resume_thread(output_thread);
			while (wait_for_thread(output_thread, &l) == B_INTERRUPTED) ;
		}
		acquire_sem(device_sem);
		delete_sem(device_sem);
		delete device;
	}

	virtual int16 open(uint16 config);
	virtual int16 prime_in(uint32 pb, uint32 dce);
	virtual int16 prime_out(uint32 pb, uint32 dce);
	virtual int16 control(uint32 pb, uint32 dce, uint16 code);
	virtual int16 status(uint32 pb, uint32 dce, uint16 code);
	virtual int16 close(void);

private:
	bool configure(uint16 config);
	void set_handshake(uint32 s, bool with_dtr);
	static status_t input_func(void *arg);
	static status_t output_func(void *arg);

	const char *device_name;	// Name of BeOS port
	BSerialPort *device;		// BeOS port object
	bool is_parallel;			// Flag: Port is parallel, use fd
	int fd;						// FD for parallel ports
	sem_id device_sem;			// BSerialPort arbitration

	thread_id input_thread;		// Data input thread
	thread_id output_thread;	// Data output thread

	bool io_killed;				// Flag: KillIO called, I/O threads must not call deferred tasks
	bool drop_dtr_on_close;		// Flag: Negate DTR when driver is closed

	uint8 tmp_in_buf[TMP_BUF_SIZE];		// Buffers for copying from/to kernel space
	uint8 tmp_out_buf[TMP_BUF_SIZE];
};


#if DEBUG
static const int baud_rates[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 31250
};
#endif


/*
 *  Initialization
 */

void SerialInit(void)
{
	// Read serial preferences and create structs for both ports
	the_serd_port[0] = new BeSERDPort(PrefsFindString("seriala"));
	the_serd_port[1] = new BeSERDPort(PrefsFindString("serialb"));
}


/*
 *  Deinitialization
 */

void SerialExit(void)
{
	delete (BeSERDPort *)the_serd_port[0];
	delete (BeSERDPort *)the_serd_port[1];
}


/*
 *  Open serial port
 */

int16 BeSERDPort::open(uint16 config)
{
	// Don't open NULL name devices
	if (device_name == NULL)
		return openErr;

	// Init variables
	io_killed = false;
	drop_dtr_on_close = true;

	// Open port
	while (acquire_sem(device_sem) == B_INTERRUPTED) ;
	if (is_parallel) {
		char name[256];
		sprintf(name, "/dev/parallel/%s", device_name);
		fd = ::open(name, O_WRONLY);
		if (fd < 0) {
			release_sem(device_sem);
			return openErr;
		}
	} else {
		device->SetFlowControl(B_HARDWARE_CONTROL);	// Must be set before port is opened
		if (device->Open(device_name) > 0) {
			device->SetBlocking(true);
			device->SetTimeout(10000000);
			device->SetDTR(true);
			device->SetRTS(true);
		} else {
			release_sem(device_sem);
			return openErr;
		}
	}

	// Start input/output threads
	release_sem(device_sem);
	configure(config);
	while (acquire_sem(device_sem) == B_INTERRUPTED) ;
	while ((input_thread = spawn_thread(input_func, "Serial Input", B_NORMAL_PRIORITY, this)) == B_INTERRUPTED) ;
	resume_thread(input_thread);
	while ((output_thread = spawn_thread(output_func, "Serial Output", B_NORMAL_PRIORITY, this)) == B_INTERRUPTED) ;
	resume_thread(output_thread);
	release_sem(device_sem);
	return noErr;
}


/*
 *  Read data from port
 */

int16 BeSERDPort::prime_in(uint32 pb, uint32 dce)
{
	// Send input command to input_thread
	read_done = false;
	read_pending = true;
	ThreadPacket p;
	p.pb = pb;
	WriteMacInt32(input_dt + serdtDCE, dce);
	while (send_data(input_thread, CMD_READ, &p, sizeof(ThreadPacket)) == B_INTERRUPTED) ;
	return 1;	// Command in progress
}


/*
 *  Write data to port
 */

int16 BeSERDPort::prime_out(uint32 pb, uint32 dce)
{
	// Send output command to output_thread
	write_done = false;
	write_pending = true;
	ThreadPacket p;
	p.pb = pb;
	WriteMacInt32(output_dt + serdtDCE, dce);
	while (send_data(output_thread, CMD_WRITE, &p, sizeof(ThreadPacket)) == B_INTERRUPTED) ;
	return 1;	// Command in progress
}


/*
 *	Control calls
 */
 
int16 BeSERDPort::control(uint32 pb, uint32 dce, uint16 code)
{
	switch (code) {
		case 1:			// KillIO
			io_killed = true;
			suspend_thread(input_thread);	// Unblock threads
			suspend_thread(output_thread);
			snooze(1000);
			resume_thread(input_thread);
			resume_thread(output_thread);
			while (read_pending || write_pending)
				snooze(10000);
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->ClearInput();
				device->ClearOutput();
				release_sem(device_sem);
			}
			io_killed = false;
			return noErr;

		case kSERDConfiguration:
			if (configure(ReadMacInt16(pb + csParam)))
				return noErr;
			else
				return paramErr;

		case kSERDInputBuffer:
			return noErr;	// Not supported under BeOS

		case kSERDSerHShake:
			set_handshake(pb + csParam, false);
			return noErr;

		case kSERDClearBreak:
		case kSERDSetBreak:
			return noErr;	// Not supported under BeOS

		case kSERDBaudRate:
			if (!is_parallel) {
				uint16 rate = ReadMacInt16(pb + csParam);
				data_rate baud_rate;
				if (rate <= 50) {
					rate = 50; baud_rate = B_50_BPS;
				} else if (rate <= 75) {
					rate = 75; baud_rate = B_75_BPS;
				} else if (rate <= 110) {
					rate = 110; baud_rate = B_110_BPS;
				} else if (rate <= 134) {
					rate = 134; baud_rate = B_134_BPS;
				} else if (rate <= 150) {
					rate = 150; baud_rate = B_150_BPS;
				} else if (rate <= 200) {
					rate = 200; baud_rate = B_200_BPS;
				} else if (rate <= 300) {
					rate = 300; baud_rate = B_300_BPS;
				} else if (rate <= 600) {
					rate = 600; baud_rate = B_600_BPS;
				} else if (rate <= 1200) {
					rate = 1200; baud_rate = B_1200_BPS;
				} else if (rate <= 1800) {
					rate = 1800; baud_rate = B_1800_BPS;
				} else if (rate <= 2400) {
					rate = 2400; baud_rate = B_2400_BPS;
				} else if (rate <= 4800) {
					rate = 4800; baud_rate = B_4800_BPS;
				} else if (rate <= 9600) {
					rate = 9600; baud_rate = B_9600_BPS;
				} else if (rate <= 19200) {
					rate = 19200; baud_rate = B_19200_BPS;
				} else if (rate <= 31250) {
					rate = 31250; baud_rate = B_31250_BPS;
				} else if (rate <= 38400) {
					rate = 38400; baud_rate = B_38400_BPS;
				} else if (rate <= 57600) {
					rate = 57600; baud_rate = B_57600_BPS;
				}
				WriteMacInt16(pb + csParam, rate);
				acquire_sem(device_sem);
				if (device->SetDataRate(baud_rate) == B_OK) {
					release_sem(device_sem);
					return noErr;
				} else {
					release_sem(device_sem);
					return paramErr;
				}
			} else
				return noErr;

		case kSERDHandshake:
		case kSERDHandshakeRS232:
			set_handshake(pb + csParam, true);
			return noErr;

		case kSERDClockMIDI:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->SetParityMode(B_NO_PARITY);
				device->SetDataBits(B_DATA_BITS_8);
				device->SetStopBits(B_STOP_BITS_1);
				if (device->SetDataRate(B_31250_BPS) == B_OK) {
					release_sem(device_sem);
					return noErr;
				} else {
					release_sem(device_sem);
					return paramErr;
				}
			} else
				return noErr;

		case kSERDMiscOptions:
			drop_dtr_on_close = !(ReadMacInt8(pb + csParam) & kOptionPreserveDTR);
			return noErr;

		case kSERDAssertDTR:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->SetDTR(true);
				release_sem(device_sem);
			}
			return noErr;

		case kSERDNegateDTR:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->SetDTR(false);
				release_sem(device_sem);
			}
			return noErr;

		case kSERDSetPEChar:
		case kSERDSetPEAltChar:
			return noErr;	// Not supported under BeOS

		case kSERDResetChannel:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->ClearInput();
				device->ClearOutput();
				release_sem(device_sem);
			}
			return noErr;

		case kSERDAssertRTS:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->SetRTS(true);
				release_sem(device_sem);
			}
			return noErr;

		case kSERDNegateRTS:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->SetRTS(false);
				release_sem(device_sem);
			}
			return noErr;

		case kSERD115KBaud:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				if (device->DataRate() != B_115200_BPS)
					if (device->SetDataRate(B_115200_BPS) != B_OK) {
						release_sem(device_sem);
						return paramErr;
					}
				release_sem(device_sem);
			}
			return noErr;

		case kSERD230KBaud:
		case kSERDSetHighSpeed:
			if (!is_parallel) {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				if (device->DataRate() != B_230400_BPS)
					if (device->SetDataRate(B_230400_BPS) != B_OK) {
						release_sem(device_sem);
						return paramErr;
					}
				release_sem(device_sem);
			}
			return noErr;

		default:
			printf("WARNING: SerialControl(): unimplemented control code %d\n", code);
			return controlErr;
	}
}


/*
 *	Status calls
 */

int16 BeSERDPort::status(uint32 pb, uint32 dce, uint16 code)
{
	switch (code) {
		case kSERDInputCount:
			WriteMacInt32(pb + csParam, 0);
			if (!is_parallel) {
				int32 num = 0;
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				device->NumCharsAvailable(&num);
				release_sem(device_sem);
				D(bug(" %d bytes in buffer\n", num));
				WriteMacInt32(pb + csParam, num);
			}
			return noErr;

		case kSERDStatus: {
			uint32 p = pb + csParam;
			WriteMacInt8(p + staCumErrs, cum_errors);
			cum_errors = 0;
			WriteMacInt8(p + staXOffSent, 0);
			WriteMacInt8(p + staXOffHold, 0);
			WriteMacInt8(p + staRdPend, read_pending);
			WriteMacInt8(p + staWrPend, write_pending);
			if (is_parallel) {
				WriteMacInt8(p + staCtsHold, 0);
				WriteMacInt8(p + staDsrHold, 0);
				WriteMacInt8(p + staModemStatus, dsrEvent | dcdEvent | ctsEvent);
			} else {
				while (acquire_sem(device_sem) == B_INTERRUPTED) ;
				WriteMacInt8(p + staCtsHold, !device->IsCTS());
				WriteMacInt8(p + staDsrHold, !device->IsDSR());
				WriteMacInt8(p + staModemStatus,
					(device->IsDSR() ? dsrEvent : 0)
					| (device->IsRI() ? riEvent : 0)
					| (device->IsDCD() ? dcdEvent : 0)
					| (device->IsCTS() ? ctsEvent : 0));
				release_sem(device_sem);
			}
			return noErr;
		}

		default:
			printf("WARNING: SerialStatus(): unimplemented status code %d\n", code);
			return statusErr;
	}
}


/*
 *	Close serial port
 */

int16 BeSERDPort::close()
{
	// Kill threads
	status_t l;
	io_killed = true;
	if (input_thread > 0) {
		while (send_data(input_thread, CMD_QUIT, NULL, 0) == B_INTERRUPTED) ;
		if (read_pending) {
			suspend_thread(input_thread);	// Unblock thread
			snooze(1000);
			resume_thread(input_thread);
		}
		while (wait_for_thread(input_thread, &l) == B_INTERRUPTED) ;
	}
	if (output_thread > 0) {
		while (send_data(output_thread, CMD_QUIT, NULL, 0) == B_INTERRUPTED) ;
		if (write_pending) {
			suspend_thread(output_thread);	// Unblock thread
			snooze(1000);
			resume_thread(output_thread);
		}
		while (wait_for_thread(output_thread, &l) == B_INTERRUPTED) ;
	}
	input_thread = output_thread = 0;

	// Close port
	while (acquire_sem(device_sem) == B_INTERRUPTED) ;
	if (is_parallel) {
		::close(fd);
		fd = -1;
	} else {
		if (drop_dtr_on_close)
			device->SetDTR(false);
		device->Close();
	}
	release_sem(device_sem);
	return noErr;
}


/*
 *  Configure serial port with MacOS config word
 */

bool BeSERDPort::configure(uint16 config)
{
	D(bug(" configure %04x\n", config));
	if (is_parallel)
		return true;

	while (acquire_sem(device_sem) == B_INTERRUPTED) ;

	// Set number of stop bits
	switch (config & 0xc000) {
		case stop10:
			if (device->StopBits() != B_STOP_BITS_1)
				device->SetStopBits(B_STOP_BITS_1);
			break;
		case stop20:
			if (device->StopBits() != B_STOP_BITS_2)
				device->SetStopBits(B_STOP_BITS_2);
			break;
		default:
			release_sem(device_sem);
			return false;
	}

	// Set parity mode
	switch (config & 0x3000) {
		case noParity:
			if (device->ParityMode() != B_NO_PARITY)
				device->SetParityMode(B_NO_PARITY);
			break;
		case oddParity:
			if (device->ParityMode() != B_ODD_PARITY)
				device->SetParityMode(B_ODD_PARITY);
			break;
		case evenParity:
			if (device->ParityMode() != B_EVEN_PARITY)
				device->SetParityMode(B_EVEN_PARITY);
			break;
		default:
			release_sem(device_sem);
			return false;
	}

	// Set number of data bits
	switch (config & 0x0c00) {
		case data7:
			if (device->DataBits() != B_DATA_BITS_7)
				device->SetDataBits(B_DATA_BITS_7);
			break;
		case data8:
			if (device->DataBits() != B_DATA_BITS_8)
				device->SetDataBits(B_DATA_BITS_8);
			break;
		default:
			release_sem(device_sem);
			return false;
	}

	// Set baud rate
	data_rate baud_rate;
	switch (config & 0x03ff) {
		case baud150: baud_rate = B_150_BPS; break;
		case baud300: baud_rate = B_300_BPS; break;
		case baud600: baud_rate = B_600_BPS; break;
		case baud1200: baud_rate = B_1200_BPS; break;
		case baud1800: baud_rate = B_1800_BPS; break;
		case baud2400: baud_rate = B_2400_BPS; break;
		case baud4800: baud_rate = B_4800_BPS; break;
		case baud9600: baud_rate = B_9600_BPS; break;
		case baud19200: baud_rate = B_19200_BPS; break;
		case baud38400: baud_rate = B_38400_BPS; break;
		case baud57600: baud_rate = B_57600_BPS; break;
		default:
			release_sem(device_sem);
			return false;
	}

	D(bug(" baud rate %d, %d stop bits, %s parity, %d data bits\n", baud_rates[baud_rate], device->StopBits() == B_STOP_BITS_1 ? 1 : 2, device->ParityMode() == B_NO_PARITY ? "no" : device->ParityMode() == B_ODD_PARITY ? "odd" : "even", device->DataBits() == B_DATA_BITS_7 ? 7 : 8));
	if (device->DataRate() != baud_rate) {
		bool res = device->SetDataRate(baud_rate) == B_OK;
		release_sem(device_sem);
		return res;
	} else {
		release_sem(device_sem);
		return true;
	}
}


/*
 *  Set serial handshaking
 */

void BeSERDPort::set_handshake(uint32 s, bool with_dtr)
{
	D(bug(" set_handshake %02x %02x %02x %02x %02x %02x %02x %02x\n",
		ReadMacInt8(s + 0), ReadMacInt8(s + 1), ReadMacInt8(s + 2), ReadMacInt8(s + 3),
		ReadMacInt8(s + 4), ReadMacInt8(s + 5), ReadMacInt8(s + 6), ReadMacInt8(s + 7)));
	if (is_parallel)
		return;

	uint32 flow;
	if (with_dtr) {
		if (ReadMacInt8(s + shkFCTS) || ReadMacInt8(s + shkFDTR))
			flow = B_HARDWARE_CONTROL;
		else
			flow = B_SOFTWARE_CONTROL;
	} else {
		if (ReadMacInt8(s + shkFCTS))
			flow = B_HARDWARE_CONTROL;
		else
			flow = B_SOFTWARE_CONTROL;
	}

	D(bug(" %sware flow control\n", flow == B_HARDWARE_CONTROL ? "hard" : "soft"));
	while (acquire_sem(device_sem) == B_INTERRUPTED) ;
	if (device->FlowControl() != flow) {
		device->Close();
		device->SetFlowControl(flow);
		device->Open(device_name);
	}
	release_sem(device_sem);
}


/*
 *  Data input thread
 */

status_t BeSERDPort::input_func(void *arg)
{
	BeSERDPort *s = (BeSERDPort *)arg;
	for (;;) {

		// Wait for commands
		thread_id sender;
		ThreadPacket p;
		uint32 code = receive_data(&sender, &p, sizeof(ThreadPacket));
		if (code == CMD_QUIT)
			break;
		if (code != CMD_READ)
			continue;

		// Execute command
		void *buf = Mac2HostAddr(ReadMacInt32(p.pb + ioBuffer));
		uint32 length = ReadMacInt32(p.pb + ioReqCount);
		D(bug("input_func waiting for %ld bytes of data...\n", length));
		int32 actual;

		// Buffer in kernel space?
		if ((uint32)buf < 0x80000000) {
	
			// Yes, transfer via buffer
			actual = 0;
			while (length) {
				uint32 transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
				int32 transferred;
				acquire_sem(s->device_sem);
				if (s->is_parallel) {
					if ((transferred = read(s->fd, s->tmp_in_buf, transfer_size)) < 0 || s->io_killed) {
						// Error
						actual = transferred;
						release_sem(s->device_sem);
						break;
					}
				} else {
					if ((transferred = s->device->Read(s->tmp_in_buf, transfer_size)) < 0 || s->io_killed) {
						// Error
						actual = transferred;
						release_sem(s->device_sem);
						break;
					}
				}
				release_sem(s->device_sem);
				memcpy(buf, s->tmp_in_buf, transferred);
				buf = (void *)((uint8 *)buf + transferred);
				length -= transferred;
				actual += transferred;
			}

		} else {

			// No, transfer directly
			acquire_sem(s->device_sem);
			if (s->is_parallel)
				actual = read(s->fd, buf, length);
			else
				actual = s->device->Read(buf, length);
			release_sem(s->device_sem);
		}

		D(bug(" %ld bytes received\n", actual));

#if MONITOR
		bug("Receiving serial data:\n");
		uint8 *adr = Mac2HostAddr(ReadMacInt32(p.pb + ioBuffer));
		for (int i=0; i<actual; i++) {
			bug("%02x ", adr[i]);
		}
		bug("\n");
#endif

		// KillIO called? Then simply return
		if (s->io_killed) {

			WriteMacInt16(p.pb + ioResult, abortErr);
			WriteMacInt32(p.pb + ioActCount, 0);
			s->read_pending = s->read_done = false;

		} else {

			// Set error code
			if (actual >= 0) {
				WriteMacInt32(p.pb + ioActCount, actual);
				WriteMacInt32(s->input_dt + serdtResult, noErr);
			} else {
				WriteMacInt32(p.pb + ioActCount, 0);
				WriteMacInt32(s->input_dt + serdtResult, readErr);
			}
	
			// Trigger serial interrupt
			D(bug(" triggering serial interrupt\n"));
			s->read_done = true;
			SetInterruptFlag(INTFLAG_SERIAL);
			TriggerInterrupt();
		}
	}
	return 0;
}


/*
 *  Data output thread
 */

status_t BeSERDPort::output_func(void *arg)
{
	BeSERDPort *s = (BeSERDPort *)arg;
	for (;;) {

		// Wait for commands
		thread_id sender;
		ThreadPacket p;
		uint32 code = receive_data(&sender, &p, sizeof(ThreadPacket));
		if (code == CMD_QUIT)
			break;
		if (code != CMD_WRITE)
			continue;

		// Execute command
		void *buf = Mac2HostAddr(ReadMacInt32(p.pb + ioBuffer));
		uint32 length = ReadMacInt32(p.pb + ioReqCount);
		D(bug("output_func transmitting %ld bytes of data...\n", length));
		int32 actual;

#if MONITOR
		bug("Sending serial data:\n");
		uint8 *adr = (uint8 *)buf;
		for (int i=0; i<length; i++) {
			bug("%02x ", adr[i]);
		}
		bug("\n");
#endif

		// Buffer in kernel space?
		if ((uint32)buf < 0x80000000) {
	
			// Yes, transfer via buffer
			actual = 0;
			while (length) {
				uint32 transfer_size = (length > TMP_BUF_SIZE) ? TMP_BUF_SIZE : length;
				memcpy(s->tmp_out_buf, buf, transfer_size);
				int32 transferred;
				acquire_sem(s->device_sem);
				if (s->is_parallel) {
					if ((transferred = write(s->fd, s->tmp_out_buf, transfer_size)) < transfer_size || s->io_killed) {
						if (transferred < 0)	// Error
							actual = transferred;
						else
							actual += transferred;
						release_sem(s->device_sem);
						break;
					}
				} else {
					if ((transferred = s->device->Write(s->tmp_out_buf, transfer_size)) < transfer_size || s->io_killed) {
						if (transferred < 0)	// Error
							actual = transferred;
						else
							actual += transferred;
						release_sem(s->device_sem);
						break;
					}
				}
				release_sem(s->device_sem);
				if (transferred > transfer_size)	// R3 parallel port driver bug
					transferred = transfer_size;
				buf = (void *)((uint8 *)buf + transferred);
				length -= transferred;
				actual += transferred;
			}

		} else {
	
			// No, transfer directly
			acquire_sem(s->device_sem);
			if (s->is_parallel)
				actual = write(s->fd, buf, length);
			else
				actual = s->device->Write(buf, length);
			release_sem(s->device_sem);
			if (actual > length)	// R3 parallel port driver bug
				actual = length;
		}

		D(bug(" %ld bytes transmitted\n", actual));

		// KillIO called? Then simply return
		if (s->io_killed) {

			WriteMacInt16(p.pb + ioResult, abortErr);
			WriteMacInt32(p.pb + ioActCount, 0);
			s->write_pending = s->write_done = false;

		} else {

			// Set error code
			if (actual >= 0) {
				WriteMacInt32(p.pb + ioActCount, actual);
				WriteMacInt32(s->output_dt + serdtResult, noErr);
			} else {
				WriteMacInt32(p.pb + ioActCount, 0);
				WriteMacInt32(s->output_dt + serdtResult, writErr);
			}
	
			// Trigger serial interrupt
			D(bug(" triggering serial interrupt\n"));
			s->write_done = true;
			SetInterruptFlag(INTFLAG_SERIAL);
			TriggerInterrupt();
		}
	}
	return 0;
}
