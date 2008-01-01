/*
 *  user_strings_windows.h - Windows-specific localizable strings
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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
	STR_SLIRP_NO_DNS_FOUND_WARN,
	STR_NO_AUDIO_WARN,
	STR_KEYCODE_FILE_WARN,
	STR_KEYCODE_VENDOR_WARN,
	STR_OPEN_WINDOW_ERR,
	STR_WINDOW_TITLE_GRABBED,
	STR_NO_WIN32_NT_4,

	STR_PREFS_MENU_FILE_GTK,
	STR_PREFS_ITEM_START_GTK,
	STR_PREFS_ITEM_ZAP_PRAM_GTK,
	STR_PREFS_ITEM_SEPL_GTK,
	STR_PREFS_ITEM_QUIT_GTK,
	STR_HELP_MENU_GTK,
	STR_HELP_ITEM_ABOUT_GTK,

	STR_FILE_CTRL,
	STR_BROWSE_CTRL,
	STR_BROWSE_TITLE,
	STR_SERIAL_PANE_TITLE,
	STR_NETWORK_PANE_TITLE,
	STR_INPUT_PANE_TITLE,
	STR_KEYCODES_CTRL,
	STR_KEYCODE_FILE_CTRL,
	STR_MOUSEWHEELMODE_CTRL,
	STR_MOUSEWHEELMODE_PAGE_LAB,
	STR_MOUSEWHEELMODE_CURSOR_LAB,
	STR_MOUSEWHEELLINES_CTRL,
	STR_POLLMEDIA_CTRL,
	STR_EXTFS_ENABLE_CTRL,
	STR_EXTFS_DRIVES_CTRL,
	STR_ETHER_FTP_PORT_LIST_CTRL,
	STR_ETHER_TCP_PORT_LIST_CTRL,
};

#endif
