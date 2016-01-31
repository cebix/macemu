/*
 * UAE - The Un*x Amiga Emulator
 *
 * Memory management
 *
 * (c) 1995 Bernd Schmidt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>

#include "sysdeps.h"

#include "cpu_emulation.h"
#include "main.h"
#include "video.h"

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"

#if !REAL_ADDRESSING && !DIRECT_ADDRESSING

static bool illegal_mem = false;

#ifdef SAVE_MEMORY_BANKS
addrbank *mem_banks[65536];
#else
addrbank mem_banks[65536];
#endif

#ifdef WORDS_BIGENDIAN
# define swap_words(X) (X)
#else
# define swap_words(X) (((X) >> 16) | ((X) << 16))
#endif

#ifdef NO_INLINE_MEMORY_ACCESS
uae_u32 longget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).lget, addr);
}
uae_u32 wordget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).wget, addr);
}
uae_u32 byteget (uaecptr addr)
{
    return call_mem_get_func (get_mem_bank (addr).bget, addr);
}
void longput (uaecptr addr, uae_u32 l)
{
    call_mem_put_func (get_mem_bank (addr).lput, addr, l);
}
void wordput (uaecptr addr, uae_u32 w)
{
    call_mem_put_func (get_mem_bank (addr).wput, addr, w);
}
void byteput (uaecptr addr, uae_u32 b)
{
    call_mem_put_func (get_mem_bank (addr).bput, addr, b);
}
#endif

/* A dummy bank that only contains zeros */

static uae_u32 REGPARAM2 dummy_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM2 dummy_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM2 dummy_bget (uaecptr) REGPARAM;
static void REGPARAM2 dummy_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 dummy_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 dummy_bput (uaecptr, uae_u32) REGPARAM;

uae_u32 REGPARAM2 dummy_lget (uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal lget at %08lx\n", addr);

    return 0;
}

uae_u32 REGPARAM2 dummy_wget (uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal wget at %08lx\n", addr);

    return 0;
}

uae_u32 REGPARAM2 dummy_bget (uaecptr addr)
{
    if (illegal_mem)
	write_log ("Illegal bget at %08lx\n", addr);

    return 0;
}

void REGPARAM2 dummy_lput (uaecptr addr, uae_u32 l)
{
    if (illegal_mem)
	write_log ("Illegal lput at %08lx\n", addr);
}
void REGPARAM2 dummy_wput (uaecptr addr, uae_u32 w)
{
    if (illegal_mem)
	write_log ("Illegal wput at %08lx\n", addr);
}
void REGPARAM2 dummy_bput (uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Illegal bput at %08lx\n", addr);
}

/* Mac RAM (32 bit addressing) */

static uae_u32 REGPARAM2 ram_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 ram_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 ram_bget(uaecptr) REGPARAM;
static void REGPARAM2 ram_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 ram_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 ram_bput(uaecptr, uae_u32) REGPARAM;
static uae_u8* REGPARAM2 ram_xlate(uaecptr addr) REGPARAM;

static uae_u8* RAMBase;	// RAMBaseHost - RAMBaseMac

uae_u32 REGPARAM2 ram_lget(uaecptr addr)
{
    return do_get_mem_long(RAMBase + addr);
}

uae_u32 REGPARAM2 ram_wget(uaecptr addr)
{
    return do_get_mem_word(RAMBase + addr);
}

uae_u32 REGPARAM2 ram_bget(uaecptr addr)
{
    return *(RAMBase + addr);
}

void REGPARAM2 ram_lput(uaecptr addr, uae_u32 l)
{
    do_put_mem_long(RAMBase + addr, l);
}

void REGPARAM2 ram_wput(uaecptr addr, uae_u32 w)
{
    do_put_mem_word(RAMBase + addr, w);
}

void REGPARAM2 ram_bput(uaecptr addr, uae_u32 b)
{
	*(RAMBase + addr) = b;
}

uae_u8* REGPARAM2 ram_xlate(uaecptr addr)
{
    return RAMBase + addr;
}

/* Mac RAM (24 bit addressing) */

