/*
 *  main.cpp - Startup/shutdown code
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

#include "cpu_emulation.h"
#include "xpram.h"
#include "timer.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "extfs.h"
#include "audio.h"
#include "video.h"
#include "serial.h"
#include "ether.h"
#include "clip.h"
#include "adb.h"
#include "rom_patches.h"
#include "user_strings.h"
#include "prefs.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"

#if ENABLE_MON
#include "mon.h"

static uint32 mon_read_byte_b2(uintptr adr)
{
	return ReadMacInt8(adr);
}

static void mon_write_byte_b2(uintptr adr, uint32 b)
{
	WriteMacInt8(adr, b);
}
#endif


/*
 *  Initialize everything, returns false on error
 */

bool InitAll(const char *vmdir)
{
	// Check ROM version
	if (!CheckROM()) {
		ErrorAlert(STR_UNSUPPORTED_ROM_TYPE_ERR);
		return false;
	}

#if EMULATED_68K
	// Set CPU and FPU type (UAE emulation)
	switch (ROMVersion) {
		case ROM_VERSION_64K:
		case ROM_VERSION_PLUS:
		case ROM_VERSION_CLASSIC:
			CPUType = 0;
			FPUType = 0;
			TwentyFourBitAddressing = true;
			break;
		case ROM_VERSION_II:
			CPUType = PrefsFindInt32("cpu");
			if (CPUType < 2) CPUType = 2;
			if (CPUType > 4) CPUType = 4;
			FPUType = PrefsFindBool("fpu") ? 1 : 0;
			if (CPUType == 4) FPUType = 1;	// 68040 always with FPU
			TwentyFourBitAddressing = true;
			break;
		case ROM_VERSION_32:
			CPUType = PrefsFindInt32("cpu");
			if (CPUType < 2) CPUType = 2;
			if (CPUType > 4) CPUType = 4;
			FPUType = PrefsFindBool("fpu") ? 1 : 0;
			if (CPUType == 4) FPUType = 1;	// 68040 always with FPU
			TwentyFourBitAddressing = false;
			break;
	}
	CPUIs68060 = false;
#endif

	// Load XPRAM
	XPRAMInit(vmdir);

	// Load XPRAM default values if signature not found
	if (XPRAM[0x0c] != 0x4e || XPRAM[0x0d] != 0x75
	 || XPRAM[0x0e] != 0x4d || XPRAM[0x0f] != 0x63) {
		D(bug("Loading XPRAM default values\n"));
		memset(XPRAM, 0, 0x100);
		XPRAM[0x0c] = 0x4e;	// "NuMc" signature
		XPRAM[0x0d] = 0x75;
		XPRAM[0x0e] = 0x4d;
		XPRAM[0x0f] = 0x63;
		XPRAM[0x01] = 0x80;	// InternalWaitFlags = DynWait (don't wait for SCSI devices upon bootup)
		XPRAM[0x10] = 0xa8;	// Standard PRAM values
		XPRAM[0x11] = 0x00;
		XPRAM[0x12] = 0x00;
		XPRAM[0x13] = 0x22;
		XPRAM[0x14] = 0xcc;
		XPRAM[0x15] = 0x0a;
		XPRAM[0x16] = 0xcc;
		XPRAM[0x17] = 0x0a;
		XPRAM[0x1c] = 0x00;
		XPRAM[0x1d] = 0x02;
		XPRAM[0x1e] = 0x63;
		XPRAM[0x1f] = 0x00;
		XPRAM[0x08] = 0x13;
		XPRAM[0x09] = 0x88;
		XPRAM[0x0a] = 0x00;
		XPRAM[0x0b] = 0xcc;
		XPRAM[0x76] = 0x00;	// OSDefault = MacOS
		XPRAM[0x77] = 0x01;
	}

	// Set boot volume
	int16 i16 = PrefsFindInt32("bootdrive");
	XPRAM[0x78] = i16 >> 8;
	XPRAM[0x79] = i16 & 0xff;
	i16 = PrefsFindInt32("bootdriver");
	XPRAM[0x7a] = i16 >> 8;
	XPRAM[0x7b] = i16 & 0xff;

	// Init drivers
	SonyInit();
	DiskInit();
	CDROMInit();
	SCSIInit();

#if SUPPORTS_EXTFS
	// Init external file system
	ExtFSInit();
#endif

	// Init serial ports
	SerialInit();

	// Init network
	EtherInit();

	// Init Time Manager
	TimerInit();

	// Init clipboard
	ClipInit();

	// Init ADB
	ADBInit();

	// Init audio
	AudioInit();

	// Init video
	if (!VideoInit(ROMVersion == ROM_VERSION_64K || ROMVersion == ROM_VERSION_PLUS || ROMVersion == ROM_VERSION_CLASSIC))
		return false;

	// Set default video mode in XPRAM
	XPRAM[0x56] = 0x42;	// 'B'
	XPRAM[0x57] = 0x32;	// '2'
	const monitor_desc &main_monitor = *VideoMonitors[0];
	XPRAM[0x58] = uint8(main_monitor.depth_to_apple_mode(main_monitor.get_current_mode().depth));
	XPRAM[0x59] = 0;

#if EMULATED_68K
	// Init 680x0 emulation (this also activates the memory system which is needed for PatchROM())
	if (!Init680x0())
		return false;
#endif

	// Install ROM patches
	if (!PatchROM()) {
		ErrorAlert(STR_UNSUPPORTED_ROM_TYPE_ERR);
		return false;
	}

#if ENABLE_MON
	// Initialize mon
	mon_init();
	mon_read_byte = mon_read_byte_b2;
	mon_write_byte = mon_write_byte_b2;
#endif

	return true;
}

void CDROMOpenDone() {
}

/*
 *  Deinitialize everything
 */

void ExitAll(void)
{
#if ENABLE_MON
	// Deinitialize mon
	mon_exit();
#endif

	// Save XPRAM
	XPRAMExit();

	// Exit video
	VideoExit();

	// Exit audio
	AudioExit();

	// Exit ADB
	ADBExit();

	// Exit clipboard
	ClipExit();

	// Exit Time Manager
	TimerExit();

	// Exit serial ports
	SerialExit();

	// Exit network
	EtherExit();

#if SUPPORTS_EXTFS
	// Exit external file system
	ExtFSExit();
#endif

	// Exit drivers
	SCSIExit();
	CDROMExit();
	DiskExit();
	SonyExit();
}


/*
 *  Display error/warning alert given the message string ID
 */

void ErrorAlert(int string_id)
{
	ErrorAlert(GetString(string_id));
}

void WarningAlert(int string_id)
{
	WarningAlert(GetString(string_id));
}
