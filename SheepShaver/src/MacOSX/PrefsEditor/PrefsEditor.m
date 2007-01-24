/*
 *  PrefsEditor.m - Preferences editing in Cocoa on Mac OS X
 *
 *  Copyright (C) 2006-2007 Alexei Svitkine
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

#import "sysdeps.h"
#import "prefs.h"

const int CDROMRefNum = -62;			// RefNum of driver

@implementation PrefsEditor

- (id) init
{
  self = [super init];

	AddPrefsDefaults();
	AddPlatformPrefsDefaults();

	// Load preferences from settings file
	LoadPrefs();
  chdir([[[NSBundle mainBundle] bundlePath] cString]);
  chdir("..");

  return self;
}

- (int)numberOfRowsInTableView:(NSTableView *)aTable
{
  return [diskArray count];
}

- (id)tableView:(NSTableView *)aTable objectValueForTableColumn:(NSTableColumn *)aCol row:(int)aRow
{
  return [diskArray objectAtIndex: aRow];
}

NSString *getStringFromPrefs(const char *key)
{
  const char *value = PrefsFindString(key);
  if (value == NULL)
    return @"";
  return [NSString stringWithCString: value];
}

- (void) awakeFromNib
{
  diskArray = [[NSMutableArray alloc] init];

  const char *dsk;
  int index = 0;
  while ((dsk = PrefsFindString("disk", index++)) != NULL)
    [diskArray addObject: [NSString stringWithCString: dsk ]];

  [disks setDataSource: self];
  [disks reloadData];

	int bootdriver = PrefsFindInt32("bootdriver"), active = 0;
	switch (bootdriver) {
		case 0: active = 0; break;
    case CDROMRefNum: active = 1; break;
	}
  [bootFrom selectItemAtIndex: active ];

  [romFile  setStringValue: getStringFromPrefs("rom") ];
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
		case 0: item = 5; break;
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

  int wheelmode = PrefsFindInt32("mousewheelmode"), wheel = 0;
	switch (wheelmode) {
		case 0: wheel = 0; break;
		case 1: wheel = 1; break;
	}
  [mouseWheel selectItemAtIndex: wheel ];

  [scrollLines setIntValue: PrefsFindInt32("mousewheellines") ];
  [scrollLinesStepper setIntValue: PrefsFindInt32("mousewheellines") ];

  [ignoreIllegalMemoryAccesses setIntValue: PrefsFindBool("ignoresegv") ];
  [dontUseCPUWhenIdle setIntValue: PrefsFindBool("idlewait") ];
  [enableJIT setIntValue: PrefsFindBool("jit") ];
  [enable68kDREmulator setIntValue: PrefsFindBool("jit68k") ];
  
  [modemPort setStringValue: getStringFromPrefs("seriala") ];
  [printerPort setStringValue: getStringFromPrefs("serialb") ];
  [ethernetInterface setStringValue: getStringFromPrefs("ether") ];
}

- (IBAction) addDisk:(id)sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  [open setCanChooseDirectories:NO];
  [open setAllowsMultipleSelection:NO];
  [open beginSheetForDirectory: @""
                          file: @"Unknown"
                modalForWindow: window
                 modalDelegate: self
                didEndSelector: @selector(_addDiskEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _addDiskEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
    if (theReturnCode == NSOKButton) {
      char cwd[1024], filename[1024];
      int cwdlen;
      strlcpy(filename, [[open filename] cString], sizeof(filename));
      getcwd(cwd, sizeof(cwd));
      cwdlen = strlen(cwd);
      if (!strncmp(cwd, filename, cwdlen)) {
        if (cwdlen >= 0 && cwd[cwdlen-1] != '/')
          cwdlen++;
        [diskArray addObject: [NSString stringWithCString: filename + cwdlen ]];
      } else {
        [diskArray addObject: [open filename]];
      }
      [disks reloadData];
    }
    [(NSData *)theContextInfo release];
}

- (IBAction) removeDisk:(id)sender
{
  int selectedRow = [disks selectedRow];
  if (selectedRow >= 0) {
    [diskArray removeObjectAtIndex: selectedRow];
    [disks reloadData];
  }
}

- (IBAction) createDisk:(id)sender
{
  NSSavePanel *save = [NSSavePanel savePanel];
  [save setAccessoryView: diskSaveSize];
  [save beginSheetForDirectory: @""
                          file: @"New.dsk"
                modalForWindow: window
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
        snprintf(cmd, sizeof(cmd), "dd if=/dev/zero \"of=%s\" bs=1024k count=%d", [[save filename] cString], [diskSaveSizeField intValue]);
        int ret = system(cmd);
        if (ret == 0) {
          char cwd[1024], filename[1024];
          int cwdlen;
          strlcpy(filename, [[save filename] cString], sizeof(filename));
          getcwd(cwd, sizeof(cwd));
          cwdlen = strlen(cwd);
          if (!strncmp(cwd, filename, cwdlen)) {
            if (cwdlen >= 0 && cwd[cwdlen-1] != '/')
              cwdlen++;
            [diskArray addObject: [NSString stringWithCString: filename + cwdlen ]];
          } else {
            [diskArray addObject: [save filename]];
          }
          [disks reloadData];
        }
      }
    }
    [(NSData *)theContextInfo release];
}

- (IBAction) useRawKeyCodesClicked:(id)sender
{
  [rawKeyCodes setEnabled:[useRawKeyCodes intValue]];
}

- (IBAction) browseForROMFileClicked:(id)sender
{
  NSOpenPanel *open = [NSOpenPanel openPanel];
  NSString *dir = @"";
  NSString *file = [romFile stringValue];
  [open setCanChooseDirectories:NO];
  [open setAllowsMultipleSelection:NO];
  [open beginSheetForDirectory: dir
                          file: file
                modalForWindow: window
                 modalDelegate: self
                didEndSelector: @selector(_browseForROMFileEnd: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _browseForROMFileEnd: (NSOpenPanel *) open returnCode: (int) theReturnCode contextInfo: (void *) theContextInfo
{
    if (theReturnCode == NSOKButton) {
      char cwd[1024], filename[1024];
      int cwdlen;
      strlcpy(filename, [[open filename] cString], sizeof(filename));
      getcwd(cwd, sizeof(cwd));
      cwdlen = strlen(cwd);
      if (!strncmp(cwd, filename, cwdlen)) {
        if (cwdlen >= 0 && cwd[cwdlen-1] != '/')
          cwdlen++;
        [romFile setStringValue: [NSString stringWithCString: filename + cwdlen ]];
      } else {
        [romFile setStringValue: [open filename]];
      }
    }
    [(NSData *)theContextInfo release];
}

- (void) windowWillClose: (NSNotification *) aNotification;
{
	while (PrefsFindString("disk"))
		PrefsRemoveItem("disk");

  for (int i = 0; i < [diskArray count]; i++) {
    PrefsAddString("disk", [[diskArray objectAtIndex:i] cString]);
  }
  PrefsReplaceInt32("bootdriver", ([bootFrom indexOfSelectedItem] == 1 ? CDROMRefNum : 0));
  PrefsReplaceString("rom", [[romFile stringValue] cString]);
  PrefsReplaceString("extfs", [[unixRoot stringValue] cString]);
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
  }
  PrefsReplaceInt32("frameskip", rate);
	PrefsReplaceBool("gfxaccel", [qdAccel intValue]);

	PrefsReplaceBool("nosound", [disableSound intValue]);
  PrefsReplaceString("dsp", [[outDevice stringValue] cString]);
  PrefsReplaceString("mixer", [[mixDevice stringValue] cString]);

	PrefsReplaceBool("keycodes", [useRawKeyCodes intValue]);
  PrefsReplaceString("keycodefile", [[rawKeyCodes stringValue] cString]);

  PrefsReplaceInt32("mousewheelmode", [mouseWheel indexOfSelectedItem]);
	PrefsReplaceInt32("mousewheellines", [scrollLines intValue]);

	PrefsReplaceBool("ignoresegv", [ignoreIllegalMemoryAccesses intValue]);
	PrefsReplaceBool("idlewait", [dontUseCPUWhenIdle intValue]);
	PrefsReplaceBool("jit", [enableJIT intValue]);
	PrefsReplaceBool("jit68k", [enable68kDREmulator intValue]);

  PrefsReplaceString("seriala", [[modemPort stringValue] cString]);
  PrefsReplaceString("serialb", [[printerPort stringValue] cString]);
  PrefsReplaceString("ether", [[ethernetInterface stringValue] cString]);

  SavePrefs();
  PrefsExit();
  exit(0);
}

- (void) dealloc
{
  [super dealloc];
}

@end

