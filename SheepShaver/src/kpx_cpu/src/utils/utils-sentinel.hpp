/*
 *  utils-sentinel.hpp - Helper functions for program initialization and termination
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

#ifndef UTILS_SENTINEL_H
#define UTILS_SENTINEL_H

class program_sentinel {
	void (*fini)(void);
public:
	program_sentinel(void (*init_func)(void) = 0, void (*exit_func)(void) = 0)
		: fini(exit_func)
		{ if (init_func) init_func(); }
	~program_sentinel()
		{ if (fini) fini(); }
};

#define DEFINE_INIT_SENTINEL(FUNCTION)														\
static void FUNCTION(void);																	\
static program_sentinel g_program_init_sentinel__##FUNCTION(FUNCTION)

#define DEFINE_EXIT_SENTINEL(FUNCTION)														\
static void FUNCTION(void);																	\
static program_sentinel g_program_exit_sentinel__##FUNCTION(0, FUNCTION)

#define DEFINE_PROG_SENTINEL(FUNCTION)														\
static void init_##FUNCTION(void);															\
static void exit_##FUNCTION(void);															\
static program_sentinel g_program_sentinel_##FUNCTION(init_##FUNCTION, exit_##FUNCTION)

#endif /* UTILS_SENTINEL_H */
