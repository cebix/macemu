/*
 *  user_strings_amiga.h - AmigaOS-specific localizable strings
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

#ifndef USER_STRINGS_AMIGA_H
#define USER_STRINGS_AMIGA_H

enum {
	STR_NO_PREPARE_EMUL_ERR = 10000,
	STR_NO_GADTOOLS_LIB_ERR,
	STR_NO_IFFPARSE_LIB_ERR,
	STR_NO_ASL_LIB_ERR,
	STR_NO_TIMER_DEV_ERR,
	STR_NO_P96_MODE_ERR,
	STR_NO_VIDEO_MODE_ERR,
	STR_WRONG_SCREEN_DEPTH_ERR,
	STR_WRONG_SCREEN_FORMAT_ERR,
	STR_ENFORCER_RUNNING_ERR,

	STR_NOT_ETHERNET_WARN,
	STR_NO_MULTICAST_WARN,
	STR_NO_GTLAYOUT_LIB_WARN,
	STR_NO_AHI_WARN,
	STR_NO_AHI_CTRL_WARN,
	STR_NOT_ENOUGH_MEM_WARN,

	STR_AHI_MODE_CTRL,
	STR_SCSI_MEMTYPE_CTRL,
	STR_MEMTYPE_CHIP_LAB,
	STR_MEMTYPE_24BITDMA_LAB,
	STR_MEMTYPE_ANY_LAB,
	STR_SCSI_DEVICES_CTRL
};

#endif
