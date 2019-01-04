/*
 *  prefs_windows.cpp - Preferences handling, Windows specific stuff
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

#include "sysdeps.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>
typedef std::basic_string<TCHAR> tstring;

#include "prefs.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"keycodes", TYPE_BOOLEAN, false,      "use keycodes rather than keysyms to decode keyboard"},
	{"keycodefile", TYPE_STRING, false,    "path of keycode translation file"},
	{"mousewheelmode", TYPE_INT32, false,  "mouse wheel support mode (0=page up/down, 1=cursor up/down)"},
	{"mousewheellines", TYPE_INT32, false, "number of lines to scroll in mouse wheel mode 1"},
#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	{"ignoresegv", TYPE_BOOLEAN, false,    "ignore illegal memory accesses"},
#endif
	{"idlewait", TYPE_BOOLEAN, false,      "sleep when idle"},
	{"enableextfs", TYPE_BOOLEAN, false,   "enable extfs system"},
	{"debugextfs", TYPE_BOOLEAN, false,    "debug extfs system"},
	{"extdrives", TYPE_STRING, false,      "define allowed extfs drives"},
	{"pollmedia", TYPE_BOOLEAN, false,     "poll for new media (e.g. cd, floppy)"},
	{"etherguid", TYPE_STRING, false,      "GUID of the ethernet device to use"},
	{"etherpermanentaddress", TYPE_BOOLEAN, false,  "use permanent NIC address to identify itself"},
	{"ethermulticastmode", TYPE_INT32, false,       "how to multicast packets"},
	{"etherfakeaddress", TYPE_STRING, false,        "optional fake hardware address"},
	{"routerenabled", TYPE_BOOLEAN, false,          "enable NAT/Router module"},
	{"ftp_port_list", TYPE_STRING, false,           "FTP ports list"},
	{"tcp_port", TYPE_STRING, false,                "TCP ports list"},
	{"portfile0", TYPE_STRING, false,               "output file for serial port 0"},
	{"portfile1", TYPE_STRING, false,               "output file for serial port 1"},

	{NULL, TYPE_END, false, NULL} // End of list
};


// Prefs file name and path
const TCHAR PREFS_FILE_NAME[] = TEXT("BasiliskII_prefs");
tstring UserPrefsPath;
static tstring prefs_path;


/*
 *  Load preferences from settings file
 */

void LoadPrefs(const char *vmdir)
{
	// Construct prefs path
	if (UserPrefsPath.empty()) {
		int pwd_len = GetCurrentDirectory(0, NULL);
		prefs_path.resize(pwd_len);
		pwd_len = GetCurrentDirectory(pwd_len, &prefs_path.front());
		prefs_path[pwd_len] = TEXT('\\');
		prefs_path += PREFS_FILE_NAME;
	} else
		prefs_path = UserPrefsPath;

	// Read preferences from settings file
	FILE *f = _tfopen(prefs_path.c_str(), TEXT("r"));
	if (f != NULL) {

		// Prefs file found, load settings
		LoadPrefsFromStream(f);
		fclose(f);

	} else {

		// No prefs file, save defaults
		SavePrefs();
	}
}


/*
 *  Save preferences to settings file
 */

void SavePrefs(void)
{
	FILE *f;
	if ((f = _tfopen(prefs_path.c_str(), TEXT("w"))) != NULL) {
		SavePrefsToStream(f);
		fclose(f);
	}
}


/*
 *  Add defaults of platform-specific prefs items
 *  You may also override the defaults set in PrefsInit()
 */

void AddPlatformPrefsDefaults(void)
{
	PrefsAddBool("keycodes", false);
	PrefsReplaceBool("pollmedia", true);
	PrefsReplaceBool("enableextfs", false);
	PrefsReplaceString("extfs", "");
	PrefsReplaceString("extdrives", "CDEFGHIJKLMNOPQRSTUVWXYZ");
	PrefsReplaceInt32("mousewheelmode", 1);
	PrefsReplaceInt32("mousewheellines", 3);
#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	PrefsAddBool("ignoresegv", false);
#endif
	PrefsAddBool("idlewait", true);
	PrefsReplaceBool("etherpermanentaddress", true);
	PrefsReplaceInt32("ethermulticastmode", 0);
	PrefsReplaceString("ftp_port_list", "21");
	PrefsReplaceString("seriala", "COM1");
	PrefsReplaceString("serialb", "COM2");
	PrefsReplaceString("portfile0", "C:\\B2TEMP0.OUT");
	PrefsReplaceString("portfile1", "C:\\B2TEMP1.OUT");
}
