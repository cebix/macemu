/*
 *  extfs_macosx.cpp - MacOS file system for access native file system access, MacOS X specific stuff
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <sys/xattr.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include "sysdeps.h"
#include "prefs.h"
#include "extfs.h"
#include "extfs_defs.h"

// XXX: don't clobber with native definitions
#define noErr	native_noErr
#define Point	native_Point
#define Rect	native_Rect
#define ProcPtr	native_ProcPtr
# include <CoreFoundation/CFString.h>
#undef ProcPtr
#undef Rect
#undef Point
#undef noErr

#define DEBUG 0
#include "debug.h"


// Default Finder flags for files (must NOT be set on folders)
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;


/*
 *  Extended attributes
 */

#define XATTR_FINFO  "org.BasiliskII.FinderInfo"
#define XATTR_FXINFO "org.BasiliskII.ExtendedFinderInfo"

static bool get_xattr(const char *path, const char *name, void *value, size_t size)
{
	return getxattr(path, name, value, size, 0, 0) == (ssize_t)size;
}

static bool set_xattr(const char *path, const char *name, const void *value, size_t size)
{
	return setxattr(path, name, value, size, 0, 0) == 0;
}


/*
 *  Initialization
 */

void extfs_init(void)
{
}


/*
 *  Deinitialization
 */

void extfs_exit(void)
{
}


/*
 *  Add component to path name
 */

void add_path_component(char *path, const char *component)
{
	size_t l = strlen(path);
	if (l > 0 && l < MAX_PATH_LENGTH - 1 && path[l - 1] != '/') {
		path[l] = '/';
		path[l + 1] = 0;
	}
	strlcat(path, component, MAX_PATH_LENGTH);
}


/*
 *  Finder info manipulation helpers
 */

typedef uint8 FinderInfo[SIZEOF_FInfo];

struct FinderInfoAttrBuf {
	uint32 length;
	FinderInfo finderInfo;
	FinderInfo extendedFinderInfo;
};

static const FinderInfo kNativeFInfoMask  = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00};
static const FinderInfo kNativeDInfoMask  = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x74,0x0f,0x00,0x00,0x00,0x00,0x00,0x00};

static void finfo_merge(FinderInfo dst, const FinderInfo emu, const FinderInfo nat, const FinderInfo mask)
{
	for (int i = 0; i < SIZEOF_FInfo; i++)
		dst[i] = (emu[i] & ~mask[i]) | (nat[i] & mask[i]);
}

static void finfo_split(FinderInfo dst, const FinderInfo emu, const FinderInfo mask)
{
	for (int i = 0; i < SIZEOF_FInfo; i++)
		dst[i] = emu[i] & mask[i];
}


/*
 *  Finder info helper fallback files
 *
 *  Finder info:
 *    /path/.finf/file
 *
 *  The .finf files store a FInfo/DInfo, followed by a FXInfo/DXInfo
 *  (16+16 bytes)
 */

static void make_finf_path(const char *src, char *dest, bool only_dir = false)
{
	dest[0] = 0;

	// Get pointer to last component of path
	const char *last_part = strrchr(src, '/');
	if (last_part)
		last_part++;
	else
		last_part = src;

	// Copy everything before
	size_t prefix_len = (size_t)(last_part - src);
	if (prefix_len >= MAX_PATH_LENGTH)
		prefix_len = MAX_PATH_LENGTH - 1;
	memcpy(dest, src, prefix_len);
	dest[prefix_len] = 0;

	// Add additional component
	strlcat(dest, ".finf/", MAX_PATH_LENGTH);

	// Add last component
	if (!only_dir)
		strlcat(dest, last_part, MAX_PATH_LENGTH);
}

static int create_finf_dir(const char *path)
{
	char finf_dir[MAX_PATH_LENGTH];
	make_finf_path(path, finf_dir, true);
	size_t len = strlen(finf_dir);
	if (len > 0 && finf_dir[len - 1] == '/')	// Remove trailing "/"
		finf_dir[len - 1] = 0;
	return mkdir(finf_dir, 0777);
}

static int open_finf(const char *path, int flag)
{
	char finf_path[MAX_PATH_LENGTH];
	make_finf_path(path, finf_path);

	if ((flag & O_ACCMODE) == O_RDWR || (flag & O_ACCMODE) == O_WRONLY)
		flag |= O_CREAT;
	int fd = open(finf_path, flag, 0666);
	if (fd < 0) {
		if (errno == ENOENT && (flag & O_CREAT)) {
			// One path component was missing, probably the finf
			// directory. Try to create it and re-open the file.
			int ret = create_finf_dir(path);
			if (ret < 0)
				return ret;
			fd = open(finf_path, flag, 0666);
		}
	}
	return fd;
}


