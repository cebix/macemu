/*
 *  main_amiga.cpp - Startup code for AmigaOS
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

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <devices/timer.h>
#include <devices/ahi.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "xpram.h"
#include "timer.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "audio.h"
#include "video.h"
#include "serial.h"
#include "ether.h"
#include "clip.h"
#include "emul_op.h"
#include "rom_patches.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "sys.h"
#include "user_strings.h"
#include "version.h"

#define DEBUG 0
#include "debug.h"


// Options for libnix
unsigned long __stack = 0x4000;		// Stack requirement
int __nocommandline = 1;			// Disable command line parsing


// Constants
static const char ROM_FILE_NAME[] = "ROM";
static const char __ver[] = "$VER: " VERSION_STRING " " __DATE__;
static const int SCRATCH_MEM_SIZE = 65536;


// RAM and ROM pointers
uint32 RAMBaseMac;		// RAM base (Mac address space)
uint8 *RAMBaseHost;		// RAM base (host address space)
uint32 RAMSize;			// Size of RAM
uint32 ROMBaseMac;		// ROM base (Mac address space)
uint8 *ROMBaseHost;		// ROM base (host address space)
uint32 ROMSize;			// Size of ROM


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables
extern ExecBase *SysBase;
struct Library *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Library *GadToolsBase = NULL;
struct Library *IFFParseBase = NULL;
struct Library *AslBase = NULL;
struct Library *P96Base = NULL;
struct Library *CyberGfxBase = NULL;
struct Library *TimerBase = NULL;
struct Library *AHIBase = NULL;
struct Library *DiskBase = NULL;

struct Task *MainTask;							// Our task
uint8 *ScratchMem = NULL;						// Scratch memory for Mac ROM writes
APTR OldTrapHandler = NULL;						// Old trap handler
APTR OldExceptionHandler = NULL;				// Old exception handler
BYTE IRQSig = -1;								// "Interrupt" signal number
ULONG IRQSigMask = 0;							// "Interrupt" signal mask

static struct timerequest *timereq = NULL;		// IORequest for timer

static struct MsgPort *ahi_port = NULL;			// Port for AHI
static struct AHIRequest *ahi_io = NULL;		// IORequest for AHI

static struct Process *xpram_proc = NULL;		// XPRAM watchdog
static volatile bool xpram_proc_active = true;	// Flag for quitting the XPRAM watchdog

static struct Process *tick_proc = NULL;		// 60Hz process
static volatile bool tick_proc_active = true;	// Flag for quitting the 60Hz process

static bool stack_swapped = false;				// Stack swapping
static StackSwapStruct stack_swap;


// Assembly functions
struct trap_regs;
extern "C" void AtomicAnd(uint32 *p, uint32 val);
extern "C" void AtomicOr(uint32 *p, uint32 val);
extern "C" void MoveVBR(void);
extern "C" void DisableSuperBypass(void);
extern "C" void TrapHandlerAsm(void);
extern "C" void ExceptionHandlerAsm(void);
extern "C" void IllInstrHandler(trap_regs *regs);
extern "C" void PrivViolHandler(trap_regs *regs);
extern "C" void quit_emulator(void);
extern "C" void AsmTriggerNMI(void);
uint16 EmulatedSR;					// Emulated SR (supervisor bit and interrupt mask)


// Prototypes
static void jump_to_rom(void);
static void xpram_func(void);
static void tick_func(void);


/*
 *  Main program
 */

