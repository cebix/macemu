/*
 *  serial_windows.cpp - Serial device driver for Win32
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
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

// TODO: serial i/o threads should have high priority.
#include "sysdeps.h"

#include <ctype.h>
#include <process.h>

#include "main.h"
#include "util_windows.h"
#include "macos_util.h"
#include "prefs.h"
#include "serial.h"
#include "serial_defs.h"
#include "cpu_emulation.h"

// This must be always on.
#define DEBUG 1
#undef OutputDebugString
#define OutputDebugString serial_log_write
static void serial_log_write( char *s );
#define SERIAL_LOG_FILE_NAME TEXT("serial.log")
#include "debug.h"
#undef D
#define D(x) if(debug_serial != DB_SERIAL_NONE) (x);


enum {
	DB_SERIAL_NONE=0,
	DB_SERIAL_NORMAL,
	DB_SERIAL_LOUD
};

static int16 debug_serial = DB_SERIAL_NONE;

static HANDLE serial_log_file = INVALID_HANDLE_VALUE;

static void serial_log_open( LPCTSTR path )
{
	if(debug_serial == DB_SERIAL_NONE) return;

	DeleteFile( path );
	serial_log_file = CreateFile(
			path,
			GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			FILE_FLAG_WRITE_THROUGH,
			NULL
	);
	if( serial_log_file == INVALID_HANDLE_VALUE ) {
		ErrorAlert( "Could not create the serial log file." );
	}
}

static void serial_log_close( void )
{
	if(debug_serial == DB_SERIAL_NONE) return;

	if( serial_log_file != INVALID_HANDLE_VALUE ) {
		CloseHandle( serial_log_file );
		serial_log_file = INVALID_HANDLE_VALUE;
	}
}

static void serial_log_write( char *s )
{
	DWORD bytes_written;

	// should have been checked already.
	if(debug_serial == DB_SERIAL_NONE) return;

	if( serial_log_file != INVALID_HANDLE_VALUE ) {

		DWORD count = strlen(s);
		if (0 == WriteFile(serial_log_file, s, count, &bytes_written, NULL) ||
				(int)bytes_written != count)
		{
			serial_log_close();
			ErrorAlert( "serial log file write error (out of disk space?). Log closed." );
		} else {
			FlushFileBuffers( serial_log_file );
		}
	}
}


// Driver private variables
class XSERDPort : public SERDPort {
public:
  XSERDPort(LPCTSTR dev, LPCTSTR suffix)
  {
		D(bug(TEXT("XSERDPort constructor %s\r\n"), dev));

		read_pending = write_pending = false;

		if(dev)
			_tcscpy( device_name, dev );
		else
			*device_name = 0;
		_tcsupr(device_name);
		is_parallel = (_tcsncmp(device_name, TEXT("LPT"), 3) == 0);
		is_file = (_tcsncmp(device_name, TEXT("FILE"), 4) == 0);
		if(is_file) {
			char entry_name[20];
			_snprintf( entry_name, lengthof(entry_name), "portfile%s", str(suffix).get() );
			const char *path = PrefsFindString(entry_name);
			if(path) {
				_tcscpy( output_file_name, tstr(path).get() );
			} else {
				_tcscpy( output_file_name, TEXT("C:\\B2TEMP.OUT") );
			}
		}

		is_serial = !is_parallel && !is_file;

		fd = INVALID_HANDLE_VALUE;
		input_thread_active = output_thread_active = NULL;
  }

  virtual ~XSERDPort()
  {
		D(bug("XSERDPort destructor \r\n"));
		if (input_thread_active) {
			D(bug("WARNING: brute TerminateThread(input)\r\n"));
			TerminateThread(input_thread_active,0);
			CloseHandle(input_signal);
			input_thread_active = NULL;
		}
		if (output_thread_active) {
			D(bug("WARNING: brute TerminateThread(output)\r\n"));
			TerminateThread(output_thread_active,0);
			CloseHandle(output_signal);
			output_thread_active = NULL;
		}
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
	static unsigned int WINAPI input_func(void *arg);
	static unsigned int WINAPI output_func(void *arg);
	static int acknowledge_error(HANDLE h, bool is_read);
	bool set_timeouts(int bauds, int parity_bits, int stop_bits);

	TCHAR device_name[256];
	HANDLE fd;

	bool io_killed;					// Flag: KillIO called, I/O threads must not call deferred tasks
	bool quitting;					// Flag: Quit threads

	HANDLE input_thread_active;		// Handle: Input thread installed (was a bool)
	unsigned int input_thread_id;
	HANDLE input_signal;				  // Signal for input thread: execute command
	uint32 input_pb, input_dce;		// Command parameters for input thread

	HANDLE output_thread_active;	// Handle: Output thread installed (was a bool)
	unsigned int output_thread_id;
	HANDLE output_signal;			    // Signal for output thread: execute command
	uint32 output_pb, output_dce;	// Command parameters for output thread

	DCB mode;			                // Terminal configuration

	bool is_serial;
	bool is_parallel;							// true if LPTx

	bool is_file;									// true if FILE
	TCHAR output_file_name[256];
};

/*
 *  Initialization
 */

