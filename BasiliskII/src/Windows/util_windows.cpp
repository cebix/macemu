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
#include <io.h>
#include <fcntl.h>

#include <list>
using std::list;

#include <string>
using std::string;
using std::wstring;
typedef std::basic_string<TCHAR> tstring;

std::unique_ptr<char[]> str(const wchar_t* s)
{
	auto length = WideCharToMultiByte(CP_ACP, 0, s, -1, nullptr, 0, nullptr, nullptr);
	if (length == -1)
		return nullptr;

	std::unique_ptr<char[]> p(new char[length]);
	WideCharToMultiByte(CP_ACP, 0, s, -1, p.get(), length, nullptr, nullptr);
	return p;
}

std::unique_ptr<wchar_t[]> wstr(const char* s)
{
	auto length = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
	if (length == -1)
		return nullptr;

	std::unique_ptr<wchar_t[]> p(new wchar_t[length]);
	MultiByteToWideChar(CP_ACP, 0, s, -1, p.get(), length);
	return p;
}

string to_string(const wchar_t* s)
{
	auto wlen = wcslen(s);	// length without null terminator
	auto len = WideCharToMultiByte(CP_ACP, 0, s, wlen, nullptr, 0, nullptr, nullptr);
	if (len == -1)
		return string();

	string str(len, '\0');
	WideCharToMultiByte(CP_ACP, 0, s, wlen, &str.front(), len, nullptr, nullptr);
	return str;
}

wstring to_wstring(const char* s)
{
	auto len = strlen(s);	// length without null terminator
	auto wlen = MultiByteToWideChar(CP_ACP, 0, s, len, nullptr, 0);
	if (len == -1)
		return wstring();

	wstring str(wlen, L'\0');
	MultiByteToWideChar(CP_ACP, 0, s, len, &str.front(), wlen);
	return str;
}

size_t strlcpy(char* dst, const char* src, size_t size)
{
	size_t length = strlen(src);
	if (size-- > 0) {
		if (length < size)
			size = length;
		memcpy(dst, src, size);
		dst[size] = '\0';
	}
	return length;
}

size_t strlcpy(char* dst, const wchar_t* src, size_t size)
{
	size_t length = WideCharToMultiByte(CP_ACP, 0, src, -1, dst, size, nullptr, nullptr);
	if (size > 0) {
		if (length == 0)
			return strlcpy(dst, str(src).get(), size);
		--length;
	}
	return length;
}

size_t strlcat(char* dst, const char* src, size_t size)
{
	char* end = static_cast<char*>(memchr(dst, '\0', size));
	if (end == nullptr)
		return size;
	size_t length = end - dst;
	return length + strlcpy(end, src, size - length);
}

size_t strlcat(char* dst, const wchar_t* src, size_t size)
{
	char* end = static_cast<char*>(memchr(dst, '\0', size));
	if (end == nullptr)
		return size;
	size_t length = end - dst;
	return length + strlcpy(end, src, size - length);
}

size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t size)
{
	size_t length = wcslen(src);
	if (size-- > 0) {
		if (length < size)
			size = length;
		wmemcpy(dst, src, size);
		dst[size] = '\0';
	}
	return length;
}

size_t wcslcpy(wchar_t* dst, const char* src, size_t size)
{
	size_t length = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, size);
	if (size > 0) {
		if (length == 0)
			return wcslcpy(dst, wstr(src).get(), size);
		--length;
	}
	return length;
}

size_t wcslcat(wchar_t* dst, const wchar_t* src, size_t size)
{
	wchar_t* end = wmemchr(dst, L'\0', size);
	if (end == nullptr)
		return size;
	size_t length = end - dst;
	return length + wcslcpy(end, src, size - length);
}

size_t wcslcat(wchar_t* dst, const char* src, size_t size)
{
	wchar_t* end = wmemchr(dst, L'\0', size);
	if (end == nullptr)
		return size;
	size_t length = end - dst;
	return length + wcslcpy(end, src, size - length);
}

BOOL exists( const TCHAR *path )
{
	HFILE h;
	bool ret = false;

	h = _topen( path, _O_RDONLY | _O_BINARY );
	if(h != -1) {
		ret = true;
		_close(h);
	}
	return(ret);
}

BOOL create_file( const TCHAR *path, DWORD size )
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

