/*
 *  sigsegv.cpp - SIGSEGV signals support
 *
 *  Derived from Bruno Haible's work on his SIGSEGV library for clisp
 *  <http://clisp.sourceforge.net/>
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

#include <signal.h>
#include "sigsegv.h"

// Return value type of a signal handler (standard type if not defined)
#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

// Type of the system signal handler
typedef RETSIGTYPE (*signal_handler)(int);

// User's SIGSEGV handler
static sigsegv_handler_t sigsegv_user_handler = 0;

// Actual SIGSEGV handler installer
static bool sigsegv_do_install_handler(int sig);


/*
 *  OS-dependant SIGSEGV signals support section
 */

#if HAVE_SIGINFO_T
// Generic extended signal handler
#if defined(__NetBSD__) || defined(__FreeBSD__)
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#else
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_ADDRESS			sip->si_addr
#if defined(__linux__)
#if (defined(i386) || defined(__i386__))
#include <sys/ucontext.h>
#define SIGSEGV_FAULT_INSTRUCTION		(((ucontext_t *)scp)->uc_mcontext.gregs[14]) /* should use REG_EIP instead */
#endif
#if (defined(ia64) || defined(__ia64__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_ip & ~0x3ULL) /* slot number is in bits 0 and 1 */
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <sys/ucontext.h>
#define SIGSEGV_FAULT_INSTRUCTION		(((ucontext_t *)scp)->uc_mcontext.regs->nip)
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
#define SIGSEGV_FAULT_ADDRESS			scs.cr2
#define SIGSEGV_FAULT_INSTRUCTION		scs.eip
#endif
#if (defined(sparc) || defined(__sparc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp, char *addr
#define SIGSEGV_FAULT_ADDRESS			addr
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->regs->dar
#define SIGSEGV_FAULT_INSTRUCTION		scp->regs->nip
#endif
#if (defined(alpha) || defined(__alpha__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
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
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_badvaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// OSF/1 on Alpha
#if defined(__osf__)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_traparg_a0
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// AIX
#if defined(_AIX)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_jmpbuf.jmp_context.o_vaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// NetBSD or FreeBSD
#if defined(__NetBSD__) || defined(__FreeBSD__)
#if (defined(m68k) || defined(__m68k__))
#include <m68k/frame.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			({																\
	struct sigstate {																					\
		int ss_flags;																					\
		struct frame ss_frame;																			\
	};																									\
	struct sigstate *state = (struct sigstate *)scp->sc_ap;												\
	char *fault_addr;																					\
	switch (state->ss_frame.f_format) {																	\
	case 7:		/* 68040 access error */																\
		/* "code" is sometimes unreliable (i.e. contains NULL or a bogus address), reason unknown */	\
		fault_addr = state->ss_frame.f_fmt7.f_fa;														\
		break;																							\
	default:																							\
		fault_addr = (char *)code;																		\
		break;																							\
	}																									\
	fault_addr;																							\
})
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#else
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, void *scp, char *addr
#define SIGSEGV_FAULT_ADDRESS			addr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)
#endif
#endif

// MacOS X
#if defined(__APPLE__) && defined(__MACH__)
#if (defined(ppc) || defined(__ppc__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			get_fault_address(scp)
#define SIGSEGV_FAULT_INSTRUCTION		scp->sc_ir
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGBUS)

// From Boehm's GC 6.0alpha8
#define EXTRACT_OP1(iw)     (((iw) & 0xFC000000) >> 26)
#define EXTRACT_OP2(iw)     (((iw) & 0x000007FE) >> 1)
#define EXTRACT_REGA(iw)    (((iw) & 0x001F0000) >> 16)
#define EXTRACT_REGB(iw)    (((iw) & 0x03E00000) >> 21)
#define EXTRACT_REGC(iw)    (((iw) & 0x0000F800) >> 11)
#define EXTRACT_DISP(iw)    ((short *) &(iw))[1]

