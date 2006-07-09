/*
 *  Generic Dynamic compiler generator
 * 
 *  Copyright (c) 2003-2004 Fabrice Bellard
 *
 *  The COFF object format support was extracted from Kazu's QEMU port
 *  to Win32.
 *
 *  Mach-O Support by Matt Reda and Pierre d'Herbemont
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>

#include "sysdeps.h"
#include "cxxdemangle.h"

/* object file format defs */
#ifndef CONFIG_WIN32
#if defined(__CYGWIN__) || defined(_WIN32)
#define CONFIG_WIN32 1
#endif
#endif
#ifndef CONFIG_DARWIN
#if defined(__APPLE__) && defined(__MACH__)
#define CONFIG_DARWIN 1
#endif
#endif

/* host cpu defs */
#if CONFIG_WIN32
#define HOST_I386 1
#elif defined(__i386__)
#define HOST_I386 1
#elif defined(__powerpc__) || defined(__ppc__)
#define HOST_PPC 1
#elif defined(__s390__)
#define HOST_S390 1
#elif defined(__alpha__)
#define HOST_ALPHA 1
#elif defined(__ia64__)
#define HOST_IA64 1
#elif defined(__sparc__)
#define HOST_SPARC 1
#elif defined(__x86_64__)
#define HOST_AMD64 1
#elif defined(__m68k__)
#define HOST_M68K 1
#elif defined(__mips__)
#define HOST_MIPS 1
#endif

/* Debug generated code */
#if ENABLE_MON && (defined(HOST_I386) || defined(HOST_AMD64)) && 0
#define DYNGEN_PRETTY_PRINT 1

#include "disass/dis-asm.h"

static inline bfd_byte bfd_read_byte(bfd_vma from)
{
  bfd_byte *p = (bfd_byte *)(uintptr_t)from;
  return *p;
}

int buffer_read_memory(bfd_vma from, bfd_byte *to, unsigned int length, struct disassemble_info *info)
{
  while (length--)
    *to++ = bfd_read_byte(from++);
  return 0;
}

void perror_memory(int status, bfd_vma memaddr, struct disassemble_info *info)
{
  info->fprintf_func(info->stream, "Unknown error %d\n", status);
}

static uintptr_t print_address_base;

void generic_print_address(bfd_vma addr, struct disassemble_info *info)
{
  addr -= print_address_base;
  if (addr >= UVAL64(0x100000000))
    info->fprintf_func(info->stream, "$%08x%08x", (uint32)(addr >> 32), (uint32)addr);
  else
    info->fprintf_func(info->stream, "$%08x", (uint32)addr);
}

int generic_symbol_at_address(bfd_vma addr, struct disassemble_info *info)
{
  return 0;
}

struct SFILE {
  char *buffer;
  char *current;
};

static int dyngen_sprintf(struct SFILE *f, const char *format, ...)
{
  int n;
  va_list args;
  va_start(args, format);
  vsprintf(f->current, format, args);
  f->current += n = strlen(f->current);
  va_end(args);
  return n;
}

#if defined(HOST_I386) || defined(HOST_AMD64)
static int pretty_print(char *buf, uintptr_t addr, uintptr_t base)
{
  disassemble_info info;
  struct SFILE sfile = {buf, buf};
  sfile.buffer = buf;
  sfile.current = buf;
  INIT_DISASSEMBLE_INFO(info, (FILE *)&sfile, (fprintf_ftype)dyngen_sprintf);
#if defined(HOST_AMD64)
  info.mach = bfd_mach_x86_64;
#endif
  print_address_base = base;
  return print_insn_i386_att(addr, &info);
}
#endif
#endif

/* NOTE: we test CONFIG_WIN32 instead of _WIN32 to enabled cross
   compilation */
#if defined(CONFIG_WIN32)
#define CONFIG_FORMAT_COFF
#elif defined(CONFIG_DARWIN)
#define CONFIG_FORMAT_MACH
#else
#define CONFIG_FORMAT_ELF
#endif

#ifdef CONFIG_FORMAT_ELF

/* elf format definitions. We use these macros to test the CPU to
   allow cross compilation (this tool must be ran on the build
   platform) */
#if defined(HOST_I386)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_386
#define elf_check_arch(x) ( ((x) == EM_386) || ((x) == EM_486) )
#undef ELF_USES_RELOCA

#elif defined(HOST_AMD64)

#define ELF_CLASS	ELFCLASS64
#define ELF_ARCH	EM_X86_64
#define elf_check_arch(x) ((x) == EM_X86_64)
#define ELF_USES_RELOCA

#elif defined(HOST_PPC)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_PPC
#define elf_check_arch(x) ((x) == EM_PPC)
#define ELF_USES_RELOCA

#elif defined(HOST_S390)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_S390
#define elf_check_arch(x) ((x) == EM_S390)
#define ELF_USES_RELOCA

#elif defined(HOST_ALPHA)

#define ELF_CLASS	ELFCLASS64
#define ELF_ARCH	EM_ALPHA
#define elf_check_arch(x) ((x) == EM_ALPHA)
#define ELF_USES_RELOCA

#elif defined(HOST_IA64)

#define ELF_CLASS	ELFCLASS64
#define ELF_ARCH	EM_IA_64
#define elf_check_arch(x) ((x) == EM_IA_64)
#define ELF_USES_RELOCA

#elif defined(HOST_SPARC)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_SPARC
#define elf_check_arch(x) ((x) == EM_SPARC || (x) == EM_SPARC32PLUS)
#define ELF_USES_RELOCA

#elif defined(HOST_SPARC64)

#define ELF_CLASS	ELFCLASS64
#define ELF_ARCH	EM_SPARCV9
#define elf_check_arch(x) ((x) == EM_SPARCV9)
#define ELF_USES_RELOCA

#elif defined(HOST_ARM)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_ARM
#define elf_check_arch(x) ((x) == EM_ARM)
#define ELF_USES_RELOC

#elif defined(HOST_M68K)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_68K
#define elf_check_arch(x) ((x) == EM_68K)
#define ELF_USES_RELOCA

#elif defined(HOST_MIPS)

#define ELF_CLASS	ELFCLASS32
#define ELF_ARCH	EM_MIPS
#define elf_check_arch(x) ((x) == EM_MIPS)
#define ELF_USES_RELOCA
#define ELF_USES_ALSO_RELOC

#else
#error unsupported CPU - please update the code
#endif

#include "elf-defs.h"

#ifndef ElfW
# if ELF_CLASS == ELFCLASS32
#  define ElfW(x)  Elf32_ ## x
#  define ELFW(x)  ELF32_ ## x
# else
#  define ElfW(x)  Elf64_ ## x
#  define ELFW(x)  ELF64_ ## x
# endif
#endif

#if ELF_CLASS == ELFCLASS32
typedef int32_t host_long;
typedef uint32_t host_ulong;
#define swabls(x) swab32s(x)
#else
typedef int64_t host_long;
typedef uint64_t host_ulong;
#define swabls(x) swab64s(x)
#endif

typedef ElfW(Ehdr) elfhdr;
typedef ElfW(Shdr) elf_shdr;
typedef ElfW(Phdr) elf_phdr;
typedef ElfW(Rel)  elf_rel;
typedef ElfW(Rela) elf_rela;

#ifdef ELF_USES_RELOCA
#define ELF_RELOC elf_rela
#define SHT_RELOC SHT_RELA
#else
#define ELF_RELOC elf_rel
#define SHT_RELOC SHT_REL
#endif

#define EXE_RELOC ELF_RELOC
#define EXE_SYM ElfW(Sym)

#endif /* CONFIG_FORMAT_ELF */

#ifdef CONFIG_FORMAT_COFF

#include "a.out-defs.h"

typedef int32_t host_long;
typedef uint32_t host_ulong;

#define FILENAMELEN 256

typedef struct coff_sym {
    struct external_syment *st_syment;
    char st_name[FILENAMELEN];
    uint32_t st_value;
    int  st_size;
    uint8_t st_type;
    uint8_t st_shndx;
} coff_Sym;

typedef struct coff_rel {
    struct external_reloc *r_reloc;
    int  r_offset;
    uint8_t r_type;
} coff_Rel;

#define EXE_RELOC struct coff_rel
#define EXE_SYM struct coff_sym

#endif /* CONFIG_FORMAT_COFF */

#ifdef CONFIG_FORMAT_MACH

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>

#ifdef HOST_PPC

#define MACH_CPU_TYPE CPU_TYPE_POWERPC
#define mach_check_cputype(x) ((x) == CPU_TYPE_POWERPC)

#elif defined(HOST_I386)

#define MACH_CPU_TYPE CPU_TYPE_I386
#define mach_check_cputype(x) ((x) == CPU_TYPE_I386)

#else
#error unsupported CPU - please update the code
#endif

# define check_mach_header(x) (x.magic == MH_MAGIC)
typedef int32_t host_long;
typedef uint32_t host_ulong;

struct nlist_extended
{
   union {
   char *n_name; 
   long  n_strx; 
   } n_un;
   unsigned char n_type; 
   unsigned char n_sect; 
   short st_desc;
   unsigned long st_value;
   unsigned long st_size;
};

#define EXE_RELOC struct relocation_info
#define EXE_SYM struct nlist_extended

#endif /* CONFIG_FORMAT_MACH */

enum {
    OUT_GEN_OP_ALL,
};

/* all dynamically generated functions begin with this code */
#define OP_PREFIX "op_"

int do_swap;

void __attribute__((noreturn)) __attribute__((format (printf, 1, 2))) error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "dyngen: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void *load_data(int fd, long offset, unsigned int size)
{
    char *data;

    data = malloc(size);
    if (!data)
        return NULL;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, data, size) != size) {
        free(data);
        return NULL;
    }
    return data;
}

int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    if (ptr)
        *ptr = p;
    return 1;
}

void pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
        return;

    for(;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1)
            break;
        *q++ = c;
    }
    *q = '\0';
}

void swab16s(uint16_t *p)
{
    *p = bswap_16(*p);
}

void swab32s(uint32_t *p)
{
    *p = bswap_32(*p);
}

void swab64s(uint64_t *p)
{
    *p = bswap_64(*p);
}

uint16_t get16(uint16_t *p)
{
    uint16_t val;
    val = *p;
    if (do_swap)
        val = bswap_16(val);
    return val;
}

uint32_t get32(uint32_t *p)
{
    uint32_t val;
    val = *p;
    if (do_swap)
        val = bswap_32(val);
    return val;
}

void put16(uint16_t *p, uint16_t val)
{
    if (do_swap)
        val = bswap_16(val);
    *p = val;
}

void put32(uint32_t *p, uint32_t val)
{
    if (do_swap)
        val = bswap_32(val);
    *p = val;
}

/* generate op code */
void gen_code(const char *name, const char *demangled_name,
              host_ulong offset, host_ulong size, 
              FILE *outfile, int gen_switch, const char *prefix);

