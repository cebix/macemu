/*
 *  ieeefp-i386.cpp - Access to FPU environment, x86 specific code
 *  Code largely derived from GNU libc
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

/*
 *  CPU features
 */

/* XXX: duplicate from cpu/ppc/ppc-dyngen.cpp! */
static uint32 cpu_features = 0;

enum {
	HWCAP_I386_CMOV		= 1 << 15,
	HWCAP_I386_MMX		= 1 << 23,
	HWCAP_I386_SSE		= 1 << 25,
	HWCAP_I386_SSE2		= 1 << 26,
};

static unsigned int x86_cpuid(void)
{
	int fl1, fl2;

	/* See if we can use cpuid. On AMD64 we always can.  */
	__asm__ ("pushfl; pushfl; popl %0; movl %0,%1; xorl %2,%0;"
			 "pushl %0; popfl; pushfl; popl %0; popfl"
			 : "=&r" (fl1), "=&r" (fl2)
			 : "i" (0x00200000));
	if (((fl1 ^ fl2) & 0x00200000) == 0)
		return (0);

	/* Host supports cpuid.  See if cpuid gives capabilities, try
	   CPUID(0).  Preserve %ebx and %ecx; cpuid insn clobbers these, we
	   don't need their CPUID values here, and %ebx may be the PIC
	   register.  */
	__asm__ ("push %%ecx ; push %%ebx ; cpuid ; pop %%ebx ; pop %%ecx"
			 : "=a" (fl1) : "0" (0) : "edx", "cc");
	if (fl1 == 0)
		return (0);

	/* Invoke CPUID(1), return %edx; caller can examine bits to
	   determine what's supported.  */
	__asm__ ("push %%ecx ; push %%ebx ; cpuid ; pop %%ebx ; pop %%ecx" : "=d" (fl2) : "a" (1) : "cc");

	return fl2;
}

static inline int has_cpu_features(int test_cpu_features)
{
	static bool initted = false;
	if (!initted) {
		cpu_features = x86_cpuid();
		initted = true;
	}
	return cpu_features & test_cpu_features;
}


/*
 *  Rounding control
 */

// Get current rounding direction
int fegetround(void)
{
	unsigned short cw;

	__asm__ __volatile__("fnstcw %0" : "=m" (*&cw));

	return cw & 0xc00;
}

// Set the rounding direction represented by ROUND
int fesetround(int round)
{
	unsigned short cw;

	if ((round & ~0xc00) != 0)
		return 1;

	__asm__ __volatile__("fnstcw %0" : "=m" (*&cw));
	cw &= ~0xc00;
	cw |= round;
	__asm__ __volatile__("fldcw %0" : : "m" (*&cw));

	if (has_cpu_features(HWCAP_I386_SSE) != 0) {
		uint32 xcw;
		__asm__ __volatile__("stmxcsr %0" : "=m" (*&xcw));
		xcw &= ~0x6000;
		xcw |= round << 3;
		__asm__ __volatile__("ldmxcsr %0" : : "m" (*&xcw));
	}

	return 0;
}
