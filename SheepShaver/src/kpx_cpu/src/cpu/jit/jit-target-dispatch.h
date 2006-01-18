
/*
 *  jit-target-dispatch.h - JIT headers dispatcher
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

/* Undefine some built-ins */
#ifdef  i386
#undef  i386
#define _jit_defined_i386
#endif
#ifdef  amd64
#undef  amd64
#define _jit_defined_amd64
#endif
#ifdef  mips
#undef  mips
#define _jit_defined_mips
#endif

/* Dispatch arch dependent header */
#define _JIT_CONCAT4(a,b,c,d) a##b##c##d
#if defined(__GNUC__)
#define _JIT_MAKE_HEADER(arch,header) <cpu/jit/arch/header>
#else
#define _JIT_MAKE_HEADER(arch,header) _JIT_CONCAT4(<cpu/jit/,arch,/,header>)
#endif
#if defined(__x86_64__)
#include _JIT_MAKE_HEADER(amd64,_JIT_HEADER)
#elif defined(__i386__)
#include _JIT_MAKE_HEADER(x86,_JIT_HEADER)
#elif defined(__powerpc__) || defined(__ppc__)
#include _JIT_MAKE_HEADER(ppc,_JIT_HEADER)
#elif defined(__mips__) || (defined __sgi && defined __mips)
#include _JIT_MAKE_HEADER(mips,_JIT_HEADER)
#else
#error "Unknown architecture, please submit bug report"
#endif
#undef _JIT_CONCAT4
#undef _JIT_MAKE_HEADER
#undef _JIT_HEADER

/* Redefine built-ins */
#ifdef  _jit_defined_i386
#undef  _jit_defined_i386
#define i386 1
#endif
#ifdef  _jit_defined_amd64
#undef  _jit_defined_amd64
#define amd64 1
#endif
#ifdef  _jit_defined_mips
#undef  _jit_defined_mips
#define mips 1
#endif