static uae_u32 REGPARAM2 ram24_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 ram24_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 ram24_bget(uaecptr) REGPARAM;
static void REGPARAM2 ram24_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 ram24_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 ram24_bput(uaecptr, uae_u32) REGPARAM;
static uae_u8* REGPARAM2 ram24_xlate(uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 ram24_lget(uaecptr addr)
{
    uae_u32* m = (uae_u32*)(RAMBase + (addr & 0xffffff));
    return do_get_mem_long(m);
}

uae_u32 REGPARAM2 ram24_wget(uaecptr addr)
{
    uae_u16* m = (uae_u16*)(RAMBase + (addr & 0xffffff));
    return do_get_mem_word(m);
}

uae_u32 REGPARAM2 ram24_bget(uaecptr addr)
{
    return *(RAMBase + (addr & 0xffffff));
}

void REGPARAM2 ram24_lput(uaecptr addr, uae_u32 l)
{
    uae_u32* m = (uae_u32*)(RAMBase + (addr & 0xffffff));
    do_put_mem_long(m, l);
}

void REGPARAM2 ram24_wput(uaecptr addr, uae_u32 w)
{
    uae_u16* m = (uae_u16*)(RAMBase + (addr & 0xffffff));
    do_put_mem_word(m, w);
}

void REGPARAM2 ram24_bput(uaecptr addr, uae_u32 b)
{
	*(RAMBase + (addr & 0xffffff)) = b;
}

uae_u8* REGPARAM2 ram24_xlate(uaecptr addr)
{
    return RAMBase + (addr & 0xffffff);
}

/* Mac ROM (32 bit addressing) */

static uae_u32 REGPARAM2 rom_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 rom_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 rom_bget(uaecptr) REGPARAM;
static void REGPARAM2 rom_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 rom_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 rom_bput(uaecptr, uae_u32) REGPARAM;
static uae_u8* REGPARAM2 rom_xlate(uaecptr addr) REGPARAM;

static uae_u8* ROMBase;	// ROMBaseHost - ROMBaseMac

uae_u32 REGPARAM2 rom_lget(uaecptr addr)
{
    return do_get_mem_long(ROMBase + addr);
}

uae_u32 REGPARAM2 rom_wget(uaecptr addr)
{
    return do_get_mem_word(ROMBase + addr);
}

uae_u32 REGPARAM2 rom_bget(uaecptr addr)
{
    return *(ROMBase + addr);
}

void REGPARAM2 rom_lput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Illegal ROM lput at %08lx\n", addr);
}

void REGPARAM2 rom_wput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Illegal ROM wput at %08lx\n", addr);
}

void REGPARAM2 rom_bput(uaecptr addr, uae_u32 b)
{
    if (illegal_mem)
	write_log ("Illegal ROM bput at %08lx\n", addr);
}

uae_u8 *REGPARAM2 rom_xlate(uaecptr addr)
{
    return ROMBase + addr;
}

/* Mac ROM (24 bit addressing) */

static uae_u32 REGPARAM2 rom24_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 rom24_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 rom24_bget(uaecptr) REGPARAM;
static uae_u8* REGPARAM2 rom24_xlate(uaecptr addr) REGPARAM;

uae_u32 REGPARAM2 rom24_lget(uaecptr addr)
{
    return do_get_mem_long(ROMBase + (addr & 0xffffff));
}

uae_u32 REGPARAM2 rom24_wget(uaecptr addr)
{
    return do_get_mem_word(ROMBase + (addr & 0xffffff));
}

uae_u32 REGPARAM2 rom24_bget(uaecptr addr)
{
    return *(ROMBase + (addr & 0xffffff));
}

uae_u8* REGPARAM2 rom24_xlate(uaecptr addr)
{
    return ROMBaseDiff + (addr & 0xffffff);
}

/* Frame buffer */

static uae_u32 REGPARAM2 frame_direct_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 frame_direct_wget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 frame_direct_bget(uaecptr) REGPARAM;
static void REGPARAM2 frame_direct_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 frame_direct_wput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 frame_direct_bput(uaecptr, uae_u32) REGPARAM;

static uae_u32 REGPARAM2 frame_host_555_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 frame_host_555_wget(uaecptr) REGPARAM;
static void REGPARAM2 frame_host_555_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 frame_host_555_wput(uaecptr, uae_u32) REGPARAM;

