/*
 *  extfs.cpp - MacOS file system for native file system access
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

/*
 *  SEE ALSO
 *    Guide to the File System Manager (from FSM 1.2 SDK)
 *
 *  TODO
 *    LockRng
 *    UnlockRng
 *    (CatSearch)
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
	fsReturn = 306,					// Area for return data of 68k routines (256 bytes, must stay >= SIZEOF_WDParam)
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
static uint32 root_crdate = 0;
static uint32 mounted_vcb = 0;

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
	FSItem *next;			// Pointer to next FSItem in global allocation list
	FSItem *hash_next;		// Pointer to next active FSItem in CNID hash bucket
	FSItem *first_child;	// First active child in directory
	FSItem *last_child;		// Last active child in directory
	FSItem *next_sibling;	// Next active sibling in parent directory
	uint32 id;				// CNID of this file/dir
	uint32 parent_id;		// CNID of parent dir
	FSItem *parent;			// Pointer to parent
	char *name;				// Object name (C string) - Host OS
	char guest_name[32];	// Object name (C string) - Guest OS
	time_t mtime;			// Modification time for get_cat_info caching
	int cache_dircount;		// Cached number of items in directory (-1 = invalid)
	bool retired;			// True if deleted/invalidated (CNID is never reused)
};

const uint32 FSITEM_HASH_SIZE = 1024;
static FSItem *fsitem_hash[FSITEM_HASH_SIZE];
static FSItem *first_fs_item = NULL, *last_fs_item = NULL;
static FSItem *root_parent_fs_item = NULL, *root_fs_item = NULL;

static uint32 next_cnid = fsUsrCNID;	// Next available CNID


/*
 *  Get object creation time (guaranteed non-zero MacOS timestamp)
 */

#if defined __APPLE__ && defined __MACH__
struct crtimebuf {
	u_int32_t length;
	struct timespec crtime;
} __attribute__((aligned(4), packed));

static uint32 do_get_creation_time(const char *path)
{
	struct attrlist attr;
	memset(&attr, 0, sizeof(attr));
	attr.bitmapcount = ATTR_BIT_MAP_COUNT;
	attr.commonattr = ATTR_CMN_CRTIME;

	crtimebuf buf;
	if (getattrlist(path, &attr, &buf, sizeof(buf), 0) < 0)
		return 0;
	return TimeToMacTime(buf.crtime.tv_sec);
}
#endif

static uint32 get_item_crdate(const char *path, const struct stat &st)
{
	uint32 cr = 0;
#if defined(__BEOS__) || defined(WIN32)
	(void)path;
	cr = TimeToMacTime(st.st_crtime);
#elif defined __APPLE__ && defined __MACH__
	if (path != NULL)
		cr = do_get_creation_time(path);
#else
	(void)path;
#endif
	if (cr == 0)
		cr = TimeToMacTime(st.st_mtime);
	if (cr == 0)
		cr = TimeToMacTime(0);
	return cr;
}


/*
 *  MacRoman case-insensitive comparison helpers (matching HFS RelString)
 */

static uint8 macroman_upper(uint8 c)
{
	if (c >= 'a' && c <= 'z')
		return (uint8)(c - ('a' - 'A'));
	switch (c) {
		case 0x87: return 0xE7; // a acute
		case 0x88: return 0xCB; // a grave
		case 0x89: return 0xE5; // a circumflex
		case 0x8A: return 0x80; // a diaeresis
		case 0x8B: return 0xCC; // a tilde
		case 0x8C: return 0x81; // a ring
		case 0x8D: return 0x82; // c cedilla
		case 0x8E: return 0x83; // e acute
		case 0x8F: return 0xE9; // e grave
		case 0x90: return 0xE6; // e circumflex
		case 0x91: return 0xE8; // e diaeresis
		case 0x92: return 0xEA; // i acute
		case 0x93: return 0xED; // i grave
		case 0x94: return 0xEB; // i circumflex
		case 0x95: return 0xEC; // i diaeresis
		case 0x96: return 0x84; // n tilde
		case 0x97: return 0xEE; // o acute
		case 0x98: return 0xF1; // o grave
		case 0x99: return 0xEF; // o circumflex
		case 0x9A: return 0x85; // o diaeresis
		case 0x9B: return 0xCD; // o tilde
		case 0x9C: return 0xF2; // u acute
		case 0x9D: return 0xF4; // u grave
		case 0x9E: return 0xF3; // u circumflex
		case 0x9F: return 0x86; // u diaeresis
		case 0xBE: return 0xAE; // ae
		case 0xBF: return 0xAF; // o slash
		case 0xCF: return 0xCE; // oe
		case 0xD8: return 0xD9; // y diaeresis
		case 0xF5: return 0x49; // dotless i
		default:   return c;
	}
}

static bool macroman_iequals(const char *a, const char *b)
{
	while (*a && *b) {
		if (macroman_upper((uint8)*a) != macroman_upper((uint8)*b))
			return false;
		a++;
		b++;
	}
	return *a == *b;
}


/*
 *  FSItem indexing, hierarchy, and path helpers
 */

static void hash_insert_fsitem(FSItem *p)
{
	uint32 bucket = p->id & (FSITEM_HASH_SIZE - 1);
	p->hash_next = fsitem_hash[bucket];
	fsitem_hash[bucket] = p;
}

static void hash_remove_fsitem(FSItem *p)
{
	uint32 bucket = p->id & (FSITEM_HASH_SIZE - 1);
	FSItem **pp = &fsitem_hash[bucket];
	while (*pp) {
		if (*pp == p) {
			*pp = p->hash_next;
			p->hash_next = NULL;
			return;
		}
		pp = &((*pp)->hash_next);
	}
}

static void link_child_fsitem(FSItem *parent, FSItem *child)
{
	child->next_sibling = NULL;
	if (parent->last_child)
		parent->last_child->next_sibling = child;
	else
		parent->first_child = child;
	parent->last_child = child;
}

static void unlink_child_fsitem(FSItem *parent, FSItem *child)
{
	FSItem *prev = NULL;
	FSItem *cur = parent->first_child;
	while (cur) {
		if (cur == child) {
			if (prev)
				prev->next_sibling = cur->next_sibling;
			else
				parent->first_child = cur->next_sibling;
			if (parent->last_child == cur)
				parent->last_child = prev;
			cur->next_sibling = NULL;
			return;
		}
		prev = cur;
		cur = cur->next_sibling;
	}
}

// Retire a deleted/stale FSItem and any cached subtree without reusing its CNID
static void retire_fsitem(FSItem *p)
{
	if (p == NULL || p->retired || p->id <= ROOT_ID)
		return;
	while (p->first_child)
		retire_fsitem(p->first_child);
	if (p->parent) {
		unlink_child_fsitem(p->parent, p);
		p->parent->cache_dircount = -1;
	}
	hash_remove_fsitem(p);
	delete[] p->name;
	p->name = NULL;
	p->retired = true;
}

static FSItem *find_fsitem_by_id(uint32 cnid)
{
	FSItem *p = fsitem_hash[cnid & (FSITEM_HASH_SIZE - 1)];
	while (p) {
		if (p->id == cnid && !p->retired)
			return p;
		p = p->hash_next;
	}
	return NULL;
}

static bool has_sibling_guest_collision(FSItem *parent, const char *candidate, const char *host_name)
{
	for (FSItem *c = parent->first_child; c != NULL; c = c->next_sibling) {
		if (strcmp(c->name, host_name) != 0 && macroman_iequals(c->guest_name, candidate))
			return true;
	}
	return false;
}

static void make_unique_guest_name(const char *raw_guest, FSItem *parent, const char *host_name, char out_guest[32])
{
	const char *src = (raw_guest && raw_guest[0]) ? raw_guest : "?";
	strncpy(out_guest, src, 31);
	out_guest[31] = 0;

	if (!has_sibling_guest_collision(parent, out_guest, host_name))
		return;

	const char *dot = strrchr(src, '.');
	size_t ext_len = 0;
	if (dot != NULL && dot > src) {
		size_t dl = strlen(dot);
		if (dl >= 2 && dl <= 6)
			ext_len = dl;
	}
	size_t stem_src_len = ext_len ? (size_t)(dot - src) : strlen(src);

	for (unsigned int idx = 2; idx < 10000; idx++) {
		char suffix[16];
		snprintf(suffix, sizeof(suffix), "#%u", idx);
		size_t suffix_len = strlen(suffix);
		size_t max_stem = (31 > suffix_len + ext_len) ? (31 - suffix_len - ext_len) : 0;
		size_t copy_stem = (stem_src_len < max_stem) ? stem_src_len : max_stem;
		memcpy(out_guest, src, copy_stem);
		memcpy(out_guest + copy_stem, suffix, suffix_len);
		if (ext_len)
			memcpy(out_guest + copy_stem + suffix_len, dot, ext_len);
		out_guest[copy_stem + suffix_len + ext_len] = 0;
		if (!has_sibling_guest_collision(parent, out_guest, host_name))
			return;
	}
}

static FSItem *create_fsitem(const char *name, const char *guest_name, FSItem *parent)
{
	FSItem *p = new FSItem;
	if (last_fs_item)
		last_fs_item->next = p;
	else
		first_fs_item = p;
	p->next = NULL;
	last_fs_item = p;
	p->hash_next = NULL;
	p->first_child = NULL;
	p->last_child = NULL;
	p->next_sibling = NULL;
	p->id = next_cnid++;
	p->parent_id = parent->id;
	p->parent = parent;
	size_t name_len = strlen(name);
	p->name = new char[name_len + 1];
	memcpy(p->name, name, name_len + 1);
	strncpy(p->guest_name, guest_name, 31);
	p->guest_name[31] = 0;
	p->mtime = 0;
	p->cache_dircount = -1;
	p->retired = false;
	hash_insert_fsitem(p);
	link_child_fsitem(parent, p);
	return p;
}

// Find active FSItem for given host name and parent, constructing a new FSItem if not found
static FSItem *find_fsitem(const char *name, FSItem *parent)
{
	for (FSItem *p = parent->first_child; p != NULL; p = p->next_sibling) {
		if (!strcmp(p->name, name))
			return p;
	}

	const char *mr = host_encoding_to_macroman(name);
	char raw_guest[MAX_PATH_LENGTH];
	strncpy(raw_guest, mr ? mr : name, sizeof(raw_guest) - 1);
	raw_guest[sizeof(raw_guest) - 1] = 0;

	char unique_guest[32];
	make_unique_guest_name(raw_guest, parent, name, unique_guest);
	return create_fsitem(name, unique_guest, parent);
}

/*
 *  Get full path for given FSItem
 */

static char full_path[MAX_PATH_LENGTH];

static void add_path_comp(const char *s)
{
	add_path_component(full_path, s);
}

static void get_path_for_fsitem_buf(FSItem *p, char *buf)
{
	if (p == NULL || p->id == ROOT_PARENT_ID) {
		buf[0] = 0;
	} else if (p->id == ROOT_ID) {
		strncpy(buf, RootPath, MAX_PATH_LENGTH - 1);
		buf[MAX_PATH_LENGTH - 1] = 0;
	} else if (p->name != NULL) {
		get_path_for_fsitem_buf(p->parent, buf);
		add_path_component(buf, p->name);
	} else {
		buf[0] = 0;
	}
}

