/*
 *  user_strings.cpp - Localizable strings
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

/*
 *  NOTES:
 *
 *  This should only be used for user-interface related messages that must be
 *  translated or transcibed for localized versions of Basilisk.
 *  It should NOT be used for:
 *   - file names
 *   - names of threads, areas, ports, semaphores, drivers, views and other "invisible" names
 *   - debugging messages
 *   - error messages that only go to the shell ("FATAL"/"WARNING", those are really debugging messages)
 */

#include "sysdeps.h"
#include "user_strings.h"

#ifdef __BEOS__
#define UTF8_ELLIPSIS "\xE2\x80\xA6"
#else
#define UTF8_ELLIPSIS "..."
#endif


struct str_def {
	int num;
	char *str;
};


// Localized strings
#if 0
static const str_def loc_strings[] = {
	{STR_READING_ROM_FILE, "ROM-Datei wird gelesen...\n"},
	{-1, NULL}	// End marker
};
#else
static const str_def loc_strings[] = {
	{-1, NULL}	// End marker
};
#endif


// Default strings
static const str_def default_strings[] = {
	{STR_ABOUT_TEXT1, "Basilisk II V%d.%d"},
	{STR_ABOUT_TEXT2, "by Christian Bauer et al."},
	{STR_READING_ROM_FILE, "Reading ROM file...\n"},
	{STR_SHELL_ERROR_PREFIX, "ERROR: %s\n"},
	{STR_GUI_ERROR_PREFIX, "Basilisk II error:\n%s"},
	{STR_ERROR_ALERT_TITLE, "Basilisk II Error"},
	{STR_SHELL_WARNING_PREFIX, "WARNING: %s\n"},
	{STR_GUI_WARNING_PREFIX, "Basilisk II warning:\n%s"},
	{STR_WARNING_ALERT_TITLE, "Basilisk II Warning"},
	{STR_NOTICE_ALERT_TITLE, "Basilisk II Notice"},
	{STR_ABOUT_TITLE, "About Basilisk II"},
	{STR_OK_BUTTON, "OK"},
	{STR_START_BUTTON, "Start"},
	{STR_QUIT_BUTTON, "Quit"},
	{STR_CANCEL_BUTTON, "Cancel"},

	{STR_NO_MEM_ERR, "Not enough free memory."},
	{STR_NOT_ENOUGH_MEMORY_ERR, "Your computer does not have enough memory to run Basilisk II."},
	{STR_NO_RAM_AREA_ERR, "Not enough memory to create RAM area."},
	{STR_NO_ROM_AREA_ERR, "Not enough memory to create ROM area."},
	{STR_NO_ROM_FILE_ERR, "Cannot open ROM file."},
	{STR_ROM_FILE_READ_ERR, "Cannot read ROM file."},
	{STR_ROM_SIZE_ERR, "Invalid ROM file size. Basilisk II requires a 512K or 1MB MacII ROM."},
	{STR_UNSUPPORTED_ROM_TYPE_ERR, "Unsupported ROM type."},
	{STR_OPEN_WINDOW_ERR, "Cannot open Mac window."},
	{STR_OPEN_SCREEN_ERR, "Cannot open Mac screen."},
	{STR_SCSI_BUFFER_ERR, "Cannot allocate SCSI buffer (requested %d bytes). Giving up."},
	{STR_SCSI_SG_FULL_ERR, "SCSI scatter/gather table full. Giving up."},

	{STR_SMALL_RAM_WARN, "Selected less than 1MB Mac RAM, using 1MB."},
	{STR_CREATE_VOLUME_WARN, "Cannot create hardfile (%s)."},

	{STR_PREFS_TITLE, "Basilisk II Settings"},
	{STR_PREFS_MENU, "Settings"},
	{STR_PREFS_ITEM_ABOUT, "About Basilisk II" UTF8_ELLIPSIS},
	{STR_PREFS_ITEM_START, "Start Basilisk II"},
	{STR_PREFS_ITEM_ZAP_PRAM, "Zap PRAM File"},
	{STR_PREFS_ITEM_QUIT, "Quit Basilisk II"},
	{STR_PREFS_MENU_FILE_GTK, "/_File"},
	{STR_PREFS_ITEM_START_GTK, "/File/_Start Basilisk II"},
	{STR_PREFS_ITEM_ZAP_PRAM_GTK, "/File/_Zap PRAM File"},
	{STR_PREFS_ITEM_SEPL_GTK, "/File/sepl"},
	{STR_PREFS_ITEM_QUIT_GTK, "/File/_Quit Basilisk II"},
	{STR_HELP_MENU_GTK, "/_Help"},
	{STR_HELP_ITEM_ABOUT_GTK, "/Help/_About Basilisk II"},

	{STR_VOLUMES_PANE_TITLE, "Volumes"},
	{STR_VOLUMES_CTRL, "Mac Volumes"},
	{STR_ADD_VOLUME_BUTTON, "Add" UTF8_ELLIPSIS},
	{STR_CREATE_VOLUME_BUTTON, "Create" UTF8_ELLIPSIS},
	{STR_EDIT_VOLUME_BUTTON, "Edit" UTF8_ELLIPSIS},
	{STR_REMOVE_VOLUME_BUTTON, "Remove"},
	{STR_ADD_VOLUME_PANEL_BUTTON, "Add"},
	{STR_CREATE_VOLUME_PANEL_BUTTON, "Create"},
	{STR_CDROM_DRIVE_CTRL, "CD-ROM Drive"},
	{STR_BOOTDRIVER_CTRL, "Boot From"},
	{STR_BOOT_ANY_LAB, "Any"},
	{STR_BOOT_CDROM_LAB, "CD-ROM"},
	{STR_NOCDROM_CTRL, "Disable CD-ROM Driver"},
	{STR_DEVICE_CTRL, "Device"},
	{STR_UNIT_CTRL, "Unit"},
	{STR_ADD_VOLUME_TITLE, "Add Volume"},
	{STR_CREATE_VOLUME_TITLE, "Create Hardfile"},
	{STR_EDIT_VOLUME_TITLE, "Edit Volume"},
	{STR_HARDFILE_SIZE_CTRL, "Size (MB)"},
	{STR_VOL_READONLY_CTRL, "Read-Only"},
	{STR_VOL_TYPE_CTRL, "Type"},
	{STR_VOL_FILE_LAB, "File"},
	{STR_VOL_DEVICE_LAB, "Device"},
	{STR_VOL_OPENFLAGS_CTRL, "Open Flags"},
	{STR_VOL_STARTBLOCK_CTRL, "Start Block"},
	{STR_VOL_SIZE_CTRL, "Size (Blocks)"},
	{STR_VOL_BLOCKSIZE_CTRL, "Block Size"},
	{STR_VOL_FILE_CTRL, "File"},

	{STR_SCSI_PANE_TITLE, "SCSI"},
	{STR_SCSI_ID_0, "ID 0"},
	{STR_SCSI_ID_1, "ID 1"},
	{STR_SCSI_ID_2, "ID 2"},
	{STR_SCSI_ID_3, "ID 3"},
	{STR_SCSI_ID_4, "ID 4"},
	{STR_SCSI_ID_5, "ID 5"},
	{STR_SCSI_ID_6, "ID 6"},

	{STR_GRAPHICS_SOUND_PANE_TITLE, "Graphics/Sound"},
	{STR_GRAPHICS_CTRL, "Graphics"},
	{STR_VIDEO_TYPE_CTRL, "Video Type"},
	{STR_WINDOW_LAB, "Window"},
	{STR_FULLSCREEN_LAB, "Fullscreen"},
	{STR_PIP_LAB, "PIP"},
	{STR_FRAMESKIP_CTRL, "Window Refresh Rate"},
	{STR_REF_5HZ_LAB, "5 Hz"},
	{STR_REF_7_5HZ_LAB, "7.5 Hz"},
	{STR_REF_10HZ_LAB, "10 Hz"},
	{STR_REF_15HZ_LAB, "15 Hz"},
	{STR_REF_30HZ_LAB, "30 Hz"},
	{STR_REF_60HZ_LAB, "60 Hz"},
	{STR_DISPLAY_X_CTRL, "Width"},
	{STR_DISPLAY_Y_CTRL, "Height"},
	{STR_COLOR_DEPTH_CTRL, "Color Depth"},
	{STR_1_BIT_LAB, "B/W (1 Bit)"},
	{STR_2_BIT_LAB, "4 (2 Bit)"},
	{STR_4_BIT_LAB, "16 (4 Bit)"},
	{STR_8_BIT_LAB, "256 (8 Bit)"},
	{STR_15_BIT_LAB, "Thousands (15 Bit)"},
	{STR_24_BIT_LAB, "Millions (24 Bit)"},
	{STR_SCREEN_MODE_CTRL, "Screen Mode"},
	{STR_8_BIT_640x480_LAB, "640x480, 8 Bit"},
	{STR_8_BIT_800x600_LAB, "800x600, 8 Bit"},
	{STR_8_BIT_1024x768_LAB, "1024x768, 8 Bit"},
    {STR_8_BIT_1152x900_LAB, "1152x900, 8 Bit"},
	{STR_8_BIT_1280x1024_LAB, "1280x1024, 8 Bit"},
	{STR_8_BIT_1600x1200_LAB, "1600x1200, 8 Bit"},
	{STR_15_BIT_640x480_LAB, "640x480, 15 Bit"},
	{STR_15_BIT_800x600_LAB, "800x600, 15 Bit"},
	{STR_15_BIT_1024x768_LAB, "1024x768, 15 Bit"},
    {STR_15_BIT_1152x900_LAB, "1152x900, 15 Bit"},
	{STR_15_BIT_1280x1024_LAB, "1280x1024, 15 Bit"},
	{STR_15_BIT_1600x1200_LAB, "1600x1200, 15 Bit"},
	{STR_24_BIT_640x480_LAB, "640x480, 24 Bit"},
	{STR_24_BIT_800x600_LAB, "800x600, 24 Bit"},
	{STR_24_BIT_1024x768_LAB, "1024x768, 24 Bit"},
    {STR_24_BIT_1152x900_LAB, "1152x900, 24 Bit"},
	{STR_24_BIT_1280x1024_LAB, "1280x1024, 24 Bit"},
	{STR_24_BIT_1600x1200_LAB, "1600x1200, 24 Bit"},
	{STR_SOUND_CTRL, "Sound"},
	{STR_AHI_MODE_CTRL, "AHI Mode"},
	{STR_NOSOUND_CTRL, "Disable Sound Output"},

	{STR_SERIAL_NETWORK_PANE_TITLE, "Serial/Network"},
	{STR_SERIALA_CTRL, "Modem Port"},
	{STR_SERIALB_CTRL, "Printer Port"},
	{STR_ISPAR_CTRL, "Parallel Device"},
	{STR_ETHER_ENABLE_CTRL, "Enable Ethernet"},
	{STR_ETHERNET_IF_CTRL, "Ethernet Interface"},

	{STR_MEMORY_MISC_PANE_TITLE, "Memory/Misc"},
	{STR_RAMSIZE_SLIDER, "MacOS RAM Size:"},
	{STR_RAMSIZE_FMT, "%ld MB"},
	{STR_MODELID_CTRL, "Mac Model ID"},
	{STR_MODELID_5_LAB, "Mac IIci (MacOS 7.x)"},
	{STR_MODELID_14_LAB, "Quadra 900 (MacOS 8.x)"},
	{STR_ROM_FILE_CTRL, "ROM File"},
	{STR_KEYCODES_CTRL, "Use Raw Keycodes"},
	{STR_KEYCODE_FILE_CTRL, "Keycode Translation File"},

	{STR_WINDOW_TITLE, "Basilisk II"},
	{STR_WINDOW_TITLE_FROZEN, "Basilisk II *** FROZEN ***"},
	{STR_WINDOW_MENU, "Basilisk II"},
	{STR_WINDOW_ITEM_ABOUT, "About Basilisk II" UTF8_ELLIPSIS},
	{STR_WINDOW_ITEM_REFRESH, "Refresh Rate"},
	{STR_WINDOW_ITEM_MOUNT, "Mount"},
	{STR_SUSPEND_WINDOW_TITLE, "Basilisk II suspended. Press space to reactivate."},

	{STR_NO_SHEEP_DRIVER_ERR, "Cannot open /dev/sheep: %s (%08x). Basilisk II is not properly installed."},
	{STR_SHEEP_UP_ERR, "Cannot allocate Low Memory Globals: %s (%08x)."},
	{STR_NO_KERNEL_DATA_ERR, "Cannot create Kernel Data area: %s (%08x)."},
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted under BeOS. Basilisk II will try to unmount it."},
	{STR_CANNOT_UNMOUNT_WARN, "The volume '%s' could not be unmounted. Basilisk II will not use it."},
	{STR_NO_NET_ADDON_WARN, "The SheepShaver net server add-on cannot be found. Ethernet will not be available."},
	{STR_NET_CONFIG_MODIFY_WARN, "To enable Ethernet networking for Basilisk II, your network configuration has to be modified and the network restarted. Do you want this to be done now (selecting \"Cancel\" will disable Ethernet under Basilisk II)?."},
	{STR_NET_ADDON_INIT_FAILED, "SheepShaver net server add-on found\nbut there seems to be no network hardware.\nPlease check your network preferences."},
	{STR_NET_ADDON_CLONE_FAILED, "Cloning of the network transfer area failed."},

	{STR_NO_XSERVER_ERR, "Cannot connect to X server '%s'."},
	{STR_NO_XVISUAL_ERR, "Cannot obtain appropriate X visual."},
	{STR_UNSUPP_DEPTH_ERR, "Unsupported color depth of screen."},
	{STR_NO_SHEEP_NET_DRIVER_WARN, "Cannot open %s (%s). Ethernet will not be available."},
	{STR_SHEEP_NET_ATTACH_WARN, "Cannot attach to Ethernet card (%s). Ethernet will not be available."},
	{STR_SCSI_DEVICE_OPEN_WARN, "Cannot open %s (%s). SCSI Manager access to this device will be disabled."},
	{STR_SCSI_DEVICE_NOT_SCSI_WARN, "%s doesn't seem to comply to the Generic SCSI API. SCSI Manager access to this device will be disabled."},
	{STR_NO_AUDIO_DEV_WARN, "Cannot open %s (%s). Audio output will be disabled."},
	{STR_AUDIO_FORMAT_WARN, "Audio hardware doesn't support signed 16 bit format. Audio output will be disabled."},
	{STR_KEYCODE_FILE_WARN, "Cannot open keycode translation file %s (%s)."},
	{STR_KEYCODE_VENDOR_WARN, "Cannot find vendor '%s' in keycode translation file %s."},

	{STR_NO_PREPARE_EMUL_ERR, "PrepareEmul is not installed. Run PrepareEmul and then try again to start Basilisk II."},
	{STR_NO_GADTOOLS_LIB_ERR, "Cannot open gadtools.library V39."},
	{STR_NO_ASL_LIB_ERR, "Cannot open asl.library V36."},
	{STR_NO_TIMER_DEV_ERR, "Cannot open timer.device."},
	{STR_NO_P96_MODE_ERR, "The selected screen mode is not a Picasso96 mode."},
	{STR_WRONG_SCREEN_DEPTH_ERR, "Basilisk II only supports 8, 16 or 24 bit screens."},
	{STR_WRONG_SCREEN_FORMAT_ERR, "Basilisk II only supports big-endian chunky ARGB screen modes."},
	{STR_NOT_ETHERNET_WARN, "The selected network device is not an Ethernet device. Networking will be disabled."},
	{STR_NO_MULTICAST_WARN, "Your Ethernet card does not support multicast and is not usable with AppleTalk. Please report this to the manufacturer of the card."},
	{STR_NO_GTLAYOUT_LIB_WARN, "Cannot open gtlayout.library V39. The preferences editor GUI will not be available."},
	{STR_NO_AHI_WARN, "Cannot open ahi.device V2. Audio output will be disabled."},
	{STR_NO_AHI_CTRL_WARN, "Cannot open AHI control structure. Audio output will be disabled."},

	{-1, NULL}	// End marker
};


/*
 *  Fetch pointer to string, given the string number
 */

char *GetString(int num)
{
	// First search for localized string
	int i = 0;
	while (loc_strings[i].num >= 0) {
		if (loc_strings[i].num == num)
			return loc_strings[i].str;
		i++;
	}

	// Not found, then get default string
	i = 0;
	while (default_strings[i].num >= 0) {
		if (default_strings[i].num == num)
			return default_strings[i].str;
		i++;
	}
	return NULL;
}
