/*
 *  sigsegv.cpp - SIGSEGV signals support
 *
 *  Derived from Bruno Haible's work on his SIGSEGV library for clisp
 *  <http://clisp.sourceforge.net/>
 *
 *  MacOS X support derived from the post by Timothy J. Wood to the
 *  omnigroup macosx-dev list:
 *    Mach Exception Handlers 101 (Was Re: ptrace, gdb)
 *    tjw@omnigroup.com Sun, 4 Jun 2000
 *    www.omnigroup.com/mailman/archive/macosx-dev/2000-June/002030.html
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <list>
#include <signal.h>
#include "sigsegv.h"

#ifndef NO_STD_NAMESPACE
using std::list;
#endif

// Return value type of a signal handler (standard type if not defined)
#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

// Type of the system signal handler
typedef RETSIGTYPE (*signal_handler)(int);

// User's SIGSEGV handler
static sigsegv_fault_handler_t sigsegv_fault_handler = 0;

// Function called to dump state if we can't handle the fault
static sigsegv_state_dumper_t sigsegv_state_dumper = 0;

// Actual SIGSEGV handler installer
static bool sigsegv_do_install_handler(int sig);


/*
 *  Instruction decoding aids
 */

// Transfer size
enum transfer_size_t {
	SIZE_UNKNOWN,
	SIZE_BYTE,
	SIZE_WORD,
	SIZE_LONG
};

// Transfer type
typedef sigsegv_transfer_type_t transfer_type_t;

#if (defined(powerpc) || defined(__powerpc__) || defined(__ppc__))
// Addressing mode
enum addressing_mode_t {
	MODE_UNKNOWN,
	MODE_NORM,
	MODE_U,
	MODE_X,
	MODE_UX
};

// Decoded instruction
struct instruction_t {
	transfer_type_t		transfer_type;
	transfer_size_t		transfer_size;
	addressing_mode_t	addr_mode;
	unsigned int		addr;
	char				ra, rd;
};

static void powerpc_decode_instruction(instruction_t *instruction, unsigned int nip, unsigned int * gpr)
{
	// Get opcode and divide into fields
	unsigned int opcode = *((unsigned int *)nip);
	unsigned int primop = opcode >> 26;
	unsigned int exop = (opcode >> 1) & 0x3ff;
	unsigned int ra = (opcode >> 16) & 0x1f;
	unsigned int rb = (opcode >> 11) & 0x1f;
	unsigned int rd = (opcode >> 21) & 0x1f;
	signed int imm = (signed short)(opcode & 0xffff);
	
	// Analyze opcode
	transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
	transfer_size_t transfer_size = SIZE_UNKNOWN;
	addressing_mode_t addr_mode = MODE_UNKNOWN;
	switch (primop) {
	case 31:
		switch (exop) {
		case 23:	// lwzx
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_LONG; addr_mode = MODE_X; break;
		case 55:	// lwzux
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_LONG; addr_mode = MODE_UX; break;
		case 87:	// lbzx
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
		case 119:	// lbzux
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
		case 151:	// stwx
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_LONG; addr_mode = MODE_X; break;
		case 183:	// stwux
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_LONG; addr_mode = MODE_UX; break;
		case 215:	// stbx
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
		case 247:	// stbux
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
		case 279:	// lhzx
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
		case 311:	// lhzux
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
		case 343:	// lhax
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
		case 375:	// lhaux
			transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
		case 407:	// sthx
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
		case 439:	// sthux
			transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
		}
		break;
	
	case 32:	// lwz
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_LONG; addr_mode = MODE_NORM; break;
	case 33:	// lwzu
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_LONG; addr_mode = MODE_U; break;
	case 34:	// lbz
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
	case 35:	// lbzu
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
	case 36:	// stw
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_LONG; addr_mode = MODE_NORM; break;
	case 37:	// stwu
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_LONG; addr_mode = MODE_U; break;
	case 38:	// stb
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
	case 39:	// stbu
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
	case 40:	// lhz
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
	case 41:	// lhzu
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
	case 42:	// lha
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
	case 43:	// lhau
		transfer_type = SIGSEGV_TRANSFER_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
	case 44:	// sth
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
	case 45:	// sthu
		transfer_type = SIGSEGV_TRANSFER_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
	}
	
	// Calculate effective address
	unsigned int addr = 0;
	switch (addr_mode) {
	case MODE_X:
	case MODE_UX:
		if (ra == 0)
			addr = gpr[rb];
		else
			addr = gpr[ra] + gpr[rb];
		break;
	case MODE_NORM:
	case MODE_U:
		if (ra == 0)
			addr = (signed int)(signed short)imm;
		else
			addr = gpr[ra] + (signed int)(signed short)imm;
		break;
	default:
		break;
	}
        
	// Commit decoded instruction
	instruction->addr = addr;
	instruction->addr_mode = addr_mode;
	instruction->transfer_type = transfer_type;
	instruction->transfer_size = transfer_size;
	instruction->ra = ra;
	instruction->rd = rd;
}
#endif