static void get_path_for_fsitem(FSItem *p)
{
	get_path_for_fsitem_buf(p, full_path);
}

// Check if a directory entry should be visible to Mac OS (suppressing dotfiles, broken symlinks, and special nodes)
static bool is_visible_dirent(const char *dir_path, const struct dirent *de, struct stat *out_st)
{
	if (de->d_name[0] == '.')
		return false;

#if defined(DT_REG) && defined(DT_DIR) && defined(DT_LNK) && defined(DT_UNKNOWN) && !defined(WIN32)
	if (de->d_type != DT_REG && de->d_type != DT_DIR && de->d_type != DT_LNK && de->d_type != DT_UNKNOWN)
		return false;
#endif

	char entry_path[MAX_PATH_LENGTH];
	strncpy(entry_path, dir_path, MAX_PATH_LENGTH - 1);
	entry_path[MAX_PATH_LENGTH - 1] = 0;
	add_path_component(entry_path, de->d_name);

	struct stat local_st;
	struct stat *stp = out_st ? out_st : &local_st;
	if (stat(entry_path, stp) < 0)
		return false;
	if (!S_ISREG(stp->st_mode) && !S_ISDIR(stp->st_mode))
		return false;
	return true;
}

// Resolve an existing child in parent by guest MacRoman name (case-insensitively), without creating phantom FSItems
static FSItem *resolve_child_by_guest_name(FSItem *parent, const char *guest_name)
{
	if (parent == NULL || guest_name == NULL || guest_name[0] == 0)
		return NULL;
	if (parent->id == ROOT_PARENT_ID) {
		if (root_fs_item && macroman_iequals(guest_name, root_fs_item->guest_name))
			return root_fs_item;
		return NULL;
	}

	// Fast path: check active children already cached under parent
	for (FSItem *p = parent->first_child; p != NULL; p = p->next_sibling) {
		if (macroman_iequals(p->guest_name, guest_name)) {
			char check_path[MAX_PATH_LENGTH];
			get_path_for_fsitem_buf(p, check_path);
			struct stat st;
			if (stat(check_path, &st) == 0 && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)))
				return p;
			retire_fsitem(p);
			break;
		}
	}

	// Slow path: scan host directory so case differences, >31-char truncations, and lossy encodings resolve canonically
	char dir_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(parent, dir_path);
	DIR *d = opendir(dir_path);
	if (d == NULL)
		return NULL;

	// Populate all visible children on slow-path lookup so #N disambiguation is invariant to lookup order
	FSItem *match = NULL;
	struct dirent *de;
	while ((de = readdir(d)) != NULL) {
		if (!is_visible_dirent(dir_path, de, NULL))
			continue;
		FSItem *child = find_fsitem(de->d_name, parent);
		if (match == NULL && macroman_iequals(child->guest_name, guest_name))
			match = child;
	}
	closedir(d);
	return match;
}

static void calc_fork_sizes(off_t raw_size, uint32 &logical_out, uint32 &physical_out)
{
	if (raw_size <= 0) {
		logical_out = 0;
		physical_out = 0;
		return;
	}
	if (raw_size > (off_t)0x7fffffff)
		logical_out = 0x7fffffff;
	else
		logical_out = (uint32)raw_size;
	if (logical_out > 0x7fffc000U)
		physical_out = 0x7fffc000U;
	else
		physical_out = (logical_out + (AL_BLK_SIZE - 1)) & ~(uint32)(AL_BLK_SIZE - 1);
}


/*
 *  String handling functions
 */

// Copy pascal string safely (unsigned length byte)
static void pstrcpy(char *dst, const char *src)
{
	uint8 size = (uint8)*src++;
	*dst++ = (char)size;
	while (size--)
		*dst++ = *src++;
}

