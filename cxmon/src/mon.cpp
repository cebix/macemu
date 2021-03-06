/*
 *  mon.cpp - cxmon main program
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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <string>
#include <map>
#include <sstream>

#if defined(HAVE_READLINE_H)
extern "C" {
#include <readline.h>
}
#elif defined(HAVE_READLINE_READLINE_H)
extern "C" {
#include <readline/readline.h>
}
#endif

#if defined(HAVE_HISTORY_H)
extern "C" {
#include <history.h>
}
#elif defined(HAVE_READLINE_HISTORY_H)
extern "C" {
#include <readline/history.h>
}
#endif

#include "mon.h"
#include "mon_cmd.h"
#include "mon_lowmem.h"

#ifndef VERSION
#define VERSION "3"
#endif

// Break points
BREAK_POINT_SET active_break_points;
BREAK_POINT_SET disabled_break_points;

// Buffer we're operating on
bool mon_use_real_mem = false;
uint32 mon_mem_size;
static uint8 *mem;


// Streams for input, output and error messages
FILE *monin, *monout, *monerr = NULL;

// Input line
static char *input;
static char *in_ptr;
char *mon_args_ptr;

// Current address, value of '.' in expressions
uintptr mon_dot_address;

// Current value of ':' in expression
static uint32 colon_value;


// Scanner variables
enum Token mon_token;  // Last token read
uintptr mon_number;    // Contains the number if mon_token==T_NUMBER
char *mon_string;      // Contains the string if mon_token==T_STRING
char *mon_name;        // Contains the variable name if mon_token==T_NAME


// List of installed commands
struct CmdSpec {
	const char *name;  // Name of command
	void (*func)();    // Function that executes this command
};

static CmdSpec *cmds;   // Array of CmdSpecs
static int num_cmds;    // Number of installed commands
static char *cmd_help;  // Help text for commands


// List of variables
typedef std::map<std::string, uintptr> var_map;
static var_map vars;


// Prototypes
static void init_abort();
static void exit_abort();

static void read_line(char *prompt);		// Scanner
static char get_char();
static void put_back(char c);
static enum Token get_hex_number(uintptr &i);
static enum Token get_dec_number(uintptr &i);
static enum Token get_char_number(uintptr &i);
static enum Token get_string(char *&str);
static enum Token get_hex_or_name(uintptr &i, char *&name);

static bool eor_expr(uintptr *number);	// Parser
static bool and_expr(uintptr *number);
static bool shift_expr(uintptr *number);
static bool add_expr(uintptr *number);
static bool mul_expr(uintptr *number);
static bool factor(uintptr *number);


/*
 *  Add command to mon
 */

void mon_add_command(const char *name, void (*func)(), const char *help_text)
{
	num_cmds++;
	if (cmds)
		cmds = (CmdSpec *)realloc(cmds, num_cmds * sizeof(CmdSpec));
	else
		cmds = (CmdSpec *)malloc(sizeof(CmdSpec));
	cmds[num_cmds - 1].name = name;
	cmds[num_cmds - 1].func = func;
	if (help_text) {
		if (cmd_help) {
			cmd_help = (char *)realloc(cmd_help, strlen(cmd_help) + strlen(help_text) + 1);
			strcat(cmd_help, help_text);
		} else
			cmd_help = strdup(help_text);
	}
}


/*
 *  Print error message
 */

void mon_error(const char *s)
{
	fprintf(monerr == NULL? stdout : monerr, "*** %s\n", s);
}


/*
 *  CTRL-C pressed?
 */

static bool was_aborted;
static struct sigaction my_sa;

#ifdef __BEOS__
static void handle_abort(int sig, void *arg, vregs *r)
#else
static void handle_abort(int sig)
#endif
{
	was_aborted = true;
}

static void init_abort()
{
	was_aborted = false;
	sigemptyset(&my_sa.sa_mask);
#ifdef __BEOS__
	my_sa.sa_handler = (__signal_func_ptr)handle_abort;
	my_sa.sa_userdata = 0;
#else
	my_sa.sa_handler = handle_abort;
#endif
	my_sa.sa_flags = 0;
	sigaction(SIGINT, &my_sa, NULL);
}

