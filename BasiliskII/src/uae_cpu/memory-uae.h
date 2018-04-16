/*
 * memory.h - memory management
 *
 * Copyright (c) 2001-2006 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 /*
  * UAE - The Un*x Amiga Emulator
  *
  * memory management
  *
  * Copyright 1995 Bernd Schmidt
  */

#ifndef UAE_MEMORY_H
#define UAE_MEMORY_H

#include "sysdeps.h"
#include "string.h"
#include "hardware.h"
#include "parameters.h"
#include "registers.h"
#include "cpummu.h"
#include "readcpu.h"

# include <csetjmp>

// newcpu.h
extern void Exception (int, uaecptr);
#ifdef EXCEPTIONS_VIA_LONGJMP
	extern JMP_BUF excep_env;
	#define SAVE_EXCEPTION \
		JMP_BUF excep_env_old; \
		memcpy(excep_env_old, excep_env, sizeof(JMP_BUF))
	#define RESTORE_EXCEPTION \
		memcpy(excep_env, excep_env_old, sizeof(JMP_BUF))
	#define TRY(var) int var = SETJMP(excep_env); if (!var)
	#define CATCH(var) else
	#define THROW(n) LONGJMP(excep_env, n)
	#define THROW_AGAIN(var) LONGJMP(excep_env, var)
	#define VOLATILE volatile
#else
	struct m68k_exception {
		int prb;
		m68k_exception (int exc) : prb (exc) {}
		operator int() { return prb; }
	};
	#define SAVE_EXCEPTION
	#define RESTORE_EXCEPTION
	#define TRY(var) try
	#define CATCH(var) catch(m68k_exception var)
	#define THROW(n) throw m68k_exception(n)
	#define THROW_AGAIN(var) throw
	#define VOLATILE
#endif /* EXCEPTIONS_VIA_LONGJMP */
extern int in_exception_2;

#define STRAM_END	0x0e00000UL	// should be replaced by global ROMBase as soon as ROMBase will be a constant
#define ROM_END		0x0e80000UL	// should be replaced by ROMBase + RealROMSize if we are going to work with larger TOS ROMs than 512 kilobytes
#define FastRAM_BEGIN	0x1000000UL	// should be replaced by global FastRAMBase as soon as FastRAMBase will be a constant
#ifdef FixedSizeFastRAM
#define FastRAM_SIZE	(FixedSizeFastRAM * 1024 * 1024)
#else
#define FastRAM_SIZE	FastRAMSize
#endif

#ifdef FIXED_VIDEORAM
#define ARANYMVRAMSTART 0xf0000000UL
#endif

#define ARANYMVRAMSIZE	0x00100000	// should be a variable to protect VGA card offscreen memory

#ifdef FIXED_VIDEORAM
extern uintptr VMEMBaseDiff;
#else
extern uae_u32 VideoRAMBase;
#endif

#ifdef ARAM_PAGE_CHECK
extern uaecptr pc_page, read_page, write_page;
extern uintptr pc_offset, read_offset, write_offset;
# ifdef PROTECT2K
#  define ARAM_PAGE_MASK 0x7ff
# else
#  ifdef FULLMMU
#   define ARAM_PAGE_MASK 0xfff
#  else
#   define ARAM_PAGE_MASK 0xfffff
#  endif
# endif
#endif

extern uintptr MEMBaseDiff;
extern uintptr ROMBaseDiff;
extern uintptr FastRAMBaseDiff;
# define InitMEMBaseDiff(va, ra)	(MEMBaseDiff = (uintptr)(va) - (uintptr)(ra))
# define InitROMBaseDiff(va, ra)        (ROMBaseDiff = (uintptr)(va) - (uintptr)(ra))
# define InitFastRAMBaseDiff(va, ra)        (FastRAMBaseDiff = (uintptr)(va) - (uintptr)(ra))

#ifdef FIXED_VIDEORAM
#define InitVMEMBaseDiff(va, ra)	(VMEMBaseDiff = (uintptr)(va) - (uintptr)(ra))
#else
#define InitVMEMBaseDiff(va, ra)        (ra = (uintptr)(va) + MEMBaseDiff)
#endif

extern "C" void breakpt(void);


static inline uae_u64 do_get_mem_quad(uae_u64 *a) {return SDL_SwapBE64(*a);}
static inline void do_put_mem_quad(uae_u64 *a, uae_u64 v) {*a = SDL_SwapBE64(v);}


