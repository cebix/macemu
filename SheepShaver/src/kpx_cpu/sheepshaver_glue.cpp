/*
 *  sheepshaver_glue.cpp - Glue Kheperix CPU to SheepShaver CPU engine interface
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

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "xlowmem.h"
#include "emul_op.h"
#include "rom_patches.h"
#include "macos_util.h"
#include "block-alloc.hpp"
#include "sigsegv.h"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-operations.hpp"

// Used for NativeOp trampolines
#include "video.h"
#include "name_registry.h"
#include "serial.h"

#include <stdio.h>

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

#define DEBUG 1
#include "debug.h"

static void enter_mon(void)
{
	// Start up mon in real-mode
#if ENABLE_MON
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
#endif
}

// Enable Execute68k() safety checks?
#define SAFE_EXEC_68K 1

// Save FP state in Execute68k()?
#define SAVE_FP_EXEC_68K 1

// Interrupts in EMUL_OP mode?
#define INTERRUPTS_IN_EMUL_OP_MODE 1

// Interrupts in native mode?
#define INTERRUPTS_IN_NATIVE_MODE 1

// 68k Emulator Data
struct EmulatorData {
	uint32	v[0x400];	
};

// Kernel Data
struct KernelData {
	uint32	v[0x400];
	EmulatorData ed;
};

// Pointer to Kernel Data
static KernelData * const kernel_data = (KernelData *)0x68ffe000;


/**
 *		PowerPC emulator glue with special 'sheep' opcodes
 **/

struct sheepshaver_exec_return { };

class sheepshaver_cpu
	: public powerpc_cpu
{
	void init_decoder();
	void execute_sheep(uint32 opcode);

public:

	sheepshaver_cpu()
		: powerpc_cpu()
		{ init_decoder(); }

	// Stack pointer accessors
	uint32 get_sp() const		{ return gpr(1); }
	void set_sp(uint32 v)		{ gpr(1) = v; }

	// Condition Register accessors
	uint32 get_cr() const		{ return cr().get(); }
	void set_cr(uint32 v)		{ cr().set(v); }

	// Execution loop
	void execute(uint32 pc);

	// Execute 68k routine
	void execute_68k(uint32 entry, M68kRegisters *r);

	// Execute MacOS/PPC code
	uint32 execute_macos_code(uint32 tvect, int nargs, uint32 const *args);

	// Resource manager thunk
	void get_resource(uint32 old_get_resource);

	// Handle MacOS interrupt
	void interrupt(uint32 entry, uint32 sp);

	// Lazy memory allocator (one item at a time)
	void *operator new(size_t size)
		{ return allocator_helper< sheepshaver_cpu, lazy_allocator >::allocate(); }
	void operator delete(void *p)
		{ allocator_helper< sheepshaver_cpu, lazy_allocator >::deallocate(p); }
	// FIXME: really make surre array allocation fail at link time?
	void *operator new[](size_t);
	void operator delete[](void *p);
};

lazy_allocator< sheepshaver_cpu > allocator_helper< sheepshaver_cpu, lazy_allocator >::allocator;

void sheepshaver_cpu::init_decoder()
{
#ifndef PPC_NO_STATIC_II_INDEX_TABLE
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;
#endif

	static const instr_info_t sheep_ii_table[] = {
		{ "sheep",
		  (execute_fn)&sheepshaver_cpu::execute_sheep,
		  NULL,
		  D_form, 6, 0, CFLOW_TRAP
		}
	};

	const int ii_count = sizeof(sheep_ii_table)/sizeof(sheep_ii_table[0]);
	D(bug("SheepShaver extra decode table has %d entries\n", ii_count));

	for (int i = 0; i < ii_count; i++) {
		const instr_info_t * ii = &sheep_ii_table[i];
		init_decoder_entry(ii);
	}
}

// Forward declaration for native opcode handler
static void NativeOp(int selector);

