/*
 *  extfs.cpp - MacOS file system for native file system access
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

/*
 *  SEE ALSO
 *    Guide to the File System Manager (from FSM 1.2 SDK)
 *
 *  TODO
 *    LockRng
 *    UnlockRng
 *    (CatSearch)
 *    (MakeFSSpec)
 *    (GetVolMountInfoSize)
 *    (GetVolMountInfo)
 *    (GetForeignPrivs)
 *    (SetForeignPrivs)
 */

#include "sysdeps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#ifndef WIN32
#include <unistd.h>
#include <dirent.h>
#endif

#if defined __APPLE__ && defined __MACH__
#include <sys/attr.h>
#endif

#include "cpu_emulation.h"
#include "emul_op.h"
#include "main.h"
#include "disk.h"
#include "prefs.h"
#include "user_strings.h"
#include "extfs.h"
#include "extfs_defs.h"

#ifdef WIN32
# include "posix_emu.h"
#endif

#define DEBUG 0
#include "debug.h"


// File system global data and 68k routines
enum {
	fsCommProcStub = 0,
	fsHFSProcStub = 6,
	fsDrvStatus = 12,				// Drive Status record
	fsFSD = 42,						// File system descriptor
	fsPB = 238,						// IOParam (for mounting and renaming), also used for temporary storage
	fsVMI = 288,					// VoumeMountInfoHeader (for mounting)
	fsParseRec = 296,				// ParsePathRec struct
	fsReturn = 306,					// Area for return data of 68k routines
	fsAllocateVCB = 562,			// UTAllocateVCB(uint16 *sysVCBLength{a0}, uint32 *vcb{a1})
	fsAddNewVCB = 578,				// UTAddNewVCB(int drive_number{d0}, int16 *vRefNum{a1}, uint32 vcb{a1})
	fsDetermineVol = 594,			// UTDetermineVol(uint32 pb{a0}, int16 *status{a1}, int16 *more_matches{a2}, int16 *vRefNum{a3}, uint32 *vcb{a4})
	fsResolveWDCB = 614,			// UTResolveWDCB(uint32 procID{d0}, int16 index{d1}, int16 vRefNum{d0}, uint32 *wdcb{a0})
	fsGetDefaultVol = 632,			// UTGetDefaultVol(uint32 wdpb{a0})
	fsGetPathComponentName = 644,	// UTGetPathComponentName(uint32 rec{a0})
	fsParsePathname = 656,			// UTParsePathname(uint32 *start{a0}, uint32 name{a1})
	fsDisposeVCB = 670,				// UTDisposeVCB(uint32 vcb{a0})
	fsCheckWDRefNum = 682,			// UTCheckWDRefNum(int16 refNum{d0})
	fsSetDefaultVol = 694,			// UTSetDefaultVol(uint32 dummy{d0}, int32 dirID{d1}, int16 refNum{d2})
	fsAllocateFCB = 710,			// UTAllocateFCB(int16 *refNum{a0}, uint32 *fcb{a1})
	fsReleaseFCB = 724,				// UTReleaseFCB(int16 refNum{d0})
	fsIndexFCB = 736,				// UTIndexFCB(uint32 vcb{a0}, int16 *refNum{a1}, uint32 *fcb{a2})
	fsResolveFCB = 752,				// UTResolveFCB(int16 refNum{d0}, uint32 *fcb{a0})
	fsAdjustEOF = 766,				// UTAdjustEOF(int16 refNum{d0})
	fsAllocateWDCB = 778,			// UTAllocateWDCB(uint32 pb{a0})
	fsReleaseWDCB = 790,			// UTReleaseWDCB(int16 vRefNum{d0})
	SIZEOF_fsdat = 802
};

static uint32 fs_data = 0;		// Mac address of global data


// File system and volume name
static char FS_NAME[32], VOLUME_NAME[32];

// This directory is our root (read from prefs)
static const char *RootPath;
static bool ready = false;
static struct stat root_stat;

// File system ID/media type
const int16 MY_FSID = EMULATOR_ID_2;
const uint32 MY_MEDIA_TYPE = EMULATOR_ID_4;

// CNID of root and root's parent
const uint32 ROOT_ID = 2;
const uint32 ROOT_PARENT_ID = 1;

// File system stack size
const int STACK_SIZE = 0x10000;

// Allocation block and clump size as reported to MacOS (these are of course
// not the real values and have no meaning on the host OS)
const int AL_BLK_SIZE = 0x4000;
const int CLUMP_SIZE = 0x4000;

// Drive number of our pseudo-drive
static int drive_number;


