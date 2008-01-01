/*
 *  serial.cpp - Serial device driver
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Devices, chapter 7 "Serial Driver"
 *    Technote HW 04: "Break/CTS Device Driver Event Structure"
 *    Technote 1018: "Understanding the SerialDMA Driver"
 */

#include <stdio.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "serial.h"
#include "serial_defs.h"

#include "emul_op.h"

#define DEBUG 0
#include "debug.h"


// Global variables
SERDPort *the_serd_port[2];


/*
 *  Driver Open() routine
 */

int16 SerialOpen(uint32 pb, uint32 dce, int port)
{
	D(bug("SerialOpen port %d, pb %08lx, dce %08lx\n", port, pb, dce));

	if (port == 0 || port == 2) {

		// Do nothing for input side
		return noErr;

	} else {

		// Do nothing if port is already open
		SERDPort *the_port = the_serd_port[port >> 1];
		if (the_port->is_open)
			return noErr;

		// Init variables
		the_port->read_pending = the_port->write_pending = false;
		the_port->read_done = the_port->write_done = false;
		the_port->cum_errors = 0;

		// Open port
		int16 res = the_port->open(ReadMacInt16(0x1fc + (port & 2)));
		if (res)
			return res;

		// Allocate Deferred Task structures
		M68kRegisters r;
		r.d[0] = SIZEOF_serdt * 2;
		Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
		if (r.a[0] == 0) {
			the_port->close();
			return openErr;
		}
		uint32 input_dt = the_port->input_dt = r.a[0];
		uint32 output_dt = the_port->output_dt = r.a[0] + SIZEOF_serdt;
		D(bug(" input_dt %08lx, output_dt %08lx\n", input_dt, output_dt));

		WriteMacInt16(input_dt + qType, dtQType);
		WriteMacInt32(input_dt + dtAddr, input_dt + serdtCode);
		WriteMacInt32(input_dt + dtParam, input_dt + serdtResult);
															// Deferred function for signalling that Prime is complete (pointer to mydtResult in a1)
		WriteMacInt16(input_dt + serdtCode, 0x2019);			// move.l	(a1)+,d0	(result)
		WriteMacInt16(input_dt + serdtCode + 2, 0x2251);		// move.l	(a1),a1		(dce)
		WriteMacInt32(input_dt + serdtCode + 4, 0x207808fc);	// move.l	JIODone,a0
		WriteMacInt16(input_dt + serdtCode + 8, 0x4ed0);		// jmp		(a0)

		WriteMacInt16(output_dt + qType, dtQType);
		WriteMacInt32(output_dt + dtAddr, output_dt + serdtCode);
		WriteMacInt32(output_dt + dtParam, output_dt + serdtResult);
															// Deferred function for signalling that Prime is complete (pointer to mydtResult in a1)
		WriteMacInt16(output_dt + serdtCode, 0x2019);			// move.l	(a1)+,d0	(result)
		WriteMacInt16(output_dt + serdtCode + 2, 0x2251);		// move.l	(a1),a1		(dce)
		WriteMacInt32(output_dt + serdtCode + 4, 0x207808fc);	// move.l	JIODone,a0
		WriteMacInt16(output_dt + serdtCode + 8, 0x4ed0);		// jmp		(a0)

		the_port->is_open = true;
		return noErr;
	}
}


/*
 *  Driver Prime() routine
 */

int16 SerialPrime(uint32 pb, uint32 dce, int port)
{
	D(bug("SerialPrime port %d, pb %08lx, dce %08lx\n", port, pb, dce));

	// Error if port is not open
	SERDPort *the_port = the_serd_port[port >> 1];
	if (!the_port->is_open)
		return notOpenErr;

	if (port == 0 || port == 2) {
		if (the_port->read_pending) {
			printf("FATAL: SerialPrimeIn() called while request is pending\n");
			return readErr;
		} else
			return the_port->prime_in(pb, dce);
	} else {
		if (the_port->write_pending) {
			printf("FATAL: SerialPrimeOut() called while request is pending\n");
			return readErr;
		} else
			return the_port->prime_out(pb, dce);
	}
}


/*
 *  Driver Control() routine
 */

int16 SerialControl(uint32 pb, uint32 dce, int port)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SerialControl %d, port %d, pb %08lx, dce %08lx\n", code, port, pb, dce));

	// Error if port is not open
	SERDPort *the_port = the_serd_port[port >> 1];
	if (!the_port->is_open)
		return notOpenErr;

	switch (code) {
		case kSERDSetPollWrite:
			return noErr;

		default:
			return the_port->control(pb, dce, code);
	}
}


/*
 *  Driver Status() routine
 */

int16 SerialStatus(uint32 pb, uint32 dce, int port)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("SerialStatus %d, port %d, pb %08lx, dce %08lx\n", code, port, pb, dce));

	// Error if port is not open
	SERDPort *the_port = the_serd_port[port >> 1];
	if (!the_port->is_open)
		return notOpenErr;

	switch (code) {
		case kSERDVersion:
			WriteMacInt8(pb + csParam, 9);		// Second-generation SerialDMA driver
			return noErr;

		case 0x8000:
			WriteMacInt8(pb + csParam, 9);		// Second-generation SerialDMA driver
			WriteMacInt16(pb + csParam + 4, 0x1997);	// Date of serial driver
			WriteMacInt16(pb + csParam + 6, 0x0616);
			return noErr;

		default:
			return the_port->status(pb, dce, code);
	}
}


/*
 *  Driver Close() routine
 */

int16 SerialClose(uint32 pb, uint32 dce, int port)
{
	D(bug("SerialClose port %d, pb %08lx, dce %08lx\n", port, pb, dce));

	if (port == 0 || port == 2) {

		// Do nothing for input side
		return noErr;

	} else {

		// Close port if open
		SERDPort *the_port = the_serd_port[port >> 1];
		if (the_port->is_open) {
			int16 res = the_port->close();
			M68kRegisters r;				// Free Deferred Task structures
			r.a[0] = the_port->input_dt;
			Execute68kTrap(0xa01f, &r);		// DisposePtr()
			the_port->is_open = false;
			return res;
		} else
			return noErr;
	}
}


/*
 *  Serial interrupt - Prime command completed, activate deferred tasks to call IODone
 */

static void serial_irq(SERDPort *p)
{
	if (p->is_open) {
		if (p->read_pending && p->read_done) {
			EnqueueMac(p->input_dt, 0xd92);
			p->read_pending = p->read_done = false;
		}
		if (p->write_pending && p->write_done) {
			EnqueueMac(p->output_dt, 0xd92);
			p->write_pending = p->write_done = false;
		}
	}
}

void SerialInterrupt(void)
{
	D(bug("SerialIRQ\n"));

	serial_irq(the_serd_port[0]);
	serial_irq(the_serd_port[1]);
}
