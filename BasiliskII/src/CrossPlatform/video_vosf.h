/*
 *  video_vosf.h - Video/graphics emulation, video on SEGV signals support
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

// Note: this file must be #include'd only in video_x.cpp
#ifdef ENABLE_VOSF

#include "sigsegv.h"
#include "vm_alloc.h"
#ifdef _WIN32
#include "util_windows.h"
#endif

// Import SDL-backend-specific functions
#ifdef USE_SDL_VIDEO
extern void update_sdl_video(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h);
extern void update_sdl_video(SDL_Surface *screen, int numrects, SDL_Rect *rects);
#endif

// Glue for SDL and X11 support
#ifdef TEST_VOSF_PERFORMANCE
#define MONITOR_INIT			/* nothing */
#else
#ifdef USE_SDL_VIDEO
#define MONITOR_INIT			SDL_monitor_desc &monitor
#define VIDEO_DRV_WIN_INIT		driver_base *drv
#define VIDEO_DRV_DGA_INIT		driver_base *drv
#define VIDEO_DRV_LOCK_PIXELS	SDL_VIDEO_LOCK_SURFACE(drv->s)
#define VIDEO_DRV_UNLOCK_PIXELS	SDL_VIDEO_UNLOCK_SURFACE(drv->s)
#define VIDEO_DRV_DEPTH			drv->s->format->BitsPerPixel
#define VIDEO_DRV_WIDTH			drv->s->w
#define VIDEO_DRV_HEIGHT		drv->s->h
#define VIDEO_DRV_ROW_BYTES		drv->s->pitch
#else
#ifdef SHEEPSHAVER
#define MONITOR_INIT			/* nothing */
#define VIDEO_DRV_WIN_INIT		/* nothing */
#define VIDEO_DRV_DGA_INIT		/* nothing */
#define VIDEO_DRV_WINDOW		the_win
#define VIDEO_DRV_GC			the_gc
#define VIDEO_DRV_IMAGE			img
#define VIDEO_DRV_HAVE_SHM		have_shm
#else
#define MONITOR_INIT			X11_monitor_desc &monitor
#define VIDEO_DRV_WIN_INIT		driver_window *drv
#define VIDEO_DRV_DGA_INIT		driver_dga *drv
#define VIDEO_DRV_WINDOW		drv->w
#define VIDEO_DRV_GC			drv->gc
#define VIDEO_DRV_IMAGE			drv->img
#define VIDEO_DRV_HAVE_SHM		drv->have_shm
#endif
#define VIDEO_DRV_LOCK_PIXELS	/* nothing */
#define VIDEO_DRV_UNLOCK_PIXELS	/* nothing */
#define VIDEO_DRV_DEPTH			VIDEO_DRV_IMAGE->depth
#define VIDEO_DRV_WIDTH			VIDEO_DRV_IMAGE->width
#define VIDEO_DRV_HEIGHT		VIDEO_DRV_IMAGE->height
#define VIDEO_DRV_ROW_BYTES		VIDEO_DRV_IMAGE->bytes_per_line
#endif
#endif

// Prototypes
static void vosf_do_set_dirty_area(uintptr first, uintptr last);
static void vosf_set_dirty_area(int x, int y, int w, int h, unsigned screen_width, unsigned screen_height, unsigned bytes_per_row);

// Variables for Video on SEGV support
static uint8 *the_host_buffer;	// Host frame buffer in VOSF mode

struct ScreenPageInfo {
    unsigned top, bottom;		// Mapping between this virtual page and Mac scanlines
};

struct ScreenInfo {
    uintptr memStart;			// Start address aligned to page boundary
    uint32 memLength;			// Length of the memory addressed by the screen pages
    
    uintptr pageSize;			// Size of a page
    int pageBits;				// Shift count to get the page number
    uint32 pageCount;			// Number of pages allocated to the screen
    
	bool dirty;					// Flag: set if the frame buffer was touched
	bool very_dirty;			// Flag: set if the frame buffer was completely modified (e.g. colormap changes)
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
	mainBuffer.very_dirty = false; \
} while (0)

#define PFLAG_SET_VERY_DIRTY do { \
	mainBuffer.very_dirty = true; \
} while (0)