static void do_print_code(FILE *outfile, const char *name, const uint8_t *code_p, int code_size, int is_code)
{
  int i, b;
  fprintf(outfile, "    static const uint8 %s[] = {", name);
#ifdef DYNGEN_PRETTY_PRINT
  if (is_code) {
    const int BYTES_PER_LINE = 5;
    uint8_t out[1024];
    int outindex = 0;
    char buf[1024];
    uintptr_t addr = (uintptr_t)code_p;
    uintptr_t end_addr = addr + code_size;
    int ip = 0;
    fprintf(outfile, "\n");
    while (addr < end_addr) {
      int num = pretty_print(buf, (uintptr_t)addr, (uintptr_t)code_p);
      int max_num = num > BYTES_PER_LINE ? num : BYTES_PER_LINE;
      for (i = 0; i < max_num; i++) {
        if ((i % BYTES_PER_LINE) == 0)
          fprintf(outfile, "/* %04x */  ", ip);
        if (i < num) {
          fprintf(outfile, "0x%02x", (out[outindex++] = code_p[ip++]));
          if (ip != code_size)
            fprintf(outfile, ", ");
          else
            fprintf(outfile, "  ");
        }
        else
          fprintf(outfile, "      ");
        if (i == BYTES_PER_LINE - 1)
          fprintf(outfile, "/* %s */", buf);
        if ((i + 1) % BYTES_PER_LINE == 0 || i == max_num - 1)
          fprintf(outfile, "\n");
      }
      addr += num;
    }
    fprintf(outfile, "    };\n");

    /* sanity check we have not forgotten any byte */
    assert(outindex == code_size);
    assert(memcmp(code_p, out, code_size) == 0);
    return;
  }
#endif
  for (i = 0; i < code_size; i++) {
    if ((i % 12) == 0) {
      if (i != 0)
        fprintf(outfile, ",");
      fprintf(outfile, "\n       ");
    }
    else
      fprintf(outfile, ", ");
    fprintf(outfile, "0x%02x", code_p[i]);
  }
  fprintf(outfile, "\n    };\n");
}

static void print_code(FILE *outfile, const char *name, const uint8_t *code_p, int code_size)
{
  char *code_name;
  code_name = alloca(strlen(name) + 5);
  strcpy(code_name, name);
  strcat(code_name, "_code");
  do_print_code(outfile, code_name, code_p, code_size, 1);
}

static void print_data(FILE *outfile, const char *name, const uint8_t *data, int data_size)
{
  do_print_code(outfile, name, data, data_size, 0);
}

static char *gen_dot_prefix(const char *sym_name)
{
  static char name[256];
  assert(sym_name[0] == '.');
  snprintf(name, sizeof(name), "dot_%s", sym_name + 1);
  return name;
}


/* executable information */
EXE_SYM *symtab;
int nb_syms;
int text_shndx;
int data_shndx;
uint8_t *text;
uint8_t *data;
EXE_RELOC *relocs;
int nb_relocs;

#ifdef CONFIG_FORMAT_ELF

/* ELF file info */
elf_shdr *shdr;
uint8_t **sdata;
elfhdr ehdr;
char *strtab;

int elf_must_swap(elfhdr *h)
{
  union {
      uint32_t i;
      uint8_t b[4];
  } swaptest;

  swaptest.i = 1;
  return (h->e_ident[EI_DATA] == ELFDATA2MSB) != 
      (swaptest.b[0] == 0);
}
  
void elf_swap_ehdr(elfhdr *h)
{
    swab16s(&h->e_type);			/* Object file type */
    swab16s(&h->	e_machine);		/* Architecture */
    swab32s(&h->	e_version);		/* Object file version */
    swabls(&h->	e_entry);		/* Entry point virtual address */
    swabls(&h->	e_phoff);		/* Program header table file offset */
    swabls(&h->	e_shoff);		/* Section header table file offset */
    swab32s(&h->	e_flags);		/* Processor-specific flags */
    swab16s(&h->	e_ehsize);		/* ELF header size in bytes */
    swab16s(&h->	e_phentsize);		/* Program header table entry size */
    swab16s(&h->	e_phnum);		/* Program header table entry count */
    swab16s(&h->	e_shentsize);		/* Section header table entry size */
    swab16s(&h->	e_shnum);		/* Section header table entry count */
    swab16s(&h->	e_shstrndx);		/* Section header string table index */
}

void elf_swap_shdr(elf_shdr *h)
{
  swab32s(&h->	sh_name);		/* Section name (string tbl index) */
  swab32s(&h->	sh_type);		/* Section type */
  swabls(&h->	sh_flags);		/* Section flags */
  swabls(&h->	sh_addr);		/* Section virtual addr at execution */
  swabls(&h->	sh_offset);		/* Section file offset */
  swabls(&h->	sh_size);		/* Section size in bytes */
  swab32s(&h->	sh_link);		/* Link to another section */
  swab32s(&h->	sh_info);		/* Additional section information */
  swabls(&h->	sh_addralign);		/* Section alignment */
  swabls(&h->	sh_entsize);		/* Entry size if section holds table */
}

void elf_swap_phdr(elf_phdr *h)
{
    swab32s(&h->p_type);			/* Segment type */
    swabls(&h->p_offset);		/* Segment file offset */
    swabls(&h->p_vaddr);		/* Segment virtual address */
    swabls(&h->p_paddr);		/* Segment physical address */
    swabls(&h->p_filesz);		/* Segment size in file */
    swabls(&h->p_memsz);		/* Segment size in memory */
    swab32s(&h->p_flags);		/* Segment flags */
    swabls(&h->p_align);		/* Segment alignment */
}

void elf_swap_rel(ELF_RELOC *rel)
{
    swabls(&rel->r_offset);
    swabls(&rel->r_info);
#ifdef ELF_USES_RELOCA
    swabls(&rel->r_addend);
#endif
}

elf_shdr *find_elf_section(elf_shdr *shdr, int shnum, const char *shstr, 
                                  const char *name)
{
    int i;
    const char *shname;
    elf_shdr *sec;

    for(i = 0; i < shnum; i++) {
        sec = &shdr[i];
        if (!sec->sh_name)
            continue;
        shname = shstr + sec->sh_name;
        if (!strcmp(shname, name))
            return sec;
    }
    return NULL;
}

static int do_find_reloc(int sh_index, ElfW(Word) type)
{
    elf_shdr *sec;
    int i;

    for(i = 0; i < ehdr.e_shnum; i++) {
        sec = &shdr[i];
        if (sec->sh_type == type && sec->sh_info == sh_index) 
            return i;
    }
    return 0;
}

static int find_reloc(int sh_index)
{
    return do_find_reloc(sh_index, SHT_RELOC);
}

static host_ulong get_rel_offset(EXE_RELOC *rel)
{
    return rel->r_offset;
}

static char *get_rel_sym_name(EXE_RELOC *rel)
{
    return strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
}

static char *get_sym_name(EXE_SYM *sym)
{
    return strtab + sym->st_name;
}

/* load an elf object file */
int load_object(const char *filename, FILE *outfile)
{
    int fd;
    elf_shdr *sec, *symtab_sec, *strtab_sec, *text_sec;
    int i, j;
    ElfW(Sym) *sym;
    char *shstr;
    ELF_RELOC *rel;
    elf_shdr *data_sec;
    elf_shdr *rodata_cst4_sec;
    uint8_t *rodata_cst4 = NULL;
    int rodata_cst4_shndx;
    elf_shdr *rodata_cst8_sec;
    uint8_t *rodata_cst8 = NULL;
    int rodata_cst8_shndx;
    elf_shdr *rodata_cst16_sec;
    uint8_t *rodata_cst16 = NULL;
    int rodata_cst16_shndx;
    
    fd = open(filename, O_RDONLY);
    if (fd < 0) 
        error("can't open file '%s'", filename);
    
    /* Read ELF header.  */
    if (read(fd, &ehdr, sizeof (ehdr)) != sizeof (ehdr))
        error("unable to read file header");

    /* Check ELF identification.  */
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0
     || ehdr.e_ident[EI_MAG1] != ELFMAG1
     || ehdr.e_ident[EI_MAG2] != ELFMAG2
     || ehdr.e_ident[EI_MAG3] != ELFMAG3
     || ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
        error("bad ELF header");
    }

    do_swap = elf_must_swap(&ehdr);
    if (do_swap)
        elf_swap_ehdr(&ehdr);
    if (ehdr.e_ident[EI_CLASS] != ELF_CLASS)
        error("Unsupported ELF class");
    if (ehdr.e_type != ET_REL)
        error("ELF object file expected");
    if (ehdr.e_version != EV_CURRENT)
        error("Invalid ELF version");
    if (!elf_check_arch(ehdr.e_machine))
        error("Unsupported CPU (e_machine=%d)", ehdr.e_machine);

    /* read section headers */
    shdr = load_data(fd, ehdr.e_shoff, ehdr.e_shnum * sizeof(elf_shdr));
    if (do_swap) {
        for(i = 0; i < ehdr.e_shnum; i++) {
            elf_swap_shdr(&shdr[i]);
        }
    }

    /* read all section data */
    sdata = malloc(sizeof(void *) * ehdr.e_shnum);
    memset(sdata, 0, sizeof(void *) * ehdr.e_shnum);
    
    for(i = 0;i < ehdr.e_shnum; i++) {
        sec = &shdr[i];
        if (sec->sh_type != SHT_NOBITS)
            sdata[i] = load_data(fd, sec->sh_offset, sec->sh_size);
    }

    sec = &shdr[ehdr.e_shstrndx];
    shstr = sdata[ehdr.e_shstrndx];

    /* swap relocations */
    for(i = 0; i < ehdr.e_shnum; i++) {
        sec = &shdr[i];
        if (sec->sh_type == SHT_REL || sec->sh_type == SHT_RELA) {
            nb_relocs = sec->sh_size / sec->sh_entsize;
            if (do_swap) {
                for(j = 0, rel = (ELF_RELOC *)sdata[i]; j < nb_relocs; j++, rel++)
                    elf_swap_rel(rel);
            }
        }
    }

    /* data section */
    data_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".data");
    if (data_sec) {
        data_shndx = data_sec - shdr;
        data = sdata[data_shndx];
    }
    else {
        data_shndx = -1;
        data = NULL;
    }

    /* rodata sections */
    rodata_cst4_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".rodata.cst4");
    if (rodata_cst4_sec) {
      rodata_cst4_shndx = rodata_cst4_sec - shdr;
      rodata_cst4 = sdata[rodata_cst4_shndx];
    }
    rodata_cst8_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".rodata.cst8");
    if (rodata_cst8_sec) {
      rodata_cst8_shndx = rodata_cst8_sec - shdr;
      rodata_cst8 = sdata[rodata_cst8_shndx];
    }
    rodata_cst16_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".rodata.cst16");
    if (rodata_cst16_sec) {
      rodata_cst16_shndx = rodata_cst16_sec - shdr;
      rodata_cst16 = sdata[rodata_cst16_shndx];
    }

    /* text section */
    text_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".text");
    if (!text_sec)
        error("could not find .text section");
    text_shndx = text_sec - shdr;
    text = sdata[text_shndx];

    /* find text relocations, if any */
    relocs = NULL;
    nb_relocs = 0;
    i = find_reloc(text_shndx);
    if (i != 0) {
        relocs = (ELF_RELOC *)sdata[i];
        nb_relocs = shdr[i].sh_size / shdr[i].sh_entsize;
    }
