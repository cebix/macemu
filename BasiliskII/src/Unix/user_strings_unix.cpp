/*
 *  user_strings_unix.cpp - Unix-specific localizable strings
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted under Unix. Basilisk II will try to unmount it."},
	{STR_EXTFS_CTRL, "Unix Root"},
	{STR_EXTFS_NAME, "Unix Directory Tree"},
	{STR_EXTFS_VOLUME_NAME, "Unix"},

	// Purely platform-specific strings
	{STR_NO_XSERVER_ERR, "Cannot connect to X server '%s'."},
	{STR_NO_XVISUAL_ERR, "Cannot obtain appropriate X visual."},
	{STR_UNSUPP_DEPTH_ERR, "Unsupported color depth of screen."},
	{STR_NO_FBDEVICE_FILE_ERR, "Cannot open frame buffer device specification file %s (%s)."},
	{STR_FBDEV_NAME_ERR, "The %s frame buffer is not supported in %d bit mode."},
	{STR_FBDEV_MMAP_ERR, "Cannot mmap() the frame buffer memory (%s)."},

	{STR_NO_SHEEP_NET_DRIVER_WARN, "Cannot open %s (%s). Ethernet will not be available."},
	{STR_SHEEP_NET_ATTACH_WARN, "Cannot attach to Ethernet card (%s). Ethernet will not be available."},
	{STR_SCSI_DEVICE_OPEN_WARN, "Cannot open %s (%s). SCSI Manager access to this device will be disabled."},
	{STR_SCSI_DEVICE_NOT_SCSI_WARN, "%s doesn't seem to comply to the Generic SCSI API. SCSI Manager access to this device will be disabled."},
	{STR_NO_AUDIO_DEV_WARN, "Cannot open %s (%s). Audio output will be disabled."},
	{STR_NO_ESD_WARN, "Cannot open ESD connection. Audio output will be disabled."},
	{STR_AUDIO_FORMAT_WARN, "Audio hardware doesn't support signed 16 bit format. Audio output will be disabled."},
	{STR_KEYCODE_FILE_WARN, "Cannot open keycode translation file %s (%s)."},
	{STR_KEYCODE_VENDOR_WARN, "Cannot find vendor '%s' in keycode translation file %s."},

	{STR_PREFS_MENU_FILE_GTK, "/_File"},
	{STR_PREFS_ITEM_START_GTK, "/File/_Start Basilisk II"},
	{STR_PREFS_ITEM_ZAP_PRAM_GTK, "/File/_Zap PRAM File"},
	{STR_PREFS_ITEM_SEPL_GTK, "/File/sepl"},
	{STR_PREFS_ITEM_QUIT_GTK, "/File/_Quit Basilisk II"},
	{STR_HELP_MENU_GTK, "/_Help"},
	{STR_HELP_ITEM_ABOUT_GTK, "/Help/_About Basilisk II"},

	{STR_KEYCODES_CTRL, "Use Raw Keycodes"},
	{STR_KEYCODE_FILE_CTRL, "Keycode Translation File"},
	{STR_FBDEV_NAME_CTRL, "Frame Buffer Name"},
	{STR_FBDEVICE_FILE_CTRL, "Frame Buffer Spec File"},

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