// Set the following macro definition to 1 if your system
// provides a really fast strchr() implementation
//#define HAVE_FAST_STRCHR 0

static inline unsigned find_next_page_set(unsigned page)
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

static inline unsigned find_next_page_clear(unsigned page)
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

#if defined(HAVE_PTHREADS)
static pthread_mutex_t vosf_lock = PTHREAD_MUTEX_INITIALIZER;	// Mutex to protect frame buffer (dirtyPages in fact)
#define LOCK_VOSF pthread_mutex_lock(&vosf_lock);
#define UNLOCK_VOSF pthread_mutex_unlock(&vosf_lock);
#elif defined(_WIN32)
static mutex_t vosf_lock;										// Mutex to protect frame buffer (dirtyPages in fact)
#define LOCK_VOSF vosf_lock.lock();
#define UNLOCK_VOSF vosf_lock.unlock();
#elif defined(HAVE_SPINLOCKS)
static spinlock_t vosf_lock = SPIN_LOCK_UNLOCKED;				// Mutex to protect frame buffer (dirtyPages in fact)
#define LOCK_VOSF spin_lock(&vosf_lock)
#define UNLOCK_VOSF spin_unlock(&vosf_lock)
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
	const uint32 page_size = vm_get_page_size();
	const uint32 page_mask = page_size - 1;
	return (size + page_mask) & ~page_mask;
}


/*
 *  Check if VOSF acceleration is profitable on this platform
 */

#ifndef VOSF_PROFITABLE_TRIES
#define VOSF_PROFITABLE_TRIES VOSF_PROFITABLE_TRIES_DFL
#endif
const int VOSF_PROFITABLE_TRIES_DFL = 3;		// Make 3 attempts for full screen update
const int VOSF_PROFITABLE_THRESHOLD = 16667/2;	// 60 Hz (half of the quantum)

static bool video_vosf_profitable(uint32 *duration_p = NULL, uint32 *n_page_faults_p = NULL)
{
	uint32 duration = 0;
	uint32 n_tries = VOSF_PROFITABLE_TRIES;
	const uint32 n_page_faults = mainBuffer.pageCount * n_tries;

#ifdef SHEEPSHAVER
	const bool accel = PrefsFindBool("gfxaccel");
#else
	const bool accel = false;
#endif

	for (uint32 i = 0; i < n_tries; i++) {
		uint64 start = GetTicks_usec();
		for (uint32 p = 0; p < mainBuffer.pageCount; p++) {
			uint8 *addr = (uint8 *)(mainBuffer.memStart + (p * mainBuffer.pageSize));
			if (accel)
				vosf_do_set_dirty_area((uintptr)addr, (uintptr)addr + mainBuffer.pageSize - 1);
			else
				addr[0] = 0; // Trigger Screen_fault_handler()
		}
		duration += uint32(GetTicks_usec() - start);

		PFLAG_CLEAR_ALL;
		mainBuffer.dirty = false;
		if (vm_protect((char *)mainBuffer.memStart, mainBuffer.memLength, VM_PAGE_READ) != 0)
			return false;
	}

	if (duration_p)
	  *duration_p = duration;
	if (n_page_faults_p)
	  *n_page_faults_p = n_page_faults;

	D(bug("Triggered %d page faults in %ld usec (%.1f usec per fault)\n", n_page_faults, duration, double(duration) / double(n_page_faults)));
	return ((duration / n_tries) < (VOSF_PROFITABLE_THRESHOLD * (frame_skip ? frame_skip : 1)));
}


/*
 *  Initialize the VOSF system (mainBuffer structure, SIGSEGV handler)
 */

