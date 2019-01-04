/* Print Motorola 68k instructions.
   Copyright 1986, 87, 89, 91, 92, 93, 94, 95, 96, 97, 1998
   Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdlib.h>
#include "dis-asm.h"
#include "floatformat.h"
#include "opintl.h"

#include "m68k.h"

/* Local function prototypes */

static int
fetch_data PARAMS ((struct disassemble_info *, bfd_byte *));

static void
dummy_print_address PARAMS ((bfd_vma, struct disassemble_info *));

static int
fetch_arg PARAMS ((unsigned char *, int, int, disassemble_info *));

static void
print_base PARAMS ((int, bfd_vma, disassemble_info*));

static unsigned char *
print_indexed PARAMS ((int, unsigned char *, bfd_vma, disassemble_info *));

static int
print_insn_arg PARAMS ((const char *, unsigned char *, unsigned char *,
			bfd_vma, disassemble_info *));

CONST char * CONST fpcr_names[] = {
  "", "fpiar", "fpsr", "fpiar/fpsr", "fpcr",
  "fpiar/fpcr", "fpsr/fpcr", "fpiar/fpsr/fpcr"};

static char *const reg_names[] = {
  "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
  "sr", "pc"};

/* Sign-extend an (unsigned char). */
#if __STDC__ == 1
#define COERCE_SIGNED_CHAR(ch) ((signed char)(ch))
#else
#define COERCE_SIGNED_CHAR(ch) ((int)(((ch) ^ 0x80) & 0xFF) - 128)
#endif

/* Get a 1 byte signed integer.  */
#define NEXTBYTE(p)  (p += 2, FETCH_DATA (info, p), COERCE_SIGNED_CHAR(p[-1]))

/* Get a 2 byte signed integer.  */
#define COERCE16(x) ((int) (((x) ^ 0x8000) - 0x8000))
#define NEXTWORD(p)  \
  (p += 2, FETCH_DATA (info, p), \
   COERCE16 ((p[-2] << 8) + p[-1]))

/* Get a 4 byte signed integer.  */
#define COERCE32(x) ((bfd_signed_vma) ((x) ^ 0x80000000) - 0x80000000)
#define NEXTLONG(p)  \
  (p += 4, FETCH_DATA (info, p), \
   (COERCE32 ((((((p[-4] << 8) + p[-3]) << 8) + p[-2]) << 8) + p[-1])))

/* Get a 4 byte unsigned integer.  */
#define NEXTULONG(p)  \
  (p += 4, FETCH_DATA (info, p), \
   (unsigned int) ((((((p[-4] << 8) + p[-3]) << 8) + p[-2]) << 8) + p[-1]))

/* Get a single precision float.  */
#define NEXTSINGLE(val, p) \
  (p += 4, FETCH_DATA (info, p), \
   floatformat_to_double (&floatformat_ieee_single_big, (char *) p - 4, &val))

/* Get a double precision float.  */
#define NEXTDOUBLE(val, p) \
  (p += 8, FETCH_DATA (info, p), \
   floatformat_to_double (&floatformat_ieee_double_big, (char *) p - 8, &val))

/* Get an extended precision float.  */
#define NEXTEXTEND(val, p) \
  (p += 12, FETCH_DATA (info, p), \
   floatformat_to_double (&floatformat_m68881_ext, (char *) p - 12, &val))

/* Need a function to convert from packed to double
   precision.   Actually, it's easier to print a
   packed number than a double anyway, so maybe
   there should be a special case to handle this... */
#define NEXTPACKED(p) \
  (p += 12, FETCH_DATA (info, p), 0.0)


/* Maximum length of an instruction.  */
#define MAXLEN 22

#include <setjmp.h>

struct private
{
  /* Points to first byte not fetched.  */
  bfd_byte *max_fetched;
  bfd_byte the_buffer[MAXLEN];
  bfd_vma insn_start;
  jmp_buf bailout;
};

/* Make sure that bytes from INFO->PRIVATE_DATA->BUFFER (inclusive)
   to ADDR (exclusive) are valid.  Returns 1 for success, longjmps
   on error.  */
#define FETCH_DATA(info, addr) \
  ((addr) <= ((struct private *)(info->private_data))->max_fetched \
   ? 1 : fetch_data ((info), (addr)))

