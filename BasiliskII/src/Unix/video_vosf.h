/*
 *  video_vosf.h - Video/graphics emulation, video on SEGV signals support
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

#ifndef VIDEO_VOSF_H
#define VIDEO_VOSF_H

// Note: this file is #include'd in video_x.cpp
#ifdef ENABLE_VOSF

#include <fcntl.h>
#include <sys/mman.h>
#include "sigsegv.h"
#include "vm_alloc.h"

#ifdef ENABLE_MON
# include "mon.h"
#endif

// Variables for Video on SEGV support
static uint8 *the_host_buffer;	// Host frame buffer in VOSF mode

struct ScreenPageInfo {
    int top, bottom;			// Mapping between this virtual page and Mac scanlines
};

struct ScreenInfo {
    uintptr memStart;			// Start address aligned to page boundary
    uint32 memLength;			// Length of the memory addressed by the screen pages
    
    uintptr pageSize;			// Size of a page
    int pageBits;				// Shift count to get the page number
    uint32 pageCount;			// Number of pages allocated to the screen
    
	bool dirty;					// Flag: set if the frame buffer was touched
    char * dirtyPages;			// Table of flags set if page was altered
    ScreenPageInfo * pageInfo;	// Table of mappings page -> Mac scanlines
};

static ScreenInfo mainBuffer;

#define PFLAG_SET_VALUE			0x00
#define PFLAG_CLEAR_VALUE		0x01
#define PFLAG_SET_VALUE_4		0x00000000
#define PFLAG_CLEAR_VALUE_4		0x01010101
#define PFLAG_SET(page)			mainBuffer.dirtyPages[page] = PFLAG_SET_VALUE
#define PFLAG_CLEAR(page)		mainBuffer.dirtyPages[page] = PFLAG_CLEAR_VALUE
#define PFLAG_ISSET(page)		(mainBuffer.dirtyPages[page] == PFLAG_SET_VALUE)
#define PFLAG_ISCLEAR(page)		(mainBuffer.dirtyPages[page] != PFLAG_SET_VALUE)

#ifdef UNALIGNED_PROFITABLE
# define PFLAG_ISSET_4(page)	(*((uint32 *)(mainBuffer.dirtyPages + (page))) == PFLAG_SET_VALUE_4)
# define PFLAG_ISCLEAR_4(page)	(*((uint32 *)(mainBuffer.dirtyPages + (page))) == PFLAG_CLEAR_VALUE_4)
#else
# define PFLAG_ISSET_4(page) \
		PFLAG_ISSET(page  ) && PFLAG_ISSET(page+1) \
	&&	PFLAG_ISSET(page+2) && PFLAG_ISSET(page+3)
# define PFLAG_ISCLEAR_4(page) \
		PFLAG_ISCLEAR(page  ) && PFLAG_ISCLEAR(page+1) \
	&&	PFLAG_ISCLEAR(page+2) && PFLAG_ISCLEAR(page+3)
#endif

// Set the selected page range [ first_page, last_page [ into the SET state
#define PFLAG_SET_RANGE(first_page, last_page) \
	memset(mainBuffer.dirtyPages + (first_page), PFLAG_SET_VALUE, \
		(last_page) - (first_page))

// Set the selected page range [ first_page, last_page [ into the CLEAR state
#define PFLAG_CLEAR_RANGE(first_page, last_page) \
	memset(mainBuffer.dirtyPages + (first_page), PFLAG_CLEAR_VALUE, \
		(last_page) - (first_page))

#define PFLAG_SET_ALL do { \
	PFLAG_SET_RANGE(0, mainBuffer.pageCount); \
	mainBuffer.dirty = true; \
} while (0)

#define PFLAG_CLEAR_ALL do { \
	PFLAG_CLEAR_RANGE(0, mainBuffer.pageCount); \
	mainBuffer.dirty = false; \
} while (0)

// Set the following macro definition to 1 if your system
// provides a really fast strchr() implementation
//#define HAVE_FAST_STRCHR 0

static inline int find_next_page_set(int page)
{
#if HAVE_FAST_STRCHR
	char *match = strchr(mainBuffer.dirtyPages + page, PFLAG_SET_VALUE);
	return match ? match - mainBuffer.dirtyPages : mainBuffer.pageCount;
#else
	while (PFLAG_ISCLEAR_4(page))
		page += 4;
	while (PFLAG_ISCLEAR(page))
		page++;
	return page;
#endif
}

static inline int find_next_page_clear(int page)
{
#if HAVE_FAST_STRCHR
	char *match = strchr(mainBuffer.dirtyPages + page, PFLAG_CLEAR_VALUE);
	return match ? match - mainBuffer.dirtyPages : mainBuffer.pageCount;
#else
	while (PFLAG_ISSET_4(page))
		page += 4;
	while (PFLAG_ISSET(page))
		page++;
	return page;
#endif
}

#ifdef HAVE_PTHREADS
static pthread_mutex_t vosf_lock = PTHREAD_MUTEX_INITIALIZER;	// Mutex to protect frame buffer (dirtyPages in fact)
#define LOCK_VOSF pthread_mutex_lock(&vosf_lock);
#define UNLOCK_VOSF pthread_mutex_unlock(&vosf_lock);
#else
#define LOCK_VOSF
#define UNLOCK_VOSF
#endif

static int log_base_2(uint32 x)
{
	uint32 mask = 0x80000000;
	int l = 31;
	while (l >= 0 && (x & mask) == 0) {
		mask >>= 1;
		l--;
	}
	return l;
}

// Extend size to page boundary
static uint32 page_extend(uint32 size)
{
	const uint32 page_size = getpagesize();
	const uint32 page_mask = page_size - 1;
	return (size + page_mask) & ~page_mask;
}


/*
 *  Initialize the VOSF system (mainBuffer structure, SIGSEGV handler)
 */

