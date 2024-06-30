/*
 *	utils_macosx.h - Mac OS X utility functions.
 *
 *  Copyright (C) 2011 Alexei Svitkine
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

#ifndef UTILS_MACOSX_H
#define UTILS_MACOSX_H

#ifdef USE_SDL
#if SDL_VERSION_ATLEAST(2,0,0)
void disable_SDL2_macosx_menu_bar_keyboard_shortcuts();
bool is_fullscreen_osx(SDL_Window * window);
#endif
#endif

void set_menu_bar_visible_osx(bool visible);

void set_current_directory();

bool MetalIsAvailable();

void make_window_transparent(SDL_Window *window);
void set_mouse_ignore(SDL_Window *window, int flag);

#endif
