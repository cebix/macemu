/*
 *	$Id$
 *
 *	misc_macosx.h - Some prototypes of functions defined in misc_macosx.mm
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

#if defined(__APPLE__) && defined(__MACH__)
	// This means we are on Mac OS X of some sort
#endif

extern void ErrorSheet	 (NSString *msg,	NSWindow *win),
			ErrorSheet	 (NSString *msg1,	NSString *msg2,
						  NSString *button,	NSWindow *win),
			WarningSheet (NSString *message,NSWindow *win),
			WarningSheet (NSString *msg1,	NSString *msg2,
						  NSString *button,	NSWindow *win),
			InfoSheet	 (NSString *msg,	NSWindow *win),
			InfoSheet	 (NSString *msg1,	NSString *msg2,
						  NSString *button,	NSWindow *win),
			EndSheet (NSWindow * window);

extern int	frequencyToTickDelay (float frequency);