// Disk/drive icon
const uint8 ExtFSIcon[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe,
	0x80, 0x00, 0x00, 0x91, 0x80, 0x00, 0x00, 0x91, 0x80, 0x00, 0x01, 0x21, 0x80, 0x00, 0x01, 0x21,
	0x80, 0x00, 0x02, 0x41, 0x8c, 0x00, 0x02, 0x41, 0x80, 0x00, 0x04, 0x81, 0x80, 0x00, 0x04, 0x81,
	0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x7f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


// These objects are used to map CNIDs to path names
struct FSItem {
	FSItem *next;			// Pointer to next FSItem in list
	uint32 id;				// CNID of this file/dir
	uint32 parent_id;		// CNID of parent file/dir
	FSItem *parent;			// Pointer to parent
	char name[32];			// Object name (C string) - Host OS
	char guest_name[32];			// Object name (C string) - Guest OS
	time_t mtime;			// Modification time for get_cat_info caching
	int cache_dircount;		// Cached number of files in directory
};

static FSItem *first_fs_item, *last_fs_item;

static uint32 next_cnid = fsUsrCNID;	// Next available CNID


/*
 *  Get object creation time
 */

#if defined __APPLE__ && defined __MACH__
struct crtimebuf {
	unsigned long length;
	struct timespec crtime;
};

static uint32 do_get_creation_time(const char *path)
{
	struct attrlist attr;
	memset(&attr, 0, sizeof(attr));
	attr.bitmapcount = ATTR_BIT_MAP_COUNT;
	attr.commonattr = ATTR_CMN_CRTIME;

	crtimebuf buf;
	if (getattrlist(path, &attr, &buf, sizeof(buf), FSOPT_NOFOLLOW) < 0)
		return 0;
	return TimeToMacTime(buf.crtime.tv_sec);
}

static uint32 get_creation_time(const char *path)
{
	if (path == NULL)
		return 0;
	if (path == RootPath) {
		static uint32 root_crtime = UINT_MAX;
		if (root_crtime == UINT_MAX)
			root_crtime = do_get_creation_time(path);
		return root_crtime;
	}
	return do_get_creation_time(path);
}
#endif


/*
 *  Find FSItem for given CNID
 */

static FSItem *find_fsitem_by_id(uint32 cnid)
{
	FSItem *p = first_fs_item;
	while (p) {
		if (p->id == cnid)
			return p;
		p = p->next;
	}
	return NULL;
}

/*
 *  Create FSItem with the given parameters
 */

static FSItem *create_fsitem(const char *name, const char *guest_name, FSItem *parent)
{
	FSItem *p = new FSItem;
	last_fs_item->next = p;
	p->next = NULL;
	last_fs_item = p;
	p->id = next_cnid++;
	p->parent_id = parent->id;
	p->parent = parent;
	strncpy(p->name, name, 31);
	p->name[31] = 0;
	strncpy(p->guest_name, guest_name, 31);
	p->guest_name[31] = 0;
	p->mtime = 0;
	return p;
}

/*
 *  Find FSItem for given name and parent, construct new FSItem if not found
 */

static FSItem *find_fsitem(const char *name, FSItem *parent)
{
	FSItem *p = first_fs_item;
	while (p) {
		if (p->parent == parent && !strcmp(p->name, name))
			return p;
		p = p->next;
	}

	// Not found, construct new FSItem
	return create_fsitem(name, host_encoding_to_macroman(name), parent);
}

/*
 *  Find FSItem for given guest_name and parent, construct new FSItem if not found
 */

static FSItem *find_fsitem_guest(const char *guest_name, FSItem *parent)
{
	FSItem *p = first_fs_item;
	while (p) {
		if (p->parent == parent && !strcmp(p->guest_name, guest_name))
			return p;
		p = p->next;
	}

	// Not found, construct new FSItem
	return create_fsitem(macroman_to_host_encoding(guest_name), guest_name, parent);
}

/*
 *  Get full path (->full_path) for given FSItem
 */

static char full_path[MAX_PATH_LENGTH];

static void add_path_comp(const char *s)
{
	add_path_component(full_path, s);
}

static void get_path_for_fsitem(FSItem *p)
{
	if (p->id == ROOT_PARENT_ID) {
		full_path[0] = 0;
	} else if (p->id == ROOT_ID) {
		strncpy(full_path, RootPath, MAX_PATH_LENGTH-1);
		full_path[MAX_PATH_LENGTH-1] = 0;
	} else {
		get_path_for_fsitem(p->parent);
		add_path_comp(p->name);
	}
}


/*
 *  Exchange parent CNIDs in all FSItems
 */

static void swap_parent_ids(uint32 parent1, uint32 parent2)
{
	FSItem *p = first_fs_item;
	while (p) {
		if (p->parent_id == parent1)
			p->parent_id = parent2;
		else if (p->parent_id == parent2)
			p->parent_id = parent1;
		p = p->next;
	}
}


/*
 *  String handling functions
 */

// Copy pascal string
static void pstrcpy(char *dst, const char *src)
{
	int size = *dst++ = *src++;
	while (size--)
		*dst++ = *src++;
}

// Convert C string to pascal string
static void cstr2pstr(char *dst, const char *src)
{
	*dst++ = strlen(src);
	char c;
	while ((c = *src++) != 0) {
		// Note: we are converting host ':' characters to Mac '/' characters here
		// '/' is not a path separator as this function is only used on object names
		if (c == ':')
			c = '/';
		*dst++ = c;
	}
}

// Convert string (no length byte) to C string, length given separately
static void strn2cstr(char *dst, const char *src, int size)
{
	while (size--) {
		char c = *src++;
		// Note: we are converting Mac '/' characters to host ':' characters here
		// '/' is not a path separator as this function is only used on object names
		if (c == '/')
			c = ':';
		*dst++ = c;
	}
	*dst = 0;
}


/*
 *  Convert errno to MacOS error code
 */

static int16 errno2oserr(void)
{
	D(bug(" errno %08x\n", errno));
	switch (errno) {
		case 0:
			return noErr;
		case ENOENT:
		case EISDIR:
			return fnfErr;
		case EACCES:
		case EPERM:
			return permErr;
		case EEXIST:
			return dupFNErr;
		case EBUSY:
		case ENOTEMPTY:
			return fBsyErr;
		case ENOSPC:
			return dskFulErr;
		case EROFS:
			return wPrErr;
		case EMFILE:
			return tmfoErr;
		case ENOMEM:
			return -108;
		case EIO:
		default:
			return ioErr;
	}
}


/*
 *  Initialization
 */

void ExtFSInit(void)
{
	// System specific initialization
	extfs_init();

	// Get file system and volume name
	cstr2pstr(FS_NAME, GetString(STR_EXTFS_NAME));
	cstr2pstr(VOLUME_NAME, GetString(STR_EXTFS_VOLUME_NAME));

	// Create root's parent FSItem
	FSItem *p = new FSItem;
	first_fs_item = last_fs_item = p;
	p->next = NULL;
	p->id = ROOT_PARENT_ID;
	p->parent_id = 0;
	p->parent = NULL;
	p->name[0] = 0;
	p->guest_name[0] = 0;

	// Create root FSItem
	p = new FSItem;
	last_fs_item->next = p;
	p->next = NULL;
	last_fs_item = p;
	p->id = ROOT_ID;
	p->parent_id = ROOT_PARENT_ID;
	p->parent = first_fs_item;
	strncpy(p->name, GetString(STR_EXTFS_VOLUME_NAME), 32);
	p->name[31] = 0;
	strncpy(p->guest_name, host_encoding_to_macroman(p->name), 32);
	p->guest_name[31] = 0;

	// Find path for root
	if ((RootPath = PrefsFindString("extfs")) != NULL) {
		if (stat(RootPath, &root_stat))
			return;
		if (!S_ISDIR(root_stat.st_mode))
			return;
		ready = true;
	}
}


/*
 *  Deinitialization
 */

void ExtFSExit(void)
{
	// Delete all FSItems
	FSItem *p = first_fs_item, *next;
	while (p) {
		next = p->next;
		delete p;
		p = next;
	}
	first_fs_item = last_fs_item = NULL;

	// System specific deinitialization
	extfs_exit();
}


/*
 *  Install file system
 */

void InstallExtFS(void)
{
	int num_blocks = 0xffff;	// Fake number of blocks of our drive
	M68kRegisters r;

	D(bug("InstallExtFS\n"));
	if (!ready)
		return;

	// FSM present?
	r.d[0] = gestaltFSAttr;
	Execute68kTrap(0xa1ad, &r);	// Gestalt()
	D(bug("FSAttr %d, %08x\n", r.d[0], r.a[0]));
	if ((r.d[0] & 0xffff) || !(r.a[0] & (1 << gestaltHasFileSystemManager))) {
		printf("WARNING: No FSM present, disabling ExtFS\n");
		return;
	}

	// Yes, version >=1.2?
	r.d[0] = gestaltFSMVersion;
	Execute68kTrap(0xa1ad, &r);	// Gestalt()
	D(bug("FSMVersion %d, %08x\n", r.d[0], r.a[0]));
	if ((r.d[0] & 0xffff) || (r.a[0] < 0x0120)) {
		printf("WARNING: FSM <1.2 found, disabling ExtFS\n");
		return;
	}

	D(bug("FSM present\n"));

	// Yes, allocate file system stack
	r.d[0] = STACK_SIZE;
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	if (r.a[0] == 0)
		return;
	uint32 fs_stack = r.a[0];

	// Allocate memory for our data structures and 68k code
	r.d[0] = SIZEOF_fsdat;
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	if (r.a[0] == 0)
		return;
	fs_data = r.a[0];

	// Set up 68k code fragments
	int p = fs_data + fsCommProcStub;
	WriteMacInt16(p, M68K_EMUL_OP_EXTFS_COMM); p += 2;
	WriteMacInt16(p, M68K_RTD); p += 2;
	WriteMacInt16(p, 10); p += 2;
	if (p - fs_data != fsHFSProcStub)
		goto fsdat_error;
	WriteMacInt16(p, M68K_EMUL_OP_EXTFS_HFS); p += 2;
	WriteMacInt16(p, M68K_RTD); p += 2;
	WriteMacInt16(p, 16);
	p = fs_data + fsAllocateVCB;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(sp)
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x7006); p+= 2;	// UTAllocateVCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsAddNewVCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(a7)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(a7)
	WriteMacInt16(p, 0x7007); p+= 2;	// UTAddNewVCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsDetermineVol)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(sp)
	WriteMacInt16(p, 0x2f0a); p+= 2;	// move.l a2,-(sp)
	WriteMacInt16(p, 0x2f0b); p+= 2;	// move.l a3,-(sp)
	WriteMacInt16(p, 0x2f0c); p+= 2;	// move.l a4,-(sp)
	WriteMacInt16(p, 0x701d); p+= 2;	// UTDetermineVol
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsResolveWDCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f00); p+= 2;	// move.l d0,-(sp)
	WriteMacInt16(p, 0x3f01); p+= 2;	// move.w d1,-(sp)
	WriteMacInt16(p, 0x3f02); p+= 2;	// move.w d2,-(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x700e); p+= 2;	// UTResolveWDCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsGetDefaultVol)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x7012); p+= 2;	// UTGetDefaultVol
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsGetPathComponentName)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x701c); p+= 2;	// UTGetPathComponentName
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsParsePathname)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(sp)
	WriteMacInt16(p, 0x701b); p+= 2;	// UTParsePathname
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsDisposeVCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x7008); p+= 2;	// UTDisposeVCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsCheckWDRefNum)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x7013); p+= 2;	// UTCheckWDRefNum
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsSetDefaultVol)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f00); p+= 2;	// move.l d0,-(sp)
	WriteMacInt16(p, 0x2f01); p+= 2;	// move.l d1,-(sp)
	WriteMacInt16(p, 0x3f02); p+= 2;	// move.w d2,-(sp)
	WriteMacInt16(p, 0x7011); p+= 2;	// UTSetDefaultVol
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsAllocateFCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(sp)
	WriteMacInt16(p, 0x7000); p+= 2;	// UTAllocateFCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsReleaseFCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x7001); p+= 2;	// UTReleaseFCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsIndexFCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x2f09); p+= 2;	// move.l a1,-(sp)
	WriteMacInt16(p, 0x2f0a); p+= 2;	// move.l a2,-(sp)
	WriteMacInt16(p, 0x7004); p+= 2;	// UTIndexFCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsResolveFCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x7005); p+= 2;	// UTResolveFCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsAdjustEOF)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x7010); p+= 2;	// UTAdjustEOF
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsAllocateWDCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x2f08); p+= 2;	// move.l a0,-(sp)
	WriteMacInt16(p, 0x700c); p+= 2;	// UTAllocateWDCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != fsReleaseWDCB)
		goto fsdat_error;
	WriteMacInt16(p, 0x4267); p+= 2;	// clr.w -(sp)
	WriteMacInt16(p, 0x3f00); p+= 2;	// move.w d0,-(sp)
	WriteMacInt16(p, 0x700d); p+= 2;	// UTReleaseWDCB
	WriteMacInt16(p, 0xa824); p+= 2;	// FSMgr
	WriteMacInt16(p, 0x301f); p+= 2;	// move.w (sp)+,d0
	WriteMacInt16(p, M68K_RTS); p+= 2;
	if (p - fs_data != SIZEOF_fsdat)
		goto fsdat_error;

	// Set up drive status
	WriteMacInt8(fs_data + fsDrvStatus + dsDiskInPlace, 8);	// Fixed disk
	WriteMacInt8(fs_data + fsDrvStatus + dsInstalled, 1);
	WriteMacInt16(fs_data + fsDrvStatus + dsQType, hard20);
	WriteMacInt16(fs_data + fsDrvStatus + dsDriveSize, num_blocks & 0xffff);
	WriteMacInt16(fs_data + fsDrvStatus + dsDriveS1, num_blocks >> 16);
	WriteMacInt16(fs_data + fsDrvStatus + dsQFSID, MY_FSID);

	// Add drive to drive queue
	drive_number = FindFreeDriveNumber(1);
	D(bug(" adding drive %d\n", drive_number));
	r.d[0] = (drive_number << 16) | (DiskRefNum & 0xffff);
	r.a[0] = fs_data + fsDrvStatus + dsQLink;
	Execute68kTrap(0xa04e, &r);	// AddDrive()

	// Init FSDRec and install file system
	D(bug(" installing file system\n"));
	WriteMacInt16(fs_data + fsFSD + fsdLength, SIZEOF_FSDRec);
	WriteMacInt16(fs_data + fsFSD + fsdVersion, fsdVersion1);
	WriteMacInt16(fs_data + fsFSD + fileSystemFSID, MY_FSID);
	Host2Mac_memcpy(fs_data + fsFSD + fileSystemName, FS_NAME, 32);
	WriteMacInt32(fs_data + fsFSD + fileSystemCommProc, fs_data + fsCommProcStub);
	WriteMacInt32(fs_data + fsFSD + fsdHFSCI + compInterfProc, fs_data + fsHFSProcStub);
	WriteMacInt32(fs_data + fsFSD + fsdHFSCI + stackTop, fs_stack + STACK_SIZE);
	WriteMacInt32(fs_data + fsFSD + fsdHFSCI + stackSize, STACK_SIZE);
	WriteMacInt32(fs_data + fsFSD + fsdHFSCI + idSector, (uint32)-1);
	r.a[0] = fs_data + fsFSD;
	r.d[0] = 0;					// InstallFS
	Execute68kTrap(0xa0ac, &r);	// FSMDispatch()
	D(bug(" InstallFS() returned %d\n", r.d[0]));

	// Enable HFS component
	D(bug(" enabling HFS component\n"));
	WriteMacInt32(fs_data + fsFSD + fsdHFSCI + compInterfMask, ReadMacInt32(fs_data + fsFSD + fsdHFSCI + compInterfMask) | (fsmComponentEnableMask | hfsCIResourceLoadedMask | hfsCIDoesHFSMask));
	r.a[0] = fs_data + fsFSD;
	r.d[3] = SIZEOF_FSDRec;
	r.d[4] = MY_FSID;
	r.d[0] = 5;					// SetFSInfo
	Execute68kTrap(0xa0ac, &r);	// FSMDispatch()
	D(bug(" SetFSInfo() returned %d\n", r.d[0]));

	// Mount volume
	D(bug(" mounting volume\n"));
	WriteMacInt32(fs_data + fsPB + ioBuffer, fs_data + fsVMI);
	WriteMacInt16(fs_data + fsVMI + vmiLength, SIZEOF_VolumeMountInfoHeader);
	WriteMacInt32(fs_data + fsVMI + vmiMedia, MY_MEDIA_TYPE);
	r.a[0] = fs_data + fsPB;
	r.d[0] = 0x41;				// PBVolumeMount
	Execute68kTrap(0xa260, &r);	// HFSDispatch()
	D(bug(" PBVolumeMount() returned %d\n", r.d[0]));
	return;

