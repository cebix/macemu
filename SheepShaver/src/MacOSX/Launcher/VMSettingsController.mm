/*
 *  VMSettingsController.mm - Preferences editing in Cocoa on Mac OS X
 *
 *  Copyright (C) 2006-2010 Alexei Svitkine
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

#import "sysdeps.h"
#import "prefs.h"

// The _UINT64 define is needed to guard against a typedef mismatch with Snow Leopard headers.
#define _UINT64

#import "VMSettingsController.h"

#import "DiskType.h"

#include <unistd.h>

// NSInteger was added in 10.5 SDK.
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
  #if __LP64__ || NS_BUILD_32_LIKE_64
    typedef long NSInteger;
  #else
    typedef int NSInteger;
  #endif
#endif

const int CDROMRefNum = -62;			// RefNum of driver

#ifdef STANDALONE_PREFS
void prefs_init()
{
}

void prefs_exit()
{
}
#endif

@implementation VMSettingsController

+ (id) sharedInstance
{
  static VMSettingsController *_sharedInstance = nil;
  if (!_sharedInstance) {
    _sharedInstance = [[VMSettingsController allocWithZone:[self zone]] init];
  }
  return _sharedInstance;
}

- (id) init
{
  self = [super initWithWindowNibName:@"VMSettingsWindow"];

  cancelWasClicked = NO;

  return self;
}

- (NSInteger) numberOfRowsInTableView: (NSTableView *) table
{
  return [diskArray count];
}

- (id) tableView: (NSTableView *) table objectValueForTableColumn: (NSTableColumn *) col row: (NSInteger) row
{
  DiskType *d = (DiskType*)[diskArray objectAtIndex:row];

  if ([[col identifier] isEqualTo:@"isCDROMcol"]) {
    return [NSString stringWithFormat:@"%d", [d isCDROM]];
  }

  return [d path];
}

- (void) tableView: (NSTableView *) table setObjectValue: (id) object forTableColumn: (NSTableColumn *) tableColumn row: (NSInteger) row
{
  if ([[tableColumn identifier] isEqual:@"isCDROMcol"]) {
    DiskType *d = (DiskType*)[diskArray objectAtIndex:row];
    [d setIsCDROM:![d isCDROM]];
  }
}

static NSString *getStringFromPrefs(const char *key)
{
  const char *value = PrefsFindString(key);
  if (value == NULL)
    return @"";
  return [NSString stringWithUTF8String: value];
}

- (void) setupGUI
{
  diskArray = [[NSMutableArray alloc] init];

  const char *dsk;
  int index = 0;
  while ((dsk = PrefsFindString("disk", index++)) != NULL) {
    DiskType *disk = [[[DiskType alloc] init] autorelease];
    [disk setPath:[NSString stringWithUTF8String: dsk ]];
    [disk setIsCDROM:NO];

    [diskArray addObject:disk];
  }

  /* Fetch all CDROMs */
  index = 0;
  while ((dsk = PrefsFindString("cdrom", index++)) != NULL) {
    NSString *path = [NSString stringWithUTF8String: dsk ];
    if (![path hasPrefix:@"/dev/"]) {
        DiskType *disk = [[[DiskType alloc] init] autorelease];
        [disk setPath:[NSString stringWithUTF8String: dsk ]];
        [disk setIsCDROM:YES];

        [diskArray addObject:disk];
    }
  }

  [disks setDataSource: self];
  [disks reloadData];

  int bootdriver = PrefsFindInt32("bootdriver"), active = 0;
  switch (bootdriver) {
    case 0: active = 0; break;
    case CDROMRefNum: active = 1; break;
  }
  [bootFrom selectItemAtIndex: active ];

  [romFile setStringValue: getStringFromPrefs("rom") ];
  [unixRoot setStringValue: getStringFromPrefs("extfs") ];
  [disableCdrom setIntValue: PrefsFindBool("nocdrom") ];
  [ramSize setIntValue: PrefsFindInt32("ramsize") / (1024*1024) ];
  [ramSizeStepper setIntValue: PrefsFindInt32("ramsize") / (1024*1024) ];

  int display_type = 0;
  int dis_width = 640;
  int dis_height = 480;

  const char *str = PrefsFindString("screen");
  if (str != NULL) {
    if (sscanf(str, "win/%d/%d", &dis_width, &dis_height) == 2)
      display_type = 0;
    else if (sscanf(str, "dga/%d/%d", &dis_width, &dis_height) == 2)
      display_type = 1;
  }

  [videoType selectItemAtIndex: display_type ];
  [width setIntValue: dis_width ];
  [height setIntValue: dis_height ];

  int frameskip = PrefsFindInt32("frameskip");
  int item = -1;
  switch (frameskip) {
    case 12: item = 0; break;
    case 8: item = 1; break;
    case 6: item = 2; break;
    case 4: item = 3; break;
    case 2: item = 4; break;
    case 1: item = 5; break;
    case 0: item = 6; break;
  }
  if (item >= 0)
    [refreshRate selectItemAtIndex: item ];

  [qdAccel setIntValue: PrefsFindBool("gfxaccel") ];

  [disableSound setIntValue: PrefsFindBool("nosound") ];
  [outDevice setStringValue: getStringFromPrefs("dsp") ];
  [mixDevice setStringValue: getStringFromPrefs("mixer") ];

  [useRawKeyCodes setIntValue: PrefsFindBool("keycodes") ];
  [rawKeyCodes setStringValue: getStringFromPrefs("keycodefile") ];
  [rawKeyCodes setEnabled:[useRawKeyCodes intValue]];
  [browseRawKeyCodesButton setEnabled:[useRawKeyCodes intValue]];

  int wheelmode = PrefsFindInt32("mousewheelmode"), wheel = 0;
  switch (wheelmode) {
    case 0: wheel = 0; break;
    case 1: wheel = 1; break;
  }
  [mouseWheel selectItemAtIndex: wheel ];

  [scrollLines setIntValue: PrefsFindInt32("mousewheellines") ];
  [scrollLinesStepper setIntValue: PrefsFindInt32("mousewheellines") ];

  [ignoreIllegalMemoryAccesses setIntValue: PrefsFindBool("ignoresegv") ];
  [ignoreIllegalInstructions setIntValue: PrefsFindBool("ignoreillegal") ];
  [dontUseCPUWhenIdle setIntValue: PrefsFindBool("idlewait") ];
  [enableJIT setIntValue: PrefsFindBool("jit") ];
  [enable68kDREmulator setIntValue: PrefsFindBool("jit68k") ];

  [modemPort setStringValue: getStringFromPrefs("seriala") ];
  [printerPort setStringValue: getStringFromPrefs("serialb") ];
  [ethernetInterface setStringValue: getStringFromPrefs("ether") ];
}