// Execute SheepShaver instruction
void sheepshaver_cpu::execute_sheep(uint32 opcode)
{
//	D(bug("Extended opcode %08x at %08x (68k pc %08x)\n", opcode, pc(), gpr(24)));
	assert((((opcode >> 26) & 0x3f) == 6) && OP_MAX <= 64 + 3);

	switch (opcode & 0x3f) {
	case 0:		// EMUL_RETURN
		QuitEmulator();
		break;
		
	case 1:		// EXEC_RETURN
		throw sheepshaver_exec_return();
		break;

	case 2:		// EXEC_NATIVE
		NativeOp((opcode >> 6) & 0x1f);
		pc() = lr();
		break;

	default: {	// EMUL_OP
		M68kRegisters r68;
		WriteMacInt32(XLM_68K_R25, gpr(25));
		WriteMacInt32(XLM_RUN_MODE, MODE_EMUL_OP);
		for (int i = 0; i < 8; i++)
			r68.d[i] = gpr(8 + i);
		for (int i = 0; i < 7; i++)
			r68.a[i] = gpr(16 + i);
		r68.a[7] = gpr(1);
		EmulOp(&r68, gpr(24), (opcode & 0x3f) - 3);
		for (int i = 0; i < 8; i++)
			gpr(8 + i) = r68.d[i];
		for (int i = 0; i < 7; i++)
			gpr(16 + i) = r68.a[i];
		gpr(1) = r68.a[7];
		WriteMacInt32(XLM_RUN_MODE, MODE_68K);
		pc() += 4;
		break;
	}
	}
}

// Execution loop
void sheepshaver_cpu::execute(uint32 entry)
{
	try {
		pc() = entry;
		powerpc_cpu::execute();
	}
	catch (sheepshaver_exec_return const &) {
		// Nothing, simply return
	}
	catch (...) {
		printf("ERROR: execute() received an unknown exception!\n");
		QuitEmulator();
	}
}

// Handle MacOS interrupt
void sheepshaver_cpu::interrupt(uint32 entry, uint32 sp)
{
	// Create stack frame
	gpr(1) = sp - 64;

	// Build trampoline to return from interrupt
	uint32 trampoline[] = { POWERPC_EMUL_OP | 1 };

	// Prepare registers for nanokernel interrupt routine
	kernel_data->v[0x004 >> 2] = gpr(1);
	kernel_data->v[0x018 >> 2] = gpr(6);

	gpr(6) = kernel_data->v[0x65c >> 2];
	WriteMacInt32(gpr(6) + 0x13c, gpr(7));
	WriteMacInt32(gpr(6) + 0x144, gpr(8));
	WriteMacInt32(gpr(6) + 0x14c, gpr(9));
	WriteMacInt32(gpr(6) + 0x154, gpr(10));
	WriteMacInt32(gpr(6) + 0x15c, gpr(11));
	WriteMacInt32(gpr(6) + 0x164, gpr(12));
	WriteMacInt32(gpr(6) + 0x16c, gpr(13));

	gpr(1)  = KernelDataAddr;
	gpr(7)  = kernel_data->v[0x660 >> 2];
	gpr(8)  = 0;
	gpr(10) = (uint32)trampoline;
	gpr(12) = (uint32)trampoline;
	gpr(13) = cr().get();

	// rlwimi. r7,r7,8,0,0
	uint32 result = op_ppc_rlwimi::apply(gpr(7), 8, 0x80000000, gpr(7));
	record_cr0(result);
	gpr(7) = result;

	gpr(11) = 0xf072; // MSR (SRR1)
	cr().set((gpr(11) & 0x0fff0000) | (cr().get() & ~0x0fff0000));

	// Enter nanokernel
	execute(entry);

	// Cleanup stack
	gpr(1) += 64;
}

