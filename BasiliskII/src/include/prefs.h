/*
 *  prefs.h - Preferences handling
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifndef PREFS_H
#define PREFS_H

#include <stdio.h>

extern void PrefsInit(int &argc, char **&argv);
extern void PrefsExit(void);

extern void PrefsPrintUsage(void);

extern void AddPrefsDefaults(void);
extern void AddPlatformPrefsDefaults(void);

// Preferences loading/saving
extern void LoadPrefs(void);
extern void SavePrefs(void);

extern void LoadPrefsFromStream(FILE *f);
extern void SavePrefsToStream(FILE *f);

// Public preferences access functions
extern void PrefsAddString(const char *name, const char *s);
extern void PrefsAddBool(const char *name, bool b);
extern void PrefsAddInt32(const char *name, int32 val);

extern void PrefsReplaceString(const char *name, const char *s, int index = 0);
extern void PrefsReplaceBool(const char *name, bool b);
extern void PrefsReplaceInt32(const char *name, int32 val);

extern const char *PrefsFindString(const char *name, int index = 0);
extern bool PrefsFindBool(const char *name);
extern int32 PrefsFindInt32(const char *name);

extern void PrefsRemoveItem(const char *name, int index = 0);


/*
 *  Definition of preferences items
 */

// Item types
enum prefs_type {
	TYPE_STRING,		// char[]
	TYPE_BOOLEAN,		// bool
	TYPE_INT32,			// int32
	TYPE_ANY,			// Wildcard for find_node
	TYPE_END = TYPE_ANY	// Terminator for prefs_desc list
};

// Item descriptor
struct prefs_desc {
	const char *name;	// Name of keyword
	prefs_type type;	// Type (see above)
	bool multiple;		// Can this item occur multiple times (only for TYPE_STRING)?
	const char *help;	// Help/descriptive text about this item
};

// List of common preferences items (those which exist on all platforms)
extern prefs_desc common_prefs_items[];

// List of platform-specific preferences items
extern prefs_desc platform_prefs_items[];

#endif
