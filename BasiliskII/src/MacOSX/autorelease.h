/*
 *	autorelease.h - a macro wrapping autorelease pools for use with Objective-C++ source files.
 *
 *  Expands to @autoreleasepool on clang, uses a little hack to emulate @autoreleasepool on gcc.
 *
 *	(C) 2012 Charles Srstka
 *
 *	Feel free to put this under whatever license you wish.
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
