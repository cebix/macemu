/*
 *  user_strings_windows.cpp - Localizable strings, Windows specific strings
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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


// Platform-specific string definitions
user_string_def platform_strings[] = {
	// Common strings that have a platform-specific variant
	{STR_EXTFS_VOLUME_NAME, "My Computer"},

	// Purely platform-specific strings
	{STR_LOW_MEM_MMAP_ERR, "Cannot map Low Memory Globals: %s."},
	{STR_KD_SHMGET_ERR, "Cannot create SHM segment for Kernel Data: %s."},
	{STR_KD_SHMAT_ERR, "Cannot map first Kernel Data area: %s."},
	{STR_KD2_SHMAT_ERR, "Cannot map second Kernel Data area: %s."},
	{STR_ROM_MMAP_ERR, "Cannot map ROM: %s."},
	{STR_RAM_MMAP_ERR, "Cannot map RAM: %s."},
	{STR_DR_CACHE_MMAP_ERR, "Cannot map DR Cache: %s."},
	{STR_DR_EMULATOR_MMAP_ERR, "Cannot map DR Emulator: %s."},
	{STR_SHEEP_MEM_MMAP_ERR, "Cannot map SheepShaver Data area: %s."},
	{STR_NO_XVISUAL_ERR, "Cannot obtain appropriate X visual."},
	{STR_SLIRP_NO_DNS_FOUND_WARN, "Cannot get DNS address. Ethernet will not be available."},
	{STR_NO_AUDIO_WARN, "No audio device found, audio output will be disabled."},
	{STR_KEYCODE_FILE_WARN, "Cannot open keycode translation file %s (%s)."},
	{STR_KEYCODE_VENDOR_WARN, "Cannot find vendor '%s' in keycode translation file %s."},
	{STR_VOSF_INIT_ERR, "Cannot initialize Video on SEGV signals."},

	{STR_OPEN_WINDOW_ERR, "Cannot open Mac window."},
	{STR_WINDOW_TITLE_GRABBED, "SheepShaver (mouse grabbed, press Ctrl-F5 to release)"},
	{STR_NO_WIN32_NT_4, "SheepShaver does not run on Windows NT versions less than 4.0"},

	{STR_PREFS_MENU_FILE_GTK, "/_File"},
	{STR_PREFS_ITEM_START_GTK, "/File/_Start SheepShaver"},
	{STR_PREFS_ITEM_ZAP_PRAM_GTK, "/File/_Zap PRAM File"},
	{STR_PREFS_ITEM_SEPL_GTK, "/File/sepl"},
	{STR_PREFS_ITEM_QUIT_GTK, "/File/_Quit SheepShaver"},
	{STR_HELP_MENU_GTK, "/_Help"},
	{STR_HELP_ITEM_ABOUT_GTK, "/Help/_About SheepShaver"},

	{STR_FILE_CTRL, "File"},
	{STR_BROWSE_TITLE, "Browse file"},
	{STR_BROWSE_CTRL, "Browse..."},
	{STR_SERIAL_PANE_TITLE, "Serial"},
	{STR_NETWORK_PANE_TITLE, "Network"},
	{STR_INPUT_PANE_TITLE, "Keyboard/Mouse"},
	{STR_KEYCODES_CTRL, "Use Raw Keycodes"},
	{STR_KEYCODE_FILE_CTRL, "Keycode Translation File"},
	{STR_MOUSEWHEELMODE_CTRL, "Mouse Wheel Function"},
	{STR_MOUSEWHEELMODE_PAGE_LAB, "Page Up/Down"},
	{STR_MOUSEWHEELMODE_CURSOR_LAB, "Cursor Up/Down"},
	{STR_MOUSEWHEELLINES_CTRL, "Lines To Scroll"},
	{STR_POLLMEDIA_CTRL, "Try to automatically detect new removable media (enable polling)"},
	{STR_EXTFS_ENABLE_CTRL, "Enable \"My Computer\" icon on your Mac desktop (external file system)"},
	{STR_EXTFS_DRIVES_CTRL, "Mount drives"},
	{STR_ETHER_FTP_PORT_LIST_CTRL, "FTP ports"},
	{STR_ETHER_TCP_PORT_LIST_CTRL, "Server ports"},

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

	// Next, search for platform-specific string
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
