/*
 *  sigsegv.h - SIGSEGV signals support
 *
 *  Derived from Bruno Haible's work on his SIGSEGV library for clisp
 *  <http://clisp.sourceforge.net/>
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

#ifndef SIGSEGV_H
#define SIGSEGV_H

#define SIGSEGV_MAJOR_VERSION 1
#define SIGSEGV_MINOR_VERSION 0
#define SIGSEGV_MICRO_VERSION 0

#define SIGSEGV_CHECK_VERSION(MAJOR, MINOR, MICRO)						\
		(SIGSEGV_MAJOR_VERSION > (MAJOR) ||								\
		 (SIGSEGV_MAJOR_VERSION == (MAJOR) && SIGSEGV_MINOR_VERSION > (MINOR)) || \
		 (SIGSEGV_MAJOR_VERSION == (MAJOR) && SIGSEGV_MINOR_VERSION == (MINOR) && SIGSEGV_MICRO_VERSION >= (MICRO)))

// Address type
typedef char *sigsegv_address_t;

// SIGSEGV handler argument (forward declaration)

#if HAVE_MACH_EXCEPTIONS
#if defined(__APPLE__) && defined(__MACH__)
extern "C" {
#include <mach/mach.h>
#include <mach/mach_error.h>
}

#ifdef __ppc__

#if __DARWIN_UNIX03 && defined _STRUCT_PPC_THREAD_STATE
#define MACH_FIELD_NAME(X)				__CONCAT(__,X)
#endif // END OF #if __DARWIN_UNIX03 && defined _STRUCT_PPC_THREAD_STATE

#define SIGSEGV_EXCEPTION_STATE_TYPE	ppc_exception_state_t
#define SIGSEGV_EXCEPTION_STATE_FLAVOR	PPC_EXCEPTION_STATE
#define SIGSEGV_EXCEPTION_STATE_COUNT	PPC_EXCEPTION_STATE_COUNT
#define SIGSEGV_FAULT_ADDRESS			SIP->exc_state.MACH_FIELD_NAME(dar)
#define SIGSEGV_THREAD_STATE_TYPE		ppc_thread_state_t
#define SIGSEGV_THREAD_STATE_FLAVOR		PPC_THREAD_STATE
#define SIGSEGV_THREAD_STATE_COUNT		PPC_THREAD_STATE_COUNT
#define SIGSEGV_FAULT_INSTRUCTION		SIP->thr_state.MACH_FIELD_NAME(srr0)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&SIP->thr_state.MACH_FIELD_NAME(srr0), (unsigned long *)&SIP->thr_state.MACH_FIELD_NAME(r0)
#endif // END OF #ifdef __ppc__

#ifdef __ppc64__

#if __DARWIN_UNIX03 && defined _STRUCT_PPC_THREAD_STATE64
#define MACH_FIELD_NAME(X)				__CONCAT(__,X)
#endif // END OF #if __DARWIN_UNIX03 && defined _STRUCT_PPC_THREAD_STATE64

#define SIGSEGV_EXCEPTION_STATE_TYPE	ppc_exception_state64_t
#define SIGSEGV_EXCEPTION_STATE_FLAVOR	PPC_EXCEPTION_STATE64
#define SIGSEGV_EXCEPTION_STATE_COUNT	PPC_EXCEPTION_STATE64_COUNT
#define SIGSEGV_FAULT_ADDRESS			SIP->exc_state.MACH_FIELD_NAME(dar)
#define SIGSEGV_THREAD_STATE_TYPE		ppc_thread_state64_t
#define SIGSEGV_THREAD_STATE_FLAVOR		PPC_THREAD_STATE64
#define SIGSEGV_THREAD_STATE_COUNT		PPC_THREAD_STATE64_COUNT
#define SIGSEGV_FAULT_INSTRUCTION		SIP->thr_state.MACH_FIELD_NAME(srr0)
#define SIGSEGV_SKIP_INSTRUCTION		powerpc_skip_instruction
#define SIGSEGV_REGISTER_FILE			(unsigned long *)&SIP->thr_state.MACH_FIELD_NAME(srr0), (unsigned long *)&SIP->thr_state.MACH_FIELD_NAME(r0)

#endif // END OF #ifdef __ppc64__

#ifdef __i386__
#if __DARWIN_UNIX03 && defined _STRUCT_X86_THREAD_STATE32
#define MACH_FIELD_NAME(X)				__CONCAT(__,X)
#endif // END OF #if __DARWIN_UNIX03 && defined _STRUCT_X86_THREAD_STATE32

#define SIGSEGV_EXCEPTION_STATE_TYPE	i386_exception_state_t
#define SIGSEGV_EXCEPTION_STATE_FLAVOR	i386_EXCEPTION_STATE
#define SIGSEGV_EXCEPTION_STATE_COUNT	i386_EXCEPTION_STATE_COUNT
#define SIGSEGV_FAULT_ADDRESS			SIP->exc_state.MACH_FIELD_NAME(faultvaddr)
#define SIGSEGV_THREAD_STATE_TYPE		i386_thread_state_t
#define SIGSEGV_THREAD_STATE_FLAVOR		i386_THREAD_STATE
#define SIGSEGV_THREAD_STATE_COUNT		i386_THREAD_STATE_COUNT
#define SIGSEGV_FAULT_INSTRUCTION		SIP->thr_state.MACH_FIELD_NAME(eip)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#define SIGSEGV_REGISTER_FILE			((SIGSEGV_REGISTER_TYPE *)&SIP->thr_state.MACH_FIELD_NAME(eax)) /* EAX is the first GPR we consider */

