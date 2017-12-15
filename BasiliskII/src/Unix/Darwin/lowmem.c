/*
 *  lowmem.c - enable access to low memory globals on Darwin
 *
 *  Copyright (c) 2003 Michael Z. Sliczniak
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/vm_prot.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

static const char progname[] = "lowmem";
static const char *filename;

static int do_swap = 0;

static uint32_t target_uint32(uint32_t value)
{
	if (do_swap)
		value = OSSwapInt32(value);
	return value;
}

void pagezero_32(struct mach_header *machhead)
{
	struct segment_command *sc_cmd;

	if (target_uint32(machhead->filetype) != MH_EXECUTE) {
		(void)fprintf(stderr, "%s: %s does not appear to be an executable file\n",
				progname, filename);
		exit(1);
	}
	if (machhead->ncmds == 0) {
		(void)fprintf(stderr, "%s: %s does not contain any load commands\n",
				progname, filename);
		exit(1);
	}
	sc_cmd = (void *)&machhead[1];
	if (target_uint32(sc_cmd->cmd) != LC_SEGMENT){
		(void)fprintf(stderr, "%s: load segment not first command in %s\n",
				progname, filename);
		exit(1);
	}
	if (strncmp(sc_cmd->segname, "__PAGEZERO", sizeof (*sc_cmd->segname))) {
		(void)fprintf(stderr, "%s: zero page not first segment in %s\n",
				progname, filename);
		exit(1);
	}
	/* change the permissions */
	sc_cmd->maxprot = target_uint32(VM_PROT_ALL);
	sc_cmd->initprot = target_uint32(VM_PROT_ALL);

#ifdef MH_PIE
	/* disable pie in header */
	machhead->flags = target_uint32(target_uint32(machhead->flags) & ~MH_PIE);
#endif
}

#if defined(MH_MAGIC_64)
void pagezero_64(struct mach_header_64 *machhead)
{
	struct segment_command_64 *sc_cmd;

	if (target_uint32(machhead->filetype) != MH_EXECUTE) {
		(void)fprintf(stderr, "%s: %s does not appear to be an executable file\n",
				progname, filename);
		exit(1);
	}
	if (machhead->ncmds == 0) {
		(void)fprintf(stderr, "%s: %s does not contain any load commands\n",
				progname, filename);
		exit(1);
	}
	sc_cmd = (void *)&machhead[1];
	if (target_uint32(sc_cmd->cmd) != LC_SEGMENT_64) {
		(void)fprintf(stderr, "%s: load segment not first command in %s\n",
				progname, filename);
		exit(1);
	}
	if (strncmp(sc_cmd->segname, "__PAGEZERO", sizeof(*sc_cmd->segname))) {
		(void)fprintf(stderr, "%s: zero page not first segment in %s\n",
				progname, filename);
		exit(1);
	}
	/* change the permissions */
	sc_cmd->maxprot = target_uint32(VM_PROT_ALL);
	sc_cmd->initprot = target_uint32(VM_PROT_ALL);
}
#endif

/*
 * Under Mach there is very little assumed about the memory map of object
 * files. It is the job of the loader to create the initial memory map of an
 * executable. In a Mach-O executable there will be numerous loader commands
 * that the loader must process. Some of these will create the initial memory
 * map used by the executable. Under Darwin the static object file linker,
 * ld, automatically adds the __PAGEZERO segment to all executables. The
 * default size of this segment is the page size of the target system and
 * the initial and maximum permissions are set to allow no access. This is so
 * that all programs fault on a NULL pointer dereference. Arguably this is
 * incorrect and the maximum permissions shoould be rwx so that programs can
 * change this default behavior. Then programs could be written that assume
 * a null string at the null address, which was the convention on some
 * systems. In our case we need to have 8K mapped at zero for the low memory
 * globals and this program modifies the segment load command in the
 * basiliskII executable so that it can be used for data.
 */

