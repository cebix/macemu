/*
 *  video_vosf.h - Video/graphics emulation, video on SEGV signals support
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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

/*
 *  Page-aligned memory allocation
 */

// Align on page boundaries
static uintptr align_on_page_boundary(uintptr size)
{
	const uint32 page_size = getpagesize();
	const uint32 page_mask = page_size - 1;
	return (size + page_mask) & ~page_mask;
}

// Allocate memory on page boundary
static void * allocate_framebuffer(uint32 size, uint8 * hint = 0)
{
	// Remind that the system can allocate at 0x00000000...
	return mmap((caddr_t)hint, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
}


/*
 *	Screen fault handler
 */

const uintptr INVALID_PC = (uintptr)-1;

static inline void do_handle_screen_fault(uintptr addr, uintptr pc = INVALID_PC)
{
	/* Someone attempted to write to the frame buffer. Make it writeable
	 * now so that the data could actually be written. It will be made
	 * read-only back in one of the screen update_*() functions.
	 */
	if ((addr >= mainBuffer.memStart) && (addr < mainBuffer.memEnd)) {
		const int page  = (addr - mainBuffer.memStart) >> mainBuffer.pageBits;
		caddr_t page_ad = (caddr_t)(addr & ~(mainBuffer.pageSize - 1));
		LOCK_VOSF;
		PFLAG_SET(page);
		mprotect(page_ad, mainBuffer.pageSize, PROT_READ | PROT_WRITE);
		mainBuffer.dirty = true;
		UNLOCK_VOSF;
		return;
	}
	
	/* Otherwise, we don't know how to handle the fault, let it crash */
	fprintf(stderr, "do_handle_screen_fault: unhandled address 0x%08X", addr);
	if (pc != INVALID_PC)
		fprintf(stderr, " [IP=0x%08X]", pc);
	fprintf(stderr, "\n");
	
	signal(SIGSEGV, SIG_DFL);
}

#if defined(HAVE_SIGINFO_T)

static void Screen_fault_handler(int, siginfo_t * sip, void *)
{
	D(bug("Screen_fault_handler: ADDR=0x%08X\n", sip->si_addr));
	do_handle_screen_fault((uintptr)sip->si_addr);
}

#elif defined(HAVE_SIGCONTEXT_SUBTERFUGE)

# if defined(__i386__) && defined(__linux__)
static void Screen_fault_handler(int, struct sigcontext scs)
{
	D(bug("Screen_fault_handler: ADDR=0x%08X from IP=0x%08X\n", scs.cr2, scs.eip));
	do_handle_screen_fault((uintptr)scs.cr2, (uintptr)scs.eip);
}

# elif defined(__m68k__) && defined(__NetBSD__)

# include <m68k/frame.h>
static void Screen_fault_handler(int, int code, struct sigcontext *scp)
{
	D(bug("Screen_fault_handler: ADDR=0x%08X\n", code));
	struct sigstate {
		int ss_flags;
		struct frame ss_frame;
	};
	struct sigstate *state = (struct sigstate *)scp->sc_ap;
	uintptr fault_addr;
	switch (state->ss_frame.f_format) {
		case 7:		// 68040 access error
			// "code" is sometimes unreliable (i.e. contains NULL or a bogus address), reason unknown
			fault_addr = state->ss_frame.f_fmt7.f_fa;
			break;
		default:
			fault_addr = (uintptr)code;
			break;
	}
	do_handle_screen_fault(fault_addr);
}

# elif defined(__powerpc__) && defined(__linux__)

static void Screen_fault_handler(int, struct sigcontext_struct *scs)
{
	D(bug("Screen_fault_handler: ADDR=0x%08X from IP=0x%08X\n", scs->regs->dar, scs->regs->nip));
	do_handle_screen_fault((uintptr)scs->regs->dar, (uintptr)scs->regs->nip);
}

# else
#  error "No suitable subterfuge for Video on SEGV signals"
# endif
#else
# error "Can't do Video on SEGV signals"
#endif


/*
 *	Screen fault handler initialization
 */

#if defined(HAVE_SIGINFO_T)
static bool Screen_fault_handler_init()
{
	// Setup SIGSEGV handler to process writes to frame buffer
	sigemptyset(&vosf_sa.sa_mask);
	vosf_sa.sa_sigaction = Screen_fault_handler;
	vosf_sa.sa_flags = SA_SIGINFO;
	return (sigaction(SIGSEGV, &vosf_sa, NULL) == 0);
}
#elif defined(HAVE_SIGCONTEXT_SUBTERFUGE)
static bool Screen_fault_handler_init()
{
	// Setup SIGSEGV handler to process writes to frame buffer
	sigemptyset(&vosf_sa.sa_mask);
	vosf_sa.sa_handler = (void (*)(int)) Screen_fault_handler;
#if !EMULATED_68K && defined(__NetBSD__)
	sigaddset(&vosf_sa.sa_mask, SIGALRM);
	vosf_sa.sa_flags = SA_ONSTACK;
#else
	vosf_sa.sa_flags = 0;
#endif
	return (sigaction(SIGSEGV, &vosf_sa, NULL) == 0);
}
#endif


/*
 *	Update display for Windowed mode and VOSF
 */

// From video_blit.cpp
extern void (*Screen_blit)(uint8 * dest, const uint8 * source, uint32 length);
extern bool Screen_blitter_init(XVisualInfo * visual_info, bool native_byte_order);

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

static inline void update_display_window_vosf(void)
{
	int page = 0;
	for (;;) {
		const int first_page = find_next_page_set(page);
		if (first_page >= mainBuffer.pageCount)
			break;

		page = find_next_page_clear(first_page);
		PFLAG_CLEAR_RANGE(first_page, page);

		// Make the dirty pages read-only again
		const int32 offset  = first_page << mainBuffer.pageBits;
		const uint32 length = (page - first_page) << mainBuffer.pageBits;
		mprotect((caddr_t)(mainBuffer.memStart + offset), length, PROT_READ);
		
		// There is at least one line to update
		const int y1 = mainBuffer.pageInfo[first_page].top;
		const int y2 = mainBuffer.pageInfo[page - 1].bottom;
		const int height = y2 - y1 + 1;
		
		const int bytes_per_row = VideoMonitor.bytes_per_row;
		const int bytes_per_pixel = VideoMonitor.bytes_per_row / VideoMonitor.x;
		int i, j;
		
		// Check for first column from left and first column
		// from right that have changed
		int x1, x2, width;
		if (depth == 1) {

			x1 = VideoMonitor.x - 1;
			for (j = y1; j <= y2; j++) {
				uint8 * const p1 = &the_buffer[j * bytes_per_row];
				uint8 * const p2 = &the_buffer_copy[j * bytes_per_row];
				for (i = 0; i < (x1>>3); i++) {
					if (p1[i] != p2[i]) {
						x1 = i << 3;
						break;
					}
				}
			}

			x2 = x1;
			for (j = y2; j >= y1; j--) {
				uint8 * const p1 = &the_buffer[j * bytes_per_row];
				uint8 * const p2 = &the_buffer_copy[j * bytes_per_row];
				for (i = (VideoMonitor.x>>3) - 1; i > (x2>>3); i--) {
					if (p1[i] != p2[i]) {
						x2 = (i << 3) + 7;
						break;
					}
				}
			}
			width = x2 - x1 + 1;

			// Update the_host_buffer and copy of the_buffer
			i = y1 * bytes_per_row + (x1 >> 3);
			for (j = y1; j <= y2; j++) {
				Screen_blit(the_host_buffer + i, the_buffer + i, width >> 3);
				memcpy(the_buffer_copy + i, the_buffer + i, width >> 3);
				i += bytes_per_row;
			}

		} else {

			x1 = VideoMonitor.x * bytes_per_pixel - 1;
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
		
			x2 = x1 * bytes_per_pixel;
			for (j = y2; j >= y1; j--) {
				uint8 * const p1 = &the_buffer[j * bytes_per_row];
				uint8 * const p2 = &the_buffer_copy[j * bytes_per_row];
				for (i = VideoMonitor.x * bytes_per_pixel - 1; i > x2; i--) {
					if (p1[i] != p2[i]) {
						x2 = i;
						break;
					}
				}
			}
			x2 /= bytes_per_pixel;
			width = x2 - x1 + 1;

			// Update the_host_buffer and copy of the_buffer
			i = y1 * bytes_per_row + x1 * bytes_per_pixel;
			for (j = y1; j <= y2; j++) {
				Screen_blit(the_host_buffer + i, the_buffer + i, bytes_per_pixel * width);
				memcpy(the_buffer_copy + i, the_buffer + i, bytes_per_pixel * width);
				i += bytes_per_row;
			}
		}
		
		if (have_shm)
			XShmPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, width, height, 0);
		else
			XPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, width, height);
	}
	mainBuffer.dirty = false;
}


