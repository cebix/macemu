/*
 *  mon_disass.h - Disassemblers
 *
 *  cxmon (C) 1997-2004 Christian Bauer, Marc Hellwig
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

#ifndef MON_DISASS_H
#define MON_DISASS_H

extern void disass_ppc(FILE *f, unsigned int adr, unsigned int w);
extern int disass_68k(FILE *f, uint32 adr);
extern int disass_x86(FILE *f, uint32 adr, uint32 bits = 32);
extern int disass_6502(FILE *f, uint32 adr, uint8 op, uint8 lo, uint8 hi);
extern int disass_z80(FILE *f, uint32 adr);

#endif
