/*
 *	video_macosx.h - Some video constants and globals
 *
 *	$Id$
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

#import <video.h>

/* Set the strategy for drawing the bitmap in the Mac OS X window */
#define CGDRAWBITMAP
//#define CGIMAGEREF
//#define NSBITMAP

// The frames-per-second benchmark function on my machine does roughly:
//
//					OS:	10.1.5		10.2.2
// CGDRAWBITMAP			15.2		36.6
// CGIMAGEREF			15.0		27-135(i)
// NSBITMAP				15.1		26.9
//
// (i) This seems to vary wildly between different builds on the same machine.
//	   I don't know why, but I definately don't trust it. Recently I noticed
//	   that it also varies by alpha channel strategy:
//		kCGImageAlphaNone				36.6fps
//		kCGImageAlphaPremultipliedFirst	112fps
//		kCGImageAlphaNoneSkipFirst		135fps

/* When the BasiliskII video driver respects the alpha bits, set this to let us use */
/* kCGImageAlphaPremultipliedFirst, and to have nice rounded corners on the screen. */
//#define CG_USE_ALPHA
/* At the moment, it writes in the fill 32bits :-( */


#define MIN_WIDTH	512
#define MIN_HEIGHT	342

#define MAX_WIDTH	1240
#define MAX_HEIGHT	1024

// Display types
enum
{
	DISPLAY_OPENGL,
	DISPLAY_SCREEN,
	DISPLAY_WINDOW
};


extern	uint8	display_type,
				frame_skip;
extern	uint16	init_width,
				init_height,
				init_depth;

extern	bool	parse_screen_prefs	(const char *);
extern	void	resizeWinTo			(const uint16, const uint16);

#import <AppKit/NSWindow.h>
#import "EmulatorView.h"

extern	NSWindow		*the_win;
extern	EmulatorView	*output;