#ifdef ELF_USES_ALSO_RELOC
    i = do_find_reloc(text_shndx, SHT_REL);
    if (i != 0) {
        if (relocs) {
            int j, nb_rels = shdr[i].sh_size / shdr[i].sh_entsize;
            ElfW(Rel) *rels = (ElfW(Rel) *)sdata[i];
            ELF_RELOC *new_relocs = (ELF_RELOC *)malloc(sizeof(ELF_RELOC) * (nb_relocs + nb_rels));
            memcpy(new_relocs, relocs, sizeof(ELF_RELOC) * nb_relocs);
            for (j = 0; j < nb_rels; j++) {
                new_relocs[j + nb_relocs].r_offset = rels[j].r_offset;
                new_relocs[j + nb_relocs].r_info = rels[j].r_info;
                new_relocs[j + nb_relocs].r_addend = 0;
            }
            nb_relocs += nb_rels;
            relocs = new_relocs;
        }
    }
#endif

    symtab_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".symtab");
    if (!symtab_sec)
        error("could not find .symtab section");
    strtab_sec = &shdr[symtab_sec->sh_link];

    symtab = (ElfW(Sym) *)sdata[symtab_sec - shdr];
    strtab = sdata[symtab_sec->sh_link];
    
    nb_syms = symtab_sec->sh_size / sizeof(ElfW(Sym));
    if (do_swap) {
        for(i = 0, sym = symtab; i < nb_syms; i++, sym++) {
            swab32s(&sym->st_name);
            swabls(&sym->st_value);
            swabls(&sym->st_size);
            swab16s(&sym->st_shndx);
        }
    }
    close(fd);

    {
        int status;
        size_t nf, nd = 256;
        char *demangled_name, *func_name;
        if ((demangled_name = malloc(nd)) == NULL)
            return -1;
        if ((func_name = malloc(nf = nd)) == NULL)
            return -1;

        for(i = 0, sym = symtab; i < nb_syms; i++, sym++) {
            const char *name;
            name = get_sym_name(sym);
            /* emit local symbols */
            if (strstart(name, ".LC", NULL)) {
                const char *dot_name = gen_dot_prefix(name);
                fprintf(outfile, "DEFINE_GEN(gen_const_%s,uint8 *,(void))\n", dot_name);
                fprintf(outfile, "#ifdef DYNGEN_IMPL\n");
                fprintf(outfile, "{\n");
                int dot_size = 0;
                if (sym->st_shndx == (rodata_cst16_sec - shdr))
                    print_data(outfile, dot_name, rodata_cst16 + sym->st_value, (dot_size = 16));
                else if (sym->st_shndx == (rodata_cst8_sec - shdr))
                    print_data(outfile, dot_name, rodata_cst8 + sym->st_value,  (dot_size = 8));
                else if (sym->st_shndx == (rodata_cst4_sec - shdr))
                    print_data(outfile, dot_name, rodata_cst4 + sym->st_value,  (dot_size = 4));
                else
                    error("invalid section for local data %s (%x)\n", name, sym->st_shndx);
                fprintf(outfile, "    static uint8 *data_p = NULL;\n");
                fprintf(outfile, "    if (data_p == NULL)\n");
                fprintf(outfile, "        data_p = copy_data(%s, %d);\n", dot_name, dot_size);
                fprintf(outfile, "    return data_p;\n");
                fprintf(outfile, "}\n");
                fprintf(outfile, "#endif\n");
            }
        }
    }
    return 0;
}

#endif /* CONFIG_FORMAT_ELF */

#ifdef CONFIG_FORMAT_COFF

/* COFF file info */
struct external_scnhdr *shdr;
uint8_t **sdata;
struct external_filehdr fhdr;
struct external_syment *coff_symtab;
char *strtab;
int coff_text_shndx, coff_data_shndx;

#define STRTAB_SIZE 4

#define DIR32   0x06
#define DISP32  0x14

#define T_FUNCTION  0x20
#define C_EXTERNAL  2