static void exit_abort()
{
	my_sa.sa_handler = SIG_DFL;
	sigaction(SIGINT, &my_sa, NULL);
}

bool mon_aborted()
{
	bool ret = was_aborted;
	was_aborted = false;
	return ret;
}


/*
 *  Access to buffer
 */

uint32 (*mon_read_byte)(uintptr adr);

uint32 mon_read_byte_buffer(uintptr adr)
{
	return mem[adr % mon_mem_size];
}

uint32 mon_read_byte_real(uintptr adr)
{
	return *(uint8 *)adr;
}

void (*mon_write_byte)(uintptr adr, uint32 b);

void mon_write_byte_buffer(uintptr adr, uint32 b)
{
	mem[adr % mon_mem_size] = b;
}

void mon_write_byte_real(uintptr adr, uint32 b)
{
	*(uint8 *)adr = b;
}

uint32 mon_read_half(uintptr adr)
{
	return (mon_read_byte(adr) << 8) | mon_read_byte(adr+1);
}

void mon_write_half(uintptr adr, uint32 w)
{
	mon_write_byte(adr, w >> 8);
	mon_write_byte(adr+1, w);
}

uint32 mon_read_word(uintptr adr)
{
	return (mon_read_byte(adr) << 24) | (mon_read_byte(adr+1) << 16) | (mon_read_byte(adr+2) << 8) | mon_read_byte(adr+3);
}

void mon_write_word(uintptr adr, uint32 l)
{
	mon_write_byte(adr, l >> 24);
	mon_write_byte(adr+1, l >> 16);
	mon_write_byte(adr+2, l >> 8);
	mon_write_byte(adr+3, l);
}


/*
 *  Read a line from the keyboard
 */

static void read_line(char *prompt)
{
#ifdef HAVE_LIBREADLINE
	if (input)
		free(input);
	input = readline(prompt);

	if (input) {
		if (*input)
			add_history(input);
	} else {
		// EOF, quit cxmon
		input = (char *)malloc(2);
		input[0] = 'x';
		input[1] = 0;
		fprintf(monout, "x\n");
	}

	in_ptr = input;
#else
	static const unsigned INPUT_LENGTH = 256;
	if (!input)
		input = (char *)malloc(INPUT_LENGTH);
	fputs(prompt, monout);
	fflush(monout);
	fgets(in_ptr = input, INPUT_LENGTH, monin);
	char *s = strchr(input, '\n');
	if (s != NULL)
		*s = 0;
#endif
}


/*
 *  Read a character from the input line
 */

static char get_char()
{
	return *in_ptr++;
}


/*
 *  Stuff back a character into the input line
 */

static void put_back(char c)
{
	*(--in_ptr) = c;
}


/*
 *  Scanner: Get a token from the input line
 */

enum Token mon_get_token()
{
	char c = get_char();

	// Skip spaces
	while (isspace(c))
		c = get_char();

	switch (c) {
		case 0:
			return mon_token = T_END;
		case '(':
			return mon_token = T_LPAREN;
		case ')':
			return mon_token = T_RPAREN;
		case '.':
			return mon_token = T_DOT;
		case ':':
			return mon_token = T_COLON;
		case ',':
			return mon_token = T_COMMA;
		case '+':
			return mon_token = T_PLUS;
		case '-':
			return mon_token = T_MINUS;
		case '*':
			return mon_token = T_MUL;
		case '/':
			return mon_token = T_DIV;
		case '%':
			return mon_token = T_MOD;
		case '&':
			return mon_token = T_AND;
		case '|':
			return mon_token = T_OR;
		case '^':
			return mon_token = T_EOR;
		case '<':
			if (get_char() == '<')
				return mon_token = T_SHIFTL;
			else {
				mon_error("Unrecognized token");
				return mon_token = T_NULL;
			}
		case '>':
			if (get_char() == '>')
				return mon_token = T_SHIFTR;
			else {
				mon_error("Unrecognized token");
				return mon_token = T_NULL;
			}
		case '~':
			return mon_token = T_NOT;
		case '=':
			return mon_token = T_ASSIGN;

		case '$':
			if ((mon_token = get_hex_number(mon_number)) == T_NULL)
				mon_error("'$' must be followed by hexadecimal number");
			return mon_token;
		case '_':
			if ((mon_token = get_dec_number(mon_number)) == T_NULL)
				mon_error("'_' must be followed by decimal number");
			return mon_token;
		case '\'':
			return mon_token = get_char_number(mon_number);
		case '"':
			return mon_token = get_string(mon_string);

		default:
			if (isalnum(c)) {
				put_back(c);
				return mon_token = get_hex_or_name(mon_number, mon_name);
			}
			mon_error("Unrecognized token");
			return mon_token = T_NULL;
	}
}

