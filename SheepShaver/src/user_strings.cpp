/*
 *  user_strings.cpp - Localizable strings
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

/*
 *  NOTES:
 *
 *  This should only be used for user-interface related messages that must be
 *  translated or transcibed for localized versions of SheepShaver.
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
	{STR_ABOUT_TEXT1, "SheepShaver V%d.%d"},
	{STR_ABOUT_TEXT2, "by Christian Bauer and Mar\"c\" Hellwig"},
	{STR_READING_ROM_FILE, "Reading ROM file...\n"},
	{STR_SHELL_ERROR_PREFIX, "ERROR: %s\n"},
	{STR_GUI_ERROR_PREFIX, "SheepShaver error:\n%s"},
	{STR_ERROR_ALERT_TITLE, "SheepShaver Error"},
	{STR_SHELL_WARNING_PREFIX, "WARNING: %s\n"},
	{STR_GUI_WARNING_PREFIX, "SheepShaver warning:\n%s"},
	{STR_WARNING_ALERT_TITLE, "SheepShaver Warning"},
	{STR_NOTICE_ALERT_TITLE, "SheepShaver Notice"},
	{STR_ABOUT_TITLE, "About SheepShaver"},
	{STR_OK_BUTTON, "OK"},
	{STR_START_BUTTON, "Start"},
	{STR_QUIT_BUTTON, "Quit"},
	{STR_CANCEL_BUTTON, "Cancel"},
	{STR_IGNORE_BUTTON, "Ignore"},

	{STR_NOT_ENOUGH_MEMORY_ERR, "Your computer does not have enough memory to run SheepShaver."},
	{STR_NO_KERNEL_DATA_ERR, "Cannot create Kernel Data area: %s (%08x)."},
	{STR_NO_ROM_FILE_ERR, "Cannot open ROM file."},
	{STR_RAM_HIGHER_THAN_ROM_ERR, "RAM area higher than ROM area. Try to decrease the MacOS RAM size."},
	{STR_ROM_FILE_READ_ERR, "Cannot read ROM file."},
	{STR_ROM_SIZE_ERR, "Invalid ROM file size. SheepShaver requires a 4MB PCI PowerMac ROM."},
	{STR_UNSUPPORTED_ROM_TYPE_ERR, "Unsupported ROM type."},
	{STR_POWER_INSTRUCTION_ERR, "Your Mac program is using POWER instructions which are not supported by SheepShaver.\n(pc %p, sp %p, opcode %08lx)"},
	{STR_MEM_ACCESS_ERR, "Your Mac program made an illegal %s %s access to address %p.\n(pc %p, 68k pc %p, sp %p)"},
	{STR_MEM_ACCESS_READ, "read"},
	{STR_MEM_ACCESS_WRITE, "write"},
	{STR_UNKNOWN_SEGV_ERR, "Your Mac program did something terribly stupid.\n(pc %p, 68k pc %p, sp %p, opcode %08lx)"},
	{STR_NO_NAME_REGISTRY_ERR, "Cannot find Name Registry. Giving up."},
	{STR_FULL_SCREEN_ERR, "Cannot open full screen display: %s (%08x)."},
	{STR_SCSI_BUFFER_ERR, "Cannot allocate SCSI buffer (requested %d bytes). Giving up."},
	{STR_SCSI_SG_FULL_ERR, "SCSI scatter/gather table full. Giving up."},

	{STR_SMALL_RAM_WARN, "Selected less than 8MB Mac RAM, using 8MB."},
	{STR_CANNOT_UNMOUNT_WARN, "The volume '%s' could not be unmounted. SheepShaver will not use it."},
	{STR_CREATE_VOLUME_WARN, "Cannot create hardfile (%s)."},

	{STR_PREFS_TITLE, "SheepShaver Settings"},
	{STR_PREFS_MENU, "Settings"},
	{STR_PREFS_ITEM_ABOUT, "About SheepShaver" ELLIPSIS},
	{STR_PREFS_ITEM_START, "Start SheepShaver"},
	{STR_PREFS_ITEM_ZAP_PRAM, "Zap PRAM File"},
	{STR_PREFS_ITEM_QUIT, "Quit SheepShaver"},

	{STR_NONE_LAB, "<none>"},

	{STR_VOLUMES_PANE_TITLE, "Volumes"},
	{STR_ADD_VOLUME_BUTTON, "Add" ELLIPSIS},
	{STR_CREATE_VOLUME_BUTTON, "Create" ELLIPSIS},
	{STR_REMOVE_VOLUME_BUTTON, "Remove"},
	{STR_ADD_VOLUME_PANEL_BUTTON, "Add"},
	{STR_CREATE_VOLUME_PANEL_BUTTON, "Create"},
	{STR_CDROM_DRIVE_CTRL, "CD-ROM Drive"},
	{STR_BOOTDRIVER_CTRL, "Boot From"},
	{STR_BOOT_ANY_LAB, "Any"},
	{STR_BOOT_CDROM_LAB, "CD-ROM"},
	{STR_NOCDROM_CTRL, "Disable CD-ROM Driver"},
	{STR_ADD_VOLUME_TITLE, "Add Volume"},
	{STR_CREATE_VOLUME_TITLE, "Create Hardfile"},
	{STR_HARDFILE_SIZE_CTRL, "Size (MB)"},

	{STR_GRAPHICS_SOUND_PANE_TITLE, "Graphics/Sound"},
	{STR_FRAMESKIP_CTRL, "Window Refresh Rate"},
	{STR_REF_5HZ_LAB, "5 Hz"},
	{STR_REF_7_5HZ_LAB, "7.5 Hz"},
	{STR_REF_10HZ_LAB, "10 Hz"},
	{STR_REF_15HZ_LAB, "15 Hz"},
	{STR_REF_30HZ_LAB, "30 Hz"},
	{STR_REF_60HZ_LAB, "60 Hz"},
	{STR_REF_DYNAMIC_LAB, "Dynamic"},
	{STR_GFXACCEL_CTRL, "QuickDraw Acceleration"},
	{STR_8_BIT_CTRL, "8 Bit"},
	{STR_16_BIT_CTRL, "15 Bit"},
	{STR_32_BIT_CTRL, "32 Bit"},
	{STR_W_640x480_CTRL, "Window 640x480"},
	{STR_W_800x600_CTRL, "Window 800x600"},
	{STR_640x480_CTRL, "Fullscreen 640x480"},
	{STR_800x600_CTRL, "Fullscreen 800x600"},
	{STR_1024x768_CTRL, "Fullscreen 1024x768"},
	{STR_1152x768_CTRL, "Fullscreen 1152x768"},
	{STR_1152x900_CTRL, "Fullscreen 1152x900"},
	{STR_1280x1024_CTRL, "Fullscreen 1280x1024"},
	{STR_1600x1200_CTRL, "Fullscreen 1600x1200"},
	{STR_VIDEO_MODE_CTRL, "Enabled Video Modes"},
	{STR_FULLSCREEN_CTRL, "Fullscreen"},
	{STR_WINDOW_CTRL, "Window"},
	{STR_VIDEO_TYPE_CTRL, "Video Type"},
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
	{STR_NOSOUND_CTRL, "Disable Sound Output"},

	{STR_SERIAL_NETWORK_PANE_TITLE, "Serial/Network"},
	{STR_SERPORTA_CTRL, "Modem Port"},
	{STR_SERPORTB_CTRL, "Printer Port"},
	{STR_NONET_CTRL, "Disable Ethernet"},
	{STR_ETHERNET_IF_CTRL, "Ethernet Interface"},

	{STR_MEMORY_MISC_PANE_TITLE, "Memory/Misc"},
	{STR_RAMSIZE_CTRL, "MacOS RAM Size (MB)"},
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
	{STR_RAMSIZE_FMT, "%d MB"},
	{STR_IGNORESEGV_CTRL, "Ignore Illegal Memory Accesses"},
	{STR_IDLEWAIT_CTRL, "Don't Use CPU When Idle"},
	{STR_ROM_FILE_CTRL, "ROM File"},

	{STR_JIT_PANE_TITLE, "JIT Compiler"},
	{STR_JIT_CTRL, "Enable JIT Compiler"},
	{STR_JIT_68K_CTRL, "Enable built-in 68k DR Emulator (EXPERIMENTAL)"},

	{STR_WINDOW_TITLE, "SheepShaver"},
	{STR_WINDOW_TITLE_FROZEN, "SheepShaver *** FROZEN ***"},
	{STR_WINDOW_TITLE_GRABBED, "SheepShaver (mouse grabbed, press Ctrl-F5 to release)"},
	{STR_WINDOW_TITLE_GRABBED0, "SheepShaver (mouse grabbed, press "},
	{STR_WINDOW_TITLE_GRABBED1, "Ctrl-"},
#ifdef __MACOSX__
	{STR_WINDOW_TITLE_GRABBED2, "Opt-"},
	{STR_WINDOW_TITLE_GRABBED3, "Cmd-"},
#else
	{STR_WINDOW_TITLE_GRABBED2, "Alt-"},
	{STR_WINDOW_TITLE_GRABBED3, "Win-"},
#endif
	{STR_WINDOW_TITLE_GRABBED4, "F5 to release)"},
	{STR_WINDOW_MENU, "SheepShaver"},
	{STR_WINDOW_ITEM_ABOUT, "About SheepShaver" ELLIPSIS},
	{STR_WINDOW_ITEM_REFRESH, "Refresh Rate"},
	{STR_WINDOW_ITEM_MOUNT, "Mount"},

	{STR_SOUND_IN_NAME, "\010Built-In"},

	{STR_EXTFS_NAME, "Host Directory Tree"},
	{STR_EXTFS_VOLUME_NAME, "Host"},

	{-1, NULL}	// End marker
};