void SerialInit(void)
{
	const char *port;

	debug_serial = PrefsFindInt32("debugserial");

	serial_log_open( SERIAL_LOG_FILE_NAME );

  // Read serial preferences and create structs for both ports

	port = PrefsFindString("seriala");
	if(port) {
		D(bug("SerialInit seriala=%s\r\n",port));
	}
  the_serd_port[0] = new XSERDPort(tstr(port).get(), TEXT("0"));

	port = PrefsFindString("serialb");
	if(port) {
		D(bug("SerialInit serialb=%s\r\n",port));
	}
  the_serd_port[1] = new XSERDPort(tstr(port).get(), TEXT("1"));
}


/*
 *  Deinitialization
 */

void SerialExit(void)
{
	D(bug("SerialExit\r\n"));
  if(the_serd_port[0]) delete (XSERDPort *)the_serd_port[0];
  if(the_serd_port[1]) delete (XSERDPort *)the_serd_port[1];
	D(bug("SerialExit done\r\n"));

	serial_log_close();
}


/*
 *  Open serial port
 */

int16 XSERDPort::open(uint16 config)
{
	// Don't open NULL name devices
	if (!device_name || !*device_name)
		return openErr;

	D(bug(TEXT("XSERDPort::open device=%s,config=0x%X\r\n"),device_name,(int)config));

	// Init variables
	io_killed = false;
	quitting = false;

	// Open port
	if(is_file) {
		DeleteFile( output_file_name );
		fd = CreateFile( output_file_name,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL
		);
	} else {
		fd = CreateFile( device_name, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0 );
	}
	if(fd == INVALID_HANDLE_VALUE) {
		goto open_error;
		D(bug(TEXT("XSERDPort::open failed to open port %s\r\n"),device_name));
	}

	if(is_serial) {
		// Configure port for raw mode
		memset( &mode, 0, sizeof(DCB) );
		mode.DCBlength = sizeof(mode);
		if(!GetCommState( fd, &mode ))
			goto open_error;

		mode.fBinary = TRUE;
		if(!configure(config)) {
			D(bug("XSERDPort::configure failed\r\n"));
			goto open_error;
		}
	}

	// Start input/output threads
	input_signal = CreateSemaphore( 0, 0, 1, NULL);
	if(!input_signal)
		goto open_error;

	output_signal = CreateSemaphore( 0, 0, 1, NULL);
	if(!output_signal)
		goto open_error;

	D(bug("Semaphores created\r\n"));

	input_thread_active = (HANDLE)_beginthreadex( 0, 0, input_func, (LPVOID)this, 0, &input_thread_id );
	output_thread_active = (HANDLE)_beginthreadex( 0, 0, output_func, (LPVOID)this, 0, &output_thread_id );

	if (!input_thread_active || !output_thread_active)
		goto open_error;

	D(bug("Threads created, Open returns success\r\n"));
	return noErr;

open_error:
	D(bug("Open cleanup after failure\r\n"));
	if (input_thread_active) {
		TerminateThread(input_thread_active,0);
		CloseHandle(input_signal);
		input_thread_active = false;
	}
	if (output_thread_active) {
		TerminateThread(output_thread_active,0);
		CloseHandle(output_signal);
		output_thread_active = false;
	}
	if(fd != INVALID_HANDLE_VALUE) {
		CloseHandle(fd);
		fd = 0;
	}
	return openErr;
}

/*
 *  Read data from port
 */

int16 XSERDPort::prime_in(uint32 pb, uint32 dce)
{
	D(bug("XSERDPort::prime_in\r\n"));
	// Send input command to input_thread
	read_done = false;
	read_pending = true;
	input_pb = pb;
	input_dce = dce;
	ReleaseSemaphore(input_signal,1,NULL);
	return 1;	// Command in progress
}


/*
 *  Write data to port
 */

int16 XSERDPort::prime_out(uint32 pb, uint32 dce)
{
	D(bug("XSERDPort::prime_out\r\n"));
	// Send output command to output_thread
	write_done = false;
	write_pending = true;
	output_pb = pb;
	output_dce = dce;
	ReleaseSemaphore(output_signal,1,NULL);
	return 1;	// Command in progress
}


