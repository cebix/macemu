/*
 *  name_registry.cpp - Name Registry handling
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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


/*
 *  Patch Name Registry during startup
 */

static void patch_name_registry(void)
{
	uint32 u32;
	D(bug("Patching Name Registry..."));

	// Create "device-tree"
	RegEntryID device_tree;
	if (!RegistryCStrEntryCreate(NULL, "Devices:device-tree", &device_tree)) {
		u32 = BusClockSpeed;
		RegistryPropertyCreate(&device_tree, "clock-frequency", &u32, 4);
		RegistryPropertyCreateStr(&device_tree, "model", "Power Macintosh");

		// Create "AAPL,ROM"
		RegEntryID aapl_rom;
		if (!RegistryCStrEntryCreate(&device_tree, "AAPL,ROM", &aapl_rom)) {
			RegistryPropertyCreateStr(&aapl_rom, "device_type", "rom");
			uint32 reg[2] = {ROM_BASE, ROM_SIZE};
			RegistryPropertyCreate(&aapl_rom, "reg", &reg, 8);
		}

		// Create "PowerPC,60x"
		RegEntryID power_pc;
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
		if (!RegistryCStrEntryCreate(&device_tree, str, &power_pc)) {
			u32 = CPUClockSpeed;
			RegistryPropertyCreate(&power_pc, "clock-frequency", &u32, 4);
			RegistryPropertyCreate(&power_pc, "cpu-version", &PVR, 4);
			RegistryPropertyCreateStr(&power_pc, "device_type", "cpu");
			switch (PVR >> 16) {
				case 1:		// 601
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 256;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				case 3:		// 603
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x2000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x2000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				case 4:		// 604
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x4000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x4000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				case 6:		// 603e
				case 7:		// 603ev
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x4000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x4000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				case 8:		// 750
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 256;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 256;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				case 9:		// 604e
				case 10:	// 604ev5
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "d-cache-block-size", &u32, 4);
					u32 = 256;
					RegistryPropertyCreate(&power_pc, "d-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "d-cache-size", &u32, 4);
					u32 = 32;
					RegistryPropertyCreate(&power_pc, "i-cache-block-size", &u32, 4);
					u32 = 256;
					RegistryPropertyCreate(&power_pc, "i-cache-sets", &u32, 4);
					u32 = 0x8000;
					RegistryPropertyCreate(&power_pc, "i-cache-size", &u32, 4);
					u32 = 64;
					RegistryPropertyCreate(&power_pc, "tlb-sets", &u32, 4);
					u32 = 128;
					RegistryPropertyCreate(&power_pc, "tlb-size", &u32, 4);
					break;
				default:
					break;
			}
			u32 = 32;
			RegistryPropertyCreate(&power_pc, "reservation-granularity", &u32, 4);
			uint32 reg[2] = {0, 0};
			RegistryPropertyCreate(&power_pc, "reg", &reg, 8);
		}

		// Create "memory"
		RegEntryID memory;
		if (!RegistryCStrEntryCreate(&device_tree, "memory", &memory)) {
			uint32 reg[2] = {RAMBase, RAMSize};
			RegistryPropertyCreateStr(&memory, "device_type", "memory");
			RegistryPropertyCreate(&memory, "reg", &reg, 8);
		}

		// Create "video"
		RegEntryID video;
		if (!RegistryCStrEntryCreate(&device_tree, "video", &video)) {
			RegistryPropertyCreateStr(&video, "AAPL,connector", "monitor");
			RegistryPropertyCreateStr(&video, "device_type", "display");
			RegistryPropertyCreate(&video, "driver,AAPL,MacOS,PowerPC", &video_driver, sizeof(video_driver));
			RegistryPropertyCreateStr(&video, "model", "SheepShaver Video");
		}

		// Create "ethernet"
		RegEntryID ethernet;
		if (!RegistryCStrEntryCreate(&device_tree, "ethernet", &ethernet)) {
			RegistryPropertyCreateStr(&ethernet, "AAPL,connector", "ethernet");
			RegistryPropertyCreateStr(&ethernet, "device_type", "network");
			RegistryPropertyCreate(&ethernet, "driver,AAPL,MacOS,PowerPC", &ethernet_driver, sizeof(ethernet_driver));
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
	ExecutePPC(patch_name_registry);
}
