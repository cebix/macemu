/*
 *	PrefsEditor.m - GUI stuff for Basilisk II preferences
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

#import "PrefsEditor.h"

@implementation TableDS

- (TableDS *) init
{
	self = [super init];

	numItems = 0;
	col1 = [NSMutableArray new];
	col2 = [NSMutableArray new];

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

#import <AppKit/NSImage.h>		// For [NSBundle pathForImageResource:] proto

#include <string>
using std::string;
extern string UserPrefsPath;	// from prefs_unix.cpp

#import "sysdeps.h"				// Types used in Basilisk C++ code
#import "video_macosx.h"		// some items that we edit here
#import "misc_macosx.h"			// WarningSheet() prototype
#import "main_macosx.h"			// ChoiceAlert() prototype


#import <prefs.h>

#define DEBUG 0
#import <debug.h>

@implementation PrefsEditor

- (PrefsEditor *) init
{
	self = [super init];

	edited = NO;

	devs = @"/dev";
	home = [NSHomeDirectory() retain];
	volsDS = [TableDS new];
	SCSIds = [TableDS new];

	lockCell = [NSImageCell new];
	if ( lockCell == nil )
		NSLog (@"%s - Can't create NSImageCell?", __PRETTY_FUNCTION__);

	blank  = [NSImage new];
	locked = [[NSImage alloc] initWithContentsOfFile:
				[[NSBundle mainBundle] pathForImageResource: @"nowrite.icns"]];
	if (locked == nil )
		NSLog(@"%s - Couldn't open write protection image", __PRETTY_FUNCTION__);

	return self;
}

- (void) dealloc
{
	[home     release];
	[volsDS   release];
	[SCSIds   release];
	[lockCell release];
	[blank    release];
	[locked   release];
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
		PrefsAddString("disk", [[oP filename] UTF8String]);
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
		PrefsReplaceString("extfs", [[oP directory] UTF8String]);
		edited = YES;
	}
}

- (IBAction) BrowsePrefs:		(id)sender
{
	NSOpenPanel *oP = [NSOpenPanel openPanel];

	[oP setCanChooseFiles: YES];
	[oP setTitle:  @"Select a Preferences file"];
	D(NSLog(@"%s - home = %@", __PRETTY_FUNCTION__, home));
	if ( [oP runModalForDirectory: ([prefsFile stringValue] ? [prefsFile stringValue] : home)
							 file:nil
							types:nil] == NSOKButton )
	{
		[prefsFile setStringValue: [oP filename] ];
		UserPrefsPath = [[oP filename] UTF8String];
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
		PrefsReplaceString("rom", [[oP filename] UTF8String]);
		edited = YES;
	}
}

#include <cdrom.h>			// for CDROMRefNum

- (IBAction) ChangeBootFrom: (NSMatrix *)sender
{
	if ( [sender selectedCell] == (id)bootFromCD )
	{
		[disableCD setState: NSOffState];

		PrefsReplaceInt32("bootdriver", CDROMRefNum);
	}
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
	int disabled = [disableCD state];

	PrefsReplaceBool("nocdrom", disabled);
	if ( disabled )
	{
		[bootFromAny setState: NSOnState];
		[bootFromCD setState: ![disableCD state]];
	}
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

- (IBAction) ChangeKeyboard: (NSPopUpButton *)sender
{
	// Deselest current item
	int  current = [keyboard indexOfItemWithTag: PrefsFindInt32("keyboardtype")];
	if ( current != -1 )
		[[keyboard itemAtIndex: current] setState: FALSE];

	PrefsReplaceInt32("keyboardtype", [[sender selectedItem] tag]);
	edited = YES;
}

- (IBAction) ChangeModel: (NSMatrix *)sender
{
	PrefsReplaceInt32("modelid", [[sender selectedCell] tag]);
	edited = YES;
}


// If we are not using the CGIMAGEREF drawing strategy,
// then source bitmaps must be 32bits deep.

- (short) testWinDepth: (int) newbpp
{
#ifdef CGIMAGEREF
	return newbpp;
#else
	if ( newbpp != 32 )
		WarningSheet(@"Sorry - In windowed mode, depth must be 32", panel);
	return 32;
#endif
}

// This is called when the screen/window,
// width, height or depth is clicked.
//
// Note that sender may not actually be an NSMatrix.

- (IBAction) ChangeScreen: (NSMatrix *)sender
{
	NSButton *cell  = [sender selectedCell];

	short newx		= [width  intValue];
	short newy		= [height intValue];
	short newbpp	= [depth  intValue];
	short newtype;
	char  str[20];

	if ( cell == screen )
		newtype = DISPLAY_SCREEN;
	else if ( cell == window )
		newtype = DISPLAY_WINDOW;
	else
		newtype = display_type;

	// Check that a field actually changed
	if ( newbpp == init_depth && newx == init_width &&
		 newy == init_height && newtype == display_type )
	{
		D(NSLog(@"No changed GUI items in ChangeScreen"));
		return;
	}

	// If we are changing type, supply some sensible defaults

	short	screenx	= CGDisplayPixelsWide(kCGDirectMainDisplay),
			screeny	= CGDisplayPixelsHigh(kCGDirectMainDisplay),
			screenb = CGDisplayBitsPerPixel(kCGDirectMainDisplay);

	if ( newtype != display_type )
	{
		D(NSLog(@"Changing display type in ChangeScreen"));

		// If changing to full screen, supply main screen dimensions as a default
		if ( newtype == DISPLAY_SCREEN )
			newx = screenx, newy = screeny, newbpp = screenb;

		// If changing from full screen, use minimum screen resolutions
		if ( display_type == DISPLAY_SCREEN )
        {
			newx = MIN_WIDTH, newy = MIN_HEIGHT;
			newbpp = [self testWinDepth: newbpp];
        }
	}
	else
	{
		newbpp = [self testWinDepth: newbpp];

		// Check size is within ranges of MIN_WIDTH ... MAX_WIDTH
		//							and MIN_HEIGHT ... MAX_HEIGHT
		// ???
	}

	[width	setIntValue: newx];
	[height setIntValue: newy];
	[depth	setIntValue: newbpp];


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
		D(NSLog(@"Display type is not SCREEN (%d), resizing window",
														display_type));
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
		const char	*filename = [[sP filename] UTF8String];
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

- (BOOL)    fileManager: (NSFileManager *) manager
shouldProceedAfterError: (NSDictionary *) errorDict
{
	NSRunAlertPanel(@"File operation error",
					@"%@ %@, toPath %@",
					@"Bugger!", nil, nil, 
					[errorDict objectForKey:@"Error"], 
					[errorDict objectForKey:@"Path"],
					[errorDict objectForKey:@"ToPath"]);
	return NO;
}

- (IBAction) DeleteVolume: (id)sender
{
	NSString	*Path = [self RemoveVolumeEntry];

	if ( ! Path )
		return;

	if ( ! [[NSFileManager defaultManager] removeFileAtPath: Path
													handler: self] )
	{
		WarningSheet(@"Unable to delete volume", panel);
		NSLog(@"%s unlink(%s) failed - %s", __PRETTY_FUNCTION__,
									[Path cString], strerror(errno));
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

	PrefsReplaceString("ether", [path UTF8String]);
	edited = YES;
}

- (IBAction) EditExtFS: (NSTextField *)sender
{
	NSString	*path = [extFS stringValue];

	PrefsReplaceString("extfs", [path UTF8String]);
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

	PrefsReplaceString("seriala", [path UTF8String]);
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

	PrefsReplaceString("serialb", [path UTF8String]);
	edited = YES;
}

- (IBAction) EditROMpath: (NSTextField *)sender
{
	NSString	*path = [ROMfile stringValue];

	PrefsReplaceString("rom", [path UTF8String]);
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
	//PrefsRemoveItem(pref,0);
	PrefsRemoveItem(pref, 1);
}

- (NSString *) RemoveVolumeEntry
{
	int		row = [diskImages selectedRow];

	if ( row != -1 )
	{
		NSString	*Path = [volsDS pathAtRow: row];
		const char	*path = [Path UTF8String],
					*str;
		int			tmp = 0;

		NSString	*prompt = [NSString stringWithFormat: @"%s\n%s",
							   "Are you sure you want to delete the file",
							   path];

		if ( ! ChoiceAlert([prompt cString], "Delete", "Cancel") )
			return NULL;

		while ( (str = PrefsFindString("disk", tmp) ) != NULL )
		{
			if ( strcmp(str, path) == 0 )
			{
				PrefsRemoveItem("disk", tmp);
				D(NSLog(@"%s - Deleted prefs entry \"disk\", %d",
											__PRETTY_FUNCTION__, tmp));
				edited = YES;
				break;
			}
			++tmp;
		}

		if ( str == NULL )
		{
			NSLog(@"%s - Couldn't find any disk preference to match %s",
												__PRETTY_FUNCTION__, path);
			return NULL;
		}

		if ( ! [volsDS deleteRow: row] )
			NSLog (@"%s - RemoveVolume %d failed", __PRETTY_FUNCTION__, tmp);
		[diskImages reloadData];
//		return path;
		return Path;
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

- (void) loadPrefs: (int) argc
			  args: (char **) argv
{
	[panel close];				// Temporarily hide preferences panel

	PrefsExit();				// Purge all the old pref values

	PrefsInit(NULL, argc, argv);
	AddPrefsDefaults();
	AddPlatformPrefsDefaults();	// and only create basic ones

	[SCSIds deleteAll];			// Clear out datasources for the tables
	[volsDS deleteAll];

	[self ShowPrefs: self];		// Reset items in panel, and redisplay
	edited = NO;
}

- (IBAction) LoadPrefs: (id)sender
{
	int		argc = 2;
	char	*argv[2];

	argv[0] = "--prefs",
	argv[1] = (char *) [[prefsFile stringValue] UTF8String];

	[self loadPrefs: argc
			   args: argv];
}

- (IBAction) ResetPrefs: (id)sender
{
	[self loadPrefs: 0
			   args: NULL];
}

- (void) setStringOf: (NSTextField *) field
				fromPref: (const char *)  prefName
{
	const char	*value = PrefsFindString(prefName, 0);

	if ( value )
		[field setStringValue: [NSString stringWithUTF8String: value] ];
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
	int				cpu, tmp, val;


	// Set simple single field items

	val = PrefsFindInt32("frameskip");
	[delay setIntValue: val];
	if ( val )
		[frequency	setFloatValue:	60.0 / val];
	else
		[frequency	setFloatValue:	60.0];

	val = PrefsFindInt32("ramsize");
	[bytes	setIntValue:   val];
	[MB		setFloatValue: val / (1024.0 * 1024.0)];

	[disableCD		setState:	PrefsFindBool("nocdrom")];
	[disableSound	setState:	PrefsFindBool("nosound")];
	[FPU			setState:	PrefsFindBool("fpu")	];

	[self setStringOf: etherNet	 fromPref: "ether"	];
	[self setStringOf: extFS	 fromPref: "extfs"	];
	[self setStringOf: modem	 fromPref: "seriala"];
	[self setStringOf: printer	 fromPref: "serialb"];
    [self setStringOf: ROMfile	 fromPref: "rom"];

	[prefsFile setStringValue: [NSString stringWithUTF8String: UserPrefsPath.c_str()] ];


	parse_screen_prefs(PrefsFindString("screen"));

	[width	setIntValue: init_width];
	[height setIntValue: init_height];
	[depth	setIntValue: init_depth];

	[screen setState: NO];
	switch ( display_type )
	{
		case DISPLAY_WINDOW: [window setState: YES]; break;
		case DISPLAY_SCREEN: [screen setState: YES]; break;
	}

	[newVolumeSize setIntValue: 10];

	// Radio button groups:

	val = PrefsFindInt32("bootdriver");
	[bootFromAny setState: val != CDROMRefNum];
	[bootFromCD  setState: val == CDROMRefNum];

	cpu = PrefsFindInt32("cpu");
	val = PrefsFindInt32("modelid");

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	puts("Current memory model does not support 24bit addressing");
	if ( val == [classic tag] )
	{
		// Window already created by NIB file, just display
		[panel makeKeyAndOrderFront:self];
		WarningSheet(@"Compiled-in memory model does not support 24bit",
						@"Disabling Mac Classic emulation", nil, panel);
		cpu = [CPU68030 tag];
		PrefsReplaceInt32("cpu", cpu);
		val = [IIci tag];
		PrefsReplaceInt32("modelid", val);
	}

	puts("Disabling 68000 & Mac Classic buttons");
	[CPU68000 setEnabled:FALSE];
	[classic  setEnabled:FALSE];
#endif

	[CPU68000   setState: [CPU68000  tag] == cpu];
	[CPU68020   setState: [CPU68020  tag] == cpu];
	[CPU68030   setState: [CPU68030  tag] == cpu];
	[CPU68040   setState: [CPU68040  tag] == cpu];

	[classic	setState: [classic	 tag] == val];
	[IIci		setState: [IIci		 tag] == val];
	[quadra900	setState: [quadra900 tag] == val];


	// Lists of thingies:

	val = PrefsFindInt32("keyboardtype");
	tmp = [keyboard indexOfItemWithTag: val];
	if ( tmp != -1 )
		[keyboard selectItemAtIndex: tmp];
	for ( tmp = 0; tmp < [keyboard numberOfItems]; ++tmp )
	{
		NSMenuItem	*type = [keyboard itemAtIndex: tmp];
		[type setState: [type tag] == val];
	}


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
					 withPath: [NSString stringWithUTF8String: str+1]];
		else
			[volsDS addObject: (NSObject *) blank
					 withPath: [NSString stringWithUTF8String: str]];
	}

	[diskImages setDataSource: volsDS];


	[panel makeKeyAndOrderFront:self];		// Window already created by NIB file, just display
}

@end
