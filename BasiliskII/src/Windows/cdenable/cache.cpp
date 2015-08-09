/*
 *  cache.cpp - simple floppy/cd cache for Win32
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
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

/*
	Note that this is particularly silly cache code
	and doesn't even use hash buckets. It is sufficient
	for floppies and maybe emulated cd's but that's it.
*/

#include "sysdeps.h"
#include "cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void cache_clear( cachetype *cptr )
{
	if(cptr->inited) {
		cptr->res_count = 0;
		memset( cptr->LRU, 0, NBLOCKS * sizeof(int) );
	}
}

static int init( cachetype *cptr, int sector_size )
{
	cache_clear( cptr );
	cptr->sector_size = sector_size;
	cptr->blocks = (char *)VirtualAlloc(
			NULL, NBLOCKS*sector_size,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	cptr->block = (int *)VirtualAlloc(
			NULL, NBLOCKS*sizeof(int),
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	cptr->LRU = (DWORD *)VirtualAlloc(
			NULL, NBLOCKS*sizeof(DWORD),
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	return(cptr->blocks != NULL);
}

static void final( cachetype *cptr )
{
	if(cptr->blocks) {
		VirtualFree( cptr->blocks, 0, MEM_RELEASE  );
		cptr->blocks = 0;
	}
	if(cptr->block) {
		VirtualFree( cptr->block, 0, MEM_RELEASE  );
		cptr->block = 0;
	}
	if(cptr->LRU) {
		VirtualFree( cptr->LRU, 0, MEM_RELEASE  );
		cptr->LRU = 0;
	}
	cptr->inited = 0;
}

void cache_init( cachetype *cptr )
{
	cptr->inited = 0;
}

void cache_final( cachetype *cptr )
{
	if(cptr->inited) {
		final( cptr );
		cptr->inited = 0;
	}
}

static int in_cache( cachetype *cptr, int block )
{
	int i;
	for(i=cptr->res_count-1; i>=0; i--) {
		if(cptr->block[i] == block) return(i);
	}
	return(-1);
}

static int get_LRU( cachetype *cptr )
{
	int i, result = 0;
	DWORD mtime = cptr->LRU[0];

	for(i=1; i<NBLOCKS; i++) {
		if(cptr->LRU[i] < mtime) {
			mtime = cptr->LRU[i];
			result = i;
		}
	}
	return(result);
}

void cache_put( cachetype *cptr, int block, char *buf, int ss )
{
	int inx;

	if(!cptr->inited) {
		if(!init(cptr,ss)) return;
		cptr->inited = 1;
	}
	inx = in_cache( cptr, block );
	if(inx < 0) {
		if(cptr->res_count == NBLOCKS) {
			inx = get_LRU( cptr );
		} else {
			inx = cptr->res_count++;
		}
		cptr->block[inx] = block;
	}
	cptr->LRU[inx] = GetTickCount();
	memcpy( cptr->blocks + inx * ss, buf, ss );
}

int cache_get( cachetype *cptr, int block, char *buf )
{
	int inx;

	if(!cptr->inited) return(0);

	inx = in_cache( cptr, block );
	if(inx >= 0) {
		memcpy( buf, cptr->blocks + inx * cptr->sector_size, cptr->sector_size );
		return(1);
	} else {
		return(0);
	}
}

void cache_remove( cachetype *cptr, int block, int ss )
{
	int inx, from;

	if(!cptr->inited) {
		if(!init(cptr,ss)) return;
		cptr->inited = 1;
	}
	inx = in_cache( cptr, block );
	if(inx >= 0) {
		if(cptr->res_count > 1) {
			from = cptr->res_count-1;
			cptr->block[inx] = cptr->block[from];
			cptr->LRU[inx]   = cptr->LRU[from];
			memcpy(
				cptr->blocks + inx  * cptr->sector_size,
				cptr->blocks + from * cptr->sector_size,
				cptr->sector_size
			);
		}
		cptr->res_count--;
	}
}

#ifdef __cplusplus
} // extern "C"
#endif
