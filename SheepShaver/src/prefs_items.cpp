/*
 *  prefs_items.cpp - Common preferences items
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

#include "sys.h"
#include "prefs.h"


// Common preferences items (those which exist on all platforms)
prefs_desc common_prefs_items[] = {
	{"disk", TYPE_STRING, true,         "device/file name of Mac volume"},
	{"floppy", TYPE_STRING, true,       "device/file name of Mac floppy drive"},
	{"cdrom", TYPE_STRING, true,        "device/file names of Mac CD-ROM drive"},
	{"extfs", TYPE_STRING, false,       "root path of ExtFS"},
	{"scsi0", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 0"},
	{"scsi1", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 1"},
	{"scsi2", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 2"},
	{"scsi3", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 3"},
	{"scsi4", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 4"},
	{"scsi5", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 5"},
	{"scsi6", TYPE_STRING, false,       "SCSI target for Mac SCSI ID 6"},
	{"screen", TYPE_STRING, false,      "video mode"},
	{"windowmodes", TYPE_INT32, false,  "bitmap of allowed window video modes"},
	{"screenmodes", TYPE_INT32, false,  "bitmap of allowed fullscreen video modes"},
	{"seriala", TYPE_STRING, false,     "device name of Mac serial port A"},
	{"serialb", TYPE_STRING, false,     "device name of Mac serial port B"},
	{"rom", TYPE_STRING, false,         "path of ROM file"},
	{"bootdrive", TYPE_INT32, false,    "boot drive number"},
	{"bootdriver", TYPE_INT32, false,   "boot driver number"},
	{"ramsize", TYPE_INT32, false,      "size of Mac RAM in bytes"},
	{"frameskip", TYPE_INT32, false,    "number of frames to skip in refreshed video modes"},
	{"gfxaccel", TYPE_BOOLEAN, false,   "turn on QuickDraw acceleration"},
	{"nocdrom", TYPE_BOOLEAN, false,    "don't install CD-ROM driver"},
	{"nonet", TYPE_BOOLEAN, false,      "don't use Ethernet"},
	{"nosound", TYPE_BOOLEAN, false,    "don't enable sound output"},
	{"nogui", TYPE_BOOLEAN, false,      "disable GUI"},
	{"noclipconversion", TYPE_BOOLEAN, false, "don't convert clipboard contents"},
	{"ignoresegv", TYPE_BOOLEAN, false, "ignore illegal memory accesses"},
	{"jit", TYPE_BOOLEAN, false,        "enable JIT compiler"},
	{"jit68k", TYPE_BOOLEAN, false,     "enable 68k DR emulator"},
	{"keyboardtype", TYPE_INT32, false, "hardware keyboard type"},
	{NULL, TYPE_END, false, NULL} // End of list
};

/*
 *  Set default values for preferences items
 */

void AddPrefsDefaults(void)
{
#ifndef PREFS_EDITOR
	SysAddSerialPrefs();
#endif
	PrefsAddInt32("bootdriver", 0);
	PrefsAddInt32("bootdrive", 0);
	PrefsAddInt32("ramsize", 16 * 1024 * 1024);
	PrefsAddInt32("frameskip", 8);
	PrefsAddBool("gfxaccel", true);
	PrefsAddBool("nocdrom", false);
	PrefsAddBool("nonet", false);
	PrefsAddBool("nosound", false);
	PrefsAddBool("nogui", false);
	PrefsAddBool("noclipconversion", false);
	PrefsAddBool("ignoresegv", false);

#if USE_JIT
	// JIT compiler specific options
	PrefsAddBool("jit", true);
#else
	PrefsAddBool("jit", false);
#endif
	PrefsAddBool("jit68k", false);

    PrefsAddInt32("keyboardtype", 5);
}