void sym_ent_name(struct external_syment *ext_sym, EXE_SYM *sym)
{
    char *q;
    int c, i, len;
    
    if (ext_sym->e.e.e_zeroes != 0) {
        q = sym->st_name;
        for(i = 0; i < 8; i++) {
            c = ext_sym->e.e_name[i];
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    } else {
        pstrcpy(sym->st_name, sizeof(sym->st_name), strtab + ext_sym->e.e.e_offset);
    }

    /* now convert the name to a C name (suppress the leading '_') */
    if (sym->st_name[0] == '_') {
        len = strlen(sym->st_name);
        memmove(sym->st_name, sym->st_name + 1, len - 1);
        sym->st_name[len - 1] = '\0';
    }
}

char *name_for_dotdata(struct coff_rel *rel)
{
	int i;
	struct coff_sym *sym;
	uint32_t text_data;

	text_data = *(uint32_t *)(text + rel->r_offset);

	for (i = 0, sym = symtab; i < nb_syms; i++, sym++) {
		if (sym->st_syment->e_scnum == data_shndx &&
                    text_data >= sym->st_value &&
                    text_data < sym->st_value + sym->st_size) {
                    
                    return sym->st_name;

		}
	}
	return NULL;
}

static char *get_sym_name(EXE_SYM *sym)
{
    return sym->st_name;
}

static char *get_rel_sym_name(EXE_RELOC *rel)
{
    char *name;
    name = get_sym_name(symtab + *(uint32_t *)(rel->r_reloc->r_symndx));
    if (!strcmp(name, ".data"))
        name = name_for_dotdata(rel);
    return name;
}

static host_ulong get_rel_offset(EXE_RELOC *rel)
{
    return rel->r_offset;
}

struct external_scnhdr *find_coff_section(struct external_scnhdr *shdr, int shnum, const char *name)
{
    int i;
    const char *shname;
    struct external_scnhdr *sec;

    for(i = 0; i < shnum; i++) {
        sec = &shdr[i];
        if (!sec->s_name)
            continue;
        shname = sec->s_name;
        if (!strcmp(shname, name))
            return sec;
    }
    return NULL;
}

/* load a coff object file */
int load_object(const char *filename, FILE *outfile)
{
    int fd;
    struct external_scnhdr *sec, *text_sec, *data_sec;
    int i, j;
    struct external_syment *ext_sym;
    struct external_reloc *coff_relocs;
    struct external_reloc *ext_rel;
    uint32_t *n_strtab;
    EXE_SYM *sym;
    EXE_RELOC *rel;
	
    fd = open(filename, O_RDONLY 
#ifdef _WIN32
              | O_BINARY
#endif
              );
    if (fd < 0) 
        error("can't open file '%s'", filename);
    
    /* Read COFF header.  */
    if (read(fd, &fhdr, sizeof (fhdr)) != sizeof (fhdr))
        error("unable to read file header");

    /* Check COFF identification.  */
    if (fhdr.f_magic != I386MAGIC) {
        error("bad COFF header");
    }
    do_swap = 0;

    /* read section headers */
    shdr = load_data(fd, sizeof(struct external_filehdr) + fhdr.f_opthdr, fhdr.f_nscns * sizeof(struct external_scnhdr));
	
    /* read all section data */
    sdata = malloc(sizeof(void *) * fhdr.f_nscns);
    memset(sdata, 0, sizeof(void *) * fhdr.f_nscns);
    
    const char *p;
    for(i = 0;i < fhdr.f_nscns; i++) {
        sec = &shdr[i];
        if (!strstart(sec->s_name,  ".bss", &p))
            sdata[i] = load_data(fd, sec->s_scnptr, sec->s_size);
    }


    /* text section */
    text_sec = find_coff_section(shdr, fhdr.f_nscns, ".text");
    if (!text_sec)
        error("could not find .text section");
    coff_text_shndx = text_sec - shdr;
    text = sdata[coff_text_shndx];

    /* data section */
    data_sec = find_coff_section(shdr, fhdr.f_nscns, ".data");
    if (!data_sec)
        error("could not find .data section");
    coff_data_shndx = data_sec - shdr;
    data = sdata[coff_data_shndx];
    
    coff_symtab = load_data(fd, fhdr.f_symptr, fhdr.f_nsyms*SYMESZ);
    for (i = 0, ext_sym = coff_symtab; i < nb_syms; i++, ext_sym++) {
        for(j=0;j<8;j++)
            printf(" %02x", ((uint8_t *)ext_sym->e.e_name)[j]);
        printf("\n");
    }

    nb_syms = fhdr.f_nsyms;
    n_strtab = load_data(fd, (fhdr.f_symptr + fhdr.f_nsyms*SYMESZ), STRTAB_SIZE);
    strtab = load_data(fd, (fhdr.f_symptr + fhdr.f_nsyms*SYMESZ), *n_strtab); 
    
    for (i = 0, ext_sym = coff_symtab; i < nb_syms; i++, ext_sym++) {
      if (strstart(ext_sym->e.e_name, ".text", NULL))
		  text_shndx = ext_sym->e_scnum;
	  if (strstart(ext_sym->e.e_name, ".data", NULL))
		  data_shndx = ext_sym->e_scnum;
    }

	/* set coff symbol */
	symtab = malloc(sizeof(struct coff_sym) * nb_syms);

	int aux_size;
	for (i = 0, ext_sym = coff_symtab, sym = symtab; i < nb_syms; i++, ext_sym++, sym++) {
		memset(sym, 0, sizeof(*sym));
		sym->st_syment = ext_sym;
		sym_ent_name(ext_sym, sym);
		sym->st_value = ext_sym->e_value;

		aux_size = *(int8_t *)ext_sym->e_numaux;
		if (ext_sym->e_scnum == text_shndx && ext_sym->e_type == T_FUNCTION) {
			for (j = aux_size + 1; j < nb_syms - i; j++) {
				if ((ext_sym + j)->e_scnum == text_shndx &&
					(ext_sym + j)->e_type == T_FUNCTION ){
					sym->st_size = (ext_sym + j)->e_value - ext_sym->e_value;
					break;
				} else if (j == nb_syms - i - 1) {
					sec = &shdr[coff_text_shndx];
					sym->st_size = sec->s_size - ext_sym->e_value;
					break;
				}
			}
		} else if (ext_sym->e_scnum == data_shndx && *(uint8_t *)ext_sym->e_sclass == C_EXTERNAL) {
			for (j = aux_size + 1; j < nb_syms - i; j++) {
				if ((ext_sym + j)->e_scnum == data_shndx) {
					sym->st_size = (ext_sym + j)->e_value - ext_sym->e_value;
					break;
				} else if (j == nb_syms - i - 1) {
					sec = &shdr[coff_data_shndx];
					sym->st_size = sec->s_size - ext_sym->e_value;
					break;
				}
			}
		} else {
			sym->st_size = 0;
		}
		
		sym->st_type = ext_sym->e_type;
		sym->st_shndx = ext_sym->e_scnum;
	}

		
    /* find text relocations, if any */
    sec = &shdr[coff_text_shndx];
    coff_relocs = load_data(fd, sec->s_relptr, sec->s_nreloc*RELSZ);
    nb_relocs = sec->s_nreloc;

    /* set coff relocation */
    relocs = malloc(sizeof(struct coff_rel) * nb_relocs);
    for (i = 0, ext_rel = coff_relocs, rel = relocs; i < nb_relocs; 
         i++, ext_rel++, rel++) {
        memset(rel, 0, sizeof(*rel));
        rel->r_reloc = ext_rel;
        rel->r_offset = *(uint32_t *)ext_rel->r_vaddr;
        rel->r_type = *(uint16_t *)ext_rel->r_type;
    }
    return 0;
}

#endif /* CONFIG_FORMAT_COFF */

#ifdef CONFIG_FORMAT_MACH

/* File Header */
struct mach_header 	mach_hdr;

/* commands */
struct segment_command 	*segment = 0;
struct dysymtab_command *dysymtabcmd = 0;
struct symtab_command 	*symtabcmd = 0;

/* section */
struct section 	*section_hdr;
struct section *text_sec_hdr;
struct section *data_sec_hdr;
uint8_t 	**sdata;

/* relocs */
struct relocation_info *relocs;
	
/* symbols */
EXE_SYM			*symtab;
struct nlist 	*symtab_std;
char			*strtab;

/* indirect symbols */
uint32_t 	*tocdylib;

/* Utility functions */

static inline char *find_str_by_index(int index)
{
    return strtab+index;
}

/* Used by dyngen common code */
static char *get_sym_name(EXE_SYM *sym)
{
	char *name = find_str_by_index(sym->n_un.n_strx);
	
	if ( sym->n_type & N_STAB ) /* Debug symbols are ignored */
		return "debug";
			
	if(!name)
		return name;
	if(name[0]=='_')
		return name + 1;
	else
		return name;
}

/* find a section index given its segname, sectname */
static int find_mach_sec_index(struct section *section_hdr, int shnum, const char *segname, 
                                  const char *sectname)
{
    int i;
    struct section *sec = section_hdr;

    for(i = 0; i < shnum; i++, sec++) {
        if (!sec->segname || !sec->sectname)
            continue;
        if (!strcmp(sec->sectname, sectname) && !strcmp(sec->segname, segname))
            return i;
    }
    return -1;
}

/* find a section header given its segname, sectname */
struct section *find_mach_sec_hdr(struct section *section_hdr, int shnum, const char *segname, 
                                  const char *sectname)
{
    int index = find_mach_sec_index(section_hdr, shnum, segname, sectname);
	if(index == -1)
		return NULL;
	return section_hdr+index;
}


static inline void fetch_next_pair_value(struct relocation_info * rel, unsigned int *value)
{
    struct scattered_relocation_info * scarel;
	
    if(R_SCATTERED & rel->r_address) {
        scarel = (struct scattered_relocation_info*)rel;
        if(scarel->r_type != PPC_RELOC_PAIR)
            error("fetch_next_pair_value: looking for a pair which was not found (1)");
        *value = scarel->r_value;
    } else {
		if(rel->r_type != PPC_RELOC_PAIR)
			error("fetch_next_pair_value: looking for a pair which was not found (2)");
		*value = rel->r_address;
	}
}

/* find a sym name given its value, in a section number */
static const char * find_sym_with_value_and_sec_number( int value, int sectnum, int * offset )
{
	int i, ret = -1;
	
	for( i = 0 ; i < nb_syms; i++ )
	{
	    if( !(symtab[i].n_type & N_STAB) && (symtab[i].n_type & N_SECT) &&
			 (symtab[i].n_sect ==  sectnum) && (symtab[i].st_value <= value) )
		{
			if( (ret<0) || (symtab[i].st_value >= symtab[ret].st_value) )
				ret = i;
		}
	}
	if( ret < 0 ) {
		*offset = 0;
		return 0;
	} else {
		*offset = value - symtab[ret].st_value;
		return get_sym_name(&symtab[ret]);
	}
}

/* 
 *  Find symbol name given a (virtual) address, and a section which is of type 
 *  S_NON_LAZY_SYMBOL_POINTERS or S_LAZY_SYMBOL_POINTERS or S_SYMBOL_STUBS
 */
static const char * find_reloc_name_in_sec_ptr(int address, struct section * sec_hdr)
{
    unsigned int tocindex, symindex, size;
    const char *name = 0;
    
    /* Sanity check */
    if(!( address >= sec_hdr->addr && address < (sec_hdr->addr + sec_hdr->size) ) )
        return (char*)0;
		
	if( sec_hdr->flags & S_SYMBOL_STUBS ){
		size = sec_hdr->reserved2;
		if(size == 0)
		    error("size = 0");
		
	}
	else if( sec_hdr->flags & S_LAZY_SYMBOL_POINTERS ||
	            sec_hdr->flags & S_NON_LAZY_SYMBOL_POINTERS)
		size = sizeof(unsigned long);
	else
		return 0;
		
    /* Compute our index in toc */
	tocindex = (address - sec_hdr->addr)/size;
	symindex = tocdylib[sec_hdr->reserved1 + tocindex];
	
	name = get_sym_name(&symtab[symindex]);

    return name;
}

static const char * find_reloc_name_given_its_address(int address)
{
    unsigned int i;
    for(i = 0; i < segment->nsects ; i++)
    {
        const char * name = find_reloc_name_in_sec_ptr(address, &section_hdr[i]);
        if((long)name != -1)
            return name;
    }
    return 0;
}

static const char * get_reloc_name(EXE_RELOC * rel, int * sslide)
{
	char * name = 0;
	struct scattered_relocation_info * sca_rel = (struct scattered_relocation_info*)rel;
	int sectnum = rel->r_symbolnum;
	int sectoffset;
	int other_half=0;
	
	/* init the slide value */
	*sslide = 0;
	
	if (R_SCATTERED & rel->r_address) {
        char *name = (char *)find_reloc_name_given_its_address(sca_rel->r_value);

        /* search it in the full symbol list, if not found */
        if (!name) {
            int i;
            for (i = 0; i < nb_syms; i++) {
                EXE_SYM *sym = &symtab[i];
                if (sym->st_value == sca_rel->r_value) {
                    name = get_sym_name(sym);
                    switch (sca_rel->r_type) {
                    case GENERIC_RELOC_VANILLA:
                        *sslide = *(uint32_t *)(text + sca_rel->r_address) - sca_rel->r_value;
                        break;
                    }
                    break;
                }
            }
        }
        return name;
    }

	if(rel->r_extern)
	{
		/* ignore debug sym */
		if ( symtab[rel->r_symbolnum].n_type & N_STAB ) 
			return 0;
		return get_sym_name(&symtab[rel->r_symbolnum]);
	}

	/* Intruction contains an offset to the symbols pointed to, in the rel->r_symbolnum section */
	sectoffset = *(uint32_t *)(text + rel->r_address) & 0xffff;
			
	if(sectnum==0xffffff)
		return 0;

	/* Sanity Check */
	if(sectnum > segment->nsects)
		error("sectnum > segment->nsects");

	switch(rel->r_type)
	{
		case PPC_RELOC_LO16: fetch_next_pair_value(rel+1, &other_half); sectoffset |= (other_half << 16);
			break;
		case PPC_RELOC_HI16: fetch_next_pair_value(rel+1, &other_half); sectoffset = (sectoffset << 16) | (uint16_t)(other_half & 0xffff);
			break;
		case PPC_RELOC_HA16: fetch_next_pair_value(rel+1, &other_half); sectoffset = (sectoffset << 16) + (int16_t)(other_half & 0xffff);
			break;
		case PPC_RELOC_BR24:
			sectoffset = ( *(uint32_t *)(text + rel->r_address) & 0x03fffffc );
			if (sectoffset & 0x02000000) sectoffset |= 0xfc000000;
			break;
        case GENERIC_RELOC_VANILLA:
            sectoffset  = *(uint32_t *)(text + rel->r_address);
            break;
		default:
			error("switch(rel->type=%d) not found", rel->r_type);
	}

	if(rel->r_pcrel) {
		sectoffset += rel->r_address;
#ifdef HOST_I386
        sectoffset += (1 << rel->r_length);
#endif
    }

	if (rel->r_type == PPC_RELOC_BR24)
		name = (char *)find_reloc_name_in_sec_ptr((int)sectoffset, &section_hdr[sectnum-1]);

	/* search it in the full symbol list, if not found */
	if(!name)
		name = (char *)find_sym_with_value_and_sec_number(sectoffset, sectnum, sslide);
	
	return name;
}

/* Used by dyngen common code */
static const char * get_rel_sym_name(EXE_RELOC * rel)
{
	int sslide;
	return get_reloc_name( rel, &sslide);
}

/* Used by dyngen common code */
static host_ulong get_rel_offset(EXE_RELOC *rel)
{
	struct scattered_relocation_info * sca_rel = (struct scattered_relocation_info*)rel;
    if(R_SCATTERED & rel->r_address)
		return sca_rel->r_address;
	else
		return rel->r_address;
}

/* load a mach-o object file */
int load_object(const char *filename, FILE *outfile)
{
	int fd;
	unsigned int offset_to_segment = 0;
    unsigned int offset_to_dysymtab = 0;
    unsigned int offset_to_symtab = 0;
    struct load_command lc;
    unsigned int i, j;
	EXE_SYM *sym;
	struct nlist *syment;
    
	fd = open(filename, O_RDONLY);
    if (fd < 0) 
        error("can't open file '%s'", filename);
		
    /* Read Mach header.  */
    if (read(fd, &mach_hdr, sizeof (mach_hdr)) != sizeof (mach_hdr))
        error("unable to read file header");

    /* Check Mach identification.  */
    if (!check_mach_header(mach_hdr)) {
        error("bad Mach header");
    }
    
    if (!mach_check_cputype(mach_hdr.cputype))
        error("Unsupported CPU");
        
    if (mach_hdr.filetype != MH_OBJECT)
        error("Unsupported Mach Object");
    
    /* read segment headers */
    for(i=0, j=sizeof(mach_hdr); i<mach_hdr.ncmds ; i++)
    {
        if(read(fd, &lc, sizeof(struct load_command)) != sizeof(struct load_command))
            error("unable to read load_command");
        if(lc.cmd == LC_SEGMENT)
        {
            offset_to_segment = j;
            lseek(fd, offset_to_segment, SEEK_SET);
            segment = malloc(sizeof(struct segment_command));
            if(read(fd, segment, sizeof(struct segment_command)) != sizeof(struct segment_command))
                error("unable to read LC_SEGMENT");
        }
        if(lc.cmd == LC_DYSYMTAB)
        {
            offset_to_dysymtab = j;
            lseek(fd, offset_to_dysymtab, SEEK_SET);
            dysymtabcmd = malloc(sizeof(struct dysymtab_command));
            if(read(fd, dysymtabcmd, sizeof(struct dysymtab_command)) != sizeof(struct dysymtab_command))
                error("unable to read LC_DYSYMTAB");
        }
        if(lc.cmd == LC_SYMTAB)
        {
            offset_to_symtab = j;
            lseek(fd, offset_to_symtab, SEEK_SET);
            symtabcmd = malloc(sizeof(struct symtab_command));
            if(read(fd, symtabcmd, sizeof(struct symtab_command)) != sizeof(struct symtab_command))
                error("unable to read LC_SYMTAB");
        }
        j+=lc.cmdsize;

        lseek(fd, j, SEEK_SET);
    }

    if(!segment)
        error("unable to find LC_SEGMENT");

    /* read section headers */
    section_hdr = load_data(fd, offset_to_segment + sizeof(struct segment_command), segment->nsects * sizeof(struct section));

    /* read all section data */
    sdata = (uint8_t **)malloc(sizeof(void *) * segment->nsects);
    memset(sdata, 0, sizeof(void *) * segment->nsects);
    
    /* Load the data in section data */
    for(i = 0; i < segment->nsects; i++)
        sdata[i] = load_data(fd, section_hdr[i].offset, section_hdr[i].size);

    /* data section */
    data_sec_hdr = find_mach_sec_hdr(section_hdr, segment->nsects, SEG_DATA, SECT_DATA);
    i = find_mach_sec_index(section_hdr, segment->nsects, SEG_DATA, SECT_DATA);
    if (i == -1 || !data_sec_hdr)
        data = NULL;
    else
        data = sdata[i];
	
    /* text section */
	text_sec_hdr = find_mach_sec_hdr(section_hdr, segment->nsects, SEG_TEXT, SECT_TEXT);
	i = find_mach_sec_index(section_hdr, segment->nsects, SEG_TEXT, SECT_TEXT);
	if (i == -1 || !text_sec_hdr)
        error("could not find __TEXT,__text section");
    text = sdata[i];
	
    /* Make sure dysym was loaded */
    if(!(int)dysymtabcmd)
        error("could not find __DYSYMTAB segment");
    
    /* read the table of content of the indirect sym */
    tocdylib = load_data( fd, dysymtabcmd->indirectsymoff, dysymtabcmd->nindirectsyms * sizeof(uint32_t) );
    
    /* Make sure symtab was loaded  */
    if(!(int)symtabcmd)
        error("could not find __SYMTAB segment");
    nb_syms = symtabcmd->nsyms;

    symtab_std = load_data(fd, symtabcmd->symoff, symtabcmd->nsyms * sizeof(struct nlist));
    strtab = load_data(fd, symtabcmd->stroff, symtabcmd->strsize);
	
	symtab = malloc(sizeof(EXE_SYM) * nb_syms);
	
	/* Now transform the symtab, to an extended version, with the sym size, and the C name */
	for(i = 0, sym = symtab, syment = symtab_std; i < nb_syms; i++, sym++, syment++) {
        struct nlist *sym_follow, *sym_next = 0;
        unsigned int j;
		memset(sym, 0, sizeof(*sym));
		
		if ( syment->n_type & N_STAB ) /* Debug symbols are skipped */
            continue;
			
		memcpy(sym, syment, sizeof(*syment));
			
		/* Find the following symbol in order to get the current symbol size */
        for(j = 0, sym_follow = symtab_std; j < nb_syms; j++, sym_follow++) {
            if ( sym_follow->n_sect != 1 || sym_follow->n_type & N_STAB || !(sym_follow->n_value > sym->st_value))
                continue;
            if(!sym_next) {
                sym_next = sym_follow;
                continue;
            }
            if(!(sym_next->n_value > sym_follow->n_value))
                continue;
            sym_next = sym_follow;
        }
		if(sym_next)
            sym->st_size = sym_next->n_value - sym->st_value;
		else
            sym->st_size = text_sec_hdr->size - sym->st_value;
	}
	
    /* Find Reloc */
    relocs = load_data(fd, text_sec_hdr->reloff, text_sec_hdr->nreloc * sizeof(struct relocation_info));
    nb_relocs = text_sec_hdr->nreloc;

	close(fd);
	return 0;
}

#endif /* CONFIG_FORMAT_MACH */

#ifdef HOST_ARM

int arm_emit_ldr_info(const char *name, unsigned long start_offset,
                      FILE *outfile, uint8_t *p_start, uint8_t *p_end,
                      ELF_RELOC *relocs, int nb_relocs)
{
    uint8_t *p;
    uint32_t insn;
    int offset, min_offset, pc_offset, data_size;
    uint8_t data_allocated[1024];
    unsigned int data_index;
    
    memset(data_allocated, 0, sizeof(data_allocated));
    
    p = p_start;
    min_offset = p_end - p_start;
    while (p < p_start + min_offset) {
        insn = get32((uint32_t *)p);
        if ((insn & 0x0d5f0000) == 0x051f0000) {
            /* ldr reg, [pc, #im] */
            offset = insn & 0xfff;
            if (!(insn & 0x00800000))
                        offset = -offset;
            if ((offset & 3) !=0)
                error("%s:%04x: ldr pc offset must be 32 bit aligned", 
                      name, start_offset + p - p_start);
            pc_offset = p - p_start + offset + 8;
            if (pc_offset <= (p - p_start) || 
                pc_offset >= (p_end - p_start))
                error("%s:%04x: ldr pc offset must point inside the function code", 
                      name, start_offset + p - p_start);
            if (pc_offset < min_offset)
                min_offset = pc_offset;
            if (outfile) {
                /* ldr position */
                fprintf(outfile, "    arm_ldr_ptr->ptr = ptr() + %d;\n", 
                        p - p_start);
                /* ldr data index */
                data_index = ((p_end - p_start) - pc_offset - 4) >> 2;
                fprintf(outfile, "    arm_ldr_ptr->data_ptr = arm_data_ptr + %d;\n", 
                        data_index);
                fprintf(outfile, "    arm_ldr_ptr++;\n");
                if (data_index >= sizeof(data_allocated))
                    error("%s: too many data", name);
                if (!data_allocated[data_index]) {
                    ELF_RELOC *rel;
                    int i, addend, type;
                    const char *sym_name, *p;
                    char relname[1024];

                    data_allocated[data_index] = 1;

                    /* data value */
                    addend = get32((uint32_t *)(p_start + pc_offset));
                    relname[0] = '\0';
                    for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                        if (rel->r_offset == (pc_offset + start_offset)) {
                            sym_name = get_rel_sym_name(rel);
                            /* the compiler leave some unnecessary references to the code */
                            if (strstart(sym_name, "__op_PARAM", &p)) {
                                snprintf(relname, sizeof(relname), "param%s", p);
                            } else {
                                snprintf(relname, sizeof(relname), "(long)(&%s)", sym_name);
                            }
                            type = ELF32_R_TYPE(rel->r_info);
                            if (type != R_ARM_ABS32)
                                error("%s: unsupported data relocation", name);
                            break;
                        }
                    }
                    fprintf(outfile, "    arm_data_ptr[%d] = 0x%x",
                            data_index, addend);
                    if (relname[0] != '\0')
                        fprintf(outfile, " + %s", relname);
                    fprintf(outfile, ";\n");
                }
            }
        }
        p += 4;
    }
    data_size = (p_end - p_start) - min_offset;
    if (data_size > 0 && outfile) {
        fprintf(outfile, "    arm_data_ptr += %d;\n", data_size >> 2);
    }

    /* the last instruction must be a mov pc, lr */
    if (p == p_start)
        goto arm_ret_error;
    p -= 4;
    insn = get32((uint32_t *)p);
    if ((insn & 0xffff0000) != 0xe91b0000) {
    arm_ret_error:
        if (!outfile)
            printf("%s: invalid epilog\n", name);
    }
    return p - p_start;	    
}
#endif