/*
 *  OS-dependant SIGSEGV signals support section
 */

#if HAVE_SIGINFO_T
// Generic extended signal handler
#define SIGSEGV_FAULT_HANDLER			sigsegv_fault_handler
#if defined(__NetBSD__) || defined(__FreeBSD__)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#else
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, sip, scp
#define SIGSEGV_FAULT_ADDRESS			sip->si_addr
#if defined(__NetBSD__) || defined(__FreeBSD__)
#if (defined(i386) || defined(__i386__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_eip)
#define SIGSEGV_REGISTER_FILE			((unsigned int *)&(((struct sigcontext *)scp)->sc_edi)) /* EDI is the first GPR (even below EIP) in sigcontext */
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#endif
#if defined(__linux__)
#if (defined(i386) || defined(__i386__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[14] /* should use REG_EIP instead */
#define SIGSEGV_REGISTER_FILE			(unsigned int *)SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(x86_64) || defined(__x86_64__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[16] /* should use REG_RIP instead */
#define SIGSEGV_REGISTER_FILE			(unsigned long *)SIGSEGV_CONTEXT_REGS
#endif
#if (defined(ia64) || defined(__ia64__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_ip & ~0x3ULL) /* slot number is in bits 0 and 1 */
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.regs)
#define SIGSEGV_FAULT_INSTRUCTION		(SIGSEGV_CONTEXT_REGS->nip)
#define SIGSEGV_REGISTER_FILE			(unsigned int *)&SIGSEGV_CONTEXT_REGS->nip, (unsigned int *)(SIGSEGV_CONTEXT_REGS->gpr)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#endif
#endif
#endif

#if HAVE_SIGCONTEXT_SUBTERFUGE
#define SIGSEGV_FAULT_HANDLER			sigsegv_fault_handler
// Linux kernels prior to 2.4 ?
#if defined(__linux__)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#if (defined(i386) || defined(__i386__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext scs
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, scs
#define SIGSEGV_FAULT_ADDRESS			scs.cr2
#define SIGSEGV_FAULT_INSTRUCTION		scs.eip
#define SIGSEGV_REGISTER_FILE			(unsigned int *)(&scs)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(sparc) || defined(__sparc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp, char *addr
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp, addr
#define SIGSEGV_FAULT_ADDRESS			addr
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, scp
#define SIGSEGV_FAULT_ADDRESS			scp->regs->dar
#define SIGSEGV_FAULT_INSTRUCTION		scp->regs->nip
#define SIGSEGV_REGISTER_FILE			(unsigned int *)&scp->regs->nip, (unsigned int *)(scp->regs->gpr)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#endif
#if (defined(alpha) || defined(__alpha__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_pc

// From Boehm's GC 6.0alpha8
static sigsegv_address_t get_fault_address(struct sigcontext *scp)
{
	unsigned int instruction = *((unsigned int *)(scp->sc_pc));
	unsigned long fault_address = scp->sc_regs[(instruction >> 16) & 0x1f];
	fault_address += (signed long)(signed short)(instruction & 0xffff);
	return (sigsegv_address_t)fault_address;
}
#endif
#endif

// Irix 5 or 6 on MIPS
#if (defined(sgi) || defined(__sgi)) && (defined(SYSTYPE_SVR4) || defined(__SYSTYPE_SVR4))
#include <ucontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_badvaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// HP-UX
#if (defined(hpux) || defined(__hpux__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_sl.sl_ss.ss_narrow.ss_cr21
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV) FAULT_HANDLER(SIGBUS)
#endif

// OSF/1 on Alpha
#if defined(__osf__)
#include <ucontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_traparg_a0
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// AIX
#if defined(_AIX)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_jmpbuf.jmp_context.o_vaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// NetBSD or FreeBSD
#if defined(__NetBSD__) || defined(__FreeBSD__)
#if (defined(m68k) || defined(__m68k__))
#include <m68k/frame.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)

// Use decoding scheme from BasiliskII/m68k native
static sigsegv_address_t get_fault_address(struct sigcontext *scp)
{
	struct sigstate {
		int ss_flags;
		struct frame ss_frame;
	};
	struct sigstate *state = (struct sigstate *)scp->sc_ap;
	char *fault_addr;
	switch (state->ss_frame.f_format) {
	case 7:		/* 68040 access error */
		/* "code" is sometimes unreliable (i.e. contains NULL or a bogus address), reason unknown */
		fault_addr = state->ss_frame.f_fmt7.f_fa;
		break;
	default:
		fault_addr = (char *)code;
		break;
	}
	return (sigsegv_address_t)fault_addr;
}
#else
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, void *scp, char *addr
#define SIGSEGV_FAULT_HANDLER_ARGS	sig, code, scp, addr
#define SIGSEGV_FAULT_ADDRESS			addr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#endif
#endif

