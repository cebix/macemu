/*
 *  jit-cache.cpp - Translation cache management
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

#include "sysdeps.h"
#include "vm_alloc.h"
#include "cpu/jit/jit-cache.hpp"

#define DEBUG 0
#include "debug.h"

// Default cache size in KB
#if defined(__alpha__)
const int JIT_CACHE_SIZE = 2 * 1024;
#elif defined(__powerpc__) || defined(__ppc__)
const int JIT_CACHE_SIZE = 4 * 1024;
#else
const int JIT_CACHE_SIZE = 8 * 1024;
#endif
const int JIT_CACHE_SIZE_GUARD = 4096;

basic_jit_cache::basic_jit_cache()
	: cache_size(0), tcode_start(NULL), code_start(NULL), code_p(NULL), code_end(NULL), data(NULL)
{
}

basic_jit_cache::~basic_jit_cache()
{
	kill_translation_cache();

	// Release data pool
	data_chunk_t *p = data;
	while (p) {
		data_chunk_t *d = p;
		p = p->next;
		D(bug("basic_jit_cache: Release data pool %p (%d KB)\n", d, d->size / 1024));
		vm_release(d, d->size);
	}
}

bool
basic_jit_cache::init_translation_cache(uint32 size)
{
	size *= 1024;

	// Round up translation cache size to 16 KB boundaries
	const uint32 roundup = 16 * 1024;
	cache_size = (size + JIT_CACHE_SIZE_GUARD + roundup - 1) & -roundup;
	assert(cache_size > 0);

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
	
  done:
	D(bug("basic_jit_cache: Translation cache: %d KB at %p\n", cache_size / 1024, tcode_start));
	code_start = tcode_start;
	code_p = code_start;
	code_end = code_p + size;
	return true;
}

void
basic_jit_cache::kill_translation_cache()
{
	if (tcode_start) {
		D(bug("basic_jit_cache: Release translation cache\n"));
		vm_release(tcode_start, cache_size);
		cache_size = 0;
		tcode_start = NULL;
	}
}

bool
basic_jit_cache::initialize(void)
{
	if (cache_size == 0)
		set_cache_size(JIT_CACHE_SIZE);
	return tcode_start && cache_size;
}

void
basic_jit_cache::set_cache_size(uint32 size)
{
	kill_translation_cache();
	if (size)
		init_translation_cache(size);
}

uint8 *
basic_jit_cache::copy_data(const uint8 *block, uint32 size)
{
	const int ALIGN = 16;
	uint8 *ptr;

	if (data && (data->offs + size) < data->size)
		ptr = (uint8 *)data + data->offs;
	else {
		// No free space left, allocate a new chunk
		uint32 to_alloc = sizeof(*data) + size + ALIGN;
		uint32 page_size = vm_get_page_size();
		to_alloc = (to_alloc + page_size - 1) & -page_size;

		D(bug("basic_jit_cache: Allocate data pool (%d KB)\n", to_alloc / 1024));
		ptr = (uint8 *)vm_acquire(to_alloc, VM_MAP_PRIVATE | VM_MAP_32BIT);
		if (ptr == VM_MAP_FAILED) {
			fprintf(stderr, "FATAL: Could not allocate data pool!\n");
			abort();
		}

		data_chunk_t *dcp = (data_chunk_t *)ptr;
		dcp->size = to_alloc;
		dcp->offs = (sizeof(*data) + ALIGN - 1) & -ALIGN;
		dcp->next = data;
		data = dcp;

		ptr += dcp->offs;
	}

	memcpy(ptr, block, size);
	data->offs += (size + ALIGN - 1) & -ALIGN;
	D(bug("basic_jit_cache: DATA %p, %d bytes [data=%p, offs=%u]\n", ptr, size, data, data->offs));
	return ptr;
}
