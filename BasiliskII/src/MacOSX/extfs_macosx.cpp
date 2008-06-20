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
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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


// Default Finder flags
const uint16 DEFAULT_FINDER_FLAGS = kHasBeenInited;


/*
 *  Extended attributes (Tiger+)
 */

#define USE_XATTRS g_use_xattrs
static bool g_use_xattrs = false;

#define XATTR_TEST   "org.BasiliskII.TestAttr"
#define XATTR_FINFO  "org.BasiliskII.FinderInfo"
#define XATTR_FXINFO "org.BasiliskII.ExtendedFinderInfo"

static bool get_xattr(const char *path, const char *name, void *value, uint32 size)
{
	return syscall(SYS_getxattr, path, name, value, size, 0, 0) == size;
}

static bool set_xattr(const char *path, const char *name, const void *value, uint32 size)
{
	return syscall(SYS_setxattr, path, name, value, size, 0, 0) == 0;
}

static bool remove_xattr(const char *path, const char *name)
{
	return syscall(SYS_removexattr, path, name, 0) == 0;
}

static bool check_xattr(void)
{
	const char *path = PrefsFindString("extfs");
	if (path == NULL)
		return false;
	const uint32 sentinel = 0xdeadbeef;
	if (!set_xattr(path, XATTR_TEST, &sentinel, sizeof(sentinel)))
		return false;
	uint32 v;
	if (!get_xattr(path, XATTR_TEST, &v, sizeof(v)))
		return false;
	if (!remove_xattr(path, XATTR_TEST))
		return false;
	return v == sentinel;
}


/*
 *  Initialization
 */

void extfs_init(void)
{
	g_use_xattrs = check_xattr();
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
	int l = strlen(path);
	if (l < MAX_PATH_LENGTH-1 && path[l-1] != '/') {
		path[l] = '/';
		path[l+1] = 0;
	}
	strncat(path, component, MAX_PATH_LENGTH-1);
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
static const FinderInfo kNativeFXInfoMask = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00};
static const FinderInfo kNativeDInfoMask  = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00};
static const FinderInfo kNativeDXInfoMask = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00}; /* XXX: keep frScroll? */

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
 *  Finder info are kept in helper files (on anything below Tiger)
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
	strncpy(dest, src, last_part-src);
	dest[last_part-src] = 0;

	// Add additional component
	strncat(dest, ".finf/", MAX_PATH_LENGTH-1);

	// Add last component
	if (!only_dir)
		strncat(dest, last_part, MAX_PATH_LENGTH-1);
}

static int create_finf_dir(const char *path)
{
	char finf_dir[MAX_PATH_LENGTH];
	make_finf_path(path, finf_dir, true);
	if (finf_dir[strlen(finf_dir) - 1] == '/')	// Remove trailing "/"
		finf_dir[strlen(finf_dir) - 1] = 0;
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
	int l = strlen(src);
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

	return open(rsrc_path, flag);
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

// Get emulated Finder info from metadata (Tiger+)
static bool get_finfo_from_xattr(const char *path, uint8 *finfo, uint8 *fxinfo)
{
	if (!get_xattr(path, XATTR_FINFO, finfo, SIZEOF_FInfo))
		return false;
	if (fxinfo && !get_xattr(path, XATTR_FXINFO, fxinfo, SIZEOF_FXInfo))
		return false;
	return true;
}

// Get emulated Finder info from helper file
static bool get_finfo_from_helper(const char *path, uint8 *finfo, uint8 *fxinfo)
{
	int fd = open_finf(path, O_RDONLY);
	if (fd < 0)
		return false;

	ssize_t actual = read(fd, finfo, SIZEOF_FInfo);
	if (fxinfo)
		actual += read(fd, fxinfo, SIZEOF_FXInfo);
	close(fd);
	return actual == (SIZEOF_FInfo + (fxinfo ? SIZEOF_FXInfo : 0));
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

static bool do_get_finfo(const char *path, bool has_fxinfo,
						 FinderInfo emu_finfo, FinderInfo emu_fxinfo,
						 FinderInfo nat_finfo, FinderInfo nat_fxinfo)
{
	memset(emu_finfo, 0, SIZEOF_FInfo);
	if (has_fxinfo)
		memset(emu_fxinfo, 0, SIZEOF_FXInfo);
	*((uint16 *)(emu_finfo + fdFlags)) = htonl(DEFAULT_FINDER_FLAGS);
	*((uint32 *)(emu_finfo + fdLocation)) = htonl((uint32)-1);

	if (USE_XATTRS)
		get_finfo_from_xattr(path, emu_finfo, has_fxinfo ? emu_fxinfo : NULL);
	else
		get_finfo_from_helper(path, emu_finfo, has_fxinfo ? emu_fxinfo : NULL);

	if (!get_finfo_from_native(path, nat_finfo, has_fxinfo ? nat_fxinfo : NULL))
		return false;
	return true;
}

void get_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	// Set default finder info
	Mac_memset(finfo, 0, SIZEOF_FInfo);
	if (fxinfo)
		Mac_memset(fxinfo, 0, SIZEOF_FXInfo);
	WriteMacInt16(finfo + fdFlags, DEFAULT_FINDER_FLAGS);
	WriteMacInt32(finfo + fdLocation, (uint32)-1);

	// Merge emulated and native Finder info
	FinderInfo emu_finfo, emu_fxinfo;
	FinderInfo nat_finfo, nat_fxinfo;
	if (do_get_finfo(path, fxinfo, emu_finfo, emu_fxinfo, nat_finfo, nat_fxinfo)) {
		if (!is_dir) {
			finfo_merge(Mac2HostAddr(finfo), emu_finfo, nat_finfo, kNativeFInfoMask);
			if (fxinfo)
				finfo_merge(Mac2HostAddr(fxinfo), emu_fxinfo, nat_fxinfo, kNativeFXInfoMask);
			if (ReadMacInt32(finfo + fdType) != 0 && ReadMacInt32(finfo + fdCreator) != 0)
				return;
		}
		else {
			finfo_merge(Mac2HostAddr(finfo), emu_finfo, nat_finfo, kNativeDInfoMask);
			if (fxinfo)
				finfo_merge(Mac2HostAddr(fxinfo), emu_fxinfo, nat_fxinfo, kNativeDXInfoMask);
			return;
		}
	}

	// No native Finder info, translate file name extension to MacOS type/creator
	if (!is_dir) {
		int path_len = strlen(path);
		for (int i=0; e2t_translation[i].ext; i++) {
			int ext_len = strlen(e2t_translation[i].ext);
			if (path_len < ext_len)
				continue;
			if (!strcmp(path + path_len - ext_len, e2t_translation[i].ext)) {
				WriteMacInt32(finfo + fdType, e2t_translation[i].type);
				WriteMacInt32(finfo + fdCreator, e2t_translation[i].creator);
				break;
			}
		}
	}
}

// Set emulated Finder info into metada (Tiger+)
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

// Set native Finder info
static bool set_finfo_to_native(const char *path, const uint8 *finfo, const uint8 *fxinfo, bool is_dir)
{
	struct attrlist attrList;
	memset(&attrList, 0, sizeof(attrList));
	attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrList.commonattr  = ATTR_CMN_FNDRINFO;

	FinderInfoAttrBuf attrBuf;
	if (getattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), 0) < 0)
		return false;

	finfo_merge(attrBuf.finderInfo, attrBuf.finderInfo, finfo, is_dir ? kNativeDInfoMask : kNativeFInfoMask);
	if (fxinfo)
		finfo_merge(attrBuf.extendedFinderInfo, attrBuf.extendedFinderInfo, fxinfo, is_dir ? kNativeDXInfoMask : kNativeFXInfoMask);

	attrList.commonattr = ATTR_CMN_FNDRINFO;
	if (setattrlist(path, &attrList, attrBuf.finderInfo, 2 * SIZEOF_FInfo, 0) < 0)
		return false;
	return true;
}