/*
 *  Resource forks are kept into their native location
 *
 *  Resource fork:
 *    /path/file/..namedfork/rsrc
 */

static void make_rsrc_path(const char *src, char *dest)
{
	size_t l = strlen(src);
	if (l + 1 + 16 + 1 <= MAX_PATH_LENGTH)
		memcpy(dest, src, l + 1);
	else {
		// The rsrc component is copied as is, if there is not enough
		// space to add it. In that case, open() will fail gracefully
		// and this is what we want.
		dest[0] = '.';
		dest[1] = '\0';
	}

	add_path_component(dest, "..namedfork/rsrc");
}

static int open_rsrc(const char *path, int flag)
{
	char rsrc_path[MAX_PATH_LENGTH];
	make_rsrc_path(path, rsrc_path);

	int fd = open(rsrc_path, flag);
	int accmode = flag & O_ACCMODE;
	if (fd < 0 && (accmode == O_WRONLY || accmode == O_RDWR))
		fd = open(rsrc_path, flag | O_CREAT, 0666); // for APFS
	return fd;
}


/*
 *  Get/set finder info for file/directory specified by full path
 */

struct ext2type {
	const char *ext;
	uint32 type;
	uint32 creator;
};

static const ext2type e2t_translation[] = {
	{".Z", FOURCC('Z','I','V','M'), FOURCC('L','Z','I','V')},
	{".gz", FOURCC('G','z','i','p'), FOURCC('G','z','i','p')},
	{".hqx", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".bin", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".pdf", FOURCC('P','D','F',' '), FOURCC('C','A','R','O')},
	{".ps", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".sit", FOURCC('S','I','T','!'), FOURCC('S','I','T','x')},
	{".tar", FOURCC('T','A','R','F'), FOURCC('T','A','R',' ')},
	{".uu", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".uue", FOURCC('T','E','X','T'), FOURCC('S','I','T','x')},
	{".zip", FOURCC('Z','I','P',' '), FOURCC('Z','I','P',' ')},
	{".8svx", FOURCC('8','S','V','X'), FOURCC('S','N','D','M')},
	{".aifc", FOURCC('A','I','F','C'), FOURCC('T','V','O','D')},
	{".aiff", FOURCC('A','I','F','F'), FOURCC('T','V','O','D')},
	{".au", FOURCC('U','L','A','W'), FOURCC('T','V','O','D')},
	{".mid", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".midi", FOURCC('M','I','D','I'), FOURCC('T','V','O','D')},
	{".mp2", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".mp3", FOURCC('M','P','G',' '), FOURCC('T','V','O','D')},
	{".wav", FOURCC('W','A','V','E'), FOURCC('T','V','O','D')},
	{".bmp", FOURCC('B','M','P','f'), FOURCC('o','g','l','e')},
	{".gif", FOURCC('G','I','F','f'), FOURCC('o','g','l','e')},
	{".lbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".ilbm", FOURCC('I','L','B','M'), FOURCC('G','K','O','N')},
	{".jpg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".jpeg", FOURCC('J','P','E','G'), FOURCC('o','g','l','e')},
	{".pict", FOURCC('P','I','C','T'), FOURCC('o','g','l','e')},
	{".png", FOURCC('P','N','G','f'), FOURCC('o','g','l','e')},
	{".sgi", FOURCC('.','S','G','I'), FOURCC('o','g','l','e')},
	{".tga", FOURCC('T','P','I','C'), FOURCC('o','g','l','e')},
	{".tif", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".tiff", FOURCC('T','I','F','F'), FOURCC('o','g','l','e')},
	{".htm", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".html", FOURCC('T','E','X','T'), FOURCC('M','O','S','S')},
	{".txt", FOURCC('T','E','X','T'), FOURCC('t','t','x','t')},
	{".rtf", FOURCC('T','E','X','T'), FOURCC('M','S','W','D')},
	{".c", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".C", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cc", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".cxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".h", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hh", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hpp", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".hxx", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".s", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".S", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".i", FOURCC('T','E','X','T'), FOURCC('R','*','c','h')},
	{".mpg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mpeg", FOURCC('M','P','E','G'), FOURCC('T','V','O','D')},
	{".mov", FOURCC('M','o','o','V'), FOURCC('T','V','O','D')},
	{".fli", FOURCC('F','L','I',' '), FOURCC('T','V','O','D')},
	{".avi", FOURCC('V','f','W',' '), FOURCC('T','V','O','D')},
	{".qxd", FOURCC('X','D','O','C'), FOURCC('X','P','R','3')},
	{".hfv", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".dsk", FOURCC('D','D','i','m'), FOURCC('d','d','s','k')},
	{".img", FOURCC('r','o','h','d'), FOURCC('d','d','s','k')},
	{NULL, 0, 0}	// End marker
};

// Get emulated Finder info from extended attributes
static bool get_finfo_from_xattr(const char *path, uint8 *finfo, uint8 *fxinfo)
{
	if (!get_xattr(path, XATTR_FINFO, finfo, SIZEOF_FInfo))
		return false;
	if (fxinfo) {
		if (!get_xattr(path, XATTR_FXINFO, fxinfo, SIZEOF_FXInfo))
			memset(fxinfo, 0, SIZEOF_FXInfo);
	}
	return true;
}

// Get emulated Finder info from helper file
static bool get_finfo_from_helper(const char *path, uint8 *finfo, uint8 *fxinfo)
{
	int fd = open_finf(path, O_RDONLY);
	if (fd < 0)
		return false;

	ssize_t actual = read(fd, finfo, SIZEOF_FInfo);
	if (actual != SIZEOF_FInfo) {
		close(fd);
		return false;
	}
	if (fxinfo) {
		ssize_t fx_actual = read(fd, fxinfo, SIZEOF_FXInfo);
		if (fx_actual != SIZEOF_FXInfo)
			memset(fxinfo, 0, SIZEOF_FXInfo);
	}
	close(fd);
	return true;
}

// Get native Finder info
static bool get_finfo_from_native(const char *path, uint8 *finfo, uint8 *fxinfo)
{
	struct attrlist attrList;
	memset(&attrList, 0, sizeof(attrList));
	attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrList.commonattr  = ATTR_CMN_FNDRINFO;

	FinderInfoAttrBuf attrBuf;
	if (getattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), 0) < 0)
		return false;

	memcpy(finfo, attrBuf.finderInfo, SIZEOF_FInfo);
	if (fxinfo)
		memcpy(fxinfo, attrBuf.extendedFinderInfo, SIZEOF_FXInfo);
	return true;
}

