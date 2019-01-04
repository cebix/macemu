/*
 *  mon_cmd.cpp - cxmon standard commands
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

#include <stdlib.h>
#include <assert.h>

#include "mon.h"
#include "mon_cmd.h"
#include "mon_disass.h"

#ifndef VERSION
#define VERSION "3"
#endif


/*
 *  range_args = [expression] [[COMMA] expression] END
 *
 *  Read start address to "adr", end address to "end_adr".
 *  "adr" defaults to '.', "end_adr" defaults to '.'+def_range
 *
 *  true: OK, false: Error
 */

static bool range_args(uintptr *adr, uintptr *end_adr, uint32 def_range)
{
	*adr = mon_dot_address;
	*end_adr = mon_dot_address + def_range;

	if (mon_token == T_END)
		return true;
	else {
		if (!mon_expression(adr))
			return false;
		*end_adr = *adr + def_range;
		if (mon_token == T_END)
			return true;
		else {
			if (mon_token == T_COMMA) mon_get_token();
			if (!mon_expression(end_adr))
				return false;
			return mon_token == T_END;
		}
	}
}


/*
 *  byte_string = (expression | STRING) {COMMA (expression | STRING)} END
 */

static bool byte_string(uint8 *&str, uintptr &len)
{
	uintptr value;

	static const int GRANULARITY = 16; // must be a power of 2
	str = NULL;
	len = 0;
	goto start;

	for (;;) {
		if (mon_token == T_COMMA) {
			mon_get_token();

start:
			if (mon_token == T_STRING) {
				unsigned n = strlen(mon_string);
				str = (uint8 *)realloc(str, (len + n - 1 + GRANULARITY) & ~(GRANULARITY - 1));
				assert(str != NULL);
				memcpy(str + len, mon_string, n);
				len += n;
				mon_get_token();
			} else if (mon_expression(&value)) {
				str = (uint8 *)realloc(str, (len + GRANULARITY) & ~(GRANULARITY - 1));
				assert(str != NULL);
				str[len] = value;
				len++;
			} else {
				if (str)
					free(str);
				return false;
			}

		} else if (mon_token == T_END) {
			return true;
		} else {
			mon_error("',' expected");
			if (str)
				free(str);
			return false;
		}
	}
}


/*
 *  Convert character to printable character
 */

static inline uint8 char2print(uint8 c)
{
	return (c >= 0x20 && c <= 0x7e) ? c : '.';
}


/*
 *  Show version
 *  ver
 */

void version(void)
{
	fprintf(monout, "cxmon V" VERSION "\n");
}


/*
 *  Redirect output
 *  o [file]
 */

void redir_output(void)
{
	// Close old file
	if (monout != monerr) {
		fclose(monout);
		monout = monerr;
		return;
	}

	// No argument given?
	if (mon_token == T_END)
		return;

	// Otherwise open file
	if (mon_token == T_STRING) {
		mon_get_token();
		if (mon_token != T_END) {
			mon_error("Too many arguments");
			return;
		}
		if (!(monout = fopen(mon_string, "w")))
			mon_error("Unable to open file");
	} else
		mon_error("'\"' around file name expected");
}


/*
 *  Compute and display expression
 *  ? expression
 */

void print_expr(void)
{
	uintptr val;

	if (!mon_expression(&val))
		return;
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	if (val > 0x7fffffff) {
		fprintf(monout, "Hex unsigned:  $%08x\n"
					  "Hex signed  : -$%08x\n"
					  "Dec unsigned:  %u\n"
					  "Dec signed  : %d\n", val, -val, val, val);
		fprintf(monout, "Char        : '%c%c%c%c'\n", char2print(val >> 24), char2print(val >> 16), char2print(val >> 8), char2print(val));
	} else {
		fprintf(monout, "Hex : $%08x\n"
					  "Dec : %d\n", val, val);
		fprintf(monout, "Char: '%c%c%c%c'\n", char2print(val >> 24), char2print(val >> 16), char2print(val >> 8), char2print(val));
	}
}


