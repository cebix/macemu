/*
 *  cxxdemangle.h - C++ demangler
 *
 *  Kheperix (C) 2003-2005-2004 Gwenole Beauchesne
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

#ifndef CXX_DEMANGLE_H
#define CXX_DEMANGLE_H

/**
 *	cxx_demangle
 *
 *		Following GCC 3.0 ABI:
 *		<http://www.codesourcery.com/cxx-abi/abi.html>
 *
 *	-	MANGLED-NAME is a pointer to a null-terminated array of
 *		characters
 *
 *	-	BUF may be null. If it is non-null, then N must also be
 *		nonnull, and BUF is a pointer to an array, of at least *N
 *		characters, that was allocated using malloc().
 *
 *	-	STATUS points to an int that is used as an error indicator. It
 *		is permitted to be null, in which case the user just doesn't
 *		get any detailed error information.
 *
 *		Codes:	 0: success
 *				-1: memory allocation failure
 *				-2: invalid mangled name
 *				-3: invalid arguments (e.g. BUG nonnull and N null)
 **/

#ifdef __cplusplus
extern "C"
#endif
char *cxx_demangle(const char *mangled_name,
				   char *buf,
				   size_t *n,
				   int *status);

#endif /* CXX_DEMANGLE_H */
