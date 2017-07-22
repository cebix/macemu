/*
 *  video.h - Video/graphics emulation
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

#ifndef VIDEO_H
#define VIDEO_H

#include <vector>

#ifndef NO_STD_NAMESPACE
using std::vector;
#endif


/*
   Some of the terminology here is completely frelled. In Basilisk II, a
   "video mode" refers to a combination of resolution and color depth, and
   this information is stored in a video_mode structure. In Apple
   documentation, a "mode" historically refers to the color depth only
   (because old Macs had fixed-frequency monitors and could not change the
   resolution). These "modes" are assigned a number (0x80, 0x81, etc.),
   which we here call "Apple mode". When Macs learned how to deal with
   multiscan monitors, Apple introduced another type of "mode", also having
   numbers starting from 0x80 but refrerring to the resolution and/or video
   timing of the display (it's possible to have two modes with the same
   dimension but different refresh rates). We call this a "resolution ID". 
   The combination of "Apple mode" and "ID" corresponds to a Basilisk II
   "video mode". To make the confusion worse, the video driver control call
   that sets the color depth is called "SetMode" while the one that sets
   both depth and resolution is "SwitchMode"...
*/

// Color depth codes
enum video_depth {
	VDEPTH_1BIT,  // 2 colors
	VDEPTH_2BIT,  // 4 colors
	VDEPTH_4BIT,  // 16 colors
	VDEPTH_8BIT,  // 256 colors
	VDEPTH_16BIT, // "Thousands"
	VDEPTH_32BIT  // "Millions"
};

// 1, 2, 4 and 8 bit depths use a color palette
inline bool IsDirectMode(video_depth depth)
{
	return depth == VDEPTH_16BIT || depth == VDEPTH_32BIT;
}

// Return the depth code that corresponds to the specified bits-per-pixel value
inline video_depth DepthModeForPixelDepth(int depth)
{
	switch (depth) {
		case 1: return VDEPTH_1BIT;
		case 2: return VDEPTH_2BIT;
		case 4: return VDEPTH_4BIT;
		case 8: return VDEPTH_8BIT;
		case 15: case 16: return VDEPTH_16BIT;
		case 24: case 32: return VDEPTH_32BIT;
		default: return VDEPTH_1BIT;
	}
}

// Returns the name of a video_depth, or an empty string if the depth is unknown
const char * NameOfDepth(video_depth depth);

// Return a bytes-per-row value (assuming no padding) for the specified depth and pixel width
inline uint32 TrivialBytesPerRow(uint32 width, video_depth depth)
{
	switch (depth) {
		case VDEPTH_1BIT: return width / 8;
		case VDEPTH_2BIT: return width / 4;
		case VDEPTH_4BIT: return width / 2;
		case VDEPTH_8BIT: return width;
		case VDEPTH_16BIT: return width * 2;
		case VDEPTH_32BIT: return width * 4;
		default: return width;
	}
}


/*
   You are not completely free in your selection of depth/resolution
   combinations:
     1) the lowest supported color depth must be available in all
        resolutions
     2) if one resolution provides a certain color depth, it must also
        provide all lower supported depths

   For example, it is possible to have this set of modes:
     640x480 @ 8 bit
     640x480 @ 32 bit
     800x600 @ 8 bit
     800x600 @ 32 bit
     1024x768 @ 8 bit

   But this is not possible (violates rule 1):
     640x480 @ 8 bit
     800x600 @ 8 bit
     1024x768 @ 1 bit

   And neither is this (violates rule 2, 640x480 @ 16 bit is missing):
     640x480 @ 8 bit
     640x480 @ 32 bit
     800x600 @ 8 bit
     800x600 @ 16 bit
     1024x768 @ 8 bit
*/

// Description of a video mode
struct video_mode {
	uint32 x;				// X size of screen (pixels)
	uint32 y;				// Y size of screen (pixels)
	uint32 resolution_id;	// Resolution ID (should be >= 0x80 and uniquely identify the sets of modes with the same X/Y size)
	video_depth depth;		// Color depth (see definitions above)
	uint32 bytes_per_row;	// Bytes per row of frame buffer
	uint32 user_data;		// Free for use by platform-specific code
};

inline bool IsDirectMode(const video_mode &mode)
{
	return IsDirectMode(mode.depth);
}


// Mac video driver per-display private variables (opaque)
struct video_locals;


// Abstract base class representing one (possibly virtual) monitor
// ("monitor" = rectangular display with a contiguous frame buffer)
class monitor_desc {
public:
	monitor_desc(const vector<video_mode> &available_modes, video_depth default_depth, uint32 default_id);
	virtual ~monitor_desc() {}