static DWORD get_comm_output_buf_size( HANDLE h )
{
	DWORD size = 0;
	COMMPROP cp;

	if(GetCommProperties(h,&cp)) {
		size = cp.dwCurrentTxQueue;
	}
	return size;
}

/*
 *  Control calls
 */

int16 XSERDPort::control(uint32 pb, uint32 dce, uint16 code)
{
	D(bug("XSERDPort::control code=%d\r\n",(int)code));
	switch (code) {

		case kSERDClockMIDI:
			/* http://til.info.apple.com/techinfo.nsf/artnum/n2425
			 A MIDI interface operates at 31.25 Kbaud (+/- 1%) [== 31400]
			 asynchronously, using a data format of one start bit, eight
			 data bits, and one stop bit. This makes a total of 10 bits
			 for each 320 microsecond period per serial byte.
			*/
			D(bug("kSERDClockMIDI setting 38400,n,8,1\n"));
			return noErr;

			/*
			mode.BaudRate = 38400;
		  mode.ByteSize = 8;
		  mode.StopBits = ONESTOPBIT;
		  mode.Parity = NOPARITY;
			if(!SetCommState( fd, &mode )) {
				D(bug("kSERDClockMIDI SetCommState() failed\n"));
				return controlErr;
			} else {
				if(!set_timeouts(38400,0,2)) {
					D(bug("kSERDClockMIDI set_timeouts() failed\n"));
					return controlErr;
				}
				D(bug("kSERDClockMIDI OK\n"));
				return noErr;
			}
			*/

		case 1:			// KillIO
			io_killed = true;

			if(is_serial) {
				// Make sure we won't hang waiting. There is something wrong
				// in how read_pending & write_pending are handled.
				DWORD endtime = GetTickCount() + 1000;
 				while ( (read_pending || write_pending) && (GetTickCount() < endtime) ) {
					Sleep(20);
				}
				if(read_pending || write_pending) {
					D(bug("Warning (KillIO): read_pending=%d, write_pending=%d\n", read_pending, write_pending));
					read_pending = write_pending = false;
				}
				// | PURGE_TXABORT | PURGE_RXABORT not needed, no overlapped i/o
				PurgeComm(fd,PURGE_TXCLEAR|PURGE_RXCLEAR);
				FlushFileBuffers(fd);
			}
			io_killed = false;
			D(bug("KillIO done\n"));
			return noErr;

		case kSERDConfiguration:
			if (configure((uint16)ReadMacInt16(pb + csParam)))
				return noErr;
			else
				return paramErr;

		case kSERDInputBuffer:
			if(is_serial) {

				// SetupComm() wants both values, so we need to know the output size.
				DWORD osize = get_comm_output_buf_size(fd);

				DWORD isize = ReadMacInt16(pb + csParam + 4) & 0xffffffc0;

				// 1k minimum
				// Was this something Amiga specific -- do I need to do this?
				if (isize < 1024)
					isize = 1024;

				if(isize > 0 && osize > 0) {
					if(SetupComm( fd, isize, osize )) {
						D(bug(" buffer size is now %08lx\n", isize));
						return noErr;
					} else {
						D(bug(" SetupComm(%d,%d) failed, error = %08lx\n", isize, osize, GetLastError()));
					}
				}
			}
			// Always return ok.
			return noErr;

		case kSERDSerHShake:
			set_handshake(pb + csParam, false);
			return noErr;

		case kSERDSetBreak:
			if(is_serial) {
				if(!SetCommBreak(fd)) return controlErr;
			}
			return noErr;

		case kSERDClearBreak:
			if(is_serial) {
				if(!ClearCommBreak(fd)) return controlErr;
			}
			return noErr;

		case kSERDBaudRate: {
			if (is_serial) {
				uint16 rate = (uint16)ReadMacInt16(pb + csParam);
				int baud_rate;
				if (rate <= 50) {
					rate = 50; baud_rate = CBR_110;
				} else if (rate <= 75) {
					rate = 75; baud_rate = CBR_110;
				} else if (rate <= 110) {
					rate = 110; baud_rate = CBR_110;
				} else if (rate <= 134) {
					rate = 134; baud_rate = CBR_110;
				} else if (rate <= 150) {
					rate = 150; baud_rate = CBR_110;
				} else if (rate <= 200) {
					rate = 200; baud_rate = CBR_300;
				} else if (rate <= 300) {
					rate = 300; baud_rate = CBR_300;
				} else if (rate <= 600) {
					rate = 600; baud_rate = CBR_600;
				} else if (rate <= 1200) {
					rate = 1200; baud_rate = CBR_1200;
				} else if (rate <= 1800) {
					rate = 1800; baud_rate = CBR_2400;
				} else if (rate <= 2400) {
					rate = 2400; baud_rate = CBR_2400;
				} else if (rate <= 4800) {
					rate = 4800; baud_rate = CBR_4800;
				} else if (rate <= 9600) {
					rate = 9600; baud_rate = CBR_9600;
				} else if (rate <= 19200) {
					rate = 19200; baud_rate = CBR_19200;
				} else if (rate <= 38400) {
					rate = 38400; baud_rate = CBR_38400;
				} else if (rate <= 57600) {
					rate = 57600; baud_rate = CBR_57600;
				} else {
					rate = 57600; baud_rate = CBR_57600;
				}
				WriteMacInt16(pb + csParam, rate);
				mode.BaudRate = baud_rate;			
				if(!SetCommState( fd, &mode )) return controlErr;
				// TODO: save parity/stop values and use here (not critical)
				if(!set_timeouts(rate,0,1)) return controlErr;
			}
			return noErr;
		}

		case kSERDHandshake:
		case kSERDHandshakeRS232:
			set_handshake(pb + csParam, true);
			return noErr;

		case kSERDMiscOptions:
			if (ReadMacInt8(pb + csParam) & kOptionPreserveDTR)
			  mode.fDtrControl =  DTR_CONTROL_ENABLE; // correct?
			else
			  mode.fDtrControl =  DTR_CONTROL_DISABLE; // correct?
			if(is_serial) {
				if(!SetCommState( fd, &mode )) return controlErr;
			}
			return noErr;

		case kSERDAssertDTR: {
			if (is_serial) {
				if(!EscapeCommFunction(fd,SETDTR)) return controlErr;
			}
			return noErr;
		}

		case kSERDNegateDTR: {
			if (is_serial) {
				if(!EscapeCommFunction(fd,CLRDTR)) return controlErr;
			}
			return noErr;
		}

		case kSERDSetPEChar:
		case kSERDSetPEAltChar:
			{
			uint16 errChar = (uint16)ReadMacInt16(pb + csParam);
			mode.fErrorChar = TRUE;
			mode.ErrorChar = (char)errChar;
			return noErr;
			}

		case kSERDResetChannel:
			if (is_serial) {
				// | PURGE_TXABORT | PURGE_RXABORT not needed, no overlapped i/o
				PurgeComm(fd,PURGE_TXCLEAR|PURGE_RXCLEAR);
				FlushFileBuffers(fd);
			}
			return noErr;

		case kSERDAssertRTS: {
			if (is_serial) {
				if(!EscapeCommFunction(fd,SETRTS)) return controlErr;
			}
			return noErr;
		}

		case kSERDNegateRTS: {
			if (is_serial) {
				if(!EscapeCommFunction(fd,CLRRTS)) return controlErr;
			}
			return noErr;
		}

		case kSERD115KBaud:
			if (is_serial) {
				mode.BaudRate = CBR_115200;
				if(!SetCommState( fd, &mode )) return controlErr;
			}
			return noErr;

		case kSERD230KBaud:
		case kSERDSetHighSpeed:
			if (is_serial) {
				mode.BaudRate = CBR_256000;
				if(!SetCommState( fd, &mode )) return controlErr;
			}
			return noErr;

		default:
			D(bug("WARNING: SerialControl(): unimplemented control code %d\r\n", code));
			return controlErr;
	}
}