#define MAX_ARGS 3

/* generate op code */
void gen_code(const char *name, const char *demangled_name,
              host_ulong offset, host_ulong size, 
              FILE *outfile, int gen_switch, const char *prefix)
{
    int copy_size = 0;
    uint8_t *p_start, *p_end;
    host_ulong start_offset;
    int nb_args, i, n;
    uint8_t args_present[MAX_ARGS];
    const char *sym_name, *p;
    EXE_RELOC *rel;
    int op_execute = 0;

    if (strncmp(name, "op_execute", 10) == 0)
      op_execute = 1;

    /* Compute exact size excluding prologue and epilogue instructions.
     * Increment start_offset to skip epilogue instructions, then compute
     * copy_size the indicate the size of the remaining instructions (in
     * bytes).
     */
    p_start = text + offset;
    p_end = p_start + size;
    start_offset = offset;
    if (op_execute) {
        uint8_t *p;
        copy_size = p_end - p_start;
#ifdef CONFIG_FORMAT_MACH
#if defined(HOST_PPC)
        for (p = p_start; p < p_end; p += 4) {
            if (get32((uint32_t *)p) == 0x18deadff)
                fprintf(outfile, "DEFINE_CST(op_exec_return_offset,0x%xL)\n\n", (p + 4) - p_start);
        }
#endif
#if defined(HOST_I386)
        static const uint8_t return_insn[] = {0x0f,0xa6,0xf0};
        for (p = p_start; p < p_end; p++) {
            if (memcmp(p, return_insn, sizeof(return_insn)) == 0)
                fprintf(outfile, "DEFINE_CST(op_exec_return_offset,0x%xL)\n\n", (p + sizeof(return_insn)) - p_start);
        }
#endif
#endif
    }
    else
#if defined(HOST_I386) || defined(HOST_AMD64)
#if defined(CONFIG_FORMAT_COFF) || defined(CONFIG_FORMAT_MACH)
    {
        uint8_t *p;
        p = p_end - 1;
        if (p == p_start)
            error("empty code for %s", name);
        while (*p != 0xc3) {
            p--;
            if (p <= p_start)
                error("ret or jmp expected at the end of %s", name);
        }
        copy_size = p - p_start;
    }
#else
    {
        int len;
        len = p_end - p_start;
        if (len == 0)
            error("empty code for %s", name);
        if (p_end[-1] == 0xc3) {
            len--;
        } else {
            error("ret or jmp expected at the end of %s", name);
        }
        copy_size = len;
    }
#endif    
#elif defined(HOST_PPC)
    {
        uint8_t *p;
        p = (void *)(p_end - 4);
        if (p == p_start)
            error("empty code for %s", name);
        if (get32((uint32_t *)p) != 0x4e800020)
            error("blr expected at the end of %s", name);
        copy_size = p - p_start;
    }
#elif defined(HOST_S390)
    {
        uint8_t *p;
        p = (void *)(p_end - 2);
        if (p == p_start)
            error("empty code for %s", name);
        if (get16((uint16_t *)p) != 0x07fe && get16((uint16_t *)p) != 0x07f4)
            error("br %%r14 expected at the end of %s", name);
        copy_size = p - p_start;
    }
#elif defined(HOST_ALPHA)
    {
        uint8_t *p;
        p = p_end - 4;
#if 0
        /* XXX: check why it occurs */
        if (p == p_start)
            error("empty code for %s", name);
#endif
        if (get32((uint32_t *)p) != 0x6bfa8001)
            error("ret expected at the end of %s", name);
        copy_size = p - p_start;	    
    }
#elif defined(HOST_IA64)
    {
        uint8_t *p;
        p = (void *)(p_end - 4);
        if (p == p_start)
            error("empty code for %s", name);
        /* br.ret.sptk.many b0;; */
        /* 08 00 84 00 */
        if (get32((uint32_t *)p) != 0x00840008)
            error("br.ret.sptk.many b0;; expected at the end of %s", name);
        copy_size = p - p_start;
    }
#elif defined(HOST_SPARC)
    {
        uint32_t start_insn, end_insn1, end_insn2;
        uint8_t *p;
        p = (void *)(p_end - 8);
        if (p <= p_start)
            error("empty code for %s", name);
        start_insn = get32((uint32_t *)(p_start + 0x0));
        end_insn1 = get32((uint32_t *)(p + 0x0));
        end_insn2 = get32((uint32_t *)(p + 0x4));
        if ((start_insn & ~0x1fff) == 0x9de3a000) {
            p_start += 0x4;
            start_offset += 0x4;
            if ((int)(start_insn | ~0x1fff) < -128)
                error("Found bogus save at the start of %s", name);
            if (end_insn1 != 0x81c7e008 || end_insn2 != 0x81e80000)
                error("ret; restore; not found at end of %s", name);
        } else {
            error("No save at the beginning of %s", name);
        }
#if 0
        /* Skip a preceeding nop, if present.  */
        if (p > p_start) {
            skip_insn = get32((uint32_t *)(p - 0x4));
            if (skip_insn == 0x01000000)
                p -= 4;
        }
#endif
        copy_size = p - p_start;
    }
#elif defined(HOST_SPARC64)
    {
        uint32_t start_insn, end_insn1, end_insn2, skip_insn;
        uint8_t *p;
        p = (void *)(p_end - 8);
        if (p <= p_start)
            error("empty code for %s", name);
        start_insn = get32((uint32_t *)(p_start + 0x0));
        end_insn1 = get32((uint32_t *)(p + 0x0));
        end_insn2 = get32((uint32_t *)(p + 0x4));
        if ((start_insn & ~0x1fff) == 0x9de3a000) {
            p_start += 0x4;
            start_offset += 0x4;
            if ((int)(start_insn | ~0x1fff) < -256)
                error("Found bogus save at the start of %s", name);
            if (end_insn1 != 0x81c7e008 || end_insn2 != 0x81e80000)
                error("ret; restore; not found at end of %s", name);
        } else {
            error("No save at the beginning of %s", name);
        }
        
        /* Skip a preceeding nop, if present.  */
        if (p > p_start) {
            skip_insn = get32((uint32_t *)(p - 0x4));
            if (skip_insn == 0x01000000)
                p -= 4;
        }
        
        copy_size = p - p_start;
    }
#elif defined(HOST_ARM)
    {
        if ((p_end - p_start) <= 16)
            error("%s: function too small", name);
        if (get32((uint32_t *)p_start) != 0xe1a0c00d ||
            (get32((uint32_t *)(p_start + 4)) & 0xffff0000) != 0xe92d0000 ||
            get32((uint32_t *)(p_start + 8)) != 0xe24cb004)
            error("%s: invalid prolog", name);
        p_start += 12;
        start_offset += 12;
        copy_size = arm_emit_ldr_info(name, start_offset, NULL, p_start, p_end, 
                                      relocs, nb_relocs);
    }
#elif defined(HOST_M68K)
    {
        uint8_t *p;
        p = (void *)(p_end - 2);
        if (p == p_start)
            error("empty code for %s", name);
        // remove NOP's, probably added for alignment
        while ((get16((uint16_t *)p) == 0x4e71) &&
               (p>p_start)) 
            p -= 2;
        if (get16((uint16_t *)p) != 0x4e75)
            error("rts expected at the end of %s", name);
        copy_size = p - p_start;
    }
#elif defined(HOST_MIPS)
    {
        uint8_t *p;
        p = (void *)(p_end - 4);
        if (p == p_start)
            error("empty code for %s", name);
        while (p > p_start && get32((uint32_t *)p) != 0x03e00008)
            p -= 4;
        if (get32((uint32_t *)p) != 0x03e00008)
            error("jr ra expected at the end of %s", name);
        copy_size = p - p_start;
    }
#else
#error unsupported CPU
#endif

    /* compute the number of arguments by looking at the relocations */
    for(i = 0;i < MAX_ARGS; i++)
        args_present[i] = 0;

    for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
        host_ulong offset = get_rel_offset(rel);
        if (offset >= start_offset &&
	    offset < start_offset + (p_end - p_start)) {
            sym_name = get_rel_sym_name(rel);
            if(!sym_name)
                continue;
            if (strstart(sym_name, "__op_PARAM", &p)) {
                n = strtoul(p, NULL, 10);
                if (n > MAX_ARGS)
                    error("too many arguments in %s", name);
                args_present[n - 1] = 1;
            }
        }
    }
    
    nb_args = 0;
    while (nb_args < MAX_ARGS && args_present[nb_args])
        nb_args++;
    for(i = nb_args; i < MAX_ARGS; i++) {
        if (args_present[i])
            error("inconsistent argument numbering in %s", name);
    }

	assert(gen_switch == 3);
	if (gen_switch == 3) {
        const char *func_name = name;
        if (prefix && strstr(func_name, prefix) == func_name)
          func_name += strlen(prefix);

        fprintf(outfile, "DEFINE_GEN(gen_%s,void,(", func_name);
        if (nb_args == 0) {
            fprintf(outfile, "void");
        } else {
            for(i = 0; i < nb_args; i++) {
                if (i != 0)
                    fprintf(outfile, ", ");
                fprintf(outfile, "long param%d", i + 1);
            }
        }
        fprintf(outfile, "))\n");
        fprintf(outfile, "#ifdef DYNGEN_IMPL\n");
        fprintf(outfile, "#define HAVE_gen_%s\n", func_name);
        fprintf(outfile, "{\n");
        print_code(outfile, name, p_start + start_offset - offset, copy_size);

        for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
            host_ulong offset = get_rel_offset(rel);
            if (offset >= start_offset &&
                offset < start_offset + (p_end - p_start)) {
                sym_name = get_rel_sym_name(rel);
                if(!sym_name)
                    continue;
                if (*sym_name && 
                    !strstart(sym_name, "__op_PARAM", NULL) &&
                    !strstart(sym_name, "__op_jmp", NULL) &&
                    !strstart(sym_name, ".LC", NULL))
                  error("unexpected external symbol %s", sym_name);
            }
        }

        fprintf(outfile, "    copy_block(%s_code, %d);\n", name, copy_size);

        /* emit code offset information */
        {
            EXE_SYM *sym;
            const char *sym_name, *p;
            unsigned long val;
            int n;

            for(i = 0, sym = symtab; i < nb_syms; i++, sym++) {
                sym_name = get_sym_name(sym);
                if (strstart(sym_name, "__op_label", &p)) {
                    uint8_t *ptr;
                    unsigned long offset;
                    
                    /* test if the variable refers to a label inside
                       the code we are generating */
#ifdef CONFIG_FORMAT_COFF
                    if (sym->st_shndx == text_shndx) {
                        ptr = sdata[coff_text_shndx];
                    } else if (sym->st_shndx == data_shndx) {
                        ptr = sdata[coff_data_shndx];
                    } else {
                        ptr = NULL;
                    }
#elif defined(CONFIG_FORMAT_MACH)
                    if(!sym->n_sect)
                        continue;
                    ptr = sdata[sym->n_sect-1];
#else
                    ptr = sdata[sym->st_shndx];
#endif
                    if (!ptr)
                        error("__op_labelN in invalid section");
                    offset = sym->st_value;
#ifdef CONFIG_FORMAT_MACH
                    offset -= section_hdr[sym->n_sect-1].addr;
#endif
                    val = *(unsigned long *)(ptr + offset);
#ifdef ELF_USES_RELOCA
                    {
                        int reloc_shndx, nb_relocs1, j;

                        /* try to find a matching relocation */
                        reloc_shndx = find_reloc(sym->st_shndx);
                        if (reloc_shndx) {
                            nb_relocs1 = shdr[reloc_shndx].sh_size / 
                                shdr[reloc_shndx].sh_entsize;
                            rel = (ELF_RELOC *)sdata[reloc_shndx];
                            for(j = 0; j < nb_relocs1; j++) {
                                if (rel->r_offset == offset) {
				    val = rel->r_addend;
                                    break;
                                }
				rel++;
                            }
                        }
                    }
#endif                    

                    if (val >= start_offset && val < start_offset + copy_size) {
                        n = strtol(p, NULL, 10);
                        fprintf(outfile, "    label_offsets[%d] = %d + (code_ptr() - gen_code_buf);\n", n, val - start_offset);
                    }
                }
            }
        }

        /* patch relocations */