#endif // END OF #ifdef __i386__

#ifdef __x86_64__

#if __DARWIN_UNIX03 && defined _STRUCT_X86_THREAD_STATE64
#define MACH_FIELD_NAME(X)				__CONCAT(__,X)
#endif // END OF #if __DARWIN_UNIX03 && defined _STRUCT_X86_THREAD_STATE64

#define SIGSEGV_EXCEPTION_STATE_TYPE	x86_exception_state64_t
#define SIGSEGV_EXCEPTION_STATE_FLAVOR	x86_EXCEPTION_STATE64
#define SIGSEGV_EXCEPTION_STATE_COUNT	x86_EXCEPTION_STATE64_COUNT
#define SIGSEGV_FAULT_ADDRESS			SIP->exc_state.MACH_FIELD_NAME(faultvaddr)
#define SIGSEGV_THREAD_STATE_TYPE		x86_thread_state64_t
#define SIGSEGV_THREAD_STATE_FLAVOR		x86_THREAD_STATE64
#define SIGSEGV_THREAD_STATE_COUNT		x86_THREAD_STATE64_COUNT
#define SIGSEGV_FAULT_INSTRUCTION		SIP->thr_state.MACH_FIELD_NAME(rip)
#define SIGSEGV_SKIP_INSTRUCTION		ix86_skip_instruction
#define SIGSEGV_REGISTER_FILE			((SIGSEGV_REGISTER_TYPE *)&SIP->thr_state.MACH_FIELD_NAME(rax)) /* RAX is the first GPR we consider */

#endif //END OF #ifdef __x86_64__

#ifdef __x86_64__
#define SIGSEGV_FAULT_ADDRESS_FAST		(((uint64_t)code[1])|0x100000000)
#elif __aarch64__
#define SIGSEGV_FAULT_ADDRESS_FAST		(((uint64_t)code[1])|0x100000000)
#else
#define SIGSEGV_FAULT_ADDRESS_FAST		code[1]
#endif // END OF #ifdef __x86_64__

#ifdef __aarch64__
#define SIGSEGV_EXCEPTION_STATE_TYPE	arm_exception_state64_t
#define SIGSEGV_EXCEPTION_STATE_FLAVOR	ARM_EXCEPTION_STATE64
#define SIGSEGV_EXCEPTION_STATE_COUNT	ARM_EXCEPTION_STATE64_COUNT
#define SIGSEGV_FAULT_ADDRESS			SIP->exc_state.MACH_FIELD_NAME(far)
#define SIGSEGV_THREAD_STATE_TYPE		arm_thread_state64_t
#define SIGSEGV_THREAD_STATE_FLAVOR		ARM_THREAD_STATE64
#define SIGSEGV_THREAD_STATE_COUNT		ARM_THREAD_STATE64_COUNT
#define SIGSEGV_REGISTER_FILE			((SIGSEGV_REGISTER_TYPE *)&SIP->thr_state.MACH_FIELD_NAME(x[0])) /* x[0] is the first GPR we consider */
#define SIGSEGV_SKIP_INSTRUCTION		aarch64_skip_instruction
#endif // END OF #ifdef __arm64__

#define SIGSEGV_FAULT_INSTRUCTION_FAST	SIGSEGV_INVALID_ADDRESS
#define SIGSEGV_FAULT_HANDLER_ARGLIST	mach_port_t thread, mach_exception_data_t code
#define SIGSEGV_FAULT_HANDLER_ARGS		thread, code

#endif // END OF #if defined(__APPLE__) && defined(__MACH__)

#endif // END OF #if HAVE_MACH_EXCEPTIONS

struct sigsegv_info_t {
	sigsegv_address_t addr;
	sigsegv_address_t pc;
#ifdef HAVE_MACH_EXCEPTIONS
	mach_port_t thread;
	bool has_exc_state;
	SIGSEGV_EXCEPTION_STATE_TYPE exc_state;
	mach_msg_type_number_t exc_state_count;
	bool has_thr_state;
	SIGSEGV_THREAD_STATE_TYPE thr_state;
	mach_msg_type_number_t thr_state_count;
#endif
};


// SIGSEGV handler return state
enum sigsegv_return_t {
  SIGSEGV_RETURN_SUCCESS,
  SIGSEGV_RETURN_FAILURE,
  SIGSEGV_RETURN_SKIP_INSTRUCTION
};

// Type of a SIGSEGV handler. Returns boolean expressing successful operation
typedef sigsegv_return_t (*sigsegv_fault_handler_t)(sigsegv_info_t *sip);

// Type of a SIGSEGV state dump function
typedef void (*sigsegv_state_dumper_t)(sigsegv_info_t *sip);

// Install a SIGSEGV handler. Returns boolean expressing success
extern bool sigsegv_install_handler(sigsegv_fault_handler_t handler);

// Remove the user SIGSEGV handler, revert to default behavior
extern void sigsegv_uninstall_handler(void);

// Set callback function when we cannot handle the fault
extern void sigsegv_set_dump_state(sigsegv_state_dumper_t handler);

// Return the address of the invalid memory reference
extern sigsegv_address_t sigsegv_get_fault_address(sigsegv_info_t *sip);

// Return the address of the instruction that caused the fault, or
// SIGSEGV_INVALID_ADDRESS if we could not retrieve this information
extern sigsegv_address_t sigsegv_get_fault_instruction_address(sigsegv_info_t *sip);

// Define an address that is bound to be invalid for a program counter
const sigsegv_address_t SIGSEGV_INVALID_ADDRESS = sigsegv_address_t(-1);

#endif /* SIGSEGV_H */