/*
 *  Status calls
 */

int16 XSERDPort::status(uint32 pb, uint32 dce, uint16 code)
{
	// D(bug("XSERDPort::status code=%d\r\n",(int)code));

	DWORD error_state;
	COMSTAT comstat;

	switch (code) {
		case kSERDInputCount: {
			uint32 num = 0;
			if (is_serial) {
				if(!ClearCommError(fd,&error_state,&comstat)) return statusErr;
				num = comstat.cbInQue;
			}
			WriteMacInt32(pb + csParam, num);
			return noErr;
		}

		case kSERDStatus: {
			uint32 p = pb + csParam;
			WriteMacInt8(p + staCumErrs, cum_errors);
			cum_errors = 0;
			DWORD status;

			if(is_serial) {
				if(!GetCommModemStatus(fd,&status)) return statusErr;
			} else {
				status = MS_CTS_ON | MS_DSR_ON | MS_RLSD_ON;
				D(bug("kSERDStatus: faking status for LPT port or FILE\r\n"));
			}

			WriteMacInt8(p + staXOffSent, 0);
			WriteMacInt8(p + staXOffHold, 0);
			WriteMacInt8(p + staRdPend, read_pending);
			WriteMacInt8(p + staWrPend, write_pending);

			WriteMacInt8(p + staCtsHold, status & MS_CTS_ON ? 0 : 1);
			WriteMacInt8(p + staDsrHold, status & MS_DSR_ON ? 0 : 1);

			WriteMacInt8(p + staModemStatus,
				(status & MS_DSR_ON ? dsrEvent : 0)
				| (status & MS_RING_ON ? riEvent : 0)
				| (status & MS_RLSD_ON ? dcdEvent : 0)   // is this carrier detect?
				| (status & MS_CTS_ON ? ctsEvent : 0));
			return noErr;
		}

		default:
			D(bug("WARNING: SerialStatus(): unimplemented status code %d\r\n", code));
			return statusErr;
	}
}


