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

// Is the fault to be ignored?
static bool sigsegv_ignore_fault = false;

// User's SIGSEGV handler
static sigsegv_fault_handler_t sigsegv_fault_handler = 0;

// Function called to dump state if we can't handle the fault
static sigsegv_state_dumper_t sigsegv_state_dumper = 0;

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
#define SIGSEGV_REGISTER_FILE			(unsigned long *)(((ucontext_t *)scp)->uc_mcontext.gregs)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#endif
#if (defined(ia64) || defined(__ia64__))
#define SIGSEGV_FAULT_INSTRUCTION		(((struct sigcontext *)scp)->sc_ip & ~0x3ULL) /* slot number is in bits 0 and 1 */
#endif
#if (defined(powerpc) || defined(__powerpc__))
#include <sys/ucontext.h>
#define SIGSEGV_FAULT_INSTRUCTION		(((ucontext_t *)scp)->uc_mcontext.regs->nip)
#define SIGSEGV_REGISTER_FILE			(unsigned long *)(((ucontext_t *)scp)->uc_mcontext.regs)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
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
#define SIGSEGV_REGISTER_FILE			(unsigned long *)(&scs)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
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
#define SIGSEGV_REGISTER_FILE			(unsigned long *)(scp->regs)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
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
#include <ucontext.h>
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_badvaddr
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV)
#endif

// HP-UX
#if (defined(hpux) || defined(__hpux__))
#define SIGSEGV_FAULT_HANDLER_ARGLIST	int sig, int code, struct sigcontext *scp
#define SIGSEGV_FAULT_ADDRESS			scp->sc_sl.sl_ss.ss_narrow.ss_cr21
#define SIGSEGV_ALL_SIGNALS				FAULT_HANDLER(SIGSEGV) FAULT_HANDLER(SIGBUS)
#endif

// OSF/1 on Alpha
#if defined(__osf__)
#include <ucontext.h>
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

