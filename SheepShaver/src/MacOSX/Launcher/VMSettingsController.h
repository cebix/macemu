/*
 *  VMSettingsController.h - Preferences editing in Cocoa on Mac OS X
 *
 *  Copyright (C) 2006-2009 Alexei Svitkine
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

@interface VMSettingsController : NSWindowController
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
    <NSTableViewDataSource>
#endif
{
    BOOL cancelWasClicked;

    IBOutlet NSView *diskSaveSize;
    IBOutlet NSTextField *diskSaveSizeField;
    NSMutableArray *diskArray;
    IBOutlet NSView *isCDROM;
    IBOutlet NSButton *isCDROMcheckbox;	
    // Setup
    IBOutlet NSTableView *disks;
    IBOutlet NSComboBox *bootFrom;
    IBOutlet NSButton *disableCdrom;
    IBOutlet NSTextField *ramSize;
    IBOutlet NSStepper *ramSizeStepper;
    IBOutlet NSTextField *romFile;
    IBOutlet NSTextField *unixRoot;
    // Audio/Video
    IBOutlet NSPopUpButton *videoType;
    IBOutlet NSPopUpButton *refreshRate;
    IBOutlet NSComboBox *width;
    IBOutlet NSComboBox *height;
    IBOutlet NSButton *qdAccel;
    IBOutlet NSButton *disableSound;
    IBOutlet NSTextField *outDevice;
    IBOutlet NSTextField *mixDevice;
    // Keyboard/Mouse
    IBOutlet NSButton *useRawKeyCodes;
    IBOutlet NSTextField *rawKeyCodes;
	IBOutlet NSButton *browseRawKeyCodesButton;
    IBOutlet NSPopUpButton *mouseWheel;
    IBOutlet NSTextField *scrollLines;
    IBOutlet NSStepper *scrollLinesStepper;
    // CPU/Misc
    IBOutlet NSButton *ignoreIllegalMemoryAccesses;
    IBOutlet NSButton *ignoreIllegalInstructions;
    IBOutlet NSButton *dontUseCPUWhenIdle;
    IBOutlet NSButton *enableJIT;
    IBOutlet NSButton *enable68kDREmulator;
    IBOutlet NSTextField *modemPort;
    IBOutlet NSTextField *printerPort;
    IBOutlet NSTextField *ethernetInterface;
}

+ (id) sharedInstance;
- (id) init;
- (void) setupGUI;
- (void) editSettingsFor: (NSString *) vmdir sender: (id) sender;
- (void) editSettingsForNewVM: (NSString *) vmdir sender: (id) sender;
- (IBAction) addDisk: (id) sender;
- (IBAction) removeDisk: (id) sender;
- (IBAction) createDisk: (id) sender;
- (IBAction) useRawKeyCodesClicked: (id) sender;
- (IBAction) browseForROMFileClicked: (id) sender;
- (IBAction) browseForUnixRootClicked: (id) sender;
- (IBAction) browseForKeyCodesFileClicked: (id) sender;
- (void) cancelEdit: (id) sender;
- (void) saveChanges: (id) sender;
- (BOOL) cancelWasClicked;
- (void) dealloc;

@end