void get_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	// Initialize clean defaults: DInfo/DXInfo must be all zeros for folders (never set kHasBeenInited or kIsAlias on folders!)
	Mac_memset(finfo, 0, SIZEOF_FInfo);
	if (fxinfo)
		Mac_memset(fxinfo, 0, SIZEOF_FXInfo);
	if (!is_dir) {
		WriteMacInt16(finfo + fdFlags, DEFAULT_FINDER_FLAGS);
		WriteMacInt32(finfo + fdLocation, (uint32)-1);
	}

	// Prefer explicit Mac OS Finder info stored by BasiliskII/SheepShaver in xattrs or .finf helper
	FinderInfo emu_finfo, emu_fxinfo;
	memset(emu_finfo, 0, SIZEOF_FInfo);
	memset(emu_fxinfo, 0, SIZEOF_FXInfo);
	if (get_finfo_from_xattr(path, emu_finfo, fxinfo ? emu_fxinfo : NULL) ||
		get_finfo_from_helper(path, emu_finfo, fxinfo ? emu_fxinfo : NULL)) {
		Host2Mac_memcpy(finfo, emu_finfo, SIZEOF_FInfo);
		if (fxinfo)
			Host2Mac_memcpy(fxinfo, emu_fxinfo, SIZEOF_FXInfo);
		if (is_dir) {
			uint16 flags = ReadMacInt16(finfo + frFlags) & kFolderValidFlagsMask;
			WriteMacInt16(finfo + frFlags, flags);
		} else {
			if ((ReadMacInt16(finfo + fdFlags) & kIsAlias) && get_rfork_size(path) == 0)
				WriteMacInt16(finfo + fdFlags, ReadMacInt16(finfo + fdFlags) & ~kIsAlias);
			if (ReadMacInt32(finfo + fdType) != 0 && ReadMacInt32(finfo + fdCreator) != 0)
				return;
		}
		if (is_dir)
			return;
	} else if (is_dir) {
		// Folders without explicit Mac OS DInfo keep clean all-zero DInfo/DXInfo
		return;
	} else {
		// Regular file without explicit Mac OS FInfo: inspect native HFS+/APFS FinderInfo if non-empty
		FinderInfo nat_finfo;
		if (get_finfo_from_native(path, nat_finfo, NULL)) {
			bool any_native = false;
			for (int i = 0; i < 10; i++) {
				if (nat_finfo[i] != 0) {
					any_native = true;
					break;
				}
			}
			if (any_native) {
				Host2Mac_memcpy(finfo + fdType, nat_finfo + fdType, 8);
				uint16 nat_flags = ((uint16)nat_finfo[fdFlags] << 8) | nat_finfo[fdFlags + 1];
				// Strip native macOS data-fork Bookmark/Alias flag if file has no Mac OS 9 resource fork
				if ((nat_flags & kIsAlias) && get_rfork_size(path) == 0)
					nat_flags &= ~kIsAlias;
				WriteMacInt16(finfo + fdFlags, nat_flags | DEFAULT_FINDER_FLAGS);
				if (ReadMacInt32(finfo + fdType) != 0 && ReadMacInt32(finfo + fdCreator) != 0)
					return;
			}
		}
	}

	// Fallback: translate file name extension (case-insensitively) to MacOS type/creator
	if (!is_dir && ReadMacInt32(finfo + fdType) == 0 && ReadMacInt32(finfo + fdCreator) == 0) {
		size_t path_len = strlen(path);
		for (int i = 0; e2t_translation[i].ext; i++) {
			size_t ext_len = strlen(e2t_translation[i].ext);
			if (path_len < ext_len)
				continue;
			if (!strcasecmp(path + path_len - ext_len, e2t_translation[i].ext)) {
				WriteMacInt32(finfo + fdType, e2t_translation[i].type);
				WriteMacInt32(finfo + fdCreator, e2t_translation[i].creator);
				break;
			}
		}
	}
}

