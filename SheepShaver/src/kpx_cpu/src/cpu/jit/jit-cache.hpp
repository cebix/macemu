/*
 *  jit-cache.hpp - Translation cache management
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

#ifndef JIT_CACHE_H
#define JIT_CACHE_H

/**
 *		Basic translation cache
 **/

class basic_jit_cache
{
	// Default cache size (2 MB)
	static const uint32 JIT_CACHE_SIZE = 2 * 1024 * 1024;
	static const uint32 JIT_CACHE_SIZE_GUARD = 4096;
	uint32 cache_size;

	// Translation cache (allocated base, current pointer, end pointer)
	uint8 *tcode_start;
	uint8 *code_start;
	uint8 *code_p;
	uint8 *code_end;

protected:

	// Initialize translation cache
	bool init_translation_cache(uint32 size);
	void kill_translation_cache();

	// Initialize user code start
	void set_code_start(uint8 *ptr);

	// Get & increase current position
	void inc_code_ptr(int offset)	{ code_p += offset; }
public:
	uint8 *code_ptr() const			{ return code_p; }

public:

	// Default constructor & destructor
	basic_jit_cache(uint32 init_cache_size = JIT_CACHE_SIZE);
	~basic_jit_cache();

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
