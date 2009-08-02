/*
 *  VMListController.mm - SheepShaver VM manager in Cocoa on Mac OS X
 *
 *  Copyright (C) 2009 Alexei Svitkine
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

#import "VMListController.h"
#import "VMSettingsController.h"

@implementation VMListController

+ (id) sharedInstance
{
	static VMListController *_sharedInstance = nil;
	if (!_sharedInstance) {
		_sharedInstance = [[VMListController allocWithZone:[self zone]] init];
	}
	return _sharedInstance;
}

- (id) init
{
	self = [super initWithWindowNibName:@"VMListWindow"];

	NSArray *vms = [[NSUserDefaults standardUserDefaults] stringArrayForKey:@"vm_list"];
	vmArray = [[NSMutableArray alloc] initWithCapacity:[vms count]];
	[vmArray addObjectsFromArray:vms];

	return self;
}

- (void) awakeFromNib
{
  [vmList setDataSource: self];
	//[vmList setDelegate: self];
  [vmList reloadData];
}

- (int) numberOfRowsInTableView: (NSTableView *) table
{
  return [vmArray count];
}

- (id) tableView: (NSTableView *) table objectValueForTableColumn: (NSTableColumn *) c row: (int) r
{
  return [vmArray objectAtIndex: r]; // [[vmArray objectAtIndex: r] lastPathComponent];
}

//- (NSString *) tableView: (NSTableView *) table toolTipForCell: (NSCell *) cell rect: (NSRectPointer) rect
//             tableColumn: (NSTableColumn *) c row: (int) r mouseLocation: (NSPoint) loc
//{
//	return [vmArray objectAtIndex: r];
//}

- (IBAction) newVirtualMachine: (id) sender
{
	NSSavePanel *save = [NSSavePanel savePanel];
	[save setMessage: @"New SheepShaver Virtual Machine:"];
	[save setRequiredFileType: @"sheepvm"];
	[save setCanSelectHiddenExtension: YES];
	[save setExtensionHidden: NO];
	[save beginSheetForDirectory: nil
                          file: @"New.sheepvm"
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_newVirtualMachineDone: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (IBAction) _newVirtualMachineDone: (NSSavePanel *) save returnCode: (int) returnCode contextInfo: (void *) contextInfo
{
	if (returnCode == NSOKButton) {
		// make dir.
		// create prefs file in there
		// edit said prefs file
		// advanced: show sub-panel in save dialog that says "Create Disk:"
	}
}

- (IBAction) importVirtualMachine: (id) sender
{
	NSOpenPanel *open = [NSOpenPanel openPanel];
	[open setMessage:@"Import SheepShaver Virtual Machine:"];
	[open setResolvesAliases:YES];
	// Curiously, bundles are treated as "files" not "directories" by NSOpenPanel.
	[open setCanChooseDirectories:NO];
	[open setCanChooseFiles:YES];
	[open setAllowsMultipleSelection:NO];
	[open beginSheetForDirectory: nil
                          file: nil
                         types: [NSArray arrayWithObject:@"sheepvm"]
                modalForWindow: [self window]
                 modalDelegate: self
                didEndSelector: @selector(_importVirtualMachineDone: returnCode: contextInfo:)
                   contextInfo: nil];
}

- (void) _importVirtualMachineDone: (NSOpenPanel *) open returnCode: (int) returnCode contextInfo: (void *) contextInfo
{
	if (returnCode == NSOKButton) {
		[vmArray addObject:[open filename]];
		[vmList reloadData];
		[[NSUserDefaults standardUserDefaults] setObject:vmArray forKey:@"vm_list"];
	}
}

- (IBAction) editVirtualMachineSettings:(id)sender
{
	int selectedRow = [vmList selectedRow];
  if (selectedRow >= 0) {
		[[VMSettingsController sharedInstance] editSettingsFor:[vmArray objectAtIndex:selectedRow] sender:sender];
  }
}

- (IBAction) runVirtualMachine:(id)sender
{
}

@end