static int
fetch_data (info, addr)
     struct disassemble_info *info;
     bfd_byte *addr;
{
  int status;
  struct private *priv = (struct private *)info->private_data;
  bfd_vma start = priv->insn_start + (priv->max_fetched - priv->the_buffer);

  status = (*info->read_memory_func) (start,
				      priv->max_fetched,
				      addr - priv->max_fetched,
				      info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, start, info);
      longjmp (priv->bailout, 1);
    }
  else
    priv->max_fetched = addr;
  return 1;
}

/* This function is used to print to the bit-bucket. */
static int
#ifdef __STDC__
dummy_printer (FILE * file, const char * format, ...)
#else
dummy_printer (file) FILE *file;
#endif
 { return 0; }

static void
dummy_print_address (vma, info)
     bfd_vma vma;
     struct disassemble_info *info;
{
}

/* Print the m68k instruction at address MEMADDR in debugged memory,
   on INFO->STREAM.  Returns length of the instruction, in bytes.  */

int
print_insn_m68k (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  register int i;
  register unsigned char *p;
  unsigned char *save_p;
  register const char *d;
  register unsigned long bestmask;
  const struct m68k_opcode *best = 0;
  unsigned int arch_mask;
  struct private priv;
  bfd_byte *buffer = priv.the_buffer;
  fprintf_ftype save_printer = info->fprintf_func;
  void (*save_print_address) PARAMS((bfd_vma, struct disassemble_info*))
    = info->print_address_func;
  int major_opcode;
  static int numopcodes[16];
  static const struct m68k_opcode **opcodes[16];

  if (!opcodes[0])
    {
      /* Speed up the matching by sorting the opcode table on the upper
	 four bits of the opcode.  */
      const struct m68k_opcode **opc_pointer[16];

      /* First count how many opcodes are in each of the sixteen buckets.  */
      for (i = 0; i < m68k_numopcodes; i++)
	numopcodes[(m68k_opcodes[i].opcode >> 28) & 15]++;

      /* Then create a sorted table of pointers that point into the
	 unsorted table.  */
      opc_pointer[0] = ((const struct m68k_opcode **)
			malloc (sizeof (struct m68k_opcode *)
				 * m68k_numopcodes));
      opcodes[0] = opc_pointer[0];
      for (i = 1; i < 16; i++)
	{
	  opc_pointer[i] = opc_pointer[i - 1] + numopcodes[i - 1];
	  opcodes[i] = opc_pointer[i];
	}

      for (i = 0; i < m68k_numopcodes; i++)
	*opc_pointer[(m68k_opcodes[i].opcode >> 28) & 15]++ = &m68k_opcodes[i];

    }

  info->private_data = (PTR) &priv;
  /* Tell objdump to use two bytes per chunk and six bytes per line for
     displaying raw data.  */
  info->bytes_per_chunk = 2;
  info->bytes_per_line = 6;
  info->display_endian = BFD_ENDIAN_BIG;
  priv.max_fetched = priv.the_buffer;
  priv.insn_start = memaddr;
  if (setjmp (priv.bailout) != 0)
    /* Error return.  */
    return -1;

  switch (info->mach)
    {
    default:
    case 0:
      arch_mask = (unsigned int) -1;
      break;
    case bfd_mach_m68000:
      arch_mask = m68000;
      break;
    case bfd_mach_m68008:
      arch_mask = m68008;
      break;
    case bfd_mach_m68010:
      arch_mask = m68010;
      break;
    case bfd_mach_m68020:
      arch_mask = m68020;
      break;
    case bfd_mach_m68030:
      arch_mask = m68030;
      break;
    case bfd_mach_m68040:
      arch_mask = m68040;
      break;
    case bfd_mach_m68060:
      arch_mask = m68060;
      break;
    }

  arch_mask |= m68881 | m68851;

  bestmask = 0;
  FETCH_DATA (info, buffer + 2);
  major_opcode = (buffer[0] >> 4) & 15;
  for (i = 0; i < numopcodes[major_opcode]; i++)
    {
      const struct m68k_opcode *opc = opcodes[major_opcode][i];
      unsigned long opcode = opc->opcode;
      unsigned long match = opc->match;

      if (((0xff & buffer[0] & (match >> 24)) == (0xff & (opcode >> 24)))
	  && ((0xff & buffer[1] & (match >> 16)) == (0xff & (opcode >> 16)))
	  /* Only fetch the next two bytes if we need to.  */
	  && (((0xffff & match) == 0)
	      ||
	      (FETCH_DATA (info, buffer + 4)
	       && ((0xff & buffer[2] & (match >> 8)) == (0xff & (opcode >> 8)))
	       && ((0xff & buffer[3] & match) == (0xff & opcode)))
	      )
	  && (opc->arch & arch_mask) != 0)
	{
	  /* Don't use for printout the variants of divul and divsl
	     that have the same register number in two places.
	     The more general variants will match instead.  */
	  for (d = opc->args; *d; d += 2)
	    if (d[1] == 'D')
	      break;

	  /* Don't use for printout the variants of most floating
	     point coprocessor instructions which use the same
	     register number in two places, as above. */
	  if (*d == '\0')
	    for (d = opc->args; *d; d += 2)
	      if (d[1] == 't')
		break;

	  /* Don't match fmovel with more than one register; wait for
             fmoveml.  */
	  if (*d == '\0')
	    {
	      for (d = opc->args; *d; d += 2)
		{
		  if (d[0] == 's' && d[1] == '8')
		    {
		      int val;

		      val = fetch_arg (buffer, d[1], 3, info);
		      if ((val & (val - 1)) != 0)
			break;
		    }
		}
	    }

	  if (*d == '\0' && match > bestmask)
	    {
	      best = opc;
	      bestmask = match;
	    }
	}
    }

  if (best == 0)
    goto invalid;

  /* Point at first word of argument data,
     and at descriptor for first argument.  */
  p = buffer + 2;
  
  /* Figure out how long the fixed-size portion of the instruction is.
     The only place this is stored in the opcode table is
     in the arguments--look for arguments which specify fields in the 2nd
     or 3rd words of the instruction.  */
  for (d = best->args; *d; d += 2)
    {
      /* I don't think it is necessary to be checking d[0] here; I suspect
	 all this could be moved to the case statement below.  */
      if (d[0] == '#')
	{
	  if (d[1] == 'l' && p - buffer < 6)
	    p = buffer + 6;
	  else if (p - buffer < 4 && d[1] != 'C' && d[1] != '8' )
	    p = buffer + 4;
	}
      if ((d[0] == 'L' || d[0] == 'l') && d[1] == 'w' && p - buffer < 4)
	p = buffer + 4;
      switch (d[1])
	{
	case '1':
	case '2':
	case '3':
	case '7':
	case '8':
	case '9':
	case 'i':
	  if (p - buffer < 4)
	    p = buffer + 4;
	  break;
	case '4':
	case '5':
	case '6':
	  if (p - buffer < 6)
	    p = buffer + 6;
	  break;
	default:
	  break;
	}
    }

  /* pflusha is an exceptions.  It takes no arguments but is two words
     long.  Recognize it by looking at the lower 16 bits of the mask.  */
  if (p - buffer < 4 && (best->match & 0xFFFF) != 0)
    p = buffer + 4;

  /* lpstop is another exception.  It takes a one word argument but is
     three words long.  */
  if (p - buffer < 6
      && (best->match & 0xffff) == 0xffff
      && best->args[0] == '#'
      && best->args[1] == 'w')
    {
      /* Copy the one word argument into the usual location for a one
	 word argument, to simplify printing it.  We can get away with
	 this because we know exactly what the second word is, and we
	 aren't going to print anything based on it.  */
      p = buffer + 6;
      FETCH_DATA (info, p);
      buffer[2] = buffer[4];
      buffer[3] = buffer[5];
    }

  FETCH_DATA (info, p);
  
  d = best->args;

  /* We can the operands twice.  The first time we don't print anything,
     but look for errors. */

  save_p = p;
  info->print_address_func = dummy_print_address;
  info->fprintf_func = (fprintf_ftype)dummy_printer;
  for ( ; *d; d += 2)
    {
      int eaten = print_insn_arg (d, buffer, p, memaddr + (p - buffer), info);
      if (eaten >= 0)
	p += eaten;
      else if (eaten == -1)
	goto invalid;
      else
	{
	  (*info->fprintf_func)(info->stream,
				/* xgettext:c-format */
				_("<internal error in opcode table: %s %s>\n"),
				best->name,
				best->args);
	  goto invalid;
	}

    }
  p = save_p;
  info->fprintf_func = save_printer;
  info->print_address_func = save_print_address;

  d = best->args;

  (*info->fprintf_func) (info->stream, "%s", best->name);

  if (*d)
    (*info->fprintf_func) (info->stream, "\t");

  while (*d)
    {
      p += print_insn_arg (d, buffer, p, memaddr + (p - buffer), info);
      d += 2;
      if (*d && *(d - 2) != 'I' && *d != 'k')
	(*info->fprintf_func) (info->stream, ",");
    }
  return p - buffer;

invalid: {
	extern void print_68k_invalid_opcode(unsigned long, struct disassemble_info *);

    /* Handle undefined instructions.  */
    info->fprintf_func = save_printer;
    info->print_address_func = save_print_address;
	print_68k_invalid_opcode((buffer[0] << 8) | buffer[1], info);
    return 2;
  }
}

