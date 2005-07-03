/*
 *  vm.hpp - Virtual memory management
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

#ifndef VM_H
#define VM_H

///
///		Optimized memory accessors
///

#if defined(__i386__) || defined(__powerpc__) || defined(__ppc__) || defined(__m68k__) || defined(__x86_64__)
# define VM_CAN_ACCESS_UNALIGNED
#endif

#ifdef WORDS_BIGENDIAN

#ifdef VM_CAN_ACCESS_UNALIGNED

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_2
#define VM_OPTIMIZED_MEMORY_ACCESS_2
static inline uint32 vm_do_read_memory_2(uint16 *a) { return *a; }
static inline void vm_do_write_memory_2(uint16 *a, uint32 v) { *a = v; }
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_4
#define VM_OPTIMIZED_MEMORY_ACCESS_4
static inline uint32 vm_do_read_memory_4(uint32 *a) { return *a; }
static inline void vm_do_write_memory_4(uint32 *a, uint32 v) { *a = v; }
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_8
#define VM_OPTIMIZED_MEMORY_ACCESS_8
static inline uint64 vm_do_read_memory_8(uint64 *a) { return *a; }
static inline void vm_do_write_memory_8(uint64 *a, uint64 v) { *a = v; }
#endif

#endif /* VM_CAN_ACCESS_UNALIGNED */

#else

#ifdef VM_CAN_ACCESS_UNALIGNED

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_2
#define VM_OPTIMIZED_MEMORY_ACCESS_2
static inline uint32 vm_do_read_memory_2(uint16 *a) { return bswap_16(*a); }
static inline void vm_do_write_memory_2(uint16 *a, uint32 v) { *a = bswap_16(v); }
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_4
#define VM_OPTIMIZED_MEMORY_ACCESS_4
static inline uint32 vm_do_read_memory_4(uint32 *a) { return bswap_32(*a); }
static inline void vm_do_write_memory_4(uint32 *a, uint32 v) { *a = bswap_32(v); }
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_8
#define VM_OPTIMIZED_MEMORY_ACCESS_8
static inline uint64 vm_do_read_memory_8(uint64 *a) { return bswap_64(*a); }
static inline void vm_do_write_memory_8(uint64 *a, uint64 v) { *a = bswap_64(v); }
#endif

#endif /* VM_CAN_ACCESS_UNALIGNED */

#endif /* WORDS_BIGENDIAN */

///
///		Generic core memory accessors
///

static inline uint32 vm_do_read_memory_1(uint8 *a)
{
	return *a;
}
static inline void vm_do_write_memory_1(uint8 *a, uint32 v)
{
	*a = v;
}

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_2
static inline uint32 vm_do_read_memory_2(uint16 *a)
{
	uint8 * b = (uint8 *)a;
	return (b[0] << 8) | b[1];
}
static inline void vm_do_write_memory_2(uint16 *a, uint32 v)
{
	uint8 * b = (uint8 *)a;
	b[0] = v >> 8;
	b[1] = v;
}
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_4
static inline uint32 vm_do_read_memory_4(uint32 *a)
{
	uint8 * b = (uint8 *)a;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}
static inline void vm_do_write_memory_4(uint32 *a, uint32 v)
{
	uint8 * b = (uint8 *)a;
	b[0] = v >> 24;
	b[1] = v >> 16;
	b[2] = v >> 8;
	b[3] = v;
}
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_8
static inline uint64 vm_do_read_memory_8(uint64 *a)
{
	uint8 * b = (uint8 *)a;
	return
		((uint64)b[0] << 56) |
		((uint64)b[1] << 48) |
		((uint64)b[2] << 40) |
		((uint64)b[3] << 32) |
		((uint64)b[4] << 24) |
		((uint64)b[5] << 16) |
		((uint64)b[6] << 8) |
		((uint64)b[7]);
}

static inline void vm_do_write_memory_8(uint64 *a, uint64 v)
{
	uint8 * b = (uint8 *)a;
	b[0] = v >> 56;
	b[1] = v >> 48;
	b[2] = v >> 40;
	b[3] = v >> 32;
	b[4] = v >> 24;
	b[5] = v >> 16;
	b[6] = v >> 8;
	b[7] = v;
}
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_2_REVERSED
static inline uint32 vm_do_read_memory_2_reversed(uint16 *a)
{
	uint8 * b = (uint8 *)a;
	return b[0] | (b[1] << 8);
}
static inline void vm_do_write_memory_2_reversed(uint16 *a, uint32 v)
{
	uint8 * b = (uint8 *)a;
	b[0] = v;
	b[1] = v >> 8;
}
#endif

#ifndef VM_OPTIMIZED_MEMORY_ACCESS_4_REVERSED
static inline uint32 vm_do_read_memory_4_reversed(uint32 *a)
{
	uint8 * b = (uint8 *)a;
	return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}