/*
 *  Close serial port
 */

int16 XSERDPort::close()
{
	D(bug("XSERDPort::close\r\n"));

	// Kill threads
	if (input_thread_active) {
		quitting = true;
		ReleaseSemaphore(input_signal,1,NULL);
		input_thread_active = false;
		CloseHandle(input_signal);
	}
	if (output_thread_active) {
		quitting = true;
		ReleaseSemaphore(output_signal,1,NULL);
		output_thread_active = false;
		// bugfix: was: CloseHandle(&output_signal);
		CloseHandle(output_signal);
	}

	// Close port
	if(fd != INVALID_HANDLE_VALUE) {
		CloseHandle(fd);
		fd = 0;
	}
  return noErr;
}

bool XSERDPort::set_timeouts(
	int bauds, int parity_bits, int stop_bits )
{
	COMMTIMEOUTS timeouts;
	uint32 bytes_per_sec;
	uint32 msecs_per_ch;
	bool result = false;

	// Should already been checked
	if (!is_serial)
		return true;

	bytes_per_sec = bauds / (mode.ByteSize + parity_bits + stop_bits);

	// 75% bytes_per_sec
	// bytes_per_sec = (bytes_per_sec+bytes_per_sec+bytes_per_sec) >> 2;

	// 50% bytes_per_sec
	bytes_per_sec = bytes_per_sec >> 1;

	msecs_per_ch = 1000 / bytes_per_sec;
	if(msecs_per_ch == 0) msecs_per_ch = 1;

	if(GetCommTimeouts(fd,&timeouts)) {
		D(bug("old timeout values: %ld %ld %ld %ld %ld\r\n",
			timeouts.ReadIntervalTimeout,
			timeouts.ReadTotalTimeoutMultiplier,
			timeouts.ReadTotalTimeoutConstant,
			timeouts.WriteTotalTimeoutMultiplier,
			timeouts.WriteTotalTimeoutConstant
		));

		timeouts.WriteTotalTimeoutMultiplier = msecs_per_ch;
		timeouts.WriteTotalTimeoutConstant = 10;

		/*
		timeouts.ReadIntervalTimeout = msecs_per_ch;
		timeouts.ReadTotalTimeoutMultiplier = msecs_per_ch;
		timeouts.ReadTotalTimeoutConstant = 10;
		*/

		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;

		if(!SetCommTimeouts(fd,&timeouts)) {
			D(bug("SetCommTimeouts() failed in configure()\r\n"));
		} else {
			D(bug("new timeout values: %ld %ld %ld %ld %ld\r\n",
				timeouts.ReadIntervalTimeout,
				timeouts.ReadTotalTimeoutMultiplier,
				timeouts.ReadTotalTimeoutConstant,
				timeouts.WriteTotalTimeoutMultiplier,
				timeouts.WriteTotalTimeoutConstant
			));
			result = true;
		}
	} else {
		D(bug("GetCommTimeouts() failed in set_timeouts()\r\n"));
	}
	return(result);
}

/*
 *  Configure serial port with MacOS config word
 */

