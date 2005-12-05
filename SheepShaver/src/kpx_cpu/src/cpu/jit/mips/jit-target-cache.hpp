/*
 *  jit-target-cache.hpp - Target specific code to invalidate cache
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

#ifndef JIT_TARGET_CACHE_H
#define JIT_TARGET_CACHE_H

#if defined __sgi
#include <sys/cachectl.h>
static inline void flush_icache_range(unsigned long start, unsigned long stop)
{
	cacheflush((void *)start, stop - start, BCACHE);
}
#elif defined __GNUC__
#error "FIXME: implement assembly code for code cache invalidation"
#endif

#endif /* JIT_TARGET_CACHE_H */
