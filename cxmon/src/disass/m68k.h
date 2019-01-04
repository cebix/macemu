/* Opcode table header for m680[01234]0/m6888[12]/m68851.
   Copyright 1989, 91, 92, 93, 94, 95, 96, 1997 Free Software Foundation.

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* These are used as bit flags for the arch field in the m68k_opcode
   structure.  */
#define	_m68k_undef  0
#define	m68000  0x001
#define	m68008  m68000 /* synonym for -m68000.  otherwise unused. */
#define	m68010  0x002
#define	m68020  0x004
#define	m68030  0x008
#define m68ec030 m68030 /* similar enough to -m68030 to ignore differences;
			   gas will deal with the few differences.  */
#define	m68040  0x010
/* there is no 68050 */
#define m68060  0x020
#define	m68881  0x040
#define	m68882  m68881 /* synonym for -m68881.  otherwise unused. */
#define	m68851  0x080
#define cpu32	0x100	/* e.g., 68332 */
#define mcf5200 0x200

 /* handy aliases */
#define	m68040up  (m68040 | m68060)
#define	m68030up  (m68030 | m68040up)
#define	m68020up  (m68020 | m68030up)
#define	m68010up  (m68010 | cpu32 | m68020up)
#define	m68000up  (m68000 | m68010up)

#define	mfloat  (m68881 | m68882 | m68040 | m68060)
#define	mmmu    (m68851 | m68030 | m68040 | m68060)

/* The structure used to hold information for an opcode.  */

struct m68k_opcode
{
  /* The opcode name.  */
  const char *name;
  /* The opcode itself.  */
  unsigned long opcode;
  /* The mask used by the disassembler.  */
  unsigned long match;
  /* The arguments.  */
  const char *args;
  /* The architectures which support this opcode.  */
  unsigned int arch;
};

/* The structure used to hold information for an opcode alias.  */

struct m68k_opcode_alias
{
  /* The alias name.  */
  const char *alias;
  /* The instruction for which this is an alias.  */
  const char *primary;
};

/* We store four bytes of opcode for all opcodes because that is the
   most any of them need.  The actual length of an instruction is
   always at least 2 bytes, and is as much longer as necessary to hold
   the operands it has.

   The match field is a mask saying which bits must match particular
   opcode in order for an instruction to be an instance of that
   opcode.

   The args field is a string containing two characters for each
   operand of the instruction.  The first specifies the kind of
   operand; the second, the place it is stored.  */