// Convert C string to pascal string (clamped to max_len, default Str31)
static void cstr2pstr(char *dst, const char *src, size_t max_len = 31)
{
	size_t len = strlen(src);
	if (len > max_len)
		len = max_len;
	*dst++ = (char)len;
	for (size_t i = 0; i < len; i++) {
		char c = src[i];
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
	while (size-- > 0) {
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
		case ENOTDIR:
			return dirNFErr;
		case ENOENT:
		case EISDIR:
			return fnfErr;
		case ENAMETOOLONG:
			return bdNamErr;
		case EINVAL:
			return paramErr;
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
		case ENFILE:
			return tmfoErr;
		case ENOMEM:
			return memFullErr;
		case EIO:
		default:
			return ioErr;
	}
}


/*
 *  Initialization
 */

static void free_all_fsitems(void)
{
	FSItem *p = first_fs_item, *next;
	while (p) {
		next = p->next;
		delete[] p->name;
		delete p;
		p = next;
	}
	first_fs_item = last_fs_item = NULL;
	root_parent_fs_item = root_fs_item = NULL;
	memset(fsitem_hash, 0, sizeof(fsitem_hash));
}

void ExtFSInit(void)
{
	free_all_fsitems();
	ready = false;
	mounted_vcb = 0;
	root_crdate = 0;
	next_cnid = fsUsrCNID;

	// System specific initialization
	extfs_init();

	// Get file system name
	const char *fs_name = GetString(STR_EXTFS_NAME);
	const char *guest_fs_name = host_encoding_to_macroman(fs_name);
	cstr2pstr(FS_NAME, guest_fs_name ? guest_fs_name : fs_name, 31);

	// Create root's parent FSItem (fsRtParID == 1)
	FSItem *p = new FSItem;
	first_fs_item = last_fs_item = root_parent_fs_item = p;
	p->next = NULL;
	p->hash_next = NULL;
	p->first_child = NULL;
	p->last_child = NULL;
	p->next_sibling = NULL;
	p->id = ROOT_PARENT_ID;
	p->parent_id = 0;
	p->parent = NULL;
	p->name = new char[1];
	p->name[0] = 0;
	memset(p->guest_name, 0, sizeof(p->guest_name));
	p->mtime = 0;
	p->cache_dircount = -1;
	p->retired = false;
	hash_insert_fsitem(p);

	// Create root FSItem (fsRtDirID == 2)
	p = new FSItem;
	last_fs_item->next = p;
	last_fs_item = root_fs_item = p;
	p->next = NULL;
	p->hash_next = NULL;
	p->first_child = NULL;
	p->last_child = NULL;
	p->next_sibling = NULL;
	p->id = ROOT_ID;
	p->parent_id = ROOT_PARENT_ID;
	p->parent = root_parent_fs_item;
	const char *volume_name = GetString(STR_EXTFS_VOLUME_NAME);
	size_t vol_len = strlen(volume_name);
	p->name = new char[vol_len + 1];
	memcpy(p->name, volume_name, vol_len + 1);
	memset(p->guest_name, 0, sizeof(p->guest_name));
	const char *guest_vol = host_encoding_to_macroman(p->name);
	strncpy(p->guest_name, guest_vol ? guest_vol : p->name, 27);
	p->guest_name[27] = 0;
	cstr2pstr(VOLUME_NAME, p->guest_name, 27);
	p->mtime = 0;
	p->cache_dircount = -1;
	p->retired = false;
	hash_insert_fsitem(p);
	link_child_fsitem(root_parent_fs_item, p);

	// Find path for root
	if ((RootPath = PrefsFindString("extfs")) != NULL) {
		if (stat(RootPath, &root_stat))
			return;
		if (!S_ISDIR(root_stat.st_mode))
			return;
		root_crdate = get_item_crdate(RootPath, root_stat);
		ready = true;
	}
}


/*
 *  Deinitialization
 */

void ExtFSExit(void)
{
	free_all_fsitems();
	ready = false;
	mounted_vcb = 0;

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

static int16 get_current_dir(uint32 pb, uint32 dirID, uint32 &current_dir, bool no_vol_name = false, uint32 *out_vcb = NULL)
{
	M68kRegisters r;
	int16 result;

	// Determine volume
	D(bug("  determining volume, dirID %u\n", dirID));
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
	int16 status = (int16)ReadMacInt16(fs_data + fsReturn);
	uint32 det_vcb = ReadMacInt32(fs_data + fsReturn + 6);
	D(bug("  UTDetermineVol() returned %d, status %d\n", r.d[0], status));
	result = (int16)(r.d[0] & 0xffff);

	if (result == noErr) {
		if (det_vcb == 0 || (int16)ReadMacInt16(det_vcb + vcbFSID) != MY_FSID)
			return nsvErr;
		if (out_vcb != NULL)
			*out_vcb = det_vcb;

		switch (status) {
			case dtmvFullPathname:	// Determined by full pathname
				current_dir = ROOT_ID;
				break;

			case dtmvVRefNum:		// Determined by refNum or by drive number
			case dtmvDriveNum:
				current_dir = dirID ? dirID : ROOT_ID;
				break;

			case dtmvWDRefNum:		// Determined by working directory refNum
				if (dirID) {
					current_dir = dirID;
				} else {
					D(bug("  resolving WDCB\n"));
					r.d[0] = 0;
					r.d[1] = 0;
					r.d[2] = ReadMacInt16(pb + ioVRefNum);
					r.a[0] = fs_data + fsReturn;
					Execute68k(fs_data + fsResolveWDCB, &r);
					uint32 wdcb = ReadMacInt32(fs_data + fsReturn);
					result = (int16)(r.d[0] & 0xffff);
					if (result == noErr) {
						if (wdcb == 0 || ReadMacInt32(wdcb + wdVCBPtr) != det_vcb)
							return rfNumErr;
						uint32 wd_dir = ReadMacInt32(wdcb + wdDirID);
						current_dir = wd_dir ? wd_dir : ROOT_ID;
					}
				}
				break;

			case dtmvDefault:		// Determined by default volume
				if (dirID) {
					current_dir = dirID;
				} else {
					uint32 wdpb = fs_data + fsReturn;
					Mac_memset(wdpb, 0, SIZEOF_WDParam);
					D(bug("  getting default volume\n"));
					r.a[0] = wdpb;
					Execute68k(fs_data + fsGetDefaultVol, &r);
					result = (int16)(r.d[0] & 0xffff);
					if (result == noErr) {
						uint32 wd_dir = ReadMacInt32(wdpb + ioWDDirID);
						current_dir = wd_dir ? wd_dir : ROOT_ID;
					}
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
	M68kRegisters r;
	r.a[0] = rec;
	Execute68k(fs_data + fsGetPathComponentName, &r);
	return (int16)(r.d[0] & 0xffff);
}


/*
 *  Resolve HFS/FSM path specification in ParamBlock without creating phantom FSItems
 */

struct ResolvedPath {
	uint32 vcb;
	FSItem *item;			// Existing target item (when result == noErr)
	FSItem *parent;			// Existing parent directory (when noErr or missing leaf)
	char leaf_guest[32];	// Canonical or parsed guest leaf name (C string, ':' for Mac '/')
	char parsed_leaf[32];	// Exact guest leaf component parsed from input (preserves case)
	char leaf_host[MAX_PATH_LENGTH];	// Host-encoded leaf name for creating/renaming
	bool trailing_delim;	// True if input path ended with ':'
};

static bool convert_guest_leaf_to_host(const char *guest_leaf, char *out_host, size_t out_size)
{
	if (guest_leaf == NULL || guest_leaf[0] == 0)
		return false;
	const char *he = macroman_to_host_encoding(guest_leaf);
	const char *src = (he && he[0]) ? he : guest_leaf;
	size_t len = strlen(src);
	if (len == 0 || len >= out_size)
		return false;
	if (src[0] == '.' || strchr(src, '/') != NULL)
		return false;
#ifdef WIN32
	if (strpbrk(src, "\\:*?\"<>|") != NULL)
		return false;
#endif
	memcpy(out_host, src, len + 1);
	return true;
}

static bool verify_existing_dir_fsitem(FSItem *p)
{
	if (p == NULL)
		return false;
	if (p->id == ROOT_PARENT_ID)
		return true;
	char dir_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(p, dir_path);
	struct stat st;
	if (stat(dir_path, &st) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(p);
		return false;
	}
	return S_ISDIR(st.st_mode);
}

static int16 resolve_path_spec(uint32 pb, uint32 dirID, bool no_vol_name, ResolvedPath &out)
{
	memset(&out, 0, sizeof(out));

	uint32 current_dir = 0;
	int16 result = get_current_dir(pb, dirID, current_dir, no_vol_name, &out.vcb);
	if (result != noErr)
		return result;

	FSItem *p = find_fsitem_by_id(current_dir);
	if (!verify_existing_dir_fsitem(p))
		return dirNFErr;

	uint32 name_ptr = no_vol_name ? 0 : ReadMacInt32(pb + ioNamePtr);
	if (name_ptr == 0 || ReadMacInt8(name_ptr) == 0) {
		if (p->id == ROOT_PARENT_ID)
			return dirNFErr;
		out.item = p;
		out.parent = p->parent;
		strncpy(out.leaf_guest, p->guest_name, 31);
		out.leaf_guest[31] = 0;
		strncpy(out.parsed_leaf, p->guest_name, 31);
		out.parsed_leaf[31] = 0;
		strncpy(out.leaf_host, p->name, sizeof(out.leaf_host) - 1);
		out.leaf_host[sizeof(out.leaf_host) - 1] = 0;
		out.trailing_delim = true;
		return noErr;
	}

	M68kRegisters r;
	uint32 parseRec = fs_data + fsParseRec;
	WriteMacInt32(parseRec + ppNamePtr, name_ptr);
	WriteMacInt16(parseRec + ppStartOffset, 0);
	WriteMacInt16(parseRec + ppComponentLength, 0);
	WriteMacInt8(parseRec + ppMoreName, false);
	WriteMacInt8(parseRec + ppFoundDelimiter, false);

	// Skip volume name if full pathname
	r.a[0] = parseRec + ppStartOffset;
	r.a[1] = name_ptr;
	Execute68k(fs_data + fsParsePathname, &r);
	result = (int16)(r.d[0] & 0xffff);
	if (result != noErr)
		return bdNamErr;

	// Check for leading delimiter
	result = get_path_component_name(parseRec);
	if (result != noErr)
		return result;
	if (ReadMacInt16(parseRec + ppComponentLength) == 0 && ReadMacInt8(parseRec + ppFoundDelimiter)) {
		WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + 1);
	}

	// Traverse intermediate directory components
	while (result == noErr && ReadMacInt8(parseRec + ppMoreName)) {
		result = get_path_component_name(parseRec);
		if (result != noErr)
			return result;

		uint16 comp_len = ReadMacInt16(parseRec + ppComponentLength);
		if (comp_len == 0) {
			if (!ReadMacInt8(parseRec + ppFoundDelimiter))
				break;
			// '::' climbs to parent directory
			if (p->parent == NULL)
				return dirNFErr;
			p = p->parent;
			if (!verify_existing_dir_fsitem(p))
				return dirNFErr;
			WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + 1);
		} else if (ReadMacInt8(parseRec + ppMoreName)) {
			if (comp_len > 31)
				return bdNamErr;
			char comp_name[32];
			strn2cstr(comp_name, (char *)Mac2HostAddr(name_ptr) + ReadMacInt16(parseRec + ppStartOffset) + 1, comp_len);
			FSItem *next_dir = resolve_child_by_guest_name(p, comp_name);
			if (next_dir == NULL || !verify_existing_dir_fsitem(next_dir))
				return dirNFErr;
			p = next_dir;
			WriteMacInt16(parseRec + ppStartOffset, ReadMacInt16(parseRec + ppStartOffset) + comp_len + 1);
		}
	}

	if (result != noErr)
		return result;

	uint16 leaf_len = ReadMacInt16(parseRec + ppComponentLength);
	bool trailing_delim = (ReadMacInt8(parseRec + ppFoundDelimiter) != 0);

	if (leaf_len == 0) {
		if (p->id == ROOT_PARENT_ID)
			return dirNFErr;
		if (!verify_existing_dir_fsitem(p))
			return dirNFErr;
		out.item = p;
		out.parent = p->parent;
		strncpy(out.leaf_guest, p->guest_name, 31);
		out.leaf_guest[31] = 0;
		strncpy(out.parsed_leaf, p->guest_name, 31);
		out.parsed_leaf[31] = 0;
		strncpy(out.leaf_host, p->name, sizeof(out.leaf_host) - 1);
		out.leaf_host[sizeof(out.leaf_host) - 1] = 0;
		out.trailing_delim = true;
		return noErr;
	}

	if (leaf_len > 31)
		return bdNamErr;

	char leaf_name[32];
	strn2cstr(leaf_name, (char *)Mac2HostAddr(name_ptr) + ReadMacInt16(parseRec + ppStartOffset) + 1, leaf_len);
	strncpy(out.parsed_leaf, leaf_name, 31);
	out.parsed_leaf[31] = 0;
	out.trailing_delim = trailing_delim;

	FSItem *target = resolve_child_by_guest_name(p, leaf_name);
	if (target != NULL) {
		char target_path[MAX_PATH_LENGTH];
		get_path_for_fsitem_buf(target, target_path);
		struct stat st;
		if (stat(target_path, &st) < 0) {
			if (errno == ENOENT || errno == ENOTDIR)
				retire_fsitem(target);
			return trailing_delim ? dirNFErr : fnfErr;
		}
		if (trailing_delim && !S_ISDIR(st.st_mode))
			return dirNFErr;
		out.item = target;
		out.parent = target->parent;
		strncpy(out.leaf_guest, target->guest_name, 31);
		out.leaf_guest[31] = 0;
		strncpy(out.leaf_host, target->name, sizeof(out.leaf_host) - 1);
		out.leaf_host[sizeof(out.leaf_host) - 1] = 0;
		return noErr;
	}

	// Target leaf does not exist
	if (p->id == ROOT_PARENT_ID)
		return trailing_delim ? dirNFErr : fnfErr;
	if (!convert_guest_leaf_to_host(leaf_name, out.leaf_host, sizeof(out.leaf_host)))
		return bdNamErr;
	out.parent = p;
	strncpy(out.leaf_guest, leaf_name, 31);
	out.leaf_guest[31] = 0;
	return trailing_delim ? dirNFErr : fnfErr;
}

/*
 *  Get existing FSItem and full path (->full_path) for file/dir specified in ParamBlock
 */

static int16 get_item_and_path(uint32 pb, uint32 dirID, FSItem *&item, bool no_vol_name = false)
{
	item = NULL;
	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, dirID, no_vol_name, resolved);
	if (result == noErr && resolved.item != NULL) {
		item = resolved.item;
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
	r.d[0] = (uint16)refNum;
	r.a[0] = fs_data + fsReturn;
	Execute68k(fs_data + fsResolveFCB, &r);
	uint32 fcb = ReadMacInt32(fs_data + fsReturn);
	D(bug("  UTResolveFCB() returned %d, fcb %08x\n", r.d[0], fcb));
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
	D(bug(" fs_mount_vol(%08x), vRefNum %d\n", pb, (int16)ReadMacInt16(pb + ioVRefNum)));
	if ((int16)ReadMacInt16(pb + ioVRefNum) == drive_number)
		return noErr;
	else
		return extFSErr;
}

// Mount volume
static int16 fs_volume_mount(uint32 pb)
{
	D(bug(" fs_volume_mount(%08x)\n", pb));
	M68kRegisters r;

	struct stat st;
	if (stat(RootPath, &st) == 0 && S_ISDIR(st.st_mode))
		root_stat = st;

	// Create new VCB
	D(bug("  creating VCB\n"));
	r.a[0] = fs_data + fsReturn;
	r.a[1] = fs_data + fsReturn + 2;
	Execute68k(fs_data + fsAllocateVCB, &r);
#if DEBUG
	uint16 sysVCBLength = ReadMacInt16(fs_data + fsReturn);
#endif
	uint32 vcb = ReadMacInt32(fs_data + fsReturn + 2);
	D(bug("  UTAllocateVCB() returned %d, vcb %08x, size %d\n", r.d[0], vcb, sysVCBLength));
	if (r.d[0] & 0xffff)
		return (int16)r.d[0];

	// Init VCB
	WriteMacInt16(vcb + vcbSigWord, 0x4244);
	WriteMacInt32(vcb + vcbCrDate, root_crdate);
	WriteMacInt32(vcb + vcbLsMod, TimeToMacTime(root_stat.st_mtime));
	WriteMacInt16(vcb + vcbAtrb, 0);
	WriteMacInt32(vcb + vcbVolBkUp, 0);
	WriteMacInt16(vcb + vcbNmFls, 1);
	WriteMacInt16(vcb + vcbNmRtDirs, 1);
	WriteMacInt16(vcb + vcbNmAlBlks, 0xffff);
	WriteMacInt32(vcb + vcbAlBlkSiz, AL_BLK_SIZE);
	WriteMacInt32(vcb + vcbClpSiz, CLUMP_SIZE);
	WriteMacInt32(vcb + vcbNxtCNID, next_cnid);
	WriteMacInt16(vcb + vcbFreeBks, 0xffff);
	Host2Mac_memcpy(vcb + vcbVN, VOLUME_NAME, 28);
	WriteMacInt16(vcb + vcbFSID, MY_FSID);
	WriteMacInt32(vcb + vcbFilCnt, 1);
	WriteMacInt32(vcb + vcbDirCnt, 1);

	// Add VCB to VCB queue
	D(bug("  adding VCB to queue\n"));
	r.d[0] = drive_number;
	r.a[0] = fs_data + fsReturn;
	r.a[1] = vcb;
	Execute68k(fs_data + fsAddNewVCB, &r);
	int16 vRefNum = (int16)ReadMacInt16(fs_data + fsReturn);
	D(bug("  UTAddNewVCB() returned %d, vRefNum %d\n", r.d[0], vRefNum));
	if (r.d[0] & 0xffff)
		return (int16)r.d[0];

	mounted_vcb = vcb;

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
	uint32 the_vcb = vcb ? vcb : mounted_vcb;
	if (the_vcb == 0)
		return nsvErr;
	D(bug(" fs_unmount_vol(%08x), vRefNum %d\n", the_vcb, (int16)ReadMacInt16(the_vcb + vcbVRefNum)));
	M68kRegisters r;

	if (mounted_vcb == the_vcb)
		mounted_vcb = 0;

	// Remove and free VCB
	D(bug("  freeing VCB\n"));
	r.a[0] = the_vcb;
	Execute68k(fs_data + fsDisposeVCB, &r);
	D(bug("  UTDisposeVCB() returned %d\n", r.d[0]));
	return (int16)r.d[0];
}

// Get information about a volume (VolumeParam / HVolumeParam)
static int16 fs_get_vol_info(uint32 pb, bool hfs, uint32 vcb)
{
	struct stat st;
	if (stat(RootPath, &st) == 0 && S_ISDIR(st.st_mode))
		root_stat = st;

	uint32 ls_mod = TimeToMacTime(root_stat.st_mtime);
	uint32 the_vcb = vcb ? vcb : mounted_vcb;
	if (the_vcb) {
		WriteMacInt32(the_vcb + vcbLsMod, ls_mod);
		WriteMacInt32(the_vcb + vcbNxtCNID, next_cnid);
		WriteMacInt16(pb + ioVRefNum, ReadMacInt16(the_vcb + vcbVRefNum));
	}

	if (ReadMacInt32(pb + ioNamePtr))
		pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), VOLUME_NAME);
	WriteMacInt32(pb + ioVCrDate, root_crdate);
	WriteMacInt32(pb + ioVLsMod, ls_mod);
	WriteMacInt16(pb + ioVAtrb, 0);
	WriteMacInt16(pb + ioVNmFls, 1);
	WriteMacInt16(pb + ioVBitMap, 0);
	WriteMacInt16(pb + ioAllocPtr, 0);
	WriteMacInt16(pb + ioVNmAlBlks, 0xffff);
	WriteMacInt32(pb + ioVAlBlkSiz, AL_BLK_SIZE);
	WriteMacInt32(pb + ioVClpSiz, CLUMP_SIZE);
	WriteMacInt16(pb + ioAlBlSt, 0);
	WriteMacInt32(pb + ioVNxtCNID, next_cnid);
	WriteMacInt16(pb + ioVFrBlk, 0xffff);
	if (hfs) {
		WriteMacInt16(pb + ioVSigWord, 0x4244);
		WriteMacInt16(pb + ioVDrvInfo, drive_number);
		WriteMacInt16(pb + ioVDRefNum, ReadMacInt16(fs_data + fsDrvStatus + dsQRefNum));
		WriteMacInt16(pb + ioVFSID, MY_FSID);
		WriteMacInt32(pb + ioVBkUp, 0);
		WriteMacInt16(pb + ioVSeqNum, 0);
		WriteMacInt32(pb + ioVWrCnt, 0);
		WriteMacInt32(pb + ioVFilCnt, 1);
		WriteMacInt32(pb + ioVDirCnt, 1);
		Mac_memset(pb + ioVFndrInfo, 0, 32);
	}
	return noErr;
}

// Get extended information about a volume (XVolumeParam)
static int16 fs_x_get_vol_info(uint32 pb, uint32 vcb)
{
	int16 err = fs_get_vol_info(pb, true, vcb);
	if (err != noErr)
		return err;
	WriteMacInt32(pb + ioXVersion, 0);
	uint32 vol_bytes = (uint32)0xffff * (uint32)AL_BLK_SIZE;
	WriteMacInt32(pb + ioVTotalBytes, 0);
	WriteMacInt32(pb + ioVTotalBytes + 4, vol_bytes);
	WriteMacInt32(pb + ioVFreeBytes, 0);
	WriteMacInt32(pb + ioVFreeBytes + 4, vol_bytes);
	return noErr;
}

// Change volume information (HVolumeParam)
static int16 fs_set_vol_info(uint32 pb)
{
	D(bug(" fs_set_vol_info(%08x)\n", pb));
	(void)pb;
	return noErr;
}

// Get volume parameter block
static int16 fs_get_vol_parms(uint32 pb)
{
	// Return parameter block
	uint32 actual = ReadMacInt32(pb + ioReqCount);
	if (actual > SIZEOF_GetVolParmsInfoBuffer)
		actual = SIZEOF_GetVolParmsInfoBuffer;
	WriteMacInt32(pb + ioActCount, actual);
	uint32 p = ReadMacInt32(pb + ioBuffer);
	if (p == 0 && actual > 0)
		return paramErr;
	if (actual >= 2) WriteMacInt16(p + vMVersion, 2);
	if (actual >= 6) WriteMacInt32(p + vMAttrib, kNoMiniFndr | kNoVNEdit | kNoLclSync | kTrshOffLine | kNoSwitchTo | kNoBootBlks | kNoSysDir | kHasExtFSVol);
	if (actual >= 10) WriteMacInt32(p + vMLocalHand, 0);
	if (actual >= 14) WriteMacInt32(p + vMServerAdr, 0);
	if (actual >= 18) WriteMacInt32(p + vMVolumeGrade, 0);
	if (actual >= 20) WriteMacInt16(p + vMForeignPrivID, 0);
	return noErr;
}

// Get default volume (IOParam / WDParam)
static int16 fs_get_vol(uint32 pb, bool hfs)
{
	D(bug(" fs_get_vol(%08x)\n", pb));
	M68kRegisters r;

	// Use scratch WDParam so non-HFS PBGetVol (50-byte IOParam) never overflows onto ioWDDirID
	uint32 wdpb = fs_data + fsReturn;
	Mac_memset(wdpb, 0, SIZEOF_WDParam);
	WriteMacInt32(wdpb + ioNamePtr, ReadMacInt32(pb + ioNamePtr));

	r.a[0] = wdpb;
	Execute68k(fs_data + fsGetDefaultVol, &r);
	int16 err = (int16)(r.d[0] & 0xffff);
	if (err == noErr) {
		WriteMacInt16(pb + ioVRefNum, ReadMacInt16(wdpb + ioVRefNum));
		if (hfs) {
			WriteMacInt32(pb + ioWDProcID, ReadMacInt32(wdpb + ioWDProcID));
			WriteMacInt16(pb + ioWDVRefNum, ReadMacInt16(wdpb + ioWDVRefNum));
			uint32 wd_dir = ReadMacInt32(wdpb + ioWDDirID);
			WriteMacInt32(pb + ioWDDirID, wd_dir ? wd_dir : ROOT_ID);
		}
	}
	return err;
}

// Set default volume (IOParam / WDParam)
static int16 fs_set_vol(uint32 pb, bool hfs, uint32 vcb)
{
	D(bug(" fs_set_vol(%08x), vRefNum %d\n", pb, (int16)ReadMacInt16(pb + ioVRefNum)));
	M68kRegisters r;

	uint32 the_vcb = vcb ? vcb : mounted_vcb;
	uint32 dirID;
	int16 refNum;
	uint32 name_ptr = ReadMacInt32(pb + ioNamePtr);
	if (hfs || (name_ptr != 0 && ReadMacInt8(name_ptr) != 0)) {

		// Find FSItem for given dir
		ResolvedPath resolved;
		int16 result = resolve_path_spec(pb, hfs ? ReadMacInt32(pb + ioWDDirID) : 0, false, resolved);
		if (result != noErr || resolved.item == NULL)
			return result;
		FSItem *fs_item = resolved.item;
		get_path_for_fsitem(fs_item);
		if (resolved.vcb != 0)
			the_vcb = resolved.vcb;
		if (the_vcb == 0)
			return nsvErr;

		// Is it a directory?
		struct stat st;
		if (stat(full_path, &st) < 0) {
			if (errno == ENOENT || errno == ENOTDIR)
				retire_fsitem(fs_item);
			return dirNFErr;
		}
		if (!S_ISDIR(st.st_mode))
			return dirNFErr;

		dirID = fs_item->id;
		refNum = (int16)ReadMacInt16(the_vcb + vcbVRefNum);

	} else {
		if (the_vcb == 0)
			return nsvErr;

		// Is the given vRefNum a working directory number?
		r.d[0] = ReadMacInt16(pb + ioVRefNum);
		Execute68k(fs_data + fsCheckWDRefNum, &r);
		if (r.d[0] & 0xffff) {
			// Volume refNum
			dirID = ROOT_ID;
			refNum = (int16)ReadMacInt16(the_vcb + vcbVRefNum);
		} else {
			// WD refNum: verify the underlying directory still exists
			r.d[0] = 0;
			r.d[1] = 0;
			r.d[2] = ReadMacInt16(pb + ioVRefNum);
			r.a[0] = fs_data + fsReturn;
			Execute68k(fs_data + fsResolveWDCB, &r);
			if (r.d[0] & 0xffff)
				return (int16)r.d[0];
			uint32 wdcb = ReadMacInt32(fs_data + fsReturn);
			uint32 wd_dir = wdcb ? ReadMacInt32(wdcb + wdDirID) : 0;
			FSItem *wd_item = find_fsitem_by_id(wd_dir ? wd_dir : ROOT_ID);
			if (!verify_existing_dir_fsitem(wd_item))
				return dirNFErr;
			dirID = 0;
			refNum = (int16)ReadMacInt16(pb + ioVRefNum);
		}
	}

	// Setting default volume
	r.d[0] = 0;
	r.d[1] = dirID;
	r.d[2] = (uint16)refNum;
	Execute68k(fs_data + fsSetDefaultVol, &r);
	return (int16)r.d[0];
}

// Query nth visible item in directory specified by dirID (sets full_path on success)
static int16 get_indexed_item_in_dir(uint32 pb, uint32 dirID, int16 dir_index, bool files_only, FSItem *&out_item, struct stat &out_st)
{
	uint32 current_dir;
	int16 result = get_current_dir(pb, dirID, current_dir, true);
	if (result != noErr)
		return result;
	if (current_dir == ROOT_PARENT_ID)
		return dirNFErr;
	FSItem *p = find_fsitem_by_id(current_dir);
	if (p == NULL)
		return dirNFErr;

	char dir_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(p, dir_path);
	DIR *d = opendir(dir_path);
	if (d == NULL) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(p);
		return dirNFErr;
	}

	struct dirent *de = NULL;
	int seen = 0;
	while ((de = readdir(d)) != NULL) {
		struct stat entry_st;
		if (!is_visible_dirent(dir_path, de, &entry_st))
			continue;
		if (files_only && !S_ISREG(entry_st.st_mode))
			continue;
		seen++;
		if (seen == dir_index) {
			out_st = entry_st;
			break;
		}
	}
	if (de == NULL) {
		closedir(d);
		return fnfErr;
	}

	strncpy(full_path, dir_path, MAX_PATH_LENGTH - 1);
	full_path[MAX_PATH_LENGTH - 1] = 0;
	add_path_comp(de->d_name);
	out_item = find_fsitem(de->d_name, p);
	closedir(d);
	return noErr;
}

