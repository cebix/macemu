/*
 *  xpram.cpp - XPRAM handling
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Operating System Utilities, chapter 7 "Parameter RAM Utilities"
 */

#include <string.h>

#include "sysdeps.h"
#include "xpram.h"


// Extended parameter RAM
uint8 XPRAM[XPRAM_SIZE];


/*
 *  Initialize XPRAM
 */

void XPRAMInit(void)
{
	// Clear XPRAM
	memset(XPRAM, 0, XPRAM_SIZE);

	// Load XPRAM from settings file
	LoadXPRAM();
}


/*
 *  Deinitialize XPRAM
 */

void XPRAMExit(void)
{
	// Save XPRAM to settings file
	SaveXPRAM();
}