static inline void vm_do_write_memory_4_reversed(uint32 *a, uint32 v)
{
	uint8 * b = (uint8 *)a;
	b[0] = v;
	b[1] = v >> 8;
	b[2] = v >> 16;
	b[3] = v >> 24;
}
#endif

///
///		Actual memory accessors visible to CPU through virtual addresses
///

typedef uintptr vm_addr_t;

#if REAL_ADDRESSING
const uintptr VMBaseDiff = 0;
#elif DIRECT_ADDRESSING
#ifdef NATMEM_OFFSET
const uintptr VMBaseDiff = NATMEM_OFFSET;
#endif
// Wrap address to 32-bit if we are not using 33-bit addressing space
#if defined(SHEEPSHAVER) && SIZEOF_VOID_P == 8
#define vm_wrap_address(ADDR) (uintptr)(uint32)(ADDR)
#endif
#endif
#ifndef vm_wrap_address
#define vm_wrap_address(ADDR) (ADDR)
#endif

#if REAL_ADDRESSING || DIRECT_ADDRESSING
static inline uint8 * vm_do_get_real_address(vm_addr_t addr)
{
	return (uint8 *)vm_wrap_address(VMBaseDiff + addr);
}
static inline vm_addr_t vm_do_get_virtual_address(uint8 *addr)
{
	return vm_wrap_address((uintptr)addr - VMBaseDiff);
}
static inline uint32 vm_read_memory_1(vm_addr_t addr)
{
	uint8 * const m = vm_do_get_real_address(addr);
	return vm_do_read_memory_1(m);
}
static inline uint32 vm_read_memory_2(vm_addr_t addr)
{
	uint16 * const m = (uint16 *)vm_do_get_real_address(addr);
	return vm_do_read_memory_2(m);
}
static inline uint32 vm_read_memory_4(vm_addr_t addr)
{
	uint32 * const m = (uint32 *)vm_do_get_real_address(addr);
	return vm_do_read_memory_4(m);
}
static inline uint64 vm_read_memory_8(vm_addr_t addr)
{
	uint64 * const m = (uint64 *)vm_do_get_real_address(addr);
	return vm_do_read_memory_8(m);
}
#define vm_read_memory_1_reversed vm_read_memory_1
static inline uint32 vm_read_memory_2_reversed(vm_addr_t addr)
{
	uint16 * const m = (uint16 *)vm_do_get_real_address(addr);
	return vm_do_read_memory_2_reversed(m);
}
static inline uint32 vm_read_memory_4_reversed(vm_addr_t addr)
{
	uint32 * const m = (uint32 *)vm_do_get_real_address(addr);
	return vm_do_read_memory_4_reversed(m);
}
static inline void vm_write_memory_1(vm_addr_t addr, uint32 value)
{
	uint8 * const m = vm_do_get_real_address(addr);
	vm_do_write_memory_1(m, value);
}
static inline void vm_write_memory_2(vm_addr_t addr, uint32 value)
{
	uint16 * const m = (uint16 *)vm_do_get_real_address(addr);
	vm_do_write_memory_2(m, value);
}
static inline void vm_write_memory_4(vm_addr_t addr, uint32 value)
{
	uint32 * const m = (uint32 *)vm_do_get_real_address(addr);
	vm_do_write_memory_4(m, value);
}
static inline void vm_write_memory_8(vm_addr_t addr, uint64 value)
{
	uint64 * const m = (uint64 *)vm_do_get_real_address(addr);
	vm_do_write_memory_8(m, value);
}
#define vm_write_memory_1_reversed vm_write_memory_1
static inline void vm_write_memory_2_reversed(vm_addr_t addr, uint32 value)
{
	uint16 * const m = (uint16 *)vm_do_get_real_address(addr);
	vm_do_write_memory_2_reversed(m, value);
}
static inline void vm_write_memory_4_reversed(vm_addr_t addr, uint32 value)
{
	uint32 * const m = (uint32 *)vm_do_get_real_address(addr);
	vm_do_write_memory_4_reversed(m, value);
}
static inline void *vm_memset(vm_addr_t addr, int c, size_t n)
{
	uint8 * const m = (uint8 *)vm_do_get_real_address(addr);
	return memset(m, c, n);
}
#ifdef __cplusplus
static inline void *vm_memcpy(void *dest, vm_addr_t src, size_t n)
{
	return memcpy(dest, vm_do_get_real_address(src), n);
}
static inline void *vm_memcpy(vm_addr_t dest, const void *src, size_t n)
{
	return memcpy(vm_do_get_real_address(dest), src, n);
}
#endif
static inline void *vm_memcpy(vm_addr_t dest, vm_addr_t src, size_t n)
{
	return memcpy(vm_do_get_real_address(dest), vm_do_get_real_address(src), n);
}
#endif

#endif /* VM_H */

