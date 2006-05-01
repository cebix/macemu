/*
 *  paranoia.cpp - Check undocumented features of the underlying
 *                 kernel that SheepShaver relies upon
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

/*
 * TODO
 * - Check for nested signal handlers vs. sigaltstack()
 */

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ucontext.h>

#include "sysdeps.h"
#include "sigregs.h"
#include "main.h"
#include "user_strings.h"

#define DEBUG 1
#include "debug.h"


// Constants
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack

// Prototypes
extern "C" void *get_sp(void);
extern "C" void *get_r2(void);
extern "C" void set_r2(void *);
extern "C" void *get_r13(void);
extern "C" void set_r13(void *);
extern void paranoia_check(void);
static void sigusr2_handler(int sig, siginfo_t *sip, void *scp);
static void *tick_func(void *);
static void *emul_func(void *);

// Global variables
static void *sig_stack = NULL;

static int err = 0;
static void *sig_sp = NULL;
static void *sig_r5 = NULL;
static int sig_sc_signal = 0;
static void *sig_sc_regs = NULL;
static uint32 sig_r2 = 0;

static pthread_t tick_thread;
static pthread_t emul_thread;
static volatile uint32 tick_thread_ready = 0;
static volatile uint32 emul_thread_regs[32] = { 0, };
static volatile uint32 &emul_thread_ready = emul_thread_regs[0];


void paranoia_check(void)
{
	char str[256];

	printf("Paranoia checks...\n");

	// Create and install stack for signal handler
	sig_stack = malloc(SIG_STACK_SIZE);
	if (sig_stack == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		exit(1);
	}

	struct sigaltstack old_stack;
	struct sigaltstack new_stack;
	new_stack.ss_sp = (char *)sig_stack;
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
	sigusr2_action.sa_sigaction = sigusr2_handler;
	sigusr2_action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
	if (sigaction(SIGUSR2, &sigusr2_action, &old_action) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGUSR2", strerror(errno));
		ErrorAlert(str);
		exit(1);
	}

	// Start tick thread that will trigger only one SIGUSR2
	pthread_create(&tick_thread, NULL, tick_func, NULL);

	// Get my thread ID and execute MacOS thread function
	emul_thread = pthread_self();
	emul_func(NULL);

	// Check error code
	switch (err) {
		case 1:
			printf("FATAL: sigaltstack() doesn't seem to work (sp in signal handler was %p, expected %p..%p)\n", sig_sp, sig_stack, (uintptr)sig_stack + SIG_STACK_SIZE);
			break;
		case 2:
			printf("FATAL: r5 in signal handler (%p) doesn't point to stack\n", sig_r5);
			break;
		case 3:
			printf("FATAL: machine registers in signal handler (%p) doesn't point to stack\n", sig_sc_regs);
			break;
		case 4:
			printf("FATAL: register file in signal handler is corrupted\n");
			break;
	}
	if (err) {
		printf("Maybe you need a different kernel?\n");
		exit(1);
	}

	// Clean up
	printf("...passed\n");
	sigaction(SIGUSR2, &old_action, NULL);
	sigaltstack(&old_stack, NULL);
	free(sig_stack);
}

static void *tick_func(void *)
{
	tick_thread_ready = true;

	// Wait for emul thread to initialize
	D(bug("[tick_thread] waiting for emul thread to initialize\n"));
	while (!emul_thread_ready)
		usleep(0);

	// Trigger interrupt and terminate
	D(bug("[tick_thread] trigger interrupt\n"));
	pthread_kill(emul_thread, SIGUSR2);
	return NULL;
}

