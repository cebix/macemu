/*
 *  vm_alloc.cpp - Wrapper to various virtual memory allocation schemes
 *                 (supports mmap, vm_allocate or fallbacks to malloc)
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// TODO: Win32 VMs ?
#include <stdlib.h>
#include <string.h>
#include "vm_alloc.h"

#ifdef HAVE_MACH_VM
#ifndef HAVE_MACH_TASK_SELF
#ifdef HAVE_TASK_SELF
#define mach_task_self task_self
#else
#error "No task_self(), you lose."
#endif
#endif
#endif

/* We want MAP_32BIT, if available, for SheepShaver and BasiliskII
   because the emulated target is 32-bit and this helps to allocate
   memory so that branches could be resolved more easily (32-bit
   displacement to code in .text), on AMD64 for example.  */
#ifndef MAP_32BIT
#define MAP_32BIT 0
#endif

#define MAP_EXTRA_FLAGS (MAP_32BIT)

#ifdef HAVE_MMAP_VM
#if defined(__linux__) && defined(__i386__)
/* Force a reasonnable address below 0x80000000 on x86 so that we
   don't get addresses above when the program is run on AMD64.
   NOTE: this is empirically determined on Linux/x86.  */
#define MAP_BASE	0x10000000
#else
#define MAP_BASE	0x00000000
#endif
static char * next_address = (char *)MAP_BASE;
#ifdef HAVE_MMAP_ANON
#define map_flags	(MAP_PRIVATE | MAP_ANON | MAP_EXTRA_FLAGS)
#define zero_fd		-1
#else
#ifdef HAVE_MMAP_ANONYMOUS
#define map_flags	(MAP_PRIVATE | MAP_ANONYMOUS | MAP_EXTRA_FLAGS)
#define zero_fd		-1
#else
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#define map_flags	(MAP_PRIVATE | MAP_EXTRA_FLAGS)
static int zero_fd	= -1;
#endif
#endif
#endif

/* Initialize the VM system. Returns 0 if successful, -1 for errors.  */

int vm_init(void)
{
#ifdef HAVE_MMAP_VM
#ifndef zero_fd
	zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0)
		return -1;
#endif
#endif
	return 0;
}

/* Deallocate all internal data used to wrap virtual memory allocators.  */

void vm_exit(void)
{
#ifdef HAVE_MMAP_VM
#ifndef zero_fd
	close(zero_fd);
#endif
#endif
}

/* Allocate zero-filled memory of SIZE bytes. The mapping is private
   and default protection bits are read / write. The return value
   is the actual mapping address chosen or VM_MAP_FAILED for errors.  */

void * vm_acquire(size_t size)
{
	void * addr;
	
#ifdef HAVE_MACH_VM
	// vm_allocate() returns a zero-filled memory region
	if (vm_allocate(mach_task_self(), (vm_address_t *)&addr, size, TRUE) != KERN_SUCCESS)
		return VM_MAP_FAILED;
#else
#ifdef HAVE_MMAP_VM
	if ((addr = mmap((caddr_t)next_address, size, VM_PAGE_DEFAULT, map_flags, zero_fd, 0)) == MAP_FAILED)
		return VM_MAP_FAILED;
	
	next_address = (char *)addr + size;
	
	// Since I don't know the standard behavior of mmap(), zero-fill here
	if (memset(addr, 0, size) != addr)
		return VM_MAP_FAILED;
#else
	if ((addr = calloc(size, 1)) == 0)
		return VM_MAP_FAILED;
	
	// Omit changes for protections because they are not supported in this mode
	return addr;
#endif
#endif

	// Explicitely protect the newly mapped region here because on some systems,
	// say MacOS X, mmap() doesn't honour the requested protection flags.
	if (vm_protect(addr, size, VM_PAGE_DEFAULT) != 0)
		return VM_MAP_FAILED;
	
	return addr;
}

/* Allocate zero-filled memory at exactly ADDR (which must be page-aligned).
   Retuns 0 if successful, -1 on errors.  */

