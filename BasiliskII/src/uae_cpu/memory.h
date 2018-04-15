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
static __inline__ uae_u32 get_word(uaecptr addr)
{
    uae_u16 * const m = (uae_u16 *)do_get_real_address(addr);
    return do_get_mem_word(m);
}
static __inline__ uae_u32 get_byte(uaecptr addr)
{
    uae_u8 * const m = (uae_u8 *)do_get_real_address(addr);
    return do_get_mem_byte(m);
}
static __inline__ void put_long(uaecptr addr, uae_u32 l)
{
    uae_u32 * const m = (uae_u32 *)do_get_real_address(addr);
    do_put_mem_long(m, l);
}
static __inline__ void put_word(uaecptr addr, uae_u32 w)
{
    uae_u16 * const m = (uae_u16 *)do_get_real_address(addr);
    do_put_mem_word(m, w);
}
static __inline__ void put_byte(uaecptr addr, uae_u32 b)
{
    uae_u8 * const m = (uae_u8 *)do_get_real_address(addr);
    do_put_mem_byte(m, b);
}
static __inline__ uae_u8 *get_real_address(uaecptr addr)
{
	return do_get_real_address(addr);
}
static __inline__ uae_u32 get_virtual_address(uae_u8 *addr)
{
	return do_get_virtual_address(addr);
}
#endif /* DIRECT_ADDRESSING */

#endif /* MEMORY_H */