/* Returns number of bytes "eaten" by the operand, or
   return -1 if an invalid operand was found, or -2 if
   an opcode tabe error was found. */

static int
print_insn_arg (d, buffer, p0, addr, info)
     const char *d;
     unsigned char *buffer;
     unsigned char *p0;
     bfd_vma addr;		/* PC for this arg to be relative to */
     disassemble_info *info;
{
  register int val = 0;
  register int place = d[1];
  register unsigned char *p = p0;
  int regno;
  register CONST char *regname;
  register unsigned char *p1;
  double flval;
  int flt_p;
  bfd_signed_vma disp;
  unsigned int uval;

  switch (*d)
    {
    case 'c':		/* cache identifier */
      {
        static char *const cacheFieldName[] = { "nc", "dc", "ic", "bc" };
        val = fetch_arg (buffer, place, 2, info);
        (*info->fprintf_func) (info->stream, cacheFieldName[val]);
        break;
      }

    case 'a':		/* address register indirect only. Cf. case '+'. */
      {
        (*info->fprintf_func)
	  (info->stream,
	   "(%s)",
	   reg_names [fetch_arg (buffer, place, 3, info) + 8]);
        break;
      }

    case '_':		/* 32-bit absolute address for move16. */
      {
        uval = NEXTULONG (p);
	(*info->print_address_func) (uval, info);
        break;
      }

    case 'C':
      (*info->fprintf_func) (info->stream, "ccr");
      break;

    case 'S':
      (*info->fprintf_func) (info->stream, "sr");
      break;

    case 'U':
      (*info->fprintf_func) (info->stream, "usp");
      break;

    case 'J':
      {
	static const struct { char *name; int value; } names[]
	  = {{"sfc", 0x000}, {"dfc", 0x001}, {"cacr", 0x002},
	     {"tc",  0x003}, {"itt0",0x004}, {"itt1", 0x005},
	     {"dtt0",0x006}, {"dtt1",0x007}, {"buscr",0x008},
	     {"usp", 0x800}, {"vbr", 0x801}, {"caar", 0x802},
	     {"msp", 0x803}, {"isp", 0x804},

	     /* Should we be calling this psr like we do in case 'Y'?  */
	     {"mmusr",0x805},

	     {"urp", 0x806}, {"srp", 0x807}, {"pcr", 0x808}};

	val = fetch_arg (buffer, place, 12, info);
	for (regno = sizeof names / sizeof names[0] - 1; regno >= 0; regno--)
	  if (names[regno].value == val)
	    {
	      (*info->fprintf_func) (info->stream, "%s", names[regno].name);
	      break;
	    }
	if (regno < 0)
	  (*info->fprintf_func) (info->stream, "$%04x", val);
      }
      break;

    case 'Q':
      val = fetch_arg (buffer, place, 3, info);
      /* 0 means 8, except for the bkpt instruction... */
      if (val == 0 && d[1] != 's')
	val = 8;
      (*info->fprintf_func) (info->stream, "#%d", val);
      break;

    case 'M':
      val = fetch_arg (buffer, place, 8, info);
      if (val & 0x80)
	val = val - 0x100;
      (*info->fprintf_func) (info->stream, "#$%02x", val);
      break;

    case 'T':
      val = fetch_arg (buffer, place, 4, info);
      (*info->fprintf_func) (info->stream, "#$%08x", val);
      break;

    case 'D':
      (*info->fprintf_func) (info->stream, "%s",
			     reg_names[fetch_arg (buffer, place, 3, info)]);
      break;

    case 'A':
      (*info->fprintf_func)
	(info->stream, "%s",
	 reg_names[fetch_arg (buffer, place, 3, info) + 010]);
      break;

    case 'R':
      (*info->fprintf_func)
	(info->stream, "%s",
	 reg_names[fetch_arg (buffer, place, 4, info)]);
      break;

    case 'r':
      regno = fetch_arg (buffer, place, 4, info);
	(*info->fprintf_func) (info->stream, "(%s)", reg_names[regno]);
      break;

    case 'F':
      (*info->fprintf_func)
	(info->stream, "fp%d",
	 fetch_arg (buffer, place, 3, info));
      break;

    case 'O':
      val = fetch_arg (buffer, place, 6, info);
      if (val & 0x20)
	(*info->fprintf_func) (info->stream, "%s", reg_names [val & 7]);
      else
	(*info->fprintf_func) (info->stream, "%d", val);
      break;

    case '+':
      (*info->fprintf_func)
	(info->stream, "(%s)+",
	 reg_names[fetch_arg (buffer, place, 3, info) + 8]);
      break;

    case '-':
      (*info->fprintf_func)
	(info->stream, "-(%s)",
	 reg_names[fetch_arg (buffer, place, 3, info) + 8]);
      break;

    case 'k':
      if (place == 'k')
	(*info->fprintf_func)
	  (info->stream, "{%s}",
	   reg_names[fetch_arg (buffer, place, 3, info)]);
      else if (place == 'C')
	{
	  val = fetch_arg (buffer, place, 7, info);
	  if ( val > 63 )		/* This is a signed constant. */
	    val -= 128;
	  (*info->fprintf_func) (info->stream, "{#%d}", val);
	}
      else
	return -2;
      break;

    case '#':
    case '^':
      p1 = buffer + (*d == '#' ? 2 : 4);
      if (place == 's')
	val = fetch_arg (buffer, place, 4, info);
      else if (place == 'C')
	val = fetch_arg (buffer, place, 7, info);
      else if (place == '8')
	val = fetch_arg (buffer, place, 3, info);
      else if (place == '3')
	val = fetch_arg (buffer, place, 8, info);
      else if (place == 'b') {
	val = NEXTBYTE (p1);
    (*info->fprintf_func) (info->stream, "#$%02x", val & 0xff);
	break;
	}
      else if (place == 'w' || place == 'W') {
	val = NEXTWORD (p1);
    (*info->fprintf_func) (info->stream, "#$%04x", val & 0xffff);
	break;
	}
      else if (place == 'l') {
	val = NEXTLONG (p1);
    (*info->fprintf_func) (info->stream, "#$%08x", val);
	break;
	}
      else
	return -2;
      (*info->fprintf_func) (info->stream, "#%d", val);
      break;

    case 'B':
      if (place == 'b')
	disp = NEXTBYTE (p);
      else if (place == 'B')
	disp = COERCE_SIGNED_CHAR(buffer[1]);
      else if (place == 'w' || place == 'W')
	disp = NEXTWORD (p);
      else if (place == 'l' || place == 'L' || place == 'C')
	disp = NEXTLONG (p);
      else if (place == 'g')
	{
	  disp = NEXTBYTE (buffer);
	  if (disp == 0)
	    disp = NEXTWORD (p);
	  else if (disp == -1)
	    disp = NEXTLONG (p);
	}
      else if (place == 'c')
	{
	  if (buffer[1] & 0x40)		/* If bit six is one, long offset */
	    disp = NEXTLONG (p);
	  else
	    disp = NEXTWORD (p);
	}
      else
	return -2;

      (*info->print_address_func) (addr + disp, info);
      break;

    case 'd':
      val = NEXTWORD (p);
      (*info->fprintf_func)
	(info->stream, "($%04x,%s)",
	 val, reg_names[fetch_arg (buffer, place, 3, info) + 8]);
      break;

    case 's':
      (*info->fprintf_func) (info->stream, "%s",
			     fpcr_names[fetch_arg (buffer, place, 3, info)]);
      break;

    case 'I':
      /* Get coprocessor ID... */
      val = fetch_arg (buffer, 'd', 3, info);
      
      if (val != 1)				/* Unusual coprocessor ID? */
	(*info->fprintf_func) (info->stream, "(cpid=%d) ", val);
      break;

    case '*':
    case '~':
    case '%':
    case ';':
    case '@':
    case '!':
    case '$':
    case '?':
    case '/':
    case '&':
    case '|':
    case '<':
    case '>':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'v':

      if (place == 'd')
	{
	  val = fetch_arg (buffer, 'x', 6, info);
	  val = ((val & 7) << 3) + ((val >> 3) & 7);
	}
      else
	val = fetch_arg (buffer, 's', 6, info);

      /* Get register number assuming address register.  */
      regno = (val & 7) + 8;
      regname = reg_names[regno];
      switch (val >> 3)
	{
	case 0:
	  (*info->fprintf_func) (info->stream, "%s", reg_names[val]);
	  break;

	case 1:
	  (*info->fprintf_func) (info->stream, "%s", regname);
	  break;

	case 2:
	  (*info->fprintf_func) (info->stream, "(%s)", regname);
	  break;

	case 3:
	  (*info->fprintf_func) (info->stream, "(%s)+", regname);
	  break;

	case 4:
	  (*info->fprintf_func) (info->stream, "-(%s)", regname);
	  break;

	case 5:
	  val = NEXTWORD (p);
	  (*info->fprintf_func) (info->stream, "($%04x,%s)", val, regname);
	  break;

	case 6:
	  p = print_indexed (regno, p, addr, info);
	  break;

	case 7:
	  switch (val & 7)
	    {
	    case 0:
	      val = NEXTWORD (p);
	      (*info->print_address_func) (val, info);
	      break;

	    case 1:
	      uval = NEXTULONG (p);
	      (*info->print_address_func) (uval, info);
	      break;

	    case 2:
	      val = NEXTWORD (p);
	      (*info->fprintf_func) (info->stream, "(");
	      (*info->print_address_func) (addr + val, info);
	      (*info->fprintf_func) (info->stream, ",pc)");
	      break;

	    case 3:
	      p = print_indexed (-1, p, addr, info);
	      break;

	    case 4:
	      switch( place )
	      {
		case 'b':
		  val = NEXTBYTE (p);
          (*info->fprintf_func) (info->stream, "#$%02x", val & 0xff);
		  goto imm_printed;

		case 'w':
		  val = NEXTWORD (p);
          (*info->fprintf_func) (info->stream, "#$%04x", val & 0xffff);
		  goto imm_printed;

		case 'l':
		  val = NEXTLONG (p);
          (*info->fprintf_func) (info->stream, "#$%08x", val);
		  goto imm_printed;

		case 'f':
		  NEXTSINGLE(flval, p);
		  break;

		case 'F':
		  NEXTDOUBLE(flval, p);
		  break;

		case 'x':
		  NEXTEXTEND(flval, p);
		  break;

		case 'p':
		  flval = NEXTPACKED(p);
		  break;

		default:
		  return -1;
	      }
		(*info->fprintf_func) (info->stream, "#%g", flval);
imm_printed:
	      break;

	    default:
	      return -1;
	    }
	}
      break;

    case 'L':
    case 'l':
	if (place == 'w')
	  {
	    char doneany;
	    p1 = buffer + 2;
	    val = NEXTWORD (p1);
	    /* Move the pointer ahead if this point is farther ahead
	       than the last.  */
	    p = p1 > p ? p1 : p;
	    if (val == 0)
	      {
		(*info->fprintf_func) (info->stream, "#0");
		break;
	      }
	    if (*d == 'l')
	      {
		register int newval = 0;
		for (regno = 0; regno < 16; ++regno)
		  if (val & (0x8000 >> regno))
		    newval |= 1 << regno;
		val = newval;
	      }
	    val &= 0xffff;
	    doneany = 0;
	    for (regno = 0; regno < 16; ++regno)
	      if (val & (1 << regno))
		{
		  int first_regno;
		  if (doneany)
		    (*info->fprintf_func) (info->stream, "/");
		  doneany = 1;
		  (*info->fprintf_func) (info->stream, "%s", reg_names[regno]);
		  first_regno = regno;
		  while (val & (1 << (regno + 1)))
		    ++regno;
		  if (regno > first_regno)
		    (*info->fprintf_func) (info->stream, "-%s",
					   reg_names[regno]);
		}
	  }
	else if (place == '3')
	  {
	    /* `fmovem' insn.  */
	    char doneany;
	    val = fetch_arg (buffer, place, 8, info);
	    if (val == 0)
	      {
		(*info->fprintf_func) (info->stream, "#0");
		break;
	      }
	    if (*d == 'l')
	      {
		register int newval = 0;
		for (regno = 0; regno < 8; ++regno)
		  if (val & (0x80 >> regno))
		    newval |= 1 << regno;
		val = newval;
	      }
	    val &= 0xff;
	    doneany = 0;
	    for (regno = 0; regno < 8; ++regno)
	      if (val & (1 << regno))
		{
		  int first_regno;
		  if (doneany)
		    (*info->fprintf_func) (info->stream, "/");
		  doneany = 1;
		  (*info->fprintf_func) (info->stream, "fp%d", regno);
		  first_regno = regno;
		  while (val & (1 << (regno + 1)))
		    ++regno;
		  if (regno > first_regno)
		    (*info->fprintf_func) (info->stream, "-fp%d", regno);
		}
	  }
	else if (place == '8')
	  {
	    /* fmoveml for FP status registers */
	    (*info->fprintf_func) (info->stream, "%s",
				   fpcr_names[fetch_arg (buffer, place, 3,
							 info)]);
	  }
	else
	  return -2;
      break;

    case 'X':
      place = '8';
    case 'Y':
    case 'Z':
    case 'W':
    case '0':
    case '1':
    case '2':
    case '3':
      {
	int val = fetch_arg (buffer, place, 5, info);
	char *name = 0;
	switch (val)
	  {
	  case 2: name = "tt0"; break;
	  case 3: name = "tt1"; break;
	  case 0x10: name = "tc"; break;
	  case 0x11: name = "drp"; break;
	  case 0x12: name = "srp"; break;
	  case 0x13: name = "crp"; break;
	  case 0x14: name = "cal"; break;
	  case 0x15: name = "val"; break;
	  case 0x16: name = "scc"; break;
	  case 0x17: name = "ac"; break;
 	  case 0x18: name = "psr"; break;
	  case 0x19: name = "pcsr"; break;
	  case 0x1c:
	  case 0x1d:
	    {
	      int break_reg = ((buffer[3] >> 2) & 7);
	      (*info->fprintf_func)
		(info->stream, val == 0x1c ? "bad%d" : "bac%d",
		 break_reg);
	    }
	    break;
	  default:
	    (*info->fprintf_func) (info->stream, "<mmu register %d>", val);
	  }
	if (name)
	  (*info->fprintf_func) (info->stream, "%s", name);
      }
      break;

    case 'f':
      {
	int fc = fetch_arg (buffer, place, 5, info);
	if (fc == 1)
	  (*info->fprintf_func) (info->stream, "dfc");
	else if (fc == 0)
	  (*info->fprintf_func) (info->stream, "sfc");
	else
	  /* xgettext:c-format */
	  (*info->fprintf_func) (info->stream, _("<function code %d>"), fc);
      }
      break;

    case 'V':
      (*info->fprintf_func) (info->stream, "val");
      break;

    case 't':
      {
	int level = fetch_arg (buffer, place, 3, info);
	(*info->fprintf_func) (info->stream, "%d", level);
      }
      break;

    default:
      return -2;
    }

  return p - p0;
}