// Execute 68k routine
void sheepshaver_cpu::execute_68k(uint32 entry, M68kRegisters *r)
{
#if SAFE_EXEC_68K
	if (ReadMacInt32(XLM_RUN_MODE) != MODE_EMUL_OP)
		printf("FATAL: Execute68k() not called from EMUL_OP mode\n");
#endif

	// Save program counters and branch registers
	uint32 saved_pc = pc();
	uint32 saved_lr = lr();
	uint32 saved_ctr= ctr();

	// Create MacOS stack frame
	uint32 sp = gpr(1);
	gpr(1) -= 56 + 19*4 + 18*8;
	WriteMacInt32(gpr(1), sp);

	// Save PowerPC registers
	memcpy(Mac2HostAddr(gpr(1)+56), &gpr(13), sizeof(uint32)*(32-13));
#if SAVE_FP_EXEC_68K
	memcpy(Mac2HostAddr(gpr(1)+56+19*4), &fpr(14), sizeof(double)*(32-14));
#endif

	// Setup registers for 68k emulator
	cr().set(0);
	cr().set(2, 1);								// Supervisor mode
	for (int i = 0; i < 8; i++)					// d[0]..d[7]
	  gpr(8 + i) = r->d[i];
	for (int i = 0; i < 7; i++)					// a[0]..a[6]
	  gpr(16 + i) = r->a[i];
	gpr(23) = 0;
	gpr(24) = entry;
	gpr(25) = ReadMacInt32(XLM_68K_R25);		// MSB of SR
	gpr(26) = 0;
	gpr(28) = 0;								// VBR
	gpr(29) = kernel_data->ed.v[0x74 >> 2];		// Pointer to opcode table
	gpr(30) = kernel_data->ed.v[0x78 >> 2];		// Address of emulator
	gpr(31) = KernelDataAddr + 0x1000;

	// Push return address (points to EXEC_RETURN opcode) on stack
	gpr(1) -= 4;
	WriteMacInt32(gpr(1), XLM_EXEC_RETURN_OPCODE);
	
	// Rentering 68k emulator
	WriteMacInt32(XLM_RUN_MODE, MODE_68K);

	// Set r0 to 0 for 68k emulator
	gpr(0) = 0;

	// Execute 68k opcode
	uint32 opcode = ReadMacInt16(gpr(24));
	gpr(27) = (int32)(int16)ReadMacInt16(gpr(24) += 2);
	gpr(29) += opcode * 8;
	execute(gpr(29));

	// Save r25 (contains current 68k interrupt level)
	WriteMacInt32(XLM_68K_R25, gpr(25));

	// Reentering EMUL_OP mode
	WriteMacInt32(XLM_RUN_MODE, MODE_EMUL_OP);

	// Save 68k registers
	for (int i = 0; i < 8; i++)					// d[0]..d[7]
	  r->d[i] = gpr(8 + i);
	for (int i = 0; i < 7; i++)					// a[0]..a[6]
	  r->a[i] = gpr(16 + i);

	// Restore PowerPC registers
	memcpy(&gpr(13), Mac2HostAddr(gpr(1)+56), sizeof(uint32)*(32-13));
#if SAVE_FP_EXEC_68K
	memcpy(&fpr(14), Mac2HostAddr(gpr(1)+56+19*4), sizeof(double)*(32-14));
#endif

	// Cleanup stack
	gpr(1) += 56 + 19*4 + 18*8;

	// Restore program counters and branch registers
	pc() = saved_pc;
	lr() = saved_lr;
	ctr()= saved_ctr;
}

