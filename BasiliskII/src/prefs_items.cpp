/*
 *  prefs_items.cpp - Common preferences items
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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
// Except for "disk", "floppy", "cdrom", "scsiX", "screen", "rom" and "ether",
// these are guaranteed to be in the prefs.
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
	{"bootdrive", TYPE_INT32, false},	// Boot drive number (main.cpp)
	{"bootdriver", TYPE_INT32, false},	// Boot driver number (main.cpp)
	{"ramsize", TYPE_INT32, false},		// Size of Mac RAM in bytes (main_*.cpp)
	{"frameskip", TYPE_INT32, false},	// Number of frames to skip in refreshed video modes (video_*.cpp)
	{"modelid", TYPE_INT32, false},		// Mac Model ID (Gestalt Model ID minus 6) (rom_patches.cpp)
	{"cpu", TYPE_INT32, false},			// CPU type (0 = 68000, 1 = 68010 etc.) (main.cpp)
	{"fpu", TYPE_BOOLEAN, false},		// Enable FPU emulation (main.cpp)
	{"nocdrom", TYPE_BOOLEAN, false},	// Don't install CD-ROM driver (cdrom.cpp/rom_patches.cpp)
	{"nosound", TYPE_BOOLEAN, false},	// Don't enable sound output (audio_*.cpp)
	{"noclipconversion", TYPE_BOOLEAN, false}, // Don't convert clipboard contents (clip_*.cpp)
	{"nogui", TYPE_BOOLEAN, false},		// Disable GUI (main_*.cpp)
	{NULL, TYPE_END, false}	// End of list
};


/*
 *  Set default values for preferences items
 */

void AddPrefsDefaults(void)
{
	SysAddSerialPrefs();
	PrefsAddInt32("bootdriver", 0);
	PrefsAddInt32("bootdrive", 0);
	PrefsAddInt32("ramsize", 8 * 1024 * 1024);
	PrefsAddInt32("frameskip", 6);
	PrefsAddInt32("modelid", 5);	// Mac IIci
	PrefsAddInt32("cpu", 3);		// 68030
	PrefsAddBool("fpu", false);
	PrefsAddBool("nocdrom", false);
	PrefsAddBool("nosound", false);
	PrefsAddBool("noclipconversion", false);
	PrefsAddBool("nogui", false);
}