int
main(int argc, const char *argv[])
{
	int fd;
	char *addr;
	size_t file_size;
	struct mach_header *machhead;
#if defined(MH_MAGIC_64)
	struct mach_header_64 *machhead64;
#endif
	struct fat_header *fathead;
	struct stat f;

	if (argc != 2) {
		(void)fprintf(stderr, "Usage: %s executable\n", progname);
		exit(1);
	}

	filename = argv[1];

	if (stat(filename, &f)) {
		(void)fprintf(stderr, "%s: could not stat %s: %s\n",
			progname, filename, strerror(errno));
		exit(1);
	}
	file_size = (size_t) f.st_size;

	fd = open(filename, O_RDWR, 0);
	if (fd == -1) {
		(void)fprintf(stderr, "%s: could not open %s: %s\n",
			progname, filename, strerror(errno));
		exit(1);
	}

	/*
	 * Size does not really matter, it will be rounded-up to a multiple
	 * of the page size automatically.
	 */
	addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE,
	    MAP_FILE | MAP_SHARED, fd, 0);
	if (addr == NULL || addr == MAP_FAILED) {
		(void)fprintf(stderr, "%s: could not mmap %s: %s\n",
				progname, filename, strerror(errno));
		exit(1);
	}

	/*
	 * Check to see if the Mach-O magic bytes are in the header.
	 */
	machhead = (void *)addr;
#if defined(MH_MAGIC_64)
	machhead64 = (void *)addr;
#endif
	fathead	= (void *)addr;

#if defined(MH_MAGIC_64)
	do_swap = machhead->magic == MH_CIGAM || fathead->magic == FAT_CIGAM || machhead64->magic == MH_CIGAM_64;
#else
	do_swap = machhead->magic == MH_CIGAM || fathead->magic == FAT_CIGAM;
#endif

	if (target_uint32(machhead->magic) == MH_MAGIC) {
		pagezero_32(machhead);
#if defined(MH_MAGIC_64)
	} else if (target_uint32(machhead64->magic) == MH_MAGIC_64) {
		pagezero_64(machhead64);
#endif
	} else if (target_uint32(fathead->magic) == FAT_MAGIC) {
		struct fat_arch *arch = (void *)&fathead[1];
		int saved_swap = do_swap;
		int i;
		for (i = 0; i < target_uint32(fathead->nfat_arch); ++i, ++arch) {
			machhead   = (void *)(addr + target_uint32(arch->offset));
#if defined(MH_MAGIC_64)
			machhead64 = (void *)(addr + target_uint32(arch->offset));
#endif
#if defined(MH_MAGIC_64)
			do_swap = machhead->magic == MH_CIGAM || machhead64->magic == MH_CIGAM_64;
#else
			do_swap = machhead->magic == MH_CIGAM;
#endif
			if (target_uint32(machhead->magic) == MH_MAGIC) {
				pagezero_32(machhead);
#if defined(MH_MAGIC_64)
			} else if (target_uint32(machhead64->magic) == MH_MAGIC_64) {
				pagezero_64(machhead64);
#endif
			} else {
				(void)fprintf(stderr, "%s: %s does not appear to be a Mach-O object file\n",
						progname, filename);
				exit(1);
			}
			do_swap = saved_swap;
		}
	} else {
		(void)fprintf(stderr, "%s: %s does not appear to be a Mach-O object file\n",
				progname, filename);
		exit(1);
	}

	/*
	 * We do not make __PAGEZERO 8K in this program because then
	 * all of the offsets would be wrong in the object file after
	 * this segment. Instead we use the -pagezero_size option
	 * to link the executable.
	 */
	if (msync(addr, file_size, MS_SYNC) == -1) {
		(void)fprintf(stderr, "%s: could not sync %s: %s\n",
				progname, filename, strerror(errno));
		exit(1);
	}

	if (munmap(addr, file_size) == -1) {
		(void)fprintf(stderr, "%s: could not unmap %s: %s\n",
				progname, filename, strerror(errno));
		exit(1);
	}

	(void)close(fd);

	exit(0);
}