fsdat_error:
	printf("FATAL: ExtFS data block initialization error\n");
	QuitEmulator();
}


/*
 *  FS communications function
 */

int16 ExtFSComm(uint16 message, uint32 paramBlock, uint32 globalsPtr)
{
	D(bug("ExtFSComm(%d, %08lx, %08lx)\n", message, paramBlock, globalsPtr));

	switch (message) {
		case ffsNopMessage:
		case ffsLoadMessage:
		case ffsUnloadMessage:
			return noErr;

		case ffsGetIconMessage: {		// Get disk/drive icon
			if (ReadMacInt8(paramBlock + iconType) == kLargeIcon && ReadMacInt32(paramBlock + requestSize) >= sizeof(ExtFSIcon)) {
				Host2Mac_memcpy(ReadMacInt32(paramBlock + iconBufferPtr), ExtFSIcon, sizeof(ExtFSIcon));
				WriteMacInt32(paramBlock + actualSize, sizeof(ExtFSIcon));
				return noErr;
			} else
				return -5012;	// afpItemNotFound
		}

		case ffsIDDiskMessage: {		// Check if volume is handled by our FS
			if ((int16)ReadMacInt16(paramBlock + ioVRefNum) == drive_number)
				return noErr;
			else
				return extFSErr;
		}

		case ffsIDVolMountMessage: {	// Check if volume can be mounted by our FS
			if (ReadMacInt32(ReadMacInt32(paramBlock + ioBuffer) + vmiMedia) == MY_MEDIA_TYPE)
				return noErr;
			else
				return extFSErr;
		}

		default:
			return fsmUnknownFSMMessageErr;
	}
}


/*
 *  Get current directory specified by given ParamBlock/dirID
 */

static int16 get_current_dir(uint32 pb, uint32 dirID, uint32 &current_dir, bool no_vol_name = false)
{
	M68kRegisters r;
	int16 result;

	// Determine volume
	D(bug("  determining volume, dirID %d\n", dirID));
	r.a[0] = pb;
	r.a[1] = fs_data + fsReturn;
	r.a[2] = fs_data + fsReturn + 2;
	r.a[3] = fs_data + fsReturn + 4;
	r.a[4] = fs_data + fsReturn + 6;
	uint32 name_ptr = 0;
	if (no_vol_name) {
		name_ptr = ReadMacInt32(pb + ioNamePtr);
		WriteMacInt32(pb + ioNamePtr, 0);
	}
	Execute68k(fs_data + fsDetermineVol, &r);
	if (no_vol_name)
		WriteMacInt32(pb + ioNamePtr, name_ptr);
	int16 status = ReadMacInt16(fs_data + fsReturn);
	D(bug("  UTDetermineVol() returned %d, status %d\n", r.d[0], status));
	result = (int16)(r.d[0] & 0xffff);

	if (result == noErr) {
		switch (status) {
			case dtmvFullPathname:	// Determined by full pathname
				current_dir = ROOT_ID;
				break;

			case dtmvVRefNum:		// Determined by refNum or by drive number
			case dtmvDriveNum:
				current_dir = dirID ? dirID : ROOT_ID;
				break;

			case dtmvWDRefNum:		// Determined by working directory refNum
				if (dirID)
					current_dir = dirID;
				else {
					D(bug("  resolving WDCB\n"));
					r.d[0] = 0;
					r.d[1] = 0;
					r.d[2] = ReadMacInt16(pb + ioVRefNum);
					r.a[0] = fs_data + fsReturn;
					Execute68k(fs_data + fsResolveWDCB, &r);
					uint32 wdcb = ReadMacInt32(fs_data + fsReturn);
					D(bug("  UTResolveWDCB() returned %d, dirID %d\n", r.d[0], ReadMacInt32(wdcb + wdDirID)));
					result = (int16)(r.d[0] & 0xffff);
					if (result == noErr)
						current_dir = ReadMacInt32(wdcb + wdDirID);
				}
				break;

			case dtmvDefault:		// Determined by default volume
				if (dirID)
					current_dir = dirID;
				else {
					uint32 wdpb = fs_data + fsReturn;
					WriteMacInt32(wdpb + ioNamePtr, 0);
					D(bug("  getting default volume\n"));
					r.a[0] = wdpb;
					Execute68k(fs_data + fsGetDefaultVol, &r);
					D(bug("  UTGetDefaultVol() returned %d, dirID %d\n", r.d[0], ReadMacInt32(wdpb + ioWDDirID)));
					result = (int16)(r.d[0] & 0xffff);
					if (result == noErr)
						current_dir = ReadMacInt32(wdpb + ioWDDirID);
				}
				break;

			default:
				result = paramErr;
				break;
		}
	}
	return result;
}


/*
 *  Get path component name
 */

static int16 get_path_component_name(uint32 rec)
{
//	D(bug("  getting path component\n"));
	M68kRegisters r;
	r.a[0] = rec;
	Execute68k(fs_data + fsGetPathComponentName, &r);
//	D(bug("  UTGetPathComponentName returned %d\n", r.d[0]));
	return (int16)(r.d[0] & 0xffff);
}


/*
 *  Get FSItem and full path (->full_path) for file/dir specified in ParamBlock
 */

static int16 get_item_and_path(uint32 pb, uint32 dirID, FSItem *&item, bool no_vol_name = false)
{
	M68kRegisters r;

	// Find FSItem for parent directory
	int16 result;
	uint32 current_dir;
	if ((result = get_current_dir(pb, dirID, current_dir, no_vol_name)) != noErr)
		return result;
	D(bug("  current dir %08x\n", current_dir));
	FSItem *p = find_fsitem_by_id(current_dir);
	if (p == NULL)
		return dirNFErr;

	// Start parsing
	uint32 parseRec = fs_data + fsParseRec;
	WriteMacInt32(parseRec + ppNamePtr, ReadMacInt32(pb + ioNamePtr));
	WriteMacInt16(parseRec + ppStartOffset, 0);
	WriteMacInt16(parseRec + ppComponentLength, 0);
	WriteMacInt8(parseRec + ppMoreName, false);
	WriteMacInt8(parseRec + ppFoundDelimiter, false);

	// Get length of volume name
	D(bug("  parsing pathname\n"));
	r.a[0] = parseRec + ppStartOffset;
	r.a[1] = ReadMacInt32(parseRec + ppNamePtr);
	Execute68k(fs_data + fsParsePathname, &r);
	D(bug("  UTParsePathname() returned %d, startOffset %d\n", r.d[0], ReadMacInt16(parseRec + ppStartOffset)));
	result = (int16)(r.d[0] & 0xffff);
	if (result == noErr) {

		// Check for leading delimiter of the partial pathname
		result = get_path_component_name(parseRec);
		if (result == noErr) {
			if (ReadMacInt16(parseRec + ppComponentLength) == 0 && ReadMacInt8(parseRec + ppFoundDelimiter)) {
				// Get past initial delimiter
				WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + 1);
			}

			// Parse until there is no more pathname to parse
			while ((result == noErr) && ReadMacInt8(parseRec + ppMoreName)) {

				// Search for the next delimiter from startOffset
				result = get_path_component_name(parseRec);
				if (result == noErr) {
					if (ReadMacInt16(parseRec + ppComponentLength) == 0) {

						// Delimiter immediately following another delimiter, get parent
						if (current_dir != ROOT_ID) {
							p = p->parent;
							current_dir = p->id;
						} else
							result = bdNamErr;

						// startOffset = start of next component
						WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + 1);

					} else if (ReadMacInt8(parseRec + ppMoreName)) {

						// Component found and isn't the last, so it must be a directory, enter it
						char name[32];
						strn2cstr(name, (char *)Mac2HostAddr(ReadMacInt32(parseRec + ppNamePtr)) + ReadMacInt16(parseRec + ppStartOffset) + 1, ReadMacInt16(parseRec + ppComponentLength));
						D(bug("  entering %s\n", name));
						p = find_fsitem_guest(name, p);
						current_dir = p->id;

						// startOffset = start of next component
						WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + ReadMacInt16(parseRec + ppComponentLength) + 1);
					}
				}
			}

			if (result == noErr) {

				// There is no more pathname to parse
				if (ReadMacInt16(parseRec + ppComponentLength) == 0) {

					// Pathname ended with '::' or was simply a volume name, so current directory is the object
					item = p;

				} else {

					// Pathname ended with 'name:' or 'name', so name is the object
					char name[32];
					strn2cstr(name, (char *)Mac2HostAddr(ReadMacInt32(parseRec + ppNamePtr)) + ReadMacInt16(parseRec + ppStartOffset) + 1, ReadMacInt16(parseRec + ppComponentLength));
					D(bug("  object is %s\n", name));
					item = find_fsitem_guest(name, p);
				}
			}
		}

	} else {

		// Default to bad name
		result = bdNamErr;

		if (ReadMacInt32(pb + ioNamePtr) == 0 || ReadMacInt8(ReadMacInt32(pb + ioNamePtr)) == 0) {

			// Pathname was NULL or a zero length string, so we found a directory at the end of the string
			item = p;
			result = noErr;
		}
	}

	// Eat the path
	if (result == noErr) {
		get_path_for_fsitem(item);
		D(bug("  path %s\n", full_path));
	}
	return result;
}


