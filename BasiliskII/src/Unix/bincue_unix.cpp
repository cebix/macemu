/*
 *  Copyright (C) 2002-2010  The DOSBox Team
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Geoffrey Brown 2010
 * Includes ideas from dosbox src/dos/cdrom_image.cpp 
 *
 * Limitations:	1) cue files must reference single bin file
 *              2) only supports raw mode1 data and audio
 *              3) no support for audio flags
 *              4) requires SDL audio or OS X core audio
 *              5) limited cue file keyword support
 *
 * Creating cue/bin files:
 * 	cdrdao read-cd --read-raw --paranoia 3 foo.toc
 *  toc2cue foo.toc
 */

#include "sysdeps.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef OSX_CORE_AUDIO
#include "../MacOSX/MacOSX_sound_if.h"
static int bincue_core_audio_callback(void);
#endif

#ifdef USE_SDL_AUDIO
#include <SDL.h>
#include <SDL_audio.h>
#endif

#include "bincue_unix.h"
#define DEBUG 0
#include "debug.h"

#define MAXTRACK 100
#define MAXLINE 512
#define CD_FRAMES 75
#define RAW_SECTOR_SIZE		2352
#define COOKED_SECTOR_SIZE	2048

// Bits of Track Control Field -- These are standard for scsi cd players

#define PREMPHASIS 0x1
#define COPY	   0x2
#define DATA	   0x4
#define AUDIO	   0
#define FOURTRACK  0x8

// Audio status -- These are standard for scsi cd players

#define CDROM_AUDIO_INVALID    0x00
#define CDROM_AUDIO_PLAY       0x11
#define CDROM_AUDIO_PAUSED     0x12
#define CDROM_AUDIO_COMPLETED  0x13
#define CDROM_AUDIO_ERROR      0x14
#define CDROM_AUDIO_NO_STATUS  0x15

typedef unsigned char uint8;

// cuefiles can be challenging as some information is
// implied.  For example, there may a pregap (also postgap)
// of silence that must be generated.  Here we implement
// only the pregap.

typedef struct {
	int number;
	unsigned int start;	// Track start in frames
	unsigned int length;	// Track length in frames
	loff_t fileoffset;		// Track frame start within file
	unsigned int pregap;	// Silence in frames to generate
	unsigned char tcf;		// Track control field
} Track;

typedef struct {
	char *binfile;			// Binary file name
	unsigned int length;	// file length in frames
	int binfh;				// binary file handle
	int tcnt;				// number of tracks
	Track tracks[MAXTRACK];
} CueSheet;

typedef struct {
	CueSheet *cs;				// cue sheet to play from
	int audiofh;				// file handle for audio data
	unsigned int audioposition; // current position from audiostart (bytes)
	unsigned int audiostart;	// start position if playing (frame)
	unsigned int audioend;		// end position if playing (frames)
	unsigned int silence;		// pregap (silence) bytes
	unsigned char audiostatus;	// See defines above for status
	loff_t fileoffset;			// offset from file beginning to audiostart
#ifdef OSX_CORE_AUDIO
	OSXsoundOutput soundoutput;
#endif
} CDPlayer;

// Minute,Second,Frame data type

typedef struct {
	int m, s, f; // note size matters since we scan for %d !
} MSF;

// Parser State

static unsigned int totalPregap;
static unsigned int prestart;

// Audio System State

static bool audio_enabled = false;
static uint8 silence_byte;


// CD Player state.  Note only one player is supported !

static CDPlayer player;

static void FramesToMSF(unsigned int frames, MSF *msf)
{
	msf->m = frames/(60 * CD_FRAMES);
	frames = frames%(60 * CD_FRAMES);
	msf->s = frames/CD_FRAMES;
	msf->f = frames%CD_FRAMES;
}

static int MSFToFrames(MSF msf)
{
	return (msf.m * 60 * CD_FRAMES) + (msf.s * CD_FRAMES) + msf.f;
}


