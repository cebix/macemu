/*
 * UAE - The Un*x Amiga Emulator
 *
 * memory management
 *
 * Copyright 1995 Bernd Schmidt
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

#ifndef UAE_MEMORY_H
#define UAE_MEMORY_H

#if DIRECT_ADDRESSING
extern uintptr MEMBaseDiff;
#endif

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

#if DIRECT_ADDRESSING
static __inline__ uae_u8 *do_get_real_address(uaecptr addr)
{
	return (uae_u8 *)MEMBaseDiff + addr;
}
static __inline__ uae_u32 do_get_virtual_address(uae_u8 *addr)
{
	return (uintptr)addr - MEMBaseDiff;
}
static __inline__ uae_u32 get_long(uaecptr addr)
{
    uae_u32 * const m = (uae_u32 *)do_get_real_address(addr);
    return do_get_mem_long(m);
}
#define phys_get_long get_long
static __inline__ uae_u32 get_word(uaecptr addr)
{
    uae_u16 * const m = (uae_u16 *)do_get_real_address(addr);
    return do_get_mem_word(m);
}
#define phys_get_word get_word
static __inline__ uae_u32 get_byte(uaecptr addr)
{
    uae_u8 * const m = (uae_u8 *)do_get_real_address(addr);
    return do_get_mem_byte(m);
}
#define phys_get_byte get_byte
static __inline__ void put_long(uaecptr addr, uae_u32 l)
{
    uae_u32 * const m = (uae_u32 *)do_get_real_address(addr);
    do_put_mem_long(m, l);
}
#define phys_put_long put_long
static __inline__ void put_word(uaecptr addr, uae_u32 w)
{
    uae_u16 * const m = (uae_u16 *)do_get_real_address(addr);
    do_put_mem_word(m, w);
}
#define phys_put_word put_word
static __inline__ void put_byte(uaecptr addr, uae_u32 b)
{
    uae_u8 * const m = (uae_u8 *)do_get_real_address(addr);
    do_put_mem_byte(m, b);
}
#define phys_put_byte put_byte
static __inline__ uae_u8 *get_real_address(uaecptr addr)
{
	return do_get_real_address(addr);
}
static inline uae_u8 *get_real_address(uaecptr addr, int write, int sz)
{
    return do_get_real_address(addr);
}
static inline uae_u8 *phys_get_real_address(uaecptr addr)
{
    return do_get_real_address(addr);
}
static __inline__ uae_u32 get_virtual_address(uae_u8 *addr)
{
	return do_get_virtual_address(addr);
}
#endif /* DIRECT_ADDRESSING */

static __inline__ void check_ram_boundary(uaecptr addr, int size, bool write) {}
static inline void flush_internals() {}

#endif /* MEMORY_H */

