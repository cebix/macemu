/*
 *  clip_unix.cpp - Clipboard handling, Unix implementation
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

/*
 *  NOTES:
 *
 *  We must have (fast) X11 display locking routines. Otherwise, we
 *  can corrupt the X11 event queue from the emulator thread whereas
 *  the redraw thread is expected to handle them.
 *
 *  Two functions are exported to video_x.cpp:
 *    - ClipboardSelectionClear()
 *      called when we lose the selection ownership
 *    - ClipboardSelectionRequest()
 *      called when another client wants our clipboard data
 *  The display is locked by the redraw thread during their execution.
 *
 *  On PutScrap (Mac application wrote to clipboard), we always cache
 *  the Mac clipboard to a local structure (clip_data). Then, the
 *  selection ownership is grabbed until ClipboardSelectionClear()
 *  occurs. In that case, contents in cache becomes invalid.
 *
 *  On GetScrap (Mac application reads clipboard), we always fetch
 *  data from the X11 clipboard and immediately put it back to Mac
 *  side. Local cache does not need to be updated. If the selection
 *  owner supports the TIMESTAMP target, we can avoid useless copies.
 *
 *  For safety purposes, we lock the X11 display in the emulator
 *  thread during the whole GetScrap/PutScrap execution. Of course, we
 *  temporarily release the lock when waiting for SelectioNotify.
 *
 *  TODO:
 *    - handle 'PICT' to image/png, image/ppm, PIXMAP (prefs order)
 *    - handle 'styl' to text/richtext (OOo Writer)
 *    - patch ZeroScrap so that we know when cached 'styl' is stale?
 */

#include "sysdeps.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <vector>

#include "macos_util.h"
#include "clip.h"
#include "prefs.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"

#define DEBUG 0
#include "debug.h"

#ifndef NO_STD_NAMESPACE
using std::vector;
#endif


// Do we want GetScrap() to check for TIMESTAMP and optimize out clipboard syncs?
#define GETSCRAP_REQUESTS_TIMESTAMP 0

// Do we want GetScrap() to first check for TARGETS available from the clipboard?
#define GETSCRAP_REQUESTS_TARGETS 0


// From main_linux.cpp
extern Display *x_display;


// Conversion tables
static const uint8 mac2iso[0x80] = {
	0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1,
	0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
	0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3,
	0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
	0x2b, 0xb0, 0xa2, 0xa3, 0xa7, 0xb7, 0xb6, 0xdf,
	0xae, 0xa9, 0x20, 0xb4, 0xa8, 0x23, 0xc6, 0xd8,
	0x20, 0xb1, 0x3c, 0x3e, 0xa5, 0xb5, 0xf0, 0x53,
	0x50, 0x70, 0x2f, 0xaa, 0xba, 0x4f, 0xe6, 0xf8,
	0xbf, 0xa1, 0xac, 0x2f, 0x66, 0x7e, 0x44, 0xab,
	0xbb, 0x2e, 0x20, 0xc0, 0xc3, 0xd5, 0x4f, 0x6f,
	0x2d, 0x2d, 0x22, 0x22, 0x60, 0x27, 0xf7, 0x20,
	0xff, 0x59, 0x2f, 0xa4, 0x3c, 0x3e, 0x66, 0x66,
	0x23, 0xb7, 0x2c, 0x22, 0x25, 0xc2, 0xca, 0xc1,
	0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
	0x20, 0xd2, 0xda, 0xdb, 0xd9, 0x69, 0x5e, 0x7e,
	0xaf, 0x20, 0xb7, 0xb0, 0xb8, 0x22, 0xb8, 0x20
};