bool XSERDPort::configure(uint16 config)
{
	D(bug("XSERDPort::configure, config=%d\r\n",(int)config));

	if (!is_serial)
		return true;

	// needed to calculate optimal timeouts
	uint32 bauds = 57600;
	uint32 stop_bits = 1;
	uint32 parity_bits = 0;

	// Not all of these can be set here anyway.
	/*
	mode.fOutxCtsFlow = TRUE;
	mode.fOutxDsrFlow = FALSE;
	mode.fDtrControl = DTR_CONTROL_ENABLE; //  DTR_CONTROL_HANDSHAKE?
  mode.fDsrSensitivity = FALSE; // ???
  mode.fOutX = FALSE;
  mode.fInX = FALSE;
  mode.fTXContinueOnXoff = FALSE;
  mode.fErrorChar = FALSE;
  mode.ErrorChar = 0;
  mode.fNull = FALSE;
  mode.fRtsControl = 2; // ???
  mode.fAbortOnError = FALSE;
  mode.XonLim = 0x800;
  mode.XoffLim = 0x200;
  mode.XonChar = 0x11;
  mode.XoffChar = 0x13;
  mode.EofChar = 0;
  mode.EvtChar = '\0';
	*/

	// Set baud rate
	switch (config & 0x03ff) {
		// no baud1800, CBR_14400, CBR_56000, CBR_115200, CBR_128000, CBR_256000
		case baud150: mode.BaudRate = CBR_110; bauds = 110; break;
		case baud300: mode.BaudRate = CBR_300; bauds = 300; break;
		case baud600: mode.BaudRate = CBR_600; bauds = 600; break;
		case baud1200: mode.BaudRate = CBR_1200; bauds = 1200; break;
		case baud1800: return false;
		case baud2400: mode.BaudRate = CBR_2400; bauds = 2400; break;
		case baud4800: mode.BaudRate = CBR_4800; bauds = 4800; break;
		case baud9600: mode.BaudRate = CBR_9600; bauds = 9600; break;
		case baud19200: mode.BaudRate = CBR_19200; bauds = 19200; break;
		case baud38400: mode.BaudRate = CBR_38400; bauds = 38400; break;
		case baud57600: mode.BaudRate = CBR_57600; bauds = 57600; break;
		default:
			return false;
	}

	// Set number of stop bits
	switch (config & 0xc000) {
		case stop10:
		  mode.StopBits = ONESTOPBIT;
			stop_bits = 1;
			break;
		case stop15:
		  mode.StopBits = ONE5STOPBITS;
			stop_bits = 2;
			break;
		case stop20:
		  mode.StopBits = TWOSTOPBITS;
			stop_bits = 2;
			break;
		default:
			return false;
	}

	// Set parity mode
	switch (config & 0x3000) {
		case noParity:
		  mode.Parity = NOPARITY;
		  mode.fParity = FALSE;
			parity_bits = 0;
			break;
		case oddParity:
		  mode.Parity = ODDPARITY;
		  mode.fParity = TRUE;
			parity_bits = 1;
			break;
		case evenParity:
		  mode.Parity = EVENPARITY;
		  mode.fParity = TRUE;
			parity_bits = 1;
			break;
		// No MARKPARITY, SPACEPARITY
		default:
			return false;
	}

	// Set number of data bits
	switch (config & 0x0c00) {
		// No data4
		case data5:
		  mode.ByteSize = 5; break;
		case data6:
		  mode.ByteSize = 6; break;
		case data7:
		  mode.ByteSize = 7; break;
		case data8:
		  mode.ByteSize = 8; break;
		default:
			return false;
	}

	D(bug("Interpreted configuration: %d,%d,%d,%d\r\n",
		bauds,
		mode.ByteSize,
		stop_bits,
		parity_bits
	));

	if(!SetCommState( fd, &mode )) {
		D(bug("SetCommState failed in configure()\r\n"));
		return false;
	}

	if(!set_timeouts(bauds,parity_bits,stop_bits))
		return false;

	return true;
}


/*
 *  Set serial handshaking
 */

void XSERDPort::set_handshake(uint32 s, bool with_dtr)
{
	D(bug(" set_handshake %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
		ReadMacInt8(s + 0), ReadMacInt8(s + 1), ReadMacInt8(s + 2), ReadMacInt8(s + 3),
		ReadMacInt8(s + 4), ReadMacInt8(s + 5), ReadMacInt8(s + 6), ReadMacInt8(s + 7)));

	if (!is_serial)
		return;

	if (with_dtr) {
		mode.fDtrControl = DTR_CONTROL_ENABLE;
		if (ReadMacInt8(s + shkFCTS) || ReadMacInt8(s + shkFDTR))
			mode.fOutxCtsFlow = TRUE;
		else
			mode.fOutxCtsFlow = FALSE;
	} else {
		mode.fDtrControl = DTR_CONTROL_DISABLE;
		if (ReadMacInt8(s + shkFCTS))
			mode.fOutxCtsFlow = TRUE;
		else
			mode.fOutxCtsFlow = FALSE;
	}

	// MIDI: set_handshake 00 00 f4 f5 21 00 00 00
	// shkFXOn = 0
	// shkFCTS = 0
	// shkXOn = f4
	// shkXOff = f5
	// shkErrs = 21 
	// shkEvts = 0
	// shkFInX = 0
	// shkFDTR = 0
	if (ReadMacInt8(s + shkXOn) && ReadMacInt8(s + shkXOn)) {
		mode.fOutX = 1;
		mode.fInX = 1;
		mode.XonChar = ReadMacInt8(s + shkXOn);
		mode.XoffChar = ReadMacInt8(s + shkXOff);
	} else {
		mode.fOutX = 0;
		mode.fInX = 0;
	}
	if (ReadMacInt8(s + shkErrs)) {
	  mode.ErrorChar = ReadMacInt8(s + shkErrs);
		mode.fErrorChar = 1;
	} else {
		mode.fErrorChar = 0;
	}

	(void)SetCommState( fd, &mode );

	// D(bug(" %sware flow control\r\n", mode.c_cflag & CRTSCTS ? "hard" : "soft"));
	// tcsetattr(fd, TCSANOW, &mode);
}

