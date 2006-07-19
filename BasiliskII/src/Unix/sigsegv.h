/*
 *  sigsegv.h - SIGSEGV signals support
 *
 *  Derived from Bruno Haible's work on his SIGSEGV library for clisp
 *  <http://clisp.sourceforge.net/>
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

#ifndef SIGSEGV_H
#define SIGSEGV_H

// Address type
typedef char * sigsegv_address_t;

// SIGSEGV handler return state
enum sigsegv_return_t {
  SIGSEGV_RETURN_SUCCESS,
  SIGSEGV_RETURN_FAILURE,
  SIGSEGV_RETURN_SKIP_INSTRUCTION,
};

// Type of a SIGSEGV handler. Returns boolean expressing successful operation
typedef sigsegv_return_t (*sigsegv_fault_handler_t)(sigsegv_address_t fault_address, sigsegv_address_t instruction_address);

// Type of a SIGSEGV state dump function
typedef void (*sigsegv_state_dumper_t)(sigsegv_address_t fault_address, sigsegv_address_t instruction_address);

// Install a SIGSEGV handler. Returns boolean expressing success
extern bool sigsegv_install_handler(sigsegv_fault_handler_t handler);

// Remove the user SIGSEGV handler, revert to default behavior
extern void sigsegv_uninstall_handler(void);

// Set callback function when we cannot handle the fault
extern void sigsegv_set_dump_state(sigsegv_state_dumper_t handler);

// Define an address that is bound to be invalid for a program counter
const sigsegv_address_t SIGSEGV_INVALID_PC = (sigsegv_address_t)(-1);

#endif /* SIGSEGV_H */
