/*
 *  prefs_unix.cpp - Preferences handling, Unix specific things
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "prefs.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"ether", TYPE_STRING, false, "device name of Mac ethernet adapter"},
	{NULL, TYPE_END, false, NULL} // End of list
};


// Prefs file name and path
const char PREFS_FILE_NAME[] = ".sheepshaver_prefs";
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
	PrefsReplaceString("extfs", "/");
	PrefsAddInt32("windowmodes", 3);
	PrefsAddInt32("screenmodes", 0x3f);
}