/*
 *  Find FCB for given file RefNum
 */

static uint32 find_fcb(int16 refNum)
{
	D(bug("  finding FCB\n"));
	M68kRegisters r;
	r.d[0] = refNum;
	r.a[0] = fs_data + fsReturn;
	Execute68k(fs_data + fsResolveFCB, &r);
	uint32 fcb = ReadMacInt32(fs_data + fsReturn);
	D(bug("  UTResolveFCB() returned %d, fcb %08lx\n", r.d[0], fcb));
	if (r.d[0] & 0xffff)
		return 0;
	else
		return fcb;
}


/*
 *  HFS interface functions
 */

// Check if volume belongs to our FS
static int16 fs_mount_vol(uint32 pb)
{
	D(bug(" fs_mount_vol(%08lx), vRefNum %d\n", pb, ReadMacInt16(pb + ioVRefNum)));
	if ((int16)ReadMacInt16(pb + ioVRefNum) == drive_number)
		return noErr;
	else
		return extFSErr;
}

// Mount volume
static int16 fs_volume_mount(uint32 pb)
{
	D(bug(" fs_volume_mount(%08lx)\n", pb));
	M68kRegisters r;

	// Create new VCB
	D(bug("  creating VCB\n"));
	r.a[0] = fs_data + fsReturn;
	r.a[1] = fs_data + fsReturn + 2;
	Execute68k(fs_data + fsAllocateVCB, &r);
#if DEBUG
	uint16 sysVCBLength = ReadMacInt16(fs_data + fsReturn);
#endif
	uint32 vcb = ReadMacInt32(fs_data + fsReturn + 2);
	D(bug("  UTAllocateVCB() returned %d, vcb %08lx, size %d\n", r.d[0], vcb, sysVCBLength));
	if (r.d[0] & 0xffff)
		return (int16)r.d[0];

	// Init VCB
	WriteMacInt16(vcb + vcbSigWord, 0x4244);
#if defined(__BEOS__) || defined(WIN32)
	WriteMacInt32(vcb + vcbCrDate, TimeToMacTime(root_stat.st_crtime));
#elif defined __APPLE__ && defined __MACH__
	WriteMacInt32(vcb + vcbCrDate, get_creation_time(RootPath));
#else
	WriteMacInt32(vcb + vcbCrDate, 0);
#endif
	WriteMacInt32(vcb + vcbLsMod, TimeToMacTime(root_stat.st_mtime));
	WriteMacInt32(vcb + vcbVolBkUp, 0);
	WriteMacInt16(vcb + vcbNmFls, 1);			//!!
	WriteMacInt16(vcb + vcbNmRtDirs, 1);		//!!
	WriteMacInt16(vcb + vcbNmAlBlks, 0xffff);	//!!
	WriteMacInt32(vcb + vcbAlBlkSiz, AL_BLK_SIZE);
	WriteMacInt32(vcb + vcbClpSiz, CLUMP_SIZE);
	WriteMacInt32(vcb + vcbNxtCNID, next_cnid);
	WriteMacInt16(vcb + vcbFreeBks, 0xffff);	//!!
	Host2Mac_memcpy(vcb + vcbVN, VOLUME_NAME, 28);
	WriteMacInt16(vcb + vcbFSID, MY_FSID);
	WriteMacInt32(vcb + vcbFilCnt, 1);			//!!
	WriteMacInt32(vcb + vcbDirCnt, 1);			//!!

	// Add VCB to VCB queue
	D(bug("  adding VCB to queue\n"));
	r.d[0] = drive_number;
	r.a[0] = fs_data + fsReturn;
	r.a[1] = vcb;
	Execute68k(fs_data + fsAddNewVCB, &r);
	int16 vRefNum = (int16)ReadMacInt32(fs_data + fsReturn);
	D(bug("  UTAddNewVCB() returned %d, vRefNum %d\n", r.d[0], vRefNum));
	if (r.d[0] & 0xffff)
		return (int16)r.d[0];

	// Post diskInsertEvent
	D(bug("  posting diskInsertEvent\n"));
	r.d[0] = drive_number;
	r.a[0] = 7;	// diskEvent
	Execute68kTrap(0xa02f, &r);		// PostEvent()

	// Return volume RefNum
	WriteMacInt16(pb + ioVRefNum, vRefNum);
	return noErr;
}