- (void) editSettingsFor: (NSString *) vmdir sender: (id) sender
{
  chdir([vmdir fileSystemRepresentation]);
  AddPrefsDefaults();
  AddPlatformPrefsDefaults();
  LoadPrefs([vmdir fileSystemRepresentation]);
  NSWindow *window = [self window];
  [self setupGUI];
  [NSApp runModalForWindow:window];
}

- (void) editSettingsForNewVM: (NSString *) vmdir sender: (id) sender
{
  chdir([vmdir fileSystemRepresentation]);
  AddPrefsDefaults();
  AddPlatformPrefsDefaults();
  LoadPrefs([vmdir fileSystemRepresentation]);
  PrefsReplaceString("screen", "win/800/600");
  PrefsReplaceString("extfs", "");
  PrefsReplaceString("ether", "slirp");
  PrefsReplaceInt32("ramsize", 64 << 20);
  PrefsReplaceInt32("frameskip", 2);
  PrefsReplaceBool("jit", true);
  NSWindow *window = [self window];
  [self setupGUI];
  [NSApp runModalForWindow:window];
}

static NSString *makeRelativeIfNecessary(NSString *path)
{
  char cwd[1024], filename[1024];
  int cwdlen;
  strlcpy(filename, [path fileSystemRepresentation], sizeof(filename));
  getcwd(cwd, sizeof(cwd));
  cwdlen = strlen(cwd);
  if (!strncmp(cwd, filename, cwdlen)) {
    if (cwdlen >= 0 && cwd[cwdlen-1] != '/')
      cwdlen++;
    return [NSString stringWithUTF8String: filename + cwdlen];
  }
  return path;
}

