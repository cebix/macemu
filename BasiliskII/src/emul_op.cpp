/*
 *  emul_op.cpp - 68k opcodes for ROM patches
 *
 *  Basilisk II (C) Christian Bauer
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
#include <stdio.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "rom_patches.h"
#include "rsrc_patches.h"
#include "xpram.h"
#include "adb.h"
#include "timer.h"
#include "clip.h"
#include "serial.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "video.h"
#include "audio.h"
#include "ether.h"
#include "extfs.h"
#include "emul_op.h"

#ifdef ENABLE_MON
#include "mon.h"
#endif

#define DEBUG 0
#include "debug.h"

extern bool tick_inhibit;

void PlayStartupSound();

/*
 *  Execute EMUL_OP opcode (called by 68k emulator or Illegal Instruction trap handler)
 */

void EmulOp(uint16 opcode, M68kRegisters *r)
{
	D(bug("EmulOp %04x\n", opcode));
	switch (opcode) {
		case M68K_EMUL_BREAK: {				// Breakpoint
			printf("*** Breakpoint\n");
			printf("d0 %08x d1 %08x d2 %08x d3 %08x\n"
				   "d4 %08x d5 %08x d6 %08x d7 %08x\n"
				   "a0 %08x a1 %08x a2 %08x a3 %08x\n"
				   "a4 %08x a5 %08x a6 %08x a7 %08x\n"
				   "sr %04x\n",
				   r->d[0], r->d[1], r->d[2], r->d[3], r->d[4], r->d[5], r->d[6], r->d[7],
				   r->a[0], r->a[1], r->a[2], r->a[3], r->a[4], r->a[5], r->a[6], r->a[7],
				   r->sr);
			VideoQuitFullScreen();
#ifdef ENABLE_MON
			const char *arg[4] = {"mon", "-m", "-r", NULL};
			mon(3, arg);
#endif
			QuitEmulator();
			break;
		}

		case M68K_EMUL_OP_SHUTDOWN:			// Quit emulator
			QuitEmulator();
			break;

		case M68K_EMUL_OP_RESET: {			// MacOS reset
			D(bug("*** RESET ***\n"));
			tick_inhibit = true;
			CDROMRemount(); // for System 7.x
			TimerReset();
			EtherReset();
			AudioReset();
#ifdef USE_SDL_AUDIO
			PlayStartupSound();
#endif
			// Create BootGlobs at top of memory
			Mac_memset(RAMBaseMac + RAMSize - 4096, 0, 4096);
			uint32 boot_globs = RAMBaseMac + RAMSize - 0x1c;
			WriteMacInt32(boot_globs + 0x00, RAMBaseMac);	// First RAM bank
			WriteMacInt32(boot_globs + 0x04, RAMSize);
			WriteMacInt32(boot_globs + 0x08, 0xffffffff);	// End of bank table
			WriteMacInt32(boot_globs + 0x0c, 0);

			// Setup registers for boot routine
			r->d[0] = ReadMacInt32(ROMBaseMac + UniversalInfo + 0x18);	// AddrMapFlags
			r->d[1] = ReadMacInt32(ROMBaseMac + UniversalInfo + 0x1c);	// UnivROMFlags
			r->d[2] = ReadMacInt32(ROMBaseMac + UniversalInfo + 0x10);	// HWCfgFlags/IDs
			if (FPUType)
				r->d[2] |= 0x10000000;									// Set FPU flag if FPU present
			else
				r->d[2] &= 0xefffffff;									// Clear FPU flag if no FPU present
			r->a[0] = ROMBaseMac + UniversalInfo + ReadMacInt32(ROMBaseMac + UniversalInfo);// AddrMap
			r->a[1] = ROMBaseMac + UniversalInfo;						// UniversalInfo
			r->a[6] = boot_globs;										// BootGlobs
			r->a[7] = RAMBaseMac + 0x10000;								// Boot stack
			tick_inhibit = false;
			break;
		}

		case M68K_EMUL_OP_CLKNOMEM: {		// Clock/PRAM operations
			bool is_read = (r->d[1] & 0x80) != 0;
			if ((r->d[1] & 0x78) == 0x38) {
				// XPRAM
				uint8 reg = ((r->d[1] << 5) & 0xe0) | ((r->d[1] >> 10) & 0x1f);
				if (is_read) {
					r->d[2] = XPRAM[reg];
					bool localtalk = !(XPRAM[0xe0] || XPRAM[0xe1]);	// LocalTalk enabled?
					switch (reg) {
						case 0x08:
							if (ROMVersion != ROM_VERSION_32)
								r->d[2] &= 0xf8;
							break;
						case 0x8a:
							r->d[2] |= 0x05;	// 32bit mode is always enabled
							break;
						case 0xe0:				// Disable LocalTalk (use EtherTalk instead)
							if (localtalk)
								r->d[2] = 0x00;
							break;
						case 0xe1:
							if (localtalk)
								r->d[2] = 0xf1;
							break;
						case 0xe2:
							if (localtalk)
								r->d[2] = 0x00;
							break;
						case 0xe3:
							if (localtalk)
								r->d[2] = 0x0a;
							break;
					}
					D(bug("Read XPRAM %02x->%02lx\n", reg, r->d[2]));
				} else {
					D(bug("Write XPRAM %02x<-%02lx\n", reg, r->d[2] & 0xff));
					if (reg == 0x8a && !TwentyFourBitAddressing)
						r->d[2] |= 0x05;	// 32bit mode is always enabled if possible
					XPRAM[reg] = r->d[2];
				}
			} else {
				// PRAM, RTC and other clock registers
				uint8 reg = (r->d[1] >> 2) & 0x1f;
				if (reg >= 0x10 || (reg >= 0x08 && reg < 0x0c)) {
					if (is_read) {
						r->d[2] = XPRAM[reg];
						D(bug("Read XPRAM %02x->%02x\n", reg, XPRAM[reg]));
					} else {
						D(bug("Write PRAM %02x<-%02lx\n", reg, r->d[2]));
						XPRAM[reg] = r->d[2];
					}
				} else if (reg < 0x08 && is_read) {
					uint32 t = TimerDateTime();
					uint8 b = t;
					switch (reg & 3) {
						case 1: b = t >> 8; break;
						case 2: b = t >> 16; break;
						case 3: b = t >> 24; break;
					}
					r->d[2] = b;
				} else
					D(bug("RTC %s op %d, d1 %08lx d2 %08lx\n", is_read ? "read" : "write", reg, r->d[1], r->d[2]));
			}
			r->d[0] = 0;
			r->d[1] = r->d[2];
			break;
		}

		case M68K_EMUL_OP_READ_XPRAM:		// Read from XPRAM (ROM10/11)
			D(bug("Read XPRAM %02lx\n", r->d[1]));
			r->d[1] = XPRAM[r->d[1] & 0xff];
			break;

		case M68K_EMUL_OP_READ_XPRAM2:		// Read from XPRAM (ROM15)
			D(bug("Read XPRAM %02lx\n", r->d[0]));
			r->d[0] = XPRAM[r->d[0] & 0xff];
			break;

		case M68K_EMUL_OP_PATCH_BOOT_GLOBS:	// Patch BootGlobs at startup
			D(bug("Patch BootGlobs\n"));
			WriteMacInt32(r->a[4] - 20, RAMBaseMac + RAMSize);			// MemTop
			WriteMacInt8(r->a[4] - 26, 0);								// No MMU
			WriteMacInt8(r->a[4] - 25, ReadMacInt8(r->a[4] - 25) | 1);	// No MMU
			r->a[6] = RAMBaseMac + RAMSize;
			break;

		case M68K_EMUL_OP_FIX_BOOTSTACK:	// Set boot stack to 3/4 of RAM (7.5)
			r->a[1] = RAMBaseMac + RAMSize * 3 / 4;
			D(bug("Fix boot stack %08x\n", r->a[1]));
			break;

		case M68K_EMUL_OP_FIX_MEMSIZE: {	// Set correct logical and physical memory size
			D(bug("Fix MemSize\n"));
			uint32 diff = ReadMacInt32(0x1ef8) - ReadMacInt32(0x1ef4);	// Difference between logical and physical size
			WriteMacInt32(0x1ef8, RAMSize);			// Physical RAM size
			WriteMacInt32(0x1ef4, RAMSize - diff);	// Logical RAM size
			break;
		}

		case M68K_EMUL_OP_ADBOP:			// ADBOp() replacement
			ADBOp(r->d[0], Mac2HostAddr(ReadMacInt32(r->a[0])));
			break;

		case M68K_EMUL_OP_INSTIME:			// InsTime() replacement
			r->d[0] = InsTime(r->a[0], r->d[1]);
			break;

		case M68K_EMUL_OP_RMVTIME:			// RmvTime() replacement
			r->d[0] = RmvTime(r->a[0]);
			break;

		case M68K_EMUL_OP_PRIMETIME:		// PrimeTime() replacement
			r->d[0] = PrimeTime(r->a[0], r->d[0]);
			break;

		case M68K_EMUL_OP_MICROSECONDS: 	// Microseconds() replacement
			Microseconds(r->a[0], r->d[0]);
			break;

		case M68K_EMUL_OP_INSTALL_DRIVERS: {// Patch to install our own drivers during startup
			// Install drivers
			D(bug("InstallDrivers\n"));
			InstallDrivers(r->a[0]);

			// Install PutScrap() patch
			M68kRegisters r;
			if (PutScrapPatch) {
				r.d[0] = 0xa9fe;
				r.a[0] = PutScrapPatch;
				Execute68kTrap(0xa647, &r);	// SetToolTrap()
			}

			// Install GetScrap() patch
			if (GetScrapPatch) {
				r.d[0] = 0xa9fd;
				r.a[0] = GetScrapPatch;
				Execute68kTrap(0xa647, &r);	// SetToolTrap()
			}

			// Setup fake ASC registers
			if (ROMVersion == ROM_VERSION_32) {
				r.d[0] = 0x1000;
				Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
				uint32 asc_regs = r.a[0];
				D(bug("ASC registers at %08lx\n", asc_regs));
				WriteMacInt8(asc_regs + 0x800, 0x0f);	// Set ASC version number
				WriteMacInt32(0xcc0, asc_regs);			// Set ASCBase
			}
			break;
		}

		case M68K_EMUL_OP_SERD:				// Install serial drivers
			D(bug("InstallSERD\n"));
			InstallSERD();
			break;

		case M68K_EMUL_OP_SONY_OPEN:		// Floppy driver functions
			r->d[0] = SonyOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SONY_PRIME:
			r->d[0] = SonyPrime(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SONY_CONTROL:
			r->d[0] = SonyControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SONY_STATUS:
			r->d[0] = SonyStatus(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_DISK_OPEN:		// Disk driver functions
			r->d[0] = DiskOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_DISK_PRIME:
			r->d[0] = DiskPrime(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_DISK_CONTROL:
			r->d[0] = DiskControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_DISK_STATUS:
			r->d[0] = DiskStatus(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_CDROM_OPEN:		// CD-ROM driver functions
			r->d[0] = CDROMOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_CDROM_PRIME:
			r->d[0] = CDROMPrime(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_CDROM_CONTROL:
			r->d[0] = CDROMControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_CDROM_STATUS:
			r->d[0] = CDROMStatus(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_VIDEO_OPEN:		// Video driver functions
			r->d[0] = VideoDriverOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_VIDEO_CONTROL:
			r->d[0] = VideoDriverControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_VIDEO_STATUS:
			r->d[0] = VideoDriverStatus(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SERIAL_OPEN:		// Serial driver functions
			r->d[0] = SerialOpen(r->a[0], r->a[1], r->d[0]);
			break;

		case M68K_EMUL_OP_SERIAL_PRIME:
			r->d[0] = SerialPrime(r->a[0], r->a[1], r->d[0]);
			break;

		case M68K_EMUL_OP_SERIAL_CONTROL:
			r->d[0] = SerialControl(r->a[0], r->a[1], r->d[0]);
			break;

		case M68K_EMUL_OP_SERIAL_STATUS:
			r->d[0] = SerialStatus(r->a[0], r->a[1], r->d[0]);
			break;

		case M68K_EMUL_OP_SERIAL_CLOSE:
			r->d[0] = SerialClose(r->a[0], r->a[1], r->d[0]);
			break;

		case M68K_EMUL_OP_ETHER_OPEN:		// Ethernet driver functions
			r->d[0] = EtherOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_ETHER_CONTROL:
			r->d[0] = EtherControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_ETHER_READ_PACKET:
			EtherReadPacket(r->a[0], r->a[3], r->d[3], r->d[1]);
			break;

		case M68K_EMUL_OP_SOUNDIN_OPEN:		// Sound input driver functions
			r->d[0] = SoundInOpen(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SOUNDIN_PRIME:
			r->d[0] = SoundInPrime(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SOUNDIN_CONTROL:
			r->d[0] = SoundInControl(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SOUNDIN_STATUS:
			r->d[0] = SoundInStatus(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SOUNDIN_CLOSE:
			r->d[0] = SoundInClose(r->a[0], r->a[1]);
			break;

		case M68K_EMUL_OP_SCSI_DISPATCH: {	// SCSIDispatch() replacement
			uint32 ret = ReadMacInt32(r->a[7]);		// Get return address
			uint16 sel = ReadMacInt16(r->a[7] + 4);	// Get selector
			r->a[7] += 6;
			int stack = 0;
			switch (sel) {
				case 0:		// SCSIReset
					WriteMacInt16(r->a[7], SCSIReset());
					stack = 0;
					break;
				case 1:		// SCSIGet
					WriteMacInt16(r->a[7], SCSIGet());
					stack = 0;
					break;
				case 2:		// SCSISelect
				case 11:	// SCSISelAtn
					WriteMacInt16(r->a[7] + 2, SCSISelect(ReadMacInt16(r->a[7]) & 0xff));
					stack = 2;
					break;
				case 3:		// SCSICmd
					WriteMacInt16(r->a[7] + 6, SCSICmd(ReadMacInt16(r->a[7]), Mac2HostAddr(ReadMacInt32(r->a[7] + 2))));
					stack = 6;
					break;
				case 4:		// SCSIComplete
					WriteMacInt16(r->a[7] + 12, SCSIComplete(ReadMacInt32(r->a[7]), ReadMacInt32(r->a[7] + 4), ReadMacInt32(r->a[7] + 8)));
					stack = 12;
					break;
				case 5:		// SCSIRead
				case 8:		// SCSIRBlind
					WriteMacInt16(r->a[7] + 4, SCSIRead(ReadMacInt32(r->a[7])));
					stack = 4;
					break;
				case 6:		// SCSIWrite
				case 9:		// SCSIWBlind
					WriteMacInt16(r->a[7] + 4, SCSIWrite(ReadMacInt32(r->a[7])));
					stack = 4;
					break;
				case 10:	// SCSIStat
					WriteMacInt16(r->a[7], SCSIStat());
					stack = 0;
					break;
				case 12:	// SCSIMsgIn
					WriteMacInt16(r->a[7] + 4, 0);
					stack = 4;
					break;
				case 13:	// SCSIMsgOut
					WriteMacInt16(r->a[7] + 2, 0);
					stack = 2;
					break;
				case 14:	// SCSIMgrBusy
					WriteMacInt16(r->a[7], SCSIMgrBusy());
					stack = 0;
					break;
				default:
					printf("FATAL: SCSIDispatch(%d): illegal selector\n", sel);
					QuitEmulator();
					break;
			}
			r->a[0] = ret;			// "rtd" emulation, a0 = return address, a1 = new stack pointer
			r->a[1] = r->a[7] + stack;
			break;
		}

		case M68K_EMUL_OP_IRQ:			// Level 1 interrupt
			r->d[0] = 0;

			if (InterruptFlags & INTFLAG_60HZ) {
				ClearInterruptFlag(INTFLAG_60HZ);

				// Increment Ticks variable
				WriteMacInt32(0x16a, ReadMacInt32(0x16a) + 1);

				if (HasMacStarted()) {

					// Mac has started, execute all 60Hz interrupt functions
#if !PRECISE_TIMING
					TimerInterrupt();
#endif
					VideoInterrupt();

					// Call DoVBLTask(0)
					if (ROMVersion == ROM_VERSION_32) {
						M68kRegisters r2;
						r2.d[0] = 0;
						Execute68kTrap(0xa072, &r2);
					}

					r->d[0] = 1;			// Flag: 68k interrupt routine executes VBLTasks etc.
				}
			}

			if (InterruptFlags & INTFLAG_1HZ) {
				ClearInterruptFlag(INTFLAG_1HZ);
				if (HasMacStarted()) {
					SonyInterrupt();
					DiskInterrupt();
					CDROMInterrupt();
				}
			}

			if (InterruptFlags & INTFLAG_SERIAL) {
				ClearInterruptFlag(INTFLAG_SERIAL);
				SerialInterrupt();
			}

			if (InterruptFlags & INTFLAG_ETHER) {
				ClearInterruptFlag(INTFLAG_ETHER);
				EtherInterrupt();
			}
#if PRECISE_TIMING
			if (InterruptFlags & INTFLAG_TIMER) {
				ClearInterruptFlag(INTFLAG_TIMER);
				TimerInterrupt();
			}
#endif
			if (InterruptFlags & INTFLAG_AUDIO) {
				ClearInterruptFlag(INTFLAG_AUDIO);
				AudioInterrupt();
			}

			if (InterruptFlags & INTFLAG_ADB) {
				ClearInterruptFlag(INTFLAG_ADB);
				if (HasMacStarted())
					ADBInterrupt();
			}

			if (InterruptFlags & INTFLAG_NMI) {
				ClearInterruptFlag(INTFLAG_NMI);
				if (HasMacStarted())
					TriggerNMI();
			}
			break;

		case M68K_EMUL_OP_PUT_SCRAP: {		// PutScrap() patch
			void *scrap = Mac2HostAddr(ReadMacInt32(r->a[7] + 4));
			uint32 type = ReadMacInt32(r->a[7] + 8);
			int32 length = ReadMacInt32(r->a[7] + 12);
			PutScrap(type, scrap, length);
			break;
		}

		case M68K_EMUL_OP_GET_SCRAP: {		// GetScrap() patch
			void **scrap_handle = (void **)Mac2HostAddr(ReadMacInt32(r->a[7] + 4));
			uint32 type = ReadMacInt32(r->a[7] + 8);
			int32 length = ReadMacInt32(r->a[7] + 12);
			GetScrap(scrap_handle, type, length);
			break;
		}

		case M68K_EMUL_OP_CHECKLOAD: {		// vCheckLoad() patch (resource loader)
			uint32 type = r->d[1];
			int16 id = ReadMacInt16(r->a[2]);
			if (r->a[0] == 0)
				break;
			uint32 adr = ReadMacInt32(r->a[0]);
			if (adr == 0)
				break;
			uint8 *p = Mac2HostAddr(adr);
			uint32 size = ReadMacInt32(adr - 8) & 0xffffff;
			CheckLoad(type, id, p, size);
			break;
		}

		case M68K_EMUL_OP_AUDIO:			// Audio component dispatch function
			r->d[0] = AudioDispatch(r->a[3], r->a[4]);
			break;

#if SUPPORTS_EXTFS
		case M68K_EMUL_OP_EXTFS_COMM:		// External file system routines
			WriteMacInt16(r->a[7] + 14, ExtFSComm(ReadMacInt16(r->a[7] + 12), ReadMacInt32(r->a[7] + 8), ReadMacInt32(r->a[7] + 4)));
			break;

		case M68K_EMUL_OP_EXTFS_HFS:
			WriteMacInt16(r->a[7] + 20, ExtFSHFS(ReadMacInt32(r->a[7] + 16), ReadMacInt16(r->a[7] + 14), ReadMacInt32(r->a[7] + 10), ReadMacInt32(r->a[7] + 6), ReadMacInt16(r->a[7] + 4)));
			break;
#endif

		case M68K_EMUL_OP_BLOCK_MOVE:		// BlockMove() cache flushing
			FlushCodeCache(Mac2HostAddr(r->a[0]), r->a[1]);
			break;

		case M68K_EMUL_OP_DEBUGUTIL:
		//	printf("DebugUtil d0=%08lx  a5=%08lx\n", r->d[0], r->a[5]);
			r->d[0] = DebugUtil(r->d[0]);
			break;

		case M68K_EMUL_OP_IDLE_TIME:	// SynchIdleTime() patch
			// Sleep if no events pending
			if (ReadMacInt32(0x14c) == 0)
				idle_wait();
			r->a[0] = ReadMacInt32(0x2b6);
			break;

		case M68K_EMUL_OP_SUSPEND: {
			printf("*** Suspend\n");
			printf("d0 %08x d1 %08x d2 %08x d3 %08x\n"
				   "d4 %08x d5 %08x d6 %08x d7 %08x\n"
				   "a0 %08x a1 %08x a2 %08x a3 %08x\n"
				   "a4 %08x a5 %08x a6 %08x a7 %08x\n"
				   "sr %04x\n",
				   r->d[0], r->d[1], r->d[2], r->d[3], r->d[4], r->d[5], r->d[6], r->d[7],
				   r->a[0], r->a[1], r->a[2], r->a[3], r->a[4], r->a[5], r->a[6], r->a[7],
				   r->sr);
#ifdef ENABLE_MON
			const char *arg[4] = {"mon", "-m", "-r", NULL};
			mon(3, arg);
#endif
			break;
		}

		default:
			printf("FATAL: EMUL_OP called with bogus opcode %08x\n", opcode);
			printf("d0 %08x d1 %08x d2 %08x d3 %08x\n"
				   "d4 %08x d5 %08x d6 %08x d7 %08x\n"
				   "a0 %08x a1 %08x a2 %08x a3 %08x\n"
				   "a4 %08x a5 %08x a6 %08x a7 %08x\n"
				   "sr %04x\n",
				   r->d[0], r->d[1], r->d[2], r->d[3], r->d[4], r->d[5], r->d[6], r->d[7],
				   r->a[0], r->a[1], r->a[2], r->a[3], r->a[4], r->a[5], r->a[6], r->a[7],
				   r->sr);
#ifdef ENABLE_MON
			const char *arg[4] = {"mon", "-m", "-r", NULL};
			mon(3, arg);
#endif
			QuitEmulator();
			break;
	}
}
