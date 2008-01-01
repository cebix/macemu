/*
 *  user_strings_amiga.cpp - AmigaOS-specific localizable strings
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

#include "sysdeps.h"
#include "user_strings.h"


// Platform-specific string definitions
user_string_def platform_strings[] = {
	// Common strings that have a platform-specific variant
	{STR_VOLUME_IS_MOUNTED_WARN, "The volume '%s' is mounted under AmigaOS. Basilisk II will try to unmount it."},
	{STR_EXTFS_CTRL, "Amiga Root"},
	{STR_EXTFS_NAME, "Amiga Directory Tree"},
	{STR_EXTFS_VOLUME_NAME, "Amiga"},

	// Purely platform-specific strings
	{STR_NO_PREPARE_EMUL_ERR, "PrepareEmul is not installed. Run PrepareEmul and then try again to start Basilisk II."},
	{STR_NO_GADTOOLS_LIB_ERR, "Cannot open gadtools.library V39."},
	{STR_NO_IFFPARSE_LIB_ERR, "Cannot open iffparse.library V39."},
	{STR_NO_ASL_LIB_ERR, "Cannot open asl.library V36."},
	{STR_NO_TIMER_DEV_ERR, "Cannot open timer.device."},
	{STR_NO_P96_MODE_ERR, "The selected screen mode is not a Picasso96 or CyberGraphX mode."},
	{STR_NO_VIDEO_MODE_ERR, "Cannot obtain selected video mode."},
	{STR_WRONG_SCREEN_DEPTH_ERR, "Basilisk II only supports 8, 16 or 24 bit screens."},
	{STR_WRONG_SCREEN_FORMAT_ERR, "Basilisk II only supports big-endian chunky ARGB screen modes."},
	{STR_ENFORCER_RUNNING_ERR, "Enforcer/CyberGuard is running. Remove and then try again to start Basilisk II."},

	{STR_NOT_ETHERNET_WARN, "The selected network device is not an Ethernet device. Networking will be disabled."},
	{STR_NO_MULTICAST_WARN, "Your Ethernet card does not support multicast and is not usable with AppleTalk. Please report this to the manufacturer of the card."},
	{STR_NO_GTLAYOUT_LIB_WARN, "Cannot open gtlayout.library V39. The preferences editor GUI will not be available."},
	{STR_NO_AHI_WARN, "Cannot open ahi.device V2. Audio output will be disabled."},
	{STR_NO_AHI_CTRL_WARN, "Cannot open AHI control structure. Audio output will be disabled."},
	{STR_NOT_ENOUGH_MEM_WARN, "Could not get %lu MBytes of memory.\nShould I use the largest Block (%lu MBytes) instead ?"},

	{STR_AHI_MODE_CTRL, "AHI Mode"},
	{STR_SCSI_MEMTYPE_CTRL, "Buffer Memory Type"},
	{STR_MEMTYPE_CHIP_LAB, "Chip"},
	{STR_MEMTYPE_24BITDMA_LAB, "24-Bit DMA"},
	{STR_MEMTYPE_ANY_LAB, "Any"},
	{STR_SCSI_DEVICES_CTRL, "Virtual SCSI Devices"},

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