static bool ix86_skip_instruction(sigsegv_address_t fault_instruction, unsigned long * regs)
{
	unsigned char * eip = (unsigned char *)fault_instruction;

	if (eip == 0)
		return false;
	
	// Transfer type
	enum {
		TYPE_UNKNOWN,
		TYPE_LOAD,
		TYPE_STORE
	} transfer_type = TYPE_UNKNOWN;
	
	// Transfer size
	enum {
		SIZE_BYTE,
		SIZE_WORD,
		SIZE_LONG
	} transfer_size = SIZE_LONG;
	
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
	case 0x8a: // MOV r8, r/m8
		transfer_size = SIZE_BYTE;
	case 0x8b: // MOV r32, r/m32 (or 16-bit operation)
		switch (eip[1] & 0xc0) {
		case 0x80:
			reg = (eip[1] >> 3) & 7;
			transfer_type = TYPE_LOAD;
			break;
		case 0x40:
			reg = (eip[1] >> 3) & 7;
			transfer_type = TYPE_LOAD;
			break;
		case 0x00:
			reg = (eip[1] >> 3) & 7;
			transfer_type = TYPE_LOAD;
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
			transfer_type = TYPE_STORE;
			break;
		case 0x40:
			reg = (eip[1] >> 3) & 7;
			transfer_type = TYPE_STORE;
			break;
		case 0x00:
			reg = (eip[1] >> 3) & 7;
			transfer_type = TYPE_STORE;
			break;
		}
		len += 2 + ix86_step_over_modrm(eip + 1);
		break;
	}

	if (transfer_type == TYPE_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

	if (transfer_type == TYPE_LOAD && reg != -1) {
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
	printf("%08x: %s %s access", fault_instruction,
		   transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_WORD ? "word" : "long",
		   transfer_type == TYPE_LOAD ? "read" : "write");
	
	if (reg != -1) {
		static const char * x86_reg_str_map[8] = {
			"eax", "ecx", "edx", "ebx",
			"esp", "ebp", "esi", "edi"
		};
		printf(" %s register %%%s", transfer_type == TYPE_LOAD ? "to" : "from", x86_reg_str_map[reg]);
	}
	printf(", %d bytes instruction\n", len);
#endif
	
	regs[X86_REG_EIP] += len;
	return true;
}
#endif
// Decode and skip PPC instruction
#if (defined(powerpc) || defined(__powerpc__))
#if defined(__linux__)
enum {
	POWERPC_REG_GPR = 0,
	POWERPC_REG_NIP = 32
};
#endif
static bool powerpc_skip_instruction(sigsegv_address_t fault_instruction, unsigned long * regs)
{
	// Get opcode and divide into fields
	unsigned int opcode = *((unsigned int *)fault_instruction);
	unsigned int primop = opcode >> 26;
	unsigned int exop = (opcode >> 1) & 0x3ff;
	unsigned int ra = (opcode >> 16) & 0x1f;
	unsigned int rb = (opcode >> 11) & 0x1f;
	unsigned int rd = (opcode >> 21) & 0x1f;
	signed int imm = (signed short)(opcode & 0xffff);
	
	// Analyze opcode
	enum {
		TYPE_UNKNOWN,
		TYPE_LOAD,
		TYPE_STORE
	} transfer_type = TYPE_UNKNOWN;
	enum {
		SIZE_UNKNOWN,
		SIZE_BYTE,
		SIZE_HALFWORD,
		SIZE_WORD
	} transfer_size = SIZE_UNKNOWN;
	enum {
		MODE_UNKNOWN,
		MODE_NORM,
		MODE_U,
		MODE_X,
		MODE_UX
	} addr_mode = MODE_UNKNOWN;
	switch (primop) {
	case 31:
		switch (exop) {
		case 23:	// lwzx
			transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
		case 55:	// lwzux
			transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
		case 87:	// lbzx
			transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
		case 119:	// lbzux
			transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
		case 151:	// stwx
			transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
		case 183:	// stwux
			transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
		case 215:	// stbx
			transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
		case 247:	// stbux
			transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
		case 279:	// lhzx
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
		case 311:	// lhzux
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
		case 343:	// lhax
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
		case 375:	// lhaux
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
		case 407:	// sthx
			transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
		case 439:	// sthux
			transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
		}
		break;
	
	case 32:	// lwz
		transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
	case 33:	// lwzu
		transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
	case 34:	// lbz
		transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
	case 35:	// lbzu
		transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
	case 36:	// stw
		transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
	case 37:	// stwu
		transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
	case 38:	// stb
		transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
	case 39:	// stbu
		transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
	case 40:	// lhz
		transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
	case 41:	// lhzu
		transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
	case 42:	// lha
		transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
	case 43:	// lhau
		transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
	case 44:	// sth
		transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
	case 45:	// sthu
		transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
	}
	
	// Calculate effective address
	unsigned int addr = 0;
	switch (addr_mode) {
	case MODE_X:
	case MODE_UX:
		if (ra == 0)
			addr = regs[POWERPC_REG_GPR + rb];
		else
			addr = regs[POWERPC_REG_GPR + ra] + regs[POWERPC_REG_GPR + rb];
		break;
	case MODE_NORM:
	case MODE_U:
		if (ra == 0)
			addr = (signed int)(signed short)imm;
		else
			addr = regs[POWERPC_REG_GPR + ra] + (signed int)(signed short)imm;
		break;
	default:
		break;
	}
	
	if (transfer_type == TYPE_UNKNOWN) {
		// Unknown machine code, let it crash. Then patch the decoder
		return false;
	}

#if DEBUG
	printf("%08x: %s %s access", fault_instruction,
		   transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_HALFWORD ? "word" : "long",
		   transfer_type == TYPE_LOAD ? "read" : "write");
	
	if (addr_mode == MODE_U || addr_mode == MODE_UX)
		printf(" r%d (ra = %08x)\n", ra, addr);
	if (transfer_type == TYPE_LOAD)
		printf(" r%d (rd = 0)\n", rd);
#endif
	
	if (addr_mode == MODE_U || addr_mode == MODE_UX)
		regs[POWERPC_REG_GPR + ra] = addr;
	if (transfer_type == TYPE_LOAD)
		regs[POWERPC_REG_GPR + rd] = 0;
	
	regs[POWERPC_REG_NIP] += 4;
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

#ifdef HAVE_SIGSEGV_RECOVERY
static void sigsegv_handler(SIGSEGV_FAULT_HANDLER_ARGLIST)
{
	sigsegv_address_t fault_address = (sigsegv_address_t)SIGSEGV_FAULT_ADDRESS;
	sigsegv_address_t fault_instruction = (sigsegv_address_t)SIGSEGV_FAULT_INSTRUCTION;
	bool fault_recovered = false;
	
	// Call user's handler and reinstall the global handler, if required
	if (sigsegv_fault_handler(fault_address, fault_instruction)) {
#if (defined(HAVE_SIGACTION) ? defined(SIGACTION_NEED_REINSTALL) : defined(SIGNAL_NEED_REINSTALL))
		sigsegv_do_install_handler(sig);
#endif
		fault_recovered = true;
	}
#if HAVE_SIGSEGV_SKIP_INSTRUCTION
	else if (sigsegv_ignore_fault) {
		// Call the instruction skipper with the register file available
		if (SIGSEGV_SKIP_INSTRUCTION(fault_instruction, SIGSEGV_REGISTER_FILE))
			fault_recovered = true;
	}
#endif

	if (!fault_recovered) {
		// FAIL: reinstall default handler for "safe" crash
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
		SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
		
		// We can't do anything with the fault_address, dump state?
		if (sigsegv_state_dumper != 0)
			sigsegv_state_dumper(fault_address, fault_instruction);
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

bool sigsegv_install_handler(sigsegv_fault_handler_t handler)
{
#ifdef HAVE_SIGSEGV_RECOVERY
	sigsegv_fault_handler = handler;
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
	sigsegv_fault_handler = 0;
#define FAULT_HANDLER(sig) signal(sig, SIG_DFL);
	SIGSEGV_ALL_SIGNALS
#undef FAULT_HANDLER
#endif
}


/*
 *  SIGSEGV ignore state modifier
 */

void sigsegv_set_ignore_state(bool ignore_fault)
{
	sigsegv_ignore_fault = ignore_fault;
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

static bool sigsegv_test_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	handler_called++;
	if ((fault_address - 123) != page)
		exit(1);
	if (vm_protect((char *)((unsigned long)fault_address & -page_size), page_size, VM_PAGE_READ | VM_PAGE_WRITE) != 0)
		exit(1);
	return true;
}

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
static bool sigsegv_insn_handler(sigsegv_address_t fault_address, sigsegv_address_t instruction_address)
{
	return false;
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
	
	if (vm_protect((char *)page, page_size, VM_PAGE_WRITE) < 0)
		return 1;
	
	for (int i = 0; i < page_size; i++)
		page[i] = (i + 1) % page_size;
	
	if (vm_protect((char *)page, page_size, VM_PAGE_NOACCESS) < 0)
		return 1;
	
	sigsegv_set_ignore_state(true);

#define TEST_SKIP_INSTRUCTION(TYPE) do {				\
		const unsigned int TAG = 0x12345678;			\
		TYPE data = *((TYPE *)(page + sizeof(TYPE)));	\
		volatile unsigned int effect = data + TAG;		\
		if (effect != TAG)								\
			return 1;									\
	} while (0)
	
	TEST_SKIP_INSTRUCTION(unsigned char);
	TEST_SKIP_INSTRUCTION(unsigned short);
	TEST_SKIP_INSTRUCTION(unsigned int);
#endif

	vm_exit();
	return 0;
}
#endif
