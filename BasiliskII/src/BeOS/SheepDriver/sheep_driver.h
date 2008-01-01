/*
 *  sheep_driver.h - Low memory and ROM access driver for SheepShaver and
 *                   Basilisk II on PowerPC systems
 *
 *  SheepShaver (C) 1997-2008 Marc Hellwig and Christian Bauer
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#ifndef SHEEP_DRIVER_H
#define SHEEP_DRIVER_H

#include <drivers/Drivers.h>

enum {
	SHEEP_UP = B_DEVICE_OP_CODES_END + 1,
	SHEEP_DOWN
};

#endif
