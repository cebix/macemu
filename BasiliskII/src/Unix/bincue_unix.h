/*
 *  bincue_unix.h -- support for cdrom image files in bin/cue format
 *
 *  (C) 2010 Geoffrey Brown
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

#ifndef BINCUE_H
#define BINCUE_H

extern void *open_bincue(const char *name);
extern bool readtoc_bincue(void *, uint8 *);
extern size_t read_bincue(void *, void *, loff_t,  size_t);
extern loff_t size_bincue(void *);
extern void close_bincue(void *);

extern bool GetPosition_bincue(void *, uint8 *);

extern bool CDPlay_bincue(void *, uint8, uint8,
						  uint8, uint8, uint8, uint8);
extern bool CDPause_bincue(void *);
extern bool CDResume_bincue(void *);
extern bool CDStop_bincue(void *);

#ifdef USE_SDL_AUDIO
extern void OpenAudio_bincue(int, int, int, uint8);
extern void MixAudio_bincue(uint8 *, int);
#endif

#endif