/*
	if mode.fAbortOnError is TRUE, ClearCommError() *MUST* be called
	after any read or write errors. Otherwise no i/o will occur again

	These error codes should be translated but the Mac Device Manager
	error code mnemonics are too cryptic to me.
*/

int XSERDPort::acknowledge_error(HANDLE h, bool is_read)
{
	DWORD error_state;
	COMSTAT comstat;
	int err;

	// default error code if cannot map correctly
	err = is_read ? readErr : writErr;

	if(ClearCommError(h,&error_state,&comstat)) {
		D(bug("A %s error 0x%X occured.\r\n", is_read ? "read" : "write", error_state));
		D(bug("There was %d bytes in input buffer and %d bytes in output buffer.\r\n",(int)comstat.cbInQue,(int)comstat.cbOutQue));
		if(error_state & CE_MODE) {
			D(bug("The requested mode is not supported.\r\n"));
		} else {
			if(error_state & CE_BREAK) {
				D(bug("The hardware detected a break condition.\r\n"));
			}
			if(error_state & CE_FRAME) {
				D(bug("The hardware detected a framing error.\r\n"));
			}
			if(error_state & CE_IOE) {
				D(bug("An I/O error occurred during communications with the device.\r\n"));
			}
			if(error_state & CE_RXOVER) {
				D(bug("An input buffer overflow has occurred.\r\n"));
			}
			if(error_state & CE_RXPARITY) {
				D(bug("The hardware detected a parity error.\r\n"));
				err = badDCksum;
			}
			if(error_state & CE_TXFULL) {
				D(bug("The application tried to transmit a character, but the output buffer was full.\r\n"));
			}

			// Win95 specific errors
			if(error_state & CE_OVERRUN) {
				D(bug("A character-buffer overrun has occurred. The next character is lost.\r\n"));
				if(!is_read) err = wrUnderrun;
			}

			// Win95 parallel devices really.
			if(error_state & CE_DNS) {
				D(bug("A parallel device is not selected (Windows 95).\r\n"));
			}
			if(error_state & CE_OOP) {
				D(bug("A parallel device signaled that it is out of paper (Windows 95 only).\r\n"));
				err = unitEmptyErr;
			}
			if(error_state & CE_PTO) {
				D(bug("A time-out occurred on a parallel device (Windows 95).\r\n"));
			}

		}
	} else {
		D(bug("Failed to resume after %s operation.\r\n",is_read ? "read" : "write"));
	}
	return(err);
}

#if DEBUG
static void dump_dirst_bytes( BYTE *buf, int32 actual )
{
	if(debug_serial != DB_SERIAL_LOUD) return;

	BYTE b[256];
	int32 i, bytes = min(actual,sizeof(b)-3);

	for (i=0; i<bytes; i++) {
		b[i] = isprint(buf[i]) ? buf[i] : '.';
	}
	b[i] = 0;
	strcat((char*)b,"\r\n");
	D(bug((char*)b));
}
#else
#define dump_dirst_bytes(b,a) {}
#endif

/*
 *  Data input thread
 */

