/*
 *  serial_amiga.cpp - Serial device driver, AmigaOS specific stuff
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

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/serial.h>
#include <devices/parallel.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/dos.h>
#include <inline/exec.h>
#include <inline/dos.h>

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


// These messages are sent to the serial process
const uint32 MSG_QUERY = 'qery';			// Query port status, return status in control_io
const uint32 MSG_SET_PARAMS = 'setp';		// Set serial parameters (parameters in control_io)
const uint32 MSG_SET_PAR_PARAMS = 'pstp';	// Set parallel parameters (parameters in control_io)
const uint32 MSG_KILL_IO = 'kill';			// Kill pending I/O requests
const uint32 MSG_BREAK = 'brek';			// Send break
const uint32 MSG_RESET = 'rset';			// Reset channel
const uint32 MSG_PRIME_IN = 'prin';			// Data input
const uint32 MSG_PRIME_OUT = 'pout';		// Data output

struct SerMessage : public Message {
	SerMessage(uint32 what_, const struct MsgPort *reply_port = NULL)
	{
		what = what_;
		mn_ReplyPort = (struct MsgPort *)reply_port;
		mn_Length = sizeof(*this);
	}
	uint32 what;
	uint32 pb;
};


// Driver private variables
class ASERDPort : public SERDPort {
public:
	ASERDPort(const char *dev)
	{
		device_name = dev;
		if (dev && dev[0] == '*') {
			is_parallel = true;
			device_name++;
		} else
			is_parallel = false;
		control_io = NULL;
		serial_proc = NULL;
		reply_port = NULL;
	}

	virtual ~ASERDPort()
	{
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
	void send_to_proc(uint32 what, uint32 pb = 0);
	bool query(void);
	bool set_params(void);
	bool set_par_params(void);
	void conv_error(struct IOExtSer *io, uint32 dt);
	static void serial_func(void);

	const char *device_name;			// Device name
	bool is_parallel;					// Flag: Port is parallel
	IOExtSer *control_io;				// IORequest for setting serial port characteristics etc.

	struct Process *serial_proc;		// Serial device handler process
	bool proc_error;					// Flag: process didn't initialize
	struct MsgPort *proc_port;			// Message port of process, for communication with main task
	struct MsgPort *reply_port;			// Reply port for communication with process

	uint8 err_mask;						// shkErrs
};


// Global variables
static void *proc_arg;						// Argument to process
extern struct Task *MainTask;				// Pointer to main task (from main_amiga.cpp)


/*
 *  Initialization
 */

void SerialInit(void)
{
	// Read serial preferences and create structs for both ports
	the_serd_port[0] = new ASERDPort(PrefsFindString("seriala"));
	the_serd_port[1] = new ASERDPort(PrefsFindString("serialb"));
}


/*
 *  Deinitialization
 */

void SerialExit(void)
{
	delete (ASERDPort *)the_serd_port[0];
	delete (ASERDPort *)the_serd_port[1];
}


/*
 *  Open serial port
 */

int16 ASERDPort::open(uint16 config)
{
	// Don't open NULL name devices
	if (device_name == NULL)
		return openErr;

	// Init variables
	err_mask = 0;

	// Create message port
	reply_port = CreateMsgPort();
	if (reply_port == NULL)
		goto open_error;

	// Start process
	proc_error = false;
	proc_arg = this;
	SetSignal(0, SIGF_SINGLE);
	serial_proc = CreateNewProcTags(
		NP_Entry, (ULONG)serial_func,
		NP_Name, (ULONG)"Basilisk II Serial Task",
		NP_Priority, 1,
		TAG_END	
	);
	if (serial_proc == NULL)
		goto open_error;

	// Wait for signal from process
	Wait(SIGF_SINGLE);

	// Initialization error? Then bail out
	if (proc_error)
		goto open_error;

	// Configure port
	configure(config);
	return noErr;

open_error:
	serial_proc = NULL;
	if (reply_port) {
		DeleteMsgPort(reply_port);
		reply_port = NULL;
	}
	return openErr;
}


