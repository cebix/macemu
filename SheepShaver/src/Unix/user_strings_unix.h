/*
 *  user_strings_unix.h - Unix-specific localizable strings
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

#ifndef USER_STRINGS_LINUX_H
#define USER_STRINGS_LINUX_H

enum {
	STR_NO_DEV_ZERO_ERR = 10000,
	STR_LOW_MEM_MMAP_ERR,
	STR_KD_SHMGET_ERR,
	STR_KD_SHMAT_ERR,
	STR_KD2_SHMAT_ERR,
	STR_ROM_MMAP_ERR,
	STR_RAM_MMAP_ERR,
	STR_DR_CACHE_MMAP_ERR,
	STR_DR_EMULATOR_MMAP_ERR,
	STR_SHEEP_MEM_MMAP_ERR,
	STR_SIGALTSTACK_ERR,
	STR_SIG_INSTALL_ERR,
	STR_NO_XSERVER_ERR,
	STR_NO_XVISUAL_ERR,
	STR_UNSUPP_DEPTH_ERR,
	STR_VOSF_INIT_ERR,

	STR_PROC_CPUINFO_WARN,
	STR_BLOCKING_NET_SOCKET_WARN,
	STR_NO_SHEEP_NET_DRIVER_WARN,
	STR_SHEEP_NET_ATTACH_WARN,
	STR_TUN_TAP_CONFIG_WARN,
	STR_SLIRP_NO_DNS_FOUND_WARN,
	STR_NO_AUDIO_DEV_WARN,
	STR_NO_AUDIO_WARN,
	STR_NO_ESD_WARN,
	STR_AUDIO_FORMAT_WARN,
	STR_SCSI_DEVICE_OPEN_WARN,
	STR_SCSI_DEVICE_NOT_SCSI_WARN,
	STR_KEYCODE_FILE_WARN,
	STR_KEYCODE_VENDOR_WARN,

	STR_PREFS_MENU_FILE_GTK,
	STR_PREFS_ITEM_START_GTK,
	STR_PREFS_ITEM_ZAP_PRAM_GTK,
	STR_PREFS_ITEM_SEPL_GTK,
	STR_PREFS_ITEM_QUIT_GTK,
	STR_HELP_MENU_GTK,
	STR_HELP_ITEM_ABOUT_GTK,
	STR_SUSPEND_WINDOW_TITLE,
	STR_DSPDEVICE_FILE_CTRL,
	STR_MIXERDEVICE_FILE_CTRL,

	STR_BROWSE_CTRL,
	STR_BROWSE_TITLE,
	STR_INPUT_PANE_TITLE,
	STR_KEYCODES_CTRL,
	STR_KEYCODE_FILE_CTRL,
	STR_MOUSEWHEELMODE_CTRL,
	STR_MOUSEWHEELMODE_PAGE_LAB,
	STR_MOUSEWHEELMODE_CURSOR_LAB,
	STR_MOUSEWHEELLINES_CTRL,

	STR_OPEN_WINDOW_ERR,
	STR_WINDOW_TITLE_GRABBED,

	STR_NO_B2_EXE_FOUND
};

#endif
