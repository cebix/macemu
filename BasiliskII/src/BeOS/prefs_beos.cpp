/*
 *  prefs_beos.cpp - Preferences handling, BeOS specific stuff
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

#include <StorageKit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysdeps.h"
#include "prefs.h"
#include "main.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"powerrom", TYPE_STRING, false, "path of PowerMac ROM"},
	{NULL, TYPE_END, false, NULL} // End of list
};


// Preferences file name and path
const char PREFS_FILE_NAME[] = "BasiliskII_prefs";
static BPath prefs_path;


/*
 *  Load preferences from settings file
 */

void LoadPrefs(const char* vmdir)
{
#if 0
	if (vmdir) {
		prefs_path.SetTo(vmdir);
		prefs_path.Append("prefs");
		FILE *prefs = fopen(prefs_path.Path(), "r");
		if (!prefs) {
			printf("No file at %s found.\n", prefs_path.Path());
			exit(1);
		}
		LoadPrefsFromStream(prefs);
		fclose(prefs);
		return;
	}
#endif

	// Construct prefs path
	find_directory(B_USER_SETTINGS_DIRECTORY, &prefs_path, true);
	prefs_path.Append(PREFS_FILE_NAME);

	// Read preferences from settings file
	FILE *f = fopen(prefs_path.Path(), "r");
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
}
