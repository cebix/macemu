/*
 *  vm_alloc.h - Wrapper to various virtual memory allocation schemes
 *               (supports mmap, vm_allocate or fallbacks to malloc)
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

#ifndef VM_ALLOC_H
#define VM_ALLOC_H

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_MACH_VM
extern "C" {
#include <mach/mach.h>
}
#endif

/* Return value of `vm_acquire' in case of an error.  */
#ifdef HAVE_MACH_VM
#define VM_MAP_FAILED			((void *)-1)
#else
#ifdef HAVE_MMAP_VM
#define VM_MAP_FAILED			((void *)-1)
#else
#define VM_MAP_FAILED			0
#endif
#endif

/* Mapping options.  */
#define VM_MAP_SHARED			0x01
#define VM_MAP_PRIVATE			0x02
#define VM_MAP_FIXED			0x04
#define VM_MAP_32BIT			0x08

/* Default mapping options.  */
#define VM_MAP_DEFAULT			(VM_MAP_PRIVATE)

/* Protection bits.  */
#ifdef HAVE_MACH_VM
#define VM_PAGE_NOACCESS		VM_PROT_NONE
#define VM_PAGE_READ			VM_PROT_READ
#define VM_PAGE_WRITE			VM_PROT_WRITE
#define VM_PAGE_EXECUTE			VM_PROT_EXECUTE
#else
#ifdef HAVE_MMAP_VM
#define VM_PAGE_NOACCESS		PROT_NONE
#define VM_PAGE_READ			PROT_READ
#define VM_PAGE_WRITE			PROT_WRITE
#define VM_PAGE_EXECUTE			PROT_EXEC
#else
#define VM_PAGE_NOACCESS		0x0
#define VM_PAGE_READ			0x1
#define VM_PAGE_WRITE			0x2
#define VM_PAGE_EXECUTE			0x4
#endif
#endif

/* Default protection bits.  */
#define VM_PAGE_DEFAULT			(VM_PAGE_READ | VM_PAGE_WRITE)

/* Initialize the VM system. Returns 0 if successful, -1 for errors.  */

extern int vm_init(void);

/* Deallocate all internal data used to wrap virtual memory allocators.  */

extern void vm_exit(void);

/* Allocate zero-filled memory of SIZE bytes. The mapping is private
   and default protection bits are read / write. The return value
   is the actual mapping address chosen or VM_MAP_FAILED for errors.  */

extern void * vm_acquire(size_t size, int options = VM_MAP_DEFAULT);

/* Allocate zero-filled memory at exactly ADDR (which must be page-aligned).
   Returns 0 if successful, -1 on errors.  */

extern int vm_acquire_fixed(void * addr, size_t size, int options = VM_MAP_DEFAULT);

/* Deallocate any mapping for the region starting at ADDR and extending
   LEN bytes. Returns 0 if successful, -1 on errors.  */

extern int vm_release(void * addr, size_t size);

/* Change the memory protection of the region starting at ADDR and
   extending LEN bytes to PROT. Returns 0 if successful, -1 for errors.  */

extern int vm_protect(void * addr, size_t size, int prot);

/* Returns the size of a page.  */

extern int vm_get_page_size(void);

#endif /* VM_ALLOC_H */
