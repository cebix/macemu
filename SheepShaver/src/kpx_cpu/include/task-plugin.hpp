/*
 *  task-plugin.hpp - Task plugin definition
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

#ifndef TASK_PLUGIN_H
#define TASK_PLUGIN_H

#include "basic-plugin.hpp"

// Forward declarations
class task_struct;
class basic_kernel;
class basic_cpu;
class program_info;

// Base class for all task components
class task_plugin
	: public basic_plugin
{
	// Parent task
	task_struct * task;

public:

	// Constructor
	task_plugin(task_struct * parent_task) : task(parent_task) { }

	// Public accessors to resolve various components of a task
	basic_kernel * kernel() const;
	basic_cpu * cpu() const;
	program_info * program() const;
};

// Get out of specified task
extern void task_exit(task_plugin *task, int status);

#endif /* TASK_PLUGIN_H */
