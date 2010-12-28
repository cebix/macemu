/*
 *  LauncherPrefix.h
 *
 *  Copyright (C) 2010 Alexei Svitkine
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

#ifndef LAUNCHERPREFIX_H
#define LAUNCHERPREFIX_H

#define CONFIG_H
#define STDC_HEADERS
#define SIZEOF_DOUBLE 8
#define SIZEOF_FLOAT 4
#define SIZEOF_INT 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2

#ifdef __LP64__
#define SIZEOF_VOID_P 8
#define SIZEOF_LONG 8
#else
#define SIZEOF_VOID_P 4
#define SIZEOF_LONG 4
#endif

#define loff_t off_t

#endif /* LAUNCHERPREFIX_H */