void set_finfo(const char *path, uint32 finfo, uint32 fxinfo, bool is_dir)
{
	// Extract native Finder info flags
	FinderInfo nat_finfo, nat_fxinfo;
	const uint8 *emu_finfo = Mac2HostAddr(finfo);
	const uint8 *emu_fxinfo = fxinfo ? Mac2HostAddr(fxinfo) : NULL;
	finfo_split(nat_finfo, emu_finfo, is_dir ? kNativeDInfoMask : kNativeFInfoMask);
	if (fxinfo)
		finfo_split(nat_fxinfo, emu_fxinfo, is_dir ? kNativeDXInfoMask : kNativeFXInfoMask);

	// Update Finder info file (all flags)
	if (USE_XATTRS)
		set_finfo_to_xattr(path, emu_finfo, emu_fxinfo);
	else
		set_finfo_to_helper(path, emu_finfo, emu_fxinfo);

	// Update native Finder info flags
	set_finfo_to_native(path, nat_finfo, nat_fxinfo, is_dir);
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
	
	// Close file and return size
	close(fd);
	return size < 0 ? 0 : size;
}

int open_rfork(const char *path, int flag)
{
	return open_rsrc(path, flag);
}

void close_rfork(const char *path, int fd)
{
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
	// Remove helpers first, don't complain if this fails
	char helper_path[MAX_PATH_LENGTH];
	make_finf_path(path, helper_path, false);
	remove(helper_path);
	make_rsrc_path(path, helper_path);
	remove(helper_path);

	// Now remove file or directory (and helper directories in the directory)
	if (remove(path) < 0) {
		if (errno == EISDIR || errno == ENOTEMPTY) {
			helper_path[0] = 0;
			strncpy(helper_path, path, MAX_PATH_LENGTH-1);
			add_path_component(helper_path, ".finf");
			rmdir(helper_path);
			return rmdir(path) == 0;
		} else
			return false;
	}
	return true;
}


/*
 *  Rename/move file/directory (and associated helper files),
 *  returns false on error (and sets errno)
 */

bool extfs_rename(const char *old_path, const char *new_path)
{
	// Rename helpers first, don't complain if this fails
	char old_helper_path[MAX_PATH_LENGTH], new_helper_path[MAX_PATH_LENGTH];
	make_finf_path(old_path, old_helper_path, false);
	make_finf_path(new_path, new_helper_path, false);
	create_finf_dir(new_path);
	rename(old_helper_path, new_helper_path);
	make_rsrc_path(old_path, old_helper_path);
	make_rsrc_path(new_path, new_helper_path);
	rename(old_helper_path, new_helper_path);

	// Now rename file
	return rename(old_path, new_path) == 0;
}


/*
 *  Strings (filenames) conversion
 */

// Convert string in the specified source and target encodings
const char *convert_string(const char *str, CFStringEncoding from, CFStringEncoding to)
{
	const char *ostr = str;
	CFStringRef cfstr = CFStringCreateWithCString(NULL, str, from);
	if (cfstr) {
		static char buffer[MAX_PATH_LENGTH];
		memset(buffer, 0, sizeof(buffer));
		if (CFStringGetCString(cfstr, buffer, sizeof(buffer), to))
			ostr = buffer;
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
