/*
 *  sheep_driver.c - Low memory and ROM access driver for SheepShaver and
 *                   Basilisk II on PowerPC systems
 *
 *  SheepShaver (C) 1997-2002 Marc Hellwig and Christian Bauer
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

#ifdef __i386__
#error The sheep driver only runs on PowerPC machines.
#endif

#include <drivers/KernelExport.h>
#include <drivers/Drivers.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>

#include "sheep_driver.h"

#define DEBUG 0

#if DEBUG==1
#define bug pprintf
#elif DEBUG==2
#define bug dprintf
#endif

#if DEBUG
#define D(x) (x)
#else
#define D(x) ;
#endif

#define PORT_NAME "sheep_driver installed"


/*
 *  For debugging
 */

static int pprintf(const char* format, ...)
{
	port_id PortNum;
	int len, ret;
	char Buffer[1024];
	va_list ap;
	
	if ((PortNum = find_port("PortLogger")) == B_NAME_NOT_FOUND)
		return(PortNum);
	for (len=0; len<1024; len++)
		Buffer[len]='\0'; 
	va_start(ap, format);
	vsprintf(Buffer, format, ap);
	ret = write_port(PortNum, 0, Buffer, strlen(Buffer));
	return ret;
}


/*
 *  Page table functions
 */

static uint32 *pte_address = 0;
static uint32 vsid;
static uint32 table_size;

static status_t map_page(uint32 ea, uint32 ra, uint32 **free_pte, uint32 bits)
{
	int i;
	int pte_class;
	uint32 hash1, hash2, api, *pteg1, *pteg2;

	D(bug("Trying to map EA %p -> RA %p\n", ea, ra));

	// Find PTEG addresses for given EA 
	hash1 = (vsid & 0x7ffff) ^ ((ea >> 12) & 0xffff);
	hash2 = ~hash1 & 0x7ffff;
	api = (ea >> 22) & 0x3f;
	pteg1 = (uint32 *)((uint32)pte_address + ((hash1 << 6) & (table_size - 1)));
	pteg2 = (uint32 *)((uint32)pte_address + ((hash2 << 6) & (table_size - 1)));
	D(bug("PTEG1 at %p, PTEG2 at %p\n", pteg1, pteg2));

	// Search all 8 PTEs of each PTEG
	*free_pte = NULL;
	pte_class = 0;
	for (i=0; i<8; i++) {
		D(bug(" found %08lx %08lx\n", pteg1[i*2], pteg1[i*2+1]));
		if (pteg1[i*2] == (0x80000000 | (vsid << 7) | (pte_class << 6) | api)) {
			*free_pte = pteg1 + i*2;
			D(bug(" existing PTE found (PTEG1)\n"));
			break;
		} else if (!pteg1[i*2]) {
			*free_pte = pteg1 + i*2;
			D(bug(" free PTE found (PTEG1)\n"));
			break;
		}
	}
	if (*free_pte == NULL) {
		pte_class = 1;
		for (i=0; i<8; i++) {
			D(bug(" found %08lx %08lx\n", pteg2[i*2], pteg2[i*2+1]));
			if (pteg2[i*2] == (0x80000000 | (vsid << 7) | (pte_class << 6) | api)) {
				*free_pte = pteg2 + i*2;
				D(bug(" existing PTE found (PTEG2)\n"));
				break;
			} else if (!pteg2[i*2]) {
				*free_pte = pteg2 + i*2;
				D(bug(" free PTE found (PTEG2)\n"));
				break;
			}
		}
	}

	// Remap page
	if (*free_pte == NULL) {
		D(bug(" No free PTE found :-(\m"));
		return B_DEVICE_FULL;
	} else {
		(*free_pte)[0] = 0x80000000 | (vsid << 7) | (pte_class << 6) | api;
		(*free_pte)[1] = ra | bits;
		D(bug(" written %08lx %08lx to PTE\n", (*free_pte)[0], (*free_pte)[1]));
		return B_NO_ERROR;
	}
}

static status_t remap_page(uint32 *free_pte, uint32 ra, uint32 bits)
{
	D(bug("Remapping PTE %p -> RA %p\n", free_pte, ra));

	// Remap page
	if (free_pte == NULL) {
		D(bug(" Invalid PTE :-(\n"));
		return B_BAD_ADDRESS;
	} else {
		free_pte[1] = ra | bits;
		D(bug(" written %08lx %08lx to PTE\n", free_pte[0], free_pte[1]));
		return B_NO_ERROR;
	}
}


/*
 *  Foward declarations for hook functions
 */

static status_t sheep_open(const char *name, uint32 flags, void **cookie);
static status_t sheep_close(void *cookie);
static status_t sheep_free(void *cookie);
static status_t sheep_control(void *cookie, uint32 op, void *data, size_t len);
static status_t sheep_read(void *cookie, off_t pos, void *data, size_t *len);
static status_t sheep_write(void *cookie, off_t pos, const void *data, size_t *len);


