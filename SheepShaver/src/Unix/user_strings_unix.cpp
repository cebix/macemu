/*
 *  user_strings_unix.cpp - Localizable strings, Unix specific strings
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

#include "sysdeps.h"
#include "user_strings.h"


// Platform-specific string definitions
user_string_def platform_strings[] = {
	// Common strings that have a platform-specific variant
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted under Linux. Basilisk II will try to unmount it."},
	{STR_EXTFS_CTRL, "Unix Root"},
	{STR_EXTFS_NAME, "Unix Directory Tree"},
	{STR_EXTFS_VOLUME_NAME, "Unix"},

	// Purely platform-specific strings
	{STR_NO_DEV_ZERO_ERR, "Cannot open /dev/zero: %s."},
	{STR_LOW_MEM_MMAP_ERR, "Cannot map Low Memory Globals: %s."},
	{STR_KD_SHMGET_ERR, "Cannot create SHM segment for Kernel Data: %s."},
	{STR_KD_SHMAT_ERR, "Cannot map first Kernel Data area: %s."},
	{STR_KD2_SHMAT_ERR, "Cannot map second Kernel Data area: %s."},
	{STR_ROM_MMAP_ERR, "Cannot map ROM: %s."},
	{STR_RAM_MMAP_ERR, "Cannot map RAM: %s."},
	{STR_DR_CACHE_MMAP_ERR, "Cannot map DR Cache: %s."},
	{STR_DR_EMULATOR_MMAP_ERR, "Cannot map DR Emulator: %s."},
	{STR_SHEEP_MEM_MMAP_ERR, "Cannot map SheepShaver Data area: %s."},
	{STR_SIGALTSTACK_ERR, "Cannot install alternate signal stack (%s). It seems that you need a newer kernel."},
	{STR_SIG_INSTALL_ERR, "Cannot install %s handler (%s)."},
	{STR_NO_XSERVER_ERR, "Cannot connect to X server %s."},
	{STR_NO_XVISUAL_ERR, "Cannot obtain appropriate X visual."},
	{STR_UNSUPP_DEPTH_ERR, "Unsupported color depth of screen."},
	{STR_PROC_CPUINFO_WARN, "Cannot open /proc/cpuinfo (%s). Assuming 100MHz PowerPC 604."},
	{STR_BLOCKING_NET_SOCKET_WARN, "Cannot set non-blocking I/O to net socket (%s). Ethernet will not be available."},
	{STR_NO_SHEEP_NET_DRIVER_WARN, "Cannot open %s (%s). Ethernet will not be available."},
	{STR_SHEEP_NET_ATTACH_WARN, "Cannot attach to Ethernet card (%s). Ethernet will not be available."},
	{STR_TUN_TAP_CONFIG_WARN, "Cannot configure TUN/TAP device (%s). Ethernet will not be available."},
	{STR_SLIRP_NO_DNS_FOUND_WARN, "Cannot get DNS address. Ethernet will not be available."},
	{STR_NO_AUDIO_DEV_WARN, "Cannot open %s (%s). Audio output will be disabled."},
	{STR_NO_AUDIO_WARN, "No audio device found, audio output will be disabled."},
	{STR_NO_ESD_WARN, "Cannot open ESD connection. Audio output will be disabled."},
	{STR_AUDIO_FORMAT_WARN, "/dev/dsp doesn't support signed 16 bit format. Audio output will be disabled."},
	{STR_SCSI_DEVICE_OPEN_WARN, "Cannot open %s (%s). SCSI Manager access to this device will be disabled."},
	{STR_SCSI_DEVICE_NOT_SCSI_WARN, "%s doesn't seem to comply to the Generic SCSI API. SCSI Manager access to this device will be disabled."},
	{STR_KEYCODE_FILE_WARN, "Cannot open keycode translation file %s (%s)."},
	{STR_KEYCODE_VENDOR_WARN, "Cannot find vendor '%s' in keycode translation file %s."},
	{STR_PREFS_MENU_FILE_GTK, "/_File"},
	{STR_PREFS_ITEM_START_GTK, "/File/_Start SheepShaver"},
	{STR_PREFS_ITEM_ZAP_PRAM_GTK, "/File/_Zap PRAM File"},
	{STR_PREFS_ITEM_SEPL_GTK, "/File/sepl"},
	{STR_PREFS_ITEM_QUIT_GTK, "/File/_Quit SheepShaver"},
	{STR_HELP_MENU_GTK, "/_Help"},
	{STR_HELP_ITEM_ABOUT_GTK, "/Help/_About SheepShaver"},
	{STR_DSPDEVICE_FILE_CTRL, "Audio Output Device"},
	{STR_MIXERDEVICE_FILE_CTRL, "Audio Mixer Device"},
	{STR_BROWSE_TITLE, "Browse file"},
	{STR_BROWSE_CTRL, "Browse..."},
	{STR_INPUT_PANE_TITLE, "Keyboard/Mouse"},
	{STR_KEYCODES_CTRL, "Use Raw Keycodes"},
	{STR_KEYCODE_FILE_CTRL, "Keycode Translation File"},
	{STR_MOUSEWHEELMODE_CTRL, "Mouse Wheel Function"},
	{STR_MOUSEWHEELMODE_PAGE_LAB, "Page Up/Down"},
	{STR_MOUSEWHEELMODE_CURSOR_LAB, "Cursor Up/Down"},
	{STR_MOUSEWHEELLINES_CTRL, "Lines To Scroll"},
	{STR_SUSPEND_WINDOW_TITLE, "SheepShaver suspended. Press Space to reactivate."},
	{STR_VOSF_INIT_ERR, "Cannot initialize Video on SEGV signals."},

	{STR_OPEN_WINDOW_ERR, "Cannot open Mac window."},
	{STR_WINDOW_TITLE_GRABBED, "SheepShaver (mouse grabbed, press Ctrl-F5 to release)"},

	{STR_NO_B2_EXE_FOUND, "Could not start %s (%s)."},

	{-1, NULL}	// End marker
};


/*
 *  Fetch pointer to string, given the string number
 */

const char *GetString(int num)
{
	// First search for platform-specific string
	int i = 0;
	while (platform_strings[i].num >= 0) {
		if (platform_strings[i].num == num)
			return platform_strings[i].str;
		i++;
	}

	// Not found, search for common string
	i = 0;
	while (common_strings[i].num >= 0) {
		if (common_strings[i].num == num)
			return common_strings[i].str;
		i++;
	}
	return NULL;
}
