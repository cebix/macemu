/*
 *  prefs_unix.cpp - Preferences handling, Unix specifix stuff
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

#include "prefs.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"keycodes", TYPE_BOOLEAN, false},		// Use keycodes rather than keysyms to decode keyboard (video_x.cpp)
	{"keycodefile", TYPE_STRING, false},	// File name of keycode translation table (video_x.cpp)
	{"fbdevicefile", TYPE_STRING, false},	// File name of frame buffer device specifications (video_x.cpp)
	{"mousewheelmode", TYPE_INT16, false},	// Mouse wheel support mode (0=Page up/down, 1=Cursor up/down) (video_x.cpp)
	{"mousewheellines", TYPE_INT16, false},	// Number of lines to scroll in mouse whell mode 1 (video_x.cpp)
	{NULL, TYPE_END, false}	// End of list
};


// Prefs file name and path
const char PREFS_FILE_NAME[] = ".basilisk_ii_prefs";
static char prefs_path[1024];


/*
 *  Load preferences from settings file
 */

void LoadPrefs(void)
{
	// Construct prefs path
	prefs_path[0] = 0;
	char *home = getenv("HOME");
	if (home != NULL && strlen(home) < 1000) {
		strncpy(prefs_path, home, 1000);
		strcat(prefs_path, "/");
	}
	strcat(prefs_path, PREFS_FILE_NAME);

	// Read preferences from settings file
	FILE *f = fopen(prefs_path, "r");
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
	if ((f = fopen(prefs_path, "w")) != NULL) {
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
	PrefsReplaceString("extfs", "/");
	PrefsReplaceInt16("mousewheelmode", 1);
	PrefsReplaceInt16("mousewheellines", 3);
}