static bool video_vosf_init(MONITOR_INIT)
{
	VIDEO_MODE_INIT_MONITOR;

	const uintptr page_size = vm_get_page_size();
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
		unsigned y1 = a / VIDEO_MODE_ROW_BYTES;
		if (y1 >= VIDEO_MODE_Y)
			y1 = VIDEO_MODE_Y - 1;

		unsigned y2 = (a + mainBuffer.pageSize) / VIDEO_MODE_ROW_BYTES;
		if (y2 >= VIDEO_MODE_Y)
			y2 = VIDEO_MODE_Y - 1;

		mainBuffer.pageInfo[i].top = y1;
		mainBuffer.pageInfo[i].bottom = y2;

		a += mainBuffer.pageSize;
		if (a > mainBuffer.memLength)
			a = mainBuffer.memLength;
	}
	
	// We can now write-protect the frame buffer
	if (vm_protect((char *)mainBuffer.memStart, mainBuffer.memLength, VM_PAGE_READ) != 0)
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
 * Update VOSF state with specified dirty area
 */

static void vosf_do_set_dirty_area(uintptr first, uintptr last)
{
	const int first_page = (first - mainBuffer.memStart) >> mainBuffer.pageBits;
	const int last_page = (last - mainBuffer.memStart) >> mainBuffer.pageBits;
	uint8 *addr = (uint8 *)(first & ~(mainBuffer.pageSize - 1));
	for (int i = first_page; i <= last_page; i++) {
		if (PFLAG_ISCLEAR(i)) {
			PFLAG_SET(i);
			vm_protect(addr, mainBuffer.pageSize, VM_PAGE_READ | VM_PAGE_WRITE);
		}
		addr += mainBuffer.pageSize;
	}
}

static void vosf_set_dirty_area(int x, int y, int w, int h, unsigned screen_width, unsigned screen_height, unsigned bytes_per_row)
{
	if (x < 0) {
		w -= -x;
		x = 0;
	}
	if (y < 0) {
		h -= -y;
		y = 0;
	}
	if (w <= 0 || h <= 0)
		return;
	if (unsigned(x + w) > screen_width)
		w -= unsigned(x + w) - screen_width;
	if (unsigned(y + h) > screen_height)
		h -= unsigned(y + h) - screen_height;
	LOCK_VOSF;
	if (bytes_per_row >= screen_width) {
		const int bytes_per_pixel = bytes_per_row / screen_width;
		if (bytes_per_row <= mainBuffer.pageSize) {
			const uintptr a0 = mainBuffer.memStart + y * bytes_per_row + x * bytes_per_pixel;
			const uintptr a1 = mainBuffer.memStart + (y + h - 1) * bytes_per_row + (x + w - 1) * bytes_per_pixel;
			vosf_do_set_dirty_area(a0, a1);
		} else {
			for (int j = y; j < y + h; j++) {
				const uintptr a0 = mainBuffer.memStart + j * bytes_per_row + x * bytes_per_pixel;
				const uintptr a1 = a0 + (w - 1) * bytes_per_pixel;
				vosf_do_set_dirty_area(a0, a1);
			}
		}
	} else {
		const int pixels_per_byte = screen_width / bytes_per_row;
		if (bytes_per_row <= mainBuffer.pageSize) {
			const uintptr a0 = mainBuffer.memStart + y * bytes_per_row + x / pixels_per_byte;
			const uintptr a1 = mainBuffer.memStart + (y + h - 1) * bytes_per_row + (x + w - 1) / pixels_per_byte;
			vosf_do_set_dirty_area(a0, a1);
		} else {
			for (int j = y; j < y + h; j++) {
				const uintptr a0 = mainBuffer.memStart + j * bytes_per_row + x / pixels_per_byte;
				const uintptr a1 = mainBuffer.memStart + j * bytes_per_row + (x + w - 1) / pixels_per_byte;
				vosf_do_set_dirty_area(a0, a1);
			}
		}
	}
	mainBuffer.dirty = true;
	UNLOCK_VOSF;
}


/*
 * Screen fault handler
 */

bool Screen_fault_handler(sigsegv_info_t *sip)
{
  const uintptr addr = (uintptr)sigsegv_get_fault_address(sip);
	
	/* Someone attempted to write to the frame buffer. Make it writeable
	 * now so that the data could actually be written to. It will be made
	 * read-only back in one of the screen update_*() functions.
	 */
	if (((uintptr)addr - mainBuffer.memStart) < mainBuffer.memLength) {
		const int page  = ((uintptr)addr - mainBuffer.memStart) >> mainBuffer.pageBits;
		LOCK_VOSF;
		if (PFLAG_ISCLEAR(page)) {
			PFLAG_SET(page);
			vm_protect((char *)(addr & ~(mainBuffer.pageSize - 1)), mainBuffer.pageSize, VM_PAGE_READ | VM_PAGE_WRITE);
		}
		mainBuffer.dirty = true;
		UNLOCK_VOSF;
		return true;
	}
	
	/* Otherwise, we don't know how to handle the fault, let it crash */
	return false;
}