static bool screen_fault_handler(sigsegv_address_t fault_address, sigsegv_address_t fault_instruction);

static bool video_vosf_init(X11_monitor_desc &monitor)
{
	const video_mode &mode = monitor.get_current_mode();

	const uintptr page_size = getpagesize();
	const uintptr page_mask = page_size - 1;
	
	// Round up frame buffer base to page boundary
	mainBuffer.memStart = (((uintptr) the_buffer) + page_mask) & ~page_mask;
	
	// The frame buffer size shall already be aligned to page boundary (use page_extend)
	mainBuffer.memLength = the_buffer_size;
	
	mainBuffer.pageSize = page_size;
	mainBuffer.pageBits = log_base_2(mainBuffer.pageSize);
	mainBuffer.pageCount =  (mainBuffer.memLength + page_mask)/mainBuffer.pageSize;
	
	// The "2" more bytes requested are a safety net to insure the
	// loops in the update routines will terminate.
	// See "How can we deal with array overrun conditions ?" hereunder for further details.
	mainBuffer.dirtyPages = (char *) malloc(mainBuffer.pageCount + 2);
	if (mainBuffer.dirtyPages == NULL)
		return false;
		
	PFLAG_CLEAR_ALL;
	PFLAG_CLEAR(mainBuffer.pageCount);
	PFLAG_SET(mainBuffer.pageCount+1);
	
	// Allocate and fill in pageInfo with start and end (inclusive) row in number of bytes
	mainBuffer.pageInfo = (ScreenPageInfo *) malloc(mainBuffer.pageCount * sizeof(ScreenPageInfo));
	if (mainBuffer.pageInfo == NULL)
		return false;
	
	uint32 a = 0;
	for (unsigned i = 0; i < mainBuffer.pageCount; i++) {
		unsigned y1 = a / mode.bytes_per_row;
		if (y1 >= mode.y)
			y1 = mode.y - 1;

		unsigned y2 = (a + mainBuffer.pageSize) / mode.bytes_per_row;
		if (y2 >= mode.y)
			y2 = mode.y - 1;

		mainBuffer.pageInfo[i].top = y1;
		mainBuffer.pageInfo[i].bottom = y2;

		a += mainBuffer.pageSize;
		if (a > mainBuffer.memLength)
			a = mainBuffer.memLength;
	}
	
	// We can now write-protect the frame buffer
	if (vm_protect((char *)mainBuffer.memStart, mainBuffer.memLength, VM_PAGE_READ) != 0)
		return false;
	
	// Initialize the handler for SIGSEGV
	if (!sigsegv_install_handler(screen_fault_handler))
		return false;
	
	// The frame buffer is sane, i.e. there is no write to it yet
	mainBuffer.dirty = false;
	return true;
}


/*
 * Deinitialize VOSF system
 */