// Set emulated Finder info into extended attributes
static bool set_finfo_to_xattr(const char *path, const uint8 *finfo, const uint8 *fxinfo)
{
	if (!set_xattr(path, XATTR_FINFO, finfo, SIZEOF_FInfo))
		return false;
	if (fxinfo && !set_xattr(path, XATTR_FXINFO, fxinfo, SIZEOF_FXInfo))
		return false;
	return true;
}

// Set emulated Finder info into helper file
static bool set_finfo_to_helper(const char *path, const uint8 *finfo, const uint8 *fxinfo)
{
	int fd = open_finf(path, O_RDWR);
	if (fd < 0)
		return false;

	ssize_t actual = write(fd, finfo, SIZEOF_FInfo);
	if (fxinfo)
		actual += write(fd, fxinfo, SIZEOF_FXInfo);
	close(fd);
	return actual == (SIZEOF_FInfo + (fxinfo ? SIZEOF_FXInfo : 0));
}

// Mirror compatible Finder info fields into native FinderInfo without clobbering APFS extended_flags
static bool set_finfo_to_native(const char *path, const uint8 *finfo, bool is_dir)
{
	struct attrlist attrList;
	memset(&attrList, 0, sizeof(attrList));
	attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrList.commonattr  = ATTR_CMN_FNDRINFO;

	FinderInfoAttrBuf attrBuf;
	if (getattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), 0) < 0)
		return false;

	finfo_merge(attrBuf.finderInfo, attrBuf.finderInfo, finfo, is_dir ? kNativeDInfoMask : kNativeFInfoMask);

	attrList.commonattr = ATTR_CMN_FNDRINFO;
	if (setattrlist(path, &attrList, attrBuf.finderInfo, 2 * SIZEOF_FInfo, 0) < 0)
		return false;
	return true;
}

void set_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	FinderInfo clean_finfo;
	Mac2Host_memcpy(clean_finfo, finfo, SIZEOF_FInfo);
	if (is_dir) {
		uint16 flags = (((uint16)clean_finfo[frFlags] << 8) | clean_finfo[frFlags + 1]) & kFolderValidFlagsMask;
		clean_finfo[frFlags] = (uint8)(flags >> 8);
		clean_finfo[frFlags + 1] = (uint8)(flags & 0xff);
	}
	const uint8 *emu_fxinfo = fxinfo ? Mac2HostAddr(fxinfo) : NULL;

	// Store full Mac OS Finder info in xattr, falling back to .finf helper only if xattrs are unsupported
	if (!set_finfo_to_xattr(path, clean_finfo, emu_fxinfo)) {
		if (errno == ENOTSUP || errno == EOPNOTSUPP)
			set_finfo_to_helper(path, clean_finfo, emu_fxinfo);
	}

	// Mirror compatible fields to native FinderInfo
	FinderInfo nat_finfo;
	finfo_split(nat_finfo, clean_finfo, is_dir ? kNativeDInfoMask : kNativeFInfoMask);
	set_finfo_to_native(path, nat_finfo, is_dir);
}


