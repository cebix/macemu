/*
 *  clip_dummy.cpp - Clipboard handling, dummy implementation
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#include "clip.h"
#include "macos_util.h"

#define DEBUG 0
#include "debug.h"


/*
 *  Initialization
 */

void ClipInit(void)
{
}


/*
 *  Deinitialization
 */

void ClipExit(void)
{
}


/*
 *  Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));
}


/*
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %08lx, length %ld\n", type, scrap, length));
	if (length <= 0)
		return;

	switch (type) {
		case FOURCC('T','E','X','T'):
			D(bug(" clipping TEXT\n"));
			break;
	}
}