// Unmount volume
static int16 fs_unmount_vol(uint32 vcb)
{
	D(bug(" fs_unmount_vol(%08lx), vRefNum %d\n", vcb, ReadMacInt16(vcb + vcbVRefNum)));
	M68kRegisters r;

	// Remove and free VCB
	D(bug("  freeing VCB\n"));
	r.a[0] = vcb;
	Execute68k(fs_data + fsDisposeVCB, &r);
	D(bug("  UTDisposeVCB() returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Get information about a volume (HVolumeParam)
static int16 fs_get_vol_info(uint32 pb, bool hfs)
{
//	D(bug(" fs_get_vol_info(%08lx)\n", pb));

	// Fill in struct
	if (ReadMacInt32(pb + ioNamePtr))
		pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), VOLUME_NAME);
#if defined(__BEOS__) || defined(WIN32)
	WriteMacInt32(pb + ioVCrDate, TimeToMacTime(root_stat.st_crtime));
#elif defined __APPLE__ && defined __MACH__
	WriteMacInt32(pb + ioVCrDate, get_creation_time(RootPath));
#else
	WriteMacInt32(pb + ioVCrDate, 0);
#endif
	WriteMacInt32(pb + ioVLsMod, TimeToMacTime(root_stat.st_mtime));
	WriteMacInt16(pb + ioVAtrb, 0);
	WriteMacInt16(pb + ioVNmFls, 1);			//!!
	WriteMacInt16(pb + ioVBitMap, 0);
	WriteMacInt16(pb + ioAllocPtr, 0);
	WriteMacInt16(pb + ioVNmAlBlks, 0xffff);	//!!
	WriteMacInt32(pb + ioVAlBlkSiz, AL_BLK_SIZE);
	WriteMacInt32(pb + ioVClpSiz, CLUMP_SIZE);
	WriteMacInt16(pb + ioAlBlSt, 0);
	WriteMacInt32(pb + ioVNxtCNID, next_cnid);
	WriteMacInt16(pb + ioVFrBlk, 0xffff);		//!!
	if (hfs) {
		WriteMacInt16(pb + ioVDrvInfo, drive_number);
		WriteMacInt16(pb + ioVDRefNum, ReadMacInt16(fs_data + fsDrvStatus + dsQRefNum));
		WriteMacInt16(pb + ioVFSID, MY_FSID);
		WriteMacInt32(pb + ioVBkUp, 0);
		WriteMacInt16(pb + ioVSeqNum, 0);
		WriteMacInt32(pb + ioVWrCnt, 0);
		WriteMacInt32(pb + ioVFilCnt, 1);			//!!
		WriteMacInt32(pb + ioVDirCnt, 1);			//!!
		Mac_memset(pb + ioVFndrInfo, 0, 32);
	}
	return noErr;
}

// Change volume information (HVolumeParam)
static int16 fs_set_vol_info(uint32 pb)
{
	D(bug(" fs_set_vol_info(%08lx)\n", pb));

	//!! times
	return noErr;
}

// Get volume parameter block
static int16 fs_get_vol_parms(uint32 pb)
{
//	D(bug(" fs_get_vol_parms(%08lx)\n", pb));

	// Return parameter block
	uint32 actual = ReadMacInt32(pb + ioReqCount);
	if (actual > SIZEOF_GetVolParmsInfoBuffer)
		actual = SIZEOF_GetVolParmsInfoBuffer;
	WriteMacInt32(pb + ioActCount, actual);
	uint32 p = ReadMacInt32(pb + ioBuffer);
	if (actual > vMVersion) WriteMacInt16(p + vMVersion, 2);
	if (actual > vMAttrib) WriteMacInt32(p + vMAttrib, kNoMiniFndr | kNoVNEdit | kNoLclSync | kTrshOffLine | kNoSwitchTo | kNoBootBlks | kNoSysDir | kHasExtFSVol);
	if (actual > vMLocalHand) WriteMacInt32(p + vMLocalHand, 0);
	if (actual > vMServerAdr) WriteMacInt32(p + vMServerAdr, 0);
	if (actual > vMVolumeGrade) WriteMacInt32(p + vMVolumeGrade, 0);
	if (actual > vMForeignPrivID) WriteMacInt16(p + vMForeignPrivID, 0);
	return noErr;
}

// Get default volume (WDParam)
static int16 fs_get_vol(uint32 pb)
{
	D(bug(" fs_get_vol(%08lx)\n", pb));
	M68kRegisters r;

	// Getting default volume
	D(bug("  getting default volume\n"));
	r.a[0] = pb;
	Execute68k(fs_data + fsGetDefaultVol, &r);
	D(bug("  UTGetDefaultVol() returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Set default volume (WDParam)
static int16 fs_set_vol(uint32 pb, bool hfs, uint32 vcb)
{
	D(bug(" fs_set_vol(%08lx), vRefNum %d, name %.31s, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt32(pb + ioWDDirID)));
	M68kRegisters r;

	// Determine parameters
	uint32 dirID;
	int16 refNum;
	if (hfs) {

		// Find FSItem for given dir
		FSItem *fs_item;
		int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioWDDirID), fs_item);
		if (result != noErr)
			return result;

		// Is it a directory?
		struct stat st;
		if (stat(full_path, &st))
			return dirNFErr;
		if (!S_ISDIR(st.st_mode))
			return dirNFErr;

		// Get dirID and refNum
		dirID = fs_item->id;
		refNum = ReadMacInt16(vcb + vcbVRefNum);

	} else {

		// Is the given vRefNum a working directory number?
		D(bug("  checking for WDRefNum\n"));
		r.d[0] = ReadMacInt16(pb + ioVRefNum);
		Execute68k(fs_data + fsCheckWDRefNum, &r);
		D(bug("  UTCheckWDRefNum() returned %d\n", r.d[0]));
		if (r.d[0] & 0xffff) {
			// Volume refNum
			dirID = ROOT_ID;
			refNum = ReadMacInt16(vcb + vcbVRefNum);
		} else {
			// WD refNum
			dirID = 0;
			refNum = ReadMacInt16(pb + ioVRefNum);
		}
	}

	// Setting default volume
	D(bug("  setting default volume\n"));
	r.d[0] = 0;
	r.d[1] = dirID;
	r.d[2] = refNum;
	Execute68k(fs_data + fsSetDefaultVol, &r);
	D(bug("  UTSetDefaultVol() returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Query file attributes (HFileParam)
static int16 fs_get_file_info(uint32 pb, bool hfs, uint32 dirID)
{
	D(bug(" fs_get_file_info(%08lx), vRefNum %d, name %.31s, idx %d, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt16(pb + ioFDirIndex), dirID));

	FSItem *fs_item;
	int16 dir_index = ReadMacInt16(pb + ioFDirIndex);
	if (dir_index <= 0) {		// Query item specified by ioDirID and ioNamePtr

		// Find FSItem for given file
		int16 result = get_item_and_path(pb, dirID, fs_item);
		if (result != noErr)
			return result;

	} else {					// Query item in directory specified by ioDirID by index

		// Find FSItem for parent directory
		int16 result;
		uint32 current_dir;
		if ((result = get_current_dir(pb, dirID, current_dir, true)) != noErr)
			return result;
		FSItem *p = find_fsitem_by_id(current_dir);
		if (p == NULL)
			return dirNFErr;
		get_path_for_fsitem(p);

		// Look for nth item in directory and add name to path
		DIR *d = opendir(full_path);
		if (d == NULL)
			return dirNFErr;
		struct dirent *de = NULL;
		for (int i=0; i<dir_index; i++) {
read_next_de:
			de = readdir(d);
			if (de == NULL) {
				closedir(d);
				return fnfErr;
			}
			if (de->d_name[0] == '.')
				goto read_next_de;	// Suppress names beginning with '.' (MacOS could interpret these as driver names)
			//!! suppress directories
		}
		add_path_comp(de->d_name);

		// Get FSItem for queried item
		fs_item = find_fsitem(de->d_name, p);
		closedir(d);
	}

	// Get stats
	struct stat st;
	if (stat(full_path, &st))
		return fnfErr;
	if (S_ISDIR(st.st_mode))
		return fnfErr;

	// Fill in struct from fs_item and stats
	if (ReadMacInt32(pb + ioNamePtr))
		cstr2pstr((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), fs_item->guest_name);
	WriteMacInt16(pb + ioFRefNum, 0);
	WriteMacInt8(pb + ioFlAttrib, access(full_path, W_OK) == 0 ? 0 : faLocked);
	WriteMacInt32(pb + ioDirID, fs_item->id);

#if defined(__BEOS__) || defined(WIN32)
	WriteMacInt32(pb + ioFlCrDat, TimeToMacTime(st.st_crtime));
#elif defined __APPLE__ && defined __MACH__
	WriteMacInt32(pb + ioFlCrDat, get_creation_time(full_path));
#else
	WriteMacInt32(pb + ioFlCrDat, 0);
#endif
	WriteMacInt32(pb + ioFlMdDat, TimeToMacTime(st.st_mtime));

	get_finfo(full_path, pb + ioFlFndrInfo, hfs ? pb + ioFlXFndrInfo : 0, false);

	WriteMacInt16(pb + ioFlStBlk, 0);
	WriteMacInt32(pb + ioFlLgLen, st.st_size);
	WriteMacInt32(pb + ioFlPyLen, (st.st_size | (AL_BLK_SIZE - 1)) + 1);
	WriteMacInt16(pb + ioFlRStBlk, 0);
	uint32 rf_size = get_rfork_size(full_path);
	WriteMacInt32(pb + ioFlRLgLen, rf_size);
	WriteMacInt32(pb + ioFlRPyLen, (rf_size | (AL_BLK_SIZE - 1)) + 1);

	if (hfs) {
		WriteMacInt32(pb + ioFlBkDat, 0);
		WriteMacInt32(pb + ioFlParID, fs_item->parent_id);
		WriteMacInt32(pb + ioFlClpSiz, 0);
	}
	return noErr;
}

// Set file attributes (HFileParam)
static int16 fs_set_file_info(uint32 pb, bool hfs, uint32 dirID)
{
	D(bug(" fs_set_file_info(%08lx), vRefNum %d, name %.31s, idx %d, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt16(pb + ioFDirIndex), dirID));

	// Find FSItem for given file/dir
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	// Get stats
	struct stat st;
	if (stat(full_path, &st) < 0)
		return errno2oserr();
	if (S_ISDIR(st.st_mode))
		return fnfErr;

	// Set Finder info
	set_finfo(full_path, pb + ioFlFndrInfo, hfs ? pb + ioFlXFndrInfo : 0, false);

	//!! times
	return noErr;
}

// Query file/directory attributes
static int16 fs_get_cat_info(uint32 pb)
{
	D(bug(" fs_get_cat_info(%08lx), vRefNum %d, name %.31s, idx %d, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt16(pb + ioFDirIndex), ReadMacInt32(pb + ioDirID)));

	FSItem *fs_item;
	int16 dir_index = ReadMacInt16(pb + ioFDirIndex);
	if (dir_index < 0) {			// Query directory specified by ioDirID

		// Find FSItem for directory
		fs_item = find_fsitem_by_id(ReadMacInt32(pb + ioDrDirID));
		if (fs_item == NULL)
			return dirNFErr;
		get_path_for_fsitem(fs_item);

	} else if (dir_index == 0) {	// Query item specified by ioDirID and ioNamePtr

		// Find FSItem for given file/dir
		int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
		if (result != noErr)
			return result;

	} else {							// Query item in directory specified by ioDirID by index

		// Find FSItem for parent directory
		int16 result;
		uint32 current_dir;
		if ((result = get_current_dir(pb, ReadMacInt32(pb + ioDirID), current_dir, true)) != noErr)
			return result;
		FSItem *p = find_fsitem_by_id(current_dir);
		if (p == NULL)
			return dirNFErr;
		get_path_for_fsitem(p);

		// Look for nth item in directory and add name to path
		DIR *d = opendir(full_path);
		if (d == NULL)
			return dirNFErr;
		struct dirent *de = NULL;
		for (int i=0; i<dir_index; i++) {
read_next_de:
			de = readdir(d);
			if (de == NULL) {
				closedir(d);
				return fnfErr;
			}
			if (de->d_name[0] == '.')
				goto read_next_de;	// Suppress names beginning with '.' (MacOS could interpret these as driver names)
		}
		add_path_comp(de->d_name);

		// Get FSItem for queried item
		fs_item = find_fsitem(de->d_name, p);
		closedir(d);
	}
	D(bug("  path %s\n", full_path));

	// Get stats
	struct stat st;
	if (stat(full_path, &st) < 0)
		return errno2oserr();
	if (dir_index == -1 && !S_ISDIR(st.st_mode))
		return dirNFErr;

	// Fill in struct from fs_item and stats
	if (ReadMacInt32(pb + ioNamePtr))
		cstr2pstr((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), fs_item->guest_name);
	WriteMacInt16(pb + ioFRefNum, 0);
	WriteMacInt8(pb + ioFlAttrib, (S_ISDIR(st.st_mode) ? faIsDir : 0) | (access(full_path, W_OK) == 0 ? 0 : faLocked));
	WriteMacInt8(pb + ioACUser, 0);
	WriteMacInt32(pb + ioDirID, fs_item->id);
	WriteMacInt32(pb + ioFlParID, fs_item->parent_id);
#if defined(__BEOS__) || defined(WIN32)
	WriteMacInt32(pb + ioFlCrDat, TimeToMacTime(st.st_crtime));
#elif defined __APPLE__ && defined __MACH__
	WriteMacInt32(pb + ioFlCrDat, get_creation_time(full_path));
#else
	WriteMacInt32(pb + ioFlCrDat, 0);
#endif
	time_t mtime = st.st_mtime;
	bool cached = true;
	if (mtime > fs_item->mtime) {
		fs_item->mtime = mtime;
		cached = false;
	}
	WriteMacInt32(pb + ioFlMdDat, TimeToMacTime(mtime));
	WriteMacInt32(pb + ioFlBkDat, 0);

	get_finfo(full_path, pb + ioFlFndrInfo, pb + ioFlXFndrInfo, S_ISDIR(st.st_mode));

	if (S_ISDIR(st.st_mode)) {

		// Determine number of files in directory (cached)
		int count;
		if (cached)
			count = fs_item->cache_dircount;
		else {
			count = 0;
			DIR *d = opendir(full_path);
			if (d) {
				struct dirent *de;
				for (;;) {
					de = readdir(d);
					if (de == NULL)
						break;
					if (de->d_name[0] == '.')
						continue;	// Suppress names beginning with '.'
					count++;
				}
				closedir(d);
			}
			fs_item->cache_dircount = count;
		}
		WriteMacInt16(pb + ioDrNmFls, count);
	} else {
		WriteMacInt16(pb + ioFlStBlk, 0);
		WriteMacInt32(pb + ioFlLgLen, st.st_size);
		WriteMacInt32(pb + ioFlPyLen, (st.st_size | (AL_BLK_SIZE - 1)) + 1);
		WriteMacInt16(pb + ioFlRStBlk, 0);
		uint32 rf_size = get_rfork_size(full_path);
		WriteMacInt32(pb + ioFlRLgLen, rf_size);
		WriteMacInt32(pb + ioFlRPyLen, (rf_size | (AL_BLK_SIZE - 1)) + 1);
		WriteMacInt32(pb + ioFlClpSiz, 0);
	}
	return noErr;
}

// Set file/directory attributes
static int16 fs_set_cat_info(uint32 pb)
{
	D(bug(" fs_set_cat_info(%08lx), vRefNum %d, name %.31s, idx %d, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt16(pb + ioFDirIndex), ReadMacInt32(pb + ioDirID)));

	// Find FSItem for given file/dir
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
	if (result != noErr)
		return result;

	// Get stats
	struct stat st;
	if (stat(full_path, &st) < 0)
		return errno2oserr();

	// Set Finder info
	set_finfo(full_path, pb + ioFlFndrInfo, pb + ioFlXFndrInfo, S_ISDIR(st.st_mode));

	//!! times
	return noErr;
}

// Open file
static int16 fs_open(uint32 pb, uint32 dirID, uint32 vcb, bool resource_fork)
{
	D(bug(" fs_open(%08lx), %s, vRefNum %d, name %.31s, dirID %d, perm %d\n", pb, resource_fork ? "rsrc" : "data", ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), dirID, ReadMacInt8(pb + ioPermssn)));
	M68kRegisters r;

	// Find FSItem for given file
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	// Convert ioPermssn to open() flag
	int flag = 0;
	bool write_ok = (access(full_path, W_OK) == 0);
	switch (ReadMacInt8(pb + ioPermssn)) {
		case fsCurPerm:		// Whatever is currently allowed
			if (write_ok)
				flag = O_RDWR;
			else
				flag = O_RDONLY;
			break;
		case fsRdPerm:		// Exclusive read
			flag = O_RDONLY;
			break;
		case fsWrPerm:		// Exclusive write
			flag = O_WRONLY;
			break;
		case fsRdWrPerm:	// Exclusive read/write
		case fsRdWrShPerm:	// Shared read/write
		default:
			flag = O_RDWR;
			break;
	}

	// Try to open and stat the file
	int fd = -1;
	struct stat st;
	if (resource_fork) {
		if (access(full_path, F_OK))
			return fnfErr;
		fd = open_rfork(full_path, flag);
		if (fd >= 0) {
			if (fstat(fd, &st) < 0) {
				close(fd);
				return errno2oserr();
			}
		} else {	// Resource fork not supported, silently ignore it ("pseudo" resource fork)
			st.st_size = 0;
			st.st_mode = 0;
		}
	} else {
		fd = open(full_path, flag);
		if (fd < 0)
			return errno2oserr();
		if (fstat(fd, &st) < 0) {
			close(fd);
			return errno2oserr();
		}
	}

	// File open, allocate FCB
	D(bug("  allocating FCB\n"));
	r.a[0] = pb + ioRefNum;
	r.a[1] = fs_data + fsReturn;
	Execute68k(fs_data + fsAllocateFCB, &r);
	uint32 fcb = ReadMacInt32(fs_data + fsReturn);
	D(bug("  UTAllocateFCB() returned %d, fRefNum %d, fcb %08lx\n", r.d[0], ReadMacInt16(pb + ioRefNum), fcb));
	if (r.d[0] & 0xffff) {
		close(fd);
		return (int16)r.d[0];
	}

	// Initialize FCB, fd is stored in fcbCatPos
	WriteMacInt32(fcb + fcbFlNm, fs_item->id);
	WriteMacInt8(fcb + fcbFlags, ((flag == O_WRONLY || flag == O_RDWR) ? fcbWriteMask : 0) | (resource_fork ? fcbResourceMask : 0) | (write_ok ? 0 : fcbFileLockedMask));
	WriteMacInt32(fcb + fcbEOF, st.st_size);
	WriteMacInt32(fcb + fcbPLen, (st.st_size | (AL_BLK_SIZE - 1)) + 1);
	WriteMacInt32(fcb + fcbCrPs, 0);
	WriteMacInt32(fcb + fcbVPtr, vcb);
	WriteMacInt32(fcb + fcbClmpSize, CLUMP_SIZE);

	get_finfo(full_path, fs_data + fsPB, 0, false);
	WriteMacInt32(fcb + fcbFType, ReadMacInt32(fs_data + fsPB + fdType));

	WriteMacInt32(fcb + fcbCatPos, fd);
	WriteMacInt32(fcb + fcbDirID, fs_item->parent_id);
	cstr2pstr((char *)Mac2HostAddr(fcb + fcbCName), fs_item->guest_name);
	return noErr;
}

// Close file
static int16 fs_close(uint32 pb)
{
	D(bug(" fs_close(%08lx), refNum %d\n", pb, ReadMacInt16(pb + ioRefNum)));
	M68kRegisters r;

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);

	// Close file
	if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {
		FSItem *item = find_fsitem_by_id(ReadMacInt32(fcb + fcbFlNm));
		if (item) {
			get_path_for_fsitem(item);
			close_rfork(full_path, fd);
		}
	} else
		close(fd);
	WriteMacInt32(fcb + fcbCatPos, (uint32)-1);

	// Release FCB
	D(bug("  releasing FCB\n"));
	r.d[0] = ReadMacInt16(pb + ioRefNum);
	Execute68k(fs_data + fsReleaseFCB, &r);
	D(bug("  UTReleaseFCB() returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Query information about FCB (FCBPBRec)
static int16 fs_get_fcb_info(uint32 pb, uint32 vcb)
{
	D(bug(" fs_get_fcb_info(%08lx), vRefNum %d, refNum %d, idx %d\n", pb, ReadMacInt16(pb + ioVRefNum), ReadMacInt16(pb + ioRefNum), ReadMacInt16(pb + ioFCBIndx)));
	M68kRegisters r;

	uint32 fcb = 0;
	if (ReadMacInt16(pb + ioFCBIndx) == 0) {	// Get information about single file

		// Find FCB for file
		fcb = find_fcb(ReadMacInt16(pb + ioRefNum));

	} else {					// Get information about file specified by index

		// Find FCB by index
		WriteMacInt16(pb + ioRefNum, 0);
		for (int i=0; i<(int)ReadMacInt16(pb + ioFCBIndx); i++) {
			D(bug("  indexing FCBs\n"));
			r.a[0] = vcb;
			r.a[1] = pb + ioRefNum;
			r.a[2] = fs_data + fsReturn;
			Execute68k(fs_data + fsIndexFCB, &r);
			fcb = ReadMacInt32(fs_data + fsReturn);
			D(bug("  UTIndexFCB() returned %d, fcb %p\n", r.d[0], fcb));
			if (r.d[0] & 0xffff)
				return (int16)r.d[0];
		}
	}
	if (fcb == 0)
		return rfNumErr;

	// Copy information from FCB
	if (ReadMacInt32(pb + ioNamePtr))
		pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), (char *)Mac2HostAddr(fcb + fcbCName));
	WriteMacInt32(pb + ioFCBFlNm, ReadMacInt32(fcb + fcbFlNm));
	WriteMacInt8(pb + ioFCBFlags, ReadMacInt8(fcb + fcbFlags));
	WriteMacInt16(pb + ioFCBStBlk, ReadMacInt16(fcb + fcbSBlk));
	WriteMacInt32(pb + ioFCBEOF, ReadMacInt32(fcb + fcbEOF));
	WriteMacInt32(pb + ioFCBPLen, ReadMacInt32(fcb + fcbPLen));
	WriteMacInt32(pb + ioFCBCrPs, ReadMacInt32(fcb + fcbCrPs));
	WriteMacInt16(pb + ioFCBVRefNum, ReadMacInt16(ReadMacInt32(fcb + fcbVPtr) + vcbVRefNum));
	WriteMacInt32(pb + ioFCBClpSiz, ReadMacInt32(fcb + fcbClmpSize));
	WriteMacInt32(pb + ioFCBParID, ReadMacInt32(fcb + fcbDirID));
	return noErr;
}

// Obtain logical size of an open file
static int16 fs_get_eof(uint32 pb)
{
	D(bug(" fs_get_eof(%08lx), refNum %d\n", pb, ReadMacInt16(pb + ioRefNum)));
	M68kRegisters r;

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioMisc, 0);
			return noErr;
		} else
			return fnOpnErr;

	// Get file size
	struct stat st;
	if (fstat(fd, &st) < 0)
		return errno2oserr();

	// Adjust FCBs
	WriteMacInt32(fcb + fcbEOF, st.st_size);
	WriteMacInt32(fcb + fcbPLen, (st.st_size | (AL_BLK_SIZE - 1)) + 1);
	WriteMacInt32(pb + ioMisc, st.st_size);
	D(bug("  adjusting FCBs\n"));
	r.d[0] = ReadMacInt16(pb + ioRefNum);
	Execute68k(fs_data + fsAdjustEOF, &r);
	D(bug("  UTAdjustEOF() returned %d\n", r.d[0]));
	return noErr;
}

// Truncate file
static int16 fs_set_eof(uint32 pb)
{
	D(bug(" fs_set_eof(%08lx), refNum %d, size %d\n", pb, ReadMacInt16(pb + ioRefNum), ReadMacInt32(pb + ioMisc)));
	M68kRegisters r;

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask)	// "pseudo" resource fork
			return noErr;
		else
			return fnOpnErr;

	// Truncate file
	uint32 size = ReadMacInt32(pb + ioMisc);
	if (ftruncate(fd, size) < 0)
		return errno2oserr();

	// Adjust FCBs
	WriteMacInt32(fcb + fcbEOF, size);
	WriteMacInt32(fcb + fcbPLen, (size | (AL_BLK_SIZE - 1)) + 1);
	D(bug("  adjusting FCBs\n"));
	r.d[0] = ReadMacInt16(pb + ioRefNum);
	Execute68k(fs_data + fsAdjustEOF, &r);
	D(bug("  UTAdjustEOF() returned %d\n", r.d[0]));
	return noErr;
}

// Query current file position
static int16 fs_get_fpos(uint32 pb)
{
	D(bug(" fs_get_fpos(%08lx), refNum %d\n", pb, ReadMacInt16(pb + ioRefNum)));

	WriteMacInt32(pb + ioReqCount, 0);
	WriteMacInt32(pb + ioActCount, 0);
	WriteMacInt16(pb + ioPosMode, 0);

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioPosOffset, 0);
			return noErr;
		} else
			return fnOpnErr;

	// Get file position
	uint32 pos = lseek(fd, 0, SEEK_CUR);
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	return noErr;
}

// Set current file position
static int16 fs_set_fpos(uint32 pb)
{
	D(bug(" fs_set_fpos(%08lx), refNum %d, posMode %d, offset %d\n", pb, ReadMacInt16(pb + ioRefNum), ReadMacInt16(pb + ioPosMode), ReadMacInt32(pb + ioPosOffset)));

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioPosOffset, 0);
			return noErr;
		} else
			return fnOpnErr;

	// Set file position
	switch (ReadMacInt16(pb + ioPosMode)) {
		case fsFromStart:
			if (lseek(fd, ReadMacInt32(pb + ioPosOffset), SEEK_SET) < 0)
				return posErr;
			break;
		case fsFromLEOF:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_END) < 0)
				return posErr;
			break;
		case fsFromMark:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_CUR) < 0)
				return posErr;
			break;
		default:
			break;
	}
	uint32 pos = lseek(fd, 0, SEEK_CUR);
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	return noErr;
}

// Read from file
static int16 fs_read(uint32 pb)
{
	D(bug(" fs_read(%08lx), refNum %d, buffer %p, count %d, posMode %d, posOffset %d\n", pb, ReadMacInt16(pb + ioRefNum), ReadMacInt32(pb + ioBuffer), ReadMacInt32(pb + ioReqCount), ReadMacInt16(pb + ioPosMode), ReadMacInt32(pb + ioPosOffset)));

	// Check parameters
	if ((int32)ReadMacInt32(pb + ioReqCount) < 0)
		return paramErr;

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioActCount, 0);
			return eofErr;
		} else
			return fnOpnErr;

	// Seek
	switch (ReadMacInt16(pb + ioPosMode) & 3) {
		case fsFromStart:
			if (lseek(fd, ReadMacInt32(pb + ioPosOffset), SEEK_SET) < 0)
				return posErr;
			break;
		case fsFromLEOF:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_END) < 0)
				return posErr;
			break;
		case fsFromMark:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_CUR) < 0)
				return posErr;
			break;
	}

	// Read
	ssize_t actual = extfs_read(fd, Mac2HostAddr(ReadMacInt32(pb + ioBuffer)), ReadMacInt32(pb + ioReqCount));
	int16 read_err = errno2oserr();
	D(bug("  actual %d\n", actual));
	WriteMacInt32(pb + ioActCount, actual >= 0 ? actual : 0);
	uint32 pos = lseek(fd, 0, SEEK_CUR);
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	if (actual != (ssize_t)ReadMacInt32(pb + ioReqCount))
		return actual < 0 ? read_err : eofErr;
	else
		return noErr;
}

