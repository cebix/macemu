/*
 *  Generic Dynamic compiler generator
 * 
 *  Copyright (c) 2003-2004 Fabrice Bellard
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

/* host cpu defs */

#if defined(__i386__)
#define HOST_I386 1
#elif defined(__powerpc__)
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
#endif


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

enum {
    OUT_GEN_OP,
    OUT_CODE,
    OUT_INDEX_OP,
	OUT_GEN_OP_ALL,
};

/* all dynamically generated functions begin with this code */
#define OP_PREFIX "op_"

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

/* ELF file info */
int do_swap;
elf_shdr *shdr;
uint8_t **sdata;
elfhdr ehdr;
ElfW(Sym) *symtab;
int nb_syms;
char *strtab;
int text_shndx;

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

int find_reloc(int sh_index)
{
    elf_shdr *sec;
    int i;

    for(i = 0; i < ehdr.e_shnum; i++) {
        sec = &shdr[i];
        if (sec->sh_type == SHT_RELOC && sec->sh_info == sh_index) 
            return i;
    }
    return 0;
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
                            sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                            /* the compiler leave some unnecessary references to the code */
                            if (strstart(sym_name, "__op_param", &p)) {
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

static void do_print_code(FILE *outfile, const char *name, const uint8_t *code_p, int code_size)
{
  int i;
  fprintf(outfile, "    static const uint8 %s[] = {", name);
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
  do_print_code(outfile, code_name, code_p, code_size);
}

static char *gen_dot_prefix(const char *sym_name)
{
  static char name[256];
  assert(sym_name[0] == '.');
  snprintf(name, sizeof(name), "dg_dot_%s", sym_name + 1);
  return name;
}


#define MAX_ARGS 3

/* generate op code */
void gen_code(const char *name, const char *demangled_name,
              host_ulong offset, host_ulong size, 
              FILE *outfile, uint8_t *text, ELF_RELOC *relocs, int nb_relocs,
              int gen_switch, const char *prefix)
{
    int copy_size = 0;
    uint8_t *p_start, *p_end;
    host_ulong start_offset;
    int nb_args, i, n;
    uint8_t args_present[MAX_ARGS];
    const char *sym_name, *p;
    ELF_RELOC *rel;
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
    if (op_execute)
      copy_size = p_end - p_start;
    else
    switch(ELF_ARCH) {
    case EM_386:
	case EM_X86_64:
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
        break;
    case EM_PPC:
        {
            uint8_t *p;
            p = (void *)(p_end - 4);
            if (p == p_start)
                error("empty code for %s", name);
            if (get32((uint32_t *)p) != 0x4e800020)
                error("blr expected at the end of %s", name);
            copy_size = p - p_start;
        }
        break;
    case EM_S390:
	{
	    uint8_t *p;
	    p = (void *)(p_end - 2);
	    if (p == p_start)
		error("empty code for %s", name);
	    if (get16((uint16_t *)p) != 0x07fe && get16((uint16_t *)p) != 0x07f4)
		error("br %%r14 expected at the end of %s", name);
	    copy_size = p - p_start;
	}
        break;
    case EM_ALPHA:
        {
	    uint8_t *p;
	    p = p_end - 4;
	    if (p == p_start)
		error("empty code for %s", name);
            if (get32((uint32_t *)p) != 0x6bfa8001)
		error("ret expected at the end of %s", name);
	    copy_size = p - p_start;	    
	}
	break;
    case EM_IA_64:
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
        break;
    case EM_SPARC:
    case EM_SPARC32PLUS:
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
	break;
    case EM_SPARCV9:
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
	break;
#ifdef HOST_ARM
    case EM_ARM:
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
        break;
#endif
    default:
	error("unknown ELF architecture");
    }

    /* compute the number of arguments by looking at the relocations */
    for(i = 0;i < MAX_ARGS; i++)
        args_present[i] = 0;

    for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
        if (rel->r_offset >= start_offset &&
	    rel->r_offset < start_offset + (p_end - p_start)) {
            sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
            if (strstart(sym_name, "__op_param", &p)) {
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

        fprintf(outfile, "DEFINE_GEN(gen_%s,(", func_name);
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
            if (rel->r_offset >= start_offset &&
                rel->r_offset < start_offset + (p_end - p_start)) {
              sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
              if (*sym_name && 
                  !strstart(sym_name, "__op_param", NULL) &&
                  !strstart(sym_name, "__op_cpuparam", NULL) &&
                  !strstart(sym_name, "__op_jmp", NULL) &&
                  !strstart(sym_name, ".LC", NULL))
                error("unexpected external symbol %s", sym_name);
            }
        }

        fprintf(outfile, "    copy_block(%s_code, %d);\n", name, copy_size);

        /* emit code offset information */
        {
            ElfW(Sym) *sym;
            const char *sym_name, *p;
            uint32_t val; // FIXME: emulated CPU dependant?!
            int n;

            for(i = 0, sym = symtab; i < nb_syms; i++, sym++) {
                sym_name = strtab + sym->st_name;
                if (strstart(sym_name, "__op_label", &p)) {
                    uint8_t *ptr;
                    unsigned long offset;
                    
                    /* test if the variable refers to a label inside
                       the code we are generating */
                    ptr = sdata[sym->st_shndx];
                    if (!ptr)
                        error("__op_labelN in invalid section");
                    offset = sym->st_value;
                    val = *(uint32_t *)(ptr + offset);
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
                char name[256];
                int type;
                int addend;
                for(i = 0, rel = relocs;i < nb_relocs; i++, rel++) {
                if (rel->r_offset >= start_offset &&
		    rel->r_offset < start_offset + copy_size) {
                    sym_name = strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
                    if (strstart(sym_name, "__op_param", &p)) {
                        snprintf(name, sizeof(name), "param%s", p);
                    } else if (strstart(sym_name, "__op_cpuparam", &p)) {
                        snprintf(name, sizeof(name), "(long)(cpu())", p);
                    } else {
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name);
                    }
                    type = ELF32_R_TYPE(rel->r_info);
                    addend = get32((uint32_t *)(text + rel->r_offset));
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
                }
                }
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
                    if (strstart(sym_name, "__op_param", &p))
                        snprintf(name, sizeof(name), "param%s", p);
                    else if (strstart(sym_name, ".LC", NULL))
                        snprintf(name, sizeof(name), "(long)(%s)", gen_dot_prefix(sym_name));
                    else
                        snprintf(name, sizeof(name), "(long)(&%s)", sym_name[0]);
                    type = ELF32_R_TYPE(rel->r_info);
                    addend = rel->r_addend;
                    switch(type) {
                    case R_X86_64_64:
                        fprintf(outfile, "    *(uintptr *)(code_ptr() + %d) = (uintptr)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                      break;
                    case R_X86_64_32:
                        fprintf(outfile, "    *(uint32 *)(code_ptr() + %d) = (uint32)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_X86_64_32S:
                        fprintf(outfile, "    *(uint32 *)(code_ptr() + %d) = (int32)%s + %d;\n", 
                                rel->r_offset - start_offset, name, addend);
                        break;
                    case R_X86_64_PC32:
                        fprintf(outfile, "    *(uint32 *)(code_ptr() + %d) = %s - (long)(code_ptr() + %d) + %d;\n", 
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
                            fprintf(outfile, "    jmp_offsets[%d] = %d + (code_ptr() - gen_code_buf);\n",
                                    n, rel->r_offset - start_offset);
                            continue;
                        }
                        
                        if (strstart(sym_name, "__op_param", &p)) {
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
                        if (strstart(sym_name, "__op_param", &p)) {
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
			    /* Handle fake relocations against __op_param symbol.  Need to emit the
			       high part of the immediate value instead.  Other symbols need no
			       special treatment.  */
			    if (strstart(sym_name, "__op_param", &p))
				fprintf(outfile, "    immediate_ldah(code_ptr() + %ld, param%s);\n",
					rel->r_offset - start_offset, p);
			    break;
			case R_ALPHA_GPRELLOW:
			    if (strstart(sym_name, "__op_param", &p))
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
                        if (strstart(sym_name, "__op_param", &p)) {
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
                        if (strstart(sym_name, "__op_param", &p)) {
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
                        if (strstart(sym_name, "__op_param", &p)) {
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
                    if (strstart(sym_name, "__op_param", &p)) {
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
#else
#error unsupported CPU
#endif
            fprintf(outfile, "    inc_code_ptr(%d);\n", copy_size);
            fprintf(outfile, "}\n");
            fprintf(outfile, "#endif\n");
            fprintf(outfile, "\n");
    }
}

/* load an elf object file */
int load_elf(const char *filename, FILE *outfile, int out_type)
{
    int fd;
    elf_shdr *sec, *symtab_sec, *strtab_sec, *text_sec;
    int i, j;
    ElfW(Sym) *sym;
    char *shstr;
    uint8_t *text;
    ELF_RELOC *relocs;
    int nb_relocs;
    ELF_RELOC *rel;
    elf_shdr *data_sec;
    uint8_t *data;
    int data_shndx;
    elf_shdr *rodata_cst4_sec;
    uint8_t *rodata_cst4 = NULL;
    int rodata_cst4_shndx;
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
        if (sec->sh_type == SHT_RELOC) {
            nb_relocs = sec->sh_size / sec->sh_entsize;
            if (do_swap) {
                for(j = 0, rel = (ELF_RELOC *)sdata[i]; j < nb_relocs; j++, rel++)
                    elf_swap_rel(rel);
            }
        }
    }

    /* data section */
    data_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".data");
    if (!data_sec)
      error("could not find .data section");
    data_shndx = data_sec - shdr;
    data = sdata[data_shndx];

    /* rodata sections */
    rodata_cst4_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".rodata.cst4");
    if (rodata_cst4_sec) {
      rodata_cst4_shndx = rodata_cst4_sec - shdr;
      rodata_cst4 = sdata[rodata_cst4_shndx];
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

    assert(out_type == OUT_GEN_OP_ALL);
    if (out_type == OUT_GEN_OP_ALL) {
        /* generate gen_xxx functions */
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
            name = strtab + sym->st_name;
            if (strstart(name, OP_PREFIX "execute", NULL)) {
                strcpy(func_name, name);
                if (sym->st_shndx != (text_sec - shdr))
                  error("invalid section for opcode (0x%x)", sym->st_shndx);
                gen_code(func_name, NULL, sym->st_value, sym->st_size, outfile, 
                         text, relocs, nb_relocs, 3, NULL);
            }
            else if (strstart(name, OP_PREFIX "exec_return_offset", NULL)) {
                host_ulong *long_p;
                if (sym->st_shndx != (data_sec - shdr))
                  error("invalid section for data (0x%x)", sym->st_shndx);
                fprintf(outfile, "DEFINE_CST(%s,0x%xL)\n\n", name, *((host_ulong *)(data + sym->st_value)));
            }
            else if (strstart(name, OP_PREFIX "invoke", NULL)) {
                const char *prefix = "helper_";
                strcpy(func_name, prefix);
                strcat(func_name, name);
                if (sym->st_shndx != (text_sec - shdr))
                    error("invalid section for opcode (0x%x)", sym->st_shndx);
                gen_code(func_name, NULL, sym->st_value, sym->st_size, outfile, 
                         text, relocs, nb_relocs, 3, prefix);
            }
            /* emit local symbols */
            else if (strstart(name, ".LC", NULL)) {
              if (sym->st_shndx == (rodata_cst16_sec - shdr)) {
                fprintf(outfile, "#ifdef DYNGEN_IMPL\n");
                do_print_code(outfile, gen_dot_prefix(name), rodata_cst16 + sym->st_value, 16);
                fprintf(outfile, "#endif\n");
              }
              else if (sym->st_shndx == (rodata_cst4_sec - shdr)) {
                fprintf(outfile, "#ifdef DYNGEN_IMPL\n");
                do_print_code(outfile, gen_dot_prefix(name), rodata_cst4 + sym->st_value, 4);
                fprintf(outfile, "#endif\n");
              }
              else
                error("invalid section for local data %s (%x)\n", name, sym->st_shndx);
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
                  if (sym->st_shndx != (text_sec - shdr))
                    error("invalid section for opcode (%s:0x%x)", name, sym->st_shndx);
                  gen_code(func_name, demangled_name, sym->st_value, sym->st_size, outfile, 
                           text, relocs, nb_relocs, 3, NULL);
                }
              }
            }
        }
        fprintf(outfile, "#undef DEFINE_CST\n");
        fprintf(outfile, "#undef DEFINE_GEN\n");

        free(func_name);
        free(demangled_name);
    }

    close(fd);
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
    load_elf(filename, outfile, out_type);
    fclose(outfile);
    return 0;
}

/*
  Local variables:
  tab-width: 4
  indent-tabs-mode: nil
  End:
 */