#if defined(HOST_I386)
            {
#ifdef CONFIG_FORMAT_MACH
                struct scattered_relocation_info *scarel;
                struct relocation_info * rel;
				char final_sym_name[256];
				const char *sym_name;
				const char *p;
				int slide, sslide;
				int i;
	
				for (i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
					unsigned int offset, length, value = 0;
					unsigned int type, pcrel, isym = 0;
					unsigned int usesym = 0;
				
					if (R_SCATTERED & rel->r_address) {
						scarel = (struct scattered_relocation_info*)rel;
						offset = (unsigned int)scarel->r_address;
						length = scarel->r_length;
						pcrel = scarel->r_pcrel;
						type = scarel->r_type;
						value = scarel->r_value;
					}
                    else {
						value = isym = rel->r_symbolnum;
						usesym = (rel->r_extern);
						offset = rel->r_address;
						length = rel->r_length;
						pcrel = rel->r_pcrel;
						type = rel->r_type;
					}
				
					slide = offset - start_offset;
		
					if (!(offset >= start_offset && offset < start_offset + size)) 
						continue;  /* not in our range */

					sym_name = get_reloc_name(rel, &sslide);
					
					if (usesym && symtab[isym].n_type & N_STAB)
						continue; /* don't handle STAB (debug sym) */
					
					if (sym_name && strstart(sym_name, "__op_jmp", &p)) {
						int n;
						n = strtol(p, NULL, 10);
						fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n", n, slide);
						continue; /* Nothing more to do */
					}
					
					if (!sym_name) {
						fprintf(outfile, "/* #warning relocation not handled in %s (value 0x%x, %s, offset 0x%x, length 0x%x, %s, type 0x%x) */\n",
                                name, value, usesym ? "use sym" : "don't use sym", offset, length, pcrel ? "pcrel":"", type);
						continue; /* dunno how to handle without final_sym_name */
					}
													   
					if (strstart(sym_name, "__op_PARAM", &p))
						snprintf(final_sym_name, sizeof(final_sym_name), "param%s", p);
                    else
						snprintf(final_sym_name, sizeof(final_sym_name), "(long)(&%s)", sym_name);

                    if (length != 2)
                        error("unsupported %d-bit relocation", 8 * (1 << length));

					switch (type) {
					case GENERIC_RELOC_VANILLA:
                        if (pcrel) {
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s - (long)(code_ptr() + %d) - 4;\n",
                                    slide, final_sym_name, slide);
                        }
                        else {
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (%s + %d);\n", 
                                    slide, final_sym_name, sslide);
                        }
                        break;
                    default:
                        error("unsupported i386 relocation (%d)", type);
                    }
                }
#else
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                if (rel->r_offset >= start_offset &&
		    rel->r_offset < start_offset + copy_size) {
                    sym_name = get_rel_sym_name(rel);
                    if (strstart(sym_name, "__op_jmp", &p)) {
                        int n;
                        n = strtol(p, NULL, 10);
                        /* __op_jmp relocations are done at
                           runtime to do translated block
                           chaining: the offset of the instruction
                           needs to be stored */
                        fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n",
                                n, rel->r_offset - start_offset);
                        continue;
                    }
                        
                    if (strstart(sym_name, "__op_PARAM", &p)) {
                        snprintf(name, sizeof(name), "param%s", p);
                    } else {
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                    }
                    addend = get32((uint32_t *)(text + rel->r_offset));
#ifdef CONFIG_FORMAT_ELF
                    type = ELF32_R_TYPE(rel->r_info);
                    switch(type) {
                    case R_386_32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_386_PC32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s - (long)(code_ptr() + %d) + %d;\n", 
                                rel->r_offset - start_offset, name, rel->r_offset - start_offset, addend);
                        break;
                    default:
                        error("unsupported i386 relocation (%d)", type);
                    }