/*
 *  Resource fork emulation functions
 */

uint32 get_rfork_size(const char *path)
{
	// Open resource file
	int fd = open_rsrc(path, O_RDONLY);
	if (fd < 0)
		return 0;

	// Get size
	off_t size = lseek(fd, 0, SEEK_END);

	// Close file and return size (clamped to positive SInt32)
	close(fd);
	if (size <= 0)
		return 0;
	return (size > (off_t)0x7fffffff) ? 0x7fffffffU : (uint32)size;
}

int open_rfork(const char *path, int flag)
{
	return open_rsrc(path, flag);
}

void close_rfork(const char *path, int fd)
{
	(void)path;
	if (fd >= 0)
		close(fd);
}


/*
 *  Read "length" bytes from file to "buffer",
 *  returns number of bytes read (or -1 on error)
 */

ssize_t extfs_read(int fd, void *buffer, size_t length)
{
	return read(fd, buffer, length);
}


/*
 *  Write "length" bytes from "buffer" to file,
 *  returns number of bytes written (or -1 on error)
 */

ssize_t extfs_write(int fd, void *buffer, size_t length)
{
	return write(fd, buffer, length);
}


/*
 *  Remove file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_remove(const char *path)
{
	char helper_path[MAX_PATH_LENGTH];

	// Remove file or directory first so failed removals never destroy metadata
	if (remove(path) < 0) {
		if (errno == EISDIR || errno == ENOTEMPTY) {
			strncpy(helper_path, path, MAX_PATH_LENGTH - 1);
			helper_path[MAX_PATH_LENGTH - 1] = 0;
			add_path_component(helper_path, ".finf");
			rmdir(helper_path);
			if (rmdir(path) < 0)
				return false;
		} else {
			return false;
		}
	}

	// Remove legacy .finf helper if present
	make_finf_path(path, helper_path, false);
	remove(helper_path);
	return true;
}


/*
 *  Rename/move file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	// Rename file/directory first (moves native resource fork and xattrs atomically)
	if (rename(old_path, new_path) < 0)
		return false;

	// Migrate legacy .finf helper if one exists for old_path
	char old_helper_path[MAX_PATH_LENGTH], new_helper_path[MAX_PATH_LENGTH];
	make_finf_path(old_path, old_helper_path, false);
	if (access(old_helper_path, F_OK) == 0) {
		make_finf_path(new_path, new_helper_path, false);
		create_finf_dir(new_path);
		rename(old_helper_path, new_helper_path);
	}
	return true;
}


/*
 *  Strings (filenames) conversion
 */

// Convert string in the specified source and target encodings, substituting '?' for unmappable characters
const char *convert_string(const char *str, CFStringEncoding from, CFStringEncoding to)
{
	if (str == NULL)
		return "";
	const char *ostr = str;
	CFStringRef cfstr = CFStringCreateWithCString(NULL, str, from);
	if (cfstr) {
		static char buffer[MAX_PATH_LENGTH];
		memset(buffer, 0, sizeof(buffer));
		if (CFStringGetCString(cfstr, buffer, sizeof(buffer), to)) {
			ostr = buffer;
		} else {
			CFIndex used = 0;
			CFIndex total = CFStringGetLength(cfstr);
			if (CFStringGetBytes(cfstr, CFRangeMake(0, total), to, '?', false, (UInt8 *)buffer, sizeof(buffer) - 1, &used) > 0 && used > 0) {
				buffer[used] = 0;
				ostr = buffer;
			}
		}
		CFRelease(cfstr);
	}
	return ostr;
}

// Convert from the host OS filename encoding to MacRoman
const char *host_encoding_to_macroman(const char *filename)
{
	return convert_string(filename, kCFStringEncodingUTF8, kCFStringEncodingMacRoman);
}

// Convert from MacRoman to host OS filename encoding
const char *macroman_to_host_encoding(const char *filename)
{
	return convert_string(filename, kCFStringEncodingMacRoman, kCFStringEncodingUTF8);
}