/*
 *  Read data from port
 */

int16 ASERDPort::prime_in(uint32 pb, uint32 dce)
{
	// Send input command to serial process
	D(bug("primein\n"));
	read_done = false;
	read_pending = true;
	WriteMacInt32(input_dt + serdtDCE, dce);
	send_to_proc(MSG_PRIME_IN, pb);
	return 1;	// Command in progress
}


/*
 *  Write data to port
 */

int16 ASERDPort::prime_out(uint32 pb, uint32 dce)
{
	// Send output command to serial process
	D(bug("primeout\n"));
	write_done = false;
	write_pending = true;
	WriteMacInt32(output_dt + serdtDCE, dce);
	send_to_proc(MSG_PRIME_OUT, pb);
	return 1;	// Command in progress
}


/*
 *	Control calls
 */

int16 ASERDPort::control(uint32 pb, uint32 dce, uint16 code)
{
	D(bug("control(%ld)\n", (uint32)code));
	switch (code) {
		case 1:			// KillIO
			send_to_proc(MSG_KILL_IO);
			return noErr;

		case kSERDConfiguration:
			if (configure(ReadMacInt16(pb + csParam)))
				return noErr;
			else
				return paramErr;

		case kSERDInputBuffer: {
			if (is_parallel)
				return noErr;
			int buf = ReadMacInt16(pb + csParam + 4) & 0xffffffc0;
			if (buf < 1024)	// 1k minimum
				buf = 1024;
			D(bug(" buffer size is now %08lx\n", buf));
			control_io->io_RBufLen = buf;
			return set_params() ? noErr : paramErr;
		}

		case kSERDSerHShake:
			set_handshake(pb + csParam, false);
			return noErr;

		case kSERDSetBreak:
			if (!is_parallel)
				send_to_proc(MSG_BREAK);
			return noErr;

		case kSERDClearBreak:
			return noErr;

		case kSERDBaudRate:
			if (is_parallel)
				return noErr;
			control_io->io_Baud = ReadMacInt16(pb + csParam);
			D(bug(" baud rate %ld\n", control_io->io_Baud));
			return set_params() ? noErr : paramErr;

		case kSERDHandshake:
		case kSERDHandshakeRS232:
			set_handshake(pb + csParam, true);
			return noErr;

		case kSERDClockMIDI:
			if (is_parallel)
				return noErr;
			control_io->io_Baud = 31250;
			control_io->io_SerFlags = SERF_XDISABLED | SERF_SHARED;
			control_io->io_StopBits = 1;
			control_io->io_ReadLen = control_io->io_WriteLen = 8;
			return set_params() ? noErr : paramErr;

		case kSERDMiscOptions:
		case kSERDAssertDTR:
		case kSERDNegateDTR:
		case kSERDSetPEChar:
		case kSERDSetPEAltChar:
		case kSERDAssertRTS:
		case kSERDNegateRTS:
			return noErr;	// Not supported under AmigaOS

		case kSERD115KBaud:
			if (is_parallel)
				return noErr;
			control_io->io_Baud = 115200;
			return set_params() ? noErr : paramErr;

		case kSERD230KBaud:
		case kSERDSetHighSpeed:
			if (is_parallel)
				return noErr;
			control_io->io_Baud = 230400;
			return set_params() ? noErr : paramErr;

		case kSERDResetChannel:
			send_to_proc(MSG_RESET);
			return noErr;

		default:
			printf("WARNING: SerialControl(): unimplemented control code %d\n", code);
			return controlErr;
	}
}


/*
 *	Status calls
 */

