/*
 *  block-cache.hpp - Basic block cache management
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

#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include "block-alloc.hpp"

template< class block_info, template<class T> class block_allocator = slow_allocator >
class block_cache
{
private:
	static const uint32 HASH_BITS = 15;
	static const uint32 HASH_SIZE = 1 << HASH_BITS;
	static const uint32 HASH_MASK = HASH_SIZE - 1;

	struct entry
		: public block_info
	{
		entry *					next_same_cl;
		entry **				prev_same_cl_p;
		entry *					next;
		entry **				prev_p;
	};

	block_allocator<entry>		allocator;
	entry *						cache_tags[HASH_SIZE];
	entry *						active;
	entry *						dormant;

	uint32 cacheline(uintptr addr) const {
		return (addr >> 2) & HASH_MASK;
	}

public:

	block_cache();
	~block_cache();

	block_info *new_blockinfo();
	void delete_blockinfo(block_info *bi);

	void initialize();
	void clear();
	void clear_range(uintptr start, uintptr end);
	block_info *fast_find(uintptr pc);
	block_info *find(uintptr pc);

	void remove_from_cl_list(block_info *bi);
	void remove_from_list(block_info *bi);
	void remove_from_lists(block_info *bi);

	void add_to_cl_list(block_info *bi);
	void raise_in_cl_list(block_info *bi);

	void add_to_active_list(block_info *bi);
	void add_to_dormant_list(block_info *bi);
};

template< class block_info, template<class T> class block_allocator >
block_cache< block_info, block_allocator >::block_cache()
	: active(NULL), dormant(NULL)
{
	initialize();
}

template< class block_info, template<class T> class block_allocator >
inline block_cache< block_info, block_allocator >::~block_cache()
{
	clear();
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::initialize()
{
	for (int i = 0; i < HASH_SIZE; i++)
		cache_tags[i] = NULL;
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::clear()
{
	entry *p;

	p = active;
	while (p) {
		entry *d = p;
		p = p->next;
		delete_blockinfo(d);
	}
	active = NULL;

	p = dormant;
	while (p) {
		entry *d = p;
		p = p->next;
		delete_blockinfo(d);
	}
	dormant = NULL;
}

template< class block_info, template<class T> class block_allocator >
inline void block_cache< block_info, block_allocator >::clear_range(uintptr start, uintptr end)
{
	if (!active)
		return;

	entry *q;
	entry *p = active;
	while (p) {
		q = p;
		p = p->next;
		if (q->intersect(start, end)) {
			q->invalidate();
			remove_from_cl_list(q);
			remove_from_list(q);
			delete_blockinfo(q);
		}
	}
}

template< class block_info, template<class T> class block_allocator >
inline block_info *block_cache< block_info, block_allocator >::new_blockinfo()
{
	entry * bce = allocator.acquire();
	return bce;
}

template< class block_info, template<class T> class block_allocator >
inline void block_cache< block_info, block_allocator >::delete_blockinfo(block_info *bi)
{
	entry * bce = (entry *)bi;
	allocator.release(bce);
}

template< class block_info, template<class T> class block_allocator >
inline block_info *block_cache< block_info, block_allocator >::fast_find(uintptr pc)
{
	// Hit: return immediately (that covers more than 95% of the cases)
	entry * bce = cache_tags[cacheline(pc)];
	if (bce && bce->pc == pc)
		return bce;

	return NULL;
}

template< class block_info, template<class T> class block_allocator >
block_info *block_cache< block_info, block_allocator >::find(uintptr pc)
{
	// Hit: return immediately
	entry * bce = cache_tags[cacheline(pc)];
	if (bce && bce->pc == pc)
		return bce;

	// Miss: perform full list search and move block to front if found
	while (bce) {
		bce = bce->next_same_cl;
		if (bce && bce->pc == pc) {
			raise_in_cl_list(bce);
			return bce;
		}
	}

	// Found none, will have to create a new block
	return NULL;
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::remove_from_cl_list(block_info *bi)
{
	entry * bce = (entry *)bi;
	if (bce->prev_same_cl_p)
		*bce->prev_same_cl_p = bce->next_same_cl;
	if (bce->next_same_cl)
		bce->next_same_cl->prev_same_cl_p = bce->prev_same_cl_p;
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::add_to_cl_list(block_info *bi)
{
	entry * bce = (entry *)bi;
	const uint32 cl = cacheline(bi->pc);
	if (cache_tags[cl])
		cache_tags[cl]->prev_same_cl_p = &bce->next_same_cl;
	bce->next_same_cl = cache_tags[cl];
	
	cache_tags[cl] = bce;
	bce->prev_same_cl_p = &cache_tags[cl];
}

template< class block_info, template<class T> class block_allocator >
inline void block_cache< block_info, block_allocator >::raise_in_cl_list(block_info *bi)
{
	remove_from_cl_list(bi);
	add_to_cl_list(bi);
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::remove_from_list(block_info *bi)
{
	entry * bce = (entry *)bi;
	if (bce->prev_p)
		*bce->prev_p = bce->next;
	if (bce->next)
		bce->next->prev_p = bce->prev_p;
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::add_to_active_list(block_info *bi)
{
	entry * bce = (entry *)bi;
	
	if (active)
		active->prev_p = &bce->next;
	bce->next = active;
	
	active = bce;
	bce->prev_p = &active;
}

template< class block_info, template<class T> class block_allocator >
void block_cache< block_info, block_allocator >::add_to_dormant_list(block_info *bi)
{
	entry * bce = (entry *)bi;
	
	if (dormant)
		dormant->prev_p = &bce->next;
	bce->next = dormant;
	
	dormant = bce;
	bce->prev_p = &dormant;
}

template< class block_info, template<class T> class block_allocator >
inline void block_cache< block_info, block_allocator >::remove_from_lists(block_info *bi)
{
	remove_from_cl_list(bi);
	remove_from_list(bi);
}

#endif /* BLOCK_CACHE_H */
