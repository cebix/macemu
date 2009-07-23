/*
 *  prefs_beos.cpp - Preferences handling, BeOS specific things
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

#include <StorageKit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sysdeps.h"
#include "prefs.h"
#include "main.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"bitbang", TYPE_BOOLEAN, false,  "draw Mac desktop directly on screen in window mode"},
	{"idlewait", TYPE_BOOLEAN, false, "sleep when idle"},
	{NULL, TYPE_END, false, NULL} // End of list
};


// Preferences file name and path
const char PREFS_FILE_NAME[] = "SheepShaver_prefs";
static BPath prefs_path;

// Modification date of prefs file
time_t PrefsFileDate = 0;


/*
 *  Load preferences from settings file
 */

void LoadPrefs(const char *vmdir)
{
	// Construct prefs path
	find_directory(B_USER_SETTINGS_DIRECTORY, &prefs_path, true);
	prefs_path.Append(PREFS_FILE_NAME);

	// Read preferences from settings file
	FILE *f = fopen(prefs_path.Path(), "r");
	if (f == NULL)	// Not found in settings directory, look in app directory
		f = fopen(PREFS_FILE_NAME, "r");
	if (f != NULL) {
		LoadPrefsFromStream(f);

		struct stat s;
		fstat(fileno(f), &s);
		PrefsFileDate = s.st_ctime;
		fclose(f);

	} else {

		// No prefs file, save defaults
		SavePrefs();
		PrefsFileDate = real_time_clock();
	}
}


/*
 *  Save preferences to settings file
 */

void SavePrefs(void)
{
	FILE *f;
	if ((f = fopen(prefs_path.Path(), "w")) != NULL) {
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
	PrefsReplaceString("extfs", "/boot");
	PrefsAddInt32("windowmodes",
		B_8_BIT_640x480 | B_15_BIT_640x480 | B_32_BIT_640x480 |
		B_8_BIT_800x600 | B_15_BIT_800x600 | B_32_BIT_800x600
	);
	PrefsAddInt32("screenmodes",
		B_8_BIT_640x480 | B_15_BIT_640x480 | B_32_BIT_640x480 |
		B_8_BIT_800x600 | B_15_BIT_800x600 | B_32_BIT_800x600 |
		B_8_BIT_1024x768 | B_15_BIT_1024x768
	);
	PrefsAddBool("bitbang", false);
	PrefsAddBool("idlewait", true);
}