/* Kinds of operands:
   Characters used: AaBCcDdFfIJkLlMmnOopQqRrSsTtUVvWXYZ0123|*~%;@!&$?/<>#^+-

   D  data register only.  Stored as 3 bits.
   A  address register only.  Stored as 3 bits.
   a  address register indirect only.  Stored as 3 bits.
   R  either kind of register.  Stored as 4 bits.
   r  either kind of register indirect only.  Stored as 4 bits.
      At the moment, used only for cas2 instruction.
   F  floating point coprocessor register only.   Stored as 3 bits.
   O  an offset (or width): immediate data 0-31 or data register.
      Stored as 6 bits in special format for BF... insns.
   +  autoincrement only.  Stored as 3 bits (number of the address register).
   -  autodecrement only.  Stored as 3 bits (number of the address register).
   Q  quick immediate data.  Stored as 3 bits.
      This matches an immediate operand only when value is in range 1 .. 8.
   M  moveq immediate data.  Stored as 8 bits.
      This matches an immediate operand only when value is in range -128..127
   T  trap vector immediate data.  Stored as 4 bits.

   k  K-factor for fmove.p instruction.   Stored as a 7-bit constant or
      a three bit register offset, depending on the field type.

   #  immediate data.  Stored in special places (b, w or l)
      which say how many bits to store.
   ^  immediate data for floating point instructions.   Special places
      are offset by 2 bytes from '#'...
   B  pc-relative address, converted to an offset
      that is treated as immediate data.
   d  displacement and register.  Stores the register as 3 bits
      and stores the displacement in the entire second word.

   C  the CCR.  No need to store it; this is just for filtering validity.
   S  the SR.  No need to store, just as with CCR.
   U  the USP.  No need to store, just as with CCR.

   I  Coprocessor ID.   Not printed if 1.   The Coprocessor ID is always
      extracted from the 'd' field of word one, which means that an extended
      coprocessor opcode can be skipped using the 'i' place, if needed.

   s  System Control register for the floating point coprocessor.

   J  Misc register for movec instruction, stored in 'j' format.
	Possible values:
	0x000	SFC	Source Function Code reg	[60, 40, 30, 20, 10]
	0x001	DFC	Data Function Code reg		[60, 40, 30, 20, 10]
	0x002	CACR	Cache Control Register		[60, 40, 30, 20]
	0x003	TC	MMU Translation Control		[60, 40]
	0x004	ITT0	Instruction Transparent
				Translation reg 0	[60, 40]
	0x005	ITT1	Instruction Transparent
				Translation reg 1	[60, 40]
	0x006	DTT0	Data Transparent
				Translation reg 0	[60, 40]
	0x007	DTT1	Data Transparent
				Translation reg 1	[60, 40]
	0x008	BUSCR	Bus Control Register		[60]
	0x800	USP	User Stack Pointer		[60, 40, 30, 20, 10]
	0x801	VBR	Vector Base reg			[60, 40, 30, 20, 10]
	0x802	CAAR	Cache Address Register		[        30, 20]
	0x803	MSP	Master Stack Pointer		[    40, 30, 20]
	0x804	ISP	Interrupt Stack Pointer		[    40, 30, 20]
	0x805	MMUSR	MMU Status reg			[    40]
	0x806	URP	User Root Pointer		[60, 40]
	0x807	SRP	Supervisor Root Pointer		[60, 40]
	0x808	PCR	Processor Configuration reg	[60]
	0xC00	ROMBAR	ROM Base Address Register	[520X]
	0xC04	RAMBAR0	RAM Base Address Register 0	[520X]
	0xC05	RAMBAR1	RAM Base Address Register 0	[520X]
	0xC0F	MBAR0	RAM Base Address Register 0	[520X]

    L  Register list of the type d0-d7/a0-a7 etc.
       (New!  Improved!  Can also hold fp0-fp7, as well!)
       The assembler tries to see if the registers match the insn by
       looking at where the insn wants them stored.

    l  Register list like L, but with all the bits reversed.
       Used for going the other way. . .

    c  cache identifier which may be "nc" for no cache, "ic"
       for instruction cache, "dc" for data cache, or "bc"
       for both caches.  Used in cinv and cpush.  Always
       stored in position "d".

 The remainder are all stored as 6 bits using an address mode and a
 register number; they differ in which addressing modes they match.

   *  all					(modes 0-6,7.0-4)
   ~  alterable memory				(modes 2-6,7.0,7.1)
   						(not 0,1,7.2-4)
   %  alterable					(modes 0-6,7.0,7.1)
						(not 7.2-4)
   ;  data					(modes 0,2-6,7.0-4)
						(not 1)
   @  data, but not immediate			(modes 0,2-6,7.0-3)
						(not 1,7.4)
   !  control					(modes 2,5,6,7.0-3)
						(not 0,1,3,4,7.4)
   &  alterable control				(modes 2,5,6,7.0,7.1)
						(not 0,1,7.2-4)
   $  alterable data				(modes 0,2-6,7.0,7.1)
						(not 1,7.2-4)
   ?  alterable control, or data register	(modes 0,2,5,6,7.0,7.1)
						(not 1,3,4,7.2-4)
   /  control, or data register			(modes 0,2,5,6,7.0-3)
						(not 1,3,4,7.4)
   >  *save operands				(modes 2,4,5,6,7.0,7.1)
						(not 0,1,3,7.2-4)
   <  *restore operands				(modes 2,3,5,6,7.0-3)
						(not 0,1,4,7.4)

   coldfire move operands:
   m  						(modes 0-4)
   n						(modes 5,7.2)
   o						(modes 6,7.0,7.1,7.3,7.4)
   p						(modes 0-5)

   coldfire bset/bclr/btst operands:
   q						(modes 0,2-5)
   v						(modes 0,2-5,7.0,7.1)
*/