unsigned int XSERDPort::input_func(void *arg)
{
	XSERDPort *s = (XSERDPort *)arg;
	int error_code;

#if 0
	SetThreadPriority( GetCurrentThread(), threads[THREAD_SERIAL_IN].priority_running );
	SetThreadAffinityMask( GetCurrentThread(), threads[THREAD_SERIAL_IN].affinity_mask );
	set_desktop();
#endif

	D(bug(TEXT("XSERDPort::input_func started for device %s\r\n"),s->device_name));

	for (;;) {

		// Wait for commands
		WaitForSingleObject(s->input_signal,INFINITE);
		if (s->quitting)
			break;

		// Execute command
		void *buf = Mac2HostAddr(ReadMacInt32(s->input_pb + ioBuffer));
		uint32 length = ReadMacInt32(s->input_pb + ioReqCount);
		D(bug("input_func waiting for %ld bytes of data...\r\n", length));

		if(length & 0xFFFF0000) {
			length &= 0x0000FFFF;
			D(bug("byte count fixed to be %ld...\r\n", length));
		}

		int32 actual;
		if(s->is_file) {
			actual = -1;
			error_code = readErr;
		} else if(!ReadFile(s->fd, buf, length, (LPDWORD)&actual, 0)) {
			actual = -1;
			if(s->is_serial)
				error_code = acknowledge_error(s->fd,true);
			else
				error_code = readErr;
		}
		D(bug(" %ld bytes received\r\n", actual));
		if(actual > 0) {
			dump_dirst_bytes( (BYTE*)buf, actual );
		}

		// KillIO called? Then simply return
		if (s->io_killed) {

			WriteMacInt16(s->input_pb + ioResult, abortErr);
			WriteMacInt32(s->input_pb + ioActCount, 0);
			s->read_pending = s->read_done = false;

		} else {

			// Set error code
			if (actual >= 0) {
				WriteMacInt32(s->input_pb + ioActCount, actual);
				WriteMacInt32(s->input_dt + serdtResult, noErr);
			} else {
				WriteMacInt32(s->input_pb + ioActCount, 0);
				WriteMacInt32(s->input_dt + serdtResult, error_code);
			}

			// Trigger serial interrupt
			D(bug(" triggering serial interrupt\r\n"));
			WriteMacInt32(s->input_dt + serdtDCE, s->input_dce);
			s->read_done = true;
			SetInterruptFlag(INTFLAG_SERIAL);
			TriggerInterrupt();
		}
	}

	D(bug("XSERDPort::input_func terminating gracefully\r\n"));

	_endthreadex( 0 );

	return(0);
}


/*
 *  Data output thread
 */

unsigned int XSERDPort::output_func(void *arg)
{
	XSERDPort *s = (XSERDPort *)arg;
	int error_code;

#if 0
	SetThreadPriority( GetCurrentThread(), threads[THREAD_SERIAL_OUT].priority_running );
	SetThreadAffinityMask( GetCurrentThread(), threads[THREAD_SERIAL_OUT].affinity_mask );
	set_desktop();
#endif

	D(bug(TEXT("XSERDPort::output_func started for device %s\r\n"),s->device_name));

	for (;;) {

		// Wait for commands
		WaitForSingleObject(s->output_signal,INFINITE);
		if (s->quitting)
			break;

		// Execute command
		void *buf = Mac2HostAddr(ReadMacInt32(s->output_pb + ioBuffer));
		uint32 length = ReadMacInt32(s->output_pb + ioReqCount);
		D(bug("output_func transmitting %ld bytes of data...\r\n", length));

		if(length & 0xFFFF0000) {
			length &= 0x0000FFFF;
			D(bug("byte count fixed to be %ld...\r\n", length));
		}

		int32 actual;
		if(!WriteFile(s->fd, buf, length, (LPDWORD)&actual, 0)) {
			actual = -1;
			if(s->is_serial)
				error_code = acknowledge_error(s->fd,false);
			else
				error_code = writErr;
		}
		D(bug(" %ld bytes transmitted\r\n", actual));
		if(actual > 0) {
			dump_dirst_bytes( (BYTE*)buf, actual );
		}

		// KillIO called? Then simply return
		if (s->io_killed) {

			WriteMacInt16(s->output_pb + ioResult, abortErr);
			WriteMacInt32(s->output_pb + ioActCount, 0);
			s->write_pending = s->write_done = false;

		} else {

			// Set error code
			if (actual >= 0) {
				WriteMacInt32(s->output_pb + ioActCount, actual);
				WriteMacInt32(s->output_dt + serdtResult, noErr);
			} else {
				WriteMacInt32(s->output_pb + ioActCount, 0);
				WriteMacInt32(s->output_dt + serdtResult, error_code);
			}

			// Trigger serial interrupt
			D(bug(" triggering serial interrupt\r\n"));
			WriteMacInt32(s->output_dt + serdtDCE, s->output_dce);
			s->write_done = true;
			SetInterruptFlag(INTFLAG_SERIAL);
			TriggerInterrupt();
		}
	}

	D(bug("XSERDPort::output_func terminating gracefully\r\n"));

	_endthreadex( 0 );

	return(0);
}