/*
 *  Execute shell command
 *  \ "command"
 */

void shell_command(void)
{
	if (mon_token != T_STRING) {
		mon_error("'\"' around command expected");
		return;
	}
	mon_get_token();
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}
	system(mon_string);
}


/*
 *  Memory dump
 *  m [start [end]]
 */

#define MEMDUMP_BPL 16  // Bytes per line

void memory_dump(void)
{
	uintptr adr, end_adr;
	uint8 mem[MEMDUMP_BPL + 1];

	mem[MEMDUMP_BPL] = 0;

	if (!range_args(&adr, &end_adr, 16 * MEMDUMP_BPL - 1))  // 16 lines unless end address specified
		return;

	while (adr <= end_adr && !mon_aborted()) {
		fprintf(monout, "%0*lx:", int(2 * sizeof(adr)), mon_use_real_mem ? adr: adr % mon_mem_size);
		for (int i=0; i<MEMDUMP_BPL; i++, adr++) {
			if (i % 4 == 0)
				fprintf(monout, " %08x", mon_read_word(adr));
			mem[i] = char2print(mon_read_byte(adr));
		}
		fprintf(monout, "  '%s'\n", mem);
	}

	mon_dot_address = adr;
}


/*
 *  ASCII dump
 *  i [start [end]]
 */

#define ASCIIDUMP_BPL 64  // Bytes per line

