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
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
#include <stdio.h>
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

// Transfer type
enum transfer_type_t {
	SIGSEGV_TRANSFER_UNKNOWN	= 0,
	SIGSEGV_TRANSFER_LOAD		= 1,
	SIGSEGV_TRANSFER_STORE		= 2,
};

// Transfer size
enum transfer_size_t {
	SIZE_UNKNOWN,
	SIZE_BYTE,
	SIZE_WORD, // 2 bytes
	SIZE_LONG, // 4 bytes
	SIZE_QUAD, // 8 bytes
};

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

static void powerpc_decode_instruction(instruction_t *instruction, unsigned int nip, unsigned long * gpr)
{
	// Get opcode and divide into fields
	unsigned int opcode = *((unsigned int *)(unsigned long)nip);
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
	case 58:	// ld, ldu, lwa
		transfer_type = SIGSEGV_TRANSFER_LOAD;
		transfer_size = SIZE_QUAD;
		addr_mode = ((opcode & 3) == 1) ? MODE_U : MODE_NORM;
		imm &= ~3;
		break;
	case 62:	// std, stdu, stq
		transfer_type = SIGSEGV_TRANSFER_STORE;
		transfer_size = SIZE_QUAD;
		addr_mode = ((opcode & 3) == 1) ? MODE_U : MODE_NORM;
		imm &= ~3;
		break;
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
#if defined(__FreeBSD__)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#else
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sip, scp
#define SIGSEGV_FAULT_ADDRESS			sip->si_addr
#if (defined(sgi) || defined(__sgi))
#include <ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		(unsigned long)SIGSEGV_CONTEXT_REGS[CTX_EPC]
#if (defined(mips) || defined(__mips))
#define SIGSEGV_REGISTER_FILE			SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		mips_skip_instruction
#endif
#endif
#if defined(__sun__)
#if (defined(sparc) || defined(__sparc__))
#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[REG_PC]
#define SIGSEGV_SPARC_GWINDOWS			(((ucontext_t *)scp)->uc_mcontext.gwins)
#define SIGSEGV_SPARC_RWINDOW			(struct rwindow *)((char *)SIGSEGV_CONTEXT_REGS[REG_SP] + STACK_BIAS)
#define SIGSEGV_REGISTER_FILE			((unsigned long *)SIGSEGV_CONTEXT_REGS), SIGSEGV_SPARC_GWINDOWS, SIGSEGV_SPARC_RWINDOW
#define SIGSEGV_SKIP_INSTRUCTION		sparc_skip_instruction
#endif
#if defined(__i386__)
#include <sys/regset.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[EIP]
#define SIGSEGV_REGISTER_FILE			(unsigned long *)SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#if (defined(i386) || defined(__i386__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_eip)
#define SIGSEGV_REGISTER_FILE			((unsigned long *)&(((struct sigcontext *)scp)->sc_edi)) /* EDI is the first GPR (even below EIP) in sigcontext */
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#endif
#if defined(__NetBSD__)
#if (defined(i386) || defined(__i386__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.__gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[_REG_EIP]
#define SIGSEGV_REGISTER_FILE			(unsigned long *)SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.__gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[_REG_PC]
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&SIGSEGV_CONTEXT_REGS[_REG_PC], (unsigned long *)&SIGSEGV_CONTEXT_REGS[_REG_R0]
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#endif
#endif
#if defined(__linux__)
#if (defined(i386) || defined(__i386__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[14] /* should use REG_EIP instead */
#define SIGSEGV_REGISTER_FILE			(unsigned long *)SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(x86_64) || defined(__x86_64__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS[16] /* should use REG_RIP instead */
#define SIGSEGV_REGISTER_FILE			(unsigned long *)SIGSEGV_CONTEXT_REGS
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(ia64) || defined(__ia64__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_ip & ~0x3ULL) /* slot number is in bits 0 and 1 */
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <sys/ucontext.h>
#define SIGSEGV_CONTEXT_REGS			(((ucontext_t *)scp)->uc_mcontext.regs)
#define SIGSEGV_FAULT_INSTRUCTION		(SIGSEGV_CONTEXT_REGS->nip)
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&SIGSEGV_CONTEXT_REGS->nip, (unsigned long *)(SIGSEGV_CONTEXT_REGS->gpr)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#endif
#if (defined(hppa) || defined(__hppa__))
#undef  SIGSEGV_FAULT_ADDRESS
#define SIGSEGV_FAULT_ADDRESS			sip->si_ptr
#endif
#if (defined(arm) || defined(__arm__))
#include <asm/ucontext.h> /* use kernel structure, glibc may not be in sync */
#define SIGSEGV_CONTEXT_REGS			(((struct ucontext *)scp)->uc_mcontext)
#define SIGSEGV_FAULT_INSTRUCTION		(SIGSEGV_CONTEXT_REGS.arm_pc)
#define SIGSEGV_REGISTER_FILE			(&SIGSEGV_CONTEXT_REGS.arm_r0)
#define SIGSEGV_SKIP_INSTRUCTION		arm_skip_instruction
#endif
#endif
#endif

#if HAVE_SIGCONTEXT_SUBTERFUGE
// Linux kernels prior to 2.4 ?
#if defined(__linux__)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#if (defined(i386) || defined(__i386__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext scs
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		&scs
#define SIGSEGV_FAULT_ADDRESS			scp->cr2
#define SIGSEGV_FAULT_INSTRUCTION		scp->eip
#define SIGSEGV_REGISTER_FILE			(unsigned long *)scp
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(sparc) || defined(__sparc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp, char *addr
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp, addr
#define SIGSEGV_FAULT_ADDRESS			addr
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, scp
#define SIGSEGV_FAULT_ADDRESS			scp->regs->dar
#define SIGSEGV_FAULT_INSTRUCTION		scp->regs->nip
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&scp->regs->nip, (unsigned long *)(scp->regs->gpr)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#endif
#if (defined(alpha) || defined(__alpha__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_pc
#endif
#if (defined(arm) || defined(__arm__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int r1, int r2, int r3, struct sigcontext sc
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		&sc
#define SIGSEGV_FAULT_ADDRESS			scp->fault_address
#define SIGSEGV_FAULT_INSTRUCTION		scp->arm_pc
#define SIGSEGV_REGISTER_FILE			&scp->arm_r0
#define SIGSEGV_SKIP_INSTRUCTION		arm_skip_instruction
#endif
#endif

// Irix 5 or 6 on MIPS
#if (defined(sgi) || defined(__sgi)) && (defined(SYSTYPE_SVR4) || defined(_SYSTYPE_SVR4))
#include <ucontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			(unsigned long)scp->sc_badvaddr
#define SIGSEGV_FAULT_INSTRUCTION		(unsigned long)scp->sc_pc
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// HP-UX
#if (defined(hpux) || defined(__hpux__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_sl.sl_ss.ss_narrow.ss_cr21
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV) FAULT_HANDLER(SIGBUS)
#endif

// OSF/1 on Alpha
#if defined(__osf__)
#include <ucontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_traparg_a0
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// AIX
#if defined(_AIX)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_jmpbuf.jmp_context.o_vaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// NetBSD
#if defined(__NetBSD__)
#if (defined(m68k) || defined(__m68k__))
#include <m68k/frame.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
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
#endif
#if (defined(alpha) || defined(__alpha__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#endif
#if (defined(i386) || defined(__i386__))
#error "FIXME: need to decode instruction and compute EA"
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif
#endif
#if defined(__FreeBSD__)
#if (defined(i386) || defined(__i386__))
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp, char *addr
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, code, scp, addr
#define SIGSEGV_FAULT_ADDRESS			addr
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_eip
#define SIGSEGV_REGISTER_FILE			((unsigned long *)&scp->sc_edi)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(alpha) || defined(__alpha__))
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, char *addr, struct sigcontext *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sig, addr, scp
#define SIGSEGV_FAULT_ADDRESS			addr
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_pc
#endif
#endif

// Extract fault address out of a sigcontext
#if (defined(alpha) || defined(__alpha__))
// From Boehm's GC 6.0alpha8
static sigsegv_address_t get_fault_address(struct sigcontext *scp)
{
	unsigned int instruction = *((unsigned int *)(scp->sc_pc));
	unsigned long fault_address = scp->sc_regs[(instruction >> 16) & 0x1f];
	fault_address += (signed long)(signed short)(instruction & 0xffff);
	return (sigsegv_address_t)fault_address;
}
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

#if HAVE_WIN32_EXCEPTIONS
#define WIN32_LEAN_AND_MEAN /* avoid including junk */
#include <windows.h>
#include <winerror.h>

#define SIGSEGV_FAULT_HANDLER_ARGLIST	EXCEPTION_POINTERS *ExceptionInfo
#define SIGSEGV_FAULT_HANDLER_ARGS		ExceptionInfo
#define SIGSEGV_FAULT_ADDRESS			ExceptionInfo->ExceptionRecord->ExceptionInformation[1]
#define SIGSEGV_CONTEXT_REGS			ExceptionInfo->ContextRecord
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_CONTEXT_REGS->Eip
#define SIGSEGV_REGISTER_FILE			((unsigned long *)&SIGSEGV_CONTEXT_REGS->Edi)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
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

#ifdef __ppc__
#define SIGSEGV_THREAD_STATE_TYPE		ppc_thread_state_t
#define SIGSEGV_THREAD_STATE_FLAVOR		PPC_THREAD_STATE
#define SIGSEGV_THREAD_STATE_COUNT		PPC_THREAD_STATE_COUNT
#define SIGSEGV_FAULT_INSTRUCTION		state->srr0
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&state->srr0, (unsigned long *)&state->r0
#endif
#ifdef __i386__
#ifdef i386_SAVED_STATE
#define SIGSEGV_THREAD_STATE_TYPE		struct i386_saved_state
#define SIGSEGV_THREAD_STATE_FLAVOR		i386_SAVED_STATE
#define SIGSEGV_THREAD_STATE_COUNT		i386_SAVED_STATE_COUNT
#define SIGSEGV_REGISTER_FILE			((unsigned long *)&state->edi) /* EDI is the first GPR we consider */
#else
#define SIGSEGV_THREAD_STATE_TYPE		struct i386_thread_state
#define SIGSEGV_THREAD_STATE_FLAVOR		i386_THREAD_STATE
#define SIGSEGV_THREAD_STATE_COUNT		i386_THREAD_STATE_COUNT
#define SIGSEGV_REGISTER_FILE			((unsigned long *)&state->eax) /* EAX is the first GPR we consider */
#endif
#define SIGSEGV_FAULT_INSTRUCTION		state->eip
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#define SIGSEGV_FAULT_ADDRESS			code[1]
#define SIGSEGV_FAULT_HANDLER_INVOKE(ADDR, IP)	((code[0] == KERN_PROTECTION_FAILURE || code[0] == KERN_INVALID_ADDRESS) ? sigsegv_fault_handler(ADDR, IP) : SIGSEGV_RETURN_FAILURE)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	mach_port_t thread, exception_data_t code, SIGSEGV_THREAD_STATE_TYPE *state
#define SIGSEGV_FAULT_HANDLER_ARGS		thread, code, &state

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
#if (defined(i386) || defined(__i386__)) || defined(__x86_64__)
#if defined(__linux__)
enum {
#if (defined(i386) || defined(__i386__))
	X86_REG_EIP = 14,
	X86_REG_EAX = 11,
	X86_REG_ECX = 10,
	X86_REG_EDX = 9,
	X86_REG_EBX = 8,
	X86_REG_ESP = 7,
	X86_REG_EBP = 6,
	X86_REG_ESI = 5,
	X86_REG_EDI = 4
#endif
#if defined(__x86_64__)
	X86_REG_R8  = 0,
	X86_REG_R9  = 1,
	X86_REG_R10 = 2,
	X86_REG_R11 = 3,
	X86_REG_R12 = 4,
	X86_REG_R13 = 5,
	X86_REG_R14 = 6,
	X86_REG_R15 = 7,
	X86_REG_EDI = 8,
	X86_REG_ESI = 9,
	X86_REG_EBP = 10,
	X86_REG_EBX = 11,
	X86_REG_EDX = 12,
	X86_REG_EAX = 13,
	X86_REG_ECX = 14,
	X86_REG_ESP = 15,
	X86_REG_EIP = 16
#endif
};
#endif
#if defined(__NetBSD__)
enum {
#if (defined(i386) || defined(__i386__))
	X86_REG_EIP = _REG_EIP,
	X86_REG_EAX = _REG_EAX,
	X86_REG_ECX = _REG_ECX,
	X86_REG_EDX = _REG_EDX,
	X86_REG_EBX = _REG_EBX,
	X86_REG_ESP = _REG_ESP,
	X86_REG_EBP = _REG_EBP,
	X86_REG_ESI = _REG_ESI,
	X86_REG_EDI = _REG_EDI
#endif
};
#endif
#if defined(__FreeBSD__)
enum {
#if (defined(i386) || defined(__i386__))
	X86_REG_EIP = 10,
	X86_REG_EAX = 7,
	X86_REG_ECX = 6,
	X86_REG_EDX = 5,
	X86_REG_EBX = 4,
	X86_REG_ESP = 13,
	X86_REG_EBP = 2,
	X86_REG_ESI = 1,
	X86_REG_EDI = 0
#endif
};
#endif
#if defined(__OpenBSD__)
enum {
#if defined(__i386__)
	// EDI is the first register we consider
#define OREG(REG) offsetof(struct sigcontext, sc_##REG)
#define DREG(REG) ((OREG(REG) - OREG(edi)) / 4)
	X86_REG_EIP = DREG(eip), // 7
	X86_REG_EAX = DREG(eax), // 6
	X86_REG_ECX = DREG(ecx), // 5
	X86_REG_EDX = DREG(edx), // 4
	X86_REG_EBX = DREG(ebx), // 3
	X86_REG_ESP = DREG(esp), // 10
	X86_REG_EBP = DREG(ebp), // 2
	X86_REG_ESI = DREG(esi), // 1
	X86_REG_EDI = DREG(edi)  // 0
#undef DREG
#undef OREG
#endif
};
#endif
#if defined(__sun__)
// Same as for Linux, need to check for x86-64
enum {
#if defined(__i386__)
	X86_REG_EIP = EIP,
	X86_REG_EAX = EAX,
	X86_REG_ECX = ECX,
	X86_REG_EDX = EDX,
	X86_REG_EBX = EBX,
	X86_REG_ESP = ESP,
	X86_REG_EBP = EBP,
	X86_REG_ESI = ESI,
	X86_REG_EDI = EDI
#endif
};
#endif
#if defined(__APPLE__) && defined(__MACH__)
enum {
#ifdef i386_SAVED_STATE
	// same as FreeBSD (in Open Darwin 8.0.1)
	X86_REG_EIP = 10,
	X86_REG_EAX = 7,
	X86_REG_ECX = 6,
	X86_REG_EDX = 5,
	X86_REG_EBX = 4,
	X86_REG_ESP = 13,
	X86_REG_EBP = 2,
	X86_REG_ESI = 1,
	X86_REG_EDI = 0
#else
	// new layout (MacOS X 10.4.4 for x86)
	X86_REG_EIP = 10,
	X86_REG_EAX = 0,
	X86_REG_ECX = 2,
	X86_REG_EDX = 4,
	X86_REG_EBX = 1,
	X86_REG_ESP = 7,
	X86_REG_EBP = 6,
	X86_REG_ESI = 5,
	X86_REG_EDI = 4
#endif
};
#endif
#if defined(_WIN32)
enum {
#if (defined(i386) || defined(__i386__))
	X86_REG_EIP = 7,
	X86_REG_EAX = 5,
	X86_REG_ECX = 4,
	X86_REG_EDX = 3,
	X86_REG_EBX = 2,
	X86_REG_ESP = 10,
	X86_REG_EBP = 6,
	X86_REG_ESI = 1,
	X86_REG_EDI = 0
#endif
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

static bool ix86_skip_instruction(unsigned long * regs)
{
	unsigned char * eip = (unsigned char *)regs[X86_REG_EIP];

	if (eip == 0)
		return false;
#ifdef _WIN32
	if (IsBadCodePtr((FARPROC)eip))
		return false;
#endif
	
	enum instruction_type_t {
		i_MOV,
		i_ADD
	};

	transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
	transfer_size_t transfer_size = SIZE_LONG;
	instruction_type_t instruction_type = i_MOV;
	
	int reg = -1;
	int len = 0;

#if DEBUG
	printf("IP: %p [%02x %02x %02x %02x...]\n",
		   eip, eip[0], eip[1], eip[2], eip[3]);
#endif

	// Operand size prefix
	if (*eip == 0x66) {
		eip++;
		len++;
		transfer_size = SIZE_WORD;
	}

	// REX prefix
#if defined(__x86_64__)
	struct rex_t {
		unsigned char W;
		unsigned char R;
		unsigned char X;
		unsigned char B;
	};
	rex_t rex = { 0, 0, 0, 0 };
	bool has_rex = false;
	if ((*eip & 0xf0) == 0x40) {
		has_rex = true;
		const unsigned char b = *eip;
		rex.W = b & (1 << 3);
		rex.R = b & (1 << 2);
		rex.X = b & (1 << 1);
		rex.B = b & (1 << 0);
#if DEBUG
		printf("REX: %c,%c,%c,%c\n",
			   rex.W ? 'W' : '_',
			   rex.R ? 'R' : '_',
			   rex.X ? 'X' : '_',
			   rex.B ? 'B' : '_');
#endif
		eip++;
		len++;
		if (rex.W)
			transfer_size = SIZE_QUAD;
	}
#else
	const bool has_rex = false;
#endif

	// Decode instruction
	int op_len = 1;
	int target_size = SIZE_UNKNOWN;
	switch (eip[0]) {
	case 0x0f:
		target_size = transfer_size;
	    switch (eip[1]) {
		case 0xbe: // MOVSX r32, r/m8
	    case 0xb6: // MOVZX r32, r/m8
			transfer_size = SIZE_BYTE;
			goto do_mov_extend;
		case 0xbf: // MOVSX r32, r/m16
	    case 0xb7: // MOVZX r32, r/m16
			transfer_size = SIZE_WORD;
			goto do_mov_extend;
		  do_mov_extend:
			op_len = 2;
			goto do_transfer_load;
		}
		break;
#if defined(__x86_64__)
	case 0x63: // MOVSXD r64, r/m32
		if (has_rex && rex.W) {
			transfer_size = SIZE_LONG;
			target_size = SIZE_QUAD;
		}
		else if (transfer_size != SIZE_WORD) {
			transfer_size = SIZE_LONG;
			target_size = SIZE_QUAD;
		}
		goto do_transfer_load;
#endif
	case 0x02: // ADD r8, r/m8
		transfer_size = SIZE_BYTE;
	case 0x03: // ADD r32, r/m32
		instruction_type = i_ADD;
		goto do_transfer_load;
	case 0x8a: // MOV r8, r/m8
		transfer_size = SIZE_BYTE;
	case 0x8b: // MOV r32, r/m32 (or 16-bit operation)
	  do_transfer_load:
		switch (eip[op_len] & 0xc0) {
		case 0x80:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		case 0x40:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		case 0x00:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_LOAD;
			break;
		}
		len += 1 + op_len + ix86_step_over_modrm(eip + op_len);
		break;
	case 0x00: // ADD r/m8, r8
		transfer_size = SIZE_BYTE;
	case 0x01: // ADD r/m32, r32
		instruction_type = i_ADD;
		goto do_transfer_store;
	case 0x88: // MOV r/m8, r8
		transfer_size = SIZE_BYTE;
	case 0x89: // MOV r/m32, r32 (or 16-bit operation)
	  do_transfer_store:
		switch (eip[op_len] & 0xc0) {
		case 0x80:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		case 0x40:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		case 0x00:
			reg = (eip[op_len] >> 3) & 7;
			transfer_type = SIGSEGV_TRANSFER_STORE;
			break;
		}
		len += 1 + op_len + ix86_step_over_modrm(eip + op_len);
		break;
	}
	if (target_size == SIZE_UNKNOWN)
		target_size = transfer_size;

	if (transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

#if defined(__x86_64__)
	if (rex.R)
		reg += 8;
#endif

	if (instruction_type == i_MOV && transfer_type == SIGSEGV_TRANSFER_LOAD && reg != -1) {
		static const int x86_reg_map[] = {
			X86_REG_EAX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBX,
			X86_REG_ESP, X86_REG_EBP, X86_REG_ESI, X86_REG_EDI,
#if defined(__x86_64__)
			X86_REG_R8,  X86_REG_R9,  X86_REG_R10, X86_REG_R11,
			X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15,
#endif
		};
		
		if (reg < 0 || reg >= (sizeof(x86_reg_map)/sizeof(x86_reg_map[0]) - 1))
			return false;

		// Set 0 to the relevant register part
		// NOTE: this is only valid for MOV alike instructions
		int rloc = x86_reg_map[reg];
		switch (target_size) {
		case SIZE_BYTE:
			if (has_rex || reg < 4)
				regs[rloc] = (regs[rloc] & ~0x00ffL);
			else {
				rloc = x86_reg_map[reg - 4];
				regs[rloc] = (regs[rloc] & ~0xff00L);
			}
			break;
		case SIZE_WORD:
			regs[rloc] = (regs[rloc] & ~0xffffL);
			break;
		case SIZE_LONG:
		case SIZE_QUAD: // zero-extension
			regs[rloc] = 0;
			break;
		}
	}

#if DEBUG
	printf("%p: %s %s access", (void *)regs[X86_REG_EIP],
		   transfer_size == SIZE_BYTE ? "byte" :
		   transfer_size == SIZE_WORD ? "word" :
		   transfer_size == SIZE_LONG ? "long" :
		   transfer_size == SIZE_QUAD ? "quad" : "unknown",
		   transfer_type == SIGSEGV_TRANSFER_LOAD ? "read" : "write");
	
	if (reg != -1) {
		static const char * x86_byte_reg_str_map[] = {
			"al",   "cl",   "dl",   "bl",
			"spl",  "bpl",  "sil",  "dil",
			"r8b",  "r9b",  "r10b", "r11b",
			"r12b", "r13b", "r14b", "r15b",
			"ah",   "ch",   "dh",   "bh",
		};
		static const char * x86_word_reg_str_map[] = {
			"ax",   "cx",   "dx",   "bx",
			"sp",   "bp",   "si",   "di",
			"r8w",  "r9w",  "r10w", "r11w",
			"r12w", "r13w", "r14w", "r15w",
		};
		static const char *x86_long_reg_str_map[] = {
			"eax",  "ecx",  "edx",  "ebx",
			"esp",  "ebp",  "esi",  "edi",
			"r8d",  "r9d",  "r10d", "r11d",
			"r12d", "r13d", "r14d", "r15d",
		};
		static const char *x86_quad_reg_str_map[] = {
			"rax", "rcx", "rdx", "rbx",
			"rsp", "rbp", "rsi", "rdi",
			"r8",  "r9",  "r10", "r11",
			"r12", "r13", "r14", "r15",
		};
		const char * reg_str = NULL;
		switch (target_size) {
		case SIZE_BYTE:
			reg_str = x86_byte_reg_str_map[(!has_rex && reg >= 4 ? 12 : 0) + reg];
			break;
		case SIZE_WORD: reg_str = x86_word_reg_str_map[reg]; break;
		case SIZE_LONG: reg_str = x86_long_reg_str_map[reg]; break;
		case SIZE_QUAD: reg_str = x86_quad_reg_str_map[reg]; break;
		}
		if (reg_str)
			printf(" %s register %%%s",
				   transfer_type == SIGSEGV_TRANSFER_LOAD ? "to" : "from",
				   reg_str);
	}
	printf(", %d bytes instruction\n", len);
#endif
	
	regs[X86_REG_EIP] += len;
	return true;
}
#endif

// Decode and skip PPC instruction
#if (defined(powerpc) || defined(__powerpc__) || defined(__ppc__))
static bool powerpc_skip_instruction(unsigned long * nip_p, unsigned long * regs)
{
	instruction_t instr;
	powerpc_decode_instruction(&instr, *nip_p, regs);
	
	if (instr.transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

#if DEBUG
	printf("%08x: %s %s access", *nip_p,
		   instr.transfer_size == SIZE_BYTE ? "byte" :
		   instr.transfer_size == SIZE_WORD ? "word" :
		   instr.transfer_size == SIZE_LONG ? "long" : "quad",
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

// Decode and skip MIPS instruction
#if (defined(mips) || defined(__mips))
enum {
#if (defined(sgi) || defined(__sgi))
  MIPS_REG_EPC = 35,
#endif
};
static bool mips_skip_instruction(greg_t * regs)
{
  unsigned int * epc = (unsigned int *)(unsigned long)regs[MIPS_REG_EPC];

  if (epc == 0)
	return false;

#if DEBUG
  printf("IP: %p [%08x]\n", epc, epc[0]);
#endif

  transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
  transfer_size_t transfer_size = SIZE_LONG;
  int direction = 0;

  const unsigned int opcode = epc[0];
  switch (opcode >> 26) {
  case 32: // Load Byte
  case 36: // Load Byte Unsigned
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_BYTE;
	break;
  case 33: // Load Halfword
  case 37: // Load Halfword Unsigned
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_WORD;
	break;
  case 35: // Load Word
  case 39: // Load Word Unsigned
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	break;
  case 34: // Load Word Left
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	direction = -1;
	break;
  case 38: // Load Word Right
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	direction = 1;
	break;
  case 55: // Load Doubleword
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_QUAD;
	break;
  case 26: // Load Doubleword Left
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_QUAD;
	direction = -1;
	break;
  case 27: // Load Doubleword Right
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_QUAD;
	direction = 1;
	break;
  case 40: // Store Byte
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_BYTE;
	break;
  case 41: // Store Halfword
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_WORD;
	break;
  case 43: // Store Word
  case 42: // Store Word Left
  case 46: // Store Word Right
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_LONG;
	break;
  case 63: // Store Doubleword
  case 44: // Store Doubleword Left
  case 45: // Store Doubleword Right
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_QUAD;
	break;
  /* Misc instructions unlikely to be used within CPU emulators */
  case 48: // Load Linked Word
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	break;
  case 52: // Load Linked Doubleword
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_QUAD;
	break;
  case 56: // Store Conditional Word
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_LONG;
	break;
  case 60: // Store Conditional Doubleword
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_QUAD;
	break;
  }

  if (transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
	// Unknown machine code, let it crash. Then patch the decoder
	return false;
  }

  // Zero target register in case of a load operation
  const int reg = (opcode >> 16) & 0x1f;
  if (transfer_type == SIGSEGV_TRANSFER_LOAD) {
	if (direction == 0)
	  regs[reg] = 0;
	else {
	  // FIXME: untested code
	  unsigned long ea = regs[(opcode >> 21) & 0x1f];
	  ea += (signed long)(signed int)(signed short)(opcode & 0xffff);
	  const int offset = ea & (transfer_size == SIZE_LONG ? 3 : 7);
	  unsigned long value;
	  if (direction > 0) {
		const unsigned long rmask = ~((1L << ((offset + 1) * 8)) - 1);
		value = regs[reg] & rmask;
	  }
	  else {
		const unsigned long lmask = (1L << (offset * 8)) - 1;
		value = regs[reg] & lmask;
	  }
	  // restore most significant bits
	  if (transfer_size == SIZE_LONG)
		value = (signed long)(signed int)value;
	  regs[reg] = value;
	}
  }

#if DEBUG
#if (defined(_ABIN32) || defined(_ABI64))
  static const char * mips_gpr_names[32] = {
	"zero", "at",   "v0",   "v1",   "a0",   "a1",   "a2",   "a3",
	"t0",   "t1",   "t2",   "t3",   "t4",   "t5",   "t6",   "t7",
	"s0",   "s1",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
	"t8",   "t9",   "k0",   "k1",   "gp",   "sp",   "s8",   "ra"
  };
#else
  static const char * mips_gpr_names[32] = {
	"zero", "at",   "v0",   "v1",   "a0",   "a1",   "a2",   "a3",
	"a4",   "a5",   "a6",   "a7",   "t0",   "t1",   "t2",   "t3",
	"s0",   "s1",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
	"t8",   "t9",   "k0",   "k1",   "gp",   "sp",   "s8",   "ra"
  };
#endif
  printf("%s %s register %s\n",
		 transfer_size == SIZE_BYTE ? "byte" :
		 transfer_size == SIZE_WORD ? "word" :
		 transfer_size == SIZE_LONG ? "long" :
		 transfer_size == SIZE_QUAD ? "quad" : "unknown",
		 transfer_type == SIGSEGV_TRANSFER_LOAD ? "load to" : "store from",
		 mips_gpr_names[reg]);
#endif

  regs[MIPS_REG_EPC] += 4;
  return true;
}
#endif

// Decode and skip SPARC instruction
#if (defined(sparc) || defined(__sparc__))
enum {
#if (defined(__sun__))
  SPARC_REG_G1 = REG_G1,
  SPARC_REG_O0 = REG_O0,
  SPARC_REG_PC = REG_PC,
  SPARC_REG_nPC = REG_nPC
#endif
};
static bool sparc_skip_instruction(unsigned long * regs, gwindows_t * gwins, struct rwindow * rwin)
{
  unsigned int * pc = (unsigned int *)regs[SPARC_REG_PC];

  if (pc == 0)
	return false;

#if DEBUG
  printf("IP: %p [%08x]\n", pc, pc[0]);
#endif

  transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
  transfer_size_t transfer_size = SIZE_LONG;
  bool register_pair = false;

  const unsigned int opcode = pc[0];
  if ((opcode >> 30) != 3)
	return false;
  switch ((opcode >> 19) & 0x3f) {
  case 9: // Load Signed Byte
  case 1: // Load Unsigned Byte
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_BYTE;
	break;
  case 10:// Load Signed Halfword
  case 2: // Load Unsigned Word
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_WORD;
	break;
  case 8: // Load Word
  case 0: // Load Unsigned Word
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	break;
  case 11:// Load Extended Word
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_QUAD;
	break;
  case 3: // Load Doubleword
	transfer_type = SIGSEGV_TRANSFER_LOAD;
	transfer_size = SIZE_LONG;
	register_pair = true;
	break;
  case 5: // Store Byte
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_BYTE;
	break;
  case 6: // Store Halfword
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_WORD;
	break;
  case 4: // Store Word
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_LONG;
	break;
  case 14:// Store Extended Word
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_QUAD;
	break;
  case 7: // Store Doubleword
	transfer_type = SIGSEGV_TRANSFER_STORE;
	transfer_size = SIZE_LONG;
	register_pair = true;
	break;
  }

  if (transfer_type == SIGSEGV_TRANSFER_UNKNOWN) {
	// Unknown machine code, let it crash. Then patch the decoder
	return false;
  }

  const int reg = (opcode >> 25) & 0x1f;

#if DEBUG
  static const char * reg_names[] = {
	"g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
	"o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
	"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
	"i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7"
  };
  printf("%s %s register %s\n",
		 transfer_size == SIZE_BYTE ? "byte" :
		 transfer_size == SIZE_WORD ? "word" :
		 transfer_size == SIZE_LONG ? "long" :
		 transfer_size == SIZE_QUAD ? "quad" : "unknown",
		 transfer_type == SIGSEGV_TRANSFER_LOAD ? "load to" : "store from",
		 reg_names[reg]);
#endif

  // Zero target register in case of a load operation
  if (transfer_type == SIGSEGV_TRANSFER_LOAD && reg != 0) {
	// FIXME: code to handle local & input registers is not tested
	if (reg >= 1 && reg < 8) {
	  // global registers
	  regs[reg - 1 + SPARC_REG_G1] = 0;
	}
	else if (reg >= 8 && reg < 16) {
	  // output registers
	  regs[reg - 8 + SPARC_REG_O0] = 0;
	}
	else if (reg >= 16 && reg < 24) {
	  // local registers (in register windows)
	  if (gwins)
		gwins->wbuf->rw_local[reg - 16] = 0;
	  else
		rwin->rw_local[reg - 16] = 0;
	}
	else {
	  // input registers (in register windows)
	  if (gwins)
		gwins->wbuf->rw_in[reg - 24] = 0;
	  else
		rwin->rw_in[reg - 24] = 0;
	}
  }

  regs[SPARC_REG_PC] += 4;
  regs[SPARC_REG_nPC] += 4;
  return true;
}
#endif
#endif

// Decode and skip ARM instruction
#if (defined(arm) || defined(__arm__))
enum {
#if (defined(__linux__))
  ARM_REG_PC = 15,
  ARM_REG_CPSR = 16
#endif
};
static bool arm_skip_instruction(unsigned long * regs)
{
  unsigned int * pc = (unsigned int *)regs[ARM_REG_PC];

  if (pc == 0)
	return false;

#if DEBUG
  printf("IP: %p [%08x]\n", pc, pc[0]);
#endif

  transfer_type_t transfer_type = SIGSEGV_TRANSFER_UNKNOWN;
  transfer_size_t transfer_size = SIZE_UNKNOWN;
  enum { op_sdt = 1, op_sdth = 2 };
  int op = 0;

  // Handle load/store instructions only
  const unsigned int opcode = pc[0];
  switch ((opcode >> 25) & 7) {
  case 0: // Halfword and Signed Data Transfer (LDRH, STRH, LDRSB, LDRSH)
	op = op_sdth;
	// Determine transfer size (S/H bits)
	switch ((opcode >> 5) & 3) {
	case 0: // SWP instruction
	  break;
	case 1: // Unsigned halfwords
	case 3: // Signed halfwords
	  transfer_size = SIZE_WORD;
	  break;
	case 2: // Signed byte
	  transfer_size = SIZE_BYTE;
	  break;
	}
	break;
  case 2:
  case 3: // Single Data Transfer (LDR, STR)
	op = op_sdt;
	// Determine transfer size (B bit)
	if (((opcode >> 22) & 1) == 1)
	  transfer_size = SIZE_BYTE;
	else
	  transfer_size = SIZE_LONG;
	break;
  default:
	// FIXME: support load/store mutliple?
	return false;
  }

  // Check for invalid transfer size (SWP instruction?)
  if (transfer_size == SIZE_UNKNOWN)
	return false;

  // Determine transfer type (L bit)
  if (((opcode >> 20) & 1) == 1)
	transfer_type = SIGSEGV_TRANSFER_LOAD;
  else
	transfer_type = SIGSEGV_TRANSFER_STORE;

  // Compute offset
  int offset;
  if (((opcode >> 25) & 1) == 0) {
	if (op == op_sdt)
	  offset = opcode & 0xfff;
	else if (op == op_sdth) {
	  int rm = opcode & 0xf;
	  if (((opcode >> 22) & 1) == 0) {
		// register offset
		offset = regs[rm];
	  }
	  else {
		// immediate offset
		offset = ((opcode >> 4) & 0xf0) | (opcode & 0x0f);
	  }
	}
  }
  else {
	const int rm = opcode & 0xf;
	const int sh = (opcode >> 7) & 0x1f;
	if (((opcode >> 4) & 1) == 1) {
	  // we expect only legal load/store instructions
	  printf("FATAL: invalid shift operand\n");
	  return false;
	}
	const unsigned int v = regs[rm];
	switch ((opcode >> 5) & 3) {
	case 0: // logical shift left
	  offset = sh ? v << sh : v;
	  break;
	case 1: // logical shift right
	  offset = sh ? v >> sh : 0;
	  break;
	case 2: // arithmetic shift right
	  if (sh)
		offset = ((signed int)v) >> sh;
	  else
		offset = (v & 0x80000000) ? 0xffffffff : 0;
	  break;
	case 3: // rotate right
	  if (sh)
		offset = (v >> sh) | (v << (32 - sh));
	  else
		offset = (v >> 1) | ((regs[ARM_REG_CPSR] << 2) & 0x80000000);
	  break;
	}
  }
  if (((opcode >> 23) & 1) == 0)
	offset = -offset;

  int rd = (opcode >> 12) & 0xf;
  int rn = (opcode >> 16) & 0xf;
#if DEBUG
  static const char * reg_names[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r9", "r9", "sl", "fp", "ip", "sp", "lr", "pc"
  };
  printf("%s %s register %s\n",
		 transfer_size == SIZE_BYTE ? "byte" :
		 transfer_size == SIZE_WORD ? "word" :
		 transfer_size == SIZE_LONG ? "long" : "unknown",
		 transfer_type == SIGSEGV_TRANSFER_LOAD ? "load to" : "store from",
		 reg_names[rd]);
#endif

  unsigned int base = regs[rn];
  if (((opcode >> 24) & 1) == 1)
	base += offset;

  if (transfer_type == SIGSEGV_TRANSFER_LOAD)
	regs[rd] = 0;

  if (((opcode >> 24) & 1) == 0)		// post-index addressing
	regs[rn] += offset;
  else if (((opcode >> 21) & 1) == 1)	// write-back address into base
	regs[rn] = base;

  regs[ARM_REG_PC] += 4;
  return true;
}
#endif


// Fallbacks
#ifndef SIGSEGV_FAULT_INSTRUCTION
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_INVALID_PC
#endif
#ifndef SIGSEGV_FAULT_HANDLER_ARGLIST_1
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	SIGSEGV_FAULT_HANDLER_ARGLIST
#endif
#ifndef SIGSEGV_FAULT_HANDLER_INVOKE
#define SIGSEGV_FAULT_HANDLER_INVOKE(ADDR, IP)	sigsegv_fault_handler(ADDR, IP)
#endif

// SIGSEGV recovery supported ?
#if defined(SIGSEGV_ALL_SIGNALS) && defined(SIGSEGV_FAULT_HANDLER_ARGLIST) && defined(SIGSEGV_FAULT_ADDRESS)
#define HAVE_SIGSEGV_RECOVERY
#endif


/*
 *  SIGSEGV global handler
 */

// This function handles the badaccess to memory.
// It is called from the signal handler or the exception handler.
static bool handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGLIST_1)
{
#ifdef HAVE_MACH_EXCEPTIONS
	// We must match the initial count when writing back the CPU state registers
	kern_return_t krc;
	mach_msg_type_number_t count;

	count = SIGSEGV_THREAD_STATE_COUNT;
	krc = thread_get_state(thread, SIGSEGV_THREAD_STATE_FLAVOR, (thread_state_t)state, &count);
	MACH_CHECK_ERROR (thread_get_state, krc);
#endif

	sigsegv_address_t fault_address = (sigsegv_address_t)SIGSEGV_FAULT_ADDRESS;
	sigsegv_address_t fault_instruction = (sigsegv_address_t)SIGSEGV_FAULT_INSTRUCTION;
	
	// Call user's handler and reinstall the global handler, if required
	switch (SIGSEGV_FAULT_HANDLER_INVOKE(fault_address, fault_instruction)) {
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
			krc = thread_set_state(thread,
								   SIGSEGV_THREAD_STATE_FLAVOR, (thread_state_t)state,
								   count);
			MACH_CHECK_ERROR (thread_set_state, krc);
#endif
			return true;
		}
		break;
#endif
	case SIGSEGV_RETURN_FAILURE:
		// We can't do anything with the fault_address, dump state?
		if (sigsegv_state_dumper != 0)
			sigsegv_state_dumper(fault_address, fault_instruction);
		break;
	}

	return false;
}


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
	thread_state_data_t thread_state;
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

	if (!VALID_THREAD_STATE_FLAVOR(flavor)) {
		fprintf(stderr, "Invalid thread_state flavor = %d. Not forwarding\n", flavor);
		return KERN_FAILURE;
	}

	/*
	 fprintf(stderr, "forwarding exception, port = 0x%x, behaviour = %d, flavor = %d\n", port, behavior, flavor);
	 */

	if (behavior != EXCEPTION_DEFAULT) {
		thread_state_count = THREAD_STATE_MAX;
		kret = thread_get_state (thread_port, flavor, (natural_t *)&thread_state,
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
								   (natural_t *)&thread_state, thread_state_count,
								   (natural_t *)&thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (exception_raise_state, kret);
	  break;
	case EXCEPTION_STATE_IDENTITY:
	  // fprintf(stderr, "forwarding to exception_raise_state_identity\n");
	  kret = exception_raise_state_identity(port, thread_port, task_port,
											exception_type, exception_data,
											data_count, &flavor,
											(natural_t *)&thread_state, thread_state_count,
											(natural_t *)&thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (exception_raise_state_identity, kret);
	  break;
	default:
	  fprintf(stderr, "forward_exception got unknown behavior\n");
	  kret = KERN_FAILURE;
	  break;
	}

	if (behavior != EXCEPTION_DEFAULT) {
		kret = thread_set_state (thread_port, flavor, (natural_t *)&thread_state,
								 thread_state_count);
		MACH_CHECK_ERROR (thread_set_state, kret);
	}

	return kret;
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
	SIGSEGV_THREAD_STATE_TYPE state;
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
				EXCEPTION_DEFAULT, SIGSEGV_THREAD_STATE_FLAVOR);
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

#ifdef HAVE_WIN32_EXCEPTIONS
static LONG WINAPI main_exception_filter(EXCEPTION_POINTERS *ExceptionInfo)
{
	if (sigsegv_fault_handler != NULL
		&& ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
		&& ExceptionInfo->ExceptionRecord->NumberParameters == 2
		&& handle_badaccess(ExceptionInfo))
		return EXCEPTION_CONTINUE_EXECUTION;

	return EXCEPTION_CONTINUE_SEARCH;
}

#if defined __CYGWIN__ && defined __i386__
/* In Cygwin programs, SetUnhandledExceptionFilter has no effect because Cygwin
   installs a global exception handler.  We have to dig deep in order to install
   our main_exception_filter.  */

/* Data structures for the current thread's exception handler chain.
   On the x86 Windows uses register fs, offset 0 to point to the current
   exception handler; Cygwin mucks with it, so we must do the same... :-/ */

/* Magic taken from winsup/cygwin/include/exceptions.h.  */

struct exception_list {
    struct exception_list *prev;
    int (*handler) (EXCEPTION_RECORD *, void *, CONTEXT *, void *);
};
typedef struct exception_list exception_list;

/* Magic taken from winsup/cygwin/exceptions.cc.  */

__asm__ (".equ __except_list,0");

extern exception_list *_except_list __asm__ ("%fs:__except_list");

/* For debugging.  _except_list is not otherwise accessible from gdb.  */
static exception_list *
debug_get_except_list ()
{
  return _except_list;
}

/* Cygwin's original exception handler.  */
static int (*cygwin_exception_handler) (EXCEPTION_RECORD *, void *, CONTEXT *, void *);

/* Our exception handler.  */
static int
libsigsegv_exception_handler (EXCEPTION_RECORD *exception, void *frame, CONTEXT *context, void *dispatch)
{
  EXCEPTION_POINTERS ExceptionInfo;
  ExceptionInfo.ExceptionRecord = exception;
  ExceptionInfo.ContextRecord = context;
  if (main_exception_filter (&ExceptionInfo) == EXCEPTION_CONTINUE_SEARCH)
    return cygwin_exception_handler (exception, frame, context, dispatch);
  else
    return 0;
}

static void
do_install_main_exception_filter ()
{
  /* We cannot insert any handler into the chain, because such handlers
     must lie on the stack (?).  Instead, we have to replace(!) Cygwin's
     global exception handler.  */
  cygwin_exception_handler = _except_list->handler;
  _except_list->handler = libsigsegv_exception_handler;
}

#else

static void
do_install_main_exception_filter ()
{
  SetUnhandledExceptionFilter ((LPTOP_LEVEL_EXCEPTION_FILTER) &main_exception_filter);
}
#endif

static bool sigsegv_do_install_handler(sigsegv_fault_handler_t handler)
{
	static bool main_exception_filter_installed = false;
	if (!main_exception_filter_installed) {
		do_install_main_exception_filter();
		main_exception_filter_installed = true;
	}
	sigsegv_fault_handler = handler;
	return true;
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
#elif defined(HAVE_MACH_EXCEPTIONS) || defined(HAVE_WIN32_EXCEPTIONS)
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
#ifdef HAVE_WIN32_EXCEPTIONS
	sigsegv_fault_handler = NULL;
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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "vm_alloc.h"

const int REF_INDEX = 123;
const int REF_VALUE = 45;

static int page_size;
static volatile char * page = 0;
static volatile int handler_called = 0;

/* Barriers */
#ifdef __GNUC__
#define BARRIER() asm volatile ("" : : : "memory")
#else
#define BARRIER() /* nothing */
#endif

#ifdef __GNUC__
// Code range where we expect the fault to come from
static void *b_region, *e_region;
#endif

static sigsegv_return_t sigsegv_test_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
#if DEBUG
	printf("sigsegv_test_handler(%p, %p)\n", fault_address, instruction_address);
	printf("expected fault at %p\n", page + REF_INDEX);
#ifdef __GNUC__
	printf("expected instruction address range: %p-%p\n", b_region, e_region);
#endif
#endif
	handler_called++;
	if ((fault_address - REF_INDEX) != page)
		exit(10);
#ifdef __GNUC__
	// Make sure reported fault instruction address falls into
	// expected code range
	if (instruction_address != SIGSEGV_INVALID_PC
		&& ((instruction_address <  (sigsegv_address_t)b_region) ||
			(instruction_address >= (sigsegv_address_t)e_region)))
		exit(11);
#endif
	if (vm_protect((char *)((unsigned long)fault_address & -page_size), page_size, VM_PAGE_READ | VM_PAGE_WRITE) != 0)
		exit(12);
	return SIGSEGV_RETURN_SUCCESS;
}

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
static sigsegv_return_t sigsegv_insn_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
#if DEBUG
	printf("sigsegv_insn_handler(%p, %p)\n", fault_address, instruction_address);
#endif
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

// More sophisticated tests for instruction skipper
static bool arch_insn_skipper_tests()
{
#if (defined(i386) || defined(__i386__)) || defined(__x86_64__)
	static const unsigned char code[] = {
		0x8a, 0x00,                    // mov    (%eax),%al
		0x8a, 0x2c, 0x18,              // mov    (%eax,%ebx,1),%ch
		0x88, 0x20,                    // mov    %ah,(%eax)
		0x88, 0x08,                    // mov    %cl,(%eax)
		0x66, 0x8b, 0x00,              // mov    (%eax),%ax
		0x66, 0x8b, 0x0c, 0x18,        // mov    (%eax,%ebx,1),%cx
		0x66, 0x89, 0x00,              // mov    %ax,(%eax)
		0x66, 0x89, 0x0c, 0x18,        // mov    %cx,(%eax,%ebx,1)
		0x8b, 0x00,                    // mov    (%eax),%eax
		0x8b, 0x0c, 0x18,              // mov    (%eax,%ebx,1),%ecx
		0x89, 0x00,                    // mov    %eax,(%eax)
		0x89, 0x0c, 0x18,              // mov    %ecx,(%eax,%ebx,1)
#if defined(__x86_64__)
		0x44, 0x8a, 0x00,              // mov    (%rax),%r8b
		0x44, 0x8a, 0x20,              // mov    (%rax),%r12b
		0x42, 0x8a, 0x3c, 0x10,        // mov    (%rax,%r10,1),%dil
		0x44, 0x88, 0x00,              // mov    %r8b,(%rax)
		0x44, 0x88, 0x20,              // mov    %r12b,(%rax)
		0x42, 0x88, 0x3c, 0x10,        // mov    %dil,(%rax,%r10,1)
		0x66, 0x44, 0x8b, 0x00,        // mov    (%rax),%r8w
		0x66, 0x42, 0x8b, 0x0c, 0x10,  // mov    (%rax,%r10,1),%cx
		0x66, 0x44, 0x89, 0x00,        // mov    %r8w,(%rax)
		0x66, 0x42, 0x89, 0x0c, 0x10,  // mov    %cx,(%rax,%r10,1)
		0x44, 0x8b, 0x00,              // mov    (%rax),%r8d
		0x42, 0x8b, 0x0c, 0x10,        // mov    (%rax,%r10,1),%ecx
		0x44, 0x89, 0x00,              // mov    %r8d,(%rax)
		0x42, 0x89, 0x0c, 0x10,        // mov    %ecx,(%rax,%r10,1)
		0x48, 0x8b, 0x08,              // mov    (%rax),%rcx
		0x4c, 0x8b, 0x18,              // mov    (%rax),%r11
		0x4a, 0x8b, 0x0c, 0x10,        // mov    (%rax,%r10,1),%rcx
		0x4e, 0x8b, 0x1c, 0x10,        // mov    (%rax,%r10,1),%r11
		0x48, 0x89, 0x08,              // mov    %rcx,(%rax)
		0x4c, 0x89, 0x18,              // mov    %r11,(%rax)
		0x4a, 0x89, 0x0c, 0x10,        // mov    %rcx,(%rax,%r10,1)
		0x4e, 0x89, 0x1c, 0x10,        // mov    %r11,(%rax,%r10,1)
		0x63, 0x47, 0x04,              // movslq 4(%rdi),%eax
		0x48, 0x63, 0x47, 0x04,        // movslq 4(%rdi),%rax
#endif
		0                              // end
	};
	const int N_REGS = 20;
	unsigned long regs[N_REGS];
	for (int i = 0; i < N_REGS; i++)
		regs[i] = i;
	const unsigned long start_code = (unsigned long)&code;
	regs[X86_REG_EIP] = start_code;
	while ((regs[X86_REG_EIP] - start_code) < (sizeof(code) - 1)
		   && ix86_skip_instruction(regs))
		; /* simply iterate */
	return (regs[X86_REG_EIP] - start_code) == (sizeof(code) - 1);
#endif
	return true;
}
#endif

int main(void)
{
	if (vm_init() < 0)
		return 1;

	page_size = vm_get_page_size();
	if ((page = (char *)vm_acquire(page_size)) == VM_MAP_FAILED)
		return 2;
	
	memset((void *)page, 0, page_size);
	if (vm_protect((char *)page, page_size, VM_PAGE_READ) < 0)
		return 3;
	
	if (!sigsegv_install_handler(sigsegv_test_handler))
		return 4;
	
#ifdef __GNUC__
	b_region = &&L_b_region1;
	e_region = &&L_e_region1;
#endif
 L_b_region1:
	page[REF_INDEX] = REF_VALUE;
	if (page[REF_INDEX] != REF_VALUE)
	  exit(20);
	page[REF_INDEX] = REF_VALUE;
	BARRIER();
 L_e_region1:

	if (handler_called != 1)
		return 5;

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	if (!sigsegv_install_handler(sigsegv_insn_handler))
		return 6;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_READ | VM_PAGE_WRITE) < 0)
		return 7;
	
	for (int i = 0; i < page_size; i++)
		page[i] = (i + 1) % page_size;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_NOACCESS) < 0)
		return 8;
	
#define TEST_SKIP_INSTRUCTION(TYPE) do {				\
		const unsigned long TAG = 0x12345678 |			\
		(sizeof(long) == 8 ? 0x9abcdef0UL << 31 : 0);	\
		TYPE data = *((TYPE *)(page + sizeof(TYPE)));	\
		volatile unsigned long effect = data + TAG;		\
		if (effect != TAG)								\
			return 9;									\
	} while (0)
	
#ifdef __GNUC__
	b_region = &&L_b_region2;
	e_region = &&L_e_region2;
#endif
 L_b_region2:
	TEST_SKIP_INSTRUCTION(unsigned char);
	TEST_SKIP_INSTRUCTION(unsigned short);
	TEST_SKIP_INSTRUCTION(unsigned int);
	TEST_SKIP_INSTRUCTION(unsigned long);
	TEST_SKIP_INSTRUCTION(signed char);
	TEST_SKIP_INSTRUCTION(signed short);
	TEST_SKIP_INSTRUCTION(signed int);
	TEST_SKIP_INSTRUCTION(signed long);
	BARRIER();
 L_e_region2:

	if (!arch_insn_skipper_tests())
		return 20;
#endif

	vm_exit();
	return 0;
}
#endif
