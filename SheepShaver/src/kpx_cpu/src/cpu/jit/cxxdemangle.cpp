/*
 *  cxxdemangle.cpp - C++ demangler
 *
 *  Kheperix (C) 2003-2004 Gwenole Beauchesne
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

#include <stddef.h>
#include "cxxdemangle.h"

#if defined(__GNUC__) && (__GXX_ABI_VERSION > 0)
#include <cxxabi.h>

char *
cxx_demangle(const char *mangled_name, char *buf, size_t *n, int *status)
{
	return abi::__cxa_demangle(mangled_name, buf, n, status);
}
#endif