	// Get Mac slot ID number
	uint8 get_slot_id(void) const {return slot_id;}

	// Get current Mac frame buffer base address
	uint32 get_mac_frame_base(void) const {return mac_frame_base;}

	// Set Mac frame buffer base address (called from switch_to_mode())
	void set_mac_frame_base(uint32 base) {mac_frame_base = base;}

	// Get current video mode
	const video_mode &get_current_mode(void) const {return *current_mode;}

	// Get Apple mode id for given depth
	uint16 depth_to_apple_mode(video_depth depth) const {return apple_mode_for_depth[depth];}

	// Get current color depth
	uint16 get_apple_mode(void) const {return depth_to_apple_mode(current_mode->depth);}

	// Get bytes-per-row value for specified resolution/depth
	// (if the mode isn't supported, make a good guess)
	uint32 get_bytes_per_row(video_depth depth, uint32 id) const;

	// Check whether a mode with the specified depth exists on this display
	bool has_depth(video_depth depth) const;

	// Mac video driver functions
	int16 driver_open(void);
	int16 driver_control(uint16 code, uint32 param, uint32 dce);
	int16 driver_status(uint16 code, uint32 param);

protected:
	vector<video_mode> modes;                         // List of supported video modes
	vector<video_mode>::const_iterator current_mode;  // Currently selected video mode

	uint32 mac_frame_base;  // Mac frame buffer address for current mode

// Mac video driver per-display private variables/functions
private:
	// Check whether the specified resolution ID is one of the supported resolutions
	bool has_resolution(uint32 id) const;

	// Return iterator signifying "invalid mode"
	vector<video_mode>::const_iterator invalid_mode(void) const {return modes.end();}

	// Find specified mode (depth/resolution) (or invalid_mode() if not found)
	vector<video_mode>::const_iterator find_mode(uint16 apple_mode, uint32 id) const;

	// Find maximum supported depth for given resolution ID
	video_depth max_depth_of_resolution(uint32 id) const;

	// Get X/Y size of specified resolution
	void get_size_of_resolution(uint32 id, uint32 &x, uint32 &y) const;

	// Set palette to 50% gray
	void set_gray_palette(void);

	// Load gamma-corrected black-to-white ramp to palette for direct-color mode
	void load_ramp_palette(void);

	// Allocate gamma table of specified size
	bool allocate_gamma_table(int size);

	// Set gamma table (0 = build linear ramp)
	int16 set_gamma_table(uint32 user_table);

	// Switch video mode
	void switch_mode(vector<video_mode>::const_iterator it, uint32 param, uint32 dce);

	uint8 slot_id;               // NuBus slot ID number
	static uint8 next_slot_id;   // Next available slot ID

	uint8 palette[256 * 3];      // Color palette, 256 entries, RGB

	bool luminance_mapping;      // Luminance mapping on/off
	bool interrupts_enabled;     // VBL interrupts on/off
	bool dm_present;             // We received a GetVideoParameters call, so the Display Manager seems to be present

	uint32 gamma_table;          // Mac address of gamma table
	int alloc_gamma_table_size;  // Allocated size of gamma table

	uint16 current_apple_mode;   // Currently selected depth/resolution
	uint32 current_id;
	uint16 preferred_apple_mode; // Preferred depth/resolution
	uint32 preferred_id;

	uint32 slot_param;           // Mac address of Slot Manager parameter block

	// For compatibility reasons with older (pre-Display Manager) versions of
	// MacOS, the Apple modes must start at 0x80 and be contiguous. Therefore
	// we maintain an array to map the depth codes to the corresponding Apple
	// mode.
	uint16 apple_mode_for_depth[6];

// The following functions are implemented by platform-specific code
public:

	// Called by the video driver to switch the video mode on this display
	// (must call set_mac_frame_base())
	virtual void switch_to_current_mode(void) = 0;

	// Called by the video driver to set the color palette (in indexed modes)
	// or the gamma table (in direct modes)
	virtual void set_palette(uint8 *pal, int num) = 0;
};

// Vector of pointers to available monitor descriptions, filled by VideoInit()
extern vector<monitor_desc *> VideoMonitors;


extern int16 VideoDriverOpen(uint32 pb, uint32 dce);
extern int16 VideoDriverControl(uint32 pb, uint32 dce);
extern int16 VideoDriverStatus(uint32 pb, uint32 dce);


// System specific and internal functions/data
extern bool VideoInit(bool classic);
extern void VideoExit(void);

extern void VideoQuitFullScreen(void);

extern void VideoInterrupt(void);
extern void VideoRefresh(void);

#endif
