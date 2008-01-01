/*
 *  ether_dummy.cpp - Ethernet device driver, dummy implementation
 *
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

#include "sysdeps.h"

#include "cpu_emulation.h"
#include "main.h"
#include "ether.h"
#include "ether_defs.h"

#define DEBUG 0
#include "debug.h"


/*
 *  Init ethernet
 */

void EtherInit(void)
{
}


/*
 *  Exit ethernet
 */

void EtherExit(void)
{
}


/*
 *  Get ethernet hardware address
 */

void AO_get_ethernet_address(uint32 addr)
{
}


/*
 *  Enable multicast address
 */

void AO_enable_multicast(uint32 addr)
{
}


/*
 *  Disable multicast address
 */

void AO_disable_multicast(uint32 addr)
{
}


/*
 *  Transmit one packet
 */

void AO_transmit_packet(uint32 mp)
{
}


/*
 *  Ethernet interrupt
 */

void EtherIRQ(void)
{
}