int main(int argc, char **argv)
{
	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	MainTask = FindTask(NULL);
	struct DateStamp ds;
	DateStamp(&ds);
	srand(ds.ds_Tick);

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Open libraries
	GfxBase = OpenLibrary((UBYTE *) "graphics.library", 39);
	if (GfxBase == NULL) {
		printf("Cannot open graphics.library V39.\n");
		exit(1);
	}
	IntuitionBase = (struct IntuitionBase *)OpenLibrary((UBYTE *) "intuition.library", 39);
	if (IntuitionBase == NULL) {
		printf("Cannot open intuition.library V39.\n");
		CloseLibrary(GfxBase);
		exit(1);
	}
	DiskBase = (struct Library *)OpenResource((UBYTE *) "disk.resource");
	if (DiskBase == NULL)
		QuitEmulator();
	GadToolsBase = OpenLibrary((UBYTE *) "gadtools.library", 39);
	if (GadToolsBase == NULL) {
		ErrorAlert(STR_NO_GADTOOLS_LIB_ERR);
		QuitEmulator();
	}
	IFFParseBase = OpenLibrary((UBYTE *) "iffparse.library", 39);
	if (IFFParseBase == NULL) {
		ErrorAlert(STR_NO_IFFPARSE_LIB_ERR);
		QuitEmulator();
	}
	AslBase = OpenLibrary((UBYTE *) "asl.library", 36);
	if (AslBase == NULL) {
		ErrorAlert(STR_NO_ASL_LIB_ERR);
		QuitEmulator();
	}

	if (FindTask((UBYTE *) "« Enforcer »")) {
		ErrorAlert(STR_ENFORCER_RUNNING_ERR);
		QuitEmulator();
	}

	// These two can fail (the respective gfx support won't be available, then)
	P96Base = OpenLibrary((UBYTE *) "Picasso96API.library", 2);
	CyberGfxBase = OpenLibrary((UBYTE *) "cybergraphics.library", 2);

	// Read preferences
	PrefsInit(NULL, argc, argv);

	// Open AHI
	ahi_port = CreateMsgPort();
	if (ahi_port) {
		ahi_io = (struct AHIRequest *)CreateIORequest(ahi_port, sizeof(struct AHIRequest));
		if (ahi_io) {
			ahi_io->ahir_Version = 2;
			if (OpenDevice((UBYTE *) AHINAME, AHI_NO_UNIT, (struct IORequest *)ahi_io, 0) == 0) {
				AHIBase = (struct Library *)ahi_io->ahir_Std.io_Device;
			}
		}
	}

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			QuitEmulator();

	// Check start of Chip memory (because we need access to 0x0000..0x2000)
	if ((uint32)FindName(&SysBase->MemList, (UBYTE *) "chip memory") < 0x2000) {
		ErrorAlert(STR_NO_PREPARE_EMUL_ERR);
		QuitEmulator();
	}

	// Open timer.device
	timereq = (struct timerequest *)AllocVec(sizeof(timerequest), MEMF_PUBLIC | MEMF_CLEAR);
	if (timereq == NULL) {
		ErrorAlert(STR_NO_MEM_ERR);
		QuitEmulator();
	}
	if (OpenDevice((UBYTE *) TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timereq, 0)) {
		ErrorAlert(STR_NO_TIMER_DEV_ERR);
		QuitEmulator();
	}
	TimerBase = (struct Library *)timereq->tr_node.io_Device;

	// Allocate scratch memory
	ScratchMem = (uint8 *)AllocMem(SCRATCH_MEM_SIZE, MEMF_PUBLIC);
	if (ScratchMem == NULL) {
		ErrorAlert(STR_NO_MEM_ERR);
		QuitEmulator();
	}
	ScratchMem += SCRATCH_MEM_SIZE/2;	// ScratchMem points to middle of block

	// Create area for Mac RAM and ROM (ROM must be higher in memory,
	// so we allocate one big chunk and put the ROM at the top of it)
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}
	RAMBaseHost = (uint8 *)AllocVec(RAMSize + 0x100000, MEMF_PUBLIC);
	if (RAMBaseHost == NULL) {
		uint32 newRAMSize = AvailMem(MEMF_LARGEST) - 0x100000;
		char xText[120];

		sprintf(xText, GetString(STR_NOT_ENOUGH_MEM_WARN), RAMSize, newRAMSize);

		if (ChoiceAlert(xText, "Use", "Quit") != 1)
			QuitEmulator();

		RAMSize = newRAMSize;
		RAMBaseHost = (uint8 *)AllocVec(RAMSize - 0x100000, MEMF_PUBLIC);
		if (RAMBaseHost == NULL) {
			ErrorAlert(STR_NO_MEM_ERR);
			QuitEmulator();
		}
	}
	RAMBaseMac = (uint32)RAMBaseHost;
	D(bug("Mac RAM starts at %08lx\n", RAMBaseHost));
	ROMBaseHost = RAMBaseHost + RAMSize;
	ROMBaseMac = (uint32)ROMBaseHost;
	D(bug("Mac ROM starts at %08lx\n", ROMBaseHost));

	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Load Mac ROM
	BPTR rom_fh = Open(rom_path ? (char *)rom_path : (char *)ROM_FILE_NAME, MODE_OLDFILE);
	if (rom_fh == 0) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		QuitEmulator();
	}
	printf(GetString(STR_READING_ROM_FILE));
	Seek(rom_fh, 0, OFFSET_END);
	ROMSize = Seek(rom_fh, 0, OFFSET_CURRENT);
	if (ROMSize != 512*1024 && ROMSize != 1024*1024) {
		ErrorAlert(STR_ROM_SIZE_ERR);
		Close(rom_fh);
		QuitEmulator();
	}
	Seek(rom_fh, 0, OFFSET_BEGINNING);
	if (Read(rom_fh, ROMBaseHost, ROMSize) != ROMSize) {
		ErrorAlert(STR_ROM_FILE_READ_ERR);
		Close(rom_fh);
		QuitEmulator();
	}

	// Set CPU and FPU type
	UWORD attn = SysBase->AttnFlags;
	CPUType = attn & AFF_68040 ? 4 : (attn & AFF_68030 ? 3 : 2);
	CPUIs68060 = attn & AFF_68060;
	FPUType = attn & AFF_68881 ? 1 : 0;

	// Initialize everything
	if (!InitAll(NULL))
		QuitEmulator();

	// Move VBR away from 0 if neccessary
	MoveVBR();

	// On 68060, disable Super Bypass mode because of a CPU bug that is triggered by MacOS 8
	if (CPUIs68060)
		DisableSuperBypass();

	memset((UBYTE *) 8, 0, 0x2000-8);

	// Install trap handler
	EmulatedSR = 0x2700;
	OldTrapHandler = MainTask->tc_TrapCode;
	MainTask->tc_TrapCode = (APTR)TrapHandlerAsm;

	// Allocate signal for interrupt emulation and install exception handler
	IRQSig = AllocSignal(-1);
	IRQSigMask = 1 << IRQSig;
	OldExceptionHandler = MainTask->tc_ExceptCode;
	MainTask->tc_ExceptCode = (APTR)ExceptionHandlerAsm;
	SetExcept(SIGBREAKF_CTRL_C | IRQSigMask, SIGBREAKF_CTRL_C | IRQSigMask);

	// Start XPRAM watchdog process
	xpram_proc = CreateNewProcTags(
		NP_Entry, (ULONG)xpram_func,
		NP_Name, (ULONG)"Basilisk II XPRAM Watchdog",
		NP_Priority, 0,
		TAG_END
	);

	// Start 60Hz process
	tick_proc = CreateNewProcTags(
		NP_Entry, (ULONG)tick_func,
		NP_Name, (ULONG)"Basilisk II 60Hz",
		NP_Priority, 5,
		TAG_END
	);

	// Set task priority to -1 so we don't use all processing time
	SetTaskPri(MainTask, -1);

	WriteMacInt32(0xbff, 0);	// MacsBugFlags

	// Swap stack to Mac RAM area
	stack_swap.stk_Lower = RAMBaseHost;
	stack_swap.stk_Upper = (ULONG)RAMBaseHost + RAMSize;
	stack_swap.stk_Pointer = RAMBaseHost + 0x8000;
	StackSwap(&stack_swap);
	stack_swapped = true;

	// Jump to ROM boot routine
	Start680x0();

	QuitEmulator();
	return 0;
}

