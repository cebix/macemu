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

#define NEED_CONFIG_H_ONLY
#include "sysdeps.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <list>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "sigsegv.h"

#ifndef NO_STD_NAMESPACE
using std::list;
#endif

// Return value type of a signal handler (standard type if not defined)
#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

// Size of an unsigned integer large enough to hold all bits of a pointer
// NOTE: this can be different than SIGSEGV_REGISTER_TYPE. In
// particular, on ILP32 systems with a 64-bit kernel (HP-UX/ia64?)
// Other systems are sane enough to follow ILP32 or LP64 models
typedef unsigned long sigsegv_uintptr_t;

// Type of the system signal handler
typedef RETSIGTYPE (*signal_handler)(int);

// User's SIGSEGV handler
static sigsegv_fault_handler_t sigsegv_fault_handler = 0;

// Function called to dump state if we can't handle the fault
static sigsegv_state_dumper_t sigsegv_state_dumper = 0;

#if defined(HAVE_SIGINFO_T) || defined(HAVE_SIGCONTEXT_SUBTERFUGE)
// Actual SIGSEGV handler installer
static bool sigsegv_do_install_handler(int sig);
#endif

/*
 *  Instruction decoding aids
 */

// Transfer type
enum transfer_type_t {
	SIGSEGV_TRANSFER_UNKNOWN	= 0,
	SIGSEGV_TRANSFER_LOAD		= 1,
	SIGSEGV_TRANSFER_STORE		= 2
};

// Transfer size
enum transfer_size_t {
	SIZE_UNKNOWN,
	SIZE_BYTE,
	SIZE_WORD, // 2 bytes
	SIZE_LONG, // 4 bytes
	SIZE_QUAD  // 8 bytes
};

/*
 *  OS-dependant SIGSEGV signals support section
 */

#if HAVE_SIGINFO_T
// Generic extended signal handler

#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	siginfo_t *sip, void *scp
#define SIGSEGV_FAULT_HANDLER_ARGS		sip, scp
#define SIGSEGV_FAULT_ADDRESS			sip->si_addr


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

#ifndef HAVE_MACH64_VM

// Undefine this to prevent a preprocessor warning when compiling on a
// 32-bit machine with Mac OS X 10.5.
#undef	MACH_EXCEPTION_CODES

#define MACH_EXCEPTION_CODES					0
#define mach_exception_data_t					exception_data_t
#define mach_exception_data_type_t				exception_data_type_t
#define mach_exc_server							exc_server
#define catch_mach_exception_raise				catch_exception_raise
#define mach_exception_raise					exception_raise
#define mach_exception_raise_state				exception_raise_state
#define mach_exception_raise_state_identity		exception_raise_state_identity
#endif

extern boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);
extern kern_return_t catch_mach_exception_raise(mach_port_t, mach_port_t,
	mach_port_t, exception_type_t, mach_exception_data_t, mach_msg_type_number_t);
extern kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port,
	exception_type_t exception, mach_exception_data_t code, mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t old_state, mach_msg_type_number_t old_state_count,
	thread_state_t new_state, mach_msg_type_number_t *new_state_count);
extern kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port,
	mach_port_t thread_port, mach_port_t task_port, exception_type_t exception,
	mach_exception_data_t code, mach_msg_type_number_t code_count,
	int *flavor,
	thread_state_t old_state, mach_msg_type_number_t old_state_count,
	thread_state_t new_state, mach_msg_type_number_t *new_state_count);
extern kern_return_t mach_exception_raise(mach_port_t, mach_port_t, mach_port_t,
	exception_type_t, mach_exception_data_t, mach_msg_type_number_t);
extern kern_return_t mach_exception_raise_state(mach_port_t, exception_type_t,
	mach_exception_data_t, mach_msg_type_number_t, thread_state_flavor_t *,
	thread_state_t, mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t *);
