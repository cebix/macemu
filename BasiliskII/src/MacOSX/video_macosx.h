/*
 *	video_macosx.h - Some video constants and globals
 *
 *	$Id$
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

#import <video.h>

// Using Core Graphics is fastest when rendering 32bit data.
// Using CGImageRefs allows us to use all the bitmaps that BasiliskII supports.
// When both Basilisk II and OS X are set to 'Thousands', updating a 312x342
// window happens at over 500fps under 10.2, and over 600fps on 10.3!

/* When the BasiliskII video driver respects the alpha bits, set this to let us use */
/* kCGImageAlphaPremultipliedFirst, and to have nice rounded corners on the screen. */
//#define CG_USE_ALPHA
/* At the moment, it writes in the full 32bits :-( */


#define MIN_WIDTH	512
#define MIN_HEIGHT	384
#define MIN_HEIGHTC	342		// For classic emulation

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