#ifndef NOCHECKBOUNDARY
static ALWAYS_INLINE bool test_ram_boundary(uaecptr addr, int size, bool super, bool write)
{
	if (addr <= (FastRAM_BEGIN + FastRAM_SIZE - size)) {
#ifdef PROTECT2K
		// protect first 2kB of RAM - access in supervisor mode only
		if (!super && addr < 0x00000800UL)
			return false;
#endif
		// check for write access to protected areas:
		// - first two longwords of ST-RAM are non-writable (ROM shadow)
		// - non-writable area between end of ST-RAM and begin of FastRAM
		if (!write || addr >= FastRAM_BEGIN || (addr >= 8 && addr <= (STRAM_END - size)))
			return true;
	}
#ifdef FIXED_VIDEORAM
	return addr >= ARANYMVRAMSTART && addr <= (ARANYMVRAMSTART + ARANYMVRAMSIZE - size);
#else
	return addr >= VideoRAMBase && addr <= (VideoRAMBase + ARANYMVRAMSIZE - size);
#endif
}
/*
 * "size" is the size of the memory access (byte = 1, word = 2, long = 4)
 */
static ALWAYS_INLINE void check_ram_boundary(uaecptr addr, int size, bool write)
{
	if (test_ram_boundary(addr, size, regs.s, write))
		return;

	// D(bug("BUS ERROR %s at $%x\n", (write ? "writing" : "reading"), addr));
	regs.mmu_fault_addr = addr;
	regs.mmu_ssw = ((size & 3) << 5) | (write ? 0 : (1 << 8));
	breakpt();
	THROW(2);
}

#else
static inline bool test_ram_boundary(uaecptr, int, bool, bool) { return 1; }
static inline void check_ram_boundary(uaecptr, int, bool) { }
#endif

#ifdef FIXED_VIDEORAM
# define do_get_real_address(a)		((uae_u8 *)(((uaecptr)(a) < ARANYMVRAMSTART) ? ((uaecptr)(a) + MEMBaseDiff) : ((uaecptr)(a) + VMEMBaseDiff)))
#else
# define do_get_real_address(a)		((uae_u8 *)((uintptr)(a) + MEMBaseDiff))
#endif

static inline uae_u8 *phys_get_real_address(uaecptr addr)
{
    return do_get_real_address(addr);
}

#ifndef NOCHECKBOUNDARY
static inline bool phys_valid_address(uaecptr addr, bool write, int sz)
{
	return test_ram_boundary(addr, sz, regs.s, write);
}
#else
static inline bool phys_valid_address(uaecptr, bool, int) { return true; }
#endif

static inline uae_u64 phys_get_quad(uaecptr addr)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ read_page) <= ARAM_PAGE_MASK))
        return do_get_mem_quad((uae_u64*)(addr + read_offset));
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) return HWget_l(addr); /* TODO: must be HWget_q */
#endif
    check_ram_boundary(addr, 8, false);
    uae_u64 * const m = (uae_u64 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    read_page = addr;
    read_offset = (uintptr)m - (uintptr)addr;
#endif
    return do_get_mem_quad(m);
}

static inline uae_u32 phys_get_long(uaecptr addr)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ read_page) <= ARAM_PAGE_MASK))
        return do_get_mem_long((uae_u32*)(addr + read_offset));
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) return HWget_l(addr);
#endif
    check_ram_boundary(addr, 4, false);
    uae_u32 * const m = (uae_u32 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    read_page = addr;
    read_offset = (uintptr)m - (uintptr)addr;
#endif
    return do_get_mem_long(m);
}

static inline uae_u32 phys_get_word(uaecptr addr)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ read_page) <= ARAM_PAGE_MASK))
        return do_get_mem_word((uae_u16*)(addr + read_offset));
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) return HWget_w(addr);
#endif
    check_ram_boundary(addr, 2, false);
    uae_u16 * const m = (uae_u16 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    read_page = addr;
    read_offset = (uintptr)m - (uintptr)addr;
#endif
    return do_get_mem_word(m);
}

static inline uae_u32 phys_get_byte(uaecptr addr)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ read_page) <= ARAM_PAGE_MASK))
        return do_get_mem_byte((uae_u8*)(addr + read_offset));
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) return HWget_b(addr);
#endif
    check_ram_boundary(addr, 1, false);
    uae_u8 * const m = (uae_u8 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    read_page = addr;
    read_offset = (uintptr)m - (uintptr)addr;
#endif
    return do_get_mem_byte(m);
}

static inline void phys_put_quad(uaecptr addr, uae_u64 l)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ write_page) <= ARAM_PAGE_MASK)) {
        do_put_mem_quad((uae_u64*)(addr + write_offset), l);
        return;
    }
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) {
        HWput_l(addr, l); /* TODO: must be HWput_q */
        return;
    } 