static void video_vosf_exit(void)
{
	if (mainBuffer.pageInfo) {
		free(mainBuffer.pageInfo);
		mainBuffer.pageInfo = NULL;
	}
	if (mainBuffer.dirtyPages) {
		free(mainBuffer.dirtyPages);
		mainBuffer.dirtyPages = NULL;
	}
}


/*
 * Screen fault handler
 */

static bool screen_fault_handler(sigsegv_address_t fault_address, sigsegv_address_t fault_instruction)
{
//	D(bug("screen_fault_handler: ADDR=%p from IP=%p\n", fault_address, fault_instruction));
	const uintptr addr = (uintptr)fault_address;
	
	/* Someone attempted to write to the frame buffer. Make it writeable
	 * now so that the data could actually be written to. It will be made
	 * read-only back in one of the screen update_*() functions.
	 */
	if (((uintptr)addr - mainBuffer.memStart) < mainBuffer.memLength) {
		const int page  = ((uintptr)addr - mainBuffer.memStart) >> mainBuffer.pageBits;
		LOCK_VOSF;
		PFLAG_SET(page);
		vm_protect((char *)(addr & -mainBuffer.pageSize), mainBuffer.pageSize, VM_PAGE_READ | VM_PAGE_WRITE);
		mainBuffer.dirty = true;
		UNLOCK_VOSF;
		return true;
	}
	
	/* Otherwise, we don't know how to handle the fault, let it crash */
	fprintf(stderr, "do_handle_screen_fault: unhandled address %p", fault_address);
	if (fault_instruction != SIGSEGV_INVALID_PC)
		fprintf(stderr, " [IP=%p]", fault_instruction);
	fprintf(stderr, "\n");
#if EMULATED_68K
	uaecptr nextpc;
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
#endif
	VideoQuitFullScreen();
#ifdef ENABLE_MON
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
#endif
	return false;
}


/*
 *	Update display for Windowed mode and VOSF
 */

// From video_blit.cpp
extern void (*Screen_blit)(uint8 * dest, const uint8 * source, uint32 length);
extern bool Screen_blitter_init(XVisualInfo * visual_info, bool native_byte_order, video_depth mac_depth);
extern uint32 ExpandMap[256];

/*	How can we deal with array overrun conditions ?
	
	The state of the framebuffer pages that have been touched are maintained
	in the dirtyPages[] table. That table is (pageCount + 2) bytes long.

Terminology
	
	"Last Page" denotes the pageCount-nth page, i.e. dirtyPages[pageCount - 1].
	"CLEAR Page Guard" refers to the page following the Last Page but is always
	in the CLEAR state. "SET Page Guard" refers to the page following the CLEAR
	Page Guard but is always in the SET state.

Rough process
	
	The update routines must determine which pages have to be blitted to the
	screen. This job consists in finding the first_page that was touched.
	i.e. find the next page that is SET. Then, finding how many pages were
	touched starting from first_page. i.e. find the next page that is CLEAR.

There are two cases to check:

	- Last Page is CLEAR: find_next_page_set() will reach the SET Page Guard
	but it is beyond the valid pageCount value. Therefore, we exit from the
	update routine.
	
	- Last Page is SET: first_page equals (pageCount - 1) and
	find_next_page_clear() will reach the CLEAR Page Guard. We blit the last
	page to the screen. On the next iteration, page equals pageCount and
	find_next_page_set() will reach the SET Page Guard. We still safely exit
	from the update routine because the SET Page Guard position is greater
	than pageCount.
*/