static int PositionToTrack(CueSheet *cs, unsigned int position)
{
	int i;
	MSF msf;

	FramesToMSF(position, &msf);

	for (i = 0; i < cs->tcnt; i++) {
		if ((position >= cs->tracks[i].start) &&
			(position <= (cs->tracks[i].start + cs->tracks[i].length)))
			break;
	}
	return i;
}

static bool AddTrack(CueSheet *cs)
{
	int skip = prestart;
	Track *prev;
	Track *curr = &(cs->tracks[cs->tcnt]);

	prestart = 0;

	if (skip > 0) {
		if (skip > curr->start) {
			D(bug("AddTrack: prestart > start\n"));
			return false;
		}
	}

	curr->fileoffset = curr->start * RAW_SECTOR_SIZE;

	// now we patch up the indicated time

	curr->start += totalPregap;

	// curr->pregap is supposed to be part of this track, but it
	// must be generated as silence

	totalPregap += curr->pregap;

	if (cs->tcnt == 0) {
		if (curr->number != 1) {
			D(bug("AddTrack: number != 1\n"));
			return false;
		}
		cs->tcnt++;
		return true;
	}

	prev = &(cs->tracks[cs->tcnt - 1]);

	if (prev->start < skip)
		prev->length = skip - prev->start - curr->pregap;
	else
		prev->length = curr->start - prev->start - curr->pregap;

	// error checks

	if (curr->number <= 1) {
		D(bug("Bad track number %d\n", curr->number));
		return false;
	}
	if ((prev->number + 1 != curr->number) && (curr->number != 0xAA)) {
		D(bug("Bad track number %d\n", curr->number));
		return false;
	}
	if (curr->start < prev->start + prev->length) {
		D(bug("unexpected start %d\n", curr->start));
		return false;
	}

	cs->tcnt++;
	return true;
}

static bool ParseCueSheet(FILE *fh, CueSheet *cs, const char *cuefile)
{
	bool seen1st = false;
	char line[MAXLINE];
	unsigned int i_line=0;
	char *keyword;
	
	totalPregap = 0;
	prestart = 0;

	while (fgets(line, MAXLINE, fh) != NULL) {
		Track *curr = &cs->tracks[cs->tcnt];

		// check for CUE file

		if (!i_line && (strncmp("FILE", line, 4) != 0)) {
			return false;
		}
		i_line++;

		// extract keyword

		if (NULL != (keyword = strtok(line, " \t\n\t"))) {
			if (!strcmp("FILE", keyword)) {
				char *filename;
				char *filetype;

				if (i_line > 1) {
					D(bug("More than one FILE token\n"));
					goto fail;	
				}	
				filename = strtok(NULL, "\"\t\n\r");
				filetype = strtok(NULL, " \"\t\n\r");
				if (strcmp("BINARY", filetype)) {
					D(bug("Not binary file %s", filetype));
					goto fail;
				}
				else {
					char *tmp = strdup(cuefile);
					char *b = dirname(tmp);
					cs->binfile = (char *) malloc(strlen(b) + strlen(filename) + 2);
					sprintf(cs->binfile, "%s/%s", b, filename);
					free(tmp);
				}
			} else if (!strcmp("TRACK", keyword)) {
				char *field;
				int i_track;

				if (seen1st) {
					if (!AddTrack(cs)){
						D(bug("AddTrack failed \n"));
						goto fail;
					}
					curr = &cs->tracks[cs->tcnt];
				}

				seen1st = true;

				// parse track number

				field = strtok(NULL, " \t\n\r");
				if (1 != sscanf(field, "%d", &i_track)) {
					D(bug("Expected  track number\n"));
					goto fail;		
				}
				curr->number = i_track;

				// parse track type

				field = strtok(NULL, " \t\n\r");
				if (!strcmp("MODE1/2352", field)) {
					curr->tcf = DATA;
				} else if (!strcmp("AUDIO", field)) {
					curr->tcf = AUDIO;
				} else {
					D(bug("Unexpected track type %s", field));
					goto fail;
				}

			} else if (!strcmp("INDEX", keyword)) {
				char *field;
				int i_index;
				MSF msf;

				// parse INDEX number

				field = strtok(NULL, " \t\n\r");
				if (1 != sscanf(field, "%d", &i_index)) {
					D(bug("Expected index number"));
					goto fail;
				}

				// parse INDEX start

				field = strtok(NULL, " \t\n\r");
				if (3 != sscanf(field, "%d:%d:%d", 
								 &msf.m, &msf.s, &msf.f)) {
					D(bug("Expected index start frame\n"));
					goto fail;
				}

				if (i_index == 1)
					curr->start = MSFToFrames(msf);
				else if (i_index == 0)
					prestart = MSFToFrames(msf);
			} else if (!strcmp("PREGAP", keyword)) {
				MSF msf;
				char *field = strtok(NULL, " \t\n\r");
				if (3 != sscanf(field, "%d:%d:%d", 
								 &msf.m, &msf.s, &msf.f)) {
					D(bug("Expected pregap frame\n"));
					goto fail;	
				}
				curr->pregap = MSFToFrames(msf);

				// Ignored directives

			} else if (!strcmp("TITLE", keyword)) {
			} else if (!strcmp("PERFORMER", keyword)) {
			} else if (!strcmp("REM", keyword)) {
			} else if (!strcmp("ISRC", keyword)) {
			} else if (!strcmp("SONGWRITER", keyword)) {
			} else {
				D(bug("Unexpected keyword %s\n", keyword));
				goto fail;		
			}
		}
	}

	AddTrack(cs); // add final track
	return true;
  fail:
	return false;
}

