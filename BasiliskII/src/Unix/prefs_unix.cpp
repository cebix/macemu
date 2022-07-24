/*
 *  prefs_unix.cpp - Preferences handling, Unix specific stuff
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

#include <sys/stat.h>

#include <string>
using std::string;

#include "prefs.h"

// Platform-specific preferences items
prefs_desc platform_prefs_items[] = {
	{"fbdevicefile", TYPE_STRING, false,   "path of frame buffer device specification file"},
	{"dsp", TYPE_STRING, false,            "audio output (dsp) device name"},
	{"mixer", TYPE_STRING, false,          "audio mixer device name"},
	{"idlewait", TYPE_BOOLEAN, false,      "sleep when idle"},
#ifdef USE_SDL_VIDEO
	{"sdlrender", TYPE_STRING, false,      "SDL_Renderer driver (\"auto\", \"software\" (may be faster), etc.)"},
#endif
	{NULL, TYPE_END, false, NULL} // End of list
};

// Prefs file name and path
static const char PREFS_FILE_NAME[] = "/prefs";
string UserPrefsPath;
static string prefs_path;
static string prefs_name;
extern string xpram_name;

/*
 *  Load preferences from settings file
 */

// Comply with XDG Base Directory Specification
// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
static void get_prefs_path_from_env(void){
	char* env;
	if(env=getenv("XDG_CONFIG_HOME")){
		prefs_path = string(env);
		return;
	}
	if(env=getenv("HOME")){
		prefs_path = string(env) + "/.config";
	}
}

void LoadPrefs(const char* vmdir){
	if(vmdir){
		prefs_path = string(vmdir);
		prefs_name = prefs_path	+ PREFS_FILE_NAME;
		FILE *prefs = fopen(prefs_name.c_str(), "r");
		if (!prefs) {
			printf("No file at %s found.\n", prefs_name.c_str());
			exit(1);
		}
		LoadPrefsFromStream(prefs);
		fclose(prefs);
		return;
	}

	// Construct prefs path
	get_prefs_path_from_env();
	if(!prefs_path.empty()){
		prefs_path += "/BasiliskII";
		prefs_name = prefs_path + PREFS_FILE_NAME;
	}
	xpram_name = prefs_path + "/xpram";

	// --config was specified
	if(!UserPrefsPath.empty()){
		prefs_name = UserPrefsPath;
	}

	// Read preferences from settings file
	FILE *f = fopen(prefs_name.c_str(), "r");
	if (f != NULL) {

		// Prefs file found, load settings
		LoadPrefsFromStream(f);
		fclose(f);

	} else {
#ifdef __linux__
		PrefsAddString("cdrom", "/dev/cdrom");
#endif
		// No prefs file, save defaults
		SavePrefs();
	}
}

static bool is_dir(const std::string& path){
	struct stat info;
	if(stat(path.c_str(), &info) != 0){
		return false;
	}
	return (info.st_mode & S_IFDIR) != 0;
}

static bool create_directories(const std::string& path,mode_t mode){
	if(mkdir(path.c_str(),mode)==0)
		return true;

	switch (errno){
		case ENOENT:
			{
				int pos = path.find_last_of('/');
				if (pos == std::string::npos)
					return false;
				if (!create_directories(path.substr(0,pos),mode))
					return false;
			}
			return 0 == mkdir(path.c_str(),mode);

		case EEXIST:
			return is_dir(path);
		default:
			return false;
	}
}

/*
 *  Save preferences to settings file
 */

void SavePrefs(void){
	FILE *f;
	if(!prefs_path.empty()&&!is_dir(prefs_path)){
		create_directories(prefs_path,0700);
	}
	if ((f = fopen(prefs_name.c_str(), "w")) != NULL) {
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
	PrefsReplaceInt32("mousewheelmode", 1);
	PrefsReplaceInt32("mousewheellines", 3);
#ifdef __linux__
	if (access("/dev/sound/dsp", F_OK) == 0) {
		PrefsReplaceString("dsp", "/dev/sound/dsp");
	} else {
		PrefsReplaceString("dsp", "/dev/dsp");
	}
	if (access("/dev/sound/mixer", F_OK) == 0) {
		PrefsReplaceString("mixer", "/dev/sound/mixer");
	} else {
		PrefsReplaceString("mixer", "/dev/mixer");
	}
#else
	PrefsReplaceString("dsp", "/dev/dsp");
	PrefsReplaceString("mixer", "/dev/mixer");
#endif
	PrefsAddBool("idlewait", true);
}