- (IBAction) addDisk: (id) sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  [open setCanChooseDirectories:YES];
  [open setAccessoryView:isCDROM];
  [open setAllowsMultipleSelection:NO];
  [open setTreatsFilePackagesAsDirectories:YES];
  [open beginSheetForDirectory: [[NSFileManager defaultManager] currentDirectoryPath]
                          file: @"Unknown"
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_addDiskEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _addDiskEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
  if (theReturnCode == NSOKButton) {
    DiskType *d = [[[DiskType alloc] init] autorelease];
    [d setPath:makeRelativeIfNecessary([open filename])];

    [d setIsCDROM:([isCDROMcheckbox state] == NSOnState)];

    [diskArray addObject: d]; 
    [disks reloadData];
  }
}

- (IBAction) removeDisk: (id) sender
{
  int selectedRow = [disks selectedRow];
  if (selectedRow >= 0) {
    [diskArray removeObjectAtIndex: selectedRow];
    [disks reloadData];
  }
}

- (IBAction) createDisk: (id) sender
{
  NSSavePanel *save = [NSSavePanel savePanel];
  [save setAccessoryView: diskSaveSize];
  [save setTreatsFilePackagesAsDirectories:YES];
  [save beginSheetForDirectory: [[NSFileManager defaultManager] currentDirectoryPath]
                          file: @"New.dsk"
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_createDiskEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _createDiskEnd: (NSSavePanel *) save returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
  if (theReturnCode == NSOKButton) {
    int size = [diskSaveSizeField intValue];
    if (size >= 0 && size <= 10000) {
      char cmd[1024];
      snprintf(cmd, sizeof(cmd), "dd if=/dev/zero \"of=%s\" bs=1024k count=%d", [[save filename] UTF8String], [diskSaveSizeField intValue]);
      int ret = system(cmd);
      if (ret == 0) {
        DiskType *d = [[[DiskType alloc] init] autorelease];
        [d setPath:makeRelativeIfNecessary([save filename])];
        [d setIsCDROM:NO];

        [diskArray addObject: d];
        [disks reloadData];
      }
    }
  }
  [(NSData *)theContextInfo release];
}

- (IBAction) useRawKeyCodesClicked: (id) sender
{
  [rawKeyCodes setEnabled:[useRawKeyCodes intValue]];
  [browseRawKeyCodesButton setEnabled:[useRawKeyCodes intValue]];
}

- (IBAction) browseForROMFileClicked: (id) sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  [open setCanChooseDirectories:NO];
  [open setAllowsMultipleSelection:NO];
  [open setTreatsFilePackagesAsDirectories:YES];
  [open beginSheetForDirectory: @""
                          file: [romFile stringValue]
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_browseForROMFileEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _browseForROMFileEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
  if (theReturnCode == NSOKButton) {
    [romFile setStringValue: makeRelativeIfNecessary([open filename])];
  }
}

- (IBAction) browseForUnixRootClicked: (id) sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  [open setCanChooseDirectories:YES];
  [open setCanChooseFiles:NO];
  [open setAllowsMultipleSelection:NO];
  [open beginSheetForDirectory: @""
                          file: [unixRoot stringValue]
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_browseForUnixRootEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _browseForUnixRootEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
  if (theReturnCode == NSOKButton) {
    [unixRoot setStringValue: makeRelativeIfNecessary([open filename])];
  }
}

- (IBAction) browseForKeyCodesFileClicked: (id) sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  [open setCanChooseDirectories:NO];
  [open setAllowsMultipleSelection:NO];
  [open setTreatsFilePackagesAsDirectories:YES];
  [open beginSheetForDirectory: @""
                          file: [unixRoot stringValue]
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_browseForKeyCodesFileEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _browseForKeyCodesFileEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
  if (theReturnCode == NSOKButton) {
    [rawKeyCodes setStringValue: makeRelativeIfNecessary([open filename])];
  }
}

- (void) cancelEdit: (id) sender
{
#ifdef STANDALONE_PREFS
  PrefsExit();
#endif
  [[self window] close];
  [NSApp stopModal];
  cancelWasClicked = YES;
}

- (void) saveChanges: (id) sender
{
  // Remove all disks
  while (PrefsFindString("disk"))
    PrefsRemoveItem("disk");
  // Remove all cdroms (but keep the ones in /dev/)
  const char *path;
  int index = 0;
  while ((path = PrefsFindString("cdrom", index)) != NULL) {
    NSString *p = [NSString stringWithUTF8String: path];
    if (![p hasPrefix:@"/dev/"]) {
      PrefsRemoveItem("cdrom", index);
    } else {
      // only increase the index if the current entry has not been deleted
      // if it has been deleted, the next entry is on the current entrys index
      index++;
    }
  }

  // Write all disks
  for (int i = 0; i < [diskArray count]; i++) {
    DiskType *d = [diskArray objectAtIndex:i];
    PrefsAddString([d isCDROM] ? "cdrom" : "disk", [[d path] UTF8String]);
  }

  PrefsReplaceInt32("bootdriver", ([bootFrom indexOfSelectedItem] == 1 ? CDROMRefNum : 0));
  PrefsReplaceString("rom", [[romFile stringValue] UTF8String]);
  PrefsReplaceString("extfs", [[unixRoot stringValue] UTF8String]);
  PrefsReplaceBool("nocdrom", [disableCdrom intValue]);
  PrefsReplaceInt32("ramsize", [ramSize intValue] << 20);

  char pref[256];
  snprintf(pref, sizeof(pref), "%s/%d/%d", [videoType indexOfSelectedItem] == 0 ? "win" : "dga", [width intValue], [height intValue]);
  PrefsReplaceString("screen", pref);

  int rate = 8;
  switch ([refreshRate indexOfSelectedItem]) {
    case 0: rate = 12; break;
    case 1: rate = 8; break;
    case 2: rate = 6; break;
    case 3: rate = 4; break;
    case 4: rate = 2; break;
    case 5: rate = 1; break;
    case 6: rate = 0; break;
  }
  PrefsReplaceInt32("frameskip", rate);
  PrefsReplaceBool("gfxaccel", [qdAccel intValue]);

  PrefsReplaceBool("nosound", [disableSound intValue]);
  PrefsReplaceString("dsp", [[outDevice stringValue] UTF8String]);
  PrefsReplaceString("mixer", [[mixDevice stringValue] UTF8String]);

  PrefsReplaceBool("keycodes", [useRawKeyCodes intValue]);
  PrefsReplaceString("keycodefile", [[rawKeyCodes stringValue] UTF8String]);

  PrefsReplaceInt32("mousewheelmode", [mouseWheel indexOfSelectedItem]);
  PrefsReplaceInt32("mousewheellines", [scrollLines intValue]);

  PrefsReplaceBool("ignoresegv", [ignoreIllegalMemoryAccesses intValue]);
  PrefsReplaceBool("ignoreillegal", [ignoreIllegalInstructions intValue]);
  PrefsReplaceBool("idlewait", [dontUseCPUWhenIdle intValue]);
  PrefsReplaceBool("jit", [enableJIT intValue]);
  PrefsReplaceBool("jit68k", [enable68kDREmulator intValue]);

  PrefsReplaceString("seriala", [[modemPort stringValue] UTF8String]);
  PrefsReplaceString("serialb", [[printerPort stringValue] UTF8String]);
  PrefsReplaceString("ether", [[ethernetInterface stringValue] UTF8String]);
  SavePrefs();
#ifdef STANDALONE_PREFS
  PrefsExit();
#endif

  [[self window] close];
  [NSApp stopModal];
  cancelWasClicked = NO;
}

- (BOOL) cancelWasClicked
{
  return cancelWasClicked;
}

- (void) dealloc
{
  [super dealloc];
}

@end

