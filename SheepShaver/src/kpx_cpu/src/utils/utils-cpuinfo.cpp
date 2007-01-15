/*
 *  utils-cpuinfo.cpp - Processor capability information
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

#include "sysdeps.h"
#include "utils/utils-cpuinfo.hpp"
#include "utils/utils-sentinel.hpp"

// x86 CPU features
static uint32 x86_cpu_features = 0;

enum {
	HWCAP_I386_CMOV			= 1 << 15,
	HWCAP_I386_MMX			= 1 << 23,
	HWCAP_I386_SSE			= 1 << 25,
	HWCAP_I386_SSE2			= 1 << 26,
	HWCAP_I386_EDX_FLAGS	= (HWCAP_I386_CMOV|HWCAP_I386_MMX|HWCAP_I386_SSE|HWCAP_I386_SSE2),
	HWCAP_I386_SSE3			= 1 << 0,
	HWCAP_I386_SSSE3		= 1 << 9,
	HWCAP_I386_ECX_FLAGS	= (HWCAP_I386_SSE3|HWCAP_I386_SSSE3),
};

// Determine x86 CPU features
DEFINE_INIT_SENTINEL(init_x86_cpu_features);

static void init_x86_cpu_features(void)
{
#if defined(__i386__) || defined(__x86_64__)
	unsigned int fl1, fl2;

#ifndef __x86_64__
	/* See if we can use cpuid. On AMD64 we always can.  */
	__asm__ ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
			 "pushl %0; popfl; pushfl; popl %0; popfl"
			 : "=&r" (fl1), "=&r" (fl2)
			 : "i" (0x00200000));
	if (((fl1 ^ fl2) & 0x00200000) == 0)
		return;
#endif

	/* Host supports cpuid.  See if cpuid gives capabilities, try
	   CPUID(0).  Preserve %ebx and %ecx; cpuid insn clobbers these, we
	   don't need their CPUID values here, and %ebx may be the PIC
	   register.  */
#ifdef __x86_64__
	__asm__ ("pushq %%rcx; pushq %%rbx; cpuid; popq %%rbx; popq %%rcx"
			 : "=a" (fl1) : "0" (0) : "rdx", "cc");
#else
	__asm__ ("push %%ecx ; push %%ebx ; cpuid ; pop %%ebx ; pop %%ecx"
			 : "=a" (fl1) : "0" (0) : "edx", "cc");
#endif
	if (fl1 == 0)
		return;

	/* Invoke CPUID(1), return %edx; caller can examine bits to
	   determine what's supported.  */
#ifdef __x86_64__
	__asm__ ("push %%rbx ; cpuid ; pop %%rbx" : "=c" (fl1), "=d" (fl2) : "a" (1) : "cc");
#else
	__asm__ ("push %%ebx ; cpuid ; pop %%ebx" : "=c" (fl1), "=d" (fl2) : "a" (1) : "cc");
#endif

	x86_cpu_features = (fl1 & HWCAP_I386_ECX_FLAGS) | (fl2 & HWCAP_I386_EDX_FLAGS);
#endif
}

// Check for x86 feature CMOV
bool cpuinfo_check_cmov(void)
{
	return x86_cpu_features & HWCAP_I386_CMOV;
}

// Check for x86 feature MMX
bool cpuinfo_check_mmx(void)
{
	return x86_cpu_features & HWCAP_I386_MMX;
}

// Check for x86 feature SSE
bool cpuinfo_check_sse(void)
{
	return x86_cpu_features & HWCAP_I386_SSE;
}

// Check for x86 feature SSE2
bool cpuinfo_check_sse2(void)
{
	return x86_cpu_features & HWCAP_I386_SSE2;
}

// Check for x86 feature SSE3
bool cpuinfo_check_sse3(void)
{
	return x86_cpu_features & HWCAP_I386_SSE3;
}

// Check for x86 feature SSSE3
bool cpuinfo_check_ssse3(void)
{
	return x86_cpu_features & HWCAP_I386_SSSE3;
}

// PowerPC CPU features
static uint32 ppc_cpu_features = 0;

enum {
	HWCAP_PPC_ALTIVEC	= 1 << 0
};

// Determine PowerPC CPU features
DEFINE_INIT_SENTINEL(init_ppc_cpu_features);

static void init_ppc_cpu_features(void)
{
}

// Check for ppc feature VMX (Altivec)
bool cpuinfo_check_altivec(void)
{
	return ppc_cpu_features & HWCAP_PPC_ALTIVEC;
}