#endif
    check_ram_boundary(addr, 8, true);
    uae_u64 * const m = (uae_u64 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    write_page = addr;
    write_offset = (uintptr)m - (uintptr)addr;
#endif
    do_put_mem_quad(m, l);
}

static inline void phys_put_long(uaecptr addr, uae_u32 l)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ write_page) <= ARAM_PAGE_MASK)) {
        do_put_mem_long((uae_u32*)(addr + write_offset), l);
        return;
    }
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) {
        HWput_l(addr, l);
        return;
    } 
#endif
    check_ram_boundary(addr, 4, true);
    uae_u32 * const m = (uae_u32 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    write_page = addr;
    write_offset = (uintptr)m - (uintptr)addr;
#endif
    do_put_mem_long(m, l);
}

static inline void phys_put_word(uaecptr addr, uae_u32 w)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ write_page) <= ARAM_PAGE_MASK)) {
        do_put_mem_word((uae_u16*)(addr + write_offset), w);
        return;
    }
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) {
        HWput_w(addr, w);
        return;
    }
#endif
    check_ram_boundary(addr, 2, true);
    uae_u16 * const m = (uae_u16 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    write_page = addr;
    write_offset = (uintptr)m - (uintptr)addr;
#endif
    do_put_mem_word(m, w);
}

static inline void phys_put_byte(uaecptr addr, uae_u32 b)
{
#ifdef ARAM_PAGE_CHECK
    if (((addr ^ write_page) <= ARAM_PAGE_MASK)) {
        do_put_mem_byte((uae_u8*)(addr + write_offset), b);
        return;
    }
#endif
#ifndef HW_SIGSEGV
    addr = addr < 0xff000000 ? addr : addr & 0x00ffffff;
    if ((addr & 0xfff00000) == 0x00f00000) {
        HWput_b(addr, b);
        return;
    }
#endif
    check_ram_boundary(addr, 1, true);
    uae_u8 * const m = (uae_u8 *)phys_get_real_address(addr);
#ifdef ARAM_PAGE_CHECK
    write_page = addr;
    write_offset = (uintptr)m - (uintptr)addr;
#endif
    do_put_mem_byte(m, b);
}

#ifdef FULLMMU
static ALWAYS_INLINE bool is_unaligned(uaecptr addr, int size)
{
    return unlikely((addr & (size - 1)) && (addr ^ (addr + size - 1)) & 0x1000);
}

static ALWAYS_INLINE uae_u8 *mmu_get_real_address(uaecptr addr, struct mmu_atc_line *cl)
{
	return do_get_real_address(cl->phys + addr);
}

static ALWAYS_INLINE uae_u32 mmu_get_quad(uaecptr addr, int data)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 0, &cl)))
		return do_get_mem_quad((uae_u64 *)mmu_get_real_address(addr, cl));
	return mmu_get_quad_slow(addr, regs.s, data, cl);
}

static ALWAYS_INLINE uae_u64 get_quad(uaecptr addr)
{
	return mmu_get_quad(addr, 1);
}

static ALWAYS_INLINE uae_u32 mmu_get_long(uaecptr addr, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 0, &cl)))
		return do_get_mem_long((uae_u32 *)mmu_get_real_address(addr, cl));
	return mmu_get_long_slow(addr, regs.s, data, size, cl);
}

static ALWAYS_INLINE uae_u32 get_long(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 4)))
		return mmu_get_long_unaligned(addr, 1);
	return mmu_get_long(addr, 1, sz_long);
}

static ALWAYS_INLINE uae_u16 mmu_get_word(uaecptr addr, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 0, &cl)))
		return do_get_mem_word((uae_u16 *)mmu_get_real_address(addr, cl));
	return mmu_get_word_slow(addr, regs.s, data, size, cl);
}

static ALWAYS_INLINE uae_u16 get_word(uaecptr addr)
{
	if (unlikely(is_unaligned(addr, 2)))
		return mmu_get_word_unaligned(addr, 1);
	return mmu_get_word(addr, 1, sz_word);
}

static ALWAYS_INLINE uae_u8 mmu_get_byte(uaecptr addr, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 0, &cl)))
		return do_get_mem_byte((uae_u8 *)mmu_get_real_address(addr, cl));
	return mmu_get_byte_slow(addr, regs.s, data, size, cl);
}

static ALWAYS_INLINE uae_u8 get_byte(uaecptr addr)
{
	return mmu_get_byte(addr, 1, sz_byte);
}

