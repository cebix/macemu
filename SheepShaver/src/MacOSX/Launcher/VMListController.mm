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
Drag VM from Finder to import
Don't show Preferences menu in spawned SheepShaver instances - or make them
use the same nib file as this app!
When choosing things like rom file and keycode files - have a checkbox to copy
selected file into the bundle.

 */

@interface NSObject (TableViewContextMenu)
- (NSMenu *) tableView: (NSTableView *) tableView menuForEvent: (NSEvent *) event;
@end

@implementation NSTableView (ContextMenu)
- (NSMenu *) menuForEvent: (NSEvent *) event
{
	if ([[self delegate] respondsToSelector:@selector(tableView:menuForEvent:)])
		return [[self delegate] tableView:self menuForEvent:event];
	return nil;
}
@end

#define VM_DRAG_TYPE @"sheepvm"

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

	tasks = [[NSMutableDictionary alloc] init];

	[[NSNotificationCenter defaultCenter] addObserver:self
		selector:@selector(onTaskTerminated:)
		name:NSTaskDidTerminateNotification
		object:nil];

	return self;
}

- (void) awakeFromNib
{
	[vmList setDataSource: self];
	[vmList setDelegate: self];
	[vmList reloadData];
	[vmList registerForDraggedTypes:[NSArray arrayWithObjects:VM_DRAG_TYPE, nil]];
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

- (BOOL) tableView: (NSTableView *) table writeRowsWithIndexes: (NSIndexSet *) rows toPasteboard: (NSPasteboard *) pboard
{
	vmBeingDragged = [vmArray objectAtIndex:[rows firstIndex]];
	[pboard declareTypes:[NSArray arrayWithObject:VM_DRAG_TYPE] owner:self];
	[pboard setString:VM_DRAG_TYPE forType:VM_DRAG_TYPE];
	return YES;
}

- (NSDragOperation) tableView: (NSTableView *) table validateDrop: (id <NSDraggingInfo>) info proposedRow: (int) row proposedDropOperation: (NSTableViewDropOperation) op
{
	if (op == NSTableViewDropAbove && row != -1) {
		return NSDragOperationPrivate;
	} else {
		return NSDragOperationNone;
	}
}

- (BOOL) tableView: (NSTableView *) table acceptDrop: (id <NSDraggingInfo>) info row: (int) row dropOperation: (NSTableViewDropOperation) op
{	
	if ([[[info draggingPasteboard] availableTypeFromArray:[NSArray arrayWithObject:VM_DRAG_TYPE]] isEqualToString:VM_DRAG_TYPE]) {
		[vmList deselectAll:nil];
		int index = [vmArray indexOfObject:vmBeingDragged];
		if (index != row) {
			[vmArray insertObject:vmBeingDragged atIndex:row];
			if (row <= index) {
				index += 1;
			} else {
				row -= 1;
			}
			[vmArray removeObjectAtIndex: index];
		}
		[[NSUserDefaults standardUserDefaults] setObject:vmArray forKey:@"vm_list"];
		[vmList reloadData];
		[vmList selectRow:row byExtendingSelection:NO];
		return YES;
	}

	return NO;
}

- (NSMenu *) tableView: (NSTableView *) table menuForEvent: (NSEvent *) event
{
	NSMenu *menu = nil;
	int row = [table rowAtPoint:[table convertPoint:[event locationInWindow] fromView:nil]];
	if (row >= 0) {
		[table selectRow:row byExtendingSelection:NO];
		menu = [[[NSMenu alloc] initWithTitle: @"Contextual Menu"] autorelease];
		[menu addItemWithTitle: @"Launch Virtual Machine"
		                action: @selector(launchVirtualMachine:) keyEquivalent: @""];
		[menu addItemWithTitle: @"Edit VM Settings..."
		                action: @selector(editVirtualMachineSettings:) keyEquivalent: @""];
		[menu addItemWithTitle: @"Reveal VM in Finder"
		                action: @selector(revealVirtualMachineInFinder:) keyEquivalent: @""];
		[menu addItemWithTitle: @"Remove VM from List"
		                action: @selector(deleteVirtualMachine:) keyEquivalent: @""];
	}
	return menu;
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
		NSString *path = [vmArray objectAtIndex:selectedRow];
		if ([tasks objectForKey:path]) {
			NSAlert *alert = [[[NSAlert alloc] init] autorelease];
			[alert setMessageText:@"Cannot edit virtual machine settings while it's running."];
			[alert setAlertStyle:NSWarningAlertStyle];
			[alert beginSheetModalForWindow:[self window]
		                    modalDelegate:self
		                   didEndSelector:nil
		                      contextInfo:nil];
		} else {
			[[VMSettingsController sharedInstance] editSettingsFor:path sender:sender];
		}
	}
}

- (IBAction) launchVirtualMachine: (id) sender
{
	int selectedRow = [vmList selectedRow];
	if (selectedRow >= 0) {
		NSString *path = [vmArray objectAtIndex:selectedRow];
		if ([tasks objectForKey:path]) {
			NSAlert *alert = [[[NSAlert alloc] init] autorelease];
			[alert setMessageText:@"The selected virtual machine is already running."];
			[alert setAlertStyle:NSWarningAlertStyle];
			[alert beginSheetModalForWindow:[self window]
		                    modalDelegate:self
		                   didEndSelector:nil
		                      contextInfo:nil];
		} else {
			NSTask *sheep = [[NSTask alloc] init];
			[sheep setLaunchPath:[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"SheepShaver"]];
			[sheep setArguments:[NSArray arrayWithObject:path]];
			[sheep launch];
			[tasks setObject:sheep forKey:path];
		}
	}
}

- (void) onTaskTerminated: (NSNotification *) notification
{
	NSArray *paths = [tasks allKeys];
	NSEnumerator *enumerator = [paths objectEnumerator];
	NSString *path;
	while ((path = [enumerator nextObject])) {
		NSTask *task = [tasks objectForKey:path];
		if (![task isRunning]) {
			[tasks removeObjectForKey:path];
			[task release];
		}
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

- (IBAction) revealVirtualMachineInFinder: (id) sender
{
	int selectedRow = [vmList selectedRow];
	if (selectedRow >= 0) {
		[[NSWorkspace sharedWorkspace] selectFile: [vmArray objectAtIndex:selectedRow] inFileViewerRootedAtPath: @""];
	}
}

@end
