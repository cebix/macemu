/*
 *	autorelease.h - a macro wrapping autorelease pools for use with Objective-C++ source files.
 *
 *  Expands to @autoreleasepool on clang, uses a little hack to emulate @autoreleasepool on gcc.
 *
 *	(C) 2012 Charles Srstka
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import <Foundation/Foundation.h>

#ifndef __Autorelease_H__
#define __Autorelease_H__

// just a little forward compatibility in case we ever support LLVM/clang
#if __clang__
#define AUTORELEASE_POOL @autoreleasepool
#else
class Autorelease_Pool_Wrapper {
public:
	Autorelease_Pool_Wrapper() { m_pool = [[NSAutoreleasePool alloc] init]; }
	~Autorelease_Pool_Wrapper() { [m_pool drain]; }
	operator bool() const { return true; }
private:
	NSAutoreleasePool *m_pool;
};

#define POOL_NAME(x, y) x##_##y
#define POOL_NAME2(x, y) POOL_NAME(x, y)
#define AUTORELEASE_POOL if(Autorelease_Pool_Wrapper POOL_NAME2(pool, __LINE__) = Autorelease_Pool_Wrapper())
#endif // !__clang__

#endif // __Autorelease_H__