static const uint8 iso2mac[0x80] = {
	0xad, 0xb0, 0xe2, 0xc4, 0xe3, 0xc9, 0xa0, 0xe0,
	0xf6, 0xe4, 0xde, 0xdc, 0xce, 0xb2, 0xb3, 0xb6,
	0xb7, 0xd4, 0xd5, 0xd2, 0xd3, 0xa5, 0xd0, 0xd1,
	0xf7, 0xaa, 0xdf, 0xdd, 0xcf, 0xba, 0xfd, 0xd9,
	0xca, 0xc1, 0xa2, 0xa3, 0xdb, 0xb4, 0xbd, 0xa4,
	0xac, 0xa9, 0xbb, 0xc7, 0xc2, 0xf0, 0xa8, 0xf8,
	0xa1, 0xb1, 0xc3, 0xc5, 0xab, 0xb5, 0xa6, 0xe1,
	0xfc, 0xc6, 0xbc, 0xc8, 0xf9, 0xda, 0xd7, 0xc0,
	0xcb, 0xe7, 0xe5, 0xcc, 0x80, 0x81, 0xae, 0x82,
	0xe9, 0x83, 0xe6, 0xe8, 0xed, 0xea, 0xeb, 0xec,
	0xf5, 0x84, 0xf1, 0xee, 0xef, 0xcd, 0x85, 0xfb,
	0xaf, 0xf4, 0xf2, 0xf3, 0x86, 0xfa, 0xb8, 0xa7,
	0x88, 0x87, 0x89, 0x8b, 0x8a, 0x8c, 0xbe, 0x8d,
	0x8f, 0x8e, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,
	0xfe, 0x96, 0x98, 0x97, 0x99, 0x9b, 0x9a, 0xd6,
	0xbf, 0x9d, 0x9c, 0x9e, 0x9f, 0xff, 0xb9, 0xd8
};

// Flag: Don't convert clipboard text
static bool no_clip_conversion;

// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the Unix side
static bool we_put_this_data = false;

// X11 variables
static int screen;						// Screen number
static Window rootwin, clip_win;		// Root window and the clipboard window
static Atom xa_clipboard;
static Atom xa_targets;
static Atom xa_multiple;
static Atom xa_timestamp;
static Atom xa_atom_pair;

// Define a byte array (rewrite if it's a bottleneck)
struct ByteArray : public vector<uint8> {
	void resize(int size) { reserve(size); vector<uint8>::resize(size); }
	uint8 *data() { return &(*this)[0]; }
};

// Clipboard data for requestors
struct ClipboardData {
	Time time;
	Atom type;
	ByteArray data;
};
static ClipboardData clip_data;

// Prototypes
static void do_putscrap(uint32 type, void *scrap, int32 length);
static void do_getscrap(void **handle, uint32 type, int32 offset);


/*
 *  Read an X11 property (hack from QT 3.1.2)
 */

static inline int max_selection_incr(Display *dpy)
{
	int max_request_size = 4 * XMaxRequestSize(dpy);
	if (max_request_size > 4 * 65536)
		max_request_size = 4 * 65536;
	else if ((max_request_size -= 100) < 0)
		max_request_size = 100;
	return max_request_size;
}

static bool read_property(Display *dpy, Window win,
						  Atom property, bool deleteProperty,
						  ByteArray & buffer, int *size, Atom *type,
						  int *format, bool nullterm)
{
	int maxsize = max_selection_incr(dpy);
	unsigned long bytes_left;
	unsigned long length;
	unsigned char *data;
	Atom dummy_type;
	int dummy_format;

	if (!type)
		type = &dummy_type;
	if (!format)
		format = &dummy_format;

	// Don't read anything but get the size of the property data
	if (XGetWindowProperty(dpy, win, property, 0, 0, False,
						   AnyPropertyType, type, format, &length, &bytes_left, &data) != Success) {
		buffer.clear();
		return false;
	}
	XFree(data);

	int offset = 0, buffer_offset = 0, format_inc = 1, proplen = bytes_left;

	switch (*format) {
	case 16:
		format_inc = sizeof(short) / 2;
		proplen *= format_inc;
		break;
	case 32:
		format_inc = sizeof(long) / 4;
		proplen *= format_inc;
		break;
	}

	buffer.resize(proplen + (nullterm ? 1 : 0));
	while (bytes_left) {
		if (XGetWindowProperty(dpy, win, property, offset, maxsize / 4,
							   False, AnyPropertyType, type, format,
							   &length, &bytes_left, &data) != Success)
			break;

		offset += length / (32 / *format);
		length *= format_inc * (*format / 8);

		memcpy(buffer.data() + buffer_offset, data, length);
		buffer_offset += length;
		XFree(data);
	}

	if (nullterm)
		buffer[buffer_offset] = '\0';

	if (size)
		*size = buffer_offset;

	if (deleteProperty)
		XDeleteProperty(dpy, win, property);

	XFlush(dpy);
	return true;
}


/*
 *  Timed wait for a SelectionNotify event
 */

static const uint64 SELECTION_MAX_WAIT = 500000; // 500 ms

