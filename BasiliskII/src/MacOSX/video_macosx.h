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
//#define CGIMAGEREF		15.05fps
//#define CGDRAWBITMAP		15.2fps
#define NSBITMAP			15.1fps

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
				init_depth,
				screen_height;

extern	uint8	bits_from_depth		(const video_depth);
extern	bool	parse_screen_prefs	(const char *);
extern	void	resizeWinTo			(const uint16, const uint16);

#import <AppKit/NSWindow.h>
#import "EmulatorView.h"

extern	NSWindow		*the_win;
extern	EmulatorView	*output;

// These record changes we made in setting full screen mode
extern	CGDirectDisplayID	theDisplay;
extern	CFDictionaryRef		originalMode, newMode;

// Macro for checking if full screen mode has started
#define FULLSCREEN	( theDisplay || originalMode || newMode )