// Call MacOS PPC code
uint32 sheepshaver_cpu::execute_macos_code(uint32 tvect, int nargs, uint32 const *args)
{
	// Save program counters and branch registers
	uint32 saved_pc = pc();
	uint32 saved_lr = lr();
	uint32 saved_ctr= ctr();

	// Build trampoline with EXEC_RETURN
	uint32 trampoline[] = { POWERPC_EMUL_OP | 1 };
	lr() = (uint32)trampoline;

	gpr(1) -= 64;								// Create stack frame
	uint32 proc = ReadMacInt32(tvect);			// Get routine address
	uint32 toc = ReadMacInt32(tvect + 4);		// Get TOC pointer

	// Save PowerPC registers
	uint32 regs[8];
	regs[0] = gpr(2);
	for (int i = 0; i < nargs; i++)
		regs[i + 1] = gpr(i + 3);

	// Prepare and call MacOS routine
	gpr(2) = toc;
	for (int i = 0; i < nargs; i++)
		gpr(i + 3) = args[i];
	execute(proc);
	uint32 retval = gpr(3);

	// Restore PowerPC registers
	for (int i = 0; i <= nargs; i++)
		gpr(i + 2) = regs[i];

	// Cleanup stack
	gpr(1) += 64;

	// Restore program counters and branch registers
	pc() = saved_pc;
	lr() = saved_lr;
	ctr()= saved_ctr;

	return retval;
}

// Resource Manager thunk
inline void sheepshaver_cpu::get_resource(uint32 old_get_resource)
{
	printf("ERROR: get_resource() unimplemented\n");
	QuitEmulator();
}


/**
 *		SheepShaver CPU engine interface
 **/

static sheepshaver_cpu *main_cpu = NULL;		// CPU emulator to handle usual control flow
static sheepshaver_cpu *interrupt_cpu = NULL;	// CPU emulator to handle interrupts
static sheepshaver_cpu *current_cpu = NULL;		// Current CPU emulator context

// Dump PPC registers
static void dump_registers(void)
{
	current_cpu->dump_registers();
}

// Dump log
static void dump_log(void)
{
	current_cpu->dump_log();
}

/*
 *  Initialize CPU emulation
 */

static struct sigaction sigsegv_action;

#if defined(__powerpc__)
#include <sys/ucontext.h>
#endif

static void sigsegv_handler(int sig, siginfo_t *sip, void *scp)
{
	const uintptr addr = (uintptr)sip->si_addr;
#if ENABLE_VOSF
	// Handle screen fault.
	extern bool Screen_fault_handler(sigsegv_address_t fault_address, sigsegv_address_t fault_instruction);
	if (Screen_fault_handler((sigsegv_address_t)addr, SIGSEGV_INVALID_PC))
		return;
#endif
#if defined(__powerpc__)
	if (addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE) {
		printf("IGNORE write access to ROM at %08x\n", addr);
		(((ucontext_t *)scp)->uc_mcontext.regs)->nip += 4;
		return;
	}
	if (addr >= 0xf3012000 && addr < 0xf3014000 && 0) {
		printf("IGNORE write access to ROM at %08x\n", addr);
		(((ucontext_t *)scp)->uc_mcontext.regs)->nip += 4;
		return;
	}
#endif
	printf("Caught SIGSEGV at address %p\n", sip->si_addr);
	printf("Native PC: %08x\n", (((ucontext_t *)scp)->uc_mcontext.regs)->nip);
	printf("Current CPU: %s\n", current_cpu == main_cpu ? "main" : "interrupts");
#if 1
	dump_registers();
#else
	printf("Main CPU context\n");
	main_cpu->dump_registers();
	printf("Interrupts CPU context\n");
	interrupt_cpu->dump_registers();
#endif
	current_cpu->dump_log();
	WriteMacInt32(XLM_IRQ_NEST, 1);
	enter_mon();
	QuitEmulator();
}

