/*
 *  mon_cmd.h - cxmon standard commands
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

#ifndef MON_CMD_H
#define MON_CMD_H

extern void version(void);
extern void redir_output(void);
extern void print_expr(void);
extern void shell_command(void);
extern void memory_dump(void);
extern void ascii_dump(void);
extern void binary_dump(void);
extern void break_point_add(void);
extern void break_point_remove(void);
extern void break_point_disable(void);
extern void break_point_enable(void);
extern void break_point_info(void);
extern void break_point_save(void);
extern void break_point_load(void);
extern void disassemble_ppc(void);
extern void disassemble_6502(void);
extern void disassemble_680x0(void);
extern void disassemble_z80(void);
extern void disassemble_80x86_32(void);
extern void disassemble_80x86_16(void);
extern void disassemble_x86_64(void);
extern void modify(void);
extern void fill(void);
extern void transfer(void);
extern void compare(void);
extern void hunt(void);
extern void load_data(void);
extern void save_data(void);

#endif