static uae_u32 REGPARAM2 frame_host_565_lget(uaecptr) REGPARAM;
static uae_u32 REGPARAM2 frame_host_565_wget(uaecptr) REGPARAM;
static void REGPARAM2 frame_host_565_lput(uaecptr, uae_u32) REGPARAM;
static void REGPARAM2 frame_host_565_wput(uaecptr, uae_u32) REGPARAM;

static uae_u32 REGPARAM2 frame_host_888_lget(uaecptr) REGPARAM;
static void REGPARAM2 frame_host_888_lput(uaecptr, uae_u32) REGPARAM;

static uae_u8* REGPARAM2 frame_xlate(uaecptr addr) REGPARAM;

static uae_u8* FrameBase;	// MacFrameBaseHost - MacFrameBaseMac

uae_u32 REGPARAM2 frame_direct_lget(uaecptr addr)
{
    return do_get_mem_long(FrameBase + addr);
}

uae_u32 REGPARAM2 frame_direct_wget(uaecptr addr)
{
    return do_get_mem_word(FrameBase + addr);
}

uae_u32 REGPARAM2 frame_direct_bget(uaecptr addr)
{
    return *(FrameBase + addr);
}

void REGPARAM2 frame_direct_lput(uaecptr addr, uae_u32 l)
{
    do_put_mem_long(FrameBase + addr, l);
}

void REGPARAM2 frame_direct_wput(uaecptr addr, uae_u32 w)
{
    do_put_mem_word(FrameBase + addr, w);
}

void REGPARAM2 frame_direct_bput(uaecptr addr, uae_u32 b)
{
    *(FrameBase + addr) = b;
}

uae_u32 REGPARAM2 frame_host_555_lget(uaecptr addr)
{
    uae_u32 l = *(uae_u32*)(FrameBase + addr);
	return swap_words(l);
}

uae_u32 REGPARAM2 frame_host_555_wget(uaecptr addr)
{
    return *(uae_u16*)(FrameBase + addr);
}

void REGPARAM2 frame_host_555_lput(uaecptr addr, uae_u32 l)
{
    uae_u32* m = (uae_u32*)(FrameBase + addr);
    *m = swap_words(l);
}

void REGPARAM2 frame_host_555_wput(uaecptr addr, uae_u32 w)
{
    uae_u16* m = (uae_u16*)(FrameBase + addr);
    *m = w;
}

uae_u32 REGPARAM2 frame_host_565_lget(uaecptr addr)
{
    uae_u32 l = *(uae_u32*)(FrameBase + addr);
    l = (l & 0x001f001f) | ((l >> 1) & 0x7fe07fe0);
    return swap_words(l);
}

uae_u32 REGPARAM2 frame_host_565_wget(uaecptr addr)
{
    uae_u16 w = *(uae_u16*)(FrameBase + addr);
    return (w & 0x1f) | ((w >> 1) & 0x7fe0);
}

void REGPARAM2 frame_host_565_lput(uaecptr addr, uae_u32 l)
{
    uae_u32* m = (uae_u32*)(FrameBase + addr);
    l = (l & 0x001f001f) | ((l << 1) & 0xffc0ffc0);
    *m = swap_words(l);
}

void REGPARAM2 frame_host_565_wput(uaecptr addr, uae_u32 w)
{
    uae_u16* m = (uae_u16*)(FrameBase + addr);
    *m = (w & 0x1f) | ((w << 1) & 0xffc0);
}

uae_u32 REGPARAM2 frame_host_888_lget(uaecptr addr)
{
    return *(uae_u32*)(FrameBase + addr);
}

void REGPARAM2 frame_host_888_lput(uaecptr addr, uae_u32 l)
{
    uae_u32* m = (uae_u32*)(MacFrameBaseHost + addr - MacFrameBaseMac);
    *m = l;
}

uae_u8* REGPARAM2 frame_xlate(uaecptr addr)
{
    return FrameBase + addr;
}

/* Default memory access functions */

uae_u8* REGPARAM2 default_xlate(uaecptr a)
{
    write_log("Your Mac program just did something terribly stupid\n");
    return NULL;
}

