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

/*

TODO:

Verify if VM exists
Create Disk on New VM
Keep track of VMs that are running and disallow editing settings, re-launching, etc..
Drag-drop to re-arrange order of VMs
Drag VM from Finder to import
Don't show Preferences menu in spawned SheepShaver instances - or make them
use the same nib file as this app!

 */

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
	[vmList setDelegate: self];
	[vmList reloadData];
}

- (void) keyDown: (NSEvent *) event
{
	if ([event type] == NSKeyDown && [[event characters] length] > 0) {
		unichar key = [[event characters] characterAtIndex:0];
		if (key == NSDeleteFunctionKey || key == NSDeleteCharacter) {
			[self deleteVirtualMachine:self];
		}
	}
}

- (int) numberOfRowsInTableView: (NSTableView *) table
{
	return [vmArray count];
}

- (id) tableView: (NSTableView *) table objectValueForTableColumn: (NSTableColumn *) c row: (int) r
{
	return [vmArray objectAtIndex: r]; // [[vmArray objectAtIndex: r] lastPathComponent];
}

- (void) tableViewSelectionDidChange: (NSNotification *) notification
{
	if ([vmList selectedRow] >= 0) {
		[settingsButton setEnabled:YES];
		[launchButton setEnabled:YES];
	} else {
		[settingsButton setEnabled:NO];
		[launchButton setEnabled:NO];
	}
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
		NSFileManager *manager = [NSFileManager defaultManager];
		[manager createDirectoryAtPath:[save filename] attributes:nil];
		[manager createFileAtPath:[[save filename] stringByAppendingPathComponent:@"prefs"] contents:nil attributes:nil];
		[vmArray addObject:[save filename]];
		[vmList reloadData];
		[[NSUserDefaults standardUserDefaults] setObject:vmArray forKey:@"vm_list"];
		[vmList selectRow:([vmArray count] - 1) byExtendingSelection:NO];
		[self editVirtualMachineSettings:self];
		if ([[VMSettingsController sharedInstance] cancelWasClicked]) {
			[manager removeFileAtPath:[save filename] handler:nil];
			[vmArray removeObjectAtIndex:([vmArray count] - 1)];
			[vmList reloadData];
		}
		// TODO advanced: show sub-panel in save dialog that says "Create Disk:"
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

- (IBAction) editVirtualMachineSettings: (id) sender
{
	int selectedRow = [vmList selectedRow];
	if (selectedRow >= 0) {
		[[VMSettingsController sharedInstance] editSettingsFor:[vmArray objectAtIndex:selectedRow] sender:sender];
	}
}

- (IBAction) launchVirtualMachine: (id) sender
{
	int selectedRow = [vmList selectedRow];
	if (selectedRow >= 0) {
		NSTask *sheep = [[NSTask alloc] init];
		[sheep setLaunchPath:[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"SheepShaver"]];
		[sheep setArguments:[NSArray arrayWithObject:[vmArray objectAtIndex:selectedRow]]];
		[sheep launch];
	}
}

- (IBAction) deleteVirtualMachine: (id) sender
{
	int selectedRow = [vmList selectedRow];
	if (selectedRow >= 0) {
		NSAlert *alert = [[[NSAlert alloc] init] autorelease];
		[alert setMessageText:@"Do you wish to remove the selected virtual machine from the list?"];
		[alert addButtonWithTitle:@"Remove"];
		[alert addButtonWithTitle:@"Cancel"];
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert beginSheetModalForWindow:[self window]
		                  modalDelegate:self
		                 didEndSelector:@selector(_deleteVirtualMachineDone: returnCode: contextInfo:)
		                    contextInfo:nil];

			}
}

- (void) _deleteVirtualMachineDone: (NSAlert *) alert returnCode: (int) returnCode contextInfo: (void *) contextInfo
{
	if (returnCode == NSAlertFirstButtonReturn) {
		[vmArray removeObjectAtIndex:[vmList selectedRow]];
		[vmList deselectAll:self];
		[vmList reloadData];
		[[NSUserDefaults standardUserDefaults] setObject:vmArray forKey:@"vm_list"];
	}
}

@end
