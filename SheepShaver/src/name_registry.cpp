/*
 *  name_registry.cpp - Name Registry handling
 *
 *  SheepShaver (C) 1997-2004 Christian Bauer and Marc Hellwig
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

#include "sysdeps.h"
#include "name_registry.h"
#include "main.h"
#include "macos_util.h"
#include "user_strings.h"
#include "emul_op.h"
#include "thunks.h"

#define DEBUG 0
#include "debug.h"


// Function pointers
typedef int16 (*rcec_ptr)(const RegEntryID *, const char *, RegEntryID *);
static uint32 rcec_tvect = 0;
static inline int16 RegistryCStrEntryCreate(const RegEntryID *arg1, const char *arg2, RegEntryID *arg3)
{
	return (int16)CallMacOS3(rcec_ptr, rcec_tvect, arg1, arg2, arg3);
}
typedef int16 (*rpc_ptr)(const RegEntryID *, const char *, const void *, uint32);
static uint32 rpc_tvect = 0;
static inline int16 RegistryPropertyCreate(const RegEntryID *arg1, const char *arg2, const void *arg3, uint32 arg4)
{
	return (int16)CallMacOS4(rpc_ptr, rpc_tvect, arg1, arg2, arg3, arg4);
}
#define RegistryPropertyCreateStr(e,n,s) RegistryPropertyCreate(e,n,s,strlen(s)+1)

// Video driver stub
static const uint8 video_driver[] = {
#include "VideoDriverStub.i"
};

// Ethernet driver stub
static const uint8 ethernet_driver[] = {
#include "EthernetDriverStub.i"
};

// Helper for RegEntryID
struct SheepRegEntryID : public SheepArray<sizeof(RegEntryID)> {
	RegEntryID *ptr() const { return (RegEntryID *)addr(); }
};

// Helper for a <uint32, uint32> pair
struct SheepPair : public SheepArray<8> {
	SheepPair(uint32 base, uint32 size) : SheepArray<8>()
		{ WriteMacInt32(addr(), base); WriteMacInt32(addr() + 4, size); }
	uint32 *ptr() const
		{ return (uint32 *)addr(); }
};


/*
 *  Patch Name Registry during startup
 */