// MacOS X, not sure which version this works in. Under 10.1
// vm_protect does not appear to work from a signal handler. Under
// 10.2 signal handlers get siginfo type arguments but the si_addr
// field is the address of the faulting instruction and not the
// address that caused the SIGBUS. Maybe this works in 10.0? In any
// case with Mach exception handlers there is a way to do what this
// was meant to do.
#ifndef HAVE_MACH_EXCEPTIONS
#if defined(__APPLE__) && defined(__MACH__)
#if (defined(ppc) || defined(__ppc__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_ir
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#define SIGSEGV_REGISTER_FILE			(unsigned int *)&scp->sc_ir, &((unsigned int *) scp->sc_regs)[2]
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction

// Use decoding scheme from SheepShaver
static sigsegv_address_t get_fault_address(struct sigcontext *scp)
{
	unsigned int   nip = (unsigned int) scp->sc_ir;
	unsigned int * gpr = &((unsigned int *) scp->sc_regs)[2];
	instruction_t  instr;

	powerpc_decode_instruction(&instr, nip, gpr);
	return (sigsegv_address_t)instr.addr;
}
#endif
#endif
#endif
#endif

#if HAVE_MACH_EXCEPTIONS

// This can easily be extended to other Mach systems, but really who
// uses HURD (oops GNU/HURD), Darwin/x86, NextStep, Rhapsody, or CMU
// Mach 2.5/3.0?
#if defined(__APPLE__) && defined(__MACH__)

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*
 * If you are familiar with MIG then you will understand the frustration
 * that was necessary to get these embedded into C++ code by hand.
 */
extern "C" {
#include <mach/mach.h>
#include <mach/mach_error.h>

extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);
extern kern_return_t catch_exception_raise(mach_port_t, mach_port_t,
	mach_port_t, exception_type_t, exception_data_t, mach_msg_type_number_t);
extern kern_return_t exception_raise(mach_port_t, mach_port_t, mach_port_t,
	exception_type_t, exception_data_t, mach_msg_type_number_t);
extern kern_return_t exception_raise_state(mach_port_t, exception_type_t,
	exception_data_t, mach_msg_type_number_t, thread_state_flavor_t *,
	thread_state_t, mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t *);
extern kern_return_t exception_raise_state_identity(mach_port_t, mach_port_t, mach_port_t,
	exception_type_t, exception_data_t, mach_msg_type_number_t, thread_state_flavor_t *,
	thread_state_t, mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t *);
}

// Could make this dynamic by looking for a result of MIG_ARRAY_TOO_LARGE
#define HANDLER_COUNT 64

// structure to tuck away existing exception handlers
typedef struct _ExceptionPorts {
	mach_msg_type_number_t maskCount;
	exception_mask_t masks[HANDLER_COUNT];
	exception_handler_t handlers[HANDLER_COUNT];
	exception_behavior_t behaviors[HANDLER_COUNT];
	thread_state_flavor_t flavors[HANDLER_COUNT];
} ExceptionPorts;

// exception handler thread
static pthread_t exc_thread;

// place where old exception handler info is stored
static ExceptionPorts ports;

// our exception port
static mach_port_t _exceptionPort = MACH_PORT_NULL;

