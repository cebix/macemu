/*
 *	$Id$
 *
 *  prefs_macosx.cpp - Preferences handling, Mac OS X specific.
 *					   Based on prefs_unix.cpp
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
using std::string;

#include "prefs.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	{"ignoresegv", TYPE_BOOLEAN, false,    "ignore illegal memory accesses"},
#endif
	{"idlewait", TYPE_BOOLEAN, false,      "sleep when idle"},
	{NULL, TYPE_END, false, NULL} // End of list
};


// Prefs file name and path
const char PREFS_FILE_NAME[] = ".basilisk_ii_prefs";
string UserPrefsPath;
static string prefs_path;


/*
 *  Load preferences from settings file
 */

void LoadPrefs(const char *vmdir)
{
	if (vmdir) {
		prefs_path = string(vmdir) + '/' + string("prefs");
		FILE *prefs = fopen(prefs_path.c_str(), "r");
		if (!prefs) {
			printf("No file at %s found.\n", prefs_path.c_str());
			exit(1);
		}
		LoadPrefsFromStream(prefs);
		fclose(prefs);
		return;
	}

	// Construct prefs path
	if (UserPrefsPath.empty()) {
		char *home = getenv("HOME");
		if (home)
			prefs_path = string(home) + '/';
		prefs_path += PREFS_FILE_NAME;
		UserPrefsPath = prefs_path;
	} else
		prefs_path = UserPrefsPath;

	// Read preferences from settings file
	FILE *f = fopen(prefs_path.c_str(), "r");
	if (f != NULL) {

		// Prefs file found, load settings
		LoadPrefsFromStream(f);
		fclose(f);

	} else {

		// No prefs file, save defaults
		SavePrefs();
	}

	// Remove Nigel's bad old serial prefs

	const char	*str;
	int			tmp = 0;

	if ( (str = PrefsFindString("seriala") ) != NULL
				&& strcmp(str, "/dev/ttys0") == 0 )
	{
		puts("Deleting invalid prefs item 'seriala /dev/ttys0'");
		PrefsRemoveItem("seriala", 1);
	}

	if ( (str = PrefsFindString("serialb") ) != NULL
				&& strcmp(str, "/dev/ttys1") == 0 )
	{
		puts("Deleting invalid prefs item 'serialb /dev/ttys1'");
		PrefsRemoveItem("serialb", 1);
	}

	// Floppy & cdrom prefs are always removed -
	// we search for them each time the emulator is started

	while ( (str = PrefsFindString("floppy", tmp) ) != NULL )
		PrefsRemoveItem("floppy", tmp);

	while ( (str = PrefsFindString("cdrom", tmp) ) != NULL )
		PrefsRemoveItem("cdrom", tmp);
}


/*
 *  Save preferences to settings file
 */

void SavePrefs(void)
{
	FILE *f;
	if ((f = fopen(prefs_path.c_str(), "w")) != NULL) {
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
	PrefsReplaceString("extfs",  getenv("HOME"));
	PrefsReplaceString("screen", "win/512/384/16");
#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	PrefsAddBool("ignoresegv", false);
#endif
	PrefsAddBool("idlewait", true);
}