static bool LoadCueSheet(const char *cuefile, CueSheet *cs)
{
	FILE *fh = NULL;
	int binfh = -1;
	struct stat buf;
	Track *tlast = NULL;

	if (cs) {
		bzero(cs, sizeof(*cs));
		if (!(fh = fopen(cuefile, "r")))
			return false;

		if (!ParseCueSheet(fh, cs, cuefile)) goto fail;

		// Open bin file and find length

		if ((binfh = open(cs->binfile,O_RDONLY)) < 0) {
			D(bug("Can't read bin file %s\n", cs->binfile));
			goto fail;
		}

		if (fstat(binfh, &buf)) {
			D(bug("fstat returned error\n"));
			goto fail;
		}

		// compute length of final track


		tlast = &cs->tracks[cs->tcnt - 1];
		tlast->length = buf.st_size/RAW_SECTOR_SIZE 
						- tlast->start + totalPregap;

		if (tlast->length < 0) {
			D(bug("Binary file too short \n"));
 		  	goto fail;	
   	    }

		// save bin file length and pointer

		cs->length = buf.st_size/RAW_SECTOR_SIZE;
		cs->binfh = binfh;

		fclose(fh);
		return true;

	  fail:
		if (binfh >= 0)
			close(binfh);	
		fclose(fh);
		free(cs->binfile);
		return false;

    }
	return false;
}



void *open_bincue(const char *name)
{
	CueSheet *cs;

	if (player.cs == NULL) {
		cs = (CueSheet *) malloc(sizeof(CueSheet));
		if (!cs) {
			D(bug("malloc failed\n"));
			return NULL;
		}

		if (LoadCueSheet(name, cs)) {
			player.cs = cs;
#ifdef OSX_CORE_AUDIO
			audio_enabled = true;
#endif
			if (audio_enabled)
				player.audiostatus = CDROM_AUDIO_NO_STATUS;
			else
				player.audiostatus = CDROM_AUDIO_INVALID;
			player.audiofh = dup(cs->binfh);
			return cs;
		}
		else
			free(cs);
	}
	return NULL;
}

