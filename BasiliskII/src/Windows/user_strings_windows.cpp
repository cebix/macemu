/*
 *  user_strings_windows.cpp - Windows-specific localizable strings
 *
 *  Basilisk II (C) 1997-2004 Christian Bauer
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
#include "user_strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


// Platform-specific string definitions
user_string_def platform_strings[] = {
	// Common strings that have a platform-specific variant
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted under Unix. Basilisk II will try to unmount it."},
	{STR_EXTFS_VOLUME_NAME, "My Computer"},

	// Purely platform-specific strings
	{STR_NO_XVISUAL_ERR, "Cannot obtain appropriate X visual."},
	{STR_VOSF_INIT_ERR, "Cannot initialize Video on SEGV signals."},
	{STR_SIG_INSTALL_ERR, "Cannot install %s handler (%s)."},
	{STR_TICK_THREAD_ERR, "Cannot create 60Hz thread (%s)."},
	{STR_NO_AUDIO_WARN, "No audio device found, audio output will be disabled."},
	{STR_KEYCODE_FILE_WARN, "Cannot open keycode translation file %s (%s)."},
	{STR_KEYCODE_VENDOR_WARN, "Cannot find vendor '%s' in keycode translation file %s."},
	{STR_WINDOW_TITLE_GRABBED, "Basilisk II (mouse grabbed, press Ctrl-F5 to release)"},

	{-1, NULL}	// End marker
};


/*
 *  Search for main volume name
 */

static const char *get_volume_name(void)
{
	HKEY hHelpKey;
	DWORD key_type, cbData;

	static char volume[256];
	memset(volume, 0, sizeof(volume));

	// Try Windows 2000 key first
	if (ERROR_SUCCESS == RegOpenKey(
			HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
			&hHelpKey))
	{
		cbData = sizeof(volume);
		RegQueryValueEx( hHelpKey, 0, NULL, &key_type, (unsigned char *)volume, &cbData );
		RegCloseKey(hHelpKey);
	}

	if (volume[0] == 0 &&
		ERROR_SUCCESS == RegOpenKey(
			HKEY_CURRENT_USER,
			"Software\\Classes\\CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
			&hHelpKey))
	{
		cbData = sizeof(volume);
		RegQueryValueEx( hHelpKey, 0, NULL, &key_type, (unsigned char *)volume, &cbData );
		RegCloseKey(hHelpKey);
	}

	if (volume[0] == 0 &&
		ERROR_SUCCESS == RegOpenKey(
			HKEY_CLASSES_ROOT,
			"CLSID\\{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
			&hHelpKey))
	{
		cbData = sizeof(volume);
		RegQueryValueEx( hHelpKey, 0, NULL, &key_type, (unsigned char *)volume, &cbData );
		RegCloseKey(hHelpKey);
	}

	// Fix the error that some "tweak" apps do.
	if (stricmp(volume, "%USERNAME% on %COMPUTER%") == 0)
		volume[0] = '\0';

	// No volume name found, default to "My Computer"
	if (volume[0] == 0)
		strcpy(volume, "My Computer");

	return volume;
}


/*
 *  Fetch pointer to string, given the string number
 */

const char *GetString(int num)
{
	// First, search for platform-specific variable string
	switch (num) {
	case STR_EXTFS_VOLUME_NAME:
		return get_volume_name();
	}

	// Next, search for platform-specific constant string
	int i = 0;
	while (platform_strings[i].num >= 0) {
		if (platform_strings[i].num == num)
			return platform_strings[i].str;
		i++;
	}

	// Not found, search for common string
	i = 0;
	while (common_strings[i].num >= 0) {
		if (common_strings[i].num == num)
			return common_strings[i].str;
		i++;
	}
	return NULL;
}
