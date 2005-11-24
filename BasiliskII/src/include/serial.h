/*
 *  serial.h - Serial device driver
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

#ifndef SERIAL_H
#define SERIAL_H

/*
 *  port:
 *    0 - .AIn
 *    1 - .AOut
 *    2 - .BIn
 *    3 - .BOut
 */

#ifdef POWERPC_ROM
extern int16 SerialOpen(uint32 pb, uint32 dce);
extern int16 SerialPrimeIn(uint32 pb, uint32 dce);
extern int16 SerialPrimeOut(uint32 pb, uint32 dce);
extern int16 SerialControl(uint32 pb, uint32 dce);
extern int16 SerialStatus(uint32 pb, uint32 dce);
extern int16 SerialClose(uint32 pb, uint32 dce);
extern int16 SerialNothing(uint32 pb, uint32 dce);
#else
extern int16 SerialOpen(uint32 pb, uint32 dce, int port);
extern int16 SerialPrime(uint32 pb, uint32 dce, int port);
extern int16 SerialControl(uint32 pb, uint32 dce, int port);
extern int16 SerialStatus(uint32 pb, uint32 dce, int port);
extern int16 SerialClose(uint32 pb, uint32 dce, int port);
#endif

extern void SerialInterrupt(void);

// System specific and internal functions/data
extern void SerialInit(void);
extern void SerialExit(void);

// Serial driver Deferred Task structure
enum {
	serdtCode = 20,		// DT code is stored here
	serdtResult = 30,
	serdtDCE = 34,
	SIZEOF_serdt = 38
};

// Variables for one (In/Out combined) serial port
// To implement a serial driver, you create a subclass of SERDPort
class SERDPort {
public:
	SERDPort()
	{
		is_open = false;
		input_dt = output_dt = 0;
	}

	virtual ~SERDPort() {}

	virtual int16 open(uint16 config) = 0;
	virtual int16 prime_in(uint32 pb, uint32 dce) = 0;
	virtual int16 prime_out(uint32 pb, uint32 dce) = 0;
	virtual int16 control(uint32 pb, uint32 dce, uint16 code) = 0;
	virtual int16 status(uint32 pb, uint32 dce, uint16 code) = 0;
	virtual int16 close(void) = 0;

	bool is_open;		// Port has been opened
	uint8 cum_errors;	// Cumulative errors

	bool read_pending;	// Read operation pending
	bool read_done;		// Read operation complete
	uint32 input_dt;	// Mac address of Deferred Task for reading

	bool write_pending;	// Write operation pending
	bool write_done;	// Write operation complete
	uint32 output_dt;	// Mac address of Deferred Task for writing

#ifdef POWERPC_ROM
	uint32 dt_store;
#endif
};

extern SERDPort *the_serd_port[2];

#endif
