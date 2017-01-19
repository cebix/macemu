/*
 *  mon_disass.cpp - Disassemblers
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

#include "sysdeps.h"

#include <stdarg.h>

#include "mon.h"
#include "mon_disass.h"

#include "mon_atraps.h"
#include "mon_lowmem.h"


// Flag: enable MacOS A-Trap and LM globals lookup in 68k disassembler
bool mon_macos_mode = false;


/*
 *  GNU disassembler callbacks
 */

extern "C" {
#include "disass/dis-asm.h"

int buffer_read_memory(bfd_vma from, bfd_byte *to, unsigned int length, struct disassemble_info *info)
{
	while (length--)
		*to++ = mon_read_byte(from++);
	return 0;
}

void perror_memory(int status, bfd_vma memaddr, struct disassemble_info *info)
{
	info->fprintf_func(info->stream, "Unknown error %d\n", status);
}

bool lookup_lowmem;

void generic_print_address(bfd_vma addr, struct disassemble_info *info)
{
	if (lookup_lowmem && addr >= 0x100 && addr < 0x3000) {
		if (((addr >= 0x400 && addr < 0x800) || (addr >= 0xe00 && addr < 0x1e00)) && ((addr & 3) == 0)) {
			// Look for address in A-Trap table
			uint16 opcode = (addr < 0xe00 ? 0xa000 + (addr - 0x400) / 4 : 0xa800 + (addr - 0xe00) / 4);
			uint16 mask = (addr < 0xe00 ? 0xf8ff : 0xffff);
			const atrap_info *p = atraps;
			while (p->word) {
				if ((p->word & mask) == opcode) {
					info->fprintf_func(info->stream, p->name);
					return;
				}
				p++;
			}
		} else {
			// Look for address in low memory globals table
			const lowmem_info *p = lowmem;
			while (p->name) {
				if (addr >= p[0].addr && addr < p[1].addr) {
					if (addr == p[0].addr)
						info->fprintf_func(info->stream, "%s", p->name);
					else
						info->fprintf_func(info->stream, "%s+%d", p->name, addr - p->addr);
					return;
				}
				p++;
			}
		}
	}
	if (addr >= UVAL64(0x100000000))
		info->fprintf_func(info->stream, "$%08x%08x", (uint32)(addr >> 32), (uint32)addr);
	else
		info->fprintf_func(info->stream, "$%08x", (uint32)addr);
}

int generic_symbol_at_address(bfd_vma addr, struct disassemble_info *info)
{
	return 0;
}

void print_68k_invalid_opcode(unsigned long opcode, struct disassemble_info *info)
{
	if (mon_macos_mode) {
		// Look for MacOS A-Trap
		const atrap_info *p = atraps;
		while (p->word) {
			if (p->word == opcode) {
				info->fprintf_func(info->stream, p->name);
				return;
			}
			p++;
		}
	}
	info->fprintf_func(info->stream, "?");
}

};


/*
 *  sprintf into a "stream"
 */

struct SFILE {
	char *buffer;
	char *current;
};

static int mon_sprintf(SFILE *f, const char *format, ...)
{
	int n;
	va_list args;
	va_start(args, format);
	vsprintf(f->current, format, args);
	f->current += n = strlen(f->current);
	va_end(args);
	return n;
}


/*
 *  Disassemble one instruction, return number of bytes
 */

int disass_68k(FILE *f, uint32 adr)
{
	// Initialize info for GDB disassembler
	disassemble_info info;
	char buf[1024];
	SFILE sfile = {buf, buf};
	sfile.buffer = buf;
	sfile.current = buf;
	INIT_DISASSEMBLE_INFO(info, (FILE *)&sfile, (fprintf_ftype)mon_sprintf);

	// Disassemble instruction
	lookup_lowmem = mon_macos_mode;
	int num = print_insn_m68k(adr, &info);

	for (int i=0; i<6; i+=2) {
		if (num > i)
			fprintf(f, "%04x ", mon_read_half(adr + i));
		else
			fprintf(f, "     ");
	}
	if (num == 8)
		fprintf(f, "%04x\t%s\n", mon_read_half(adr + 6), buf);
	else if (num > 8)
		fprintf(f, "...\t%s\n", buf);
	else
		fprintf(f, "   \t%s\n", buf);

	return num;
}

int disass_x86(FILE *f, uint32 adr, uint32 bits)
{
	// Initialize info for GDB disassembler
	disassemble_info info;
	char buf[1024];
	SFILE sfile = {buf, buf};
	sfile.buffer = buf;
	sfile.current = buf;
	INIT_DISASSEMBLE_INFO(info, (FILE *)&sfile, (fprintf_ftype)mon_sprintf);
	if (bits == 16)
		info.mach = bfd_mach_i386_i8086;
	else if (bits == 64)
		info.mach = bfd_mach_x86_64;

	// Disassemble instruction
	lookup_lowmem = false;
	int num = print_insn_i386_att(adr, &info);

	for (int i=0; i<6; i++) {
		if (num > i)
			fprintf(f, "%02x ", mon_read_byte(adr + i));
		else
			fprintf(f, "   ");
	}
	if (num == 7)
		fprintf(f, "%02x\t%s\n", mon_read_byte(adr + 7), buf);
	else if (num > 7)
		fprintf(f, "..\t%s\n", buf);
	else
		fprintf(f, "  \t%s\n", buf);

	return num;
}