static ALWAYS_INLINE void mmu_put_quad(uaecptr addr, uae_u64 val, int data)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 1, &cl)))
		do_put_mem_quad((uae_u64 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_quad_slow(addr, val, regs.s, data, cl);
}

static ALWAYS_INLINE void put_quad(uaecptr addr, uae_u32 val)
{
	mmu_put_quad(addr, val, 1);
}

static ALWAYS_INLINE void mmu_put_long(uaecptr addr, uae_u32 val, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 1, &cl)))
		do_put_mem_long((uae_u32 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_long_slow(addr, val, regs.s, data, size, cl);
}

static ALWAYS_INLINE void put_long(uaecptr addr, uae_u32 val)
{
	if (unlikely(is_unaligned(addr, 4)))
		mmu_put_long_unaligned(addr, val, 1);
	else
		mmu_put_long(addr, val, 1, sz_long);
}

static ALWAYS_INLINE void mmu_put_word(uaecptr addr, uae_u16 val, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 1, &cl)))
		do_put_mem_word((uae_u16 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_word_slow(addr, val, regs.s, data, size, cl);
}

static ALWAYS_INLINE void put_word(uaecptr addr, uae_u16 val)
{
	if (unlikely(is_unaligned(addr, 2)))
		mmu_put_word_unaligned(addr, val, 1);
	else
		mmu_put_word(addr, val, 1, sz_word);
}

static ALWAYS_INLINE void mmu_put_byte(uaecptr addr, uae_u8 val, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_lookup(addr, data, 1, &cl)))
		do_put_mem_byte((uae_u8 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_byte_slow(addr, val, regs.s, data, size, cl);
}

static ALWAYS_INLINE void put_byte(uaecptr addr, uae_u8 val)
{
	mmu_put_byte(addr, val, 1, sz_byte);
}

static inline uae_u8 *get_real_address(uaecptr addr, int write, int sz)
{
	(void)sz;
    return phys_get_real_address(mmu_translate(addr, regs.s, 1, write));
}

static ALWAYS_INLINE uae_u32 mmu_get_user_long(uaecptr addr, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 0, &cl)))
		return do_get_mem_long((uae_u32 *)mmu_get_real_address(addr, cl));
	return mmu_get_long_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE uae_u16 mmu_get_user_word(uaecptr addr, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 0, &cl)))
		return do_get_mem_word((uae_u16 *)mmu_get_real_address(addr, cl));
	return mmu_get_word_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE uae_u8 mmu_get_user_byte(uaecptr addr, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 0, &cl)))
		return do_get_mem_byte((uae_u8 *)mmu_get_real_address(addr, cl));
	return mmu_get_byte_slow(addr, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_long(uaecptr addr, uae_u32 val, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 1, &cl)))
		do_put_mem_long((uae_u32 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_long_slow(addr, val, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_word(uaecptr addr, uae_u16 val, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 1, &cl)))
		do_put_mem_word((uae_u16 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_word_slow(addr, val, super, data, size, cl);
}

static ALWAYS_INLINE void mmu_put_user_byte(uaecptr addr, uae_u8 val, int super, int data, int size)
{
	struct mmu_atc_line *cl;

	if (likely(mmu_user_lookup(addr, super, data, 1, &cl)))
		do_put_mem_byte((uae_u8 *)mmu_get_real_address(addr, cl), val);
	else
		mmu_put_byte_slow(addr, val, super, data, size, cl);
}

static inline bool valid_address(uaecptr addr, bool write, int sz)
{
    SAVE_EXCEPTION;
    TRY(prb) {
		(void)sz;
		check_ram_boundary(mmu_translate(addr, regs.s, 1, (write ? 1 : 0)), sz, write);
		RESTORE_EXCEPTION;
		return true;
    }
    CATCH(prb) {
		RESTORE_EXCEPTION;
		return false;
    } 
}

#else

#  define get_quad(a)			phys_get_quad(a)
#  define get_long(a)			phys_get_long(a)
#  define get_word(a)			phys_get_word(a)
#  define get_byte(a)			phys_get_byte(a)
#  define put_quad(a,b)			phys_put_quad(a,b)
#  define put_long(a,b)			phys_put_long(a,b)
#  define put_word(a,b)			phys_put_word(a,b)
#  define put_byte(a,b)			phys_put_byte(a,b)
#  define get_real_address(a,w,s)	phys_get_real_address(a)

#define valid_address(a,w,s)		phys_valid_address(a,w,s)
#endif

static inline void flush_internals() {
#ifdef ARAM_PAGE_CHECK
    pc_page = 0xeeeeeeee;
    read_page = 0xeeeeeeee;
    write_page = 0xeeeeeeee;
#endif
}

#endif /* MEMORY_H */

/*
vim:ts=4:sw=4:
*/
