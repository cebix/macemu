/*
 *  serial_dummy.cpp - Serial device driver, dummy implementation
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
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "serial.h"
#include "serial_defs.h"

#define DEBUG 0
#include "debug.h"


// Driver private variables
class DSERDPort : public SERDPort {
public:
	DSERDPort(const char *dev)
	{
		device_name = (char *)dev;
	}

	virtual ~DSERDPort()
	{
	}

	virtual int16 open(uint16 config);
	virtual int16 prime_in(uint32 pb, uint32 dce);
	virtual int16 prime_out(uint32 pb, uint32 dce);
	virtual int16 control(uint32 pb, uint32 dce, uint16 code);
	virtual int16 status(uint32 pb, uint32 dce, uint16 code);
	virtual int16 close(void);

private:
	char *device_name;			// Device name
};


/*
 *  Initialization
 */

void SerialInit(void)
{
	// Read serial preferences and create structs for both ports
	the_serd_port[0] = new DSERDPort(PrefsFindString("seriala"));
	the_serd_port[1] = new DSERDPort(PrefsFindString("serialb"));
}


/*
 *  Deinitialization
 */

void SerialExit(void)
{
	delete (DSERDPort *)the_serd_port[0];
	delete (DSERDPort *)the_serd_port[1];
}


/*
 *  Open serial port
 */

int16 DSERDPort::open(uint16 config)
{
	return openErr;
}


/*
 *  Read data from port
 */

int16 DSERDPort::prime_in(uint32 pb, uint32 dce)
{
	return readErr;
}


/*
 *  Write data to port
 */

int16 DSERDPort::prime_out(uint32 pb, uint32 dce)
{
	return writErr;
}


/*
 *	Control calls
 */
 
int16 DSERDPort::control(uint32 pb, uint32 dce, uint16 code)
{
	return controlErr;
}


/*
 *	Status calls
 */

int16 DSERDPort::status(uint32 pb, uint32 dce, uint16 code)
{
	return statusErr;
}


/*
 *	Close serial port
 */

int16 DSERDPort::close()
{
	return noErr;
}