void Start680x0(void)
{
	typedef void (*rom_func)(void);
	rom_func fp = (rom_func)(ROMBaseHost + 0x2a);
	fp();
}


/*
 *  Quit emulator (__saveds because it might be called from an exception)
 */

// Assembly entry point
void __saveds quit_emulator(void)
{
	QuitEmulator();
}

void QuitEmulator(void)
{
	// Stop 60Hz process
	if (tick_proc) {
		SetSignal(0, SIGF_SINGLE);
		tick_proc_active = false;
		Wait(SIGF_SINGLE);
	}

	// Stop XPRAM watchdog process
	if (xpram_proc) {
		SetSignal(0, SIGF_SINGLE);
		xpram_proc_active = false;
		Wait(SIGF_SINGLE);
	}

	// Restore stack
	if (stack_swapped) {
		stack_swapped = false;
		StackSwap(&stack_swap);
	}

	// Remove exception handler
	if (IRQSig >= 0) {
		SetExcept(0, SIGBREAKF_CTRL_C | IRQSigMask);
		MainTask->tc_ExceptCode = OldExceptionHandler;
		FreeSignal(IRQSig);
	}

	// Remove trap handler
	MainTask->tc_TrapCode = OldTrapHandler;

	// Deinitialize everything
	ExitAll();

	// Delete RAM/ROM area
	if (RAMBaseHost)
		FreeVec(RAMBaseHost);

	// Delete scratch memory area
	if (ScratchMem)
		FreeMem((void *)(ScratchMem - SCRATCH_MEM_SIZE/2), SCRATCH_MEM_SIZE);

	// Close timer.device
	if (TimerBase)
		CloseDevice((struct IORequest *)timereq);
	if (timereq)
		FreeVec(timereq);

	// Exit system routines
	SysExit();

	// Close AHI
	if (AHIBase)
		CloseDevice((struct IORequest *)ahi_io);
	if (ahi_io)
		DeleteIORequest((struct IORequest *)ahi_io);
	if (ahi_port)
		DeleteMsgPort(ahi_port);

	// Exit preferences
	PrefsExit();

	// Close libraries
	if (CyberGfxBase)
		CloseLibrary(CyberGfxBase);
	if (P96Base)
		CloseLibrary(P96Base);
	if (AslBase)
		CloseLibrary(AslBase);
	if (IFFParseBase)
		CloseLibrary(IFFParseBase);
	if (GadToolsBase)
		CloseLibrary(GadToolsBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);
	if (GfxBase)
		CloseLibrary(GfxBase);

	exit(0);
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

void FlushCodeCache(void *start, uint32 size)
{
	CacheClearE(start, size, CACRF_ClearI | CACRF_ClearD);
}


/*
 *  Mutexes
 */

struct B2_mutex {
	int dummy;	//!!
};

B2_mutex *B2_create_mutex(void)
{
	return new B2_mutex;
}

void B2_lock_mutex(B2_mutex *mutex)
{
}

void B2_unlock_mutex(B2_mutex *mutex)
{
}

void B2_delete_mutex(B2_mutex *mutex)
{
	delete mutex;
}


/*
 *  Interrupt flags (must be handled atomically!)
 */

uint32 InterruptFlags;

void SetInterruptFlag(uint32 flag)
{
	AtomicOr(&InterruptFlags, flag);
}

void ClearInterruptFlag(uint32 flag)
{
	AtomicAnd(&InterruptFlags, ~flag);
}

void TriggerInterrupt(void)
{
	Signal(MainTask, IRQSigMask);
}

void TriggerNMI(void)
{
	AsmTriggerNMI();
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static __saveds void tick_func(void)
{
	int tick_counter = 0;
	struct MsgPort *timer_port = NULL;
	struct timerequest *timer_io = NULL;
	ULONG timer_mask = 0;

	// Start 60Hz timer
	timer_port = CreateMsgPort();
	if (timer_port) {
		timer_io = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
		if (timer_io) {
			if (!OpenDevice((UBYTE *) TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timer_io, 0)) {
				timer_mask = 1 << timer_port->mp_SigBit;
				timer_io->tr_node.io_Command = TR_ADDREQUEST;
				timer_io->tr_time.tv_secs = 0;
				timer_io->tr_time.tv_micro = 16625;
				SendIO((struct IORequest *)timer_io);
			}
		}
	}

	while (tick_proc_active) {

		// Wait for timer tick
		Wait(timer_mask);

		// Restart timer
		timer_io->tr_node.io_Command = TR_ADDREQUEST;
		timer_io->tr_time.tv_secs = 0;
		timer_io->tr_time.tv_micro = 16625;
		SendIO((struct IORequest *)timer_io);

		// Pseudo Mac 1Hz interrupt, update local time
		if (++tick_counter > 60) {
			tick_counter = 0;
			WriteMacInt32(0x20c, TimerDateTime());
			SetInterruptFlag(INTFLAG_1HZ);
			TriggerInterrupt();
		}

		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}

	// Stop timer
	if (timer_io) {
		if (!CheckIO((struct IORequest *)timer_io))
			AbortIO((struct IORequest *)timer_io);
		WaitIO((struct IORequest *)timer_io);
		CloseDevice((struct IORequest *)timer_io);
		DeleteIORequest(timer_io);
	}
	if (timer_port)
		DeleteMsgPort(timer_port);

	// Main task asked for termination, send signal
	Forbid();
	Signal(MainTask, SIGF_SINGLE);
}


/*
 *  XPRAM watchdog thread (saves XPRAM every minute)
 */

static __saveds void xpram_func(void)
{
	uint8 last_xpram[XPRAM_SIZE];
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);

	while (xpram_proc_active) {
		for (int i=0; i<60 && xpram_proc_active; i++)
			Delay(50);		// Only wait 1 second so we quit promptly when xpram_proc_active becomes false
		if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
			memcpy(last_xpram, XPRAM, XPRAM_SIZE);
			SaveXPRAM();
		}
	}

	// Main task asked for termination, send signal
	Forbid();
	Signal(MainTask, SIGF_SINGLE);
}


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	if (PrefsFindBool("nogui")) {
		printf(GetString(STR_SHELL_ERROR_PREFIX), text);
		return;
	}
	EasyStruct req;
	req.es_StructSize = sizeof(EasyStruct);
	req.es_Flags = 0;
	req.es_Title = (UBYTE *)GetString(STR_ERROR_ALERT_TITLE);
	req.es_TextFormat = (UBYTE *)GetString(STR_GUI_ERROR_PREFIX);
	req.es_GadgetFormat = (UBYTE *)GetString(STR_QUIT_BUTTON);
	EasyRequest(NULL, &req, NULL, (ULONG)text);
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
	if (PrefsFindBool("nogui")) {
		printf(GetString(STR_SHELL_WARNING_PREFIX), text);
		return;
	}
	EasyStruct req;
	req.es_StructSize = sizeof(EasyStruct);
	req.es_Flags = 0;
	req.es_Title = (UBYTE *)GetString(STR_WARNING_ALERT_TITLE);
	req.es_TextFormat = (UBYTE *)GetString(STR_GUI_WARNING_PREFIX);
	req.es_GadgetFormat = (UBYTE *)GetString(STR_OK_BUTTON);
	EasyRequest(NULL, &req, NULL, (ULONG)text);
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	char str[256];
	sprintf(str, "%s|%s", pos, neg);
	EasyStruct req;
	req.es_StructSize = sizeof(EasyStruct);
	req.es_Flags = 0;
	req.es_Title = (UBYTE *)GetString(STR_WARNING_ALERT_TITLE);
	req.es_TextFormat = (UBYTE *)GetString(STR_GUI_WARNING_PREFIX);
	req.es_GadgetFormat = (UBYTE *)str;
	return EasyRequest(NULL, &req, NULL, (ULONG)text);
}