/* Fetch BITS bits from a position in the instruction specified by CODE.
   CODE is a "place to put an argument", or 'x' for a destination
   that is a general address (mode and register).
   BUFFER contains the instruction.  */

static int
fetch_arg (buffer, code, bits, info)
     unsigned char *buffer;
     int code;
     int bits;
     disassemble_info *info;
{
  register int val = 0;
  switch (code)
    {
    case 's':
      val = buffer[1];
      break;

    case 'd':			/* Destination, for register or quick.  */
      val = (buffer[0] << 8) + buffer[1];
      val >>= 9;
      break;

    case 'x':			/* Destination, for general arg */
      val = (buffer[0] << 8) + buffer[1];
      val >>= 6;
      break;

    case 'k':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[3] >> 4);
      break;

    case 'C':
      FETCH_DATA (info, buffer + 3);
      val = buffer[3];
      break;

    case '1':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      val >>= 12;
      break;

    case '2':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      val >>= 6;
      break;

    case '3':
    case 'j':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      break;

    case '4':
      FETCH_DATA (info, buffer + 5);
      val = (buffer[4] << 8) + buffer[5];
      val >>= 12;
      break;

    case '5':
      FETCH_DATA (info, buffer + 5);
      val = (buffer[4] << 8) + buffer[5];
      val >>= 6;
      break;

    case '6':
      FETCH_DATA (info, buffer + 5);
      val = (buffer[4] << 8) + buffer[5];
      break;

    case '7':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      val >>= 7;
      break;
      
    case '8':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      val >>= 10;
      break;

    case '9':
      FETCH_DATA (info, buffer + 3);
      val = (buffer[2] << 8) + buffer[3];
      val >>= 5;
      break;

    case 'e':
      val = (buffer[1] >> 6);
      break;

    default:
      abort ();
    }

  switch (bits)
    {
    case 2:
      return val & 3;
    case 3:
      return val & 7;
    case 4:
      return val & 017;
    case 5:
      return val & 037;
    case 6:
      return val & 077;
    case 7:
      return val & 0177;
    case 8:
      return val & 0377;
    case 12:
      return val & 07777;
    default:
      abort ();
    }
}