#elif defined(CONFIG_FORMAT_COFF)
                    {
                        char *temp_name;
                        int j;
                        EXE_SYM *sym;
                        temp_name = get_sym_name(symtab + *(uint32_t *)(rel->r_reloc->r_symndx));
                        if (!strcmp(temp_name, ".data")) {
                            for (j = 0, sym = symtab; j < nb_syms; j++, sym++) {
                                if (strstart(sym->st_name, sym_name, NULL)) {
                                    addend -= sym->st_value;
                                }
                            }
                        }
                    }
                    type = rel->r_type;
                    switch(type) {
                    case DIR32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case DISP32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s - (long)(code_ptr() + %d) + %d -4;\n", 
                                rel->r_offset - start_offset, name, rel->r_offset - start_offset, addend);
                        break;
                    default:
                        error("unsupported i386 relocation (%d)", type);
                    }
#else
#error unsupport object format
#endif
                }
                }
#endif
            }
#elif defined(HOST_AMD64)
            {
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                if (rel->r_offset >= start_offset &&
		    rel->r_offset < start_offset + copy_size) {
                    sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                    if (strstart(sym_name, "__op_jmp", &p)) {
                      int n;
                      n = strtol(p, NULL, 10);
                      /* __op_jmp relocations are done at
                         runtime to do translated block
                         chaining: the offset of the instruction
                         needs to be stored */
                      fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n",
                              n, rel->r_offset - start_offset);
                      continue;
                    }

                    if (strstart(sym_name, "__op_PARAM", &p))
                        snprintf(name, sizeof(name), "param%s", p);
                    else if (strstart(sym_name, ".LC", NULL))
                        snprintf(name, sizeof(name), "(long)(gen_const_%s())", gen_dot_prefix(sym_name));
                    else
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                    type = ELF32_R_TYPE(rel->r_info);
                    addend = rel->r_addend;
                    switch(type) {
                    case R_X86_64_64:
                        fprintf(outfile, "    *(uintptr *)(code_ptr() + %d) = (uintptr)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                      break;
                    case R_X86_64_32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (uint32_t)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_X86_64_32S:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (int32_t)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_X86_64_PC32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s - (long)(code_ptr() + %d) + %d;\n", 
                                rel->r_offset - start_offset, name, rel->r_offset - start_offset, addend);
                        break;
                    default:
                        error("unsupported AMD64 relocation (%d)", type);
                    }
                }
                }
            }
#elif defined(HOST_PPC)
            {
#ifdef CONFIG_FORMAT_ELF
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset &&
			rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_jmp", &p)) {
                            int n;
                            n = strtol(p, NULL, 10);
                            /* __op_jmp relocations are done at
                               runtime to do translated block
                               chaining: the offset of the instruction
                               needs to be stored */
                            fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n",
                                    n, rel->r_offset - start_offset);
                            continue;
                        }
                        
                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
                            snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                        }
                        type = ELF32_R_TYPE(rel->r_info);
                        addend = rel->r_addend;
                        switch(type) {
                        case R_PPC_ADDR32:
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_PPC_ADDR16_LO:
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = (%s + %d);\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_PPC_ADDR16_HI:
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = (%s + %d) >> 16;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_PPC_ADDR16_HA:
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = (%s + %d + 0x8000) >> 16;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_PPC_REL24:
                            /* warning: must be at 32 MB distancy */
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (*(uint32_t *)(code_ptr() + %d) & ~0x03fffffc) | ((%s - (long)(code_ptr() + %d) + %d) & 0x03fffffc);\n", 
                                    rel->r_offset - start_offset, rel->r_offset - start_offset, name, rel->r_offset - start_offset, addend);
                            break;
                        default:
                            error("unsupported powerpc relocation (%d)", type);
                        }
                    }
                }
#elif defined(CONFIG_FORMAT_MACH)
				struct scattered_relocation_info *scarel;
				struct relocation_info * rel;
				char final_sym_name[256];
				const char *sym_name;
				const char *p;
				int slide, sslide;
				int i;
	
				for(i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
					unsigned int offset, length, value = 0;
					unsigned int type, pcrel, isym = 0;
					unsigned int usesym = 0;
				
					if(R_SCATTERED & rel->r_address) {
						scarel = (struct scattered_relocation_info*)rel;
						offset = (unsigned int)scarel->r_address;
						length = scarel->r_length;
						pcrel = scarel->r_pcrel;
						type = scarel->r_type;
						value = scarel->r_value;
					} else {
						value = isym = rel->r_symbolnum;
						usesym = (rel->r_extern);
						offset = rel->r_address;
						length = rel->r_length;
						pcrel = rel->r_pcrel;
						type = rel->r_type;
					}
				
					slide = offset - start_offset;
		
					if (!(offset >= start_offset && offset < start_offset + size)) 
						continue;  /* not in our range */

					sym_name = get_reloc_name(rel, &sslide);
					
					if(usesym && symtab[isym].n_type & N_STAB)
						continue; /* don't handle STAB (debug sym) */
					
					if (sym_name && strstart(sym_name, "__op_jmp", &p)) {
						int n;
						n = strtol(p, NULL, 10);
						fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n",
							n, slide);
						continue; /* Nothing more to do */
					}
					
					if(!sym_name)
					{
						fprintf(outfile, "/* #warning relocation not handled in %s (value 0x%x, %s, offset 0x%x, length 0x%x, %s, type 0x%x) */\n",
						           name, value, usesym ? "use sym" : "don't use sym", offset, length, pcrel ? "pcrel":"", type);
						continue; /* dunno how to handle without final_sym_name */
					}
													   
					if (strstart(sym_name, "__op_PARAM", &p)) {
						snprintf(final_sym_name, sizeof(final_sym_name), "param%s", p);
					} else {
						snprintf(final_sym_name, sizeof(final_sym_name), "(long)(&%s)", sym_name);
					}
			
					switch(type) {
					case PPC_RELOC_BR24:
                                          if (!strstart(sym_name, "__op_PARAM", &p)) {
						fprintf(outfile, "{\n");
						fprintf(outfile, "    uint32_t imm = *(uint32_t *)(code_ptr() + %d) & 0x3fffffc;\n", slide);
						fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (*(uint32_t *)(code_ptr() + %d) & ~0x03fffffc) | ((imm + ((long)%s - (long)code_ptr()) + %d) & 0x03fffffc);\n", 
											slide, slide, name, sslide );
						fprintf(outfile, "}\n");
                                          } else {
                                              fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = (*(uint32_t *)(code_ptr() + %d) & ~0x03fffffc) | (((long)%s - (long)code_ptr() - %d) & 0x03fffffc);\n",
                                                      slide, slide, final_sym_name, slide);
                                          }
						break;
					case PPC_RELOC_HI16:
						fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d + 2) = (%s + %d) >> 16;\n", 
							slide, final_sym_name, sslide);
						break;
					case PPC_RELOC_LO16:
						fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d + 2) = (%s + %d);\n", 
					slide, final_sym_name, sslide);
                            break;
					case PPC_RELOC_HA16:
						fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d + 2) = (%s + %d + 0x8000) >> 16;\n", 
							slide, final_sym_name, sslide);
						break;
				default:
					error("unsupported powerpc relocation (%d)", type);
				}
			}
#else
#error unsupport object format
#endif
            }
#elif defined(HOST_S390)
            {
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset &&
			rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
                            snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                        }
                        type = ELF32_R_TYPE(rel->r_info);
                        addend = rel->r_addend;
                        switch(type) {
                        case R_390_32:
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_390_16:
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = %s + %d;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        case R_390_8:
                            fprintf(outfile, "    *(uint8_t *)(code_ptr() + %d) = %s + %d;\n", 
                                    rel->r_offset - start_offset, name, addend);
                            break;
                        default:
                            error("unsupported s390 relocation (%d)", type);
                        }
                    }
                }
            }
#elif defined(HOST_ALPHA)
            {
                for (i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
		    if (rel->r_offset >= start_offset && rel->r_offset < start_offset + copy_size) {
			int type;

			type = ELF64_R_TYPE(rel->r_info);
			sym_name = strtab + symtab[ELF64_R_SYM(rel->r_info)].st_name;
			switch (type) {
			case R_ALPHA_GPDISP:
			    /* The gp is just 32 bit, and never changes, so it's easiest to emit it
			       as an immediate instead of constructing it from the pv or ra.  */
			    fprintf(outfile, "    immediate_ldah(code_ptr() + %ld, gp);\n",
				    rel->r_offset - start_offset);
			    fprintf(outfile, "    immediate_lda(code_ptr() + %ld, gp);\n",
				    rel->r_offset - start_offset + rel->r_addend);
			    break;
			case R_ALPHA_LITUSE:
			    /* jsr to literal hint. Could be used to optimize to bsr. Ignore for
			       now, since some called functions (libc) need pv to be set up.  */
			    break;
			case R_ALPHA_HINT:
			    /* Branch target prediction hint. Ignore for now.  Should be already
			       correct for in-function jumps.  */
			    break;
			case R_ALPHA_LITERAL:
			    /* Load a literal from the GOT relative to the gp.  Since there's only a
			       single gp, nothing is to be done.  */
			    break;
			case R_ALPHA_GPRELHIGH:
			    /* Handle fake relocations against __op_PARAM symbol.  Need to emit the
			       high part of the immediate value instead.  Other symbols need no
			       special treatment.  */
			    if (strstart(sym_name, "__op_PARAM", &p))
				fprintf(outfile, "    immediate_ldah(code_ptr() + %ld, param%s);\n",
					rel->r_offset - start_offset, p);
			    break;
			case R_ALPHA_GPRELLOW:
			    if (strstart(sym_name, "__op_PARAM", &p))
				fprintf(outfile, "    immediate_lda(code_ptr() + %ld, param%s);\n",
					rel->r_offset - start_offset, p);
			    break;
			case R_ALPHA_BRSGP:
			    /* PC-relative jump. Tweak offset to skip the two instructions that try to
			       set up the gp from the pv.  */
			    fprintf(outfile, "    fix_bsr(code_ptr() + %ld, (uint8_t *) &%s - (code_ptr() + %ld + 4) + 8);\n",
				    rel->r_offset - start_offset, sym_name, rel->r_offset - start_offset);
			    break;
			default:
			    error("unsupported Alpha relocation (%d)", type);
			}
		    }
                }
            }
#elif defined(HOST_IA64)
            {
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset && rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELF64_R_SYM(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
                            snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                        }
                        type = ELF64_R_TYPE(rel->r_info);
                        addend = rel->r_addend;
                        switch(type) {
			case R_IA64_LTOFF22:
			    error("must implemnt R_IA64_LTOFF22 relocation");
			case R_IA64_PCREL21B:
			    error("must implemnt R_IA64_PCREL21B relocation");
                        default:
                            error("unsupported ia64 relocation (%d)", type);
                        }
                    }
                }
            }
