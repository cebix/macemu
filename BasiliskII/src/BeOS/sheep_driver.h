/*
 *  sheep_driver.h - Driver for SheepShaver (low memory, ROM access)
 *
 *  SheepShaver (C) 1997-1999 Mar"c" Hellwig and Christian Bauer
 *  All rights reserved.
 */

#ifndef SHEEP_DRIVER_H
#define SHEEP_DRIVER_H

#include <drivers/Drivers.h>

enum {
	SHEEP_UP = B_DEVICE_OP_CODES_END + 1,
	SHEEP_DOWN
};

#endif