/* Print an indexed argument.  The base register is BASEREG (-1 for pc).
   P points to extension word, in buffer.
   ADDR is the nominal core address of that extension word.  */

static unsigned char *
print_indexed (basereg, p, addr, info)
     int basereg;
     unsigned char *p;
     bfd_vma addr;
     disassemble_info *info;
{
  register int word;
  static char *const scales[] = {"", "*2", "*4", "*8"};
  bfd_vma base_disp;
  bfd_vma outer_disp;
  char buf[40];
  char vmabuf[50];

  word = NEXTWORD (p);

  /* Generate the text for the index register.
     Where this will be output is not yet determined.  */
  sprintf (buf, "%s.%c%s",
	   reg_names[(word >> 12) & 0xf],
	   (word & 0x800) ? 'l' : 'w',
	   scales[(word >> 9) & 3]);

  /* Handle the 68000 style of indexing.  */

  if ((word & 0x100) == 0)
    {
      base_disp = word & 0xff;
      if ((base_disp & 0x80) != 0)
	base_disp -= 0x100;
      if (basereg == -1)
	base_disp += addr;
      (*info->fprintf_func) (info->stream, "(", buf);
      print_base (basereg, base_disp, info);
      (*info->fprintf_func) (info->stream, ",%s)", buf);
      return p;
    }

  /* Handle the generalized kind.  */
  /* First, compute the displacement to add to the base register.  */

  if (word & 0200)
    {
      if (basereg == -1)
	basereg = -3;
      else
	basereg = -2;
    }
  if (word & 0100)
    buf[0] = '\0';
  base_disp = 0;
  switch ((word >> 4) & 3)
    {
    case 2:
      base_disp = NEXTWORD (p);
      break;
    case 3:
      base_disp = NEXTLONG (p);
    }
  if (basereg == -1)
    base_disp += addr;

  /* Handle single-level case (not indirect) */

  if ((word & 7) == 0)
    {
      (*info->fprintf_func) (info->stream, "(");
      print_base (basereg, base_disp, info);
      if (buf[0] != '\0')
	(*info->fprintf_func) (info->stream, ",%s", buf);
      (*info->fprintf_func) (info->stream, ")");
      return p;
    }

  /* Two level.  Compute displacement to add after indirection.  */

  outer_disp = 0;
  switch (word & 3)
    {
    case 2:
      outer_disp = NEXTWORD (p);
      break;
    case 3:
      outer_disp = NEXTLONG (p);
    }

  (*info->fprintf_func) (info->stream, "([");
  print_base (basereg, base_disp, info);
  if ((word & 4) == 0 && buf[0] != '\0')
    {
      (*info->fprintf_func) (info->stream, ",%s", buf);
      buf[0] = '\0';
    }
  if (outer_disp)
    (*info->fprintf_func) (info->stream, "],$%08x", (uint32)outer_disp);
  else
    (*info->fprintf_func) (info->stream, "]");
  if (buf[0] != '\0')
    (*info->fprintf_func) (info->stream, ",%s", buf);
  (*info->fprintf_func) (info->stream, ")");

  return p;
}

/* Print a base register REGNO and displacement DISP, on INFO->STREAM.
   REGNO = -1 for pc, -2 for none (suppressed).  */

static void
print_base (regno, disp, info)
     int regno;
     bfd_vma disp;
     disassemble_info *info;
{
	if (regno == -1) {
		(*info->print_address_func) (disp, info);
		(*info->fprintf_func) (info->stream, ",pc");
	}
	else {
		if (regno == -3) {
			(*info->print_address_func) (disp, info);
			(*info->fprintf_func) (info->stream, ",zpc");
		}
		else if (regno == -2)
			(*info->print_address_func) (disp, info);
		else
			(*info->fprintf_func) (info->stream, "$%08x,%s", (uint32)disp, reg_names[regno]);
	}
}
