/*
 *  utils-cpuinfo.hpp - Processor capability information
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

#ifndef UTILS_CPUINFO_H
#define UTILS_CPUINFO_H

// Check for x86 feature CMOV
extern bool cpuinfo_check_cmov(void);

// Check for x86 feature MMX
extern bool cpuinfo_check_mmx(void);

// Check for x86 feature SSE
extern bool cpuinfo_check_sse(void);

// Check for x86 feature SSE2
extern bool cpuinfo_check_sse2(void);

// Check for x86 feature SSE3
extern bool cpuinfo_check_sse3(void);

// Check for x86 feature SSSE3
extern bool cpuinfo_check_ssse3(void);

// Check for ppc feature VMX (Altivec)
extern bool cpuinfo_check_altivec(void);

#endif /* UTILS_CPUINFO_H */