void init_emul_ppc(void)
{
	// Initialize main CPU emulator
	main_cpu = new sheepshaver_cpu();
	main_cpu->set_register(powerpc_registers::GPR(3), any_register((uint32)ROM_BASE + 0x30d000));
	WriteMacInt32(XLM_RUN_MODE, MODE_68K);

	// Initialize alternate CPU emulator to handle interrupts
	interrupt_cpu = new sheepshaver_cpu();

	// Install SIGSEGV handler
	sigemptyset(&sigsegv_action.sa_mask);
	sigsegv_action.sa_sigaction = sigsegv_handler;
	sigsegv_action.sa_flags = SA_SIGINFO;
	sigsegv_action.sa_restorer = NULL;
	sigaction(SIGSEGV, &sigsegv_action, NULL);

#if ENABLE_MON
	// Install "regs" command in cxmon
	mon_add_command("regs", dump_registers, "regs                     Dump PowerPC registers\n");
	mon_add_command("log", dump_log, "log                      Dump PowerPC emulation log\n");
#endif
}

/*
 *  Emulation loop
 */

void emul_ppc(uint32 entry)
{
	current_cpu = main_cpu;
	current_cpu->start_log();
	current_cpu->execute(entry);
}

/*
 *  Handle PowerPC interrupt
 */

// Atomic operations
extern int atomic_add(int *var, int v);
extern int atomic_and(int *var, int v);
extern int atomic_or(int *var, int v);

void HandleInterrupt(void)
{
	// Do nothing if interrupts are disabled
	if (ReadMacInt32(XLM_IRQ_NEST) > 0 || InterruptFlags == 0)
		return;

	// Do nothing if CPU objects are not initialized yet
	if (current_cpu == NULL)
		return;

	// Disable MacOS stack sniffer
	WriteMacInt32(0x110, 0);

	// Interrupt action depends on current run mode
	switch (ReadMacInt32(XLM_RUN_MODE)) {
	case MODE_68K:
		// 68k emulator active, trigger 68k interrupt level 1
		assert(current_cpu == main_cpu);
		WriteMacInt16(tswap32(kernel_data->v[0x67c >> 2]), 1);
		main_cpu->set_cr(main_cpu->get_cr() | tswap32(kernel_data->v[0x674 >> 2]));
		break;
    
#if INTERRUPTS_IN_NATIVE_MODE
	case MODE_NATIVE:
		// 68k emulator inactive, in nanokernel?
		assert(current_cpu == main_cpu);
		if (main_cpu->gpr(1) != KernelDataAddr) {
			// Prepare for 68k interrupt level 1
			WriteMacInt16(tswap32(kernel_data->v[0x67c >> 2]), 1);
			WriteMacInt32(tswap32(kernel_data->v[0x658 >> 2]) + 0xdc,
						  ReadMacInt32(tswap32(kernel_data->v[0x658 >> 2]) + 0xdc)
						  | tswap32(kernel_data->v[0x674 >> 2]));
      
			// Execute nanokernel interrupt routine (this will activate the 68k emulator)
			atomic_add((int32 *)XLM_IRQ_NEST, htonl(1));
			current_cpu = interrupt_cpu;
			if (ROMType == ROMTYPE_NEWWORLD)
				current_cpu->interrupt(ROM_BASE + 0x312b1c, main_cpu->get_sp());
			else
				current_cpu->interrupt(ROM_BASE + 0x312a3c, main_cpu->get_sp());
			current_cpu = main_cpu;
		}
		break;
#endif
    
#if INTERRUPTS_IN_EMUL_OP_MODE
	case MODE_EMUL_OP:
		// 68k emulator active, within EMUL_OP routine, execute 68k interrupt routine directly when interrupt level is 0
		if ((ReadMacInt32(XLM_68K_R25) & 7) == 0) {
#if 1
			// Execute full 68k interrupt routine
			M68kRegisters r;
			uint32 old_r25 = ReadMacInt32(XLM_68K_R25);	// Save interrupt level
			WriteMacInt32(XLM_68K_R25, 0x21);			// Execute with interrupt level 1
			static const uint16 proc[] = {
				0x3f3c, 0x0000,		// move.w	#$0000,-(sp)	(fake format word)
				0x487a, 0x000a,		// pea		@1(pc)			(return address)
				0x40e7,				// move		sr,-(sp)		(saved SR)
				0x2078, 0x0064,		// move.l	$64,a0
				0x4ed0,				// jmp		(a0)
				M68K_RTS			// @1
			};
			Execute68k((uint32)proc, &r);
			WriteMacInt32(XLM_68K_R25, old_r25);		// Restore interrupt level
#else
			// Only update cursor
			if (HasMacStarted()) {
				if (InterruptFlags & INTFLAG_VIA) {
					ClearInterruptFlag(INTFLAG_VIA);
					ADBInterrupt();
					ExecutePPC(VideoVBL);
				}
			}
#endif
		}
		break;
#endif
	}
}

