/*
 *  paranoia.cpp - Check undocumented features of the Linux kernel that
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

#include "sysdeps.h"
#include "main.h"
#include "user_strings.h"

typedef struct {
	uint32 u[4];
} __attribute((aligned(16))) vector128;
#include <linux/elf.h>

#define DEBUG 1
#include "debug.h"


// Constants
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack

// Prototypes
extern "C" void *get_sp(void);
extern "C" void *get_r2(void);
extern "C" void set_r2(void *);
extern "C" void *get_r13(void);
extern void paranoia_check(void);
static void sigusr2_handler(int sig, sigcontext_struct *sc);

// Global variables
static void *sig_stack = NULL;

static int err = 0;
static void *sig_sp = NULL;
static void *sig_r4 = NULL;
static int sig_sc_signal = 0;
static void *sig_sc_regs = NULL;
static uint32 sig_r2 = 0;


int raise(int sig)
{
	// Reimplement to get rid of access to r2 (TLS pointer)
	return kill(getpid(), sig);
}

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

	// Install SIGUSR2 signal handler
	static struct sigaction old_action;
	static struct sigaction sigusr2_action;
	sigemptyset(&sigusr2_action.sa_mask);
	sigusr2_action.sa_handler = (__sighandler_t)sigusr2_handler;
	sigusr2_action.sa_flags = SA_ONSTACK | SA_RESTART;
	sigusr2_action.sa_restorer = NULL;
	if (sigaction(SIGUSR2, &sigusr2_action, &old_action) < 0) {
		sprintf(str, GetString(STR_SIGUSR2_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		exit(1);
	}

	// Raise SIGUSR2
	TOC = get_r2();
	R13 = get_r13();
	set_r2((void *)0xaffebad5);
	raise(SIGUSR2);
	if (TOC != get_r2())
		err = 6;
	if (R13 != get_r13())
		err = 7;

	// Check error code
	switch (err) {
		case 1:
			printf("FATAL: sigaltstack() doesn't seem to work (sp in signal handler was %08lx, expected %08lx..%08lx)\n", (uint32)sig_sp, (uint32)sig_stack, (uint32)sig_stack + SIG_STACK_SIZE);
			break;
		case 2:
			printf("FATAL: r4 in signal handler (%08lx) doesn't point to stack\n", (uint32)sig_r4);
			break;
		case 3:
			printf("FATAL: r4 in signal handler doesn't seem to point to a sigcontext_struct (signal number was %d, expected %d)", sig_sc_signal, SIGUSR2);
			break;
		case 4:
			printf("FATAL: sc->regs in signal handler (%08lx) doesn't point to stack\n", (uint32)sig_sc_regs);
			break;
		case 5:
			printf("FATAL: sc->regs->gpr[2] in signal handler (%08lx) doesn't have expected value (%08x)\n", (uint32)sig_r2, 0xaffebad5);
			break;
		case 6:
			printf("FATAL: signal handler failed to restore initial r2 value (%08x, was %08x)\n", (uint32)get_r2(), (uint32)TOC);
			break;
		case 7:
			printf("FATAL: signal handler failed to restore initial r13 value (%08x, was %08x)\n", get_r13(), (uint32)R13);
	}
	if (err) {
		printf("Maybe you need a different kernel?\n");
		exit(1);
	}

	// Clean up
	D(bug("...passed\n"));
	sigaction(SIGUSR2, &old_action, NULL);
	sigaltstack(&old_stack, NULL);
	free(sig_stack);
}


static void sigusr2_handler(int sig, sigcontext_struct *sc)
{
	// Check whether sigaltstack works
	sig_sp = get_sp();
	if (sig_sp < sig_stack || sig_sp >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 1;
		goto ret;
	}

	// Check whether r4 points to info on the stack
	sig_r4 = sc;
	if (sig_r4 < sig_stack || sig_r4 >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 2;
		goto ret;
	}

	// Check whether r4 looks like a sigcontext
	sig_sc_signal = sc->signal;
	if (sig_sc_signal != SIGUSR2) {
		err = 3;
		goto ret;
	}

	// Check whether sc->regs points to info on the stack
	sig_sc_regs = sc->regs;
	if (sig_sc_regs < sig_stack || sig_sc_regs >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 4;
		goto ret;
	}

	// Check whether r2 still holds the value we set it to
	sig_r2 = sc->regs->gpr[2];
	if (sig_r2 != 0xaffebad5) {
		err = 5;
		goto ret;
	}

	// Restore pointer to Thread Local Storage
  ret:
#ifdef SYSTEM_CLOBBERS_R2
	sc->regs->gpr[2] = (unsigned long)TOC;
#endif
}
