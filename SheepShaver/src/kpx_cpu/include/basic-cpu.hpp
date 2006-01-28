/*
 *  basic-cpu.hpp - Basic CPU definitions
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
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

#ifndef BASIC_CPU_H
#define BASIC_CPU_H

#include "sysdeps.h"
#include <setjmp.h>
#include "task-plugin.hpp"

/**
 *		Generic register value
 **/

union any_register
{
	uint32 i;
	uint64 j;
	float  f;
	double d;
	
	// Explicit casts may be required to use those constructors
	any_register(uint32 v = 0)	: i(v) { }
	any_register(uint64 v)		: j(v) { }
	any_register(float v)		: f(v) { }
	any_register(double v)		: d(v) { }
};

/**
 *		Basic CPU model
 **/

struct task_struct;

struct basic_cpu
	: public task_plugin
{
	// Basic register set
	struct registers
	{
		enum {
			PC = -1,	// Program Counter
			SP = -2,	// Stack Pointer
		};
	};

	// Constructor & destructor
	basic_cpu(task_struct * parent_task);
	virtual ~basic_cpu();

	// Execute code at current address
	virtual void execute() = 0;

	// Set VALUE to register ID
	virtual void set_register(int id, any_register const & value) = 0;

	// Get register ID
	virtual any_register get_register(int id) = 0;

	// Start emulation, returns exit status
	int run();

	// Stop emulation
	void exit(int status);

private:
	jmp_buf env;
	int exit_status;
};

// Alias basic register set
typedef basic_cpu::registers basic_registers;

#endif /* BASIC_CPU_H */
