/*
 *  paranoia.cpp - Check undocumented features of the NetBSD kernel that
 *                 SheepShaver relies upon
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

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/ucontext.h>

#include "sysdeps.h"
#include "main.h"
#include "user_strings.h"

#define DEBUG 1
#include "debug.h"


// Constants
const int SIG_IRQ = SIGUSR2;				// Signal to trigger on interrupt
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack

// Prototypes
extern "C" void *get_sp(void);
extern "C" void set_r2(uint32 val);
extern void paranoia_check(void);
static void sigusr2_handler(int sig, siginfo_t *sip, void *scp);

// Global variables
static void *sig_stack = NULL;

static int err = 0;
static void *sig_sp = NULL;
static void *sig_r4 = NULL;
static void *sig_sc_regs = NULL;
static uint32 sig_r2 = 0;


void paranoia_check(void)
{
	char str[256];

	D(bug("Paranoia checks...\n"));

	// Create and install stack for signal handler
	sig_stack = malloc(SIG_STACK_SIZE);
	if (sig_stack == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		exit(1);
	}

	struct sigaltstack old_stack;
	struct sigaltstack new_stack;
	new_stack.ss_sp = sig_stack;
	new_stack.ss_flags = 0;
	new_stack.ss_size = SIG_STACK_SIZE;
	if (sigaltstack(&new_stack, &old_stack) < 0) {
		sprintf(str, GetString(STR_SIGALTSTACK_ERR), strerror(errno));
		ErrorAlert(str);
		exit(1);
	}

	// Install SIG_IRQ signal handler
	static struct sigaction old_action;
	static struct sigaction sigusr2_action;
	sigemptyset(&sigusr2_action.sa_mask);
	sigusr2_action.sa_sigaction = sigusr2_handler;
	sigusr2_action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
	if (sigaction(SIG_IRQ, &sigusr2_action, &old_action) < 0) {
		sprintf(str, GetString(STR_SIGUSR2_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		exit(1);
	}

	// Raise SIG_IRQ
	set_r2(0xaffebad5);
	raise(SIG_IRQ);

	// Check error code
	switch (err) {
		case 1:
			printf("FATAL: sigaltstack() doesn't seem to work (sp in signal handler was %08lx, expected %08lx..%08lx)\n", (uint32)sig_sp, (uint32)sig_stack, (uint32)sig_stack + SIG_STACK_SIZE);
			break;
		case 2:
			printf("FATAL: r4 in signal handler (%08lx) doesn't point to stack\n", (uint32)sig_r4);
			break;
		case 4:
			printf("FATAL: sc->regs in signal handler (%08lx) doesn't point to stack\n", (uint32)sig_sc_regs);
			break;
		case 5:
			printf("FATAL: sc->regs->gpr[2] in signal handler (%08lx) doesn't have expected value (%08x)\n", (uint32)sig_r2, 0xaffebad5);
			break;
	}
	if (err) {
		printf("Maybe you need a different kernel?\n");
		exit(1);
	}

	// Clean up
	D(bug("...passed\n"));
	sigaction(SIG_IRQ, &old_action, NULL);
	sigaltstack(&old_stack, NULL);
	free(sig_stack);
}

static void sigusr2_handler(int sig, siginfo_t *sip, void *scp)
{
	ucontext_t *ucp = (ucontext_t *)scp;
	D(bug("SIGUSR2 handler caught\n"));

	// Check whether sigaltstack works
	D(bug(" check whether sigaltstack() works\n"));
	sig_sp = get_sp();
	if (sig_sp < sig_stack || sig_sp >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 1;
		return;
	}

	// Check whether r4 points to info on the stack
	D(bug(" check whether r4 points to info on the stack\n"));
	sig_r4 = ucp;
	if (sig_r4 < sig_stack || sig_r4 >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 2;
		return;
	}

	// Check whether context regs points to info on the stack
	D(bug(" check whether context regs points to info on the stack\n"));
	sig_sc_regs = ucp->uc_mcontext.__gregs;
	if (sig_sc_regs < sig_stack || sig_sc_regs >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 4;
		return;
	}

	// Check whether r2 still holds the value we set it to
	D(bug(" check whether r2 still holds the value we set it to\n"));
	sig_r2 = ucp->uc_mcontext.__gregs[_REG_R2];
	if (sig_r2 != 0xaffebad5) {
		err = 5;
		return;
	}
}