/*
 *  Illegal Instruction and Privilege Violation trap handlers
 */

struct trap_regs {	// This must match the layout of M68kRegisters
	uint32 d[8];
	uint32 a[8];
	uint16 sr;
	uint32 pc;
};

void __saveds IllInstrHandler(trap_regs *r)
{
//	D(bug("IllInstrHandler/%ld\n", __LINE__));

	uint16 opcode = *(uint16 *)(r->pc);
	if ((opcode & 0xff00) != 0x7100) {
		printf("Illegal Instruction %04x at %08lx\n", *(uint16 *)(r->pc), r->pc);
		printf("d0 %08lx d1 %08lx d2 %08lx d3 %08lx\n"
			   "d4 %08lx d5 %08lx d6 %08lx d7 %08lx\n"
			   "a0 %08lx a1 %08lx a2 %08lx a3 %08lx\n"
			   "a4 %08lx a5 %08lx a6 %08lx a7 %08lx\n"
			   "sr %04x\n",
			   r->d[0], r->d[1], r->d[2], r->d[3], r->d[4], r->d[5], r->d[6], r->d[7],
			   r->a[0], r->a[1], r->a[2], r->a[3], r->a[4], r->a[5], r->a[6], r->a[7],
			   r->sr);
		QuitEmulator();
	} else {
		// Disable interrupts
		uint16 sr = EmulatedSR;
		EmulatedSR |= 0x0700;

		// Call opcode routine
		EmulOp(opcode, (M68kRegisters *)r);
		r->pc += 2;

		// Restore interrupts
		EmulatedSR = sr;
		if ((EmulatedSR & 0x0700) == 0 && InterruptFlags)
			Signal(MainTask, IRQSigMask);
	}
}

void __saveds PrivViolHandler(trap_regs *r)
{
	printf("Privileged instruction %04x %04x at %08lx\n", *(uint16 *)(r->pc), *(uint16 *)(r->pc + 2), r->pc);
	printf("d0 %08lx d1 %08lx d2 %08lx d3 %08lx\n"
		   "d4 %08lx d5 %08lx d6 %08lx d7 %08lx\n"
		   "a0 %08lx a1 %08lx a2 %08lx a3 %08lx\n"
		   "a4 %08lx a5 %08lx a6 %08lx a7 %08lx\n"
		   "sr %04x\n",
		   r->d[0], r->d[1], r->d[2], r->d[3], r->d[4], r->d[5], r->d[6], r->d[7],
		   r->a[0], r->a[1], r->a[2], r->a[3], r->a[4], r->a[5], r->a[6], r->a[7],
		   r->sr);
	QuitEmulator();
}
