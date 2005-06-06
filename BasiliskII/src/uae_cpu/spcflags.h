 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation
  *
  * Copyright 1995 Bernd Schmidt
  */

#ifndef SPCFLAGS_H
#define SPCFLAGS_H

typedef uae_u32 spcflags_t;

enum {
	SPCFLAG_STOP				= 0x01,
	SPCFLAG_INT					= 0x02,
	SPCFLAG_BRK					= 0x04,
	SPCFLAG_TRACE				= 0x08,
	SPCFLAG_DOTRACE				= 0x10,
	SPCFLAG_DOINT				= 0x20,
#if USE_JIT
	SPCFLAG_JIT_END_COMPILE		= 0x40,
	SPCFLAG_JIT_EXEC_RETURN		= 0x80,
#else
	SPCFLAG_JIT_END_COMPILE		= 0,
	SPCFLAG_JIT_EXEC_RETURN		= 0,
#endif
	
	SPCFLAG_ALL					= SPCFLAG_STOP
								| SPCFLAG_INT
								| SPCFLAG_BRK
								| SPCFLAG_TRACE
								| SPCFLAG_DOTRACE
								| SPCFLAG_DOINT
								| SPCFLAG_JIT_END_COMPILE
								| SPCFLAG_JIT_EXEC_RETURN
								,
	
	SPCFLAG_ALL_BUT_EXEC_RETURN	= SPCFLAG_ALL & ~SPCFLAG_JIT_EXEC_RETURN
};

#define SPCFLAGS_TEST(m) \
	((regs.spcflags & (m)) != 0)

/* Macro only used in m68k_reset() */
#define SPCFLAGS_INIT(m) do { \
	regs.spcflags = (m); \
} while (0)

#if !(ENABLE_EXCLUSIVE_SPCFLAGS)

#define SPCFLAGS_SET(m) do { \
	regs.spcflags |= (m); \
} while (0)

#define SPCFLAGS_CLEAR(m) do { \
	regs.spcflags &= ~(m); \
} while (0)

#elif (defined(__i386__) || defined(__x86_64__)) && defined(X86_ASSEMBLY)

#define HAVE_HARDWARE_LOCKS

#define SPCFLAGS_SET(m) do { \
	__asm__ __volatile__("lock\n\torl %1,%0" : "=m" (regs.spcflags) : "i" ((m))); \
} while (0)

#define SPCFLAGS_CLEAR(m) do { \
	__asm__ __volatile__("lock\n\tandl %1,%0" : "=m" (regs.spcflags) : "i" (~(m))); \
} while (0)

#else

#undef HAVE_HARDWARE_LOCKS

#include "main.h"
extern B2_mutex *spcflags_lock;

#define SPCFLAGS_SET(m) do { 				\
	B2_lock_mutex(spcflags_lock);			\
	regs.spcflags |= (m);					\
	B2_unlock_mutex(spcflags_lock);		\
} while (0)

#define SPCFLAGS_CLEAR(m) do {				\
	B2_lock_mutex(spcflags_lock);			\
	regs.spcflags &= ~(m);					\
	B2_unlock_mutex(spcflags_lock);		\
} while (0)

#endif

#endif /* SPCFLAGS_H */
