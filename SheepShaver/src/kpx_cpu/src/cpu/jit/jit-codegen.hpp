/*
 *  jit-codegen.hpp - Generic code generator
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

#ifndef JIT_CODEGEN_H
#define JIT_CODEGEN_H

#include "cpu/jit/jit-cache.hpp"

#if defined(__i386__)
#include "cpu/jit/x86/jit-target-codegen.hpp"
typedef x86_codegen jit_codegen;
#elif defined(__x86_64__)
#include "cpu/jit/amd64/jit-target-codegen.hpp"
typedef amd64_codegen jit_codegen;
#else
struct jit_codegen
	: public basic_jit_cache
{
};
#endif

#endif /* JIT_CODEGEN_H */