static enum Token get_hex_number(uintptr &i)
{
	char c = get_char();

	i = 0;
	if (!isxdigit(c))
		return T_NULL;

	do {
		c = tolower(c);
		if (c < 'a')
			i = (i << 4) + (c - '0');
		else
			i = (i << 4) + (c - 'a' + 10);
		c = get_char();
	} while (isxdigit(c));

	if (isalnum(c))
		return T_NULL;
	else {
		put_back(c);
		return T_NUMBER;
	}
}

static enum Token get_dec_number(uintptr &i)
{
	char c = get_char();

	i = 0;
	if (!isdigit(c))
		return T_NULL;

	do {
		i = (i * 10) + (c - '0');
		c = get_char();
	} while (isdigit(c));

	if (isalnum(c))
		return T_NULL;
	else {
		put_back(c);
		return T_NUMBER;
	}
}

static enum Token get_char_number(uintptr &i)
{
	char c;

	i = 0;
	while ((c = get_char()) != 0) {
		if (c == '\'')
			return T_NUMBER;
		i = (i << 8) + (uint8)c;
	}

	mon_error("Unterminated character constant");
	return T_NULL;
}

static enum Token get_string(char *&str)
{
	// Remember start of string
	char *old_in_ptr = in_ptr;

	// Determine string length
	char c;
	unsigned n = 0;
	while ((c = get_char()) != 0) {
		n++;
		if (c == '"')
			break;
	}
	if (c == 0) {
		mon_error("Unterminated string");
		return T_NULL;
	}

	// Allocate new buffer (n: size needed including terminating 0)
	str = (char *)realloc(str, n);

	// Copy string to buffer
	char *p = str;
	in_ptr = old_in_ptr;
	while (--n)
		*p++ = get_char();
	*p++ = 0;
	get_char();  // skip closing '"'
	return T_STRING;
}

static enum Token get_hex_or_name(uintptr &i, char *&name)
{
	// Remember start of token
	char *old_in_ptr = in_ptr;

	// Try hex number first
	if (get_hex_number(i) == T_NUMBER)
		return T_NUMBER;

	// Not a hex number, must be a variable name; determine its length
	in_ptr = old_in_ptr;
	char c = get_char();
	unsigned n = 1;
	do {
		n++;
		c = get_char();
	} while (isalnum(c));

	// Allocate new buffer (n: size needed including terminating 0)
	name = (char *)realloc(name, n);

	// Copy name to buffer
	in_ptr = old_in_ptr;
	char *p = name;
	while (--n)
		*p++ = get_char();
	*p = 0;
	return T_NAME;
}


/*
 *  expression = eor_expr {OR eor_expr}
 *  true: OK, false: Error
 */