static bool wait_for_selection_notify_event(Display *dpy, Window win, XEvent *event, int timeout)
{
	uint64 start = GetTicks_usec();

	do {
		// Wait
		XDisplayUnlock();
		Delay_usec(5000);
		XDisplayLock();

		// Check for SelectionNotify event
		if (XCheckTypedWindowEvent(dpy, win, SelectionNotify, event))
			return true;

	} while ((GetTicks_usec() - start) < timeout);

	return false;
}


/*
 *  Initialization
 */

void ClipInit(void)
{
	no_clip_conversion = PrefsFindBool("noclipconversion");

	// Find screen and root window
	screen = XDefaultScreen(x_display);
	rootwin = XRootWindow(x_display, screen);

	// Create fake window to receive selection events
	clip_win = XCreateSimpleWindow(x_display, rootwin, 0, 0, 1, 1, 0, 0, 0);

	// Initialize X11 atoms
	xa_clipboard = XInternAtom(x_display, "CLIPBOARD", False);
	xa_targets = XInternAtom(x_display, "TARGETS", False);
	xa_multiple = XInternAtom(x_display, "MULTIPLE", False);
	xa_timestamp = XInternAtom(x_display, "TIMESTAMP", False);
	xa_atom_pair = XInternAtom(x_display, "ATOM_PAIR", False);
}


/*
 *  Deinitialization
 */

void ClipExit(void)
{
	// Close window
	if (clip_win)
		XDestroyWindow(x_display, clip_win);
}