// Query file attributes (FileParam / HFileParam: 80 bytes — never write past offset 79!)
static int16 fs_get_file_info(uint32 pb, bool hfs, uint32 dirID)
{
	(void)hfs;
	D(bug(" fs_get_file_info(%08x), vRefNum %d, idx %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), (int16)ReadMacInt16(pb + ioFDirIndex), dirID));

	FSItem *fs_item = NULL;
	struct stat st;
	int16 dir_index = (int16)ReadMacInt16(pb + ioFDirIndex);
	if (dir_index <= 0) {		// Query file specified by ioDirID and ioNamePtr

		int16 result = get_item_and_path(pb, dirID, fs_item);
		if (result != noErr)
			return result;
		if (stat(full_path, &st) < 0) {
			if (errno == ENOENT || errno == ENOTDIR)
				retire_fsitem(fs_item);
			return fnfErr;
		}
		if (!S_ISREG(st.st_mode))
			return fnfErr;

	} else {					// Query nth regular file in directory specified by ioDirID

		int16 result = get_indexed_item_in_dir(pb, dirID, dir_index, true, fs_item, st);
		if (result != noErr)
			return result;
	}

	// Only write to ioNamePtr when indexing (dir_index > 0); for dir_index <= 0, ioNamePtr is an input buffer!
	if (dir_index > 0 && ReadMacInt32(pb + ioNamePtr))
		cstr2pstr((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), fs_item->guest_name);

	WriteMacInt16(pb + ioFRefNum, 0);
	WriteMacInt8(pb + ioFVersNum, 0);
	WriteMacInt8(pb + ioFFiller1, 0);
	WriteMacInt8(pb + ioFlAttrib, access(full_path, W_OK) == 0 ? 0 : faLocked);
	WriteMacInt8(pb + ioFlVersNum, 0);
	get_finfo(full_path, pb + ioFlFndrInfo, 0, false);
	WriteMacInt32(pb + ioDirID, fs_item->id);

	uint32 df_log, df_phy, rf_log, rf_phy;
	calc_fork_sizes(st.st_size, df_log, df_phy);
	calc_fork_sizes((off_t)get_rfork_size(full_path), rf_log, rf_phy);

	WriteMacInt16(pb + ioFlStBlk, 0);
	WriteMacInt32(pb + ioFlLgLen, df_log);
	WriteMacInt32(pb + ioFlPyLen, df_phy);
	WriteMacInt16(pb + ioFlRStBlk, 0);
	WriteMacInt32(pb + ioFlRLgLen, rf_log);
	WriteMacInt32(pb + ioFlRPyLen, rf_phy);
	WriteMacInt32(pb + ioFlCrDat, get_item_crdate(full_path, st));
	WriteMacInt32(pb + ioFlMdDat, TimeToMacTime(st.st_mtime));
	return noErr;
}

// Set file attributes (FileParam / HFileParam: 80 bytes — never read past offset 79!)
static int16 fs_set_file_info(uint32 pb, bool hfs, uint32 dirID)
{
	(void)hfs;
	D(bug(" fs_set_file_info(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), dirID));

	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	struct stat st;
	if (stat(full_path, &st) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(fs_item);
		return errno2oserr();
	}
	if (!S_ISREG(st.st_mode))
		return fnfErr;

	set_finfo(full_path, pb + ioFlFndrInfo, 0, false);
	return noErr;
}

// Query file/directory catalog attributes (CInfoPBRec: 108 bytes)
static int16 fs_get_cat_info(uint32 pb)
{
	D(bug(" fs_get_cat_info(%08x), vRefNum %d, idx %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), (int16)ReadMacInt16(pb + ioFDirIndex), ReadMacInt32(pb + ioDirID)));

	FSItem *fs_item = NULL;
	struct stat st;
	int16 dir_index = (int16)ReadMacInt16(pb + ioFDirIndex);

	if (dir_index < 0) {			// Query directory specified by ioDrDirID (ioNamePtr is output-only)

		uint32 current_dir;
		int16 result = get_current_dir(pb, ReadMacInt32(pb + ioDrDirID), current_dir, true);
		if (result != noErr)
			return result;
		if (current_dir == ROOT_PARENT_ID)
			return dirNFErr;
		fs_item = find_fsitem_by_id(current_dir);
		if (fs_item == NULL)
			return dirNFErr;
		get_path_for_fsitem(fs_item);
		if (stat(full_path, &st) < 0) {
			if (errno == ENOENT || errno == ENOTDIR)
				retire_fsitem(fs_item);
			return dirNFErr;
		}
		if (!S_ISDIR(st.st_mode))
			return dirNFErr;

	} else if (dir_index == 0) {	// Query item specified by ioDirID and ioNamePtr (ioNamePtr is input-only)

		int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
		if (result != noErr)
			return result;
		if (stat(full_path, &st) < 0) {
			if (errno == ENOENT || errno == ENOTDIR)
				retire_fsitem(fs_item);
			return errno2oserr();
		}
		if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
			return fnfErr;

	} else {						// Query nth item in directory specified by ioDirID (ioNamePtr is output-only)

		int16 result = get_indexed_item_in_dir(pb, ReadMacInt32(pb + ioDirID), dir_index, false, fs_item, st);
		if (result != noErr)
			return result;
	}
	D(bug("  path %s\n", full_path));

	bool is_dir = S_ISDIR(st.st_mode);

	// Only write to ioNamePtr when dir_index != 0; when dir_index == 0, ioNamePtr is the caller's input string!
	if (dir_index != 0 && ReadMacInt32(pb + ioNamePtr))
		cstr2pstr((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), fs_item->guest_name);

	WriteMacInt16(pb + ioFRefNum, 0);
	WriteMacInt8(pb + ioFVersNum, 0);
	WriteMacInt8(pb + ioFFiller1, 0);
	uint8 attrib = is_dir ? faIsDir : 0;
	if (fs_item->id != ROOT_ID && access(full_path, W_OK) != 0)
		attrib |= faLocked;
	WriteMacInt8(pb + ioFlAttrib, attrib);
	WriteMacInt8(pb + ioACUser, 0);

	get_finfo(full_path, pb + ioFlFndrInfo, pb + ioFlXFndrInfo, is_dir);

	WriteMacInt32(pb + ioDirID, fs_item->id);
	WriteMacInt32(pb + ioFlParID, fs_item->parent_id);
	WriteMacInt32(pb + ioFlCrDat, (fs_item->id == ROOT_ID) ? root_crdate : get_item_crdate(full_path, st));

	time_t mtime = st.st_mtime;
	WriteMacInt32(pb + ioFlMdDat, TimeToMacTime(mtime));
	WriteMacInt32(pb + ioFlBkDat, 0);

	if (is_dir) {
		int count = fs_item->cache_dircount;
		if (count < 0 || mtime != fs_item->mtime) {
			count = 0;
			DIR *d = opendir(full_path);
			if (d) {
				struct dirent *de;
				while ((de = readdir(d)) != NULL) {
					if (is_visible_dirent(full_path, de, NULL))
						count++;
				}
				closedir(d);
			}
			fs_item->mtime = mtime;
			fs_item->cache_dircount = count;
		}
		WriteMacInt16(pb + ioDrNmFls, (uint16)(count > 0x7fff ? 0x7fff : count));
		Mac_memset(pb + ioDrFiller3, 0, SIZEOF_ioDrFiller3);
	} else {
		uint32 df_log, df_phy, rf_log, rf_phy;
		calc_fork_sizes(st.st_size, df_log, df_phy);
		calc_fork_sizes((off_t)get_rfork_size(full_path), rf_log, rf_phy);

		WriteMacInt16(pb + ioFlStBlk, 0);
		WriteMacInt32(pb + ioFlLgLen, df_log);
		WriteMacInt32(pb + ioFlPyLen, df_phy);
		WriteMacInt16(pb + ioFlRStBlk, 0);
		WriteMacInt32(pb + ioFlRLgLen, rf_log);
		WriteMacInt32(pb + ioFlRPyLen, rf_phy);
		WriteMacInt32(pb + ioFlClpSiz, 0);
	}
	return noErr;
}

// Set file/directory attributes
static int16 fs_set_cat_info(uint32 pb)
{
	D(bug(" fs_set_cat_info(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), ReadMacInt32(pb + ioDirID)));

	FSItem *fs_item;
	int16 result = get_item_and_path(pb, ReadMacInt32(pb + ioDirID), fs_item);
	if (result != noErr)
		return result;

	struct stat st;
	if (stat(full_path, &st) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(fs_item);
		return errno2oserr();
	}

	set_finfo(full_path, pb + ioFlFndrInfo, pb + ioFlXFndrInfo, S_ISDIR(st.st_mode));
	return noErr;
}

// Construct an FSSpec for a file or directory (PBMakeFSSpec / FSMakeFSSpec)
static int16 fs_make_fsspec(uint32 pb, uint32 dirID, uint32 vcb)
{
	uint32 spec = ReadMacInt32(pb + ioMisc);
	if (spec == 0)
		return paramErr;

	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, dirID, false, resolved);
	uint32 the_vcb = resolved.vcb ? resolved.vcb : (vcb ? vcb : mounted_vcb);
	int16 vol_ref_num = the_vcb ? (int16)ReadMacInt16(the_vcb + vcbVRefNum) : 0;

	if (result == noErr && resolved.item != NULL && resolved.item->id >= ROOT_ID) {
		Mac_memset(spec, 0, SIZEOF_FSSpec);
		WriteMacInt16(spec + fsSpecVRefNum, vol_ref_num);
		WriteMacInt32(spec + fsSpecParID, resolved.item->parent_id);
		cstr2pstr((char *)Mac2HostAddr(spec + fsSpecName), resolved.item->guest_name, 31);
		return noErr;
	}

	if (result == fnfErr && resolved.parent != NULL && resolved.parent->id >= ROOT_ID && !resolved.trailing_delim && resolved.leaf_guest[0] != 0) {
		Mac_memset(spec, 0, SIZEOF_FSSpec);
		WriteMacInt16(spec + fsSpecVRefNum, vol_ref_num);
		WriteMacInt32(spec + fsSpecParID, resolved.parent->id);
		cstr2pstr((char *)Mac2HostAddr(spec + fsSpecName), resolved.leaf_guest, 31);
		return fnfErr;
	}

	Mac_memset(spec, 0, SIZEOF_FSSpec);
	return (result == fnfErr) ? dirNFErr : result;
}

// Open file (data or resource fork)
static int16 fs_open(uint32 pb, uint32 dirID, uint32 vcb, bool resource_fork)
{
	D(bug(" fs_open(%08x), %s, vRefNum %d, dirID %u, perm %d\n", pb, resource_fork ? "rsrc" : "data", (int16)ReadMacInt16(pb + ioVRefNum), dirID, ReadMacInt8(pb + ioPermssn)));
	M68kRegisters r;

	// Find FSItem and VCB for given file
	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, dirID, false, resolved);
	if (result != noErr || resolved.item == NULL)
		return result;
	FSItem *fs_item = resolved.item;
	get_path_for_fsitem(fs_item);
	uint32 the_vcb = resolved.vcb ? resolved.vcb : (vcb ? vcb : mounted_vcb);
	if (the_vcb == 0)
		return nsvErr;

	struct stat base_st;
	if (stat(full_path, &base_st) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(fs_item);
		return errno2oserr();
	}
	if (!S_ISREG(base_st.st_mode))
		return fnfErr;

	// Convert ioPermssn to open() flag
	int flag = 0;
	bool write_ok = (access(full_path, W_OK) == 0);
	switch (ReadMacInt8(pb + ioPermssn)) {
		case fsCurPerm:		// Whatever is currently allowed
			flag = write_ok ? O_RDWR : O_RDONLY;
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

	// Try to open and stat the fork
	int fd = -1;
	struct stat st;
	memset(&st, 0, sizeof(st));
	if (resource_fork) {
		errno = 0;
		fd = open_rfork(full_path, flag);
		if (fd >= 0) {
			if (fstat(fd, &st) < 0) {
				int saved_errno = errno;
				close_rfork(full_path, fd);
				errno = saved_errno;
				return errno2oserr();
			}
		} else {
			if (errno == EACCES || errno == EPERM || errno == EROFS || errno == ENOSPC || errno == EMFILE || errno == ENFILE || errno == EIO)
				return errno2oserr();
			if ((flag == O_WRONLY || flag == O_RDWR) && !write_ok)
				return permErr;
			// Resource fork not supported on host filesystem or empty read-only fork: treat as 0-byte pseudo resource fork
			st.st_size = 0;
			st.st_mode = 0;
		}
	} else {
		fd = open(full_path, flag);
		if (fd < 0)
			return errno2oserr();
		if (fstat(fd, &st) < 0) {
			int saved_errno = errno;
			close(fd);
			errno = saved_errno;
			return errno2oserr();
		}
	}

	// Allocate FCB using scratch refNum so pb + ioRefNum is untouched on failure
	D(bug("  allocating FCB\n"));
	WriteMacInt16(fs_data + fsReturn, 0);
	WriteMacInt32(fs_data + fsReturn + 2, 0);
	r.a[0] = fs_data + fsReturn;
	r.a[1] = fs_data + fsReturn + 2;
	Execute68k(fs_data + fsAllocateFCB, &r);
	int16 new_refnum = (int16)ReadMacInt16(fs_data + fsReturn);
	uint32 fcb = ReadMacInt32(fs_data + fsReturn + 2);
	D(bug("  UTAllocateFCB() returned %d, fRefNum %d, fcb %08x\n", r.d[0], new_refnum, fcb));
	if ((r.d[0] & 0xffff) || fcb == 0) {
		if (fd >= 0) {
			if (resource_fork)
				close_rfork(full_path, fd);
			else
				close(fd);
		}
		return (r.d[0] & 0xffff) ? (int16)r.d[0] : tmfoErr;
	}

	WriteMacInt16(pb + ioRefNum, new_refnum);

	// Initialize FCB, fd is stored in fcbCatPos
	WriteMacInt32(fcb + fcbFlNm, fs_item->id);
	WriteMacInt8(fcb + fcbFlags, ((flag == O_WRONLY || flag == O_RDWR) ? fcbWriteMask : 0) | (resource_fork ? fcbResourceMask : 0) | (write_ok ? 0 : fcbFileLockedMask));
	WriteMacInt8(fcb + fcbTypByt, 0);
	WriteMacInt16(fcb + fcbSBlk, 0);
	uint32 fork_log, fork_phy;
	calc_fork_sizes(st.st_size, fork_log, fork_phy);
	WriteMacInt32(fcb + fcbEOF, fork_log);
	WriteMacInt32(fcb + fcbPLen, fork_phy);
	WriteMacInt32(fcb + fcbCrPs, 0);
	WriteMacInt32(fcb + fcbVPtr, the_vcb);
	WriteMacInt32(fcb + fcbClmpSize, CLUMP_SIZE);

	get_finfo(full_path, fs_data + fsPB, 0, false);
	WriteMacInt32(fcb + fcbFType, ReadMacInt32(fs_data + fsPB + fdType));

	WriteMacInt32(fcb + fcbCatPos, (uint32)fd);
	WriteMacInt32(fcb + fcbDirID, fs_item->parent_id);
	cstr2pstr((char *)Mac2HostAddr(fcb + fcbCName), fs_item->guest_name);
	return noErr;
}

// Close file
static int16 fs_close(uint32 pb)
{
	D(bug(" fs_close(%08x), refNum %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum)));
	M68kRegisters r;

	// Find FCB and fd for file
	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);

	// Close file
	if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {
		FSItem *item = find_fsitem_by_id(ReadMacInt32(fcb + fcbFlNm));
		if (item) {
			get_path_for_fsitem(item);
			close_rfork(full_path, fd);
		} else if (fd >= 0) {
			close(fd);
		}
	} else if (fd >= 0) {
		close(fd);
	}
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
	D(bug(" fs_get_fcb_info(%08x), vRefNum %d, refNum %d, idx %d\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), (int16)ReadMacInt16(pb + ioRefNum), (int16)ReadMacInt16(pb + ioFCBIndx)));
	M68kRegisters r;

	uint32 fcb = 0;
	int16 fcb_idx = (int16)ReadMacInt16(pb + ioFCBIndx);
	if (fcb_idx <= 0) {			// Get information about single open file specified by ioRefNum

		fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));

	} else {					// Get information about open file specified by 1-based index

		// Use scratch refNum so pb + ioRefNum is only updated on success
		WriteMacInt16(fs_data + fsReturn, 0);
		for (int i = 0; i < (int)fcb_idx; i++) {
			r.a[0] = vcb;
			r.a[1] = fs_data + fsReturn;
			r.a[2] = fs_data + fsReturn + 2;
			Execute68k(fs_data + fsIndexFCB, &r);
			fcb = ReadMacInt32(fs_data + fsReturn + 2);
			if (r.d[0] & 0xffff)
				return (int16)r.d[0];
		}
		if (fcb != 0 && ReadMacInt32(fcb + fcbFlNm) != 0 && ReadMacInt32(fcb + fcbVPtr) != 0)
			WriteMacInt16(pb + ioRefNum, ReadMacInt16(fs_data + fsReturn));
	}
	if (fcb == 0 || ReadMacInt32(fcb + fcbFlNm) == 0 || ReadMacInt32(fcb + fcbVPtr) == 0)
		return rfNumErr;

	// Copy information from FCB
	if (ReadMacInt32(pb + ioNamePtr))
		pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), (char *)Mac2HostAddr(fcb + fcbCName));
	WriteMacInt32(pb + ioFCBFlNm, ReadMacInt32(fcb + fcbFlNm));
	WriteMacInt16(pb + ioFCBFlags, ReadMacInt16(fcb + fcbFlags));
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
	D(bug(" fs_get_eof(%08x), refNum %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum)));
	M68kRegisters r;

	// Find FCB and fd for file
	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0) {
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioMisc, 0);
			return noErr;
		} else
			return fnOpnErr;
	}

	// Get file size
	struct stat st;
	if (fstat(fd, &st) < 0)
		return errno2oserr();

	// Adjust FCBs
	uint32 file_log, file_phy;
	calc_fork_sizes(st.st_size, file_log, file_phy);
	WriteMacInt32(fcb + fcbEOF, file_log);
	WriteMacInt32(fcb + fcbPLen, file_phy);
	WriteMacInt32(pb + ioMisc, file_log);
	D(bug("  adjusting FCBs\n"));
	r.d[0] = ReadMacInt16(pb + ioRefNum);
	Execute68k(fs_data + fsAdjustEOF, &r);
	D(bug("  UTAdjustEOF() returned %d\n", r.d[0]));
	return noErr;
}

// Truncate file
static int16 fs_set_eof(uint32 pb)
{
	D(bug(" fs_set_eof(%08x), refNum %d, size %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum), (int32)ReadMacInt32(pb + ioMisc)));
	M68kRegisters r;

	if ((int32)ReadMacInt32(pb + ioMisc) < 0)
		return paramErr;

	// Find FCB and fd for file
	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	if (!(ReadMacInt8(fcb + fcbFlags) & fcbWriteMask))
		return wrPermErr;
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0) {
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask)	// "pseudo" resource fork
			return noErr;
		else
			return fnOpnErr;
	}

	// Truncate file
	uint32 size = ReadMacInt32(pb + ioMisc);
	if (ftruncate(fd, (off_t)size) < 0)
		return errno2oserr();

	// Adjust FCBs
	uint32 file_log, file_phy;
	calc_fork_sizes((off_t)size, file_log, file_phy);
	WriteMacInt32(fcb + fcbEOF, file_log);
	WriteMacInt32(fcb + fcbPLen, file_phy);
	D(bug("  adjusting FCBs\n"));
	r.d[0] = ReadMacInt16(pb + ioRefNum);
	Execute68k(fs_data + fsAdjustEOF, &r);
	D(bug("  UTAdjustEOF() returned %d\n", r.d[0]));
	return noErr;
}

// Query current file position
static int16 fs_get_fpos(uint32 pb)
{
	D(bug(" fs_get_fpos(%08x), refNum %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum)));

	WriteMacInt32(pb + ioReqCount, 0);
	WriteMacInt32(pb + ioActCount, 0);
	WriteMacInt16(pb + ioPosMode, 0);

	// Find FCB and fd for file
	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0) {
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioPosOffset, 0);
			return noErr;
		} else
			return fnOpnErr;
	}

	// Get file position
	off_t raw_pos = lseek(fd, 0, SEEK_CUR);
	if (raw_pos < 0)
		return ioErr;
	uint32 pos = (raw_pos > (off_t)0x7fffffff) ? 0x7fffffffU : (uint32)raw_pos;
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	return noErr;
}

// Set current file position
static int16 fs_set_fpos(uint32 pb)
{
	D(bug(" fs_set_fpos(%08x), refNum %d, posMode %d, offset %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum), ReadMacInt16(pb + ioPosMode), (int32)ReadMacInt32(pb + ioPosOffset)));

	// Find FCB and fd for file
	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0)
		return rfNumErr;
	if (ReadMacInt32(fcb + fcbFlNm) == 0)
		return fnOpnErr;
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0) {
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioPosOffset, 0);
			return noErr;
		} else
			return fnOpnErr;
	}

	// Set file position
	switch (ReadMacInt16(pb + ioPosMode) & 3) {
		case fsFromStart:
			if ((int32)ReadMacInt32(pb + ioPosOffset) < 0 || lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_SET) < 0)
				return posErr;
			break;
		case fsFromLEOF:
			if (lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_END) < 0)
				return posErr;
			break;
		case fsFromMark:
			if (lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_CUR) < 0)
				return posErr;
			break;
		default:
			break;
	}
	off_t raw_pos = lseek(fd, 0, SEEK_CUR);
	uint32 pos = (raw_pos > 0) ? ((raw_pos > (off_t)0x7fffffff) ? 0x7fffffffU : (uint32)raw_pos) : 0;
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
	return noErr;
}

