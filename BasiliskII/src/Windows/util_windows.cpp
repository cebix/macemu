/*
 *  util_windows.cpp - Miscellaneous utilities for Win32
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
#include "util_windows.h"
#include "main.h"

#include <list>
using std::list;

#include <string>
using std::string;

BOOL exists( const char *path )
{
	HFILE h;
	bool ret = false;

	h = _lopen( path, OF_READ );
	if(h != HFILE_ERROR) {
		ret = true;
		_lclose(h);
	}
	return(ret);
}

BOOL create_file( const char *path, DWORD size )
{
	HANDLE h;
	bool ok = false;

	h = CreateFile( path,
		GENERIC_READ | GENERIC_WRITE,
		0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL
	);
	if(h != INVALID_HANDLE_VALUE) {
		if(size == 0) {
			ok = true;
		} else if(SetFilePointer( h, size, NULL, FILE_BEGIN) != 0xFFFFFFFF) {
			if(SetEndOfFile(h)) {
				ok = true;
				if(SetFilePointer( h, 0, NULL, FILE_BEGIN) != 0xFFFFFFFF) {
					DWORD written;
					DWORD zeroed_size = size;
					if (zeroed_size > 1024*1024)
						zeroed_size = 1024*1024;
					char *b = (char *)malloc(zeroed_size);
					if(b) {
						memset( b, 0, zeroed_size );
						WriteFile( h, b, zeroed_size, &written, NULL );
						free(b);
					}
				}
			}
		}
		CloseHandle(h);
	}
	if(!ok) DeleteFile(path);
	return(ok);
}

int32 get_file_size( const char *path )
{
	HANDLE h;
	DWORD size = 0;

	h = CreateFile( path,
		GENERIC_READ,
		0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
	);
	if(h != INVALID_HANDLE_VALUE) {
		size = GetFileSize( h, NULL );
		CloseHandle(h);
	}
	return(size);
}


/*
 *  Thread wrappers
 */

HANDLE create_thread(LPTHREAD_START_ROUTINE start_routine, void *arg)
{
	DWORD dwThreadId;
	return CreateThread(NULL, 0, start_routine, arg, 0, &dwThreadId);
}

void wait_thread(HANDLE thread)
{
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

void kill_thread(HANDLE thread)
{
	TerminateThread(thread, 0);
}


/*
 *  Check that drivers are installed
 */

bool check_drivers(void)
{
	char path[_MAX_PATH];
	GetSystemDirectory(path, sizeof(path));
	strcat(path, "\\drivers\\cdenable.sys");

	if (exists(path)) {
		int32 size = get_file_size(path);
		if (size != 6112) {
			char str[256];
			sprintf(str, "The CD-ROM driver file \"%s\" is too old or corrupted.", path);
			ErrorAlert(str);
			return false;
		}
	}
	else {
		char str[256];
		sprintf(str, "The CD-ROM driver file \"%s\" is missing.", path);
		WarningAlert(str);
	}

	return true;
}


/*
 *  Network control panel helpers
 */

struct panel_reg {
	string name;
	string guid;
};

static list<panel_reg> network_registry;
typedef list<panel_reg>::const_iterator network_registry_iterator;

#define NETWORK_CONNECTIONS_KEY \
		"SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

static void get_network_registry(void)
{
	LONG status;
	HKEY network_connections_key;
	DWORD len;
	int i = 0;

	if (network_registry.size() > 0)
		return;

	status = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		NETWORK_CONNECTIONS_KEY,
		0,
		KEY_READ,
		&network_connections_key);

	if (status != ERROR_SUCCESS)
		return;

	while (true) {
		char enum_name[256];
		char connection_string[256];
		HKEY connection_key;
		char name_data[256];
		DWORD name_type;
		const char name_string[] = "Name";

		len = sizeof (enum_name);
		status = RegEnumKeyEx(
			network_connections_key,
			i,
			enum_name,
			&len,
			NULL,
			NULL,
			NULL,
			NULL);
		if (status != ERROR_SUCCESS)
			break;

		snprintf (connection_string, sizeof(connection_string),
				  "%s\\%s\\Connection",
				  NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			connection_string,
			0,
			KEY_READ,
			&connection_key);

		if (status == ERROR_SUCCESS) {
			len = sizeof (name_data);
			status = RegQueryValueEx(
				connection_key,
				name_string,
				NULL,
				&name_type,
				(BYTE *)name_data,
				&len);

			if (status == ERROR_SUCCESS && name_type == REG_SZ) {
				panel_reg pr;
				pr.name = name_data;
				pr.guid = enum_name;
				network_registry.push_back(pr);
			}
			RegCloseKey (connection_key);
		}
		++i;
    }

	RegCloseKey (network_connections_key);
}

const char *ether_name_to_guid(const char *name)
{
	get_network_registry();

	for (network_registry_iterator it = network_registry.begin(); it != network_registry.end(); it++) {
		if (strcmp((*it).name.c_str(), name) == 0)
			return (*it).guid.c_str();
	}

	return NULL;
}

const char *ether_guid_to_name(const char *guid)
{
	get_network_registry();

	for (network_registry_iterator it = network_registry.begin(); it != network_registry.end(); it++) {
		if (strcmp((*it).guid.c_str(), guid) == 0)
			return (*it).name.c_str();
	}

	return NULL;
}


/*
 *  Get TAP-Win32 adapters
 */

#define ADAPTER_KEY "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

#define TAP_COMPONENT_ID "tap0801"

const char *ether_tap_devices(void)
{
	HKEY adapter_key;
	LONG status;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		ADAPTER_KEY,
		0,
		KEY_READ,
		&adapter_key);

	if (status != ERROR_SUCCESS)
		return NULL;

	list<string> devices;

	while (true) {
		char enum_name[256];
		char unit_string[256];
		HKEY unit_key;
		char component_id_string[] = "ComponentId";
		char component_id[256];
		char net_cfg_instance_id_string[] = "NetCfgInstanceId";
		char net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof (enum_name);
		status = RegEnumKeyEx(
			adapter_key,
			i,
			enum_name,
			&len,
			NULL,
			NULL,
			NULL,
			NULL);
		if (status != ERROR_SUCCESS)
			break;

		snprintf (unit_string, sizeof(unit_string), "%s\\%s",
				  ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			unit_string,
			0,
			KEY_READ,
			&unit_key);

		if (status == ERROR_SUCCESS) {
			len = sizeof (component_id);
			status = RegQueryValueEx(
				unit_key,
				component_id_string,
				NULL,
				&data_type,
				(BYTE *)component_id,
				&len);

			if (status == ERROR_SUCCESS && data_type == REG_SZ) {
				len = sizeof (net_cfg_instance_id);
				status = RegQueryValueEx(
					unit_key,
					net_cfg_instance_id_string,
					NULL,
					&data_type,
					(BYTE *)net_cfg_instance_id,
					&len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ) {
					if (!strcmp (component_id, TAP_COMPONENT_ID))
						devices.push_back(net_cfg_instance_id);
				}
			}
			RegCloseKey (unit_key);
		}
		++i;
    }

	RegCloseKey (adapter_key);

	if (devices.empty())
		return NULL;

	// The result is a '\0' separated list of strings
	list<string>::const_iterator it;
	len = 0;
	for (it = devices.begin(); it != devices.end(); it++)
		len += (*it).length() + 1;

	char *names = (char *)malloc(len);
	if (names) {
		char *p = names;
		for (it = devices.begin(); it != devices.end(); it++) {
			len = (*it).length();
			strcpy(p, (*it).c_str());
			p[len] = '\0';
			p += len + 1;
		}
	}

	return names;
}