int32 get_file_size( const TCHAR *path )
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
	TCHAR path[_MAX_PATH];
	GetSystemDirectory(path, lengthof(path));
	_tcscat(path, TEXT("\\drivers\\cdenable.sys"));

	if (exists(path)) {
		int32 size = get_file_size(path);
		if (size != 6112) {
			TCHAR str[256];
			_sntprintf(str, lengthof(str), TEXT("The CD-ROM driver file \"%s\" is too old or corrupted."), path);
			ErrorAlert(str);
			return false;
		}
	}
	else {
		TCHAR str[256];
		_sntprintf(str, lengthof(str), TEXT("The CD-ROM driver file \"%s\" is missing."), path);
		WarningAlert(str);
	}

	return true;
}


/*
 *  Network control panel helpers
 */

struct panel_reg {
	tstring name;
	tstring guid;
};

static list<panel_reg> network_registry;
typedef list<panel_reg>::const_iterator network_registry_iterator;

#define NETWORK_CONNECTIONS_KEY \
		TEXT("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}")

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
		TCHAR enum_name[256];
		TCHAR connection_string[256];
		HKEY connection_key;
		TCHAR name_data[256];
		DWORD name_type;
		const TCHAR name_string[] = TEXT("Name");

		len = lengthof(enum_name);
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

		_sntprintf (connection_string, lengthof(connection_string),
				  TEXT("%s\\%s\\Connection"),
				  NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			connection_string,
			0,
			KEY_READ,
			&connection_key);

		if (status == ERROR_SUCCESS) {
			len = lengthof(name_data);
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

const TCHAR *ether_name_to_guid(const TCHAR *name)
{
	get_network_registry();

	for (network_registry_iterator it = network_registry.begin(); it != network_registry.end(); it++) {
		if (_tcscmp((*it).name.c_str(), name) == 0)
			return (*it).guid.c_str();
	}

	return NULL;
}

const TCHAR *ether_guid_to_name(const TCHAR *guid)
{
	get_network_registry();

	for (network_registry_iterator it = network_registry.begin(); it != network_registry.end(); it++) {
		if (_tcscmp((*it).guid.c_str(), guid) == 0)
			return (*it).name.c_str();
	}

	return NULL;
}


/*
 *  Get TAP-Win32 adapters
 */

#define ADAPTER_KEY TEXT("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}")

const _TCHAR * tap_component_ids[] = { TEXT("tap0801"), TEXT("tap0901"), 0 };

const TCHAR *ether_tap_devices(void)
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

	list<tstring> devices;

	while (true) {
		TCHAR enum_name[256];
		TCHAR unit_string[256];
		HKEY unit_key;
		TCHAR component_id_string[] = TEXT("ComponentId");
		TCHAR component_id[256];
		TCHAR net_cfg_instance_id_string[] = TEXT("NetCfgInstanceId");
		TCHAR net_cfg_instance_id[256];
		DWORD data_type;

		len = lengthof(enum_name);
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

		_sntprintf (unit_string, lengthof(unit_string), TEXT("%s\\%s"),
				  ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			unit_string,
			0,
			KEY_READ,
			&unit_key);

		if (status == ERROR_SUCCESS) {
			len = lengthof(component_id);
			status = RegQueryValueEx(
				unit_key,
				component_id_string,
				NULL,
				&data_type,
				(BYTE *)component_id,
				&len);

			if (status == ERROR_SUCCESS && data_type == REG_SZ) {
				len = lengthof(net_cfg_instance_id);
				status = RegQueryValueEx(
					unit_key,
					net_cfg_instance_id_string,
					NULL,
					&data_type,
					(BYTE *)net_cfg_instance_id,
					&len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ) {
					const _TCHAR * * cur_tap_component_id;
					for (cur_tap_component_id = tap_component_ids; *cur_tap_component_id != NULL; cur_tap_component_id++) {
						if (!_tcscmp(component_id, *cur_tap_component_id)) {
							devices.push_back(net_cfg_instance_id);
							break;
						}
					}
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
	list<tstring>::const_iterator it;
	len = 0;
	for (it = devices.begin(); it != devices.end(); it++)
		len += (*it).length() + 1;

	TCHAR *names = (TCHAR *)malloc(len * sizeof(TCHAR));
	if (names) {
		TCHAR *p = names;
		for (it = devices.begin(); it != devices.end(); it++) {
			len = (*it).length();
			_tcscpy(p, (*it).c_str());
			p[len] = '\0';
			p += len + 1;
		}
	}

	return names;
}