#define MACH_CHECK_ERROR(name,ret) \
if (ret != KERN_SUCCESS) { \
	mach_error(#name, ret); \
	exit (1); \
}

#define SIGSEGV_FAULT_ADDRESS			code[1]
#define	SIGSEGV_FAULT_INSTRUCTION		get_fault_instruction(thread, state)
#define SIGSEGV_FAULT_HANDLER			(code[0] == KERN_PROTECTION_FAILURE) && sigsegv_fault_handler
#define SIGSEGV_FAULT_HANDLER_ARGLIST	mach_port_t thread, exception_data_t code, ppc_thread_state_t *state
#define SIGSEGV_FAULT_HANDLER_ARGS		thread, code, &state
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#define SIGSEGV_REGISTER_FILE			&state->srr0, &state->r0

// Given a suspended thread, stuff the current instruction and
// registers into state.
//
// It would have been nice to have this be ppc/x86 independant which
// could have been done easily with a thread_state_t instead of
// ppc_thread_state_t, but because of the way this is called it is
// easier to do it this way.
#if (defined(ppc) || defined(__ppc__))
static inline sigsegv_address_t get_fault_instruction(mach_port_t thread, ppc_thread_state_t *state)
{
	kern_return_t krc;
	mach_msg_type_number_t count;

	count = MACHINE_THREAD_STATE_COUNT;
	krc = thread_get_state(thread, MACHINE_THREAD_STATE, (thread_state_t)state, &count);
	MACH_CHECK_ERROR (thread_get_state, krc);

	return (sigsegv_address_t)state->srr0;
}
#endif

// Since there can only be one exception thread running at any time
// this is not a problem.
#define MSG_SIZE 512
static char msgbuf[MSG_SIZE];
static char replybuf[MSG_SIZE];

/*
 * This is the entry point for the exception handler thread. The job
 * of this thread is to wait for exception messages on the exception
 * port that was setup beforehand and to pass them on to exc_server.
 * exc_server is a MIG generated function that is a part of Mach.
 * Its job is to decide what to do with the exception message. In our
 * case exc_server calls catch_exception_raise on our behalf. After
 * exc_server returns, it is our responsibility to send the reply.
 */
static void *
handleExceptions(void *priv)
{
	mach_msg_header_t *msg, *reply;
	kern_return_t krc;

	msg = (mach_msg_header_t *)msgbuf;
	reply = (mach_msg_header_t *)replybuf;

	for (;;) {
		krc = mach_msg(msg, MACH_RCV_MSG, MSG_SIZE, MSG_SIZE,
				_exceptionPort, 0, MACH_PORT_NULL);
		MACH_CHECK_ERROR(mach_msg, krc);

		if (!exc_server(msg, reply)) {
			fprintf(stderr, "exc_server hated the message\n");
			exit(1);
		}

		krc = mach_msg(reply, MACH_SEND_MSG, reply->msgh_size, 0,
				 msg->msgh_local_port, 0, MACH_PORT_NULL);
		if (krc != KERN_SUCCESS) {
			fprintf(stderr, "Error sending message to original reply port, krc = %d, %s",
				krc, mach_error_string(krc));
			exit(1);
		}
	}
}
#endif
#endif


/*
 *  Instruction skipping
 */

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
// Decode and skip X86 instruction
#if (defined(i386) || defined(__i386__))
#if defined(__linux__)
enum {
	X86_REG_EIP = 14,
	X86_REG_EAX = 11,
	X86_REG_ECX = 10,
	X86_REG_EDX = 9,
	X86_REG_EBX = 8,
	X86_REG_ESP = 7,
	X86_REG_EBP = 6,
	X86_REG_ESI = 5,
	X86_REG_EDI = 4
};
#endif
#if defined(__NetBSD__) || defined(__FreeBSD__)
enum {
	X86_REG_EIP = 10,
	X86_REG_EAX = 7,
	X86_REG_ECX = 6,
	X86_REG_EDX = 5,
	X86_REG_EBX = 4,
	X86_REG_ESP = 13,
	X86_REG_EBP = 2,
	X86_REG_ESI = 1,
	X86_REG_EDI = 0
};
#endif
// FIXME: this is partly redundant with the instruction decoding phase
// to discover transfer type and register number
static inline int ix86_step_over_modrm(unsigned char * p)
{
	int mod = (p[0] >> 6) & 3;
	int rm = p[0] & 7;
	int offset = 0;

	// ModR/M Byte
	switch (mod) {
	case 0: // [reg]
		if (rm == 5) return 4; // disp32
		break;
	case 1: // disp8[reg]
		offset = 1;
		break;
	case 2: // disp32[reg]
		offset = 4;
		break;
	case 3: // register
		return 0;
	}
	
	// SIB Byte
	if (rm == 4) {
		if (mod == 0 && (p[1] & 7) == 5)
			offset = 5; // disp32[index]
		else
			offset++;
	}

	return offset;
}

static bool ix86_skip_instruction(unsigned int * regs)
{
	unsigned char * eip = (unsigned char *)regs[X86_REG_EIP];

	if (eip == 0)
		return false;
	
	transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
	transfer_size_t transfer_size = SIZE_LONG;
	
	int reg = -1;
	int len = 0;
	
	// Operand size prefix
	if (*eip == 0x66) {
		eip++;
		len++;
		transfer_size = SIZE_WORD;
	}

	// Decode instruction
	switch (eip[0]) {
	case 0x0f:
	    switch (eip[1]) {
	    case 0xb6: // MOVZX r32, r/m8
	    case 0xb7: // MOVZX r32, r/m16
		switch (eip[2] & 0xc0) {
		case 0x80:
		    reg = (eip[2] >> 3) & 7;
		    transfer_type = SIGSEGV_TRANSFER_LOAD;
		    break;
		case 0x40:
		    reg = (eip[2] >> 3) & 7;
		    transfer_type = SIGSEGV_TRANSFER_LOAD;
		    break;
		case 0x00:
		    reg = (eip[2] >> 3) & 7;
		    transfer_type = SIGSEGV_TRANSFER_LOAD;
		    break;
		}
		len += 3 + ix86_step_over_modrm(eip + 2);
		break;
	    }
	  break;
	case 0x8a: // MOV r8, r/m8
		transfer_size = SIZE_BYTE;
	case 0x8b: // MOV r32, r/m32 (or 16-bit operation)
		switch (eip[1] & 0xc0) {
		case 0x80:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		case 0x40:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		case 0x00:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		}
		len += 2 + ix86_step_over_modrm(eip + 1);
		break;
	case 0x88: // MOV r/m8, r8
		transfer_size = SIZE_BYTE;
	case 0x89: // MOV r/m32, r32 (or 16-bit operation)
		switch (eip[1] & 0xc0) {
		case 0x80:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		case 0x40:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		case 0x00:
			reg = (eip[1] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		}
		len += 2 + ix86_step_over_modrm(eip + 1);
		break;
	}

	if (transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

	if (transfer_type == SIGSEGV_TRANSFER_LOAD && reg != -1) {
		static const int x86_reg_map[8] = {
			X86_REG_EAX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBX,
			X86_REG_ESP, X86_REG_EBP, X86_REG_ESI, X86_REG_EDI
		};
		
		if (reg < 0 || reg >= 8)
			return false;

		int rloc = x86_reg_map[reg];
		switch (transfer_size) {
		case SIZE_BYTE:
			regs[rloc] = (regs[rloc] & ~0xff);
			break;
		case SIZE_WORD:
			regs[rloc] = (regs[rloc] & ~0xffff);
			break;
		case SIZE_LONG:
			regs[rloc] = 0;
			break;
		}
	}

#if DEBUG
	printf("%08x: %s %s access", regs[X86_REG_EIP],
		   transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_WORD ? "word" : "long",
		   transfer_type == SIGSEGV_TRANSFER_LOAD ? "read" : "write");
	
	if (reg != -1) {
		static const char * x86_reg_str_map[8] = {
			"eax", "ecx", "edx", "ebx",
			"esp", "ebp", "esi", "edi"
		};
		printf(" %s register %%%s", transfer_type == SIGSEGV_TRANSFER_LOAD ? "to" : "from", x86_reg_str_map[reg]);
	}
	printf(", %d bytes instruction\n", len);
#endif
	
	regs[X86_REG_EIP] += len;
	return true;
}
#endif

// Decode and skip PPC instruction
#if (defined(powerpc) || defined(__powerpc__) || defined(__ppc__))
static bool powerpc_skip_instruction(unsigned int * nip_p, unsigned int * regs)
{
	instruction_t instr;
	powerpc_decode_instruction(&instr, *nip_p, regs);
	
	if (instr.transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

#if DEBUG
	printf("%08x: %s %s access", *nip_p,
		   instr.transfer_size == SIZE_BYTE ? "byte" : instr.transfer_size == SIZE_WORD ? "word" : "long",
		   instr.transfer_type == SIGSEGV_TRANSFER_LOAD ? "read" : "write");
	
	if (instr.addr_mode == MODE_U || instr.addr_mode == MODE_UX)
		printf(" r%d (ra = %08x)\n", instr.ra, instr.addr);
	if (instr.transfer_type == SIGSEGV_TRANSFER_LOAD)
		printf(" r%d (rd = 0)\n", instr.rd);
#endif
	
	if (instr.addr_mode == MODE_U || instr.addr_mode == MODE_UX)
		regs[instr.ra] = instr.addr;
	if (instr.transfer_type == SIGSEGV_TRANSFER_LOAD)
		regs[instr.rd] = 0;
	
	*nip_p += 4;
	return true;
}
#endif
#endif

// Fallbacks
#ifndef SIGSEGV_FAULT_INSTRUCTION
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_INVALID_PC
#endif

// SIGSEGV recovery supported ?
#if defined(SIGSEGV_ALL_SIGNALS) && defined(SIGSEGV_FAULT_HANDLER_ARGLIST) && defined(SIGSEGV_FAULT_ADDRESS)
#define HAVE_SIGSEGV_RECOVERY
#endif


/*
 *  SIGSEGV global handler
 */

#if defined(HAVE_SIGSEGV_RECOVERY) || defined(HAVE_MACH_EXCEPTIONS)
// This function handles the badaccess to memory.
// It is called from the signal handler or the exception handler.
static bool handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGLIST)
{
	sigsegv_address_t fault_address = (sigsegv_address_t)SIGSEGV_FAULT_ADDRESS;
	sigsegv_address_t fault_instruction = (sigsegv_address_t)SIGSEGV_FAULT_INSTRUCTION;
	
	// Call user's handler and reinstall the global handler, if required
	switch (sigsegv_fault_handler(fault_address, fault_instruction)) {
	case SIGSEGV_RETURN_SUCCESS:
		return true;

#if HAVE_SIGSEGV_SKIP_INSTRUCTION
	case SIGSEGV_RETURN_SKIP_INSTRUCTION:
		// Call the instruction skipper with the register file
		// available
		if (SIGSEGV_SKIP_INSTRUCTION(SIGSEGV_REGISTER_FILE)) {
#ifdef HAVE_MACH_EXCEPTIONS
			// Unlike UNIX signals where the thread state
			// is modified off of the stack, in Mach we
			// need to actually call thread_set_state to
			// have the register values updated.
			kern_return_t krc;

			krc = thread_set_state(thread,
								   MACHINE_THREAD_STATE, (thread_state_t)state,
								   MACHINE_THREAD_STATE_COUNT);
			MACH_CHECK_ERROR (thread_get_state, krc);
#endif
			return true;
		}
		break;
#endif
	}
        
        // We can't do anything with the fault_address, dump state?
	if (sigsegv_state_dumper != 0)
		sigsegv_state_dumper(fault_address, fault_instruction);

	return false;
}
#endif


/*
 * There are two mechanisms for handling a bad memory access,
 * Mach exceptions and UNIX signals. The implementation specific
 * code appears below. Its reponsibility is to call handle_badaccess
 * which is the routine that handles the fault in an implementation
 * agnostic manner. The implementation specific code below is then
 * reponsible for checking whether handle_badaccess was able
 * to handle the memory access error and perform any implementation
 * specific tasks necessary afterwards.
 */

#ifdef HAVE_MACH_EXCEPTIONS
/*
 * We need to forward all exceptions that we do not handle.
 * This is important, there are many exceptions that may be
 * handled by other exception handlers. For example debuggers
 * use exceptions and the exception hander is in another
 * process in such a case. (Timothy J. Wood states in his
 * message to the list that he based this code on that from
 * gdb for Darwin.)
 */
static inline kern_return_t
forward_exception(mach_port_t thread_port,
				  mach_port_t task_port,
				  exception_type_t exception_type,
				  exception_data_t exception_data,
				  mach_msg_type_number_t data_count,
				  ExceptionPorts *oldExceptionPorts)
{
	kern_return_t kret;
	unsigned int portIndex;
	mach_port_t port;
	exception_behavior_t behavior;
	thread_state_flavor_t flavor;
	thread_state_t thread_state;
	mach_msg_type_number_t thread_state_count;

	for (portIndex = 0; portIndex < oldExceptionPorts->maskCount; portIndex++) {
		if (oldExceptionPorts->masks[portIndex] & (1 << exception_type)) {
			// This handler wants the exception
			break;
		}
	}

	if (portIndex >= oldExceptionPorts->maskCount) {
		fprintf(stderr, "No handler for exception_type = %d. Not fowarding\n", exception_type);
		return KERN_FAILURE;
	}

	port = oldExceptionPorts->handlers[portIndex];
	behavior = oldExceptionPorts->behaviors[portIndex];
	flavor = oldExceptionPorts->flavors[portIndex];

	/*
	 fprintf(stderr, "forwarding exception, port = 0x%x, behaviour = %d, flavor = %d\n", port, behavior, flavor);
	 */

	if (behavior != EXCEPTION_DEFAULT) {
		thread_state_count = THREAD_STATE_MAX;
		kret = thread_get_state (thread_port, flavor, thread_state,
								 &thread_state_count);
		MACH_CHECK_ERROR (thread_get_state, kret);
	}

	switch (behavior) {
	case EXCEPTION_DEFAULT:
	  // fprintf(stderr, "forwarding to exception_raise\n");
	  kret = exception_raise(port, thread_port, task_port, exception_type,
							 exception_data, data_count);
	  MACH_CHECK_ERROR (exception_raise, kret);
	  break;
	case EXCEPTION_STATE:
	  // fprintf(stderr, "forwarding to exception_raise_state\n");
	  kret = exception_raise_state(port, exception_type, exception_data,
								   data_count, &flavor,
								   thread_state, thread_state_count,
								   thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (exception_raise_state, kret);
	  break;
	case EXCEPTION_STATE_IDENTITY:
	  // fprintf(stderr, "forwarding to exception_raise_state_identity\n");
	  kret = exception_raise_state_identity(port, thread_port, task_port,
											exception_type, exception_data,
											data_count, &flavor,
											thread_state, thread_state_count,
											thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (exception_raise_state_identity, kret);
	  break;
	default:
	  fprintf(stderr, "forward_exception got unknown behavior\n");
	  break;
	}

	if (behavior != EXCEPTION_DEFAULT) {
		kret = thread_set_state (thread_port, flavor, thread_state,
								 thread_state_count);
		MACH_CHECK_ERROR (thread_set_state, kret);
	}

	return KERN_SUCCESS;
}

/*
 * This is the code that actually handles the exception.
 * It is called by exc_server. For Darwin 5 Apple changed
 * this a bit from how this family of functions worked in
 * Mach. If you are familiar with that it is a little
 * different. The main variation that concerns us here is
 * that code is an array of exception specific codes and
 * codeCount is a count of the number of codes in the code
 * array. In typical Mach all exceptions have a code
 * and sub-code. It happens to be the case that for a
 * EXC_BAD_ACCESS exception the first entry is the type of
 * bad access that occurred and the second entry is the
 * faulting address so these entries correspond exactly to
 * how the code and sub-code are used on Mach.
 *
 * This is a MIG interface. No code in Basilisk II should
 * call this directley. This has to have external C
 * linkage because that is what exc_server expects.
 */
kern_return_t
catch_exception_raise(mach_port_t exception_port,
					  mach_port_t thread,
					  mach_port_t task,
					  exception_type_t exception,
					  exception_data_t code,
					  mach_msg_type_number_t codeCount)
{
	ppc_thread_state_t state;
	kern_return_t krc;

	if ((exception == EXC_BAD_ACCESS)  && (codeCount >= 2)) {
		if (handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGS))
			return KERN_SUCCESS;
	}

	// In Mach we do not need to remove the exception handler.
	// If we forward the exception, eventually some exception handler
	// will take care of this exception.
	krc = forward_exception(thread, task, exception, code, codeCount, &ports);

	return krc;
}
#endif

#ifdef HAVE_SIGSEGV_RECOVERY
// Handle bad memory accesses with signal handler
static void sigsegv_handler(SIGSEGV_FAULT_HANDLER_ARGLIST)
{
	// Call handler and reinstall the global handler, if required
	if (handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGS)) {
#if (defined(HAVE_SIGACTION) ? defined(SIGACTION_NEED_REINSTALL) : defined(SIGNAL_NEED_REINSTALL))
		sigsegv_do_install_handler(sig);
#endif
		return;
	}

	// Failure: reinstall default handler for "safe" crash
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
}
#endif


/*
 *  SIGSEGV handler initialization
 */

#if defined(HAVE_SIGINFO_T)
static bool sigsegv_do_install_handler(int sig)
{
	// Setup SIGSEGV handler to process writes to frame buffer
#ifdef HAVE_SIGACTION
	struct sigaction sigsegv_sa;
	sigemptyset(&sigsegv_sa.sa_mask);
	sigsegv_sa.sa_sigaction = sigsegv_handler;
	sigsegv_sa.sa_flags = SA_SIGINFO;
	return (sigaction(sig, &sigsegv_sa, 0) == 0);
#else
	return (signal(sig, (signal_handler)sigsegv_handler) != SIG_ERR);
#endif
}
#endif

#if defined(HAVE_SIGCONTEXT_SUBTERFUGE)
static bool sigsegv_do_install_handler(int sig)
{
	// Setup SIGSEGV handler to process writes to frame buffer
#ifdef HAVE_SIGACTION
	struct sigaction sigsegv_sa;
	sigemptyset(&sigsegv_sa.sa_mask);
	sigsegv_sa.sa_handler = (signal_handler)sigsegv_handler;
	sigsegv_sa.sa_flags = 0;
#if !EMULATED_68K && defined(__NetBSD__)
	sigaddset(&sigsegv_sa.sa_mask, SIGALRM);
	sigsegv_sa.sa_flags |= SA_ONSTACK;
#endif
	return (sigaction(sig, &sigsegv_sa, 0) == 0);
#else
	return (signal(sig, (signal_handler)sigsegv_handler) != SIG_ERR);
#endif
}
#endif

#if defined(HAVE_MACH_EXCEPTIONS)
static bool sigsegv_do_install_handler(sigsegv_fault_handler_t handler)
{
	/*
	 * Except for the exception port functions, this should be
	 * pretty much stock Mach. If later you choose to support
	 * other Mach's besides Darwin, just check for __MACH__
	 * here and __APPLE__ where the actual differences are.
	 */
#if defined(__APPLE__) && defined(__MACH__)
	if (sigsegv_fault_handler != NULL) {
		sigsegv_fault_handler = handler;
		return true;
	}

	kern_return_t krc;

	// create the the exception port
	krc = mach_port_allocate(mach_task_self(),
			  MACH_PORT_RIGHT_RECEIVE, &_exceptionPort);
	if (krc != KERN_SUCCESS) {
		mach_error("mach_port_allocate", krc);
		return false;
	}

	// add a port send right
	krc = mach_port_insert_right(mach_task_self(),
			      _exceptionPort, _exceptionPort,
			      MACH_MSG_TYPE_MAKE_SEND);
	if (krc != KERN_SUCCESS) {
		mach_error("mach_port_insert_right", krc);
		return false;
	}

	// get the old exception ports
	ports.maskCount = sizeof (ports.masks) / sizeof (ports.masks[0]);
	krc = thread_get_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, ports.masks,
 				&ports.maskCount, ports.handlers, ports.behaviors, ports.flavors);
 	if (krc != KERN_SUCCESS) {
 		mach_error("thread_get_exception_ports", krc);
 		return false;
 	}

	// set the new exception port
	//
	// We could have used EXCEPTION_STATE_IDENTITY instead of
	// EXCEPTION_DEFAULT to get the thread state in the initial
	// message, but it turns out that in the common case this is not
	// neccessary. If we need it we can later ask for it from the
	// suspended thread.
	//
	// Even with THREAD_STATE_NONE, Darwin provides the program
	// counter in the thread state.  The comments in the header file
	// seem to imply that you can count on the GPR's on an exception
	// as well but just to be safe I use MACHINE_THREAD_STATE because
	// you have to ask for all of the GPR's anyway just to get the
	// program counter. In any case because of update effective
	// address from immediate and update address from effective
	// addresses of ra and rb modes (as good an name as any for these
	// addressing modes) used in PPC instructions, you will need the
	// GPR state anyway.
	krc = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, _exceptionPort,
				EXCEPTION_DEFAULT, MACHINE_THREAD_STATE);
	if (krc != KERN_SUCCESS) {
		mach_error("thread_set_exception_ports", krc);
		return false;
	}

	// create the exception handler thread
	if (pthread_create(&exc_thread, NULL, &handleExceptions, NULL) != 0) {
		(void)fprintf(stderr, "creation of exception thread failed\n");
		return false;
	}

	// do not care about the exception thread any longer, let is run standalone
	(void)pthread_detach(exc_thread);

	sigsegv_fault_handler = handler;
	return true;
#else
	return false;
#endif
}
#endif

bool sigsegv_install_handler(sigsegv_fault_handler_t handler)
{
#if defined(HAVE_SIGSEGV_RECOVERY)
	bool success = true;
#define FAULT_HANDLER(sig) success = success && sigsegv_do_install_handler(sig);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
	if (success)
            sigsegv_fault_handler = handler;
	return success;
#elif defined(HAVE_MACH_EXCEPTIONS)
	return sigsegv_do_install_handler(handler);
#else
	// FAIL: no siginfo_t nor sigcontext subterfuge is available
	return false;
#endif
}


/*
 *  SIGSEGV handler deinitialization
 */

void sigsegv_deinstall_handler(void)
{
  // We do nothing for Mach exceptions, the thread would need to be
  // suspended if not already so, and we might mess with other
  // exception handlers that came after we registered ours. There is
  // no need to remove the exception handler, in fact this function is
  // not called anywhere in Basilisk II.
#ifdef HAVE_SIGSEGV_RECOVERY
	sigsegv_fault_handler = 0;
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
#endif
}


/*
 *  Set callback function when we cannot handle the fault
 */

void sigsegv_set_dump_state(sigsegv_state_dumper_t handler)
{
	sigsegv_state_dumper = handler;
}


/*
 *  Test program used for configure/test
 */

#ifdef CONFIGURE_TEST_SIGSEGV_RECOVERY
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "vm_alloc.h"

static int page_size;
static volatile char * page = 0;
static volatile int handler_called = 0;

static sigsegv_return_t sigsegv_test_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	handler_called++;
	if ((fault_address - 123) != page)
		exit(1);
	if (vm_protect((char *)((unsigned long)fault_address & -page_size), page_size, VM_PAGE_READ | VM_PAGE_WRITE) != 0)
		exit(1);
	return SIGSEGV_RETURN_SUCCESS;
}

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
#ifdef __GNUC__
// Code range where we expect the fault to come from
static void *b_region, *e_region;
#endif