static sigsegv_address_t get_fault_address(struct sigcontext *scp)
{
	unsigned int   instr = *((unsigned int *) scp->sc_ir);
	unsigned int * regs = &((unsigned int *) scp->sc_regs)[2];
	int            disp = 0, tmp;
	unsigned int   baseA = 0, baseB = 0;
	unsigned int   addr, alignmask = 0xFFFFFFFF;

	switch(EXTRACT_OP1(instr)) {
	case 38:   /* stb */
	case 39:   /* stbu */
	case 54:   /* stfd */
	case 55:   /* stfdu */
	case 52:   /* stfs */
	case 53:   /* stfsu */
	case 44:   /* sth */
	case 45:   /* sthu */
	case 47:   /* stmw */
	case 36:   /* stw */
	case 37:   /* stwu */
		tmp = EXTRACT_REGA(instr);
		if(tmp > 0)
			baseA = regs[tmp];
		disp = EXTRACT_DISP(instr);
		break;
	case 31:
		switch(EXTRACT_OP2(instr)) {
		case 86:    /* dcbf */
		case 54:    /* dcbst */
		case 1014:  /* dcbz */
		case 247:   /* stbux */
		case 215:   /* stbx */
		case 759:   /* stfdux */
		case 727:   /* stfdx */
		case 983:   /* stfiwx */
		case 695:   /* stfsux */
		case 663:   /* stfsx */
		case 918:   /* sthbrx */
		case 439:   /* sthux */
		case 407:   /* sthx */
		case 661:   /* stswx */
		case 662:   /* stwbrx */
		case 150:   /* stwcx. */
		case 183:   /* stwux */
		case 151:   /* stwx */
		case 135:   /* stvebx */
		case 167:   /* stvehx */
		case 199:   /* stvewx */
		case 231:   /* stvx */
		case 487:   /* stvxl */
			tmp = EXTRACT_REGA(instr);
			if(tmp > 0)
				baseA = regs[tmp];
			baseB = regs[EXTRACT_REGC(instr)];
			/* determine Altivec alignment mask */
			switch(EXTRACT_OP2(instr)) {
			case 167:   /* stvehx */
				alignmask = 0xFFFFFFFE;
				break;
			case 199:   /* stvewx */
				alignmask = 0xFFFFFFFC;
				break;
			case 231:   /* stvx */
				alignmask = 0xFFFFFFF0;
				break;
			case 487:  /* stvxl */
				alignmask = 0xFFFFFFF0;
				break;
			}
			break;
		case 725:   /* stswi */
			tmp = EXTRACT_REGA(instr);
			if(tmp > 0)
				baseA = regs[tmp];
			break;
		default:   /* ignore instruction */
			return 0;
			break;
		}
		break;
	default:   /* ignore instruction */
		return 0;
		break;
	}
	
	addr = (baseA + baseB) + disp;
	addr &= alignmask;
	return (sigsegv_address_t)addr;
}
#endif
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

#ifdef HAVE_SIGSEGV_RECOVERY
static void sigsegv_handler(SIGSEGV_FAULT_HANDLER_ARGLIST)
{
	// Call user's handler and reinstall the global handler, if required
	if (sigsegv_user_handler((sigsegv_address_t)SIGSEGV_FAULT_ADDRESS, (sigsegv_address_t)SIGSEGV_FAULT_INSTRUCTION)) {
#if (defined(HAVE_SIGACTION) ? defined(SIGACTION_NEED_REINSTALL) : defined(SIGNAL_NEED_REINSTALL))
		sigsegv_do_install_handler(sig);
#endif
	}
	else {
		// FAIL: reinstall default handler for "safe" crash
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
		SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
	}
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
	struct sigaction vosf_sa;
	sigemptyset(&vosf_sa.sa_mask);
	vosf_sa.sa_sigaction = sigsegv_handler;
	vosf_sa.sa_flags = SA_SIGINFO;
	return (sigaction(sig, &vosf_sa, 0) == 0);
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
	struct sigaction vosf_sa;
	sigemptyset(&vosf_sa.sa_mask);
	vosf_sa.sa_handler = (signal_handler)sigsegv_handler;
#if !EMULATED_68K && defined(__NetBSD__)
	sigaddset(&vosf_sa.sa_mask, SIGALRM);
	vosf_sa.sa_flags = SA_ONSTACK;
#else
	vosf_sa.sa_flags = 0;
#endif
	return (sigaction(sig, &vosf_sa, 0) == 0);
#else
	return (signal(sig, (signal_handler)sigsegv_handler) != SIG_ERR);
#endif
}
#endif

bool sigsegv_install_handler(sigsegv_handler_t handler)
{
#ifdef HAVE_SIGSEGV_RECOVERY
	sigsegv_user_handler = handler;
	bool success = true;
#define FAULT_HANDLER(sig) success = success && sigsegv_do_install_handler(sig);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
	return success;
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
#ifdef HAVE_SIGSEGV_RECOVERY
	sigsegv_user_handler = 0;
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
#endif
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

static bool sigsegv_test_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	handler_called++;
	if ((fault_address - 123) != page)
		exit(1);
	if (vm_protect((char *)((unsigned long)fault_address & -page_size), page_size, VM_PAGE_READ | VM_PAGE_WRITE) != 0)
		exit(1);
	return true;
}

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

	vm_exit();
	return 0;
}
#endif