int vm_acquire_fixed(void * addr, size_t size)
{
#ifdef HAVE_MACH_VM
	// vm_allocate() returns a zero-filled memory region
	if (vm_allocate(mach_task_self(), (vm_address_t *)&addr, size, 0) != KERN_SUCCESS)
		return -1;
#else
#ifdef HAVE_MMAP_VM
	if (mmap((caddr_t)addr, size, VM_PAGE_DEFAULT, map_flags | MAP_FIXED, zero_fd, 0) == MAP_FAILED)
		return -1;
	
	// Since I don't know the standard behavior of mmap(), zero-fill here
	if (memset(addr, 0, size) != addr)
		return -1;
#else
	// Unsupported
	return -1;
#endif
#endif
	
	// Explicitely protect the newly mapped region here because on some systems,
	// say MacOS X, mmap() doesn't honour the requested protection flags.
	if (vm_protect(addr, size, VM_PAGE_DEFAULT) != 0)
		return -1;
	
	return 0;
}

/* Deallocate any mapping for the region starting at ADDR and extending
   LEN bytes. Returns 0 if successful, -1 on errors.  */

int vm_release(void * addr, size_t size)
{
	// Safety check: don't try to release memory that was not allocated
	if (addr == VM_MAP_FAILED)
		return 0;

#ifdef HAVE_MACH_VM
	if (vm_deallocate(mach_task_self(), (vm_address_t)addr, size) != KERN_SUCCESS)
		return -1;
#else
#ifdef HAVE_MMAP_VM
	if (munmap((caddr_t)addr, size) != 0)
		return -1;
#else
	free(addr);
#endif
#endif
	
	return 0;
}

/* Change the memory protection of the region starting at ADDR and
   extending LEN bytes to PROT. Returns 0 if successful, -1 for errors.  */

int vm_protect(void * addr, size_t size, int prot)
{
#ifdef HAVE_MACH_VM
	int ret_code = vm_protect(mach_task_self(), (vm_address_t)addr, size, 0, prot);
	return ret_code == KERN_SUCCESS ? 0 : -1;
#else
#ifdef HAVE_MMAP_VM
	int ret_code = mprotect((caddr_t)addr, size, prot);
	return ret_code == 0 ? 0 : -1;
#else
	// Unsupported
	return -1;
#endif
#endif
}

#ifdef CONFIGURE_TEST_VM_MAP
/* Tests covered here:
   - TEST_VM_PROT_* program slices actually succeeds when a crash occurs
   - TEST_VM_MAP_ANON* program slices succeeds when it could be compiled
*/
int main(void)
{
	vm_init();
	
#define page_align(address) ((char *)((unsigned long)(address) & -page_size))
	unsigned long page_size = getpagesize();
	
	const int area_size = 6 * page_size;
	volatile char * area = (volatile char *) vm_acquire(area_size);
	volatile char * fault_address = area + (page_size * 7) / 2;

#if defined(TEST_VM_MMAP_ANON) || defined(TEST_VM_MMAP_ANONYMOUS)
	if (area == VM_MAP_FAILED)
		return 1;

	if (vm_release((char *)area, area_size) < 0)
		return 1;
	
	return 0;
#endif

#if defined(TEST_VM_PROT_NONE_READ) || defined(TEST_VM_PROT_NONE_WRITE)
	if (area == VM_MAP_FAILED)
		return 0;
	
	if (vm_protect(page_align(fault_address), page_size, VM_PAGE_NOACCESS) < 0)
		return 0;
#endif

#if defined(TEST_VM_PROT_RDWR_WRITE)
	if (area == VM_MAP_FAILED)
		return 1;
	
	if (vm_protect(page_align(fault_address), page_size, VM_PAGE_READ) < 0)
		return 1;
	
	if (vm_protect(page_align(fault_address), page_size, VM_PAGE_READ | VM_PAGE_WRITE) < 0)
		return 1;
#endif

#if defined(TEST_VM_PROT_READ_WRITE)
	if (vm_protect(page_align(fault_address), page_size, VM_PAGE_READ) < 0)
		return 0;
#endif

#if defined(TEST_VM_PROT_NONE_READ)
	// this should cause a core dump
	char foo = *fault_address;
	return 0;
#endif

#if defined(TEST_VM_PROT_NONE_WRITE) || defined(TEST_VM_PROT_READ_WRITE)
	// this should cause a core dump
	*fault_address = 'z';
	return 0;
#endif

#if defined(TEST_VM_PROT_RDWR_WRITE)
	// this should not cause a core dump
	*fault_address = 'z';
	return 0;
#endif
}
#endif
