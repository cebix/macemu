/*
 *  thunks.h - Thunks to share data and code with MacOS
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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

#ifndef THUNKS_H
#define THUNKS_H

#include "cpu_emulation.h"

/*
 *  Native function invocation
 */

enum {
  NATIVE_PATCH_NAME_REGISTRY,
  NATIVE_VIDEO_INSTALL_ACCEL,
  NATIVE_VIDEO_VBL,
  NATIVE_VIDEO_DO_DRIVER_IO,
  NATIVE_ETHER_IRQ,
  NATIVE_ETHER_INIT,
  NATIVE_ETHER_TERM,
  NATIVE_ETHER_OPEN,
  NATIVE_ETHER_CLOSE,
  NATIVE_ETHER_WPUT,
  NATIVE_ETHER_RSRV,
  NATIVE_SERIAL_NOTHING,
  NATIVE_SERIAL_OPEN,
  NATIVE_SERIAL_PRIME_IN,
  NATIVE_SERIAL_PRIME_OUT,
  NATIVE_SERIAL_CONTROL,
  NATIVE_SERIAL_STATUS,
  NATIVE_SERIAL_CLOSE,
  NATIVE_GET_RESOURCE,
  NATIVE_GET_1_RESOURCE,
  NATIVE_GET_IND_RESOURCE,
  NATIVE_GET_1_IND_RESOURCE,
  NATIVE_R_GET_RESOURCE,
  NATIVE_DISABLE_INTERRUPT,
  NATIVE_ENABLE_INTERRUPT,
  NATIVE_MAKE_EXECUTABLE,
  NATIVE_CHECK_LOAD_INVOC,
  NATIVE_SYNC_HOOK,
  NATIVE_BITBLT_HOOK,
  NATIVE_FILLRECT_HOOK,
  NATIVE_BITBLT,
  NATIVE_INVRECT,
  NATIVE_FILLRECT,
  NATIVE_OP_MAX
};

// Initialize the thunks system
extern bool ThunksInit(void);

// Exit the thunks system
extern void ThunksExit(void);

// Return the fake PowerPC opcode to handle specified native code
#if EMULATED_PPC
extern uint32 NativeOpcode(int selector);
#endif

// Return the native function descriptor (TVECT)
extern uint32 NativeTVECT(int selector);

// Return the native function address
extern uint32 NativeFunction(int selector);

// Return the routine descriptor address of the native function
extern uint32 NativeRoutineDescriptor(int selector);


/*
 *  Helpers to share 32-bit addressable data with MacOS
 */

class SheepMem {
	static uint32 align(uint32 size);
protected:
	static uint32  page_size;
	static uintptr zero_page;
	static uintptr base;
	static uintptr top;
	static const uint32 size = 0x40000; // 256 KB
public:
	static bool Init(void);
	static void Exit(void);
	static uint32 PageSize();
	static uintptr ZeroPage();
	static uintptr Reserve(uint32 size);
	static void Release(uint32 size);
	friend class SheepVar;
};

inline uint32 SheepMem::align(uint32 size)
{
	// Align on 4 bytes boundaries
	return (size + 3) & -4;
}

inline uint32 SheepMem::PageSize()
{
  return page_size;
}

inline uintptr SheepMem::ZeroPage()
{
  return zero_page;
}

inline uintptr SheepMem::Reserve(uint32 size)
{
	top -= align(size);
	assert(top >= base);
	return top;
}

inline void SheepMem::Release(uint32 size)
{
	top += align(size);
}

class SheepVar
{
	uintptr m_base;
	uint32  m_size;
public:
	SheepVar(uint32 requested_size);
	~SheepVar() { SheepMem::Release(m_size); }
	uintptr addr() const { return m_base; }
	void *ptr() const { return (void *)addr(); }
};

inline SheepVar::SheepVar(uint32 requested_size)
{
	m_size = SheepMem::align(requested_size);
	m_base = SheepMem::Reserve(m_size);
}

// TODO: optimize for 32-bit platforms

template< int size >
struct SheepArray : public SheepVar
{
	SheepArray() : SheepVar(size) { }
	uint8 *ptr() const { return (uint8 *)addr(); }
};

struct SheepVar32 : public SheepVar
{
	SheepVar32() : SheepVar(4) { }
	SheepVar32(uint32 value) : SheepVar(4) { set_value(value); }
	uint32 value() const { return ReadMacInt32(addr()); }
	void set_value(uint32 v) { WriteMacInt32(addr(), v); }
	uint32 *ptr() const { return (uint32 *)addr(); }
};

struct SheepString : public SheepVar
{
	SheepString(const char *str) : SheepVar(strlen(str) + 1)
		{ if (str) strcpy((char *)addr(), str); else WriteMacInt8(addr(), 0); }
	char *value() const
		{ return (char *)addr(); }
	char *ptr() const
		{ return (char *)addr(); }
};

#endif
