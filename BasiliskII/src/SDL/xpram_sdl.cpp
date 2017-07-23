/*
 *  xpram_sdl.cpp - XPRAM handling, SDL implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "xpram.h"


// XPRAM file name and path
const char XPRAM_FILE_NAME[] = ".basilisk_ii_xpram";


/*
 *  Load XPRAM from settings file
 */

void LoadXPRAM(const char *dir)
{
	// Build a full-path to the file
	char full_path[4096];
	if (!dir) {
		dir = SDL_getenv("HOME");
	}
	if (!dir) {
		dir = "./";
	}
	SDL_snprintf(full_path, sizeof(full_path), "%s/%s", dir, XPRAM_FILE_NAME);
	
	// Open the XPRAM file
	FILE *f = fopen(full_path, "rb");
	if (f != NULL) {
		fread(XPRAM, 256, 1, f);
		fclose(f);
	}
}


/*
 *  Save XPRAM to settings file
 */

void SaveXPRAM(void)
{
	// Build a full-path to the file
	char full_path[4096];
	const char *dir = SDL_getenv("HOME");
	if (!dir) {
		dir = "./";
	}
	SDL_snprintf(full_path, sizeof(full_path), "%s/%s", dir, XPRAM_FILE_NAME);

	// Save the XPRAM file
	FILE *f = fopen(XPRAM_FILE_NAME, "wb");
	if (f != NULL) {
		fwrite(XPRAM, 256, 1, f);
		fclose(f);
	}
}


/*
 *  Delete PRAM file
 */

void ZapPRAM(void)
{
	// Build a full-path to the file
	char full_path[4096];
	const char *dir = SDL_getenv("HOME");
	if (!dir) {
		dir = "./";
	}
	SDL_snprintf(full_path, sizeof(full_path), "%s/%s", dir, XPRAM_FILE_NAME);

	// Delete the XPRAM file
	remove(full_path);
}