static void *emul_func(void *)
{
	// Wait for tick thread to initialize
	D(bug("[emul_thread] waiting for tick thread to initialize\n"));
	while (!tick_thread_ready)
		usleep(0);

	// Fill in register and wait for an interrupt from the tick thread
	D(bug("[emul_thread] filling in registers and waiting for interrupt\n"));
#if defined(__APPLE__) && defined(__MACH__)
#define REG(n) "r" #n
#else
#define REG(n) #n
#endif
	asm volatile ("stw  " REG(2) ",2*4(%0)\n"
				  "mr   " REG(2) ",%0\n"
#define SAVE_REG(n) \
				  "stw  " REG(n) "," #n "*4(" REG(2) ")\n" \
				  "addi " REG(n) "," REG(2) ","#n"\n"
				  SAVE_REG(1)
				  SAVE_REG(3)
				  SAVE_REG(4)
				  SAVE_REG(5)
				  SAVE_REG(6)
				  SAVE_REG(7)
				  SAVE_REG(8)
				  SAVE_REG(9)
				  SAVE_REG(10)
				  SAVE_REG(11)
				  SAVE_REG(12)
				  SAVE_REG(13)
				  SAVE_REG(14)
				  SAVE_REG(15)
				  SAVE_REG(16)
				  SAVE_REG(17)
				  SAVE_REG(18)
				  SAVE_REG(19)
				  SAVE_REG(20)
				  SAVE_REG(21)
				  SAVE_REG(22)
				  SAVE_REG(23)
				  SAVE_REG(24)
				  SAVE_REG(25)
				  SAVE_REG(26)
				  SAVE_REG(27)
				  SAVE_REG(28)
				  SAVE_REG(29)
				  SAVE_REG(30)
				  SAVE_REG(31)
#undef SAVE_REG
				  "   li   " REG(0) ",1\n"
				  "   stw  " REG(0) ",0(" REG(2) ")\n" // regs[0] == emul_thread_ready
				  "0: lwz  " REG(0) ",0(" REG(2) ")\n"
				  "   cmpi 0," REG(0) ",0\n"
				  "   bne+ 0b\n"
#define LOAD_REG(n) \
				 "lwz  " REG(n) "," #n "*4(" REG(2) ")\n"
				  LOAD_REG(1)
				  LOAD_REG(3)
				  LOAD_REG(4)
				  LOAD_REG(5)
				  LOAD_REG(6)
				  LOAD_REG(7)
				  LOAD_REG(8)
				  LOAD_REG(9)
				  LOAD_REG(10)
				  LOAD_REG(11)
				  LOAD_REG(12)
				  LOAD_REG(13)
				  LOAD_REG(14)
				  LOAD_REG(15)
				  LOAD_REG(16)
				  LOAD_REG(17)
				  LOAD_REG(18)
				  LOAD_REG(19)
				  LOAD_REG(20)
				  LOAD_REG(21)
				  LOAD_REG(22)
				  LOAD_REG(23)
				  LOAD_REG(24)
				  LOAD_REG(25)
				  LOAD_REG(26)
				  LOAD_REG(27)
				  LOAD_REG(28)
				  LOAD_REG(29)
				  LOAD_REG(30)
				  LOAD_REG(31)
				  LOAD_REG(2)
#undef LOAD_REG
		: :  "r" ((uintptr)&emul_thread_regs[0]) : "r0");
#undef REG
}


void sigusr2_handler(int sig, siginfo_t *sip, void *scp)
{
	machine_regs *r = MACHINE_REGISTERS(scp);

#ifdef SYSTEM_CLOBBERS_R2
	// Restore pointer to Thread Local Storage
	set_r2(TOC);
#endif

#ifdef SYSTEM_CLOBBERS_R13
	// Restore pointer to .sdata section
	set_r13(R13);
#endif

	ucontext_t *ucp = (ucontext_t *)scp;
	D(bug("SIGUSR2 caught\n"));

	// Check whether sigaltstack works
	sig_sp = get_sp();
	if (sig_sp < sig_stack || sig_sp >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 1;
		goto ret;
	}

	// Check whether r5 points to info on the stack
	sig_r5 = ucp;
	if (sig_r5 < sig_stack || sig_r5 >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 2;
		goto ret;
	}

	// Check whether context regs points to info on the stack
	sig_sc_regs = &r->gpr(0);
	if (sig_sc_regs < sig_stack || sig_sc_regs >= ((uint8 *)sig_stack + SIG_STACK_SIZE)) {
		err = 3;
		goto ret;
	}

	// Check whether registers still hold the values we set them to
	for (int n = 0; n < 32; n++) {
		uint32 expected = (uintptr)&emul_thread_regs[0];
		if (n == 0)
			expected = 1;
		else if (n != 2)
			expected += n;
		if (r->gpr(n) != expected) {
			D(bug("Register corruption: r%d was %08x, expected %08x\n", n, r->gpr(n), expected));
			err = 4;
			goto ret;
		}
	}

  ret:
	// Tell emul_func() to exit
	emul_thread_ready = false;
}


#ifdef TEST
void *TOC;
void *R13;

extern "C" void EmulOp(void *r, uint32 pc, int selector);
void EmulOp(void *r, uint32 pc, int selector)
{
}

extern "C" void check_load_invoc(uint32 type, int16 id, uint32 h);
void check_load_invoc(uint32 type, int16 id, uint32 h)
{
}

void ErrorAlert(const char *text)
{
	printf(GetString(STR_SHELL_ERROR_PREFIX), text);
}

int main(void)
{
#ifdef SYSTEM_CLOBBERS_R2
	// Get TOC pointer
	TOC = get_r2();
#endif

#ifdef SYSTEM_CLOBBERS_R13
	// Get r13 register
	R13 = get_r13();
#endif

	// Check some things
	paranoia_check();
}
#endif