/*
 *	Update display for Windowed mode and VOSF
 */

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

#ifndef TEST_VOSF_PERFORMANCE
static void update_display_window_vosf(VIDEO_DRV_WIN_INIT)
{
	VIDEO_MODE_INIT;

	unsigned page = 0;
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

		// Update the_host_buffer
		VIDEO_DRV_LOCK_PIXELS;
		const int src_bytes_per_row = VIDEO_MODE_ROW_BYTES;
		const int dst_bytes_per_row = VIDEO_DRV_ROW_BYTES;
		int i1 = y1 * src_bytes_per_row, i2 = y1 * dst_bytes_per_row, j;
		for (j = y1; j <= y2; j++) {
			Screen_blit(the_host_buffer + i2, the_buffer + i1, src_bytes_per_row);
			i1 += src_bytes_per_row;
			i2 += dst_bytes_per_row;
		}
		VIDEO_DRV_UNLOCK_PIXELS;

#ifdef USE_SDL_VIDEO
		update_sdl_video(drv->s, 0, y1, VIDEO_MODE_X, height);
#else
		if (VIDEO_DRV_HAVE_SHM)
			XShmPutImage(x_display, VIDEO_DRV_WINDOW, VIDEO_DRV_GC, VIDEO_DRV_IMAGE, 0, y1, 0, y1, VIDEO_MODE_X, height, 0);
		else
			XPutImage(x_display, VIDEO_DRV_WINDOW, VIDEO_DRV_GC, VIDEO_DRV_IMAGE, 0, y1, 0, y1, VIDEO_MODE_X, height);
#endif
	}
	mainBuffer.dirty = false;
}
#endif


/*
 *	Update display for DGA mode and VOSF
 *	(only in Real or Direct Addressing mode)
 */

