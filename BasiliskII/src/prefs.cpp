/*
 *  prefs.cpp - Preferences handling
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "sysdeps.h"
#include "sys.h"
#include "prefs.h"


// Common preferences items (those which exist on all platforms)
// Except for "disk", "floppy", "cdrom", "scsiX", "screen", "rom" and "ether",
// these are guaranteed to be in the prefs; "disk", "floppy" and "cdrom" can
// occur multiple times
prefs_desc common_prefs_items[] = {
	{"disk", TYPE_STRING, true},		// Device/file names of Mac volumes (disk.cpp)
	{"floppy", TYPE_STRING, true},		// Device/file names of Mac floppy drives (sony.cpp)
	{"cdrom", TYPE_STRING, true},		// Device/file names of Mac CD-ROM drives (cdrom.cpp)
	{"extfs", TYPE_STRING, false},		// Root path of ExtFS (extfs.cpp)
	{"scsi0", TYPE_STRING, false},		// SCSI targets for Mac SCSI ID 0..6 (scsi_*.cpp)
	{"scsi1", TYPE_STRING, false},
	{"scsi2", TYPE_STRING, false},
	{"scsi3", TYPE_STRING, false},
	{"scsi4", TYPE_STRING, false},
	{"scsi5", TYPE_STRING, false},
	{"scsi6", TYPE_STRING, false},
	{"screen", TYPE_STRING, false},		// Video mode (video.cpp)
	{"seriala", TYPE_STRING, false},	// Device name of Mac serial port A (serial_*.cpp)
	{"serialb", TYPE_STRING, false},	// Device name of Mac serial port B (serial_*.cpp)
	{"ether", TYPE_STRING, false},		// Device name of Mac ethernet adapter (ether_*.cpp)
	{"rom", TYPE_STRING, false},		// Path of ROM file (main_*.cpp)
	{"bootdrive", TYPE_INT16, false},	// Boot drive number (main.cpp)
	{"bootdriver", TYPE_INT16, false},	// Boot driver number (main.cpp)
	{"ramsize", TYPE_INT32, false},		// Size of Mac RAM in bytes (main_*.cpp)
	{"frameskip", TYPE_INT32, false},	// Number of frames to skip in refreshed video modes (video_*.cpp)
	{"modelid", TYPE_INT32, false},		// Mac Model ID (Gestalt Model ID minus 6) (rom_patches.cpp)
	{"cpu", TYPE_INT32, false},			// CPU type (0 = 68000, 1 = 68010 etc.) (main.cpp)
	{"fpu", TYPE_BOOLEAN, false},		// Enable FPU emulation (main.cpp)
	{"nocdrom", TYPE_BOOLEAN, false},	// Don't install CD-ROM driver (cdrom.cpp/rom_patches.cpp)
	{"nosound", TYPE_BOOLEAN, false},	// Don't enable sound output (audio_*.cpp)
	{"nogui", TYPE_BOOLEAN, false},		// Disable GUI (main_*.cpp)
	{NULL, TYPE_END, false}	// End of list
};


// Prefs item are stored in a linked list of these nodes
struct prefs_node {
	prefs_node *next;
	const char *name;
	prefs_type type;
	void *data;
};

// List of prefs nodes
static prefs_node *the_prefs;


/*
 *  Initialize preferences
 */

void PrefsInit(void)
{
	// Start with empty list
	the_prefs = NULL;

	// Set defaults
	SysAddSerialPrefs();
	PrefsAddInt16("bootdriver", 0);
	PrefsAddInt16("bootdrive", 0);
	PrefsAddInt32("ramsize", 8 * 1024 * 1024);
	PrefsAddInt32("frameskip", 6);
	PrefsAddInt32("modelid", 5);	// Mac IIci
	PrefsAddInt32("cpu", 3);		// 68030
	PrefsAddBool("fpu", false);
	PrefsAddBool("nocdrom", false);
	PrefsAddBool("nosound", false);
	PrefsAddBool("nogui", false);
	AddPlatformPrefsDefaults();

	// Load preferences from settings file
	LoadPrefs();
}


/*
 *  Deinitialize preferences
 */

void PrefsExit(void)
{
	// Free prefs list
	prefs_node *p = the_prefs, *next;
	while (p) {
		next = p->next;
		free((void *)p->name);
		free(p->data);
		delete p;
		p = next;
	}
}


/*
 *  Find preferences descriptor by keyword
 */

static const prefs_desc *find_prefs_desc(const char *name, const prefs_desc *list)
{
	while (list->type != TYPE_ANY) {
		if (strcmp(list->name, name) == 0)
			return list;
		list++;
	}
	return NULL;
}


/*
 *  Set prefs items
 */

static void add_data(const char *name, prefs_type type, void *data, int size)
{
	void *d = malloc(size);
	if (d == NULL)
		return;
	memcpy(d, data, size);
	prefs_node *p = new prefs_node;
	p->next = 0;
	p->name = strdup(name);
	p->type = type;
	p->data = d;
	if (the_prefs) {
		prefs_node *prev = the_prefs;
		while (prev->next)
			prev = prev->next;
		prev->next = p;
	} else
		the_prefs = p;
}

void PrefsAddString(const char *name, const char *s)
{
	add_data(name, TYPE_STRING, (void *)s, strlen(s) + 1);
}

void PrefsAddBool(const char *name, bool b)
{
	add_data(name, TYPE_BOOLEAN, &b, sizeof(bool));
}

void PrefsAddInt16(const char *name, int16 val)
{
	add_data(name, TYPE_INT16, &val, sizeof(int16));
}

