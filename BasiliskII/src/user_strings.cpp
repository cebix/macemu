/*
 *  user_strings.cpp - Common localizable strings
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
#define ELLIPSIS "\xE2\x80\xA6"
#else
#define ELLIPSIS "..."
#endif


// Common string definitions
user_string_def common_strings[] = {
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
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted. Basilisk II will try to unmount it."},
	{STR_CANNOT_UNMOUNT_WARN, "The volume '%s' could not be unmounted. Basilisk II will not use it."},

	{STR_PREFS_TITLE, "Basilisk II Settings"},
	{STR_PREFS_MENU, "Settings"},
	{STR_PREFS_ITEM_ABOUT, "About Basilisk II" ELLIPSIS},
	{STR_PREFS_ITEM_START, "Start Basilisk II"},
	{STR_PREFS_ITEM_ZAP_PRAM, "Zap PRAM File"},
	{STR_PREFS_ITEM_QUIT, "Quit Basilisk II"},

	{STR_NONE_LAB, "<none>"},

	{STR_VOLUMES_PANE_TITLE, "Volumes"},
	{STR_VOLUMES_CTRL, "Mac Volumes"},
	{STR_ADD_VOLUME_BUTTON, "Add" ELLIPSIS},
	{STR_CREATE_VOLUME_BUTTON, "Create" ELLIPSIS},
	{STR_EDIT_VOLUME_BUTTON, "Edit" ELLIPSIS},
	{STR_REMOVE_VOLUME_BUTTON, "Remove"},
	{STR_ADD_VOLUME_PANEL_BUTTON, "Add"},
	{STR_CREATE_VOLUME_PANEL_BUTTON, "Create"},
	{STR_CDROM_DRIVE_CTRL, "CD-ROM Drive"},
	{STR_BOOTDRIVER_CTRL, "Boot From"},
	{STR_BOOT_ANY_LAB, "Any"},
	{STR_BOOT_CDROM_LAB, "CD-ROM"},
	{STR_NOCDROM_CTRL, "Disable CD-ROM Driver"},
	{STR_EXTFS_CTRL, "Host Root"},
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
	{STR_REF_DYNAMIC_LAB, "Dynamic"},
	{STR_DISPLAY_X_CTRL, "Width"},
	{STR_DISPLAY_Y_CTRL, "Height"},
	{STR_SIZE_384_LAB, "384"},
	{STR_SIZE_480_LAB, "480"},
	{STR_SIZE_512_LAB, "512"},
	{STR_SIZE_600_LAB, "600"},
	{STR_SIZE_640_LAB, "640"},
	{STR_SIZE_768_LAB, "768"},
	{STR_SIZE_800_LAB, "800"},
	{STR_SIZE_1024_LAB, "1024"},
	{STR_SIZE_MAX_LAB, "Maximum"},
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
	{STR_NOSOUND_CTRL, "Disable Sound Output"},

	{STR_SERIAL_NETWORK_PANE_TITLE, "Serial/Network"},
	{STR_SERIALA_CTRL, "Modem Port"},
	{STR_SERIALB_CTRL, "Printer Port"},
	{STR_ISPAR_CTRL, "Parallel Device"},
	{STR_ETHER_ENABLE_CTRL, "Enable Ethernet"},
	{STR_ETHERNET_IF_CTRL, "Ethernet Interface"},
	{STR_UDPTUNNEL_CTRL, "Tunnel MacOS Networking over UDP"},
	{STR_UDPPORT_CTRL, "UDP Port Number"},

	{STR_MEMORY_MISC_PANE_TITLE, "Memory/Misc"},
	{STR_RAMSIZE_CTRL, "MacOS RAM Size (MB)"},
	{STR_RAMSIZE_2MB_LAB, "2"},
	{STR_RAMSIZE_4MB_LAB, "4"},
	{STR_RAMSIZE_8MB_LAB, "8"},
	{STR_RAMSIZE_16MB_LAB, "16"},
	{STR_RAMSIZE_32MB_LAB, "32"},
	{STR_RAMSIZE_64MB_LAB, "64"},
	{STR_RAMSIZE_128MB_LAB, "128"},
	{STR_RAMSIZE_256MB_LAB, "256"},
	{STR_RAMSIZE_512MB_LAB, "512"},
	{STR_RAMSIZE_1024MB_LAB, "1024"},
	{STR_RAMSIZE_SLIDER, "MacOS RAM Size:"},
	{STR_RAMSIZE_FMT, "%ld MB"},
	{STR_MODELID_CTRL, "Mac Model ID"},
	{STR_MODELID_5_LAB, "Mac IIci (MacOS 7.x)"},
	{STR_MODELID_7_LAB, "Mac IIfx (MacOS 7.x)"},
	{STR_MODELID_12_LAB, "Mac IIsi (MacOS 7.x"},
	{STR_MODELID_13_LAB, "Mac LC (MacOS 7.x"},
	{STR_MODELID_14_LAB, "Quadra 900 (MacOS 8.x)"},
	{STR_MODELID_15_LAB, "PowerBook 170 (MacOS 7.x)"},
	{STR_MODELID_16_LAB, "Quadra 700 (MacOS 8.x)"},
	{STR_MODELID_19_LAB, "PowerBook 140 (MacOS 7.x)"},
	{STR_MODELID_20_LAB, "Quadra 950 (MacOS 8.x)"},
	{STR_MODELID_21_LAB, "Mac LC III-Performa 450 (MacOS 7.x)"},
	{STR_MODELID_24_LAB, "Centris 650 (MacOS 8.x)"},
	{STR_MODELID_29_LAB, "Quadra 800 (MacOS 8.x)"},
	{STR_MODELID_30_LAB, "Quadra 650 (MacOS 8.x)"},
	{STR_MODELID_31_LAB, "Mac LC II (MacOS 7.x)"},
	{STR_MODELID_38_LAB, "Mac IIvi (MacOS 7.x)"},
	{STR_MODELID_39_LAB, "Performa 600 (MacOS 7.x)"},
	{STR_MODELID_42_LAB, "Mac IIvx (MacOS 7.x)"},
	{STR_MODELID_43_LAB, "Color Classic (MacOS 7.x)"},
	{STR_MODELID_46_LAB, "Centris 610 (MacOS 8.x)"},
	{STR_MODELID_47_LAB, "Quadra 610 (MacOS 8.x)"},
	{STR_MODELID_50_LAB, "Mac LC 520 (MacOS 7.x)"},
	{STR_MODELID_54_LAB, "Centris-Quadra 660AV (MacOS 8.x)"},
	{STR_MODELID_56_LAB, "Performa 46x (MacOS 7.x)"},
	{STR_MODELID_72_LAB, "Quadra 840AV (MacOS 8.x)"},
	{STR_MODELID_74_LAB, "Mac LC-Performa 550 (MacOS 7.x)"},
	{STR_MODELID_82_LAB, "Mac TV (MacOS 7.x)"},
	{STR_MODELID_83_LAB, "Mac LC 475-Performa 47x (MacOS 8.x)"},
	{STR_MODELID_86_LAB, "Mac LC 575-Performa 57x (MacOS 8.x)"},
	{STR_MODELID_87_LAB, "Quadra 605 (MacOS 8.x)"},
	{STR_MODELID_92_LAB, "Mac LC-Performa-Quadra 630 (MacOS 8.x)"},
	{STR_MODELID_93_LAB, "Mac LC 580 (MacOS 8.x)"},
	{STR_CPU_CTRL, "CPU Type"},
	{STR_CPU_68020_LAB, "68020"},
	{STR_CPU_68020_FPU_LAB, "68020 with FPU"},
	{STR_CPU_68030_LAB, "68030"},
	{STR_CPU_68030_FPU_LAB, "68030 with FPU"},
	{STR_CPU_68040_LAB, "68040"},
	{STR_ROM_FILE_CTRL, "ROM File"},
	{STR_IDLEWAIT_CTRL, "Don't Use CPU When Idle"},
	{STR_JIT_PANE_TITLE, "JIT Compiler"},
	{STR_JIT_CTRL, "Enable JIT Compiler"},
	{STR_JIT_FPU_CTRL, "Compile FPU Instructions"},
	{STR_JIT_CACHE_SIZE_CTRL, "Translation Cache Size (KB)"},
	{STR_JIT_CACHE_SIZE_2MB_LAB, "2048"},
	{STR_JIT_CACHE_SIZE_4MB_LAB, "4096"},
	{STR_JIT_CACHE_SIZE_8MB_LAB, "8192"},
	{STR_JIT_CACHE_SIZE_16MB_LAB, "16384"},
	{STR_JIT_LAZY_CINV_CTRL, "Enable lazy invalidation of translation cache"},
	{STR_JIT_FOLLOW_CONST_JUMPS, "Translate through constant jumps (inline blocks)"},

	{STR_WINDOW_TITLE, "Basilisk II"},
	{STR_WINDOW_TITLE_FROZEN, "Basilisk II *** FROZEN ***"},
	{STR_WINDOW_TITLE_GRABBED, "Basilisk II (mouse grabbed, press Ctrl-F5 to release)"},
	{STR_WINDOW_MENU, "Basilisk II"},
	{STR_WINDOW_ITEM_ABOUT, "About Basilisk II" ELLIPSIS},
	{STR_WINDOW_ITEM_REFRESH, "Refresh Rate"},
	{STR_WINDOW_ITEM_MOUNT, "Mount"},
	{STR_SUSPEND_WINDOW_TITLE, "Basilisk II suspended. Press space to reactivate."},

	{STR_EXTFS_NAME, "Host Directory Tree"},
	{STR_EXTFS_VOLUME_NAME, "Host"},

	{-1, NULL}	// End marker
};