/*
 *  Version of our driver
 */

int32 api_version = B_CUR_DRIVER_API_VERSION;


/*
 *  Device_hooks structure - has function pointers to the
 *  various entry points for device operations
 */

static device_hooks my_device_hooks = {
	&sheep_open,
	&sheep_close,
	&sheep_free,
	&sheep_control,
	&sheep_read,
	&sheep_write,
	NULL,
	NULL,
	NULL,
	NULL
};


/*
 *  List of device names to be returned by publish_devices()
 */

static char *device_name_list[] = {
	"sheep",
	0
};


/*
 *  Init - do nothing
 */

status_t init_hardware(void)
{
#if DEBUG==2
	set_dprintf_enabled(true);
#endif
	D(bug("init_hardware()\n"));
	return B_NO_ERROR;
}

status_t init_driver(void)
{
	D(bug("init_driver()\n"));
	return B_NO_ERROR;
}

void uninit_driver(void)
{
	D(bug("uninit_driver()\n"));
}


/*
 *  publish_devices - return list of device names implemented by this driver
 */

const char **publish_devices(void)
{
	return device_name_list;
}


/*
 *  find_device - return device hooks for a specific device name
 */

device_hooks *find_device(const char *name)
{
	if (!strcmp(name, device_name_list[0]))
		return &my_device_hooks;
	
	return NULL;
}


/*
 *  sheep_open - hook function for the open call.
 */

static status_t sheep_open(const char *name, uint32 flags, void **cookie)
{
	return B_NO_ERROR;
}


/*
 *  sheep_close - hook function for the close call.
 */

static status_t sheep_close(void *cookie)
{
	return B_NO_ERROR;
}


/*
 *  sheep_free - hook function to free the cookie returned
 *  by the open hook.  Since the open hook did not return
 *  a cookie, this is a no-op.
 */

static status_t sheep_free(void *cookie)
{
	return B_NO_ERROR;
}


/*
 *  sheep_control - hook function for the ioctl call
 */

static asm void inval_tlb(uint32 ea)
{
	isync
	tlbie	r3
	sync
	blr
}

static asm void tlbsync(void)
{
	machine 604
	tlbsync
	sync
	blr
}