/*
 *	Update display for DGA mode and VOSF
 *	(only in Direct Addressing mode)
 */

#if REAL_ADDRESSING || DIRECT_ADDRESSING
static inline void update_display_dga_vosf(void)
{
	int page = 0;
	for (;;) {
		const int first_page = find_next_page_set(page);
		if (first_page >= mainBuffer.pageCount)
			break;

		page = find_next_page_clear(first_page);
		PFLAG_CLEAR_RANGE(first_page, page);

		// Make the dirty pages read-only again
		const int32 offset  = first_page << mainBuffer.pageBits;
		const uint32 length = (page - first_page) << mainBuffer.pageBits;
		mprotect((caddr_t)(mainBuffer.memStart + offset), length, PROT_READ);
		
		// I am sure that y2 >= y1 and depth != 1
		const int y1 = mainBuffer.pageInfo[first_page].top;
		const int y2 = mainBuffer.pageInfo[page - 1].bottom;
		
		const int bytes_per_row = VideoMonitor.bytes_per_row;
		const int bytes_per_pixel = VideoMonitor.bytes_per_row / VideoMonitor.x;
		int i, j;
		
		// Check for first column from left and first column
		// from right that have changed
		int x1 = VideoMonitor.x * bytes_per_pixel - 1;
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
			for (i = VideoMonitor.x * bytes_per_pixel - 1; i > x2; i--) {
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