/*
 *  Execute NATIVE_OP opcode (called by PowerPC emulator)
 */

#define POWERPC_NATIVE_OP(selector) \
		{ tswap32(POWERPC_EMUL_OP | 2 | (((uint32)selector) << 6)) }

// FIXME: Make sure 32-bit relocations are used
const uint32 NativeOpTable[NATIVE_OP_MAX] = {
	POWERPC_NATIVE_OP(NATIVE_PATCH_NAME_REGISTRY),
	POWERPC_NATIVE_OP(NATIVE_VIDEO_INSTALL_ACCEL),
	POWERPC_NATIVE_OP(NATIVE_VIDEO_VBL),
	POWERPC_NATIVE_OP(NATIVE_VIDEO_DO_DRIVER_IO),
	POWERPC_NATIVE_OP(NATIVE_ETHER_IRQ),
	POWERPC_NATIVE_OP(NATIVE_ETHER_INIT),
	POWERPC_NATIVE_OP(NATIVE_ETHER_TERM),
	POWERPC_NATIVE_OP(NATIVE_ETHER_OPEN),
	POWERPC_NATIVE_OP(NATIVE_ETHER_CLOSE),
	POWERPC_NATIVE_OP(NATIVE_ETHER_WPUT),
	POWERPC_NATIVE_OP(NATIVE_ETHER_RSRV),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_NOTHING),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_OPEN),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_PRIME_IN),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_PRIME_OUT),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_CONTROL),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_STATUS),
	POWERPC_NATIVE_OP(NATIVE_SERIAL_CLOSE),
	POWERPC_NATIVE_OP(NATIVE_GET_RESOURCE),
	POWERPC_NATIVE_OP(NATIVE_GET_1_RESOURCE),
	POWERPC_NATIVE_OP(NATIVE_GET_IND_RESOURCE),
	POWERPC_NATIVE_OP(NATIVE_GET_1_IND_RESOURCE),
	POWERPC_NATIVE_OP(NATIVE_R_GET_RESOURCE),
};

static void get_resource(void);
static void get_1_resource(void);
static void get_ind_resource(void);
static void get_1_ind_resource(void);
static void r_get_resource(void);

#define GPR(REG) current_cpu->gpr(REG)

static void NativeOp(int selector)
{
	switch (selector) {
	case NATIVE_PATCH_NAME_REGISTRY:
		DoPatchNameRegistry();
		break;
	case NATIVE_VIDEO_INSTALL_ACCEL:
		VideoInstallAccel();
		break;
	case NATIVE_VIDEO_VBL:
		VideoVBL();
		break;
	case NATIVE_VIDEO_DO_DRIVER_IO:
		GPR(3) = (int32)(int16)VideoDoDriverIO((void *)GPR(3), (void *)GPR(4),
											   (void *)GPR(5), GPR(6), GPR(7));
		break;
	case NATIVE_GET_RESOURCE:
		get_resource();
		break;
	case NATIVE_GET_1_RESOURCE:
		get_1_resource();
		break;
	case NATIVE_GET_IND_RESOURCE:
		get_ind_resource();
		break;
	case NATIVE_GET_1_IND_RESOURCE:
		get_1_ind_resource();
		break;
	case NATIVE_R_GET_RESOURCE:
		r_get_resource();
		break;
	case NATIVE_SERIAL_NOTHING:
	case NATIVE_SERIAL_OPEN:
	case NATIVE_SERIAL_PRIME_IN:
	case NATIVE_SERIAL_PRIME_OUT:
	case NATIVE_SERIAL_CONTROL:
	case NATIVE_SERIAL_STATUS:
	case NATIVE_SERIAL_CLOSE: {
		typedef int16 (*SerialCallback)(uint32, uint32);
		static const SerialCallback serial_callbacks[] = {
			SerialNothing,
			SerialOpen,
			SerialPrimeIn,
			SerialPrimeOut,
			SerialControl,
			SerialStatus,
			SerialClose
		};
		GPR(3) = serial_callbacks[selector - NATIVE_SERIAL_NOTHING](GPR(3), GPR(4));
		break;
	}
	default:
		printf("FATAL: NATIVE_OP called with bogus selector %d\n", selector);
		QuitEmulator();
		break;
	}
}

