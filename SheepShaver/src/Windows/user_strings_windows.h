/*
 *  user_strings_windows.h - Windows-specific localizable strings
 *
 *  SheepShaver (C) 1997-2004 Christian Bauer and Marc Hellwig
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

#ifndef USER_STRINGS_WINDOWS_H
#define USER_STRINGS_WINDOWS_H

enum {
	STR_LOW_MEM_MMAP_ERR = 10000,
	STR_KD_SHMGET_ERR,
	STR_KD_SHMAT_ERR,
	STR_KD2_SHMAT_ERR,
	STR_ROM_MMAP_ERR,
	STR_RAM_MMAP_ERR,
	STR_DR_CACHE_MMAP_ERR,
	STR_DR_EMULATOR_MMAP_ERR,
	STR_SHEEP_MEM_MMAP_ERR,
	STR_SIGSEGV_INSTALL_ERR,
	STR_NO_XVISUAL_ERR,
	STR_VOSF_INIT_ERR,
	STR_NO_AUDIO_WARN,
	STR_KEYCODE_FILE_WARN,
	STR_KEYCODE_VENDOR_WARN,
	STR_OPEN_WINDOW_ERR,
	STR_WINDOW_TITLE_GRABBED
};

#endif
