/*
 *	PrefsEditor.h - GUI stuff for Basilisk II preferences
 *					(which is a text file in the user's home directory)
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

#import <Cocoa/Cocoa.h>

@interface TableDS : NSObject
{
	int				numItems;
	NSMutableArray	*col1,
					*col2;
}

- (void) addInt: (int)target
	   withPath: (NSString *)path;

- (void) addObject: (NSObject *)obj
		  withPath: (NSString *)path;

- (void) deleteAll;

- (BOOL) deleteRow: (int)row;

- (int) intAtRow: (int)row;

- (int) numberOfRowsInTableView: (NSTableView *)tView;

- (NSString *) pathAtRow: (int)row;

- (id)	tableView: (NSTableView *)tView
		objectValueForTableColumn: (NSTableColumn *)tColumn
		row: (int)rowIndex;
@end

#include "Emulator.h"

@interface PrefsEditor : NSObject
{
	IBOutlet NSSlider		*emuFreq;
	IBOutlet NSView			*newVolumeView;
	IBOutlet NSTextField	*newVolumeSize;
	IBOutlet NSWindow		*panel;
	IBOutlet Emulator		*theEmulator;


	IBOutlet NSButton		*bootFromAny,
							*bootFromCD;
	IBOutlet NSTextField	*bytes;
	IBOutlet NSButton		*classic,
							*CPU68000,
							*CPU68020,
							*CPU68030,
							*CPU68040;
	IBOutlet NSTextField	*delay,
							*depth;
	IBOutlet NSButton		*disableCD,
							*disableSound;
	IBOutlet NSTableView	*diskImages;
	IBOutlet NSTextField	*etherNet,
							*extFS;
	IBOutlet NSButton		*FPU;
	IBOutlet NSTextField	*frequency,
							*height;
    IBOutlet NSButton		*IIci;
	IBOutlet NSPopUpButton	*keyboard;
	IBOutlet NSTextField	*MB,
							*modem;
	IBOutlet NSButton		*openGL;
	IBOutlet NSTextField	*prefsFile,
							*printer;
	IBOutlet NSButton		*quadra900;
	IBOutlet NSTextField	*ROMfile;
	IBOutlet NSButton		*screen;
	IBOutlet NSTableView	*SCSIdisks;
	IBOutlet NSTextField	*width;
	IBOutlet NSButton		*window;

	NSString	*devs,
				*home;

	TableDS		*volsDS,	// Object managing tha data in the Volumes,
				*SCSIds;	// and SCSI devices, tables

	NSImage		*locked,
				*blank;
	NSImageCell	*lockCell;

	BOOL		edited;		// Set if the user changes anything, reset on save
}

- (BOOL) hasEdited;
- (NSWindow *) window;

- (IBAction) AddSCSI:		(id)sender;
- (IBAction) AddVolume:		(id)sender;
- (IBAction) BrowseExtFS:	(id)sender;
- (IBAction) BrowsePrefs:	(id)sender;
- (IBAction) BrowseROM:		(id)sender;
- (IBAction) ChangeBootFrom:	(NSMatrix *)sender;
- (IBAction) ChangeCPU:			(NSMatrix *)sender;
- (IBAction) ChangeDisableCD:	(NSButton *)sender;
- (IBAction) ChangeDisableSound:(NSButton *)sender;
- (IBAction) ChangeFPU:			(NSButton *)sender;
- (IBAction) ChangeKeyboard:	(NSPopUpButton *)sender;
- (IBAction) ChangeModel:		(NSMatrix *)sender;
- (IBAction) ChangeScreen:	(id)sender;
- (IBAction) CreateVolume:	(id)sender;
- (IBAction) DeleteVolume:	(id)sender;
- (IBAction) EditBytes:			(NSTextField *)sender;
- (IBAction) EditDelay:			(NSTextField *)sender;
- (IBAction) EditEtherNetDevice:(NSTextField *)sender;
- (IBAction) EditExtFS:			(NSTextField *)sender;
- (IBAction) EditFrequency:		(NSTextField *)sender;
- (IBAction) EditMB:			(NSTextField *)sender;
- (IBAction) EditModemDevice:	(NSTextField *)sender;
- (IBAction) EditPrinterDevice:	(NSTextField *)sender;
- (IBAction) EditROMpath:		(NSTextField *)sender;
- (IBAction) LoadPrefs:		(id)sender;
- (IBAction) RemoveSCSI:	(id)sender;
- (IBAction) RemoveVolume:	(id)sender;
- (NSString *) RemoveVolumeEntry;
- (IBAction) ResetPrefs:	(id)sender;
- (IBAction) ShowPrefs: 	(id)sender;
- (IBAction) SavePrefs:		(id)sender;

@end