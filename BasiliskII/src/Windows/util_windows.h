/*
 *  util_windows.h - Miscellaneous utilities for Win32
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

#ifndef _UTIL_WINDOWS_H
#define _UTIL_WINDOWS_H

#include <memory>
#include <string>

BOOL exists( const TCHAR *path );
int32 get_file_size( const TCHAR *path );
BOOL create_file( const TCHAR *path, DWORD size );
bool check_drivers(void);

// Thread wrappers
extern HANDLE create_thread(LPTHREAD_START_ROUTINE start_routine, void *arg = NULL);
extern void wait_thread(HANDLE thread);
extern void kill_thread(HANDLE thread);

// Mutex wrappers
class mutex_t {
    CRITICAL_SECTION cs;
 public:
    mutex_t()		{ InitializeCriticalSection(&cs); }
    ~mutex_t()		{ DeleteCriticalSection(&cs); }
    void lock()		{ EnterCriticalSection(&cs); }
    void unlock()	{ LeaveCriticalSection(&cs); }
};

// Network control panel helpers
extern const TCHAR *ether_name_to_guid(const TCHAR *name);
extern const TCHAR *ether_guid_to_name(const TCHAR *guid);

// Get TAP-Win32 devices (caller free()s returned buffer)
extern const TCHAR *ether_tap_devices(void);

// Wide string versions of commonly used functions
extern void ErrorAlert(const wchar_t *text);
extern void WarningAlert(const wchar_t *text);

// ----------------- String conversion functions -----------------

// Null deleter -- does nothing.  Allows returning a non-owning
// unique_ptr.  This should go away if observer_ptr makes it into
// the standard.
template <class T> struct null_delete {
    constexpr null_delete() noexcept = default;
    template <class U> null_delete(const null_delete<U>&) noexcept { }
    void operator ()(T*) const noexcept { }
};
template <class T> struct null_delete<T[]> {
    constexpr null_delete() noexcept = default;
    void operator ()(T*) const noexcept { }
    template <class U> void operator ()(U*) const = delete;
};

// Functions returning null-terminated C strings
std::unique_ptr<char[]>		str(const wchar_t* s);
std::unique_ptr<wchar_t[]>  wstr(const char* s);

inline std::unique_ptr<const char[], null_delete<const char[]>> str(const char* s)
{
    return std::unique_ptr<const char[], null_delete<const char[]>>(s);
}
inline std::unique_ptr<const wchar_t[], null_delete<const wchar_t[]>> wstr(const wchar_t* s)
{
    return std::unique_ptr<const wchar_t[], null_delete<const wchar_t[]>>(s);
}

#ifdef _UNICODE
#define tstr wstr
#else
#define tstr str
#endif

// Functions returning std::strings
std::string to_string(const wchar_t* s);
std::wstring to_wstring(const char* s);
inline std::string to_string(const char* s) { return std::string(s); }
inline std::wstring to_wstring(const wchar_t* s) { return std::wstring(s); }

#ifdef _UNICODE
#define to_tstring to_wstring
#else
#define to_tstring to_string
#endif

// BSD strlcpy/strlcat with overloads for converting between wide and narrow strings
size_t strlcpy(char* dst, const char* src, size_t size);
size_t strlcpy(char* dst, const wchar_t* src, size_t size);
size_t strlcat(char* dst, const char* src, size_t size);
size_t strlcat(char* dst, const wchar_t* src, size_t size);
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t size);
size_t wcslcpy(wchar_t* dst, const char* src, size_t size);
size_t wcslcat(wchar_t* dst, const wchar_t* src, size_t size);
size_t wcslcat(wchar_t* dst, const char* src, size_t size);

#ifdef _UNICODE
#define tcslcpy wcslcpy
#define tcslcat wcslcat
#else
#define tcslcpy strlcpy
#define tcslcat strlcat
#endif

#endif // _UTIL_WINDOWS_H
