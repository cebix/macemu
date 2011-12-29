/*
 *	utils_macosx.mm - Mac OS X utility functions.
 *
 *  Copyright (C) 2011 Alexei Svitkine
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

#include <Cocoa/Cocoa.h>
#include "utils_macosx.h"

// This is used from video_sdl.cpp.
void NSAutoReleasePool_wrap(void (*fn)(void))
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	fn();
	[pool release];
}