static sigsegv_return_t sigsegv_insn_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	if (((unsigned long)fault_address - (unsigned long)page) < page_size) {
#ifdef __GNUC__
		// Make sure reported fault instruction address falls into
		// expected code range
		if (instruction_address != SIGSEGV_INVALID_PC
			&& ((instruction_address <  (sigsegv_address_t)b_region) ||
				(instruction_address >= (sigsegv_address_t)e_region)))
			return SIGSEGV_RETURN_FAILURE;
#endif
		return SIGSEGV_RETURN_SKIP_INSTRUCTION;
	}

	return SIGSEGV_RETURN_FAILURE;
}
#endif

int main(void)
{
	if (vm_init() < 0)
		return 1;

	page_size = getpagesize();
	if ((page = (char *)vm_acquire(page_size)) == VM_MAP_FAILED)
		return 1;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_READ) < 0)
		return 1;
	
	if (!sigsegv_install_handler(sigsegv_test_handler))
		return 1;
	
	page[123] = 45;
	page[123] = 45;
	
	if (handler_called != 1)
		return 1;

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	if (!sigsegv_install_handler(sigsegv_insn_handler))
		return 1;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_READ | VM_PAGE_WRITE) < 0)
		return 1;
	
	for (int i = 0; i < page_size; i++)
		page[i] = (i + 1) % page_size;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_NOACCESS) < 0)
		return 1;
	
#define TEST_SKIP_INSTRUCTION(TYPE) do {				\
		const unsigned int TAG = 0x12345678;			\
		TYPE data = *((TYPE *)(page + sizeof(TYPE)));	\
		volatile unsigned int effect = data + TAG;		\
		if (effect != TAG)								\
			return 1;									\
	} while (0)
	
#ifdef __GNUC__
	b_region = &&L_b_region;
	e_region = &&L_e_region;
#endif
 L_b_region:
	TEST_SKIP_INSTRUCTION(unsigned char);
	TEST_SKIP_INSTRUCTION(unsigned short);
	TEST_SKIP_INSTRUCTION(unsigned int);
 L_e_region:
#endif

	vm_exit();
	return 0;
}
#endif