void close_bincue(void *fh)
{


}

/*
 * File read (cooked)
 * Data are stored in raw sectors of which only COOKED_SECTOR_SIZE
 * bytes are valid -- the remaining include 16 bytes at the beginning
 * of each raw sector and RAW_SECTOR_SIZE - COOKED_SECTOR_SIZE - bytes
 * at the end
 *
 * We assume that a read request can land in the middle of
 * sector.  We compute the byte address of that sector (sec)
 * and the offset of the first byte we want within that sector (secoff)
 *
 * Reading is performed one raw sector at a time, extracting as many
 * valid bytes as possible from that raw sector (available)
 */

size_t read_bincue(void *fh, void *b, loff_t offset, size_t len)
{
	size_t bytes_read = 0;						// bytes read so far
	unsigned char *buf = (unsigned char *) b;	// target buffer
	unsigned char secbuf[RAW_SECTOR_SIZE];		// temporary buffer

	off_t sec = ((offset/COOKED_SECTOR_SIZE) * RAW_SECTOR_SIZE);
	off_t secoff = offset % COOKED_SECTOR_SIZE;

	// sec contains location (in bytes) of next raw sector to read
	// secoff contains offset within that sector at which to start
	// reading since we can request a read that starts in the middle
	// of a sector

	CueSheet *cs = (CueSheet *) fh;

	if (cs == NULL || lseek(cs->binfh, sec, SEEK_SET) < 0) {
		return -1;
	}
	while (len) {

		// bytes available in next raw sector or len (bytes)
		// we want whichever is less

		size_t available = COOKED_SECTOR_SIZE - secoff;
		available = (available > len) ? len : available;

		// read the next raw sector

		if (read(cs->binfh, secbuf, RAW_SECTOR_SIZE) != RAW_SECTOR_SIZE) {
			return bytes_read;
		}

		// copy cooked sector bytes (skip first 16)
		// we want out of those available

		bcopy(&secbuf[16+secoff], &buf[bytes_read], available);

		// next sector we start at the beginning

		secoff = 0;

		// increment running count decrement request

		bytes_read += available;
		len -= available;
	}
	return bytes_read;
}

loff_t size_bincue(void *fh)
{
	if (fh) {
		return ((CueSheet *)fh)->length * COOKED_SECTOR_SIZE;
	}
}

bool readtoc_bincue(void *fh, unsigned char *toc)
{
	CueSheet *cs = (CueSheet *) fh;
	if (cs) {

		MSF msf;
		unsigned char *p = toc + 2;
		*p++ = cs->tracks[0].number;
		*p++ = cs->tracks[cs->tcnt - 1].number;
		for (int i = 0; i < cs->tcnt; i++) {

			FramesToMSF(cs->tracks[i].start, &msf);
			*p++ = 0;
			*p++ = 0x10 | cs->tracks[i].tcf;
			*p++ = cs->tracks[i].number;
			*p++ = 0;
			*p++ = 0;
			*p++ = msf.m;
			*p++ = msf.s;
			*p++ = msf.f;
		}
		FramesToMSF(cs->length, &msf);
		*p++ = 0;
		*p++ = 0x14;
		*p++ = 0xAA;
		*p++ = 0;
		*p++ = 0;
		*p++ = msf.m;
		*p++ = msf.s;
		*p++ = msf.f;

		int toc_size = p - toc;
		*toc++ = toc_size >> 8;
		*toc++ = toc_size & 0xff;
		return true;
	}
}

