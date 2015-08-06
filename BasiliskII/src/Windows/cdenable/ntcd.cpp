/*
 *  ntcd.cpp - Interface to cdenable.sys driver
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

#include "sysdeps.h"

extern "C" {

#include <winioctl.h>
#include <winsvc.h>
#include "ntcd.h"
#include "cdenable.h"

static LPCTSTR sDriverShort   = TEXT("cdenable");
static LPCTSTR sDriverLong    = TEXT("System32\\Drivers\\cdenable.sys");
static LPCTSTR sCompleteName  = TEXT("\\\\.\\cdenable");

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Start type must be SERVICE_AUTO_START or lower, in order
// it to start automatically and allow the mechanism work
// for users with no admin rights.
static BOOL InstallDriver(
  IN SC_HANDLE  SchSCManager,
  IN LPCTSTR    DriverName,
  IN LPCTSTR    ServiceExe
)
{
  SC_HANDLE  schService;
  DWORD      err;

  schService = CreateService (
		SchSCManager,          // SCManager database
    DriverName,            // name of service
    DriverName,            // name to display
    SERVICE_ALL_ACCESS,    // desired access
    SERVICE_KERNEL_DRIVER, // service type
		SERVICE_AUTO_START,		 // SERVICE_DEMAND_START,  // start type
    SERVICE_ERROR_NORMAL,  // error control type
    ServiceExe,            // service's binary
    NULL,                  // no load ordering group
    NULL,                  // no tag identifier
    NULL,                  // no dependencies
    NULL,                  // LocalSystem account
    NULL                   // no password
    );

  if (schService == NULL) {
      err = GetLastError();
      if (err == ERROR_SERVICE_EXISTS) {
				  return TRUE;
      } else {
		      return FALSE;
      }
  }
  CloseServiceHandle (schService);
  return TRUE;
}

static BOOL RemoveDriver(
  IN SC_HANDLE  SchSCManager,
  IN LPCTSTR    DriverName
)
{
  SC_HANDLE  schService;
  BOOL       ret;

  schService = OpenService (SchSCManager,
                            DriverName,
                            SERVICE_ALL_ACCESS
                            );
  if (schService == NULL) return FALSE;
  ret = DeleteService (schService);
  CloseServiceHandle (schService);
  return ret;
}

static BOOL StartDriver(
  IN SC_HANDLE  SchSCManager,
  IN LPCTSTR    DriverName
) {
  SC_HANDLE  schService;
  BOOL       ret;
  DWORD      err;

  schService = OpenService (SchSCManager,
                            DriverName,
                            SERVICE_ALL_ACCESS
                            );
  if (schService == NULL) return FALSE;
  ret = StartService (schService,    // service identifier
                      0,             // number of arguments
                      NULL           // pointer to arguments
                      );
  if(ret == 0) {
    err = GetLastError();
    if (err == ERROR_SERVICE_ALREADY_RUNNING) {
			ret = TRUE;
    } else {
			ret = FALSE;
		}
  }
  CloseServiceHandle (schService);
  return ret;
}

static BOOL StopDriver(
  IN SC_HANDLE  SchSCManager,
  IN LPCTSTR    DriverName
)
{
  SC_HANDLE       schService;
  BOOL            ret;
  SERVICE_STATUS  serviceStatus;

  schService = OpenService (SchSCManager,
                            DriverName,
                            SERVICE_ALL_ACCESS
                            );
  if (schService == NULL) return FALSE;
  ret = ControlService (schService,
                        SERVICE_CONTROL_STOP,
                        &serviceStatus
                        );
  CloseServiceHandle (schService);
  return ret;
}

static BOOL __cdecl start_driver( void )
{
	SC_HANDLE   schSCManager;
	BOOL ret = FALSE;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if(!schSCManager) return(FALSE);
	if(!InstallDriver( schSCManager, sDriverShort, sDriverLong )) {
		CloseServiceHandle( schSCManager );
		return(FALSE);
	}
	ret = StartDriver( schSCManager, sDriverShort );
	if(!ret) {
		(void)RemoveDriver( schSCManager, sDriverShort );
	}
	CloseServiceHandle( schSCManager );
	return( ret );
}

static BOOL __cdecl stop_driver( void )
{
	SC_HANDLE   schSCManager;
	BOOL ret = FALSE;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if(!schSCManager) return(FALSE);
	if(StopDriver( schSCManager, sDriverShort )) ret = TRUE;
	CloseServiceHandle( schSCManager );
	return( ret );
}

static BOOL __cdecl remove_driver( void )
{
	SC_HANDLE   schSCManager;
	BOOL ret = FALSE;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if(!schSCManager) return(FALSE);
	if(RemoveDriver( schSCManager, sDriverShort )) ret = TRUE;
	CloseServiceHandle( schSCManager );
	return( ret );
}



// Exported stuff begins

int CdenableSysReadCdBytes( HANDLE h, DWORD start, DWORD count, char *buf )
{
  HANDLE   hDevice;
  int      ret;
	DWORD		 nb;
	DWORD    in_buffer[10];
	DWORD    out_buffer[10];

  ret = 0;

	in_buffer[0] = (DWORD)h;
	in_buffer[1] = (DWORD)start;
	in_buffer[2] = (DWORD)count;
	in_buffer[3] = (DWORD)buf;
	out_buffer[0] = 0;

  hDevice = CreateFile (sCompleteName,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                        );

  if (hDevice == ((HANDLE)-1)) {
    ret = 0;
	} else {
		if ( DeviceIoControl(	hDevice,
					IOCTL_CDENABLE_READ,
					(LPVOID)in_buffer, 16,
					(LPVOID)out_buffer, 4,
					&nb, NULL ) )
		{
			if(out_buffer[0] != 0) ret = count;
		}
    CloseHandle (hDevice);
  }

  return ret;
}

int CdenableSysReadCdSectors( HANDLE h, DWORD start, DWORD count, char *buf )
{
	return( CdenableSysReadCdBytes( h, (start<<11), (count<<11), buf ) );
}

int CdenableSysWriteCdBytes( HANDLE h, DWORD start, DWORD count, char *buf )
{
	return( 0 );

	/*
  HANDLE   hDevice;
  int      ret;
	DWORD		 nb;
	DWORD    in_buffer[10];
	DWORD    out_buffer[10];

  ret = 0;

	in_buffer[0] = (DWORD)h;
	in_buffer[1] = (DWORD)start;
	in_buffer[2] = (DWORD)count;
	in_buffer[3] = (DWORD)buf;
	out_buffer[0] = 0;

  hDevice = CreateFile (sCompleteName,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                        );

  if (hDevice == ((HANDLE)-1)) {
    ret = 0;
	} else {
		if ( DeviceIoControl(	hDevice,
					IOCTL_CDENABLE_WRITE,
					(LPVOID)in_buffer, 16,
					(LPVOID)out_buffer, 4,
					&nb, NULL ) )
		{
			if(out_buffer[0] != 0) ret = count;
		}
    CloseHandle (hDevice);
  }

  return ret;
	*/
}

int CdenableSysWriteCdSectors( HANDLE h, DWORD start, DWORD count, char *buf )
{
	// return( CdenableSysWriteCdBytes( h, (start<<11), (count<<11), buf ) );
	return( 0 );
}

BOOL CdenableSysInstallStart(void)
{
	return(start_driver());
}

void CdenableSysStopRemove(void)
{
	stop_driver();
	remove_driver();
}

DWORD CdenableSysGetVersion( void )
{
  HANDLE   hDevice;
  DWORD    ret;
	DWORD		 nb;
	DWORD    out_buffer[10];

  ret = 0;
	out_buffer[0] = 0;
  hDevice = CreateFile (sCompleteName,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                        );
  if (hDevice == ((HANDLE)-1)) {
    ret = 0;
	} else {
		if ( DeviceIoControl(	hDevice,
					IOCTL_CDENABLE_GET_VERSION,
					NULL, 0,
					(LPVOID)out_buffer, 4,
					&nb, NULL ) )
		{
			ret = out_buffer[0];
		}
    CloseHandle (hDevice);
  }
  return ret;
}

#ifdef __cplusplus
} //extern "C"
#endif

