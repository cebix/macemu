/*
 *	PrefsEditor.m - GUI stuff for Basilisk II preferences
 *					(which is a text file in the user's home directory)
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

#import "PrefsEditor.h"

@implementation TableDS

- (TableDS *) init
{
	self = [super init];

	numItems = 0;
	col1 = [[NSMutableArray alloc] init];
	col2 = [[NSMutableArray alloc] init];

	return self;
}

- (void) dealloc
{
	[col1  dealloc];
	[col2  dealloc];
	[super dealloc];
}

- (void) addInt: (int)target
	   withPath: (NSString *)path
{
	[col1 addObject: [NSNumber numberWithInt: target]];
	[col2 addObject: path];
	++numItems;
}

- (void) addObject: (NSObject *)obj
		  withPath: (NSString *)path
{
	[col1 addObject: obj];
	[col2 addObject: path];
	++numItems;
}

- (void) deleteAll
{
	numItems = 0;
	[col1 removeAllObjects];
	[col2 removeAllObjects];
}

- (BOOL) deleteRow: (int)row
{
	if ( row > numItems )
		return NO;

	[col1 removeObjectAtIndex: row];
	[col2 removeObjectAtIndex: row];
	-- numItems;

	return YES;
}

- (int)intAtRow: (int)row
{
	return [[col1 objectAtIndex: row] intValue];
}

- (int) numberOfRowsInTableView: (NSTableView *)tView
{
	return numItems;
}

- (NSString *)pathAtRow: (int)row
{
	return (NSString *) [col2 objectAtIndex: row];
}

- (id)	tableView: (NSTableView *)tView
		objectValueForTableColumn: (NSTableColumn *)tColumn
		row: (int)row
{
	if ( [[tColumn identifier] isEqualToString:@"path"] )
		return [col2 objectAtIndex: row];
	else
		return [col1 objectAtIndex: row];
}

@end

@implementation PrefsEditor

#import <AppKit/NSImage.h>		// For [NSBundle pathForImageResource:] proto

#import "sysdeps.h"				// Types used in Basilisk C++ code
#import "video_macosx.h"		// some items that we edit here
#import "misc_macosx.h"			// WarningSheet() prototype

#import <prefs.h>

#define DEBUG 1
#import <debug.h>

- (PrefsEditor *) init
{
	self = [super init];

	edited = NO;

	devs = @"/dev";
	home = NSHomeDirectory();
	volsDS = [[TableDS alloc] init];
	SCSIds = [[TableDS alloc] init];

	lockCell = [[NSImageCell alloc] init];
	if ( lockCell == nil )
		NSLog (@"%s - Can't create NSImageCell?", __PRETTY_FUNCTION__);

	blank  = [[NSImage alloc] init];
	locked = [NSImage alloc];
	if ( [locked initWithContentsOfFile:
				 [[NSBundle mainBundle]
						pathForImageResource: @"nowrite.icns"]] == nil )
		NSLog(@"%s - Couldn't open write protection image", __PRETTY_FUNCTION__);

	return self;
}

- (void) dealloc
{
	[volsDS   dealloc];
	[SCSIds   dealloc];
	[lockCell dealloc];
	[locked   dealloc];
	[blank    dealloc];
	[super    dealloc];
}

- (void) awakeFromNib
{
	emuFreq = [theEmulator speed];
#if DEBUG
	[self ShowPrefs: self];			// For testing
#endif
}

- (BOOL) hasEdited
{
	return edited;
}

- (NSWindow *) window
{
	return panel;
}

- (IBAction) AddSCSI: (id)sender
{
	NSOpenPanel *oP = [NSOpenPanel openPanel];

	if ( [oP runModalForDirectory:home file:nil types:nil] == NSOKButton )
	{
		[SCSIds addInt: -1
			  withPath: [oP filename] ];
		[SCSIdisks reloadData];
		edited = YES;
	}
}

- (IBAction) AddVolume: (id)sender
{
	NSOpenPanel *oP = [NSOpenPanel openPanel];

	if ( [oP runModalForDirectory:home file:nil types:nil] == NSOKButton )
	{
		[volsDS addObject: (NSObject *) locked
				 withPath: [oP filename] ];
		PrefsAddString("disk", [[oP filename] cString]);
		[diskImages reloadData];
		edited = YES;
	}
}

- (IBAction) BrowseExtFS:	(id)sender
{
	NSOpenPanel *oP = [NSOpenPanel openPanel];

	[oP setCanChooseDirectories: YES];
	[oP setCanChooseFiles: NO];
	[oP setPrompt: @"Select"];
	[oP setTitle:  @"Select a directory to mount"];
	D(NSLog(@"%s - home = %@, [extFS stringValue] = %@",
			__PRETTY_FUNCTION__, home, [extFS stringValue]));
	if ( [oP runModalForDirectory: ([extFS stringValue] ? [extFS stringValue] : home)
							 file:nil
							types:nil] == NSOKButton )
	{
		[extFS setStringValue: [oP directory] ];
		PrefsReplaceString("extfs", [[oP directory] cString]);
		edited = YES;
	}
}

- (IBAction) BrowseROM:		(id)sender
{
	NSOpenPanel *oP = [NSOpenPanel openPanel];

	[oP setCanChooseFiles: YES];
	[oP setTitle:  @"Open a ROM file"];
	D(NSLog(@"%s - home = %@", __PRETTY_FUNCTION__, home));
	if ( [oP runModalForDirectory: ([ROMfile stringValue] ? [ROMfile stringValue] : home)
							 file:nil
							types:nil] == NSOKButton )
	{
		[ROMfile setStringValue: [oP filename] ];
		PrefsReplaceString("rom", [[oP filename] cString]);
		edited = YES;
	}
}

#include <cdrom.h>			// for CDROMRefNum

- (IBAction) ChangeBootFrom: (NSMatrix *)sender
{
	if ( [sender selectedCell] == bootFromCD )
		PrefsReplaceInt32("bootdriver", CDROMRefNum);
	else
		PrefsReplaceInt32("bootdriver", 0);
	edited = YES;
}

- (IBAction) ChangeCPU: (NSMatrix *)sender
{
	PrefsReplaceInt32("cpu", [[sender selectedCell] tag]);
	edited = YES;
}

- (IBAction) ChangeDisableCD: (NSButton *)sender
{
	PrefsReplaceBool("nocdrom", [disableCD state]);
	edited = YES;
}

- (IBAction) ChangeDisableSound: (NSButton *)sender
{
	BOOL	noSound = [disableSound state];

	if ( ! noSound )
		WarningSheet(@"Sound is currently unimplemented", panel);

	PrefsReplaceBool("nosound", noSound);
	edited = YES;
}

- (IBAction) ChangeFPU: (NSButton *)sender
{
	PrefsReplaceBool("fpu", [FPU state]);
	edited = YES;
}

- (IBAction) ChangeModel: (NSMatrix *)sender
{
	PrefsReplaceInt32("modelid", [[sender selectedCell] tag]);
	edited = YES;
}

// Screen/window changing stuff

// This is called when any of the screen/window, width, height or depth is changed

- (IBAction) ChangeScreen: (id)sender
{
	short newx		= [width  intValue];
	short newy		= [height intValue];
	short newbpp	= [depth  intValue];
	short newtype;
	char  str[20];

	if ( [openGL state] )
		newtype = DISPLAY_OPENGL;
	else if ( [screen state] )
		newtype = DISPLAY_SCREEN;
	else if ( [window state] )
		newtype = DISPLAY_WINDOW;
	else
		newtype = display_type;

	// Check that a field actually changed
	if ( newbpp == init_depth && newx == init_width &&
		 newy == init_height && newtype == display_type )
	{
		NSLog(@"No changed GUI items in ChangeScreen");
		return;
	}

	// If we are changing type, supply some sensible defaults
	if ( newtype != display_type )
	{
		NSLog(@"Changing disylay type in ChangeScreen");
		if ( newtype == DISPLAY_SCREEN )		// If changing to full screen
		{
			// supply main screen dimensions as a default
			NSScreen	*s = [NSScreen mainScreen];
			NSRect		sr = [s frame];

			newx = (short) sr.size.width;
			newy = (short) sr.size.height;
			// This always returns 24, despite the mode
			//newbpp = NSBitsPerPixelFromDepth([s depth]);
			newbpp = CGDisplayBitsPerPixel(kCGDirectMainDisplay);
		}

		if ( display_type == DISPLAY_SCREEN )	// If changing from full screen
			newx = MIN_WIDTH, newy = MIN_HEIGHT;

		[width	setIntValue: newx];
		[height setIntValue: newy];
		[depth	setIntValue: newbpp];
	}
	else
	{
		// Check size is within ranges of MIN_WIDTH ... MAX_WIDTH
		//							and MIN_HEIGHT ... MAX_HEIGHT
		// ???
	}


	// Store new prefs
	*str = '\0';
	switch ( newtype )
	{
		case DISPLAY_WINDOW:
			if ( newbpp )
				sprintf(str, "win/%hd/%hd/%hd",  newx, newy, newbpp);
			else
				sprintf(str, "win/%hd/%hd",  newx, newy);
			break;
		case DISPLAY_OPENGL:
			if ( newbpp )
				sprintf(str, "opengl/%hd/%hd/%hd",  newx, newy, newbpp);
			else
				sprintf(str, "opengl/%hd/%hd",  newx, newy);
			break;
		case DISPLAY_SCREEN:
			if ( newbpp )
				sprintf(str, "full/%hd/%hd/%hd", newx, newy, newbpp);
			else
				sprintf(str, "full/%hd/%hd", newx, newy);
			break;
	};
	PrefsReplaceString("screen", str);

	parse_screen_prefs(str);

	edited = YES;

	if ( display_type != DISPLAY_SCREEN )
	{
		NSLog(@"Display type is not SCREEN (%d), resizing window", display_type);
		resizeWinTo(newx, newy);
	}
}

- (IBAction) CreateVolume: (id)sender
{
	NSSavePanel *sP = [NSSavePanel savePanel];

	[sP setAccessoryView: newVolumeView];
	[sP setPrompt:        @"Create"];
	[sP setTitle:         @"Create new volume as"];

	if ( [sP runModalForDirectory:NSHomeDirectory() file:@"basilisk-II.vol"] == NSOKButton )
	{
		char		cmd[1024];
		const char	*filename = [[sP filename] cString];
		int			retVal,
					size = [newVolumeSize intValue];

		sprintf(cmd, "dd if=/dev/zero \"of=%s\" bs=1024k count=%d", filename, size);
		retVal = system(cmd);
		if (retVal != 0)
		{
			NSString *details = [NSString stringWithFormat:
								 @"The dd command failed.\nReturn status %d (%s)",
								 retVal, strerror(errno)];
			WarningSheet(@"Unable to create volume", details, nil, panel);
		}
		else
		{
			[volsDS addObject: (NSObject *) blank
					 withPath: [sP filename] ];
			PrefsAddString("disk", filename);
			[diskImages reloadData];
		}
	}
}

- (IBAction) DeleteVolume: (id)sender
{
	const char *path = [self RemoveVolumeEntry];
	if ( unlink(path) == -1 )
	{
		NSLog(@"%s unlink(%s) failed", __PRETTY_FUNCTION__, path, strerror(errno));
	}
}

- (IBAction) EditDelay: (NSTextField *)sender
{
	int		ticks = [delay intValue];
	float	freq;

	if ( ticks )
		freq = 60.0 / ticks;
	else
		freq = 60.0;

	[frequency	setFloatValue: freq];
	[emuFreq	setFloatValue: freq];
	PrefsReplaceInt32("frameskip", ticks);
	edited = YES;
}

- (IBAction) EditBytes: (NSTextField *)sender
{
	int		B = (int) [bytes floatValue];
	float	M = B / 1024 / 1024;

	D(NSLog(@"%s = %f %d", __PRETTY_FUNCTION__, M, B));
	PrefsReplaceInt32("ramsize", B);
	[MB setFloatValue: M];
	edited = YES;
}

- (IBAction) EditEtherNetDevice: (NSTextField *)sender
{
	NSString	*path = [etherNet stringValue];

	PrefsReplaceString("ether", [path cString]);
	edited = YES;
}

- (IBAction) EditExtFS: (NSTextField *)sender
{
	NSString	*path = [extFS stringValue];

	PrefsReplaceString("extfs", [path cString]);
	edited = YES;
}

- (IBAction) EditFrequency: (NSTextField *)sender
{
	float freq = [frequency floatValue];

	[delay		setIntValue:   frequencyToTickDelay(freq)];
	[emuFreq	setFloatValue: freq];
	edited = YES;
}

- (IBAction) EditModemDevice: (NSTextField *)sender
{
	NSString	*path = [modem stringValue];

	PrefsReplaceString("seriala", [path cString]);
	edited = YES;
}

- (IBAction) EditMB: (NSTextField *)sender
{
	float	M = [MB floatValue];
	int		B = (int) (M * 1024 * 1024);

	D(NSLog(@"%s = %f %d", __PRETTY_FUNCTION__, M, B));
	PrefsReplaceInt32("ramsize", B);
	[bytes setIntValue: B];
	edited = YES;
}

- (IBAction) EditPrinterDevice: (NSTextField *)sender
{
	NSString	*path = [printer stringValue];

	PrefsReplaceString("serialb", [path cString]);
	edited = YES;
}

- (IBAction) EditROMpath: (NSTextField *)sender
{
	NSString	*path = [ROMfile stringValue];

	PrefsReplaceString("rom", [path cString]);
}

- (IBAction) RemoveSCSI: (id)sender
{
	char	pref[6];
	int 	row    = [SCSIdisks selectedRow],
			SCSIid = [SCSIds intAtRow: row];

	if ( ! [SCSIds deleteRow: row] )
		NSLog (@"%s - [SCSIds deleteRow: %d] failed", __PRETTY_FUNCTION__, row);
	[SCSIdisks reloadData];
	sprintf(pref, "scsi%d", SCSIid);
	PrefsRemoveItem(pref,0);
}

- (const char *) RemoveVolumeEntry
{
	int		row = [diskImages selectedRow];

	if ( row != -1 )
	{
		const char	*path = [[volsDS pathAtRow: row] cString],
					*str;
		int			tmp = 0;

		while ( (str = PrefsFindString("disk", tmp) ) != NULL )
		{
			if ( strcmp(str, path) == 0 )
			{
				PrefsRemoveItem("disk", tmp);
				D(NSLog(@"%s - Deleted prefs entry \"disk\", %d", __PRETTY_FUNCTION__, tmp));
				edited = YES;
				break;
			}
			++tmp;
		}

		if ( str == NULL )
		{
			NSLog(@"%s - Couldn't find any disk preference to match %s", __PRETTY_FUNCTION__, path);
			return NULL;
		}

		if ( ! [volsDS deleteRow: row] )
			NSLog (@"%s - RemoveVolume %d failed", __PRETTY_FUNCTION__, tmp);
		[diskImages reloadData];
		return path;
	}
	else
	{
		WarningSheet(@"Please select a volume first", panel);
		return NULL;
	}
}

- (IBAction) RemoveVolume: (id)sender
{
	[self RemoveVolumeEntry];
}

- (IBAction) ResetPrefs: (id)sender
{
	int		argc = 0;
	char	**argv = NULL;

	[panel close];				// Temporarily hide preferences panel

	PrefsExit();				// Purge all the old pref values

	PrefsInit(argc, argv);
	AddPrefsDefaults();
	AddPlatformPrefsDefaults();	// and only create basic ones

	[SCSIds deleteAll];			// Clear out datasources for the tables
	[volsDS deleteAll];

	[self ShowPrefs: self];		// Reset items in panel, and redisplay
	edited = NO;
}

- (void) setStringOf: (NSTextField *) field
			fromPref: (const char *)  prefName
{
	const char	*value = PrefsFindString(prefName, 0);

	if ( value )
		[field setStringValue: [NSString stringWithCString: value] ];
}

- (IBAction) SavePrefs: (id)sender
{
	SavePrefs();
	edited = NO;
}

- (IBAction) ShowPrefs: (id)sender
{
	NSTableColumn	*locks;
	const char		*str;
	int				cpu, tmp;


	// Set simple single field items

	tmp = PrefsFindInt32("frameskip");
	[delay setIntValue: tmp];
	if ( tmp )
		[frequency	setFloatValue:	60.0 / tmp];
	else
		[frequency	setFloatValue:	60.0];

	tmp = PrefsFindInt32("ramsize");
	[bytes	setIntValue:   tmp];
	[MB		setFloatValue: tmp / (1024.0 * 1024.0)];

	[disableCD		setState:	PrefsFindBool("nocdrom")];
	[disableSound	setState:	PrefsFindBool("nosound")];
	[FPU			setState:	PrefsFindBool("fpu")	];

	[self setStringOf: etherNet	 fromPref: "ether"	];
	[self setStringOf: extFS	 fromPref: "extfs"	];
	[self setStringOf: modem	 fromPref: "seriala"];
	[self setStringOf: printer	 fromPref: "serialb"];
    [self setStringOf: ROMfile	 fromPref: "rom"	];


	parse_screen_prefs(PrefsFindString("screen"));

	[width	setIntValue: init_width];
	[height setIntValue: init_height];
	[depth	setIntValue: init_depth];

	[window setState: NO];
	switch ( display_type )
	{
		case DISPLAY_WINDOW: [window setState: YES]; break;
		case DISPLAY_OPENGL: [openGL setState: YES]; break;
		case DISPLAY_SCREEN: [screen setState: YES]; break;
	}

	[newVolumeSize setIntValue: 10];

	// Radio button groups:

	tmp = PrefsFindInt32("bootdriver");
	[bootFromAny setState: tmp != CDROMRefNum];
	[bootFromCD  setState: tmp == CDROMRefNum];

	cpu = PrefsFindInt32("cpu");
	tmp = PrefsFindInt32("modelid");

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	puts("Current memory model does not support 24bit addressing");
	if ( tmp == [classic tag] )
	{
		// Window already created by NIB file, just display
		[panel makeKeyAndOrderFront:self];
		WarningSheet(@"Compiled-in memory model does not support 24bit",
						@"Disabling Mac Classic emulation", nil, panel);
		cpu = [CPU68030 tag];
		PrefsReplaceInt32("cpu", cpu);
		tmp = [IIci tag];
		PrefsReplaceInt32("modelid", tmp);
	}

	puts("Disabling 68000 & Mac Classic buttons");
	[CPU68000 setEnabled:FALSE];
	[classic  setEnabled:FALSE];
#endif

	[CPU68000   setState: [CPU68000  tag] == cpu];
	[CPU68020   setState: [CPU68020  tag] == cpu];
	[CPU68030   setState: [CPU68030  tag] == cpu];
	[CPU68040   setState: [CPU68040  tag] == cpu];

	[classic	setState: [classic	 tag] == tmp];
	[IIci		setState: [IIci		 tag] == tmp];
	[quadra900	setState: [quadra900 tag] == tmp];


	// Lists of thingies:

	for ( tmp = 0; tmp < 7; ++tmp)
	{
		char pref[6];

		pref[0] = '\0';

		sprintf (pref, "scsi%d", tmp);
		if ( (str = PrefsFindString(pref, 0) ) )
			[SCSIds addInt: tmp
				  withPath: [NSString stringWithCString: str] ];
	}

	[SCSIdisks setDataSource: SCSIds];

	locks = [diskImages tableColumnWithIdentifier: @"locked"];
	if ( locks == nil )
		NSLog (@"%s - Can't find column for lock images", __PRETTY_FUNCTION__);
	[locks setDataCell: lockCell];

	tmp = 0;
	while ( (str = PrefsFindString("disk", tmp++) ) != NULL )
	{
		if ( *str == '*' )
			[volsDS addObject: (NSObject *) locked
					 withPath: [NSString stringWithCString: str+1]];
		else
			[volsDS addObject: (NSObject *) blank
					 withPath: [NSString stringWithCString: str]];
	}

	[diskImages setDataSource: volsDS];


	[panel makeKeyAndOrderFront:self];		// Window already created by NIB file, just display
}

@end