// Validate parameters, resolve FCB/fd, and seek for fs_read / fs_write
static int16 prepare_fcb_io(uint32 pb, bool is_write, uint32 &out_fcb, int &out_fd, uint32 &out_req, uint32 &out_buf)
{
	if ((int32)ReadMacInt32(pb + ioReqCount) < 0) {
		WriteMacInt32(pb + ioActCount, 0);
		return paramErr;
	}

	uint32 fcb = find_fcb((int16)ReadMacInt16(pb + ioRefNum));
	if (fcb == 0) {
		WriteMacInt32(pb + ioActCount, 0);
		return rfNumErr;
	}
	if (ReadMacInt32(fcb + fcbFlNm) == 0) {
		WriteMacInt32(pb + ioActCount, 0);
		return fnOpnErr;
	}
	if (is_write && !(ReadMacInt8(fcb + fcbFlags) & fcbWriteMask)) {
		WriteMacInt32(pb + ioActCount, 0);
		return wrPermErr;
	}
	int fd = (int32)ReadMacInt32(fcb + fcbCatPos);
	if (fd < 0) {
		if (ReadMacInt8(fcb + fcbFlags) & fcbResourceMask) {	// "pseudo" resource fork
			WriteMacInt32(pb + ioActCount, is_write ? ReadMacInt32(pb + ioReqCount) : 0);
			return is_write ? noErr : eofErr;
		}
		WriteMacInt32(pb + ioActCount, 0);
		return fnOpnErr;
	}

	switch (ReadMacInt16(pb + ioPosMode) & 3) {
		case fsFromStart:
			if ((int32)ReadMacInt32(pb + ioPosOffset) < 0 || lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_SET) < 0) {
				WriteMacInt32(pb + ioActCount, 0);
				return posErr;
			}
			break;
		case fsFromLEOF:
			if (lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_END) < 0) {
				WriteMacInt32(pb + ioActCount, 0);
				return posErr;
			}
			break;
		case fsFromMark:
			if (lseek(fd, (off_t)(int32)ReadMacInt32(pb + ioPosOffset), SEEK_CUR) < 0) {
				WriteMacInt32(pb + ioActCount, 0);
				return posErr;
			}
			break;
	}

	uint32 req_count = ReadMacInt32(pb + ioReqCount);
	uint32 buf_addr = ReadMacInt32(pb + ioBuffer);
	if (req_count > 0 && buf_addr == 0) {
		WriteMacInt32(pb + ioActCount, 0);
		return paramErr;
	}

	out_fcb = fcb;
	out_fd = fd;
	out_req = req_count;
	out_buf = buf_addr;
	return noErr;
}

