/*
 *  jit-cache.cpp - Translation cache management
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

#include "sysdeps.h"
#include "vm_alloc.h"
#include "cpu/jit/jit-cache.hpp"

#define DEBUG 0
#include "debug.h"

basic_jit_cache::basic_jit_cache(uint32 init_cache_size)
	: tcode_start(NULL), code_start(NULL), code_p(NULL), code_end(NULL)
{
	init_translation_cache(init_cache_size);
}

basic_jit_cache::~basic_jit_cache()
{
	kill_translation_cache();
}

bool
basic_jit_cache::init_translation_cache(uint32 size)
{
	// Round up translation cache size to next 16 KB boundaries
	const uint32 roundup = 16 * 1024;
	cache_size = (size + JIT_CACHE_SIZE_GUARD + roundup - 1) & -roundup;

	tcode_start = (uint8 *)vm_acquire(cache_size, VM_MAP_PRIVATE | VM_MAP_32BIT);
	if (tcode_start == VM_MAP_FAILED) {
		tcode_start = NULL;
		return false;
	}

	if (vm_protect(tcode_start, cache_size,
				   VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		vm_release(tcode_start, cache_size);
		tcode_start = NULL;
		return false;
	}
	
	D(bug("basic_jit_cache: Translation cache: %d KB at %p\n", cache_size / 1024, tcode_start));
	code_start = tcode_start;
	code_p = code_start;
	code_end = code_p + cache_size;
	return true;
}

void
basic_jit_cache::kill_translation_cache()
{
	if (tcode_start)
		vm_release(tcode_start, cache_size);
}
