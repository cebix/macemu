/*
 *  prefs_items.cpp - Common preferences items
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

#include "sys.h"
#include "prefs.h"


// Common preferences items (those which exist on all platforms)
// Except for "disk", "floppy", "cdrom", "scsiX", "screen", "rom" and "ether",
// these are guaranteed to be in the prefs.
prefs_desc common_prefs_items[] = {
	{"displaycolordepth", TYPE_INT32, false, "display color depth"},
	{"disk", TYPE_STRING, true,       "device/file name of Mac volume"},
	{"floppy", TYPE_STRING, true,     "device/file name of Mac floppy drive"},
	{"cdrom", TYPE_STRING, true,      "device/file names of Mac CD-ROM drive"},
	{"extfs", TYPE_STRING, false,     "root path of ExtFS"},
	{"scsi0", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 0"},
	{"scsi1", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 1"},
	{"scsi2", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 2"},
	{"scsi3", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 3"},
	{"scsi4", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 4"},
	{"scsi5", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 5"},
	{"scsi6", TYPE_STRING, false,     "SCSI target for Mac SCSI ID 6"},
	{"screen", TYPE_STRING, false,    "video mode"},
	{"seriala", TYPE_STRING, false,   "device name of Mac serial port A"},
	{"serialb", TYPE_STRING, false,   "device name of Mac serial port B"},
	{"ether", TYPE_STRING, false,     "device name of Mac ethernet adapter"},
	{"etherconfig", TYPE_STRING, false,"path of network config script"},
	{"udptunnel", TYPE_BOOLEAN, false, "tunnel all network packets over UDP"},
	{"udpport", TYPE_INT32, false,    "IP port number for tunneling"},
	{"redir", TYPE_STRING, true,      "port forwarding for slirp"},
	{"rom", TYPE_STRING, false,       "path of ROM file"},
	{"bootdrive", TYPE_INT32, false,  "boot drive number"},
	{"bootdriver", TYPE_INT32, false, "boot driver number"},
	{"ramsize", TYPE_INT32, false,    "size of Mac RAM in bytes"},
	{"frameskip", TYPE_INT32, false,  "number of frames to skip in refreshed video modes"},
	{"modelid", TYPE_INT32, false,    "Mac Model ID (Gestalt Model ID minus 6)"},
	{"cpu", TYPE_INT32, false,        "CPU type (0 = 68000, 1 = 68010 etc.)"},
	{"fpu", TYPE_BOOLEAN, false,      "enable FPU emulation"},
	{"nocdrom", TYPE_BOOLEAN, false,  "don't install CD-ROM driver"},
	{"nosound", TYPE_BOOLEAN, false,  "don't enable sound output"},
	{"noclipconversion", TYPE_BOOLEAN, false, "don't convert clipboard contents"},
	{"nogui", TYPE_BOOLEAN, false,    "disable GUI"},
	{"jit", TYPE_BOOLEAN, false,         "enable JIT compiler"},
	{"jitfpu", TYPE_BOOLEAN, false,      "enable JIT compilation of FPU instructions"},
	{"jitdebug", TYPE_BOOLEAN, false,    "enable JIT debugger (requires mon builtin)"},
	{"jitcachesize", TYPE_INT32, false,  "translation cache size in KB"},
	{"jitlazyflush", TYPE_BOOLEAN, false, "enable lazy invalidation of translation cache"},
	{"jitinline", TYPE_BOOLEAN, false,   "enable translation through constant jumps"},
	{"jitblacklist", TYPE_STRING, false, "blacklist opcodes from translation"},
	{"keyboardtype", TYPE_INT32, false, "hardware keyboard type"},
	{"keycodes",TYPE_BOOLEAN,false,"use raw keycode"},
	{"keycodefile",TYPE_STRING,"Keycode file"},
	{"mousewheelmode",TYPE_BOOLEAN,"Use WheelMode"},
	{"mousewheellines",TYPE_INT32,"wheel line nb"},
	{NULL, TYPE_END, false, NULL} // End of list
};


/*
 *  Set default values for preferences items
 */

void AddPrefsDefaults(void)
{
	SysAddSerialPrefs();
	PrefsAddBool("udptunnel", false);
	PrefsAddInt32("udpport", 6066);
	PrefsAddInt32("bootdriver", 0);
	PrefsAddInt32("bootdrive", 0);
	PrefsAddInt32("ramsize", 8 * 1024 * 1024);
	PrefsAddInt32("frameskip", 6);
	PrefsAddInt32("modelid", 5);	// Mac IIci
	PrefsAddInt32("cpu", 3);		// 68030
	PrefsAddInt32("displaycolordepth", 0);
	PrefsAddBool("fpu", false);
	PrefsAddBool("nocdrom", false);
	PrefsAddBool("nosound", false);
	PrefsAddBool("noclipconversion", false);
	PrefsAddBool("nogui", false);
	
#if USE_JIT
	// JIT compiler specific options
	PrefsAddBool("jit", true);
	PrefsAddBool("jitfpu", true);
	PrefsAddBool("jitdebug", false);
	PrefsAddInt32("jitcachesize", 8192);
	PrefsAddBool("jitlazyflush", true);
	PrefsAddBool("jitinline", true);
#else
	PrefsAddBool("jit", false);
#endif

    PrefsAddInt32("keyboardtype", 5);
}