#elif defined(HOST_SPARC)
            {
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset &&
			rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELF32_R_SYM(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
				if (sym_name[0] == '.')
					snprintf(name, sizeof(name),
						 "(long)(&__dot_%s)",
						 sym_name + 1);
				else
					snprintf(name, sizeof(name),
						 "(long)(&%s)", sym_name);
                        }
                        type = ELF32_R_TYPE(rel->r_info);
                        addend = rel->r_addend;
                        switch(type) {
                        case R_SPARC_32:
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                    rel->r_offset - start_offset, name, addend);
			    break;
			case R_SPARC_HI22:
                            fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3fffff) "
				    " | (((%s + %d) >> 10) & 0x3fffff);\n",
                                    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend);
			    break;
			case R_SPARC_LO10:
                            fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3ff) "
				    " | ((%s + %d) & 0x3ff);\n",
                                    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend);
			    break;
			case R_SPARC_WDISP30:
			    fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3fffffff) "
				    " | ((((%s + %d) - (long)(code_ptr() + %d))>>2) "
				    "    & 0x3fffffff);\n",
				    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend,
				    rel->r_offset - start_offset);
			    break;
                        default:
                            error("unsupported sparc relocation (%d)", type);
                        }
                    }
                }
            }
#elif defined(HOST_SPARC64)
            {
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset &&
			rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELF64_R_SYM(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
                            snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                        }
                        type = ELF64_R_TYPE(rel->r_info);
                        addend = rel->r_addend;
                        switch(type) {
                        case R_SPARC_32:
                            fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n",
                                    rel->r_offset - start_offset, name, addend);
			    break;
			case R_SPARC_HI22:
                            fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3fffff) "
				    " | (((%s + %d) >> 10) & 0x3fffff);\n",
                                    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend);
			    break;
			case R_SPARC_LO10:
                            fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3ff) "
				    " | ((%s + %d) & 0x3ff);\n",
                                    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend);
			    break;
			case R_SPARC_WDISP30:
			    fprintf(outfile,
				    "    *(uint32_t *)(code_ptr() + %d) = "
				    "((*(uint32_t *)(code_ptr() + %d)) "
				    " & ~0x3fffffff) "
				    " | ((((%s + %d) - (long)(code_ptr() + %d))>>2) "
				    "    & 0x3fffffff);\n",
				    rel->r_offset - start_offset,
				    rel->r_offset - start_offset,
				    name, addend,
				    rel->r_offset - start_offset);
			    break;
                        default:
			    error("unsupported sparc64 relocation (%d)", type);
                        }
                    }
                }
            }
#elif defined(HOST_ARM)
            {
                char name[256];
                int type;
                int addend;

                arm_emit_ldr_info(name, start_offset, outfile, p_start, p_end,
                                  relocs, nb_relocs);

                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                if (rel->r_offset >= start_offset &&
		    rel->r_offset < start_offset + copy_size) {
                    sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                    /* the compiler leave some unnecessary references to the code */
                    if (sym_name[0] == '\0')
                        continue;
                    if (strstart(sym_name, "__op_PARAM", &p)) {
                        snprintf(name, sizeof(name), "param%s", p);
                    } else {
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                    }
                    type = ELF32_R_TYPE(rel->r_info);
                    addend = get32((uint32_t *)(text + rel->r_offset));
                    switch(type) {
                    case R_ARM_ABS32:
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_ARM_PC24:
                        fprintf(outfile, "    arm_reloc_pc24((uint32_t *)(code_ptr() + %d), 0x%x, %s);\n", 
                                rel->r_offset - start_offset, addend, name);
                        break;
                    default:
                        error("unsupported arm relocation (%d)", type);
                    }
                }
                }
            }
#elif defined(HOST_M68K)
            {
                char name[256];
                int type;
                int addend;
		Elf32_Sym *sym;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                if (rel->r_offset >= start_offset &&
		    rel->r_offset < start_offset + copy_size) {
		    sym = &(symtab[ELFW(R_SYM)(rel->r_info)]);
                    sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                    if (strstart(sym_name, "__op_PARAM", &p)) {
                        snprintf(name, sizeof(name), "param%s", p);
                    } else {
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                    }
                    type = ELF32_R_TYPE(rel->r_info);
                    addend = get32((uint32_t *)(text + rel->r_offset)) + rel->r_addend;
                    switch(type) {
                    case R_68K_32:
		        fprintf(outfile, "    /* R_68K_32 RELOC, offset %x */\n", rel->r_offset) ;
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s + %#x;\n", 
                                rel->r_offset - start_offset, name, addend );
                        break;
                    case R_68K_PC32:
		        fprintf(outfile, "    /* R_68K_PC32 RELOC, offset %x */\n", rel->r_offset);
                        fprintf(outfile, "    *(uint32_t *)(code_ptr() + %d) = %s - (long)(code_ptr() + %#x) + %#x;\n", 
                                rel->r_offset - start_offset, name, rel->r_offset - start_offset, /*sym->st_value+*/ addend);
                        break;
                    default:
                        error("unsupported m68k relocation (%d)", type);
                    }
                }
                }
            }
#elif defined(HOST_MIPS)
            {
                char name[256];
                int type;
                int addend;
                for (i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
                    if (rel->r_offset >= start_offset &&
                        rel->r_offset < start_offset + copy_size) {
                        sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                        if (strstart(sym_name, "__op_jmp", &p)) {
                            int n;
                            n = strtol(p, NULL, 10);
                            /* __op_jmp relocations are done at
                               runtime to do translated block
                               chaining: the offset of the instruction
                               needs to be stored */
                            fprintf(outfile, "    jmp_addr[%d] = code_ptr() + %d;\n",
                                    n, rel->r_offset - start_offset);
                            continue;
                        }

                        if (strstart(sym_name, "__op_PARAM", &p)) {
                            snprintf(name, sizeof(name), "param%s", p);
                        } else {
                            snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                        }
                        type = ELFW(R_TYPE)(rel->r_info);
                        addend = rel->r_addend;
                        if (addend)
                            error("non zero addend (%d), deal with this", addend);
                        switch (type) {
                        case R_MIPS_HI16:
                            fprintf(outfile, "    /* R_MIPS_HI16 reloc, offset %x */\n", rel->r_offset);
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = (uint16_t)((uint32_t)(%s)>>16);\n",
                                    rel->r_offset - start_offset + 2, name);
                            break;
                        case R_MIPS_LO16:
                            fprintf(outfile, "    /* R_MIPS_LO16 reloc, offset %x */\n", rel->r_offset);
                            fprintf(outfile, "    *(uint16_t *)(code_ptr() + %d) = (uint16_t)((uint32_t)(%s)&0xffff);\n",
                                    rel->r_offset - start_offset + 2, name);
                            break;
                        default:
                            error("unsupported MIPS relocation (%d)", type);
                        }
                    }
                }
            }
#else
#error unsupported CPU
#endif
            fprintf(outfile, "    inc_code_ptr(%d);\n", copy_size);
        fprintf(outfile, "}\n");
            fprintf(outfile, "#endif\n");
            fprintf(outfile, "\n");
    }
}

int gen_file(FILE *outfile, int out_type)
{
    int i;
    EXE_SYM *sym;

    assert(out_type == OUT_GEN_OP_ALL);
    if (out_type == OUT_GEN_OP_ALL) {
        int status;
        size_t nf, nd = 256;
        char *demangled_name, *func_name;
        if ((demangled_name = malloc(nd)) == NULL)
            return -1;
        if ((func_name = malloc(nf = nd)) == NULL)
            return -1;

        fprintf(outfile, "#ifndef DEFINE_CST\n");
        fprintf(outfile, "#define DEFINE_CST(NAME, VALUE)\n");
        fprintf(outfile, "#endif\n");
        for(i = 0, sym = symtab; i < nb_syms; i++, sym++) {
            const char *name;
            name = get_sym_name(sym);
            if (strstart(name, OP_PREFIX "execute", NULL)) {
                strcpy(func_name, name);
#if defined(CONFIG_FORMAT_ELF) || defined(CONFIG_FORMAT_COFF)
                if (sym->st_shndx != text_shndx)
                    error("invalid section for opcode (0x%x)", sym->st_shndx);
#endif
                gen_code(func_name, NULL, sym->st_value, sym->st_size, outfile, 3, NULL);
            }
            else if (strstart(name, OP_PREFIX "exec_return_offset", NULL)) {
                host_ulong *long_p;
#if defined(CONFIG_FORMAT_ELF) || defined(CONFIG_FORMAT_COFF)
                if (sym->st_shndx != data_shndx)
                    error("invalid section for data (0x%x)", sym->st_shndx);
#endif
                if (data == NULL)
                    error("no .data section found");
                fprintf(outfile, "DEFINE_CST(%s,0x%xL)\n\n", name, *((host_ulong *)(data + sym->st_value)));
            }
            else if (strstart(name, OP_PREFIX "invoke", NULL)) {
                const char *prefix = "helper_";
                strcpy(func_name, prefix);
                strcat(func_name, name);
#if defined(CONFIG_FORMAT_ELF) || defined(CONFIG_FORMAT_COFF)
                if (sym->st_shndx != text_shndx)
                    error("invalid section for opcode (0x%x)", sym->st_shndx);
#endif
                gen_code(func_name, NULL, sym->st_value, sym->st_size, outfile, 3, prefix);
            }
            else {
                /* demangle C++ symbols */
                demangled_name = cxx_demangle(name, demangled_name, &nd, &status);
                if (status == 0 && strstart(demangled_name, OP_PREFIX, NULL)) {
                    /* get real function name */
                    char *p = strchr(demangled_name, '(');
                    if (p && !strstart(p, "()::label", NULL)) {
                        int func_name_length = p - demangled_name;
                        if (nd > nf) {
                            nf = nd;
                            if ((func_name = realloc(func_name, nf)) == NULL)
                                return -1;
                        }
                        strncpy(func_name, demangled_name, func_name_length);
                        func_name[func_name_length] = '\0';
                        /* emit code generator */
#if defined(CONFIG_FORMAT_ELF) || defined(CONFIG_FORMAT_COFF)
                        if (sym->st_shndx != text_shndx)
                            error("invalid section for opcode (%s:0x%x)", name, sym->st_shndx);
#endif
                        gen_code(func_name, demangled_name, sym->st_value, sym->st_size, outfile, 3, NULL);
                    }
                }
            }
        }
        fprintf(outfile, "#undef DEFINE_CST\n");
        fprintf(outfile, "#undef DEFINE_GEN\n");

        free(func_name);
        free(demangled_name);
    }

    return 0;
}

void usage(void)
{
    printf("dyngen (c) 2003-2004 Fabrice Bellard\n"
           "usage: dyngen [-o outfile] objfile\n"
           "Generate a dynamic code generator from an object file\n"
           );
    exit(1);
}

int main(int argc, char **argv)
{
    int c, out_type;
    const char *filename, *outfilename;
    FILE *outfile;

    outfilename = "out.c";
    out_type = OUT_GEN_OP_ALL;
    for(;;) {
        c = getopt(argc, argv, "ho:");
        if (c == -1)
            break;
        switch(c) {
        case 'h':
            usage();
            break;
        case 'o':
            outfilename = optarg;
            break;
        }
    }
    if (optind >= argc)
        usage();
    filename = argv[optind];
    outfile = fopen(outfilename, "w");
    if (!outfile)
        error("could not open '%s'", outfilename);

    load_object(filename, outfile);
    gen_file(outfile, out_type);
    fclose(outfile);
    return 0;
}

/*
  Local variables:
  tab-width: 4
  indent-tabs-mode: nil
  End:
 */
