/*
 *  prefs_sdl.cpp - Preferences handling, SDL2 implementation
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
#include <SDL.h>

#include "prefs.h"


// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"idlewait", TYPE_BOOLEAN, false,      "sleep when idle"},
	{"sdlrender", TYPE_STRING, false,      "SDL_Renderer driver (\"auto\", \"software\" (may be faster), etc.)"},
	{NULL, TYPE_END, false}	// End of list
};


// Prefs file name and path
const char PREFS_FILE_NAME[] = ".basilisk_ii_prefs";

std::string UserPrefsPath;


/*
 *  Load preferences from settings file
 */

void LoadPrefs(const char * vmdir)	// TODO: load prefs from 'vmdir'
{
	// Build a full-path to the settings file
	char prefs_path[4096];
	if (!vmdir) {
		vmdir = SDL_getenv("HOME");
	}
	if (!vmdir) {
		vmdir = "./";
	}
	SDL_snprintf(prefs_path, sizeof(prefs_path), "%s/%s", vmdir, PREFS_FILE_NAME);
	
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
	// Build a full-path to the settings file
	char prefs_path[4096];
	const char * dir = SDL_getenv("HOME");
	if (!dir) {
		dir = "./";
	}
	SDL_snprintf(prefs_path, sizeof(prefs_path), "%s/%s", dir, PREFS_FILE_NAME);

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
}
