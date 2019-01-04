/*
 *  posix_emu.h
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
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

#include <fcntl.h>
#include <errno.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>

#include "extfs.h"

void init_posix_emu(void);
void final_posix_emu(void);

typedef struct dirent {
	char d_name[MAX_PATH_LENGTH];
} dirent;

typedef struct DIR {
	HANDLE h;
	WIN32_FIND_DATA FindFileData;
	dirent de;
	TCHAR *vname_list;
} DIR;

// emulated
DIR *opendir( const char *path );
void closedir( DIR *d );
struct dirent *readdir( DIR *d );

// access() mode: exists?
#ifndef F_OK
#define F_OK 0
#endif
// access() mode: can do r/w?
#ifndef W_OK
#define W_OK 6
#endif

// hook stat functions to create virtual desktop
// because of errno all used funcs must be hooked.
int my_stat( const char *, struct my_stat * );
int my_fstat( int, struct my_stat * );
int my_open( const char *, int, ... );
int my_rename( const char *, const char * );
int my_access( const char *, int );
int my_mkdir( const char *path, int mode );
int my_remove( const char * );
int my_creat( const char *path, int mode );
int my_creat( const char *path, int mode );
int my_close( int fd );
long my_lseek( int fd, long, int);
int my_read( int fd, void *, unsigned int);
int my_write( int fd, const void *, unsigned int);
int my_chsize( int fd, unsigned int size );
int my_locking( int fd, int mode, long nbytes );
int my_utime( const char *path, struct my_utimbuf * );

extern int my_errno;

// must hook all other functions that manipulate file names
#ifndef NO_POSIX_API_HOOK
#define stat my_stat
#define fstat my_fstat
#define open my_open
#define rename my_rename
#define access my_access
#define mkdir my_mkdir
#define remove my_remove
#define creat my_creat
#define close my_close
#define lseek my_lseek
#define read my_read
#define write my_write
#define ftruncate my_chsize
#define locking my_locking
#define utime my_utime

#undef errno
#define errno my_errno
#endif //!NO_POSIX_API_HOOK

#ifndef S_ISDIR
#define S_ISDIR(stat_mode) (((stat_mode) & _S_IFDIR) != 0)
#endif

// can't #define "stat" unless there's a replacement for "struct stat"
struct my_stat {
  _dev_t st_dev;
  _ino_t st_ino;
  unsigned short st_mode;
  short st_nlink;
  short st_uid;
  short st_gid;
  _dev_t st_rdev;
  _off_t st_size;
  time_t st_atime;
  time_t st_mtime;
  time_t st_ctime;
};

struct my_utimbuf
{
	time_t actime;      // access time
	time_t modtime;     // modification time
};

// Your compiler may have different "struct stat" -> edit "struct my_stat"
#define validate_stat_struct ( sizeof(struct my_stat) == sizeof(struct stat) )

#define st_crtime st_ctime