static status_t sheep_control(void *cookie, uint32 op, void *data, size_t len)
{
	static void *block;
	static void *block_aligned;
	physical_entry pe[2];
	system_info sysinfo;
	area_id id;
	area_info info;
	cpu_status cpu_st;
	status_t res;
	uint32 ra0, ra1;
	uint32 *free_pte_0, *free_pte_1;
	int i;

	D(bug("control(%d) data %p, len %08x\n", op, data, len));

	switch (op) {
		case SHEEP_UP:
	
			// Already messed up? Then do nothing now
			if (find_port(PORT_NAME) != B_NAME_NOT_FOUND)
				return B_NO_ERROR;

			// Get system info
			get_system_info(&sysinfo);

			// Prepare replacement memory
			block = malloc(B_PAGE_SIZE * 3);
			D(bug("3 pages malloc()ed at %p\n", block));
			block_aligned = (void *)(((uint32)block + B_PAGE_SIZE - 1) & ~(B_PAGE_SIZE-1));
			D(bug("Address aligned to %p\n", block_aligned));
			res = lock_memory(block_aligned, B_PAGE_SIZE * 2, 0);
			if (res < 0)
				return res;

			// Get memory mapping
			D(bug("Memory locked\n"));
			res = get_memory_map(block_aligned, B_PAGE_SIZE * 2, pe, 2);
			D(bug("get_memory_map returned %d\n", res));
			if (res != B_NO_ERROR)
				return res;

			// Find PTE table area
			id = find_area("pte_table");
			get_area_info(id, &info);
			pte_address = (uint32 *)info.address;
			D(bug("PTE table seems to be at %p\n", pte_address));
			table_size = info.size;
			D(bug("PTE table size: %dKB\n", table_size / 1024));

			// Disable interrupts
			cpu_st = disable_interrupts();

			// Find vsid and real addresses of replacement memory
			for (i=0; i<table_size/8; i++) {
				if (((uint32)pe[0].address & 0xfffff000)==(pte_address[i*2+1]&0xfffff000)) { 
					D(bug("Found page 0  PtePos %04x V%x VSID %03x H%x API %02x RPN %03x R%1x C%1x WIMG%1x PP%1x \n", 
					  i << 2,
					  ((pte_address[i*2]&0x80000000) >> 31),((pte_address[i*2]&0x7fffff80) >> 7),
					  ((pte_address[i*2]&0x00000040) >> 6),(pte_address[i*2] & 0x3f),
					  ((pte_address[i*2+1]&0xfffff000) >> 12),((pte_address[i*2+1]&0x00000100) >> 8),
					  ((pte_address[i*2+1]&0x00000080) >> 7),((pte_address[i*2+1]&0x00000078) >> 3),
					  (pte_address[i*2+1]&0x00000003)));
					vsid = (pte_address[i*2]&0x7fffff80) >> 7;
					ra0 = (uint32)pe[0].address & 0xfffff000;
				}
				if ((uint32)pe[0].size == B_PAGE_SIZE) {
					if (((uint32)pe[1].address & 0xfffff000)==(pte_address[i*2+1]&0xfffff000)) {
						D(bug("Found page 1f PtePos %04x V%x VSID %03x H%x API %02x RPN %03x R%1x C%1x WIMG%1x PP%1x \n", 
						  i << 2,
						  ((pte_address[i*2]&0x80000000) >> 31), ((pte_address[i*2]&0x7fffff80) >> 7),
						  ((pte_address[i*2]&0x00000040) >> 6), (pte_address[i*2] & 0x3f),
						  ((pte_address[i*2+1]&0xfffff000) >> 12), ((pte_address[i*2+1]&0x00000100) >> 8),
						  ((pte_address[i*2+1]&0x00000080) >> 7), ((pte_address[i*2+1]&0x00000078) >> 3),
						  (pte_address[i*2+1]&0x00000003)));
						ra1 = (uint32)pe[1].address & 0xfffff000;
					}
				} else {
					if ((((uint32)pe[0].address + B_PAGE_SIZE) & 0xfffff000)==(pte_address[i*2+1]&0xfffff000)) {
						D(bug("Found page 1d PtePos %04x V%x VSID %03x H%x API %02x RPN %03x R%1x C%1x WIMG%1x PP%1x \n", 
						  i << 2,
						  ((pte_address[i*2]&0x80000000) >> 31), ((pte_address[i*2]&0x7fffff80) >> 7),
						  ((pte_address[i*2]&0x00000040) >> 6), (pte_address[i*2] & 0x3f),
						  ((pte_address[i*2+1]&0xfffff000) >> 12), ((pte_address[i*2+1]&0x00000100) >> 8),
						  ((pte_address[i*2+1]&0x00000080) >> 7), ((pte_address[i*2+1]&0x00000078) >> 3),
						  (pte_address[i*2+1]&0x00000003)));
						ra1 = ((uint32)pe[0].address + B_PAGE_SIZE) & 0xfffff000;
					}
				}
			}

			// Map low memory for emulator
			free_pte_0 = NULL;
			free_pte_1 = NULL;
			__sync();
			__isync();
			inval_tlb(0);
			inval_tlb(B_PAGE_SIZE);
			if (sysinfo.cpu_type != B_CPU_PPC_603 && sysinfo.cpu_type != B_CPU_PPC_603e)
				tlbsync();
			res = map_page(0, ra0, &free_pte_0, 0x12);
			if (res == B_NO_ERROR)
				res = map_page(B_PAGE_SIZE, ra1, &free_pte_1, 0x12);
			inval_tlb(0);
			inval_tlb(B_PAGE_SIZE);
			if (sysinfo.cpu_type != B_CPU_PPC_603 && sysinfo.cpu_type != B_CPU_PPC_603e)
				tlbsync();
			__sync();
			__isync();

			// Restore interrupts
			restore_interrupts(cpu_st);

			// Create port so we know that messing was successful
			set_port_owner(create_port(1, PORT_NAME), B_SYSTEM_TEAM);
			return B_NO_ERROR;

		case SHEEP_DOWN:
			return B_NO_ERROR;

		default:
			return B_BAD_VALUE;
	}
}


/*
 *  sheep_read - hook function for the read call
 */

static status_t sheep_read(void *cookie, off_t pos, void *data, size_t *len)
{
	void *rom_adr;
	area_id area;
	system_info info;

	D(bug("read() pos %Lx, data %p, len %08x\n", pos, data, *len));

	get_system_info(&info);
	if (info.platform_type == B_BEBOX_PLATFORM) {
		*len = 0;
		return B_ERROR;
	}
	if (*len != 0x400000 && pos != 0) {
		*len = 0;
		return B_BAD_VALUE;
	}
	area = map_physical_memory("mac_rom", (void *)0xff000000, 0x00400000, B_ANY_KERNEL_ADDRESS, B_READ_AREA, &rom_adr);
	D(bug("Mapped ROM to %p, area id %d\n", rom_adr, area));
	if (area < 0) {
		*len = 0;
		return area;
	}
	D(bug("Copying ROM\n"));
	memcpy(data, rom_adr, *len);
	D(bug("Deleting area\n"));
	delete_area(area);
	return B_NO_ERROR;
}


/*
 *  sheep_write - hook function for the write call
 */
 
static status_t sheep_write(void *cookie, off_t pos, const void *data, size_t *len)
{
	D(bug("write() pos %Lx, data %p, len %08x\n", pos, data, *len));
	return B_READ_ONLY_DEVICE;
}