/*
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %p, length %ld\n", type, scrap, length));
	if (we_put_this_data) {
		we_put_this_data = false;
		return;
	}
	if (length <= 0)
		return;

	XDisplayLock();
	do_putscrap(type, scrap, length);
	XDisplayUnlock();
}

static void do_putscrap(uint32 type, void *scrap, int32 length)
{
	clip_data.type = None;
	switch (type) {
	case FOURCC('T','E','X','T'): {
		D(bug(" clipping TEXT\n"));
		clip_data.type = XA_STRING;
		clip_data.data.clear();
		clip_data.data.reserve(length);

		// Convert text from Mac charset to ISO-Latin1
		uint8 *p = (uint8 *)scrap;
		for (int i=0; i<length; i++) {
			uint8 c = *p++;
			if (c < 0x80) {
				if (c == 13)	// CR -> LF
					c = 10;
			} else if (!no_clip_conversion)
				c = mac2iso[c & 0x7f];
			clip_data.data.push_back(c);
		}
		break;
	}

	case FOURCC('s','t','y','l'): {
		D(bug(" clipping styl\n"));
		uint16 *p = (uint16 *)scrap;
		uint16 n = ntohs(*p++);
		D(bug(" %d styles (%d bytes)\n", n, length));
		for (int i = 0; i < n; i++) {
			uint32 offset = ntohl(*(uint32 *)p); p += 2;
			uint16 line_height = ntohs(*p++);
			uint16 font_ascent = ntohs(*p++);
			uint16 font_family = ntohs(*p++);
			uint16 style_code = ntohs(*p++);
			uint16 char_size = ntohs(*p++);
			uint16 r = ntohs(*p++);
			uint16 g = ntohs(*p++);
			uint16 b = ntohs(*p++);
			D(bug("  offset=%d, height=%d, font ascent=%d, id=%d, style=%x, size=%d, RGB=%x/%x/%x\n",
				  offset, line_height, font_ascent, font_family, style_code, char_size, r, g, b));
		}
		break;
	}
	}

	// Acquire selection ownership
	if (clip_data.type != None) {
		clip_data.time = CurrentTime;
		while (XGetSelectionOwner(x_display, xa_clipboard) != clip_win)
			XSetSelectionOwner(x_display, xa_clipboard, clip_win, clip_data.time);
	}
}


/*
 *  Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));

	XDisplayLock();
	do_getscrap(handle, type, offset);
	XDisplayUnlock();
}

static void do_getscrap(void **handle, uint32 type, int32 offset)
{
	ByteArray data;
	XEvent event;

	// If we own the selection, the data is already available on MacOS side
	if (XGetSelectionOwner(x_display, xa_clipboard) == clip_win)
		return;

	// Check TIMESTAMP
#if GETSCRAP_REQUESTS_TIMESTAMP
	static Time last_timestamp = 0;
	XConvertSelection(x_display, xa_clipboard, xa_timestamp, xa_clipboard, clip_win, CurrentTime);
	if (wait_for_selection_notify_event(x_display, clip_win, &event, SELECTION_MAX_WAIT) &&
		event.xselection.property != None &&
		read_property(x_display,
					  event.xselection.requestor, event.xselection.property,
					  true, data, 0, 0, 0, false)) {
		Time timestamp = ((long *)data.data())[0];
		if (timestamp <= last_timestamp)
			return;
	}
	last_timestamp = CurrentTime;
#endif

	// Get TARGETS available
#if GETSCRAP_REQUESTS_TARGETS
	XConvertSelection(x_display, xa_clipboard, xa_targets, xa_clipboard, clip_win, CurrentTime);
	if (!wait_for_selection_notify_event(x_display, clip_win, &event, SELECTION_MAX_WAIT) ||
		event.xselection.property == None ||
		!read_property(x_display,
					   event.xselection.requestor, event.xselection.property,
					   true, data, 0, 0, 0, false))
		return;
#endif

	// Get appropriate format for requested data
	Atom format = None;
#if GETSCRAP_REQUESTS_TARGETS
	int n_atoms = data.size() / sizeof(long);
	long *atoms = (long *)data.data();
	for (int i = 0; i < n_atoms; i++) {
		Atom target = atoms[i];
		D(bug("  target %08x (%s)\n", target, XGetAtomName(x_display, target)));
		switch (type) {
		case FOURCC('T','E','X','T'):
			D(bug(" clipping TEXT\n"));
			if (target == XA_STRING)
				format = target;
			break;
		case FOURCC('P','I','C','T'):
			break;
		}
	}
#else
	switch (type) {
	case FOURCC('T','E','X','T'):
		D(bug(" clipping TEXT\n"));
		format = XA_STRING;
		break;
	case FOURCC('P','I','C','T'):
		break;
	}
#endif
	if (format == None)
		return;

	// Get the native clipboard data
	XConvertSelection(x_display, xa_clipboard, format, xa_clipboard, clip_win, CurrentTime);
	if (!wait_for_selection_notify_event(x_display, clip_win, &event, SELECTION_MAX_WAIT) ||
		event.xselection.property == None ||
		!read_property(x_display,
					   event.xselection.requestor, event.xselection.property,
					   false, data, 0, 0, 0, format == XA_STRING))
		return;

	// Allocate space for new scrap in MacOS side
	M68kRegisters r;
	r.d[0] = data.size();
	Execute68kTrap(0xa71e, &r);			// NewPtrSysClear()
	uint32 scrap_area = r.a[0];

	if (scrap_area) {
		switch (type) {
		case FOURCC('T','E','X','T'):
			// Convert text from ISO-Latin1 to Mac charset
			uint8 *p = Mac2HostAddr(scrap_area);
			for (int i = 0; i < data.size(); i++) {
				uint8 c = data[i];
				if (c < 0x80) {
					if (c == 10)	// LF -> CR
						c = 13;
				} else if (!no_clip_conversion)
					c = iso2mac[c & 0x7f];
				*p++ = c;
			}
			break;
		}

		// Add new data to clipboard
		static uint8 proc[] = {
			0x59, 0x8f,					// subq.l	#4,sp
			0xa9, 0xfc,					// ZeroScrap()
			0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#length,-(sp)
			0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#type,-(sp)
			0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#outbuf,-(sp)
			0xa9, 0xfe,					// PutScrap()
			0x58, 0x8f,					// addq.l	#4,sp
			M68K_RTS >> 8, M68K_RTS
		};
		r.d[0] = sizeof(proc);
		Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
		uint32 proc_area = r.a[0];

		// The procedure is run-time generated because it must lays in
		// Mac address space. This is mandatory for "33-bit" address
		// space optimization on 64-bit platforms because the static
		// proc[] array is not remapped
		Host2Mac_memcpy(proc_area, proc, sizeof(proc));
		WriteMacInt32(proc_area +  6, data.size());
		WriteMacInt32(proc_area + 12, type);
		WriteMacInt32(proc_area + 18, scrap_area);
		we_put_this_data = true;
		Execute68k(proc_area, &r);

		// We are done with scratch memory
		r.a[0] = proc_area;
		Execute68kTrap(0xa01f, &r);		// DisposePtr
		r.a[0] = scrap_area;
		Execute68kTrap(0xa01f, &r);		// DisposePtr
	}
}


/*
 *	Handle X11 selection events
 */

void ClipboardSelectionClear(XSelectionClearEvent *xev)
{
	if (xev->selection != xa_clipboard)
		return;

	D(bug("Selection cleared, lost clipboard ownership\n"));
	clip_data.type = None;
	clip_data.data.clear();
}

