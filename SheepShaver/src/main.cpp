/*
 *  main.cpp - ROM patches
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

#include "main.h"
#include "version.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "xlowmem.h"
#include "xpram.h"
#include "timer.h"
#include "adb.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "video.h"
#include "audio.h"
#include "ether.h"
#include "serial.h"
#include "clip.h"
#include "extfs.h"
#include "sys.h"
#include "macos_util.h"
#include "rom_patches.h"
#include "user_strings.h"
#include "vm_alloc.h"
#include "sigsegv.h"
#include "thunks.h"

#define DEBUG 0
#include "debug.h"

#ifdef ENABLE_MON
#include "mon.h"

static uint32 sheepshaver_read_byte(uintptr adr)
{
	return ReadMacInt8(adr);
}

static void sheepshaver_write_byte(uintptr adr, uint32 b)
{
	WriteMacInt8(adr, b);
}
#endif


/*
 *  Initialize everything, returns false on error
 */

bool InitAll(void)
{
	// Load NVRAM
	XPRAMInit();

	// Load XPRAM default values if signature not found
	if (XPRAM[0x130c] != 0x4e || XPRAM[0x130d] != 0x75
	 || XPRAM[0x130e] != 0x4d || XPRAM[0x130f] != 0x63) {
		D(bug("Loading XPRAM default values\n"));
		memset(XPRAM + 0x1300, 0, 0x100);
		XPRAM[0x130c] = 0x4e;	// "NuMc" signature
		XPRAM[0x130d] = 0x75;
		XPRAM[0x130e] = 0x4d;
		XPRAM[0x130f] = 0x63;
		XPRAM[0x1301] = 0x80;	// InternalWaitFlags = DynWait (don't wait for SCSI devices upon bootup)
		XPRAM[0x1310] = 0xa8;	// Standard PRAM values
		XPRAM[0x1311] = 0x00;
		XPRAM[0x1312] = 0x00;
		XPRAM[0x1313] = 0x22;
		XPRAM[0x1314] = 0xcc;
		XPRAM[0x1315] = 0x0a;
		XPRAM[0x1316] = 0xcc;
		XPRAM[0x1317] = 0x0a;
		XPRAM[0x131c] = 0x00;
		XPRAM[0x131d] = 0x02;
		XPRAM[0x131e] = 0x63;
		XPRAM[0x131f] = 0x00;
		XPRAM[0x1308] = 0x13;
		XPRAM[0x1309] = 0x88;
		XPRAM[0x130a] = 0x00;
		XPRAM[0x130b] = 0xcc;
		XPRAM[0x1376] = 0x00;	// OSDefault = MacOS
		XPRAM[0x1377] = 0x01;
	}

	// Set boot volume
	int16 i16 = PrefsFindInt32("bootdrive");
	XPRAM[0x1378] = i16 >> 8;
	XPRAM[0x1379] = i16 & 0xff;
	i16 = PrefsFindInt32("bootdriver");
	XPRAM[0x137a] = i16 >> 8;
	XPRAM[0x137b] = i16 & 0xff;

	// Create BootGlobs at top of Mac memory
	memset(RAMBaseHost + RAMSize - 4096, 0, 4096);
	BootGlobsAddr = RAMBase + RAMSize - 0x1c;
	WriteMacInt32(BootGlobsAddr - 5 * 4, RAMBase + RAMSize);	// MemTop
	WriteMacInt32(BootGlobsAddr + 0 * 4, RAMBase);				// First RAM bank
	WriteMacInt32(BootGlobsAddr + 1 * 4, RAMSize);
	WriteMacInt32(BootGlobsAddr + 2 * 4, (uint32)-1);			// End of bank table

	// Init thunks
	if (!ThunksInit())
		return false;

	// Init drivers
	SonyInit();
	DiskInit();
	CDROMInit();
	SCSIInit();

	// Init external file system
	ExtFSInit(); 

	// Init ADB
	ADBInit();

	// Init audio
	AudioInit();

	// Init network
	EtherInit();

	// Init serial ports
	SerialInit();

	// Init Time Manager
	TimerInit();

	// Init clipboard
	ClipInit();

	// Init video
	if (!VideoInit())
		return false;

	// Install ROM patches
	if (!PatchROM()) {
		ErrorAlert(GetString(STR_UNSUPPORTED_ROM_TYPE_ERR));
		return false;
	}

	// Initialize Kernel Data
	KernelData *kernel_data = (KernelData *)Mac2HostAddr(KERNEL_DATA_BASE);
	memset(kernel_data, 0, sizeof(KernelData));
	if (ROMType == ROMTYPE_NEWWORLD) {
		uint32 of_dev_tree = SheepMem::Reserve(4 * sizeof(uint32));
		Mac_memset(of_dev_tree, 0, 4 * sizeof(uint32));
		uint32 vector_lookup_tbl = SheepMem::Reserve(128);
		uint32 vector_mask_tbl = SheepMem::Reserve(64);
		memset((uint8 *)kernel_data + 0xb80, 0x3d, 0x80);
		Mac_memset(vector_lookup_tbl, 0, 128);
		Mac_memset(vector_mask_tbl, 0, 64);
		kernel_data->v[0xb80 >> 2] = htonl(ROM_BASE);
		kernel_data->v[0xb84 >> 2] = htonl(of_dev_tree);			// OF device tree base
		kernel_data->v[0xb90 >> 2] = htonl(vector_lookup_tbl);
		kernel_data->v[0xb94 >> 2] = htonl(vector_mask_tbl);
		kernel_data->v[0xb98 >> 2] = htonl(ROM_BASE);				// OpenPIC base
		kernel_data->v[0xbb0 >> 2] = htonl(0);						// ADB base
		kernel_data->v[0xc20 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc24 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc30 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc34 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc38 >> 2] = htonl(0x00010020);
		kernel_data->v[0xc3c >> 2] = htonl(0x00200001);
		kernel_data->v[0xc40 >> 2] = htonl(0x00010000);
		kernel_data->v[0xc50 >> 2] = htonl(RAMBase);
		kernel_data->v[0xc54 >> 2] = htonl(RAMSize);
		kernel_data->v[0xf60 >> 2] = htonl(PVR);
		kernel_data->v[0xf64 >> 2] = htonl(CPUClockSpeed);			// clock-frequency
		kernel_data->v[0xf68 >> 2] = htonl(BusClockSpeed);			// bus-frequency
		kernel_data->v[0xf6c >> 2] = htonl(TimebaseSpeed);			// timebase-frequency
	} else if (ROMType == ROMTYPE_GOSSAMER) {
		kernel_data->v[0xc80 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc84 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc90 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc94 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc98 >> 2] = htonl(0x00010020);
		kernel_data->v[0xc9c >> 2] = htonl(0x00200001);
		kernel_data->v[0xca0 >> 2] = htonl(0x00010000);
		kernel_data->v[0xcb0 >> 2] = htonl(RAMBase);
		kernel_data->v[0xcb4 >> 2] = htonl(RAMSize);
		kernel_data->v[0xf60 >> 2] = htonl(PVR);
		kernel_data->v[0xf64 >> 2] = htonl(CPUClockSpeed);			// clock-frequency
		kernel_data->v[0xf68 >> 2] = htonl(BusClockSpeed);			// bus-frequency
		kernel_data->v[0xf6c >> 2] = htonl(TimebaseSpeed);			// timebase-frequency
	} else {
		kernel_data->v[0xc80 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc84 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc90 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc94 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc98 >> 2] = htonl(0x00010020);
		kernel_data->v[0xc9c >> 2] = htonl(0x00200001);
		kernel_data->v[0xca0 >> 2] = htonl(0x00010000);
		kernel_data->v[0xcb0 >> 2] = htonl(RAMBase);
		kernel_data->v[0xcb4 >> 2] = htonl(RAMSize);
		kernel_data->v[0xf80 >> 2] = htonl(PVR);
		kernel_data->v[0xf84 >> 2] = htonl(CPUClockSpeed);			// clock-frequency
		kernel_data->v[0xf88 >> 2] = htonl(BusClockSpeed);			// bus-frequency
		kernel_data->v[0xf8c >> 2] = htonl(TimebaseSpeed);			// timebase-frequency
	}

	// Initialize extra low memory
	D(bug("Initializing Low Memory...\n"));
	Mac_memset(0, 0, 0x3000);
	WriteMacInt32(XLM_SIGNATURE, FOURCC('B','a','a','h'));			// Signature to detect SheepShaver
	WriteMacInt32(XLM_KERNEL_DATA, KernelDataAddr);					// For trap replacement routines
	WriteMacInt32(XLM_PVR, PVR);									// Theoretical PVR
	WriteMacInt32(XLM_BUS_CLOCK, BusClockSpeed);					// For DriverServicesLib patch
	WriteMacInt16(XLM_EXEC_RETURN_OPCODE, M68K_EXEC_RETURN);		// For Execute68k() (RTS from the executed 68k code will jump here and end 68k mode)
	WriteMacInt32(XLM_ZERO_PAGE, SheepMem::ZeroPage());				// Pointer to read-only page with all bits set to 0
#if !EMULATED_PPC
#ifdef SYSTEM_CLOBBERS_R2
	WriteMacInt32(XLM_TOC, (uint32)TOC);							// TOC pointer of emulator
#endif
#ifdef SYSTEM_CLOBBERS_R13
	WriteMacInt32(XLM_R13, (uint32)R13);							// TLS register
#endif
#endif

	WriteMacInt32(XLM_ETHER_AO_GET_HWADDR, NativeFunction(NATIVE_ETHER_AO_GET_HWADDR));	// Low level ethernet driver functions
	WriteMacInt32(XLM_ETHER_AO_ADD_MULTI, NativeFunction(NATIVE_ETHER_AO_ADD_MULTI));
	WriteMacInt32(XLM_ETHER_AO_DEL_MULTI, NativeFunction(NATIVE_ETHER_AO_DEL_MULTI));
	WriteMacInt32(XLM_ETHER_AO_SEND_PACKET, NativeFunction(NATIVE_ETHER_AO_SEND_PACKET));

	WriteMacInt32(XLM_ETHER_INIT, NativeFunction(NATIVE_ETHER_INIT));	// DLPI ethernet driver functions
	WriteMacInt32(XLM_ETHER_TERM, NativeFunction(NATIVE_ETHER_TERM));
	WriteMacInt32(XLM_ETHER_OPEN, NativeFunction(NATIVE_ETHER_OPEN));
	WriteMacInt32(XLM_ETHER_CLOSE, NativeFunction(NATIVE_ETHER_CLOSE));
	WriteMacInt32(XLM_ETHER_WPUT, NativeFunction(NATIVE_ETHER_WPUT));
	WriteMacInt32(XLM_ETHER_RSRV, NativeFunction(NATIVE_ETHER_RSRV));
	WriteMacInt32(XLM_VIDEO_DOIO, NativeFunction(NATIVE_VIDEO_DO_DRIVER_IO));
	D(bug("Low Memory initialized\n"));

#if ENABLE_MON
	// Initialize mon
	mon_init();
	mon_read_byte = sheepshaver_read_byte;
	mon_write_byte = sheepshaver_write_byte;
#endif

	return true;
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

	// Save NVRAM
	XPRAMExit();

	// Exit clipboard
	ClipExit();

	// Exit Time Manager
	TimerExit();

	// Exit serial
	SerialExit();

	// Exit network
	EtherExit();

	// Exit audio
	AudioExit();

	// Exit ADB
	ADBExit();

	// Exit video
	VideoExit();

	// Exit external file system
	ExtFSExit();

	// Exit drivers
	SCSIExit();
	CDROMExit();
	DiskExit();
	SonyExit();

	// Delete thunks
	ThunksExit();
}


/*
 *  Patch things after system startup (gets called by disk driver accRun routine)
 */

void PatchAfterStartup(void)
{
	ExecuteNative(NATIVE_VIDEO_INSTALL_ACCEL);
	InstallExtFS();
}