// Write to file
static int16 fs_write(uint32 pb)
{
	D(bug(" fs_write(%08lx), refNum %d, buffer %p, count %d, posMode %d, posOffset %d\n", pb, ReadMacInt16(pb + ioRefNum), ReadMacInt32(pb + ioBuffer), ReadMacInt32(pb + ioReqCount), ReadMacInt16(pb + ioPosMode), ReadMacInt32(pb + ioPosOffset)));

	// Check parameters
	if ((int32)ReadMacInt32(pb + ioReqCount) < 0)
		return paramErr;

	// Find FCB and fd for file
	uint32 fcb = find_fcb(ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0)
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioActCount, ReadMacInt32(pb + ioReqCount));
			return noErr;
		} else
			return fnOpnErr;

	// Seek
	switch (ReadMacInt16(pb + ioPosMode) & 3) {
		case fsFromStart:
			if (lseek(fd, ReadMacInt32(pb + ioPosOffset), SEEK_SET) < 0)
				return posErr;
			break;
		case fsFromLEOF:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_END) < 0)
				return posErr;
			break;
		case fsFromMark:
			if (lseek(fd, (int32)ReadMacInt32(pb + ioPosOffset), SEEK_CUR) < 0)
				return posErr;
			break;
	}

	// Write
	ssize_t actual = extfs_write(fd, Mac2HostAddr(ReadMacInt32(pb + ioBuffer)), ReadMacInt32(pb + ioReqCount));
	int16 write_err = errno2oserr();
	D(bug("  actual %d\n", actual));
	WriteMacInt32(pb + ioActCount, actual >= 0 ? actual : 0);
	uint32 pos = lseek(fd, 0, SEEK_CUR);
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	if (actual != (ssize_t)ReadMacInt32(pb + ioReqCount))
		return write_err;
	else
		return noErr;
}

