/*
 *  thunks.cpp - Thunks to share data and code with MacOS
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

#include "sysdeps.h"
#include "thunks.h"
#include "emul_op.h"
#include "cpu_emulation.h"

// Native function declarations
#include "main.h"
#include "video.h"
#include "name_registry.h"
#include "serial.h"
#include "ether.h"
#include "macos_util.h"


/*		NativeOp instruction format:
		+------------+--------------------------+--+----------+------------+
		|      6     |                          |FN|    OP    |      2     |
		+------------+--------------------------+--+----------+------------+
		 0         5 |6                       19 20 21      25 26        31
*/

#define POWERPC_NATIVE_OP(LR, OP) \
		(POWERPC_EMUL_OP | ((LR) << 11) | (((uint32)OP) << 6) | 2)

/*
 *  Return the fake PowerPC opcode to handle specified native code
 */

#if EMULATED_PPC
uint32 NativeOpcode(int selector)
{
	uint32 opcode;
	switch (selector) {
	case NATIVE_DISABLE_INTERRUPT:
	case NATIVE_ENABLE_INTERRUPT:
		opcode = POWERPC_NATIVE_OP(0, selector);
		break;
  	case NATIVE_PATCH_NAME_REGISTRY:
  	case NATIVE_VIDEO_INSTALL_ACCEL:
  	case NATIVE_VIDEO_VBL:
  	case NATIVE_VIDEO_DO_DRIVER_IO:
  	case NATIVE_ETHER_IRQ:
  	case NATIVE_ETHER_INIT:
  	case NATIVE_ETHER_TERM:
  	case NATIVE_ETHER_OPEN:
  	case NATIVE_ETHER_CLOSE:
  	case NATIVE_ETHER_WPUT:
  	case NATIVE_ETHER_RSRV:
  	case NATIVE_SERIAL_NOTHING:
  	case NATIVE_SERIAL_OPEN:
  	case NATIVE_SERIAL_PRIME_IN:
  	case NATIVE_SERIAL_PRIME_OUT:
  	case NATIVE_SERIAL_CONTROL:
  	case NATIVE_SERIAL_STATUS:
  	case NATIVE_SERIAL_CLOSE:
  	case NATIVE_GET_RESOURCE:
  	case NATIVE_GET_1_RESOURCE:
  	case NATIVE_GET_IND_RESOURCE:
  	case NATIVE_GET_1_IND_RESOURCE:
  	case NATIVE_R_GET_RESOURCE:
  	case NATIVE_MAKE_EXECUTABLE:
		opcode = POWERPC_NATIVE_OP(1, selector);
		break;
	default:
		abort();
	}
	return opcode;
}
#endif


/*
 *  Initialize the thunks system
 */

struct native_op_t {
	uint32 tvect;
	uint32 func;
};
static native_op_t native_op[NATIVE_OP_MAX];

bool ThunksInit(void)
{
#if EMULATED_PPC
	for (int i = 0; i < NATIVE_OP_MAX; i++) {
		uintptr base = SheepMem::Reserve(12);
		WriteMacInt32(base + 0, base + 8);
		WriteMacInt32(base + 4, 0); // Fake TVECT
		WriteMacInt32(base + 8, NativeOpcode(i));
		native_op[i].tvect = base;
		native_op[i].func  = base + 8;
	}
#else
#if defined(__linux__)
#define DEFINE_NATIVE_OP(ID, FUNC) do {				\
		uintptr base = SheepMem::Reserve(8);		\
		WriteMacInt32(base + 0, (uint32)FUNC);		\
		WriteMacInt32(base + 4, 0); /*Fake TVECT*/	\
		native_op[ID].tvect = base;					\
		native_op[ID].func  = (uint32)FUNC;			\
	} while (0)
#elif defined(__BEOS__)
#define DEFINE_NATIVE_OP(ID, FUNC) do {				\
		native_op[ID].tvect = FUNC;					\
		native_op[ID].func  = ((uint32 *)FUNC)[0];	\
	} while (0)
#else
#error "FIXME: define NativeOp for your platform"
#endif
	DEFINE_NATIVE_OP(NATIVE_PATCH_NAME_REGISTRY, DoPatchNameRegistry);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_INSTALL_ACCEL, VideoInstallAccel);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_VBL, VideoVBL);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_DO_DRIVER_IO, VideoDoDriverIO);
	DEFINE_NATIVE_OP(NATIVE_ETHER_IRQ, EtherIRQ);
	DEFINE_NATIVE_OP(NATIVE_ETHER_INIT, InitStreamModule);
	DEFINE_NATIVE_OP(NATIVE_ETHER_TERM, TerminateStreamModule);
	DEFINE_NATIVE_OP(NATIVE_ETHER_OPEN, ether_open);
	DEFINE_NATIVE_OP(NATIVE_ETHER_CLOSE, ether_close);
	DEFINE_NATIVE_OP(NATIVE_ETHER_WPUT, ether_wput);
	DEFINE_NATIVE_OP(NATIVE_ETHER_RSRV, ether_rsrv);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_NOTHING, SerialNothing);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_OPEN, SerialOpen);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_PRIME_IN, SerialPrimeIn);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_PRIME_OUT, SerialPrimeOut);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_CONTROL, SerialControl);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_STATUS, SerialStatus);
	DEFINE_NATIVE_OP(NATIVE_SERIAL_CLOSE, SerialClose);
	DEFINE_NATIVE_OP(NATIVE_MAKE_EXECUTABLE, MakeExecutable);
#undef DEFINE_NATIVE_OP
#endif
	return true;
}


/*
 *  Return the native function descriptor (TVECT)
 */

uint32 NativeTVECT(int selector)
{
	assert(selector < NATIVE_OP_MAX);
	const uint32 tvect = native_op[selector].tvect;
	assert(tvect != 0);
	return tvect;
}


/*
 *  Return the native function address
 */

uint32 NativeFunction(int selector)
{
	assert(selector < NATIVE_OP_MAX);
	const uint32 func = native_op[selector].func;
	assert(func != 0);
	return func;
}


/*
 *  Execute native code from EMUL_OP routine (real mode switch)
 */

void ExecuteNative(int selector)
{
	SheepRoutineDescriptor desc(0, NativeTVECT(selector));
	M68kRegisters r;
	Execute68k(desc.addr(), &r);
}
