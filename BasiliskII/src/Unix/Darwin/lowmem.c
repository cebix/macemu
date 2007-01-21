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

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/vm_prot.h>
#include <mach-o/loader.h>

static const char progname[] = "lowmem";

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
	struct mach_header *machhead;
	struct segment_command *sc_cmd;

	if (argc != 2) {
		(void)fprintf(stderr, "Usage: %s executable\n", progname);
		exit(1);
	}

	fd = open(argv[1], O_RDWR, 0);
	if (fd == -1) {
		(void)fprintf(stderr, "%s: could not open %s: %s\n",
			progname, argv[1], strerror(errno));
		exit(1);
	}

	/*
	 * Size does not really matter, it will be rounded-up to a multiple
	 * of the page size automatically.
	 */
	addr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE,
	    MAP_FILE | MAP_SHARED, fd, 0);
	if (addr == NULL) {
		(void)fprintf(stderr, "%s: could not mmap %s: %s\n",
				progname, argv[1], strerror(errno));
		exit(1);
	}

	/*
	 * Check to see if the Mach-O magic bytes are in the header.
	 * If we cared about cross compiling we would also check against
	 * MH_CIGAM and then change the endianness with every access, but
	 * we do not care about that.
	 */
	machhead = (void *)addr;
	if (machhead->magic != MH_MAGIC) {
		(void)fprintf(stderr, "%s: %s does not appear to be a Mach-O object file\n",
				progname, argv[1]);
		exit(1);
	}

	if (machhead->filetype != MH_EXECUTE) {
		(void)fprintf(stderr, "%s: %s does not appear to be an executable file\n",
				progname, argv[1]);
		exit(1);
	}

	if (machhead->ncmds == 0) {
		(void)fprintf(stderr, "%s: %s does not contain any load commands\n",
				progname, argv[1]);
		exit(1);
	}

	sc_cmd = (void *)&machhead[1];
	if (sc_cmd->cmd != LC_SEGMENT){
		(void)fprintf(stderr, "%s: load segment not first command in %s\n",
				progname, argv[1]);
		exit(1);
	}

	if (strncmp(sc_cmd->segname, "__PAGEZERO",
			 sizeof (*sc_cmd->segname))) {
		(void)fprintf(stderr, "%s: zero page not first segment in %s\n",
				progname, argv[1]);
		exit(1);
	}

	/* change the permissions */
	sc_cmd->maxprot = VM_PROT_ALL;
	sc_cmd->initprot = VM_PROT_ALL;

	/*
	 * We do not make __PAGEZERO 8K in this program because then
	 * all of the offsets would be wrong in the object file after
	 * this segment. Instead we use the -pagezero_size option
	 * to link the executable.
	 */
	if (msync(addr, 0x1000, MS_SYNC) == -1) {
		(void)fprintf(stderr, "%s: could not sync %s: %s\n",
				progname, argv[1], strerror(errno));
		exit(1);
	}

	if (munmap(addr, 0x1000) == -1) {
		(void)fprintf(stderr, "%s: could not unmap %s: %s\n",
				progname, argv[1], strerror(errno));
		exit(1);
	}

	(void)close(fd);

	exit(0);
}