extern kern_return_t mach_exception_raise_state_identity(mach_port_t, mach_port_t, mach_port_t,
	exception_type_t, mach_exception_data_t, mach_msg_type_number_t, thread_state_flavor_t *,
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

#ifndef MACH_FIELD_NAME
#define MACH_FIELD_NAME(X) X
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

		if (!mach_exc_server(msg, reply)) {
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

#ifndef SIGSEGV_REGISTER_TYPE
#define SIGSEGV_REGISTER_TYPE sigsegv_uintptr_t
#endif

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
// Decode and skip X86 instruction
#if (defined(i386) || defined(__i386__) || defined(_M_IX86)) || (defined(__x86_64__) || defined(_M_X64))

#if defined(__APPLE__) && defined(__MACH__)
enum {
#if (defined(i386) || defined(__i386__))
#ifdef i386_SAVED_STATE
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
	X86_REG_EDX = 3,
	X86_REG_EBX = 1,
	X86_REG_ESP = 7,
	X86_REG_EBP = 6,
	X86_REG_ESI = 5,
	X86_REG_EDI = 4
#endif
#endif
#if defined(__x86_64__)
	X86_REG_R8  = 8,
	X86_REG_R9  = 9,
	X86_REG_R10 = 10,
	X86_REG_R11 = 11,
	X86_REG_R12 = 12,
	X86_REG_R13 = 13,
	X86_REG_R14 = 14,
	X86_REG_R15 = 15,
	X86_REG_EDI = 4,
	X86_REG_ESI = 5,
	X86_REG_EBP = 6,
	X86_REG_EBX = 1,
	X86_REG_EDX = 3,
	X86_REG_EAX = 0,
	X86_REG_ECX = 2,
	X86_REG_ESP = 7,
	X86_REG_EIP = 16
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

static bool ix86_skip_instruction(SIGSEGV_REGISTER_TYPE * regs)
{
	unsigned char * eip = (unsigned char *)regs[X86_REG_EIP];

	if (eip == 0)
		return false;
	
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
#if defined(__x86_64__) || defined(_M_X64)
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
#if defined(__x86_64__) || defined(_M_X64)
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
		// fall through
	case 0x03: // ADD r32, r/m32
		instruction_type = i_ADD;
		goto do_transfer_load;
	case 0x8a: // MOV r8, r/m8
		transfer_size = SIZE_BYTE;
		// fall through
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
		// fall through
	case 0x01: // ADD r/m32, r32
		instruction_type = i_ADD;
		goto do_transfer_store;
	case 0x88: // MOV r/m8, r8
		transfer_size = SIZE_BYTE;
		// fall through
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

#if defined(__x86_64__) || defined(_M_X64)
	if (rex.R)
		reg += 8;
#endif

	if (instruction_type == i_MOV && transfer_type == SIGSEGV_TRANSFER_LOAD && reg != -1) {
		static const int x86_reg_map[] = {
			X86_REG_EAX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBX,
			X86_REG_ESP, X86_REG_EBP, X86_REG_ESI, X86_REG_EDI,
#if defined(__x86_64__) || defined(_M_X64)
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

#endif


// Fallbacks
#ifndef SIGSEGV_FAULT_ADDRESS_FAST
#define SIGSEGV_FAULT_ADDRESS_FAST		SIGSEGV_FAULT_ADDRESS
#endif
#ifndef SIGSEGV_FAULT_INSTRUCTION_FAST
#define SIGSEGV_FAULT_INSTRUCTION_FAST	SIGSEGV_FAULT_INSTRUCTION
#endif
#ifndef SIGSEGV_FAULT_INSTRUCTION
#define SIGSEGV_FAULT_INSTRUCTION		SIGSEGV_INVALID_ADDRESS
#endif
#ifndef SIGSEGV_FAULT_HANDLER_ARGLIST_1
#define SIGSEGV_FAULT_HANDLER_ARGLIST_1	SIGSEGV_FAULT_HANDLER_ARGLIST
#endif
#ifndef SIGSEGV_FAULT_HANDLER_INVOKE
#define SIGSEGV_FAULT_HANDLER_INVOKE(P)	sigsegv_fault_handler(P)
#endif

// SIGSEGV recovery supported ?
#if defined(SIGSEGV_ALL_SIGNALS) && defined(SIGSEGV_FAULT_HANDLER_ARGLIST) && defined(SIGSEGV_FAULT_ADDRESS)
#define HAVE_SIGSEGV_RECOVERY
#endif


/*
 *  SIGSEGV global handler
 */

#ifdef HAVE_MACH_EXCEPTIONS


static void mach_get_thread_state(sigsegv_info_t *SIP)
{
	SIP->thr_state_count = SIGSEGV_THREAD_STATE_COUNT;
	kern_return_t krc = thread_get_state(SIP->thread,
										 SIGSEGV_THREAD_STATE_FLAVOR,
										 (natural_t *)&SIP->thr_state,
										 &SIP->thr_state_count);
	MACH_CHECK_ERROR(thread_get_state, krc);
	SIP->has_thr_state = true;
}

static void mach_set_thread_state(sigsegv_info_t *SIP)
{
	kern_return_t krc = thread_set_state(SIP->thread,
										 SIGSEGV_THREAD_STATE_FLAVOR,
										 (natural_t *)&SIP->thr_state,
										 SIP->thr_state_count);
	MACH_CHECK_ERROR(thread_set_state, krc);
}
#endif

// Return the address of the invalid memory reference
sigsegv_address_t sigsegv_get_fault_address(sigsegv_info_t *SIP)
{
	return SIP->addr;
}

// Return the address of the instruction that caused the fault, or
// SIGSEGV_INVALID_ADDRESS if we could not retrieve this information
sigsegv_address_t sigsegv_get_fault_instruction_address(sigsegv_info_t *SIP)
{
	return SIP->pc;
}

// This function handles the badaccess to memory.
// It is called from the signal handler or the exception handler.
static bool handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGLIST_1)
{
	sigsegv_info_t SI;
	SI.addr = (sigsegv_address_t)SIGSEGV_FAULT_ADDRESS_FAST;
	SI.pc = (sigsegv_address_t)SIGSEGV_FAULT_INSTRUCTION_FAST;
#ifdef HAVE_MACH_EXCEPTIONS
	SI.thread = thread;
	SI.has_exc_state = false;
	SI.has_thr_state = false;
#endif
	sigsegv_info_t * const SIP = &SI;

	// Call user's handler and reinstall the global handler, if required
	switch (SIGSEGV_FAULT_HANDLER_INVOKE(SIP)) {
	case SIGSEGV_RETURN_SUCCESS:
		return true;

#if HAVE_SIGSEGV_SKIP_INSTRUCTION
	case SIGSEGV_RETURN_SKIP_INSTRUCTION:
		// Call the instruction skipper with the register file
		// available
#ifdef HAVE_MACH_EXCEPTIONS
		if (!SIP->has_thr_state)
			mach_get_thread_state(SIP);
#endif
		if (SIGSEGV_SKIP_INSTRUCTION(SIGSEGV_REGISTER_FILE)) {
#ifdef HAVE_MACH_EXCEPTIONS
			// Unlike UNIX signals where the thread state
			// is modified off of the stack, in Mach we
			// need to actually call thread_set_state to
			// have the register values updated.
			mach_set_thread_state(SIP);
#endif
			return true;
		}
		break;
#endif
	case SIGSEGV_RETURN_FAILURE:
		// We can't do anything with the fault_address, dump state?
		if (sigsegv_state_dumper != 0)
			sigsegv_state_dumper(SIP);
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
				  mach_exception_data_t exception_data,
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
	  kret = mach_exception_raise(port, thread_port, task_port, exception_type,
								  exception_data, data_count);
	  MACH_CHECK_ERROR (mach_exception_raise, kret);
	  break;
	case EXCEPTION_STATE:
	  // fprintf(stderr, "forwarding to exception_raise_state\n");
	  kret = mach_exception_raise_state(port, exception_type, exception_data,
										data_count, &flavor,
										(natural_t *)&thread_state, thread_state_count,
										(natural_t *)&thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (mach_exception_raise_state, kret);
	  break;
	case EXCEPTION_STATE_IDENTITY:
	  // fprintf(stderr, "forwarding to exception_raise_state_identity\n");
	  kret = mach_exception_raise_state_identity(port, thread_port, task_port,
												 exception_type, exception_data,
												 data_count, &flavor,
												 (natural_t *)&thread_state, thread_state_count,
												 (natural_t *)&thread_state, &thread_state_count);
	  MACH_CHECK_ERROR (mach_exception_raise_state_identity, kret);
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
catch_mach_exception_raise(mach_port_t exception_port,
						   mach_port_t thread,
						   mach_port_t task,
						   exception_type_t exception,
						   mach_exception_data_t code,
						   mach_msg_type_number_t code_count)
{
	kern_return_t krc;

	if (exception == EXC_BAD_ACCESS) {
		switch (code[0]) {
		case KERN_PROTECTION_FAILURE:
		case KERN_INVALID_ADDRESS:
			if (handle_badaccess(SIGSEGV_FAULT_HANDLER_ARGS))
				return KERN_SUCCESS;
			break;
		}
	}

	// In Mach we do not need to remove the exception handler.
	// If we forward the exception, eventually some exception handler
	// will take care of this exception.
	krc = forward_exception(thread, task, exception, code, code_count, &ports);

	return krc;
}

/* XXX: borrowed from launchd and gdb */
kern_return_t
catch_mach_exception_raise_state(mach_port_t exception_port,
								 exception_type_t exception,
								 mach_exception_data_t code,
								 mach_msg_type_number_t code_count,
								 int *flavor,
								 thread_state_t old_state,
								 mach_msg_type_number_t old_state_count,
								 thread_state_t new_state,
								 mach_msg_type_number_t *new_state_count)
{
	memcpy(new_state, old_state, old_state_count * sizeof(old_state[0]));
	*new_state_count = old_state_count;
	return KERN_SUCCESS;
}

/* XXX: borrowed from launchd and gdb */
kern_return_t
catch_mach_exception_raise_state_identity(mach_port_t exception_port,
										  mach_port_t thread_port,
										  mach_port_t task_port,
										  exception_type_t exception,
										  mach_exception_data_t code,
										  mach_msg_type_number_t code_count,
										  int *flavor,
										  thread_state_t old_state,
										  mach_msg_type_number_t old_state_count,
										  thread_state_t new_state,
										  mach_msg_type_number_t *new_state_count)
{
	kern_return_t kret;

	memcpy(new_state, old_state, old_state_count * sizeof(old_state[0]));
	*new_state_count = old_state_count;

	kret = mach_port_deallocate(mach_task_self(), task_port);
	MACH_CHECK_ERROR(mach_port_deallocate, kret);
	kret = mach_port_deallocate(mach_task_self(), thread_port);
	MACH_CHECK_ERROR(mach_port_deallocate, kret);

	return KERN_SUCCESS;
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
	memset(&sigsegv_sa, 0, sizeof(struct sigaction));
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
	memset(&sigsegv_sa, 0, sizeof(struct sigaction));
	sigemptyset(&sigsegv_sa.sa_mask);
	sigsegv_sa.sa_handler = (signal_handler)sigsegv_handler;
	sigsegv_sa.sa_flags = 0;
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
				EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, SIGSEGV_THREAD_STATE_FLAVOR);
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
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "vm_alloc.h"

const int REF_INDEX = 123;
const int REF_VALUE = 45;

static sigsegv_uintptr_t page_size;
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

static sigsegv_return_t sigsegv_test_handler(sigsegv_info_t *sip)
{
	const sigsegv_address_t fault_address = sigsegv_get_fault_address(sip);
	const sigsegv_address_t instruction_address = sigsegv_get_fault_instruction_address(sip);
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
	if (instruction_address != SIGSEGV_INVALID_ADDRESS
		&& ((instruction_address <  (sigsegv_address_t)b_region) ||
			(instruction_address >= (sigsegv_address_t)e_region)))
		exit(11);
#endif
	if (vm_protect((char *)((sigsegv_uintptr_t)fault_address & -page_size), page_size, VM_PAGE_READ | VM_PAGE_WRITE) != 0)
		exit(12);
	return SIGSEGV_RETURN_SUCCESS;
}

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
static sigsegv_return_t sigsegv_insn_handler(sigsegv_info_t *sip)
{
	const sigsegv_address_t fault_address = sigsegv_get_fault_address(sip);
	const sigsegv_address_t instruction_address = sigsegv_get_fault_instruction_address(sip);
#if DEBUG
	printf("sigsegv_insn_handler(%p, %p)\n", fault_address, instruction_address);
	printf("expected instruction address range: %p-%p\n", b_region, e_region);
#endif
	if (((sigsegv_uintptr_t)fault_address - (sigsegv_uintptr_t)page) < page_size) {
#ifdef __GNUC__
		// Make sure reported fault instruction address falls into
		// expected code range
		if (instruction_address != SIGSEGV_INVALID_ADDRESS
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
#if (defined(i386) || defined(__i386__)) || (defined(__x86_64__) || defined(_M_X64))
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
#if defined(__x86_64__) || defined(_M_X64)
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
	SIGSEGV_REGISTER_TYPE regs[N_REGS];
	for (int i = 0; i < N_REGS; i++)
		regs[i] = i;
	const sigsegv_uintptr_t start_code = (sigsegv_uintptr_t)&code;
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
	/* This is a really awful hack but otherwise gcc is smart enough
	 * (or bug'ous enough?) to optimize the labels and place them
	 * e.g. at the "main" entry point, which is wrong.
	 */
	volatile int label_hack = 3;
	switch (label_hack) {
	case 3:
	L_b_region1:
		page[REF_INDEX] = REF_VALUE;
		if (page[REF_INDEX] != REF_VALUE)
			exit(20);
		page[REF_INDEX] = REF_VALUE;
		BARRIER();
		// fall-through
	case 2:
	L_e_region1:
		BARRIER();
		break;
	}

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
#ifdef DEBUG
	printf("switch footage : \n");
	printf("   4 : %p\n", &&L_b_4_region2);
	printf("   5 : %p\n", &&L_b_5_region2);
	printf("   8 : %p\n", &&L_b_8_region2);
	printf("   6 : %p\n", &&L_b_6_region2);
	printf("   7 : %p\n", &&L_b_7_region2);
	printf("   9 : %p\n", &&L_b_9_region2);
	printf("   1 : %p\n", &&L_b_1_region2);
#endif
#endif
	switch (label_hack) {
	case 3:
	L_b_region2:
		TEST_SKIP_INSTRUCTION(unsigned char);
		BARRIER();
	case 4:
	L_b_4_region2:
		TEST_SKIP_INSTRUCTION(unsigned short);
		BARRIER();
	case 5:
	L_b_5_region2:
		TEST_SKIP_INSTRUCTION(unsigned int);
		BARRIER();
	case 8:
	L_b_8_region2:
		TEST_SKIP_INSTRUCTION(unsigned long);
		BARRIER();
	case 6:
	L_b_6_region2:
		TEST_SKIP_INSTRUCTION(signed char);
		BARRIER();
	case 7:
	L_b_7_region2:
		TEST_SKIP_INSTRUCTION(signed short);
		BARRIER();
	case 9:
	L_b_9_region2:
		TEST_SKIP_INSTRUCTION(signed int);
		BARRIER();
	case 1:
	L_b_1_region2:
		TEST_SKIP_INSTRUCTION(signed long);
		BARRIER();
		// fall-through
	case 2:
	L_e_region2:
		BARRIER();
		break;
	}
	if (!arch_insn_skipper_tests())
		return 20;
#endif

	vm_exit();
	return 0;
}
#endif