static inline void update_display_window_vosf(driver_window *drv)
{
	const video_mode &mode = drv->monitor.get_current_mode();

	int page = 0;
	for (;;) {
		const unsigned first_page = find_next_page_set(page);
		if (first_page >= mainBuffer.pageCount)
			break;

		page = find_next_page_clear(first_page);
		PFLAG_CLEAR_RANGE(first_page, page);

		// Make the dirty pages read-only again
		const int32 offset  = first_page << mainBuffer.pageBits;
		const uint32 length = (page - first_page) << mainBuffer.pageBits;
		vm_protect((char *)mainBuffer.memStart + offset, length, VM_PAGE_READ);
		
		// There is at least one line to update
		const int y1 = mainBuffer.pageInfo[first_page].top;
		const int y2 = mainBuffer.pageInfo[page - 1].bottom;
		const int height = y2 - y1 + 1;
		
		if (mode.depth < VDEPTH_8BIT) {

			// Update the_host_buffer and copy of the_buffer
			const int src_bytes_per_row = mode.bytes_per_row;
			const int dst_bytes_per_row = drv->img->bytes_per_line;
			const int pixels_per_byte = mode.x / src_bytes_per_row;
			int i1 = y1 * src_bytes_per_row, i2 = y1 * dst_bytes_per_row, j;
			for (j = y1; j <= y2; j++) {
				Screen_blit(the_host_buffer + i2, the_buffer + i1, mode.x / pixels_per_byte);
				i1 += src_bytes_per_row;
				i2 += dst_bytes_per_row;
			}

		} else {

			// Update the_host_buffer and copy of the_buffer
			const int src_bytes_per_row = mode.bytes_per_row;
			const int dst_bytes_per_row = drv->img->bytes_per_line;
			const int bytes_per_pixel = src_bytes_per_row / mode.x;
			int i1 = y1 * src_bytes_per_row, i2 = y1 * dst_bytes_per_row, j;
			for (j = y1; j <= y2; j++) {
				Screen_blit(the_host_buffer + i2, the_buffer + i1, bytes_per_pixel * mode.x);
				i1 += src_bytes_per_row;
				i2 += dst_bytes_per_row;
			}
		}

		if (drv->have_shm)
			XShmPutImage(x_display, drv->w, drv->gc, drv->img, 0, y1, 0, y1, mode.x, height, 0);
		else
			XPutImage(x_display, drv->w, drv->gc, drv->img, 0, y1, 0, y1, mode.x, height);
	}
	mainBuffer.dirty = false;
}


/*
 *	Update display for DGA mode and VOSF
 *	(only in Real or Direct Addressing mode)
 */

#if REAL_ADDRESSING || DIRECT_ADDRESSING
static inline void update_display_dga_vosf(void)
{
	const video_mode &mode = drv->monitor.get_current_mode();

	int page = 0;
	for (;;) {
		const unsigned first_page = find_next_page_set(page);
		if (first_page >= mainBuffer.pageCount)
			break;

		page = find_next_page_clear(first_page);
		PFLAG_CLEAR_RANGE(first_page, page);

		// Make the dirty pages read-only again
		const int32 offset  = first_page << mainBuffer.pageBits;
		const uint32 length = (page - first_page) << mainBuffer.pageBits;
		vm_protect((char *)mainBuffer.memStart + offset, length, VM_PAGE_READ);
		
		// I am sure that y2 >= y1 and depth != 1
		const int y1 = mainBuffer.pageInfo[first_page].top;
		const int y2 = mainBuffer.pageInfo[page - 1].bottom;
		
		const int bytes_per_row = mode.bytes_per_row;
		const int bytes_per_pixel = mode.bytes_per_row / mode.x;
		int i, j;
		
		// Check for first column from left and first column
		// from right that have changed
		int x1 = mode.x * bytes_per_pixel - 1;
		for (j = y1; j <= y2; j++) {
			uint8 * const p1 = &the_buffer[j * bytes_per_row];
			uint8 * const p2 = &the_buffer_copy[j * bytes_per_row];
			for (i = 0; i < x1; i++) {
				if (p1[i] != p2[i]) {
					x1 = i;
					break;
				}
			}
		}
		x1 /= bytes_per_pixel;
		
		int x2 = x1 * bytes_per_pixel;
		for (j = y2; j >= y1; j--) {
			uint8 * const p1 = &the_buffer[j * bytes_per_row];
			uint8 * const p2 = &the_buffer_copy[j * bytes_per_row];
			for (i = mode.x * bytes_per_pixel - 1; i > x2; i--) {
				if (p1[i] != p2[i]) {
					x2 = i;
					break;
				}
			}
		}
		x2 /= bytes_per_pixel;
		
		// Update the_host_buffer and copy of the_buffer
		// There should be at least one pixel to copy
		const int width = x2 - x1 + 1;
		i = y1 * bytes_per_row + x1 * bytes_per_pixel;
		for (j = y1; j <= y2; j++) {
			Screen_blit(the_host_buffer + i, the_buffer + i, bytes_per_pixel * width);
			memcpy(the_buffer_copy + i, the_buffer + i, bytes_per_pixel * width);
			i += bytes_per_row;
		}
	}
	mainBuffer.dirty = false;
}
#endif

#endif /* ENABLE_VOSF */

#endif /* VIDEO_VOSF_H */