void PrefsAddInt32(const char *name, int32 val)
{
	add_data(name, TYPE_INT32, &val, sizeof(int32));
}


/*
 *  Replace prefs items
 */

static prefs_node *find_node(const char *name, prefs_type type, int index = 0)
{
	prefs_node *p = the_prefs;
	int i = 0;
	while (p) {
		if ((type == TYPE_ANY || p->type == type) && !strcmp(p->name, name)) {
			if (i == index)
				return p;
			else
				i++;
		}
		p = p->next;
	}
	return NULL;
}

void PrefsReplaceString(const char *name, const char *s, int index)
{
	prefs_node *p = find_node(name, TYPE_STRING, index);
	if (p) {
		free(p->data);
		p->data = strdup(s);
	} else
		add_data(name, TYPE_STRING, (void *)s, strlen(s) + 1);
}

void PrefsReplaceBool(const char *name, bool b)
{
	prefs_node *p = find_node(name, TYPE_BOOLEAN);
	if (p)
		*(bool *)(p->data) = b;
	else
		add_data(name, TYPE_BOOLEAN, &b, sizeof(bool));
}

void PrefsReplaceInt16(const char *name, int16 val)
{
	prefs_node *p = find_node(name, TYPE_INT16);
	if (p)
		*(int16 *)(p->data) = val;
	else
		add_data(name, TYPE_INT16, &val, sizeof(int16));
}

void PrefsReplaceInt32(const char *name, int32 val)
{
	prefs_node *p = find_node(name, TYPE_INT32);
	if (p)
		*(int32 *)(p->data) = val;
	else
		add_data(name, TYPE_INT32, &val, sizeof(int32));
}


/*
 *  Get prefs items
 */

const char *PrefsFindString(const char *name, int index)
{
	prefs_node *p = find_node(name, TYPE_STRING, index);
	if (p)
		return (char *)(p->data);
	else
		return NULL;
}

bool PrefsFindBool(const char *name)
{
	prefs_node *p = find_node(name, TYPE_BOOLEAN, 0);
	if (p)
		return *(bool *)(p->data);
	else
		return false;
}

int16 PrefsFindInt16(const char *name)
{
	prefs_node *p = find_node(name, TYPE_INT16, 0);
	if (p)
		return *(int16 *)(p->data);
	else
		return 0;
}

int32 PrefsFindInt32(const char *name)
{
	prefs_node *p = find_node(name, TYPE_INT32, 0);
	if (p)
		return *(int32 *)(p->data);
	else
		return 0;
}


/*
 *  Remove prefs items
 */

void PrefsRemoveItem(const char *name, int index)
{
	prefs_node *p = find_node(name, TYPE_ANY, index);
	if (p) {
		free((void *)p->name);
		free(p->data);
		prefs_node *q = the_prefs;
		if (q == p) {
			the_prefs = NULL;
			delete p;
			return;
		}
		while (q) {
			if (q->next == p) {
				q->next = p->next;
				delete p;
				return;
			}
			q = q->next;
		}
	}
}


/*
 *  Load prefs from stream (utility function for LoadPrefs() implementation)
 */

void LoadPrefsFromStream(FILE *f)
{
	char line[256];
	while(fgets(line, 255, f)) {
		// Read line
		int len = strlen(line);
		if (len == 0)
			continue;
		line[len-1] = 0;

		// Comments begin with "#" or ";"
		if (line[0] == '#' || line[0] == ';')
			continue;

		// Terminate string after keyword
		char *p = line;
		while (!isspace(*p)) p++;
		*p++ = 0;

		// Skip whitespace until value
		while (isspace(*p)) p++;
		if (*p == 0)
			continue;
		char *keyword = line;
		char *value = p;
		int32 i = atol(value);

		// Look for keyword first in common item list, then in platform specific list
		const prefs_desc *desc = find_prefs_desc(keyword, common_prefs_items);
		if (desc == NULL)
			desc = find_prefs_desc(keyword, platform_prefs_items);
		if (desc == NULL) {
			printf("WARNING: Unknown preferences keyword '%s'\n", keyword);
			continue;
		}

		// Add item to prefs
		switch (desc->type) {
			case TYPE_STRING:
				if (desc->multiple)
					PrefsAddString(keyword, value);
				else
					PrefsReplaceString(keyword, value);
				break;
			case TYPE_BOOLEAN:
				PrefsReplaceBool(keyword, !strcmp(value, "true"));
				break;
			case TYPE_INT16:
				PrefsReplaceInt16(keyword, i);
				break;
			case TYPE_INT32:
				PrefsReplaceInt32(keyword, i);
				break;
			default:
				break;
		}
	}
}


/*
 *  Save settings to stream (utility function for SavePrefs() implementation)
 */

static void write_prefs(FILE *f, const prefs_desc *list)
{
	while (list->type != TYPE_ANY) {
		switch (list->type) {
			case TYPE_STRING: {
				int index = 0;
				const char *str;
				while ((str = PrefsFindString(list->name, index++)) != NULL)
					fprintf(f, "%s %s\n", list->name, str);
				break;
			}
			case TYPE_BOOLEAN:
				fprintf(f, "%s %s\n", list->name, PrefsFindBool(list->name) ? "true" : "false");
				break;
			case TYPE_INT16:
				fprintf(f, "%s %d\n", list->name, PrefsFindInt16(list->name));
				break;
			case TYPE_INT32:
				fprintf(f, "%s %ld\n", list->name, PrefsFindInt32(list->name));
				break;
			default:
				break;
		}
		list++;
	}
}

void SavePrefsToStream(FILE *f)
{
	write_prefs(f, common_prefs_items);
	write_prefs(f, platform_prefs_items);
}