bool mon_expression(uintptr *number)
{
	uintptr accu, expr;

	if (!eor_expr(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_OR:
				mon_get_token();
				if (!eor_expr(&expr))
					return false;
				accu |= expr;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  eor_expr = and_expr {EOR and_expr}
 *  true: OK, false: Error
 */

static bool eor_expr(uintptr *number)
{
	uintptr accu, expr;

	if (!and_expr(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_EOR:
				mon_get_token();
				if (!and_expr(&expr))
					return false;
				accu ^= expr;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  and_expr = shift_expr {AND shift_expr}
 *  true: OK, false: Error
 */

static bool and_expr(uintptr *number)
{
	uintptr accu, expr;

	if (!shift_expr(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_AND:
				mon_get_token();
				if (!shift_expr(&expr))
					return false;
				accu &= expr;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  shift_expr = add_expr {(SHIFTL | SHIFTR) add_expr}
 *  true: OK, false: Error
 */

static bool shift_expr(uintptr *number)
{
	uintptr accu, expr;

	if (!add_expr(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_SHIFTL:
				mon_get_token();
				if (!add_expr(&expr))
					return false;
				accu <<= expr;
				break;

			case T_SHIFTR:
				mon_get_token();
				if (!add_expr(&expr))
					return false;
				accu >>= expr;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  add_expr = mul_expr {(PLUS | MINUS) mul_expr}
 *  true: OK, false: Error
 */

static bool add_expr(uintptr *number)
{
	uintptr accu, expr;

	if (!mul_expr(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_PLUS:
				mon_get_token();
				if (!mul_expr(&expr))
					return false;
				accu += expr;
				break;

			case T_MINUS:
				mon_get_token();
				if (!mul_expr(&expr))
					return false;
				accu -= expr;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  mul_expr = factor {(MUL | DIV | MOD) factor}
 *  true: OK, false: Error
 */

static bool mul_expr(uintptr *number)
{
	uintptr accu, fact;

	if (!factor(&accu))
		return false;

	for (;;)
		switch (mon_token) {
			case T_MUL:
				mon_get_token();
				if (!factor(&fact))
					return false;
				accu *= fact;
				break;

			case T_DIV:
				mon_get_token();
				if (!factor(&fact))
					return false;
				if (fact == 0) {
					mon_error("Division by 0");
					return false;
				}
				accu /= fact;
				break;

			case T_MOD:
				mon_get_token();
				if (!factor(&fact))
					return false;
				if (fact == 0) {
					mon_error("Division by 0");
					return false;
				}
				accu %= fact;
				break;

			default:
				*number = accu;
				return true;
		}
}


/*
 *  factor = NUMBER | NAME | DOT | COLON | (PLUS | MINUS | NOT) factor | LPAREN expression RPAREN
 *  true: OK, false: Error
 */

static bool factor(uintptr *number)
{
	switch (mon_token) {
		case T_NUMBER:
			*number = mon_number;
			mon_get_token();
			return true;

		case T_NAME:{
			var_map::const_iterator v = vars.find(mon_name);
			if (v == vars.end())
				return false;
			else {
				*number = v->second;
				mon_get_token();
				return true;
			}
		}

		case T_DOT:
			*number = mon_dot_address;
			mon_get_token();
			return true;

		case T_COLON:
			*number = colon_value;
			mon_get_token();
			return true;

		case T_PLUS:
			mon_get_token();
			return factor(number);

		case T_MINUS:
			mon_get_token();
			if (factor(number)) {
				*number = -*number;
				return true;
			} else
				return false;

		case T_NOT:
			mon_get_token();
			if (factor(number)) {
				*number = ~*number;
				return true;
			} else
				return false;

		case T_LPAREN:
			mon_get_token();
			if (mon_expression(number))
				if (mon_token == T_RPAREN) {
					mon_get_token();
					return true;
				} else {
					mon_error("Missing ')'");
					return false;
				}
			else {
				mon_error("Error in expression");
				return false;
			}

		case T_END:
			mon_error("Required argument missing");
			return false;

		default:
			mon_error("'(' or number expected");
			return false;
	}
}


/*
 *  Set/clear/show variables
 *  set [var[=value]]
 */

static void set_var()
{
	if (mon_token == T_END) {

		// Show all variables
		if (vars.empty())
			fprintf(monout, "No variables defined\n");
		else {
			var_map::const_iterator v = vars.begin(), end = vars.end();
			for (v=vars.begin(); v!=end; ++v)
				fprintf(monout, "%s = %08lx\n", v->first.c_str(), v->second);
		}

	} else if (mon_token == T_NAME) {
		std::string var_name = mon_name;
		mon_get_token();
		if (mon_token == T_ASSIGN) {

			// Set variable
			uintptr value;
			mon_get_token();
			if (!mon_expression(&value))
				return;
			if (mon_token != T_END) {
				mon_error("Too many arguments");
				return;
			}
			vars[var_name] = value;

		} else if (mon_token == T_END) {

			// Clear variable
			vars.erase(var_name);

		} else
			mon_error("'=' expected");
	} else
		mon_error("Variable name expected");
}


/*
 *  Clear all variables
 *  cv
 */

static void clear_vars()
{
	vars.clear();
}


/*
 *  Display help
 *  h
 */

static void help_or_hunt()
{
	if (mon_token != T_END) {
		hunt();
		return;
	}
	fprintf(monout, "x                        Quit mon\n"
					"h                        This help text\n");
	fprintf(monout, "%s", cmd_help);
}


/*
 *  Display command list
 *  ??
 */

static void mon_cmd_list()
{
	for (int i=0; i<num_cmds; i++)
		fprintf(monout, "%s ", cmds[i].name);
	fprintf(monout, "\n");
}


/*
 *  Reallocate buffer
 *  @ [size]
 */

static void reallocate()
{
	uintptr size;

	if (mon_use_real_mem) {
		fprintf(monerr, "Cannot reallocate buffer in real mode\n");
		return;
	}

	if (mon_token == T_END) {
		fprintf(monerr, "Buffer size: %08x bytes\n", mon_mem_size);
		return;
	}

	if (!mon_expression(&size))
		return;
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	if ((mem = (uint8 *)realloc(mem, size)) != NULL)
		fprintf(monerr, "Buffer size: %08x bytes\n", mon_mem_size = size);
	else
		fprintf(monerr, "Unable to reallocate buffer\n");
}


/*
 *  Apply expression to memory
 *  y[b|h|w] start end expression
 */

static void apply(int size)
{
	uintptr adr, end_adr, value;
	char c;

	if (!mon_expression(&adr))
		return;
	if (!mon_expression(&end_adr))
		return;
	if (!mon_expression(&value))
		return;
	if (mon_token != T_END) {
		mon_error("Too many arguments");
		return;
	}

	uint32 (*read_func)(uintptr adr);
	void (*write_func)(uintptr adr, uint32 val);
	switch (size) {
		case 1:
			read_func = mon_read_byte;
			write_func = mon_write_byte;
			break;
		case 2:
			read_func = mon_read_half;
			write_func = mon_write_half;
			break;
		case 4:
			read_func = mon_read_word;
			write_func = mon_write_word;
			break;
		default:
			abort();
			break;
	}

	while (adr<=end_adr) {
		colon_value = read_func(adr);
		mon_dot_address = adr;

		in_ptr = input;
		while ((c = get_char()) == ' ') ;
		while ((c = get_char()) != ' ') ;
		while ((c = get_char()) == ' ') ;
		put_back(c);
		mon_get_token();
		mon_expression(&value);	// Skip start address
		mon_expression(&value);	// Skip end address
		mon_expression(&value);

		write_func(adr, value);
		adr += size;
	}

	mon_dot_address = adr;
}

static void apply_byte()
{
	apply(1);
}

static void apply_half()
{
	apply(2);
}

static void apply_word()
{
	apply(4);
}


/*
 *  Execute command via system() (for ls, rm, etc.)
 */

static void mon_exec()
{
	system(input);
}


/*
 *  Change current directory
 */

void mon_change_dir()
{
	in_ptr = input;
	char c = get_char();
	while (isspace(c))
		c = get_char();
	while (isgraph(c))
		c = get_char();
	while (isspace(c))
		c = get_char();
	put_back(c);
	if (chdir(in_ptr) != 0)
		mon_error("Cannot change directory");
}


/*
 * Add break point
 */

void mon_add_break_point(uintptr addr)
{
	BREAK_POINT_SET::iterator it = disabled_break_points.find(addr);
	// Save break point
	if (it == disabled_break_points.end()) {
		active_break_points.insert(addr);
	} else {
		disabled_break_points.erase(it);
		active_break_points.insert(addr);
	}
}


/*
 * Load break point from file
 */
void mon_load_break_point(const char* file_path)
{
	FILE *file;
	if (!(file = fopen(file_path, "r"))) {
		mon_error("Unable to create file");
		return;
	}

	char line_buff[1024];
	bool is_disabled_break_points = false;

	if (fgets(line_buff, sizeof(line_buff), file) == NULL ||
			strcmp(line_buff, STR_ACTIVE_BREAK_POINTS) != 0) {
		mon_error("Invalid break point file format!");
		fclose(file);
		return;
	}

	while (fgets(line_buff, sizeof(line_buff), file) != NULL) {
		if (strcmp(line_buff, STR_DISABLED_BREAK_POINTS) == 0) {
			is_disabled_break_points = true;
			continue;
		}
		uintptr address;
		std::stringstream ss;
		ss << std::hex << line_buff;
		ss >> address;
		if (is_disabled_break_points)
			disabled_break_points.insert(address);
		else
			active_break_points.insert(address);
	}

	fclose(file);
}


/*
 *  Initialize mon
 */

void mon_init()
{
	cmds = NULL;
	num_cmds = 0;
	cmd_help = NULL;

	mon_add_command("??", mon_cmd_list,				"??                       Show list of commands\n");
	mon_add_command("ver", version,					"ver                      Show version\n");
	mon_add_command("?", print_expr,				"? expression             Calculate expression\n");
	mon_add_command("@", reallocate,				"@ [size]                 Reallocate buffer\n");
	mon_add_command("i", ascii_dump,				"i [start [end]]          ASCII memory dump\n");
	mon_add_command("m", memory_dump,				"m [start [end]]          Hex/ASCII memory dump\n");
	mon_add_command("b", binary_dump,				"b [start [end]]          Binary memory dump\n");
	mon_add_command("ba", break_point_add,				"ba [address]             Add a break point\n");
	mon_add_command("br", break_point_remove,				"br [breakpoints#]        Remove a break point. If # is 0, remove all break points.\n");
	mon_add_command("bd", break_point_disable,				"bd [breakpoints#]        Disable a break point. If # is 0, disable all break points.\n");
	mon_add_command("be", break_point_enable,				"be [breakpoints#]        Enable a break point. If # is 0, enable all break points.\n");
	mon_add_command("bi", break_point_info,				"bi                       List all break points\n");
	mon_add_command("bs", break_point_save,				"bs \"file\"                Save all break points to a file\n");
	mon_add_command("bl", break_point_load,				"bl \"file\"                Load break points from a file\n");
	mon_add_command("d", disassemble_ppc,			"d [start [end]]          Disassemble PowerPC code\n");
	mon_add_command("d65", disassemble_6502,		"d65 [start [end]]        Disassemble 6502 code\n");
	mon_add_command("d68", disassemble_680x0,		"d68 [start [end]]        Disassemble 680x0 code\n");
	mon_add_command("d80", disassemble_z80,			"d80 [start [end]]        Disassemble Z80 code\n");
	mon_add_command("d86", disassemble_80x86_32,	"d86 [start [end]]        Disassemble 80x86 (32-bit) code\n");
	mon_add_command("d8086", disassemble_80x86_16,	"d8086 [start [end]]      Disassemble 80x86 (16-bit) code\n");
	mon_add_command("d8664", disassemble_x86_64,	"d8664 [start [end]]      Disassemble x86-64 code\n");
	mon_add_command(":", modify,					": start string           Modify memory\n");
	mon_add_command("f", fill,						"f start end string       Fill memory\n");
	mon_add_command("y", apply_byte,				"y[b|h|w] start end expr  Apply expression to memory\n");
	mon_add_command("yb", apply_byte, NULL);
	mon_add_command("yh", apply_half, NULL);
	mon_add_command("yw", apply_word, NULL);
	mon_add_command("t", transfer,					"t start end dest         Transfer memory\n");
	mon_add_command("c", compare,					"c start end dest         Compare memory\n");
	mon_add_command("h", help_or_hunt,				"h start end string       Search for byte string\n");
	mon_add_command("\\", shell_command,			"\\ \"command\"              Execute shell command\n");
	mon_add_command("ls", mon_exec,					"ls [args]                List directory contents\n");
	mon_add_command("rm", mon_exec,					"rm [args]                Remove file(s)\n");
	mon_add_command("cp", mon_exec,					"cp [args]                Copy file(s)\n");
	mon_add_command("mv", mon_exec,					"mv [args]                Move file(s)\n");
	mon_add_command("cd", mon_change_dir,			"cd directory             Change current directory\n");
	mon_add_command("o", redir_output,				"o [\"file\"]               Redirect output\n");
	mon_add_command("[", load_data,					"[ start \"file\"           Load data from file\n");
	mon_add_command("]", save_data,					"] start size \"file\"      Save data to file\n");
	mon_add_command("set", set_var,					"set [var[=value]]        Set/clear/show variables\n");
	mon_add_command("cv", clear_vars,				"cv                       Clear all variables\n");

	mon_read_byte = NULL;
	mon_write_byte = NULL;

	input = NULL;
	mon_string = NULL;
	mon_name = NULL;
}


/*
 *  Deinitialize mon
 */

void mon_exit()
{
	if (cmds) {
		free(cmds);
		cmds = NULL;
	}
	num_cmds = 0;
	cmd_help = NULL;

	if (input) {
		free(input);
		input = NULL;
	}
	if (mon_string) {
		free(mon_string);
		mon_string = NULL;
	}
	if (mon_name) {
		free(mon_name);
		mon_name = NULL;
	}
}


/*
 *  Main function, read-execute loop
 */

void mon(int argc, const char **argv)
{
	bool done = false, interactive = true;

	// Setup input/output streams
	monin = stdin;
	monout = stdout;
	monerr = stdout;

	// Make argc/argv point to the actual arguments
	const char *prg_name = argv[0];
	if (argc)
		argc--; argv++;

	// Parse arguments
	mon_macos_mode = false;
	mon_use_real_mem = false;
	while (argc > 0) {
		if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
			printf("Usage: %s [-m] [-r] [command...]\n", prg_name);
			exit(0);
		} else if (strcmp(argv[0], "-m") == 0)
			mon_macos_mode = true;
		else if (strcmp(argv[0], "-r") == 0)
			mon_use_real_mem = true;
		else
			break;
		argc--; argv++;
	}
	interactive = (argc == 0);

	// Set up memory access functions if not supplied by the user
	if (mon_read_byte == NULL) {
		if (mon_use_real_mem)
			mon_read_byte = mon_read_byte_real;
		else
			mon_read_byte = mon_read_byte_buffer;
	}
	if (mon_write_byte == NULL) {
		if (mon_use_real_mem)
			mon_write_byte = mon_write_byte_real;
		else
			mon_write_byte = mon_write_byte_buffer;
	}

	// Allocate buffer
	if (!mon_use_real_mem) {
		mon_mem_size = 0x100000;
		mem = (uint8 *)malloc(mon_mem_size);

		// Print banner
		if (interactive)
			fprintf(monerr, "\n *** cxmon V" VERSION " by Christian Bauer and Marc Hellwig ***\n"
							" ***               Press 'h' for help               ***\n\n");
	}

	// Clear variables
	vars.clear();

	// In MacOS mode, pull in the lowmem globals as variables
	if (mon_macos_mode) {
		const lowmem_info *l = lowmem;
		while (l->name) {
			vars[l->name] = l->addr;
			l++;
		}
	}

	init_abort();

	// Read and parse command line
	char *cmd = NULL;
	while (!done) {
		if (interactive) {
			char prompt[16];
			sprintf(prompt, "[%0*lx]-> ", int(2 * sizeof(mon_dot_address)), mon_dot_address);
			read_line(prompt);
			if (!input) {
				done = true;
				continue;
			}
		} else {
			if (argc == 0) {
				done = true;
				break;
			} else {
				unsigned n = strlen(argv[0]) + 1;
				input = (char *)realloc(input, n);
				strcpy(in_ptr = input, argv[0]);
				argc--;
				argv++;
			}
		}

		// Skip leading spaces
		char c = get_char();
		while (isspace(c))
			c = get_char();
		put_back(c);
		if (!c)
			continue;  // blank line

		// Read command word
		char *p = in_ptr;
		while (isgraph(c))
			c = get_char();
		put_back(c);
		unsigned n = in_ptr - p;
		cmd = (char *)realloc(cmd, n + 1);
		memcpy(cmd, p, n);
		cmd[n] = 0;

		// Execute command
		if (strcmp(cmd, "x") == 0) {	// Exit
			done = true;
			continue;
		}
		for (int i=0; i<num_cmds; i++) {
			if (strcmp(cmd, cmds[i].name) == 0) {
				mon_get_token();
				cmds[i].func();
				goto cmd_done;
			}
		}
		mon_error("Unknown command");
cmd_done: ;
	}

	if (cmd)
		free(cmd);

	exit_abort();

	// Free buffer
	if (!mon_use_real_mem)
		free(mem);

	// Close output file if redirected
	if (monout != monerr)
		fclose(monout);
}