/*
 *  Execute native subroutine (LR must contain return address)
 */

void ExecuteNative(int selector)
{
	uint32 tvect[2];
	tvect[0] = tswap32(POWERPC_NATIVE_OP_FUNC(selector));
	tvect[1] = 0; // Fake TVECT
	RoutineDescriptor desc = BUILD_PPC_ROUTINE_DESCRIPTOR(0, tvect);
	M68kRegisters r;
	Execute68k((uint32)&desc, &r);
}

/*
 *  Execute 68k subroutine (must be ended with EXEC_RETURN)
 *  This must only be called by the emul_thread when in EMUL_OP mode
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68k(uint32 pc, M68kRegisters *r)
{
	current_cpu->execute_68k(pc, r);
}

/*
 *  Execute 68k A-Trap from EMUL_OP routine
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68kTrap(uint16 trap, M68kRegisters *r)
{
	uint16 proc[2] = {trap, M68K_RTS};
	Execute68k((uint32)proc, r);
}

/*
 *  Call MacOS PPC code
 */

uint32 call_macos(uint32 tvect)
{
	return current_cpu->execute_macos_code(tvect, 0, NULL);
}

uint32 call_macos1(uint32 tvect, uint32 arg1)
{
	const uint32 args[] = { arg1 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos2(uint32 tvect, uint32 arg1, uint32 arg2)
{
	const uint32 args[] = { arg1, arg2 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos3(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3)
{
	const uint32 args[] = { arg1, arg2, arg3 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos4(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos5(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos6(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5, arg6 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

uint32 call_macos7(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6, uint32 arg7)
{
	const uint32 args[] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7 };
	return current_cpu->execute_macos_code(tvect, sizeof(args)/sizeof(args[0]), args);
}

/*
 *  Atomic operations
 */

int atomic_add(int *var, int v)
{
	int ret = *var;
	*var += v;
	return ret;
}

int atomic_and(int *var, int v)
{
	int ret = *var;
	*var &= v;
	return ret;
}

int atomic_or(int *var, int v)
{
	int ret = *var;
	*var |= v;
	return ret;
}

/*
 *  Resource Manager thunks
 */

extern "C" void check_load_invoc(uint32 type, int16 id, uint16 **h);

void get_resource(void)
{
	current_cpu->get_resource(ReadMacInt32(XLM_GET_RESOURCE));
}

void get_1_resource(void)
{
	current_cpu->get_resource(ReadMacInt32(XLM_GET_1_RESOURCE));
}

void get_ind_resource(void)
{
	current_cpu->get_resource(ReadMacInt32(XLM_GET_IND_RESOURCE));
}

void get_1_ind_resource(void)
{
	current_cpu->get_resource(ReadMacInt32(XLM_GET_1_IND_RESOURCE));
}

void r_get_resource(void)
{
	current_cpu->get_resource(ReadMacInt32(XLM_R_GET_RESOURCE));
}