int16 ASERDPort::status(uint32 pb, uint32 dce, uint16 code)
{
	D(bug("status(%ld)\n", (uint32)code));
	switch (code) {
		case kSERDInputCount:
			WriteMacInt32(pb + csParam, 0);
			if (!is_parallel) {
				if (!query())
					return noErr;
				D(bug("status(2) successful, returning %08lx\n", control_io->IOSer.io_Actual));
				WriteMacInt32(pb + csParam, control_io->IOSer.io_Actual);
			}
			return noErr;

		case kSERDStatus: {
			uint32 p = pb + csParam;
			WriteMacInt8(p + staCumErrs, cum_errors);
			cum_errors = 0;
			WriteMacInt8(p + staRdPend, read_pending);
			WriteMacInt8(p + staWrPend, write_pending);
			if (is_parallel) {
				WriteMacInt8(p + staXOffSent, 0);
				WriteMacInt8(p + staXOffHold, 0);
				WriteMacInt8(p + staCtsHold, 0);
				WriteMacInt8(p + staDsrHold, 0);
				WriteMacInt8(p + staModemStatus, dsrEvent | dcdEvent | ctsEvent);
			} else {
				query();
				WriteMacInt8(p + staXOffSent,
					(control_io->io_Status & IO_STATF_XOFFREAD ? xOffWasSent : 0)
					| (control_io->io_Status & (1 << 6) ? dtrNegated : 0));		// RTS
				WriteMacInt8(p + staXOffHold, control_io->io_Status & IO_STATF_XOFFWRITE);
				WriteMacInt8(p + staCtsHold, control_io->io_Status & (1 << 4));	// CTS
				WriteMacInt8(p + staDsrHold, control_io->io_Status & (1 << 3));	// DSR
				WriteMacInt8(p + staModemStatus,
					(control_io->io_Status & (1 << 3) ? 0 : dsrEvent)
					| (control_io->io_Status & (1 << 2) ? riEvent : 0)
					| (control_io->io_Status & (1 << 5) ? 0 : dcdEvent)
					| (control_io->io_Status & (1 << 4) ? 0 : ctsEvent)
					| (control_io->io_Status & IO_STATF_READBREAK ? breakEvent : 0));
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

int16 ASERDPort::close()
{
	// Stop process
	if (serial_proc) {
		SetSignal(0, SIGF_SINGLE);
		Signal(&serial_proc->pr_Task, SIGBREAKF_CTRL_C);
		Wait(SIGF_SINGLE);
	}

	// Delete reply port
	if (reply_port) {
		DeleteMsgPort(reply_port);
		reply_port = NULL;
	}
	return noErr;
}


/*
 *  Configure serial port with MacOS config word
 */

bool ASERDPort::configure(uint16 config)
{
	D(bug(" configure %04lx\n", (uint32)config));
	if (is_parallel)
		return true;

	// Set number of stop bits
	switch (config & 0xc000) {
		case stop10:
			control_io->io_StopBits = 1;
			break;
		case stop20:
			control_io->io_StopBits = 2;
			break;
		default:
			return false;
	}

	// Set parity mode
	switch (config & 0x3000) {
		case noParity:
			control_io->io_SerFlags &= ~SERF_PARTY_ON;
			break;
		case oddParity:
			control_io->io_SerFlags |= SERF_PARTY_ON | SERF_PARTY_ODD;
			break;
		case evenParity:
			control_io->io_SerFlags |= SERF_PARTY_ON;
			control_io->io_SerFlags &= ~SERF_PARTY_ODD;
			break;
		default:
			return false;
	}

	// Set number of data bits
	switch (config & 0x0c00) {
		case data5:
			control_io->io_ReadLen = control_io->io_WriteLen = 5;
			break;
		case data6:
			control_io->io_ReadLen = control_io->io_WriteLen = 6;
			break;
		case data7:
			control_io->io_ReadLen = control_io->io_WriteLen = 7;
			break;
		case data8:
			control_io->io_ReadLen = control_io->io_WriteLen = 8;
			break;
	}

	// Set baud rate
	control_io->io_Baud = 115200 / ((config & 0x03ff) + 2);
	return set_params();
}


/*
 *  Set serial handshaking
 */

void ASERDPort::set_handshake(uint32 s, bool with_dtr)
{
	D(bug(" set_handshake %02x %02x %02x %02x %02x %02x %02x %02x\n",
		ReadMacInt8(s + 0), ReadMacInt8(s + 1), ReadMacInt8(s + 2), ReadMacInt8(s + 3),
		ReadMacInt8(s + 4), ReadMacInt8(s + 5), ReadMacInt8(s + 6), ReadMacInt8(s + 7)));

	err_mask = ReadMacInt8(s + shkErrs);

	if (is_parallel) {

		// Parallel handshake
		if (with_dtr) {
			if (ReadMacInt8(s + shkFCTS) || ReadMacInt8(s + shkFDTR))
				((IOExtPar *)control_io)->io_ParFlags |= PARF_ACKMODE;
			else
				((IOExtPar *)control_io)->io_ParFlags &= ~PARF_ACKMODE;
		} else {
			if (ReadMacInt8(s + shkFCTS))
				((IOExtPar *)control_io)->io_ParFlags |= PARF_ACKMODE;
			else
				((IOExtPar *)control_io)->io_ParFlags &= ~PARF_ACKMODE;
		}
		set_par_params();

	} else {

		// Serial handshake
		if (ReadMacInt8(s + shkFXOn) || ReadMacInt8(s + shkFInX))
			control_io->io_SerFlags &= ~SERF_XDISABLED;
		else
			control_io->io_SerFlags |= SERF_XDISABLED;

		if (with_dtr) {
			if (ReadMacInt8(s + shkFCTS) || ReadMacInt8(s + shkFDTR))
				control_io->io_SerFlags |= SERF_7WIRE;
			else
				control_io->io_SerFlags &= ~SERF_7WIRE;
		} else {
			if (ReadMacInt8(s + shkFCTS))
				control_io->io_SerFlags |= SERF_7WIRE;
			else
				control_io->io_SerFlags &= ~SERF_7WIRE;
		}
		control_io->io_CtlChar = ReadMacInt16(s + shkXOn) << 16;
		set_params();
	}
}


/*
 *  Send message to serial process
 */

void ASERDPort::send_to_proc(uint32 what, uint32 pb)
{
	D(bug("sending %08lx to serial_proc\n", what));
	SerMessage msg(what, reply_port);
	msg.pb = pb;
	PutMsg(proc_port, &msg);
	WaitPort(reply_port);
	GetMsg(reply_port);
	D(bug(" sent\n"));
}


/*
 *  Query serial port status
 */

bool ASERDPort::query(void)
{
	send_to_proc(MSG_QUERY);
	return control_io->IOSer.io_Error == 0;
}


/*
 *  Set serial parameters
 */

bool ASERDPort::set_params(void)
{
	// Set/clear RadBoogie
	UBYTE flags = control_io->io_SerFlags;
	if (!(flags & SERF_PARTY_ON) && (flags & SERF_XDISABLED) && control_io->io_ReadLen == 8)
		control_io->io_SerFlags |= SERF_RAD_BOOGIE;
	else
		control_io->io_SerFlags &= ~SERF_RAD_BOOGIE;

	// Send message to serial process
	send_to_proc(MSG_SET_PARAMS);
	return control_io->IOSer.io_Error == 0;
}


/*
 *  Set parallel parameters
 */

bool ASERDPort::set_par_params(void)
{
	send_to_proc(MSG_SET_PAR_PARAMS);
	return control_io->IOSer.io_Error == 0;
}


/*
 *  Convert AmigaOS error code to MacOS error code, set serdtResult and cum_errors
 */

void ASERDPort::conv_error(struct IOExtSer *io, uint32 dt)
{
	int16 oserr;
	uint8 cum;

	BYTE err = io->IOSer.io_Error;
	if (err == 0 || err == IOERR_NOCMD) {
		oserr = 0;
		cum = 0;
	} else {
		if (is_parallel) {
			oserr = (err_mask & framingErr) ? rcvrErr : 0;
			cum = framingErr;
		} else {
			switch (io->IOSer.io_Error) {
				case SerErr_DetectedBreak:
					oserr = breakRecd;
					cum = breakErr;
					break;
				case SerErr_ParityErr:
					oserr = (err_mask & parityErr) ? rcvrErr : 0;
					cum = parityErr;
					break;
				case SerErr_BufOverflow:
					oserr = (err_mask & swOverrunErr) ? rcvrErr : 0;
					cum = swOverrunErr;
					break;
				case SerErr_LineErr:
					oserr = (err_mask & hwOverrunErr) ? rcvrErr : 0;
					cum = hwOverrunErr;
					break;
				default:
					oserr = (err_mask & framingErr) ? rcvrErr : 0;
					cum = framingErr;
					break;
			}
		}
	}

	WriteMacInt32(dt + serdtResult, oserr);
	cum_errors |= cum;
}


/*
 *  Process for communication with the serial.device
 */

__saveds void ASERDPort::serial_func(void)
{
	struct ASERDPort *obj = (ASERDPort *)proc_arg;
	struct MsgPort *proc_port = NULL, *io_port = NULL, *control_port = NULL;
	struct IOExtSer *read_io = NULL, *write_io = NULL, *control_io = NULL;
	uint8 orig_params[sizeof(struct IOExtSer)];
	bool opened = false;
	ULONG io_mask = 0, proc_port_mask = 0;

	// Default: error occured
	obj->proc_error = true;

	// Create message port for communication with main task
	proc_port = CreateMsgPort();
	if (proc_port == NULL)
		goto quit;
	proc_port_mask = 1 << proc_port->mp_SigBit;

	// Create message ports for serial.device I/O
	io_port = CreateMsgPort();
	if (io_port == NULL)
		goto quit;
	io_mask = 1 << io_port->mp_SigBit;
	control_port = CreateMsgPort();
	if (control_port == NULL)
		goto quit;

	// Create IORequests
	read_io = (struct IOExtSer *)CreateIORequest(io_port, sizeof(struct IOExtSer));
	write_io = (struct IOExtSer *)CreateIORequest(io_port, sizeof(struct IOExtSer));
	control_io = (struct IOExtSer *)CreateIORequest(control_port, sizeof(struct IOExtSer));
	if (read_io == NULL || write_io == NULL || control_io == NULL)
		goto quit;
	read_io->IOSer.io_Message.mn_Node.ln_Type = 0;	// Avoid CheckIO() bug
	write_io->IOSer.io_Message.mn_Node.ln_Type = 0;
	control_io->IOSer.io_Message.mn_Node.ln_Type = 0;

	// Parse device name
	char dev_name[256];
	ULONG dev_unit;
	if (sscanf(obj->device_name, "%[^/]/%ld", dev_name, &dev_unit) < 2)
		goto quit;

	// Open device
	if (obj->is_parallel)
		((IOExtPar *)read_io)->io_ParFlags = PARF_SHARED;
	else
		read_io->io_SerFlags = SERF_SHARED | SERF_7WIRE;
	if (OpenDevice((UBYTE *) dev_name, dev_unit, (struct IORequest *)read_io, 0) || read_io->IOSer.io_Device == NULL)
		goto quit;
	opened = true;

	// Copy IORequests
	memcpy(write_io, read_io, sizeof(struct IOExtSer));
	memcpy(control_io, read_io, sizeof(struct IOExtSer));

	// Attach control_io to control_port and set default values
	control_io->IOSer.io_Message.mn_ReplyPort = control_port;
	if (!obj->is_parallel) {
		control_io->io_CtlChar = SER_DEFAULT_CTLCHAR;
		control_io->io_RBufLen = 64;
		control_io->io_ExtFlags = 0;
		control_io->io_Baud = 9600;
		control_io->io_BrkTime = 250000;
		control_io->io_ReadLen = control_io->io_WriteLen = 8;
		control_io->io_StopBits = 1;
		control_io->io_SerFlags = SERF_SHARED;
		control_io->IOSer.io_Command = SDCMD_SETPARAMS;
		DoIO((struct IORequest *)control_io);
		memcpy(orig_params, &(control_io->io_CtlChar), (uint8 *)&(control_io->io_Status) - (uint8 *)&(control_io->io_CtlChar));
	}

	// Initialization went well, inform main task
	obj->proc_port = proc_port;
	obj->control_io = control_io;
	obj->proc_error = false;
	Signal(MainTask, SIGF_SINGLE);

	// Main loop
	for (;;) {

		// Wait for I/O and messages (CTRL_C is used for quitting the task)
		ULONG sig = Wait(proc_port_mask | io_mask | SIGBREAKF_CTRL_C);

		// Main task wants to quit us
		if (sig & SIGBREAKF_CTRL_C)
			break;

		// Main task sent a command to us
		if (sig & proc_port_mask) {
			struct SerMessage *msg;
			while (msg = (SerMessage *)GetMsg(proc_port)) {
				D(bug("serial_proc received %08lx\n", msg->what));
				switch (msg->what) {
					case MSG_QUERY:
						control_io->IOSer.io_Command = SDCMD_QUERY;
						DoIO((struct IORequest *)control_io);
						D(bug(" query returned %08lx, actual %08lx\n", control_io->IOSer.io_Error, control_io->IOSer.io_Actual));
						break;

					case MSG_SET_PARAMS:
						// Only send SDCMD_SETPARAMS when configuration has changed
						if (memcmp(orig_params, &(control_io->io_CtlChar), (uint8 *)&(control_io->io_Status) - (uint8 *)&(control_io->io_CtlChar))) {
							memcpy(orig_params, &(control_io->io_CtlChar), (uint8 *)&(control_io->io_Status) - (uint8 *)&(control_io->io_CtlChar));
							memcpy(&(read_io->io_CtlChar), &(control_io->io_CtlChar), (uint8 *)&(control_io->io_Status) - (uint8 *)&(control_io->io_CtlChar));
							memcpy(&(write_io->io_CtlChar), &(control_io->io_CtlChar), (uint8 *)&(control_io->io_Status) - (uint8 *)&(control_io->io_CtlChar));
							control_io->IOSer.io_Command = SDCMD_SETPARAMS;
							D(bug(" params %08lx %08lx %08lx %08lx %08lx %08lx\n", control_io->io_CtlChar, control_io->io_RBufLen, control_io->io_ExtFlags, control_io->io_Baud, control_io->io_BrkTime, *(uint32 *)((uint8 *)control_io + 76)));
							DoIO((struct IORequest *)control_io);
							D(bug(" set_parms returned %08lx\n", control_io->IOSer.io_Error));
						}
						break;

					case MSG_SET_PAR_PARAMS:
						control_io->IOSer.io_Command = PDCMD_SETPARAMS;
						DoIO((struct IORequest *)control_io);
						D(bug(" set_par_parms returned %08lx\n", control_io->IOSer.io_Error));
						break;

					case MSG_BREAK:
						control_io->IOSer.io_Command = SDCMD_BREAK;
						DoIO((struct IORequest *)control_io);
						D(bug(" break returned %08lx\n", control_io->IOSer.io_Error));
						break;

					case MSG_RESET:
						control_io->IOSer.io_Command = CMD_RESET;
						DoIO((struct IORequest *)control_io);
						D(bug(" reset returned %08lx\n", control_io->IOSer.io_Error));
						break;

					case MSG_KILL_IO:
						AbortIO((struct IORequest *)read_io);
						AbortIO((struct IORequest *)write_io);
						WaitIO((struct IORequest *)read_io);
						WaitIO((struct IORequest *)write_io);
						obj->read_pending = obj->write_pending = false;
						obj->read_done = obj->write_done = false;
						break;

					case MSG_PRIME_IN:
						read_io->IOSer.io_Message.mn_Node.ln_Name = (char *)msg->pb;
						read_io->IOSer.io_Data = Mac2HostAddr(ReadMacInt32(msg->pb + ioBuffer));
						read_io->IOSer.io_Length = ReadMacInt32(msg->pb + ioReqCount);
						read_io->IOSer.io_Actual = 0;
						read_io->IOSer.io_Command = CMD_READ;
						D(bug("serial_proc receiving %ld bytes from %08lx\n", read_io->IOSer.io_Length, read_io->IOSer.io_Data));
						SendIO((struct IORequest *)read_io);
						break;

					case MSG_PRIME_OUT: {
						write_io->IOSer.io_Message.mn_Node.ln_Name = (char *)msg->pb;
						write_io->IOSer.io_Data = Mac2HostAddr(ReadMacInt32(msg->pb + ioBuffer));
						write_io->IOSer.io_Length = ReadMacInt32(msg->pb + ioReqCount);
						write_io->IOSer.io_Actual = 0;
						write_io->IOSer.io_Command = CMD_WRITE;
						D(bug("serial_proc transmitting %ld bytes from %08lx\n", write_io->IOSer.io_Length, write_io->IOSer.io_Data));
#if MONITOR
						bug("Sending serial data:\n");
						uint8 *adr = Mac2HostAddr(ReadMacInt32(msg->pb + ioBuffer));
						for (int i=0; i<len; i++) {
							bug("%02lx ", adr[i]);
						}
						bug("\n");
#endif
						SendIO((struct IORequest *)write_io);
						break;
					}
				}
				D(bug(" serial_proc replying\n"));
				ReplyMsg(msg);
			}
		}

		// I/O operation completed
		if (sig & io_mask) {
			struct IOExtSer *io;
			while (io = (struct IOExtSer *)GetMsg(io_port)) {
				if (io == read_io) {
					D(bug("read_io complete, %ld bytes received, error %ld\n", read_io->IOSer.io_Actual, read_io->IOSer.io_Error));
					uint32 pb = (uint32)read_io->IOSer.io_Message.mn_Node.ln_Name;
#if MONITOR
					bug("Receiving serial data:\n");
					uint8 *adr = Mac2HostAddr(ReadMacInt32(msg->pb + ioBuffer));
					for (int i=0; i<read_io->IOSer.io_Actual; i++) {
						bug("%02lx ", adr[i]);
					}
					bug("\n");
#endif
					WriteMacInt32(pb + ioActCount, read_io->IOSer.io_Actual);
					obj->conv_error(read_io, obj->input_dt);
					obj->read_done = true;
					SetInterruptFlag(INTFLAG_SERIAL);
					TriggerInterrupt();
				} else if (io == write_io) {
					D(bug("write_io complete, %ld bytes sent, error %ld\n", write_io->IOSer.io_Actual, write_io->IOSer.io_Error));
					uint32 pb = (uint32)write_io->IOSer.io_Message.mn_Node.ln_Name;
					WriteMacInt32(pb + ioActCount, write_io->IOSer.io_Actual);
					obj->conv_error(write_io, obj->output_dt);
					obj->write_done = true;
					SetInterruptFlag(INTFLAG_SERIAL);
					TriggerInterrupt();
				}
			}
		}
	}
quit:

	// Close everything
	if (opened) {
		if (CheckIO((struct IORequest *)write_io) == 0) {
			AbortIO((struct IORequest *)write_io);
			WaitIO((struct IORequest *)write_io);
		}
		if (CheckIO((struct IORequest *)read_io) == 0) {
			AbortIO((struct IORequest *)read_io);
			WaitIO((struct IORequest *)read_io);
		}
		CloseDevice((struct IORequest *)read_io);
	}
	if (control_io)
		DeleteIORequest(control_io);
	if (write_io)
		DeleteIORequest(write_io);
	if (read_io)
		DeleteIORequest(read_io);
	if (control_port)
		DeleteMsgPort(control_port);
	if (io_port)
		DeleteMsgPort(io_port);

	// Send signal to main task to confirm termination
	Forbid();
	Signal(MainTask, SIGF_SINGLE);
}