void ascii_dump(void)
{
	uintptr adr, end_adr;
	uint8 str[ASCIIDUMP_BPL + 1];

	str[ASCIIDUMP_BPL] = 0;

	if (!range_args(&adr, &end_adr, 16 * ASCIIDUMP_BPL - 1))  // 16 lines unless end address specified
		return;

	while (adr <= end_adr && !mon_aborted()) {
		fprintf(monout, "%0*lx:", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
		for (int i=0; i<ASCIIDUMP_BPL; i++, adr++)
			str[i] = char2print(mon_read_byte(adr));
		fprintf(monout, " '%s'\n", str);
	}

	mon_dot_address = adr;
}


/*
 *  Binary dump
 *  b [start [end]]
 */

void binary_dump(void)
{
	uintptr adr, end_adr;
	uint8 str[9];

	str[8] = 0;

	if (!range_args(&adr, &end_adr, 7))  // 8 lines unless end address specified
		return;

	while (adr <= end_adr && !mon_aborted()) {
		fprintf(monout, "%0*lx:", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
		uint8 b = mon_read_byte(adr);
		for (int m=0x80, i=0; i<8; m>>=1, i++)
			str[i] = (b & m) ? '*' : '.';
		fprintf(monout, " '%s'\n", str);
		adr++;
	}

	mon_dot_address = adr;
}


/*
 * Add Break Point
 */
void break_point_add(void)
{
	uintptr address;

	if (mon_token == T_END || !mon_expression(&address)) {
		mon_error("Expect break point in hexadecimal.");
		return;
	}

	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	mon_add_break_point(address);
}


bool validate_index(uintptr *index_ptr, const BREAK_POINT_SET& break_point_set)
{
	if (mon_token == T_END || !mon_expression(index_ptr)) {
		mon_error("Expect index number of break point in hexadecimal.\n");
		return false;
	}

	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return false;
	}

	if (*index_ptr > break_point_set.size()) {
		mon_error("Illegal index number!");
		return false;
	}

	return true;
}


/*
 * Remove Break Point
 */
void break_point_remove(void)
{
	uintptr index;

	if (!validate_index(&index, active_break_points))
		return;

	if (0 == index) {
		active_break_points.clear();
		printf("Removed all break points!\n");
		return;
	}

	BREAK_POINT_SET::iterator it = active_break_points.begin();
	std::advance(it, index - 1);
	// Remove break point
	printf("Removed break point %4x at address %08lx\n", index, *it);
	active_break_points.erase(it);
}


/*
 * Disable Break Point
 */
void break_point_disable(void)
{
	uintptr index;

	if (!validate_index(&index, active_break_points))
		return;

	if (0 == index) {
		for (BREAK_POINT_SET::iterator it = active_break_points.begin(); it != active_break_points.end(); it++)
			disabled_break_points.insert(*it);
		active_break_points.clear();
		printf("Disabled all break points!\n");
		return;
	}

	BREAK_POINT_SET::iterator it = active_break_points.begin();
	std::advance(it, index - 1);
	// Add to disable break points
	printf("Disabled break point %4x at address %08lx\n", index, *it);
	disabled_break_points.insert(*it);
	// Remove break point
	active_break_points.erase(it);
}


/*
 * Enable Break Point
 */
void break_point_enable(void)
{
	uintptr index;

	if (!validate_index(&index, disabled_break_points))
		return;

	if (0 == index) {
		active_break_points.insert(disabled_break_points.begin(), disabled_break_points.end());
		disabled_break_points.clear();
		printf("Enabled all break points!\n");
		return;
	}

	BREAK_POINT_SET::iterator it = disabled_break_points.begin();
	std::advance(it, index - 1);
	// Add to active break points
	printf("Disabled break point %4x at address %08lx\n", index, *it);
	active_break_points.insert(*it);
	// Remove break point
	disabled_break_points.erase(it);
}


/*
 * List all Active Break Points
 */
void break_point_info(void)
{
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	BREAK_POINT_SET::iterator it;

	if (!active_break_points.empty()) {
		int pos = 1;
		printf(STR_ACTIVE_BREAK_POINTS);
		for (it = active_break_points.begin(); it != active_break_points.end(); it++)
			printf("\tBreak point %4x at address %08lx\n", pos++, *it);
	}

	if (!disabled_break_points.empty()) {
		putchar('\n');
		printf(STR_DISABLED_BREAK_POINTS);
		int pos = 1;
		for (it = disabled_break_points.begin(); it != disabled_break_points.end(); it++)
			printf("\tBreak point %4x at address %08lx\n", pos++, *it);
	}
}


/*
 * Save all Active Break Points to a file
 */
void break_point_save(void)
{
	if (mon_token == T_END) {
		mon_error("Missing file name");
		return;
	}
	if (mon_token != T_STRING) {
		mon_error("'\"' around file name expected");
		return;
	}
	mon_get_token();
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	FILE *file;
	if (!(file = fopen(mon_string, "w"))) {
		mon_error("Unable to create file");
		return;
	}

	BREAK_POINT_SET::iterator it;

	fprintf(file, STR_ACTIVE_BREAK_POINTS);
	for (it = active_break_points.begin(); it != active_break_points.end(); it++)
		fprintf(file, "%x\n", *it);

	fprintf(file, STR_DISABLED_BREAK_POINTS);
	for (it = disabled_break_points.begin(); it != disabled_break_points.end(); it++)
		fprintf(file, "%x\n", *it);

	fclose(file);
}


/*
 * Load Break Point from a file
 */
void break_point_load(void)
{
	if (mon_token == T_END) {
		mon_error("Missing file name");
		return;
	}
	if (mon_token != T_STRING) {
		mon_error("'\"' around file name expected");
		return;
	}
	mon_get_token();
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	// load from file
	mon_load_break_point(mon_string);
}


/*
 *  Disassemble
 *  d [start [end]]
 *  d65 [start [end]]
 *  d68 [start [end]]
 *  d80 [start [end]]
 *  d86 [start [end]]
 *  d8086 [start [end]]
 */

enum CPUType {
	CPU_PPC,
	CPU_6502,
	CPU_680x0,
	CPU_Z80,
	CPU_80x86_32,
	CPU_80x86_16,
	CPU_x86_64
};

static void disassemble(CPUType type)
{
	uintptr adr, end_adr;

	if (!range_args(&adr, &end_adr, 16 * 4 - 1))  // 16 lines unless end address specified
		return;

	switch (type) {
		case CPU_PPC:
			while (adr <= end_adr && !mon_aborted()) {
				uint32 w = mon_read_word(adr);
				fprintf(monout, "%0*lx: %08x\t", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size, w);
				disass_ppc(monout, mon_use_real_mem ? adr : adr % mon_mem_size, w);
				adr += 4;
			}
			break;

		case CPU_6502:
			while (adr <= end_adr && !mon_aborted()) {
				uint8 op = mon_read_byte(adr);
				uint8 lo = mon_read_byte(adr + 1);
				uint8 hi = mon_read_byte(adr + 2);
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_6502(monout, mon_use_real_mem ? adr : adr % mon_mem_size, op, lo, hi);
			}
			break;

		case CPU_680x0:
			while (adr <= end_adr && !mon_aborted()) {
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_68k(monout, mon_use_real_mem ? adr : adr % mon_mem_size);
			}
			break;

		case CPU_Z80:
			while (adr <= end_adr && !mon_aborted()) {
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_z80(monout, mon_use_real_mem ? adr : adr % mon_mem_size);
			}
			break;

		case CPU_x86_64:
			while (adr <= end_adr && !mon_aborted()) {
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_x86(monout, mon_use_real_mem ? adr : adr % mon_mem_size, 64);
			}
			break;

		case CPU_80x86_32:
			while (adr <= end_adr && !mon_aborted()) {
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_x86(monout, mon_use_real_mem ? adr : adr % mon_mem_size, 32);
			}
			break;

		case CPU_80x86_16:
			while (adr <= end_adr && !mon_aborted()) {
				fprintf(monout, "%0*lx: ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
				adr += disass_x86(monout, mon_use_real_mem ? adr : adr % mon_mem_size, 16);
			}
	}

	mon_dot_address = adr;
}

void disassemble_ppc(void)
{
	disassemble(CPU_PPC);
}

void disassemble_6502(void)
{
	disassemble(CPU_6502);
}

void disassemble_680x0(void)
{
	disassemble(CPU_680x0);
}

void disassemble_z80(void)
{
	disassemble(CPU_Z80);
}

void disassemble_80x86_32(void)
{
	disassemble(CPU_80x86_32);
}

void disassemble_80x86_16(void)
{
	disassemble(CPU_80x86_16);
}

void disassemble_x86_64(void)
{
	disassemble(CPU_x86_64);
}


/*
 *  Modify memory
 *  : addr bytestring
 */

void modify(void)
{
	uintptr adr, len, src_adr = 0;
	uint8 *str;

	if (!mon_expression(&adr))
		return;
	if (!byte_string(str, len))
		return;

	while (src_adr < len)
		mon_write_byte(adr++, str[src_adr++]);
	mon_dot_address = adr;

	free(str);
}


/*
 *  Fill
 *  f start end bytestring
 */

void fill(void)
{
	uintptr adr, end_adr, len, src_adr = 0;
	uint8 *str;

	if (!mon_expression(&adr))
		return;
	if (!mon_expression(&end_adr))
		return;
	if (!byte_string(str, len))
		return;

	while (adr <= end_adr)
		mon_write_byte(adr++, str[src_adr++ % len]);

	free(str);
}


/*
 *  Transfer memory
 *  t start end dest
 */

void transfer(void)
{
	uintptr adr, end_adr, dest;
	int num;

	if (!mon_expression(&adr))
		return;
	if (!mon_expression(&end_adr))
		return;
	if (!mon_expression(&dest))
		return;
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	num = end_adr - adr + 1;

	if (dest < adr)
		for (int i=0; i<num; i++)
			mon_write_byte(dest++, mon_read_byte(adr++));
	else {
		dest += end_adr - adr;
		for (int i=0; i<num; i++)
			mon_write_byte(dest--, mon_read_byte(end_adr--));
	}
}


/*
 *  Compare
 *  c start end dest
 */

void compare(void)
{
	uintptr adr, end_adr, dest;
	int num = 0;

	if (!mon_expression(&adr))
		return;
	if (!mon_expression(&end_adr))
		return;
	if (!mon_expression(&dest))
		return;
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	while (adr <= end_adr && !mon_aborted()) {
		if (mon_read_byte(adr) != mon_read_byte(dest)) {
			fprintf(monout, "%0*lx ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
			num++;
			if (!(num & 7))
				fputc('\n', monout);
		}
		adr++; dest++;
	}

	if (num & 7)
		fputc('\n', monout);
	fprintf(monout, "%d byte(s) different\n", num);
}


/*
 *  Search for byte string
 *  h start end bytestring
 */

void hunt(void)
{
	uintptr adr, end_adr, len;
	uint8 *str;
	int num = 0;

	if (!mon_expression(&adr))
		return;
	if (!mon_expression(&end_adr))
		return;
	if (!byte_string(str, len))
		return;

	while ((adr+len-1) <= end_adr && !mon_aborted()) {
		uint32 i;

		for (i=0; i<len; i++)
			if (mon_read_byte(adr + i) != str[i])
				break;

		if (i == len) {
			fprintf(monout, "%0*lx ", int(2 * sizeof(adr)), mon_use_real_mem ? adr : adr % mon_mem_size);
			num++;
			if (num == 1)
				mon_dot_address = adr;
			if (!(num & 7))
				fputc('\n', monout);
		}
		adr++;
	}

	free(str);

	if (num & 7)
		fputc('\n', monout);
	fprintf(monout, "Found %d occurrences\n", num);
}


/*
 *  Load data
 *  [ start "file"
 */

void load_data(void)
{
	uintptr start_adr;
	FILE *file;
	int fc;

	if (!mon_expression(&start_adr))
		return;
	if (mon_token == T_END) {
		mon_error("Missing file name");
		return;
	}
	if (mon_token != T_STRING) {
		mon_error("'\"' around file name expected");
		return;
	}
	mon_get_token();
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	if (!(file = fopen(mon_string, "rb")))
		mon_error("Unable to open file");
	else {
		uintptr adr = start_adr;

		while ((fc = fgetc(file)) != EOF)
			mon_write_byte(adr++, fc);
		fclose(file);

		fprintf(monerr, "%08x bytes read from %0*lx to %0*lx\n", adr - start_adr, int(2 * sizeof(adr)), mon_use_real_mem ? start_adr : start_adr % mon_mem_size, int(2 * sizeof(adr)), mon_use_real_mem ? adr-1 : (adr-1) % mon_mem_size);
		mon_dot_address = adr;
	}
}


/*
 *  Save data
 *  ] start size "file"
 */

void save_data(void)
{
	uintptr start_adr, size;
	FILE *file;

	if (!mon_expression(&start_adr))
		return;
	if (!mon_expression(&size))
		return;
	if (mon_token == T_END) {
		mon_error("Missing file name");
		return;
	}
	if (mon_token != T_STRING) {
		mon_error("'\"' around file name expected");
		return;
	}
	mon_get_token();
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	if (!(file = fopen(mon_string, "wb")))
		mon_error("Unable to create file");
	else {
		uintptr adr = start_adr, end_adr = start_adr + size - 1;

		while (adr <= end_adr)
			fputc(mon_read_byte(adr++), file);
		fclose(file);

		fprintf(monerr, "%08x bytes written from %0*lx to %0*lx\n", size, int(2 * sizeof(adr)), mon_use_real_mem ? start_adr : start_adr % mon_mem_size, int(2 * sizeof(adr)), mon_use_real_mem ? end_adr : end_adr % mon_mem_size);
	}
}