#ifndef TEST_VOSF_PERFORMANCE
#if REAL_ADDRESSING || DIRECT_ADDRESSING
static void update_display_dga_vosf(VIDEO_DRV_DGA_INIT)
{
	VIDEO_MODE_INIT;

	// Compute number of bytes per row, take care to virtual screens
	const int src_bytes_per_row = VIDEO_MODE_ROW_BYTES;
	const int dst_bytes_per_row = TrivialBytesPerRow(VIDEO_MODE_X, DepthModeForPixelDepth(VIDEO_DRV_DEPTH));
	const int scr_bytes_per_row = VIDEO_DRV_ROW_BYTES;
	assert(dst_bytes_per_row <= scr_bytes_per_row);
	const int scr_bytes_left = scr_bytes_per_row - dst_bytes_per_row;

	// Full screen update requested?
	if (mainBuffer.very_dirty) {
		PFLAG_CLEAR_ALL;
		vm_protect((char *)mainBuffer.memStart, mainBuffer.memLength, VM_PAGE_READ);
		memcpy(the_buffer_copy, the_buffer, VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y);
		VIDEO_DRV_LOCK_PIXELS;
		int i1 = 0, i2 = 0;
		for (uint32_t j = 0;  j < VIDEO_MODE_Y; j++) {
			Screen_blit(the_host_buffer + i2, the_buffer + i1, src_bytes_per_row);
			i1 += src_bytes_per_row;
			i2 += scr_bytes_per_row;
		}
#ifdef USE_SDL_VIDEO
		update_sdl_video(drv->s, 0, 0, VIDEO_MODE_X, VIDEO_MODE_Y);
#endif
		VIDEO_DRV_UNLOCK_PIXELS;
		return;
	}

	// Setup partial blitter (use 64-pixel wide chunks)
	const uint32 n_pixels = 64;
	const uint32 n_chunks = VIDEO_MODE_X / n_pixels;
	const uint32 n_pixels_left = VIDEO_MODE_X - (n_chunks * n_pixels);
	const uint32 src_chunk_size = src_bytes_per_row / n_chunks;
	const uint32 dst_chunk_size = dst_bytes_per_row / n_chunks;
	const uint32 src_chunk_size_left = src_bytes_per_row - (n_chunks * src_chunk_size);
	const uint32 dst_chunk_size_left = dst_bytes_per_row - (n_chunks * dst_chunk_size);

	unsigned page = 0;
	uint32 last_scanline = uint32(-1);
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

		// Optimized for scanlines, don't process overlapping lines again
		uint32 y1 = mainBuffer.pageInfo[first_page].top;
		uint32 y2 = mainBuffer.pageInfo[page - 1].bottom;
		if (last_scanline != uint32(-1)) {
			if (y1 <= last_scanline && ++y1 >= VIDEO_MODE_Y)
				continue;
			if (y2 <= last_scanline && ++y2 >= VIDEO_MODE_Y)
				continue;
		}
		last_scanline = y2;

		// Update the_host_buffer and copy of the_buffer, one line at a time
		uint32 i1 = y1 * src_bytes_per_row;
		uint32 i2 = y1 * scr_bytes_per_row;
#ifdef USE_SDL_VIDEO
		int bbi = 0;
		SDL_Rect bb[3] = {
			{ Sint16(VIDEO_MODE_X), Sint16(y1), 0, 0 },
			{ Sint16(VIDEO_MODE_X), -1, 0, 0 },
			{ Sint16(VIDEO_MODE_X), -1, 0, 0 }
		};
#endif
		VIDEO_DRV_LOCK_PIXELS;
		for (uint32 j = y1; j <= y2; j++) {
			for (uint32 i = 0; i < n_chunks; i++) {
				if (memcmp(the_buffer_copy + i1, the_buffer + i1, src_chunk_size) != 0) {
					memcpy(the_buffer_copy + i1, the_buffer + i1, src_chunk_size);
					Screen_blit(the_host_buffer + i2, the_buffer + i1, src_chunk_size);
#ifdef USE_SDL_VIDEO
					const int x = i * n_pixels;
					if (x < bb[bbi].x) {
						if (bb[bbi].w)
							bb[bbi].w += bb[bbi].x - x;
						else
							bb[bbi].w = n_pixels;
						bb[bbi].x = x;
					}
					else if (x >= bb[bbi].x + bb[bbi].w)
						bb[bbi].w = x + n_pixels - bb[bbi].x;
#endif
				}
				i1 += src_chunk_size;
				i2 += dst_chunk_size;
			}
			if (src_chunk_size_left && dst_chunk_size_left) {
				if (memcmp(the_buffer_copy + i1, the_buffer + i1, src_chunk_size_left) != 0) {
					memcpy(the_buffer_copy + i1, the_buffer + i1, src_chunk_size_left);
					Screen_blit(the_host_buffer + i2, the_buffer + i1, src_chunk_size_left);
				}
				i1 += src_chunk_size_left;
				i2 += dst_chunk_size_left;
#ifdef USE_SDL_VIDEO
				const int x = n_chunks * n_pixels;
				if (x < bb[bbi].x) {
					if (bb[bbi].w)
						bb[bbi].w += bb[bbi].x - x;
					else
						bb[bbi].w = n_pixels_left;
					bb[bbi].x = x;
				}
				else if (x >= bb[bbi].x + bb[bbi].w)
					bb[bbi].w  = x + n_pixels_left - bb[bbi].x;
#endif
			}
			i2 += scr_bytes_left;
#ifdef USE_SDL_VIDEO
			bb[bbi].h++;
			if (bb[bbi].w && (j == y1 || j == y2 - 1 || j == y2)) {
				bbi++;
				assert(bbi <= 3);
				if (j != y2)
					bb[bbi].y = j + 1;
			}
#endif
		}
#ifdef USE_SDL_VIDEO
		update_sdl_video(drv->s, bbi, bb);
#endif
		VIDEO_DRV_UNLOCK_PIXELS;
	}
	mainBuffer.dirty = false;
}
#endif
#endif

#endif /* ENABLE_VOSF */

#endif /* VIDEO_VOSF_H */