bool GetPosition_bincue(void *fh, uint8 *pos)
{
	CueSheet *cs = (CueSheet *) fh;
	if (cs && player.cs == cs) {
		MSF abs, rel;
		int fpos = player.audioposition / RAW_SECTOR_SIZE + player.audiostart;
		int trackno = PositionToTrack(cs, fpos);

		if (!audio_enabled)
			return false;

		FramesToMSF(fpos, &abs);
		if (trackno < cs->tcnt) {
			// compute position relative to start of frame

			unsigned int position =  player.audioposition/RAW_SECTOR_SIZE +
				player.audiostart - player.cs->tracks[trackno].start;

			FramesToMSF(position, &rel);
		}
		else
			FramesToMSF(0, &rel);

		*pos++ = 0;
		*pos++ = player.audiostatus;
		*pos++ = 0;
		*pos++ = 12; // Sub-Q data length
		*pos++ = 0;
		if (trackno < cs->tcnt)
			*pos++ = 0x10 | cs->tracks[trackno].tcf;
		*pos++ = (trackno < cs->tcnt) ? cs->tracks[trackno].number : 0xAA;
		*pos++ = 1;  // track index
		*pos++ = 0;
		*pos++ = abs.m;
		*pos++ = abs.s;
		*pos++ = abs.f;
		*pos++ = 0;
		*pos++ = rel.m;
		*pos++ = rel.s;
		*pos++ = rel.f;
		*pos++ = 0;
//		D(bug("CDROM position %02d:%02d:%02d track %02d\n", abs.m, abs.s, abs.f, trackno));
		return true;
	}
	else
		return false;
}

bool CDPause_bincue(void *fh)
{
	CueSheet *cs = (CueSheet *) fh;
	if (cs && cs == player.cs) {
		if (player.audiostatus == CDROM_AUDIO_PLAY) {
			player.audiostatus = CDROM_AUDIO_PAUSED;
			return true;
		}
	}
	return false;
}

bool CDStop_bincue(void *fh)
{
	CueSheet *cs = (CueSheet *) fh;

	if (cs && cs == player.cs) {
#ifdef OSX_CORE_AUDIO
		player.soundoutput.stop();
#endif
		if (player.audiostatus != CDROM_AUDIO_INVALID)
			player.audiostatus = CDROM_AUDIO_NO_STATUS;
		return true;
	}
	return false;
}

bool CDResume_bincue(void *fh)
{
	CueSheet *cs = (CueSheet *) fh;
	if (cs && cs == player.cs) {
		if (player.audiostatus == CDROM_AUDIO_PAUSED) {
			player.audiostatus = CDROM_AUDIO_PLAY;
			return true;
		}
	}
	return false;
}

bool CDPlay_bincue(void *fh, uint8 start_m, uint8 start_s, uint8 start_f,
				   uint8 end_m, uint8 end_s, uint8 end_f)
{
	CueSheet *cs = (CueSheet *)fh;
	if (cs && cs == player.cs) {
		int track;
		MSF msf;

#ifdef USE_SDL_AUDIO
		SDL_LockAudio();
#endif

		player.audiostatus = CDROM_AUDIO_NO_STATUS;

		player.audiostart = (start_m * 60 * CD_FRAMES) +
			(start_s * CD_FRAMES) + start_f;
		player.audioend	= (end_m * 60 * CD_FRAMES) + (end_s * CD_FRAMES) + end_f;

		track = PositionToTrack(player.cs, player.audiostart);

		if (track < player.cs->tcnt) {
			player.audioposition = 0;

			// here we need to compute silence

			if (player.audiostart - player.cs->tracks[track].start >
				player.cs->tracks[track].pregap)
				player.silence = 0;
			else
				player.silence = (player.cs->tracks[track].pregap -
								  player.audiostart +
								  player.cs->tracks[track].start) * RAW_SECTOR_SIZE;

			player.fileoffset = player.cs->tracks[track].fileoffset;

			D(bug("file offset %d\n", (unsigned int) player.fileoffset));

			// fix up file offset if beyond the silence bytes

			if (!player.silence) // not at the beginning
				player.fileoffset += (player.audiostart - 
									  player.cs->tracks[track].start -
									  player.cs->tracks[track].pregap) * RAW_SECTOR_SIZE;

			FramesToMSF(player.cs->tracks[track].start, &msf);
			D(bug("CDPlay_bincue track %02d start %02d:%02d:%02d silence %d",
				player.cs->tracks[track].number, msf.m, msf.s, msf.f,
				player.silence/RAW_SECTOR_SIZE));
			D(bug(" Stop %02u:%02u:%02u\n", end_m, end_s, end_f));
		}
		else
			D(bug("CDPlay_bincue: play beyond last track !\n"));

#ifdef USE_SDL_AUDIO
		SDL_UnlockAudio();
#endif

		if (audio_enabled) {
			player.audiostatus = CDROM_AUDIO_PLAY;
#ifdef OSX_CORE_AUDIO
			D(bug("starting os x sound"));
			player.soundoutput.setCallback(bincue_core_audio_callback);
			// should be from current track !
			player.soundoutput.start(16, 2, 44100);
#endif
			return true;
		}
	}
	return false;
}