/* Address banks */

addrbank dummy_bank = {
    dummy_lget, dummy_wget, dummy_bget,
    dummy_lput, dummy_wput, dummy_bput,
    default_xlate
};

addrbank ram_bank = {
    ram_lget, ram_wget, ram_bget,
    ram_lput, ram_wput, ram_bput,
    ram_xlate
};

addrbank ram24_bank = {
    ram24_lget, ram24_wget, ram24_bget,
    ram24_lput, ram24_wput, ram24_bput,
    ram24_xlate
};

addrbank rom_bank = {
    rom_lget, rom_wget, rom_bget,
    rom_lput, rom_wput, rom_bput,
    rom_xlate
};

addrbank rom24_bank = {
    rom24_lget, rom24_wget, rom24_bget,
    rom_lput, rom_wput, rom_bput,
    rom24_xlate
};

addrbank frame_direct_bank = {
    frame_direct_lget, frame_direct_wget, frame_direct_bget,
    frame_direct_lput, frame_direct_wput, frame_direct_bput,
    frame_xlate
};

addrbank frame_host_555_bank = {
    frame_host_555_lget, frame_host_555_wget, frame_direct_bget,
    frame_host_555_lput, frame_host_555_wput, frame_direct_bput,
    frame_xlate
};

addrbank frame_host_565_bank = {
    frame_host_565_lget, frame_host_565_wget, frame_direct_bget,
    frame_host_565_lput, frame_host_565_wput, frame_direct_bput,
    frame_xlate
};

addrbank frame_host_888_bank = {
    frame_host_888_lget, frame_direct_wget, frame_direct_bget,
    frame_host_888_lput, frame_direct_wput, frame_direct_bput,
    frame_xlate
};

void memory_init(void)
{
	for(long i=0; i<65536; i++)
		put_mem_bank(i<<16, &dummy_bank);

	// Limit RAM size to not overlap ROM
	uint32 ram_size = RAMSize > ROMBaseMac ? ROMBaseMac : RAMSize;

	RAMBase = RAMBaseHost - RAMBaseMac;
	ROMBase = ROMBaseHost - ROMBaseMac;
	FrameBase = MacFrameBaseHost - MacFrameBaseMac;

	// Map RAM and ROM
	if (TwentyFourBitAddressing) {
		map_banks(&ram24_bank, RAMBaseMac >> 16, ram_size >> 16);
		map_banks(&rom24_bank, ROMBaseMac >> 16, ROMSize >> 16);
	} else {
		map_banks(&ram_bank, RAMBaseMac >> 16, ram_size >> 16);
		map_banks(&rom_bank, ROMBaseMac >> 16, ROMSize >> 16);
	}

	// Map frame buffer
	switch (MacFrameLayout) {
		case FLAYOUT_DIRECT:
			map_banks(&frame_direct_bank, MacFrameBaseMac >> 16, (MacFrameSize >> 16) + 1);
			break;
		case FLAYOUT_HOST_555:
			map_banks(&frame_host_555_bank, MacFrameBaseMac >> 16, (MacFrameSize >> 16) + 1);
			break;
		case FLAYOUT_HOST_565:
			map_banks(&frame_host_565_bank, MacFrameBaseMac >> 16, (MacFrameSize >> 16) + 1);
			break;
		case FLAYOUT_HOST_888:
			map_banks(&frame_host_888_bank, MacFrameBaseMac >> 16, (MacFrameSize >> 16) + 1);
			break;
	}
}

void map_banks(addrbank *bank, int start, int size)
{
    int bnr;
    unsigned long int hioffs = 0, endhioffs = 0x100;

    if (start >= 0x100) {
	for (bnr = start; bnr < start + size; bnr++)
	    put_mem_bank (bnr << 16, bank);
	return;
    }
    if (TwentyFourBitAddressing) endhioffs = 0x10000;
    for (hioffs = 0; hioffs < endhioffs; hioffs += 0x100)
	for (bnr = start; bnr < start+size; bnr++)
	    put_mem_bank((bnr + hioffs) << 16, bank);
}

#endif /* !REAL_ADDRESSING && !DIRECT_ADDRESSING */