void DoPatchNameRegistry(void)
{
	SheepVar32 u32;
	D(bug("Patching Name Registry..."));

	// Create "device-tree"
	SheepRegEntryID device_tree;
	if (!RegistryCStrEntryCreate(NULL, "Devices:device-tree", device_tree.ptr())) {
		u32.set_value(BusClockSpeed);
		RegistryPropertyCreate(device_tree.ptr(), "clock-frequency", u32.ptr(), 4);
		RegistryPropertyCreateStr(device_tree.ptr(), "model", "Power Macintosh");

		// Create "AAPL,ROM"
		SheepRegEntryID aapl_rom;
		if (!RegistryCStrEntryCreate(device_tree.ptr(), "AAPL,ROM", aapl_rom.ptr())) {
			RegistryPropertyCreateStr(aapl_rom.ptr(), "device_type", "rom");
			SheepPair reg(ROM_BASE, ROM_SIZE);
			RegistryPropertyCreate(aapl_rom.ptr(), "reg", reg.ptr(), 8);
		}

		// Create "PowerPC,60x"
		SheepRegEntryID power_pc;
		char *str;
		switch (PVR >> 16) {
			case 1:		// 601
				str = "PowerPC,601";
				break;
			case 3:		// 603
				str = "PowerPC,603";
				break;
			case 4:		// 604
				str = "PowerPC,604";
				break;
			case 6:		// 603e
				str = "PowerPC,603e";
				break;
			case 7:		// 603ev
				str = "PowerPC,603ev";
				break;
			case 8:		// 750
				str = "PowerPC,750";
				break;
			case 9:		// 604e
				str = "PowerPC,604e";
				break;
			case 10:	// 604ev5
				str = "PowerPC,604ev";
				break;
			case 50:	// 821
				str = "PowerPC,821";
				break;
			case 80:	// 860
				str = "PowerPC,860";
				break;
			default:
				str = "PowerPC,???";
				break;
		}
		if (!RegistryCStrEntryCreate(device_tree.ptr(), str, power_pc.ptr())) {
			u32.set_value(CPUClockSpeed);
			RegistryPropertyCreate(power_pc.ptr(), "clock-frequency", u32.ptr(), 4);
			u32.set_value(BusClockSpeed);
			RegistryPropertyCreate(power_pc.ptr(), "bus-frequency", u32.ptr(), 4);
			u32.set_value(TimebaseSpeed);
			RegistryPropertyCreate(power_pc.ptr(), "timebase-frequency", u32.ptr(), 4);
			u32.set_value(PVR);
			RegistryPropertyCreate(power_pc.ptr(), "cpu-version", u32.ptr(), 4);
			RegistryPropertyCreateStr(power_pc.ptr(), "device_type", "cpu");
			switch (PVR >> 16) {
				case 1:		// 601
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 3:		// 603
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x2000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x2000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 4:		// 604
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x4000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x4000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 6:		// 603e
				case 7:		// 603ev
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x4000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x4000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 8:		// 750, 750FX
				case 0x7000:
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 9:		// 604e
				case 10:	// 604ev5
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 12:	// 7400, 7410, 7450, 7455, 7457
				case 0x800c:
				case 0x8000:
				case 0x8001:
				case 0x8002:
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(32);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(64);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				case 0x39:	// 970
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-block-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-sets", u32.ptr(), 4);
					u32.set_value(0x8000);
					RegistryPropertyCreate(power_pc.ptr(), "d-cache-size", u32.ptr(), 4);
					u32.set_value(128);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-block-size", u32.ptr(), 4);
					u32.set_value(512);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-sets", u32.ptr(), 4);
					u32.set_value(0x10000);
					RegistryPropertyCreate(power_pc.ptr(), "i-cache-size", u32.ptr(), 4);
					u32.set_value(256);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-sets", u32.ptr(), 4);
					u32.set_value(0x1000);
					RegistryPropertyCreate(power_pc.ptr(), "tlb-size", u32.ptr(), 4);
					break;
				default:
					break;
			}
			u32.set_value(32);
			RegistryPropertyCreate(power_pc.ptr(), "reservation-granularity", u32.ptr(), 4);
			SheepPair reg(0, 0);
			RegistryPropertyCreate(power_pc.ptr(), "reg", reg.ptr(), 8);
		}

		// Create "memory"
		SheepRegEntryID memory;
		if (!RegistryCStrEntryCreate(device_tree.ptr(), "memory", memory.ptr())) {
			SheepPair reg(RAMBase, RAMSize);
			RegistryPropertyCreateStr(memory.ptr(), "device_type", "memory");
			RegistryPropertyCreate(memory.ptr(), "reg", reg.ptr(), 8);
		}

		// Create "video"
		SheepRegEntryID video;
		if (!RegistryCStrEntryCreate(device_tree.ptr(), "video", video.ptr())) {
			RegistryPropertyCreateStr(video.ptr(), "AAPL,connector", "monitor");
			RegistryPropertyCreateStr(video.ptr(), "device_type", "display");
			RegistryPropertyCreate(video.ptr(), "driver,AAPL,MacOS,PowerPC", video_driver, sizeof(video_driver));
			RegistryPropertyCreateStr(video.ptr(), "model", "SheepShaver Video");
		}

		// Create "ethernet"
		SheepRegEntryID ethernet;
		if (!RegistryCStrEntryCreate(device_tree.ptr(), "ethernet", ethernet.ptr())) {
			RegistryPropertyCreateStr(ethernet.ptr(), "AAPL,connector", "ethernet");
			RegistryPropertyCreateStr(ethernet.ptr(), "device_type", "network");
			RegistryPropertyCreate(ethernet.ptr(), "driver,AAPL,MacOS,PowerPC", ethernet_driver, sizeof(ethernet_driver));
			// local-mac-address
			// max-frame-size 2048
		}
	}
	D(bug("done.\n"));
}

void PatchNameRegistry(void)
{
	// Find RegistryCStrEntryCreate() and RegistryPropertyCreate() TVECTs
	rcec_tvect = (uint32)FindLibSymbol("\017NameRegistryLib", "\027RegistryCStrEntryCreate");
	D(bug("RegistryCStrEntryCreate TVECT at %08x\n", rcec_tvect));
	rpc_tvect = (uint32)FindLibSymbol("\017NameRegistryLib", "\026RegistryPropertyCreate");
	D(bug("RegistryPropertyCreate TVECT at %08x\n", rpc_tvect));
	if (rcec_tvect == 0 || rpc_tvect == 0) {
		ErrorAlert(GetString(STR_NO_NAME_REGISTRY_ERR));
		QuitEmulator();
	}

	// Main routine must be executed in PPC mode
	ExecuteNative(NATIVE_PATCH_NAME_REGISTRY);
}
