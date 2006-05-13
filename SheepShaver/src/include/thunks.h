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
  NATIVE_ETHER_AO_GET_HWADDR,
  NATIVE_ETHER_AO_ADD_MULTI,
  NATIVE_ETHER_AO_DEL_MULTI,
  NATIVE_ETHER_AO_SEND_PACKET,
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
  NATIVE_MAKE_EXECUTABLE,
  NATIVE_CHECK_LOAD_INVOC,
  NATIVE_NQD_SYNC_HOOK,
  NATIVE_NQD_BITBLT_HOOK,
  NATIVE_NQD_FILLRECT_HOOK,
  NATIVE_NQD_UNKNOWN_HOOK,
  NATIVE_NQD_BITBLT,
  NATIVE_NQD_INVRECT,
  NATIVE_NQD_FILLRECT,
  NATIVE_NAMED_CHECK_LOAD_INVOC,
  NATIVE_GET_NAMED_RESOURCE,
  NATIVE_GET_1_NAMED_RESOURCE,
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
 *
 *  There are two distinct allocatable regions:
 *
 *  - The Data region is used to share data between MacOS and
 *    SheepShaver. This is stack-like allocation since it is
 *    meant to only hold temporary data which dies at the end
 *    of the current function scope.
 *
 *  - The Procedure region is used to hold permanent M68K or
 *    PowerPC code to assist native routine implementations.
 *
 *  - The Procedure region grows up whereas the Data region
 *    grows down. They may intersect into the ZeroPage, which
 *    is a read-only page with all bits set to zero. In practise,
 *    the intersection is unlikely since the Procedure region is
 *    static and the Data region is meant to be small (< 256 KB).
 */

class SheepMem {
	static uint32 align(uint32 size);
protected:
	static uint32  page_size;
	static uintptr zero_page;
	static uintptr base;
	static uintptr data;
	static uintptr proc;
	static const uint32 size = 0x80000; // 512 KB
public:
	static bool Init(void);
	static void Exit(void);
	static uint32 PageSize();
	static uint32 ZeroPage();
	static uint32 Reserve(uint32 size);
	static void Release(uint32 size);
	static uint32 ReserveProc(uint32 size);
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

inline uint32 SheepMem::ZeroPage()
{
  return zero_page;
}

inline uint32 SheepMem::Reserve(uint32 size)
{
	data -= align(size);
	assert(data >= proc);
	return data;
}

inline void SheepMem::Release(uint32 size)
{
	data += align(size);
}

inline uint32 SheepMem::ReserveProc(uint32 size)
{
	uint32 mproc = proc;
	proc += align(size);
	assert(proc < data);
	return mproc;
}

static inline uint32 SheepProc(const uint8 *proc, uint32 proc_size)
{
	uint32 mac_proc = SheepMem::ReserveProc(proc_size);
	Host2Mac_memcpy(mac_proc, proc, proc_size);
	return mac_proc;
}

#define BUILD_SHEEPSHAVER_PROCEDURE(PROC)							\
	static uint32 PROC = 0;											\
	if (PROC == 0)													\
		PROC = SheepProc(PROC##_template, sizeof(PROC##_template))

class SheepVar
{
	uint32 m_base;
	uint32 m_size;
public:
	SheepVar(uint32 requested_size);
	~SheepVar() { SheepMem::Release(m_size); }
	uint32 addr() const { return m_base; }
};

inline SheepVar::SheepVar(uint32 requested_size)
{
	m_size = SheepMem::align(requested_size);
	m_base = SheepMem::Reserve(m_size);
}

// TODO: optimize for 32-bit platforms

template< int requested_size >
struct SheepArray : public SheepVar
{
	SheepArray() : SheepVar(requested_size) { }
};

struct SheepVar32 : public SheepVar
{
	SheepVar32() : SheepVar(4) { }
	SheepVar32(uint32 value) : SheepVar(4) { set_value(value); }
	uint32 value() const { return ReadMacInt32(addr()); }
	void set_value(uint32 v) { WriteMacInt32(addr(), v); }
};

struct SheepString : public SheepVar
{
	SheepString(const char *str) : SheepVar(strlen(str) + 1)
		{ if (str) strcpy(value(), str); else WriteMacInt8(addr(), 0); }
	char *value() const
		{ return (char *)Mac2HostAddr(addr()); }
};

#endif