// Create file
static int16 fs_create(uint32 pb, uint32 dirID)
{
	D(bug(" fs_create(%08lx), vRefNum %d, name %.31s, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), dirID));

	// Find FSItem for given file
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	// Does the file already exist?
	if (access(full_path, F_OK) == 0)
		return dupFNErr;

	// Create file
	int fd = creat(full_path, 0666);
	if (fd < 0)
		return errno2oserr();
	else {
		close(fd);
		return noErr;
	}
}

// Create directory
static int16 fs_dir_create(uint32 pb)
{
	D(bug(" fs_dir_create(%08lx), vRefNum %d, name %.31s, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt32(pb + ioDirID)));

	// Find FSItem for given directory
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
	if (result != noErr)
		return result;

	// Does the directory already exist?
	if (access(full_path, F_OK) == 0)
		return dupFNErr;

	// Create directory
	if (mkdir(full_path, 0777) < 0)
		return errno2oserr();
	else {
		WriteMacInt32(pb + ioDirID, fs_item->id);
		return noErr;
	}
}

// Delete file/directory
static int16 fs_delete(uint32 pb, uint32 dirID)
{
	D(bug(" fs_delete(%08lx), vRefNum %d, name %.31s, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), dirID));

	// Find FSItem for given file/dir
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	// Delete file
	if (!extfs_remove(full_path))
		return errno2oserr();
	else
		return noErr;
}

// Rename file/directory
static int16 fs_rename(uint32 pb, uint32 dirID)
{
	D(bug(" fs_rename(%08lx), vRefNum %d, name %.31s, dirID %d, new name %.31s\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), dirID, Mac2HostAddr(ReadMacInt32(pb + ioMisc) + 1)));

	// Find path of given file/dir
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	// Save path of existing item
	char old_path[MAX_PATH_LENGTH];
	strcpy(old_path, full_path);

	// Find path for new name
	Mac2Mac_memcpy(fs_data + fsPB, pb, SIZEOF_IOParam);
	WriteMacInt32(fs_data + fsPB + ioNamePtr, ReadMacInt32(pb + ioMisc));
	FSItem *new_item;
	result = get_item_and_path(fs_data + fsPB, dirID, new_item);
	if (result != noErr)
		return result;

	// Does the new name already exist?
	if (access(full_path, F_OK) == 0)
		return dupFNErr;

	// Rename item
	D(bug("  renaming %s -> %s\n", old_path, full_path));
	if (!extfs_rename(old_path, full_path))
		return errno2oserr();
	else {
		// The ID of the old file/dir has to stay the same, so we swap the IDs of the FSItems
		swap_parent_ids(fs_item->id, new_item->id);
		uint32 t = fs_item->id;
		fs_item->id = new_item->id;
		new_item->id = t;
		return noErr;
	}
}

// Move file/directory (CMovePBRec)
static int16 fs_cat_move(uint32 pb)
{
	D(bug(" fs_cat_move(%08lx), vRefNum %d, name %.31s, dirID %d, new name %.31s, new dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt32(pb + ioDirID), Mac2HostAddr(ReadMacInt32(pb + ioNewName) + 1), ReadMacInt32(pb + ioNewDirID)));

	// Find path of given file/dir
	FSItem *fs_item;
	int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
	if (result != noErr)
		return result;

	// Save path of existing item
	char old_path[MAX_PATH_LENGTH];
	strcpy(old_path, full_path);

	// Find path for new directory
	Mac2Mac_memcpy(fs_data + fsPB, pb, SIZEOF_IOParam);
	WriteMacInt32(fs_data + fsPB + ioNamePtr, ReadMacInt32(pb + ioNewName));
	FSItem *new_dir_item;
	result = get_item_and_path(fs_data + fsPB, ReadMacInt32(pb + ioNewDirID), new_dir_item);
	if (result != noErr)
		return result;

	// Append old file/dir name
	add_path_comp(fs_item->name);

	// Does the new name already exist?
	if (access(full_path, F_OK) == 0)
		return dupFNErr;

	// Move item
	D(bug("  moving %s -> %s\n", old_path, full_path));
	if (!extfs_rename(old_path, full_path))
		return errno2oserr();
	else {
		// The ID of the old file/dir has to stay the same, so we swap the IDs of the FSItems
		FSItem *new_item = find_fsitem(fs_item->name, new_dir_item);
		if (new_item) {
			swap_parent_ids(fs_item->id, new_item->id);
			uint32 t = fs_item->id;
			fs_item->id = new_item->id;
			new_item->id = t;
		}
		return noErr;
	}
}

// Open working directory (WDParam)
static int16 fs_open_wd(uint32 pb)
{
	D(bug(" fs_open_wd(%08lx), vRefNum %d, name %.31s, dirID %d\n", pb, ReadMacInt16(pb + ioVRefNum), Mac2HostAddr(ReadMacInt32(pb + ioNamePtr) + 1), ReadMacInt32(pb + ioWDDirID)));
	M68kRegisters r;

	// Allocate WDCB
	D(bug("  allocating WDCB\n"));
	r.a[0] = pb;
	Execute68k(fs_data + fsAllocateWDCB, &r);
	D(bug("  UTAllocateWDCB returned %d, refNum is %d\n", r.d[0], ReadMacInt16(pb + ioVRefNum)));
	return (int16)r.d[0];
}

// Close working directory (WDParam)
static int16 fs_close_wd(uint32 pb)
{
	D(bug(" fs_close_wd(%08lx), vRefNum %d\n", pb, ReadMacInt16(pb + ioVRefNum)));
	M68kRegisters r;

	// Release WDCB
	D(bug("  releasing WDCB\n"));
	r.d[0] = ReadMacInt16(pb + ioVRefNum);
	Execute68k(fs_data + fsReleaseWDCB, &r);
	D(bug("  UTReleaseWDCB returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Query information about working directory (WDParam)
static int16 fs_get_wd_info(uint32 pb, uint32 vcb)
{
	D(bug(" fs_get_wd_info(%08lx), vRefNum %d, idx %d, procID %d\n", pb, ReadMacInt16(pb + ioVRefNum), ReadMacInt16(pb + ioWDIndex), ReadMacInt32(pb + ioWDProcID)));
	M68kRegisters r;

	// Querying volume?
	if (ReadMacInt16(pb + ioWDIndex) == 0 && ReadMacInt16(pb + ioVRefNum) == ReadMacInt16(vcb + vcbVRefNum)) {
		WriteMacInt32(pb + ioWDProcID, 0);
		WriteMacInt16(pb + ioWDVRefNum, ReadMacInt16(vcb + vcbVRefNum));
		if (ReadMacInt32(pb + ioNamePtr))
			Mac2Mac_memcpy(ReadMacInt32(pb + ioNamePtr), vcb + vcbVN, 28);
		WriteMacInt32(pb + ioWDDirID, ROOT_ID);
		return noErr;
	}

	// Resolve WDCB
	D(bug("  resolving WDCB\n"));
	r.d[0] = ReadMacInt32(pb + ioWDProcID);
	r.d[1] = ReadMacInt16(pb + ioWDIndex);
	r.d[2] = ReadMacInt16(pb + ioVRefNum);
	r.a[0] = fs_data + fsReturn;
	Execute68k(fs_data + fsResolveWDCB, &r);
	uint32 wdcb = ReadMacInt32(fs_data + fsReturn);
	D(bug("  UTResolveWDCB() returned %d, dirID %d\n", r.d[0], ReadMacInt32(wdcb + wdDirID)));
	if (r.d[0] & 0xffff)
		return (int16)r.d[0];

	// Return information
	WriteMacInt32(pb + ioWDProcID, ReadMacInt32(wdcb + wdProcID));
	WriteMacInt16(pb + ioWDVRefNum, ReadMacInt16(ReadMacInt32(wdcb + wdVCBPtr) + vcbVRefNum));
	if (ReadMacInt32(pb + ioNamePtr))
		Mac2Mac_memcpy(ReadMacInt32(pb + ioNamePtr), ReadMacInt32(wdcb + wdVCBPtr) + vcbVN, 28);
	WriteMacInt32(pb + ioWDDirID, ReadMacInt32(wdcb + wdDirID));
	return noErr;
}

// Main dispatch routine
int16 ExtFSHFS(uint32 vcb, uint16 selectCode, uint32 paramBlock, uint32 globalsPtr, int16 fsid)
{
	uint16 trapWord = selectCode & 0xf0ff;
	bool hfs = selectCode & kHFSMask;
	switch (trapWord) {
		case kFSMOpen:
			return fs_open(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0, vcb, false);

		case kFSMClose:
			return fs_close(paramBlock);

		case kFSMRead:
			return fs_read(paramBlock);

		case kFSMWrite:
			return fs_write(paramBlock);

		case kFSMGetVolInfo:
			return fs_get_vol_info(paramBlock, hfs);

		case kFSMCreate:
			return fs_create(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0);

		case kFSMDelete:
			return fs_delete(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0);

		case kFSMOpenRF:
			return fs_open(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0, vcb, true);

		case kFSMRename:
			return fs_rename(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0);

		case kFSMGetFileInfo:
			return fs_get_file_info(paramBlock, hfs, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0);

		case kFSMSetFileInfo:
			return fs_set_file_info(paramBlock, hfs, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0);

		case kFSMUnmountVol:
			return fs_unmount_vol(vcb);

		case kFSMMountVol:
			return fs_mount_vol(paramBlock);

		case kFSMAllocate:
			D(bug(" allocate\n"));
			WriteMacInt32(paramBlock + ioActCount, ReadMacInt32(paramBlock + ioReqCount));
			return noErr;

		case kFSMGetEOF:
			return fs_get_eof(paramBlock);

		case kFSMSetEOF:
			return fs_set_eof(paramBlock);

		case kFSMGetVol:
			return fs_get_vol(paramBlock);

		case kFSMSetVol:
			return fs_set_vol(paramBlock, hfs, vcb);

		case kFSMEject:
			D(bug(" eject\n"));
			return noErr;

		case kFSMGetFPos:
			return fs_get_fpos(paramBlock);

		case kFSMOffline:
			D(bug(" offline\n"));
			return noErr;

		case kFSMSetFilLock:
			return noErr;	//!!

		case kFSMRstFilLock:
			return noErr;	//!!

		case kFSMSetFPos:
			return fs_set_fpos(paramBlock);

		case kFSMOpenWD:
			return fs_open_wd(paramBlock);

		case kFSMCloseWD:
			return fs_close_wd(paramBlock);

		case kFSMCatMove:
			return fs_cat_move(paramBlock);

		case kFSMDirCreate:
			return fs_dir_create(paramBlock);

		case kFSMGetWDInfo:
			return fs_get_wd_info(paramBlock, vcb);

		case kFSMGetFCBInfo:
			return fs_get_fcb_info(paramBlock, vcb);

		case kFSMGetCatInfo:
			return fs_get_cat_info(paramBlock);

		case kFSMSetCatInfo:
			return fs_set_cat_info(paramBlock);

		case kFSMSetVolInfo:
			return fs_set_vol_info(paramBlock);

		case kFSMGetVolParms:
			return fs_get_vol_parms(paramBlock);

		case kFSMVolumeMount:
			return fs_volume_mount(paramBlock);

		case kFSMFlushVol:
		case kFSMFlushFile:
			D(bug(" flush_vol/flush_file\n"));
			return noErr;

		default:
			D(bug("ExtFSHFS(%08lx, %04x, %08lx, %08lx, %d)\n", vcb, selectCode, paramBlock, globalsPtr, fsid));
			return paramErr;
	}
}