static void finish_fcb_pos(uint32 pb, uint32 fcb, int fd, ssize_t actual)
{
	WriteMacInt32(pb + ioActCount, actual >= 0 ? (uint32)actual : 0);
	off_t raw_pos = lseek(fd, 0, SEEK_CUR);
	uint32 pos = (raw_pos > 0) ? ((raw_pos > (off_t)0x7fffffff) ? 0x7fffffffU : (uint32)raw_pos) : 0;
	WriteMacInt32(fcb + fcbCrPs, pos);
	WriteMacInt32(pb + ioPosOffset, pos);
}

// Read from file
static int16 fs_read(uint32 pb)
{
	D(bug(" fs_read(%08x), refNum %d, count %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum), (int32)ReadMacInt32(pb + ioReqCount)));

	uint32 fcb = 0, req_count = 0, buf_addr = 0;
	int fd = -1;
	int16 prep_err = prepare_fcb_io(pb, false, fcb, fd, req_count, buf_addr);
	if (prep_err != noErr || fd < 0)
		return prep_err;

	ssize_t actual = 0;
	int16 read_err = noErr;
	if (req_count > 0) {
		actual = extfs_read(fd, Mac2HostAddr(buf_addr), req_count);
		if (actual < 0)
			read_err = errno2oserr();
	}
	finish_fcb_pos(pb, fcb, fd, actual);
	if (actual != (ssize_t)req_count)
		return actual < 0 ? read_err : eofErr;
	return noErr;
}

// Write to file
static int16 fs_write(uint32 pb)
{
	D(bug(" fs_write(%08x), refNum %d, count %d\n", pb, (int16)ReadMacInt16(pb + ioRefNum), (int32)ReadMacInt32(pb + ioReqCount)));

	uint32 fcb = 0, req_count = 0, buf_addr = 0;
	int fd = -1;
	int16 prep_err = prepare_fcb_io(pb, true, fcb, fd, req_count, buf_addr);
	if (prep_err != noErr || fd < 0)
		return prep_err;

	ssize_t actual = 0;
	int16 write_err = noErr;
	if (req_count > 0) {
		actual = extfs_write(fd, Mac2HostAddr(buf_addr), req_count);
		if (actual < 0)
			write_err = errno2oserr();
		else if ((size_t)actual < req_count)
			write_err = dskFulErr;
	}
	finish_fcb_pos(pb, fcb, fd, actual);
	if (actual > 0) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			uint32 file_log, file_phy;
			calc_fork_sizes(st.st_size, file_log, file_phy);
			WriteMacInt32(fcb + fcbEOF, file_log);
			WriteMacInt32(fcb + fcbPLen, file_phy);
		}
	}
	if (actual != (ssize_t)req_count)
		return write_err;
	return noErr;
}