static uint8 *fill_buffer(int stream_len)
{
	static uint8 *buf = 0;
	static int bufsize = 0;
	int offset = 0;

	if (bufsize < stream_len) {
		free(buf);
		buf = (uint8 *) malloc(stream_len);
		if (buf) {
			bufsize = stream_len;
		}
		else {
			D(bug("malloc failed \n"));
			return NULL;
		}
	}

	memset(buf, silence_byte, stream_len);

	if (player.audiostatus == CDROM_AUDIO_PLAY) {
		int remaining_silence = player.silence - player.audioposition;

		if (player.audiostart + player.audioposition/RAW_SECTOR_SIZE
			>= player.audioend) {
			player.audiostatus = CDROM_AUDIO_COMPLETED;
			return buf;
		}

		if (remaining_silence >= stream_len) {
			player.audioposition += stream_len;
			return buf;
		}

		if (remaining_silence > 0) {
			offset += remaining_silence;
			player.audioposition += remaining_silence;
		}

		int ret = 0;
		int available = ((player.audioend - player.audiostart) *
							 RAW_SECTOR_SIZE) - player.audioposition;
		if (available > (stream_len - offset))
			available = stream_len - offset;

		if (lseek(player.audiofh,
				  player.fileoffset + player.audioposition - player.silence,
					  SEEK_SET) < 0)
			return NULL;

		if (available < 0) {
			player.audioposition += available; // correct end !;
			available = 0;
		}

		if ((ret = read(player.audiofh, &buf[offset], available)) >= 0) {
			player.audioposition += ret;
			offset += ret;
			available -= ret;
		}

		while (offset < stream_len) {
			buf[offset++] = silence_byte;
			if (available-- > 0){
				player.audioposition++;
			}
		}
	}		
	return buf;
}


#ifdef USE_SDL_AUDIO
void MixAudio_bincue(uint8 *stream, int stream_len)
{
	uint8 *buf;
	if (audio_enabled && (player.audiostatus == CDROM_AUDIO_PLAY)) {
		if (buf = fill_buffer(stream_len))
			SDL_MixAudio(stream, buf, stream_len, SDL_MIX_MAXVOLUME);
	}
}

void OpenAudio_bincue(int freq, int format, int channels, uint8 silence)
{
	if (freq == 44100 && format == AUDIO_S16MSB && channels == 2) {
		audio_enabled = true;
		silence_byte = silence;
	}
	else {
		D(bug("unexpected frequency %d , format %d, or channels %d\n",
			  freq, format, channels));
	}
}
#endif

#ifdef OSX_CORE_AUDIO
static int bincue_core_audio_callback(void)
{
	int frames = player.soundoutput.bufferSizeFrames();
	uint8 *buf = fill_buffer(frames*4);

	//  D(bug("Audio request %d\n", stream_len));

	player.soundoutput.sendAudioBuffer((void *) buf, (buf ? frames : 0));

	return 1;
}
#endif
