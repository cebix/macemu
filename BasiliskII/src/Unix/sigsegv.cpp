/*
 *  sigsegv.cpp - SIGSEGV signals support
 *
 *  Derived from Bruno Haible's work on his SIGSEGV library for clisp
 *  <http://clisp.sourceforge.net/>
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, siginfo_t *sip, void *
#define SIGSEGV_FAULT_ADDRESS			sip->si_addr
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
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext* scp, char* addr
#define SIGSEGV_FAULT_ADDRESS			addr
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <asm/sigcontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, struct sigcontext* scp
#define SIGSEGV_FAULT_ADDRESS			scp->regs->dar
#define SIGSEGV_FAULT_INSTRUCTION		scp->regs->nip
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

#ifdef CONFIGURE_TEST
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static int page_size;
static volatile char * page = 0;
static volatile int handler_called = 0;

static bool sigsegv_test_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	handler_called++;
	if ((fault_address - 123) != page)
		exit(1);
	if (mprotect((char *)((unsigned long)fault_address & -page_size), page_size, PROT_READ | PROT_WRITE) != 0)
		exit(1);
	return true;
}

int main(void)
{
	int zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0)
		return 1;

	page_size = getpagesize();
   	page = (char *)mmap(0, page_size, PROT_READ, MAP_PRIVATE, zero_fd, 0);
	if (page == MAP_FAILED)
		return 1;
	
	if (!sigsegv_install_handler(sigsegv_test_handler))
		return 1;
	
	page[123] = 45;
	page[123] = 45;
	
	if (handler_called != 1)
		return 1;

	return 0;
}
#endif