// Create file
static int16 fs_create(uint32 pb, uint32 dirID)
{
	D(bug(" fs_create(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), dirID));

	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, dirID, false, resolved);
	if (result == noErr)
		return dupFNErr;
	if (result != fnfErr || resolved.parent == NULL || resolved.parent->id < ROOT_ID || resolved.trailing_delim)
		return (result == fnfErr) ? dirNFErr : result;

	char new_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(resolved.parent, new_path);
	add_path_component(new_path, resolved.leaf_host);

	if (access(new_path, F_OK) == 0)
		return dupFNErr;

	int fd = open(new_path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd < 0)
		return errno2oserr();
	close(fd);

	resolved.parent->cache_dircount = -1;
	find_fsitem(resolved.leaf_host, resolved.parent);
	return noErr;
}

// Create directory
static int16 fs_dir_create(uint32 pb)
{
	D(bug(" fs_dir_create(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), ReadMacInt32(pb + ioDirID)));

	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, ReadMacInt32(pb + ioDirID), false, resolved);
	if (result == noErr)
		return dupFNErr;
	bool valid_missing = (result == fnfErr) || (result == dirNFErr && resolved.trailing_delim && resolved.parent != NULL && resolved.leaf_host[0] != 0);
	if (!valid_missing || resolved.parent == NULL || resolved.parent->id < ROOT_ID)
		return (result == fnfErr) ? dirNFErr : result;

	char new_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(resolved.parent, new_path);
	add_path_component(new_path, resolved.leaf_host);

	if (access(new_path, F_OK) == 0)
		return dupFNErr;

	if (mkdir(new_path, 0777) < 0)
		return errno2oserr();

	resolved.parent->cache_dircount = -1;
	FSItem *dir_item = find_fsitem(resolved.leaf_host, resolved.parent);
	WriteMacInt32(pb + ioDirID, dir_item->id);
	return noErr;
}

// Delete file/directory
static int16 fs_delete(uint32 pb, uint32 dirID)
{
	D(bug(" fs_delete(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), dirID));

	FSItem *fs_item;
	int16 result = get_item_and_path(pb, dirID, fs_item);
	if (result != noErr)
		return result;

	if (fs_item->id <= ROOT_ID)
		return fBsyErr;

	struct stat st;
	if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
		// Reject non-empty directories (also guards platforms like Windows where remove() recurses)
		DIR *d = opendir(full_path);
		if (d != NULL) {
			struct dirent *de;
			while ((de = readdir(d)) != NULL) {
				if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0 &&
				    strcmp(de->d_name, ".finf") != 0 && strcmp(de->d_name, ".rsrc") != 0) {
					closedir(d);
					return fBsyErr;
				}
			}
			closedir(d);
		}
	}

	if (!extfs_remove(full_path))
		return errno2oserr();

	retire_fsitem(fs_item);
	return noErr;
}

// Rename file/directory in place (preserving CNID and all child parent_id links)
static int16 fs_rename(uint32 pb, uint32 dirID)
{
	D(bug(" fs_rename(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), dirID));

	ResolvedPath src;
	int16 result = resolve_path_spec(pb, dirID, false, src);
	if (result != noErr || src.item == NULL)
		return result;

	FSItem *fs_item = src.item;
	if (fs_item->id <= ROOT_ID || fs_item->parent == NULL)
		return wPrErr;

	uint32 new_name_ptr = ReadMacInt32(pb + ioMisc);
	if (new_name_ptr == 0 || ReadMacInt8(new_name_ptr) == 0)
		return bdNamErr;

	char old_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(fs_item, old_path);
	struct stat src_st;
	if (stat(old_path, &src_st) < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			retire_fsitem(fs_item);
		return errno2oserr();
	}
	bool src_is_dir = S_ISDIR(src_st.st_mode);

	// Resolve new name relative to the existing item's parent directory
	Mac2Mac_memcpy(fs_data + fsPB, pb, SIZEOF_IOParam);
	if (ReadMacInt16(pb + ioVRefNum) == 0 && src.vcb != 0)
		WriteMacInt16(fs_data + fsPB + ioVRefNum, ReadMacInt16(src.vcb + vcbVRefNum));
	WriteMacInt32(fs_data + fsPB + ioNamePtr, new_name_ptr);
	ResolvedPath dst;
	result = resolve_path_spec(fs_data + fsPB, fs_item->parent_id, false, dst);
	if (result == noErr) {
		// Allow case-only rename of the same item; reject collision with a different item
		if (dst.item != fs_item)
			return dupFNErr;
		if (dst.parent != fs_item->parent)
			return badMovErr;
		if (dst.trailing_delim && !src_is_dir)
			return dirNFErr;
	} else {
		bool valid_missing = (result == fnfErr) || (result == dirNFErr && src_is_dir && dst.trailing_delim && dst.parent != NULL && dst.parsed_leaf[0] != 0);
		if (!valid_missing)
			return result;
		if (dst.parent != fs_item->parent)
			return badMovErr;
	}

	char new_host[MAX_PATH_LENGTH];
	if (!convert_guest_leaf_to_host(dst.parsed_leaf, new_host, sizeof(new_host)))
		return bdNamErr;

	char new_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(fs_item->parent, new_path);
	add_path_component(new_path, new_host);

	if (dst.item != fs_item && access(new_path, F_OK) == 0)
		return dupFNErr;

	if (strcmp(old_path, new_path) != 0) {
		if (!extfs_rename(old_path, new_path))
			return errno2oserr();
	}

	// Update FSItem name and guest_name in place so its CNID and subtree stay intact
	size_t new_host_len = strlen(new_host);
	char *new_name_alloc = new char[new_host_len + 1];
	memcpy(new_name_alloc, new_host, new_host_len + 1);
	delete[] fs_item->name;
	fs_item->name = new_name_alloc;

	const char *mr = host_encoding_to_macroman(fs_item->name);
	char raw_guest[MAX_PATH_LENGTH];
	strncpy(raw_guest, mr ? mr : fs_item->name, sizeof(raw_guest) - 1);
	raw_guest[sizeof(raw_guest) - 1] = 0;
	make_unique_guest_name(raw_guest, fs_item->parent, fs_item->name, fs_item->guest_name);
	fs_item->parent->cache_dircount = -1;
	return noErr;
}

// Move file/directory (CMovePBRec) in place (preserving CNID and all child parent_id links)
static int16 fs_cat_move(uint32 pb)
{
	D(bug(" fs_cat_move(%08x), vRefNum %d, dirID %u, new dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), ReadMacInt32(pb + ioDirID), ReadMacInt32(pb + ioNewDirID)));

	// Find source item
	ResolvedPath src;
	int16 result = resolve_path_spec(pb, ReadMacInt32(pb + ioDirID), false, src);
	if (result != noErr || src.item == NULL)
		return result;
	FSItem *fs_item = src.item;
	if (fs_item->id <= ROOT_ID)
		return badMovErr;

	char old_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(fs_item, old_path);

	// Find destination directory
	Mac2Mac_memcpy(fs_data + fsPB, pb, SIZEOF_IOParam);
	if (ReadMacInt16(pb + ioVRefNum) == 0 && src.vcb != 0)
		WriteMacInt16(fs_data + fsPB + ioVRefNum, ReadMacInt16(src.vcb + vcbVRefNum));
	WriteMacInt32(fs_data + fsPB + ioNamePtr, ReadMacInt32(pb + ioNewName));
	FSItem *new_dir_item;
	result = get_item_and_path(fs_data + fsPB, ReadMacInt32(pb + ioNewDirID), new_dir_item);
	if (result != noErr)
		return result;
	if (new_dir_item->id < ROOT_ID || !verify_existing_dir_fsitem(new_dir_item))
		return badMovErr;

	// Reject moving a directory into itself or one of its own descendants
	for (FSItem *anc = new_dir_item; anc != NULL; anc = anc->parent) {
		if (anc == fs_item)
			return badMovErr;
	}
	if (new_dir_item == fs_item->parent)
		return dupFNErr;

	char new_path[MAX_PATH_LENGTH];
	get_path_for_fsitem_buf(new_dir_item, new_path);
	add_path_component(new_path, fs_item->name);

	if (resolve_child_by_guest_name(new_dir_item, fs_item->guest_name) != NULL || access(new_path, F_OK) == 0)
		return dupFNErr;

	if (!extfs_rename(old_path, new_path))
		return errno2oserr();

	// Re-parent fs_item in place so its CNID and any cached children remain valid
	if (fs_item->parent) {
		unlink_child_fsitem(fs_item->parent, fs_item);
		fs_item->parent->cache_dircount = -1;
	}
	fs_item->parent = new_dir_item;
	fs_item->parent_id = new_dir_item->id;

	const char *mr = host_encoding_to_macroman(fs_item->name);
	char raw_guest[MAX_PATH_LENGTH];
	strncpy(raw_guest, mr ? mr : fs_item->name, sizeof(raw_guest) - 1);
	raw_guest[sizeof(raw_guest) - 1] = 0;
	make_unique_guest_name(raw_guest, new_dir_item, fs_item->name, fs_item->guest_name);

	link_child_fsitem(new_dir_item, fs_item);
	new_dir_item->cache_dircount = -1;
	return noErr;
}

// Open working directory (WDParam)
static int16 fs_open_wd(uint32 pb, uint32 vcb)
{
	D(bug(" fs_open_wd(%08x), vRefNum %d, dirID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), ReadMacInt32(pb + ioWDDirID)));
	M68kRegisters r;

	// Resolve and validate target directory before allocating WDCB
	ResolvedPath resolved;
	int16 result = resolve_path_spec(pb, ReadMacInt32(pb + ioWDDirID), false, resolved);
	if (result != noErr || resolved.item == NULL)
		return result;
	FSItem *fs_item = resolved.item;
	if (fs_item->id < ROOT_ID || !verify_existing_dir_fsitem(fs_item))
		return dirNFErr;
	uint32 the_vcb = resolved.vcb ? resolved.vcb : (vcb ? vcb : mounted_vcb);
	if (the_vcb == 0)
		return nsvErr;

	uint32 wdpb = fs_data + fsReturn;
	Mac2Mac_memcpy(wdpb, pb, SIZEOF_WDParam);
	WriteMacInt32(wdpb + ioNamePtr, 0);
	WriteMacInt16(wdpb + ioVRefNum, ReadMacInt16(the_vcb + vcbVRefNum));
	WriteMacInt32(wdpb + ioWDDirID, fs_item->id);

	r.a[0] = wdpb;
	Execute68k(fs_data + fsAllocateWDCB, &r);
	int16 err = (int16)(r.d[0] & 0xffff);
	if (err == noErr) {
		WriteMacInt16(pb + ioVRefNum, ReadMacInt16(wdpb + ioVRefNum));
		WriteMacInt16(pb + ioWDCreated, ReadMacInt16(wdpb + ioWDCreated));
	}
	return err;
}

// Close working directory (WDParam)
static int16 fs_close_wd(uint32 pb)
{
	D(bug(" fs_close_wd(%08x), vRefNum %d\n", pb, (int16)ReadMacInt16(pb + ioVRefNum)));
	M68kRegisters r;

	r.d[0] = ReadMacInt16(pb + ioVRefNum);
	Execute68k(fs_data + fsReleaseWDCB, &r);
	return (int16)r.d[0];
}

// Query information about working directory (WDParam)
static int16 fs_get_wd_info(uint32 pb, uint32 vcb)
{
	D(bug(" fs_get_wd_info(%08x), vRefNum %d, idx %d, procID %u\n", pb, (int16)ReadMacInt16(pb + ioVRefNum), (int16)ReadMacInt16(pb + ioWDIndex), ReadMacInt32(pb + ioWDProcID)));
	M68kRegisters r;

	uint32 the_vcb = vcb ? vcb : mounted_vcb;
	if (the_vcb == 0)
		return nsvErr;

	int16 wd_index = (int16)ReadMacInt16(pb + ioWDIndex);
	int16 vref = (int16)ReadMacInt16(pb + ioVRefNum);
	int16 vol_vref = (int16)ReadMacInt16(the_vcb + vcbVRefNum);

	if (wd_index <= 0) {
		if (vref == vol_vref || vref == drive_number) {
			WriteMacInt32(pb + ioWDProcID, 0);
			WriteMacInt16(pb + ioWDVRefNum, vol_vref);
			if (ReadMacInt32(pb + ioNamePtr))
				pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), VOLUME_NAME);
			WriteMacInt32(pb + ioWDDirID, ROOT_ID);
			return noErr;
		}
		if (vref == 0) {
			uint32 wdpb = fs_data + fsReturn;
			Mac_memset(wdpb, 0, SIZEOF_WDParam);
			r.a[0] = wdpb;
			Execute68k(fs_data + fsGetDefaultVol, &r);
			int16 err = (int16)(r.d[0] & 0xffff);
			if (err != noErr)
				return err;
			WriteMacInt32(pb + ioWDProcID, ReadMacInt32(wdpb + ioWDProcID));
			WriteMacInt16(pb + ioWDVRefNum, vol_vref);
			if (ReadMacInt32(pb + ioNamePtr))
				pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), VOLUME_NAME);
			uint32 wd_dir = ReadMacInt32(wdpb + ioWDDirID);
			WriteMacInt32(pb + ioWDDirID, wd_dir ? wd_dir : ROOT_ID);
			return noErr;
		}
	}

	// Resolve WDCB
	r.d[0] = (wd_index > 0) ? ReadMacInt32(pb + ioWDProcID) : 0;
	r.d[1] = (wd_index > 0) ? (uint16)wd_index : 0;
	r.d[2] = (uint16)vref;
	r.a[0] = fs_data + fsReturn;
	Execute68k(fs_data + fsResolveWDCB, &r);
	uint32 wdcb = ReadMacInt32(fs_data + fsReturn);
	if ((r.d[0] & 0xffff) || wdcb == 0)
		return (r.d[0] & 0xffff) ? (int16)r.d[0] : rfNumErr;

	uint32 wd_vcb = ReadMacInt32(wdcb + wdVCBPtr);
	if (wd_vcb == 0)
		return rfNumErr;

	WriteMacInt32(pb + ioWDProcID, ReadMacInt32(wdcb + wdProcID));
	WriteMacInt16(pb + ioWDVRefNum, ReadMacInt16(wd_vcb + vcbVRefNum));
	if (ReadMacInt32(pb + ioNamePtr))
		pstrcpy((char *)Mac2HostAddr(ReadMacInt32(pb + ioNamePtr)), (char *)Mac2HostAddr(wd_vcb + vcbVN));
	uint32 wd_dir = ReadMacInt32(wdcb + wdDirID);
	WriteMacInt32(pb + ioWDDirID, wd_dir ? wd_dir : ROOT_ID);
	return noErr;
}

// Main dispatch routine
int16 ExtFSHFS(uint32 vcb, uint16 selectCode, uint32 paramBlock, uint32 globalsPtr, int16 fsid)
{
	uint16 trapWord = selectCode & 0xf0ff;
	bool hfs = (selectCode & kHFSMask) != 0;
	switch (trapWord) {
		case kFSMOpen:
			return fs_open(paramBlock, hfs ? ReadMacInt32(paramBlock + ioDirID) : 0, vcb, false);

		case kFSMOpenDF:
			// Mac OS 9's FSM invokes kFSMOpenDF without kHFSMask even for FSpOpenDF/PBHOpenDFSync;
			// gating on hfs breaks FSpOpenDF on Mac OS 9.
			return fs_open(paramBlock, ReadMacInt32(paramBlock + ioDirID), vcb, false);

		case kFSMClose:
			return fs_close(paramBlock);

		case kFSMRead:
			return fs_read(paramBlock);

		case kFSMWrite:
			return fs_write(paramBlock);

		case kFSMGetVolInfo:
			return fs_get_vol_info(paramBlock, hfs, vcb);

		case kFSMXGetVolInfo:
			return fs_x_get_vol_info(paramBlock, vcb);

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
			return fs_get_vol(paramBlock, hfs);

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
			return noErr;

		case kFSMRstFilLock:
			return noErr;

		case kFSMSetFPos:
			return fs_set_fpos(paramBlock);

		case kFSMOpenWD:
			return fs_open_wd(paramBlock, vcb);

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

		case kFSMMakeFSSpec:
			// _HFSDispatch 0x001B (PBMakeFSSpecSync) always passes HParmBlkPtr without kHFSMask
			return fs_make_fsspec(paramBlock, ReadMacInt32(paramBlock + ioDirID), vcb);

		case kFSMCreateFileIDRef:
			WriteMacInt32(paramBlock + ioFileID, 0);
			return paramErr;

		case kFSMDeleteFileIDRef:
		case kFSMResolveFileIDRef:
			return paramErr;

		case kFSMGetVolParms:
			return fs_get_vol_parms(paramBlock);

		case kFSMVolumeMount:
			return fs_volume_mount(paramBlock);

		case kFSMFlushVol:
		case kFSMFlushFile:
			D(bug(" flush_vol/flush_file\n"));
			return noErr;

		default:
			D(bug("ExtFSHFS(%08x, %04x, %08x, %08x, %d)\n", vcb, selectCode, paramBlock, globalsPtr, fsid));
			return paramErr;
	}
}
