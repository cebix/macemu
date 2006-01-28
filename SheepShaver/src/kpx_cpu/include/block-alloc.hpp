/*
 *  block_alloc.hpp - Memory allocation in blocks
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

#ifndef BLOCK_ALLOC_H
#define BLOCK_ALLOC_H

/**
 *	Slow memory allocator
 *
 *		Each time a DATA item is requested (resp. released), the block
 *		is immediately allocated (resp. free'd).
 **/

template< class data >
struct slow_allocator
{
	data * acquire() const { return new data; }
	void release(data * const x) const { delete x; }
};

/**
 *	Lazy memory allocator
 *
 *		Allocate a big memory block, typically larger than a page,
 *		that contains up to POOL_COUNT data items of type DATA.
 **/

template< class data >
class lazy_allocator
{
	static const int pool_size = 4096;
	static const int pool_count = 1 + pool_size / sizeof(data);

	struct chunk
	{
		data    value;
		chunk * next;
	};

	struct pool
	{
		chunk  chunks[pool_count];
		pool * next;
	};

	pool *  pools;
	chunk * chunks;

public:
	lazy_allocator() : pools(0), chunks(0) { }
	~lazy_allocator();
	data * acquire();
	void release(data * const);
};

template< class data >
lazy_allocator<data>::~lazy_allocator()
{
	pool * p = pools;
	while (p) {
		pool * d = p;
		p = p->next;
		delete d;
	}
}

template< class data >
data * lazy_allocator<data>::acquire()
{
	if (!chunks) {
		// There is no chunk left, allocate a new pool and link the
		// chunks into the free list
		pool * p = new pool;
		for (chunk * c = &p->chunks[0]; c < &p->chunks[pool_count]; c++) {
			c->next = chunks;
			chunks = c;
		}
		p->next = pools;
		pools = p;
	}
	chunk * c = chunks;
	chunks = c->next;
	return &c->value;
}

template< class data >
void lazy_allocator<data>::release(data * const d)
{
	chunk *c = (chunk *)d;
	c->next = chunks;
	chunks = c;
}

/**
 *		Helper memory allocator
 **/

template< class data_type, template< class > class allocator_type = lazy_allocator >
class allocator_helper
{
	static allocator_type<data_type> allocator;
public:
	static inline void *allocate()
		{ return allocator.acquire(); }
	static inline void deallocate(void * p)
		{ allocator.release((data_type *)p); }
};

#endif /* BLOCK_ALLOC_H */
