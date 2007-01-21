/*
 *  jit-cache.hpp - Translation cache management
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

#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#define _JIT_HEADER jit-target-cache.hpp
#include "cpu/jit/jit-target-dispatch.h"

/**
 *		Basic translation cache
 **/

class basic_jit_cache
{
	// Translation cache (allocated base, current pointer, end pointer)
	uint32 cache_size;
	uint8 *tcode_start;
	uint8 *code_start;
	uint8 *code_p;
	uint8 *code_end;

	// Data pool (32-bit addressable)
	struct data_chunk_t {
		uint32 size;
		uint32 offs;
		data_chunk_t *next;
	};
	data_chunk_t *data;

protected:

	// Initialize translation cache
	bool init_translation_cache(uint32 size);
	void kill_translation_cache();

	// Initialize user code start
	void set_code_start(uint8 *ptr);

	// Increase/set/get current position
	void inc_code_ptr(int offset)	{ code_p += offset; }
	void set_code_ptr(uint8 *ptr)	{ code_p = ptr; }
public:
	uint8 *code_ptr() const			{ return code_p; }

public:

	// Default constructor & destructor (use default JIT_CACHE_SIZE)
	basic_jit_cache();
	~basic_jit_cache();

	bool initialize(void);
	void set_cache_size(uint32 size);

	// Invalidate translation cache
	void invalidate_cache();
	bool full_translation_cache() const
		{ return code_p >= code_end; }

	// Emit code to translation cache
	template< typename T >
	void emit_generic(T v);
	void emit_8(uint8 v)		{ emit_generic<uint8>(v); }
	void emit_16(uint16 v)		{ emit_generic<uint16>(v); }
	void emit_32(uint32 v)		{ emit_generic<uint32>(v); }
	void emit_64(uint64 v)		{ emit_generic<uint64>(v); }
	void emit_ptr(uintptr v)	{ emit_generic<uintptr>(v); }
	void copy_block(const uint8 *block, uint32 size);
	void emit_block(const uint8 *block, uint32 size);

	// Emit data to constant pool
	uint8 *copy_data(const uint8 *block, uint32 size);
};

inline void
basic_jit_cache::set_code_start(uint8 *ptr)
{
	assert(ptr >= tcode_start && ptr < code_end);
	code_start = ptr;
}

inline void
basic_jit_cache::invalidate_cache()
{
	code_p = code_start;
}

template< class T >
inline void
basic_jit_cache::emit_generic(T v)
{
	*((T *)code_ptr()) = v;
	inc_code_ptr(sizeof(T));
}

inline void
basic_jit_cache::copy_block(const uint8 *block, uint32 size)
{
	memcpy(code_ptr(), block, size);
}

inline void
basic_jit_cache::emit_block(const uint8 *block, uint32 size)
{
	copy_block(block, size);
	inc_code_ptr(size);
}

#endif /* JIT_CACHE_H */
