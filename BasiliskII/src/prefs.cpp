/*
 *  prefs.cpp - Preferences handling
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "sysdeps.h"
#include "sys.h"
#include "prefs.h"


// Prefs items are stored in a linked list of these nodes
struct prefs_node {
	prefs_node *next;
	const char *name;
	prefs_type type;
	void *data;
};

// List of prefs nodes
static prefs_node *the_prefs = NULL;

// Prototypes
static const prefs_desc *find_prefs_desc(const char *name);


/*
 *  Initialize preferences
 */

void PrefsInit(int &argc, char **&argv)
{
	// Set defaults
	AddPrefsDefaults();
	AddPlatformPrefsDefaults();

	// Load preferences from settings file
	LoadPrefs();

	// Override prefs with command line options
	for (int i=1; i<argc; i++) {

		// Options are of the form '--keyword'
		const char *option = argv[i];
		if (strlen(option) < 3 || option[0] != '-' || option[1] != '-')
			continue;
		const char *keyword = option + 2;

		// Find descriptor for keyword
		const prefs_desc *d = find_prefs_desc(keyword);
		if (d == NULL)
			continue;
		argv[i] = NULL;

		// Get value
		i++;
		if (i >= argc) {
			fprintf(stderr, "Option '%s' must be followed by a value\n", option);
			continue;
		}
		const char *value = argv[i];
		argv[i] = NULL;

		// Add/replace prefs item
		switch (d->type) {
			case TYPE_STRING:
				if (d->multiple)
					PrefsAddString(keyword, value);
				else
					PrefsReplaceString(keyword, value);
				break;

			case TYPE_BOOLEAN: {
				if (!strcmp(value, "true") || !strcmp(value, "on") || !strcmp(value, "yes"))
					PrefsReplaceBool(keyword, true);
				else if (!strcmp(value, "false") || !strcmp(value, "off") || !strcmp(value, "no"))
					PrefsReplaceBool(keyword, false);
				else
					fprintf(stderr, "Value for option '%s' must be 'true' or 'false'\n", option);
				break;
			}

			case TYPE_INT32:
				PrefsReplaceInt32(keyword, atoi(value));
				break;

			default:
				break;
		}
	}

	// Remove processed arguments
	for (int i=1; i<argc; i++) {
		int k;
		for (k=i; k<argc; k++)
			if (argv[k] != NULL)
				break;
		if (k > i) {
			k -= i;
			for (int j=i+k; j<argc; j++)
				argv[j-k] = argv[j];
			argc -= k;
		}
	}
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
	the_prefs = NULL;
}


/*
 *  Print preferences options help
 */

static void print_options(const prefs_desc *list)
{
	while (list->type != TYPE_END) {
		if (list->help) {
			const char *typestr, *defstr;
			char numstr[32];
			switch (list->type) {
				case TYPE_STRING:
					typestr = "STRING";
					defstr = PrefsFindString(list->name);
					if (defstr == NULL)
						defstr = "none";
					break;
				case TYPE_BOOLEAN:
					typestr = "BOOL";
					if (PrefsFindBool(list->name))
						defstr = "true";
					else
						defstr = "false";
					break;
				case TYPE_INT32:
					typestr = "NUMBER";
					sprintf(numstr, "%d", PrefsFindInt32(list->name));
					defstr = numstr;
					break;
				default:
					typestr = "<unknown>";
					defstr = "none";
					break;
			}
			printf("  --%s %s\n    %s [default=%s]\n", list->name, typestr, list->help, defstr);
		}
		list++;
	}
}

void PrefsPrintUsage(void)
{
	printf("\nGeneral options:\n");
	print_options(common_prefs_items);
	printf("\nPlatform-specific options:\n");
	print_options(platform_prefs_items);
	printf("\nBoolean options are specified as '--OPTION true|on|yes' or\n'--OPTION false|off|no'.\n");
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

static const prefs_desc *find_prefs_desc(const char *name)
{
	const prefs_desc *d = find_prefs_desc(name, common_prefs_items);
	if (d == NULL)
		d = find_prefs_desc(name, platform_prefs_items);
	return d;
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
		char *keyword = line;
		char *value = p;
		int32 i = atol(value);

		// Look for keyword first in prefs item list
		const prefs_desc *desc = find_prefs_desc(keyword);
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
			case TYPE_INT32:
				fprintf(f, "%s %d\n", list->name, PrefsFindInt32(list->name));
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