/* For the 68851: */
/*
   I didn't use much imagination in choosing the 
   following codes, so many of them aren't very
   mnemonic. -rab

   0  32 bit pmmu register
	Possible values:
	000	TC	Translation Control Register (68030, 68851)

   1  16 bit pmmu register
	111	AC	Access Control (68851)

   2  8 bit pmmu register
	100	CAL	Current Access Level (68851)
	101	VAL	Validate Access Level (68851)
	110	SCC	Stack Change Control (68851)

   3  68030-only pmmu registers (32 bit)
	010	TT0	Transparent Translation reg 0
			(aka Access Control reg 0 -- AC0 -- on 68ec030)
	011	TT1	Transparent Translation reg 1
			(aka Access Control reg 1 -- AC1 -- on 68ec030)

   W  wide pmmu registers
	Possible values:
	001	DRP	Dma Root Pointer (68851)
	010	SRP	Supervisor Root Pointer (68030, 68851)
	011	CRP	Cpu Root Pointer (68030, 68851)

   f	function code register (68030, 68851)
	0	SFC
	1	DFC

   V	VAL register only (68851)

   X	BADx, BACx (16 bit)
	100	BAD	Breakpoint Acknowledge Data (68851)
	101	BAC	Breakpoint Acknowledge Control (68851)

   Y	PSR (68851) (MMUSR on 68030) (ACUSR on 68ec030)
   Z	PCSR (68851)

   |	memory 		(modes 2-6, 7.*)

   t  address test level (68030 only)
      Stored as 3 bits, range 0-7.
      Also used for breakpoint instruction now.

*/

/* Places to put an operand, for non-general operands:
   s  source, low bits of first word.
   d  dest, shifted 9 in first word
   1  second word, shifted 12
   2  second word, shifted 6
   3  second word, shifted 0
   4  third word, shifted 12
   5  third word, shifted 6
   6  third word, shifted 0
   7  second word, shifted 7
   8  second word, shifted 10
   9  second word, shifted 5
   D  store in both place 1 and place 3; for divul and divsl.
   B  first word, low byte, for branch displacements
   W  second word (entire), for branch displacements
   L  second and third words (entire), for branch displacements
      (also overloaded for move16)
   b  second word, low byte
   w  second word (entire) [variable word/long branch offset for dbra]
   W  second word (entire) (must be signed 16 bit value)
   l  second and third word (entire)
   g  variable branch offset for bra and similar instructions.
      The place to store depends on the magnitude of offset.
   t  store in both place 7 and place 8; for floating point operations
   c  branch offset for cpBcc operations.
      The place to store is word two if bit six of word one is zero,
      and words two and three if bit six of word one is one.
   i  Increment by two, to skip over coprocessor extended operands.   Only
      works with the 'I' format.
   k  Dynamic K-factor field.   Bits 6-4 of word 2, used as a register number.
      Also used for dynamic fmovem instruction.
   C  floating point coprocessor constant - 7 bits.  Also used for static
      K-factors...
   j  Movec register #, stored in 12 low bits of second word.

 Places to put operand, for general operands:
   d  destination, shifted 6 bits in first word
   b  source, at low bit of first word, and immediate uses one byte
   w  source, at low bit of first word, and immediate uses two bytes
   l  source, at low bit of first word, and immediate uses four bytes
   s  source, at low bit of first word.
      Used sometimes in contexts where immediate is not allowed anyway.
   f  single precision float, low bit of 1st word, immediate uses 4 bytes
   F  double precision float, low bit of 1st word, immediate uses 8 bytes
   x  extended precision float, low bit of 1st word, immediate uses 12 bytes
   p  packed float, low bit of 1st word, immediate uses 12 bytes
*/

extern const struct m68k_opcode m68k_opcodes[];
extern const struct m68k_opcode_alias m68k_opcode_aliases[];

extern const int m68k_numopcodes, m68k_numaliases;

/* end of m68k-opcode.h */