// Top level selection handler
static bool handle_selection(XSelectionRequestEvent *req, bool is_multiple);

static bool handle_selection_TIMESTAMP(XSelectionRequestEvent *req)
{
	// 32-bit integer values are always passed as a long whatever is its size
	long timestamp = clip_data.time;

	// Change requestor property
	XChangeProperty(x_display, req->requestor, req->property,
					XA_INTEGER, 32,
					PropModeReplace, (uint8 *)&timestamp, 1);

	return true;
}

static bool handle_selection_TARGETS(XSelectionRequestEvent *req)
{
	// Default supported targets
	vector<long> targets;
	targets.push_back(xa_targets);
	targets.push_back(xa_multiple);
	targets.push_back(xa_timestamp);

	// Extra targets matchin current clipboard data
	if (clip_data.type != None)
		targets.push_back(clip_data.type);

	// Change requestor property
	XChangeProperty(x_display, req->requestor, req->property,
					xa_targets, 32,
					PropModeReplace, (uint8 *)&targets[0], targets.size());

	return true;
}

static bool handle_selection_STRING(XSelectionRequestEvent *req)
{
	// Make sure we have valid data to send though ICCCM compliant
	// clients should have first requested TARGETS to identify
	// this possibility.
	if (clip_data.type != XA_STRING)
		return false;

	// Send the string, it's already encoded as ISO-8859-1
	XChangeProperty(x_display, req->requestor, req->property,
					XA_STRING, 8,
					PropModeReplace, (uint8 *)clip_data.data.data(), clip_data.data.size());

	return true;
}

static bool handle_selection_MULTIPLE(XSelectionRequestEvent *req)
{
	Atom rtype;
	int rformat;
	ByteArray data;

	if (!read_property(x_display, req->requestor, req->property,
					  false, data, 0, &rtype, &rformat, 0))
		return false;

	// rtype should be ATOM_PAIR but some clients don't honour that
	if (rformat != 32)
		return false;

	struct AtomPair { long target; long property; };
	AtomPair *atom_pairs = (AtomPair *)data.data();
	int n_atom_pairs = data.size() / sizeof(AtomPair);

	bool handled = true;
	if (n_atom_pairs) {
		// Setup a new XSelectionRequestEvent when servicing individual requests
		XSelectionRequestEvent event;
		memcpy(&event, req, sizeof(event));

		for (int i = 0; i < n_atom_pairs; i++) {
			Atom target = atom_pairs[i].target;
			Atom property = atom_pairs[i].property;

			// None is not a valid property
			if (property == None)
				continue;

			// Service this request
			event.target = target;
			event.property = property;
			if (!handle_selection(&event, true)) {
				/* FIXME: ICCCM 2.6.2:

				   If the owner fails to convert the target named by an
				   atom in the MULTIPLE property, it should replace that
				   atom in the property with None.
				*/
				handled = false;
			}
		}
	}

	return handled;
}

static bool handle_selection(XSelectionRequestEvent *req, bool is_multiple)
{
	bool handled =false;

	if (req->target == xa_timestamp)
		handled = handle_selection_TIMESTAMP(req);
	else if (req->target == xa_targets)
		handled = handle_selection_TARGETS(req);
	else if (req->target == XA_STRING)
		handled = handle_selection_STRING(req);
	else if (req->target == xa_multiple)
		handled = handle_selection_MULTIPLE(req);

	// Notify requestor only when we are done with his request
	if (handled && !is_multiple) {
		XEvent out_event;
		out_event.xselection.type      = SelectionNotify;
		out_event.xselection.requestor = req->requestor;
		out_event.xselection.selection = req->selection;
		out_event.xselection.target    = req->target;
		out_event.xselection.property  = handled ? req->property : None;
		out_event.xselection.time      = req->time;
		XSendEvent(x_display, req->requestor, False, 0, &out_event);
	}

	return handled;
}

void ClipboardSelectionRequest(XSelectionRequestEvent *req)
{
	if (req->requestor == clip_win || req->selection != xa_clipboard)
		return;

	D(bug("Selection requested from 0x%lx to 0x%lx (%s) 0x%lx (%s)\n",
		  req->requestor,
		  req->selection,
		  XGetAtomName(req->display, req->selection),
		  req->target,
		  XGetAtomName(req->display, req->target)));

	handle_selection(req, false);
}
