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
#include "xlowmem.h"

// Native function declarations
#include "main.h"
#include "video.h"
#include "name_registry.h"
#include "serial.h"
#include "ether.h"
#include "macos_util.h"

// Generate PowerPC thunks for GetResource() replacements?
#define POWERPC_GET_RESOURCE_THUNKS 1


/*		NativeOp instruction format:
		+------------+-------------------------+--+-----------+------------+
		|      6     |                         |FN|    OP     |      2     |
		+------------+-------------------------+--+-----------+------------+
		 0         5 |6                      18 19 20      25 26        31
*/

#define POWERPC_NATIVE_OP(FN, OP) \
		(POWERPC_EMUL_OP | ((FN) << 12) | (((uint32)OP) << 6) | 2)

/*
 *  Return the fake PowerPC opcode to handle specified native code
 */

#if EMULATED_PPC
uint32 NativeOpcode(int selector)
{
	uint32 opcode;
	switch (selector) {
	case NATIVE_CHECK_LOAD_INVOC:
	case NATIVE_NAMED_CHECK_LOAD_INVOC:
	case NATIVE_NQD_SYNC_HOOK:
	case NATIVE_NQD_BITBLT_HOOK:
	case NATIVE_NQD_FILLRECT_HOOK:
	case NATIVE_NQD_UNKNOWN_HOOK:
	case NATIVE_NQD_BITBLT:
	case NATIVE_NQD_INVRECT:
	case NATIVE_NQD_FILLRECT:
		opcode = POWERPC_NATIVE_OP(0, selector);
		break;
  	case NATIVE_PATCH_NAME_REGISTRY:
  	case NATIVE_VIDEO_INSTALL_ACCEL:
  	case NATIVE_VIDEO_VBL:
  	case NATIVE_VIDEO_DO_DRIVER_IO:
	case NATIVE_ETHER_AO_GET_HWADDR:
	case NATIVE_ETHER_AO_ADD_MULTI:
	case NATIVE_ETHER_AO_DEL_MULTI:
	case NATIVE_ETHER_AO_SEND_PACKET:
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
	case NATIVE_GET_NAMED_RESOURCE:
	case NATIVE_GET_1_NAMED_RESOURCE:
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
 *  Generate PowerPC thunks for GetResource() replacements
 */

#if EMULATED_PPC
static uint32 get_resource_func;
static uint32 get_1_resource_func;
static uint32 get_ind_resource_func;
static uint32 get_1_ind_resource_func;
static uint32 r_get_resource_func;
static uint32 get_named_resource_func;
static uint32 get_1_named_resource_func;

static void generate_powerpc_thunks(void)
{
	// check_load_invoc() thunk
	uint32 check_load_invoc_opcode = NativeOpcode(NATIVE_CHECK_LOAD_INVOC);
	uint32 base;

	static uint32 get_resource_template[] = {
		PL(0x7c0802a6),		// mflr    r0
		PL(0x90010008),		// stw     r0,8(r1)
		PL(0x9421ffbc),		// stwu    r1,-68(r1)
		PL(0x90610038),		// stw     r3,56(r1)
		PL(0x9081003c),		// stw     r4,60(r1)
		PL(0x00000000),		// lwz     r0,XLM_GET_RESOURCE(r0)
		PL(0x80402834),		// lwz     r2,XLM_RES_LIB_TOC(r0)
		PL(0x7c0903a6),		// mtctr   r0
		PL(0x4e800421),		// bctrl
		PL(0x90610040),		// stw     r3,64(r1)
		PL(0x80610038),		// lwz     r3,56(r1)
		PL(0xa881003e),		// lha     r4,62(r1)
		PL(0x80a10040),		// lwz     r5,64(r1)
		PL(0x00000001),		// <check_load_invoc>
		PL(0x80610040),		// lwz     r3,64(r1)
		PL(0x8001004c),		// lwz     r0,76(r1)
		PL(0x7c0803a6),		// mtlr    r0
		PL(0x38210044),		// addi    r1,r1,68
		PL(0x4e800020)		// blr
	};
	const uint32 get_resource_template_size = sizeof(get_resource_template);

	int xlm_index = -1, check_load_invoc_index = -1;
	for (int i = 0; i < get_resource_template_size/4; i++) {
		uint32 opcode = ntohl(get_resource_template[i]);
		switch (opcode) {
		case 0x00000000:
			xlm_index = i;
			break;
		case 0x00000001:
			check_load_invoc_index = i;
			break;
		}
	}
	assert(xlm_index != -1 && check_load_invoc_index != -1);

	// GetResource()
	get_resource_func = base = SheepMem::Reserve(get_resource_template_size);
	Host2Mac_memcpy(base, get_resource_template, get_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// Get1Resource()
	get_1_resource_func = base = SheepMem::Reserve(get_resource_template_size);
	Host2Mac_memcpy(base, get_resource_template, get_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_1_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// GetIndResource()
	get_ind_resource_func = base = SheepMem::Reserve(get_resource_template_size);
	Host2Mac_memcpy(base, get_resource_template, get_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_IND_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// Get1IndResource()
	get_1_ind_resource_func = base = SheepMem::Reserve(get_resource_template_size);
	Host2Mac_memcpy(base, get_resource_template, get_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_1_IND_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// RGetResource()
	r_get_resource_func = base = SheepMem::Reserve(get_resource_template_size);
	Host2Mac_memcpy(base, get_resource_template, get_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_R_GET_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// named_check_load_invoc() thunk
	check_load_invoc_opcode = NativeOpcode(NATIVE_NAMED_CHECK_LOAD_INVOC);

	static uint32 get_named_resource_template[] = {
		PL(0x7c0802a6),		// mflr    r0
		PL(0x90010008),		// stw     r0,8(r1)
		PL(0x9421ffbc),		// stwu    r1,-68(r1)
		PL(0x90610038),		// stw     r3,56(r1)
		PL(0x9081003c),		// stw     r4,60(r1)
		PL(0x00000000),		// lwz     r0,XLM_GET_NAMED_RESOURCE(r0)
		PL(0x80402834),		// lwz     r2,XLM_RES_LIB_TOC(r0)
		PL(0x7c0903a6),		// mtctr   r0
		PL(0x4e800421),		// bctrl
		PL(0x90610040),		// stw     r3,64(r1)
		PL(0x80610038),		// lwz     r3,56(r1)
		PL(0x8081003c),		// lwz     r4,60(r1)
		PL(0x80a10040),		// lwz     r5,64(r1)
		PL(0x00000001),		// <named_check_load_invoc>
		PL(0x80610040),		// lwz     r3,64(r1)
		PL(0x8001004c),		// lwz     r0,76(r1)
		PL(0x7c0803a6),		// mtlr    r0
		PL(0x38210044),		// addi    r1,r1,68
		PL(0x4e800020)		// blr
	};
	const uint32 get_named_resource_template_size = sizeof(get_named_resource_template);

	xlm_index = -1, check_load_invoc_index = -1;
	for (int i = 0; i < get_resource_template_size/4; i++) {
		uint32 opcode = ntohl(get_resource_template[i]);
		switch (opcode) {
		case 0x00000000:
			xlm_index = i;
			break;
		case 0x00000001:
			check_load_invoc_index = i;
			break;
		}
	}
	assert(xlm_index != -1 && check_load_invoc_index != -1);

	// GetNamedResource()
	get_named_resource_func = base = SheepMem::Reserve(get_named_resource_template_size);
	Host2Mac_memcpy(base, get_named_resource_template, get_named_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_NAMED_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);

	// Get1NamedResource()
	get_1_named_resource_func = base = SheepMem::Reserve(get_named_resource_template_size);
	Host2Mac_memcpy(base, get_named_resource_template, get_named_resource_template_size);
	WriteMacInt32(base + xlm_index * 4, 0x80000000 | XLM_GET_1_NAMED_RESOURCE);
	WriteMacInt32(base + check_load_invoc_index * 4, check_load_invoc_opcode);
}
#endif


/*
 *  Initialize the thunks system
 */

struct native_op_t {
	uint32 tvect;
	uint32 func;
	SheepRoutineDescriptor *desc;
};
static native_op_t native_op[NATIVE_OP_MAX];

bool ThunksInit(void)
{
#if EMULATED_PPC
	for (int i = 0; i < NATIVE_OP_MAX; i++) {
		uintptr base = SheepMem::Reserve(16);
		WriteMacInt32(base + 0, base + 8);
		WriteMacInt32(base + 4, 0); // Fake TVECT
		WriteMacInt32(base + 8, NativeOpcode(i));
		WriteMacInt32(base + 12, POWERPC_BLR);
		native_op[i].tvect = base;
		native_op[i].func  = base + 8;
	}
#if POWERPC_GET_RESOURCE_THUNKS
	generate_powerpc_thunks();
	native_op[NATIVE_GET_RESOURCE].func = get_resource_func;
	native_op[NATIVE_GET_1_RESOURCE].func = get_1_resource_func;
	native_op[NATIVE_GET_IND_RESOURCE].func = get_ind_resource_func;
	native_op[NATIVE_GET_1_IND_RESOURCE].func = get_1_ind_resource_func;
	native_op[NATIVE_R_GET_RESOURCE].func = r_get_resource_func;
	native_op[NATIVE_GET_NAMED_RESOURCE].func = get_named_resource_func;
	native_op[NATIVE_GET_1_NAMED_RESOURCE].func = get_1_named_resource_func;
#endif
#else
#if defined(__linux__) || defined(__NetBSD__) || (defined(__APPLE__) && defined(__MACH__))
#define DEFINE_NATIVE_OP(ID, FUNC) do {				\
		uintptr base = SheepMem::Reserve(8);		\
		WriteMacInt32(base + 0, (uint32)FUNC);		\
		WriteMacInt32(base + 4, (uint32)TOC);		\
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
	// FIXME: add GetResource() and friends for completeness
	DEFINE_NATIVE_OP(NATIVE_PATCH_NAME_REGISTRY, DoPatchNameRegistry);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_INSTALL_ACCEL, VideoInstallAccel);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_VBL, VideoVBL);
	DEFINE_NATIVE_OP(NATIVE_VIDEO_DO_DRIVER_IO, VideoDoDriverIO);
	DEFINE_NATIVE_OP(NATIVE_ETHER_AO_GET_HWADDR, AO_get_ethernet_address);
	DEFINE_NATIVE_OP(NATIVE_ETHER_AO_ADD_MULTI, AO_enable_multicast);
	DEFINE_NATIVE_OP(NATIVE_ETHER_AO_DEL_MULTI, AO_disable_multicast);
	DEFINE_NATIVE_OP(NATIVE_ETHER_AO_SEND_PACKET, AO_transmit_packet);
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
	DEFINE_NATIVE_OP(NATIVE_NQD_SYNC_HOOK, NQD_sync_hook);
	DEFINE_NATIVE_OP(NATIVE_NQD_BITBLT_HOOK, NQD_bitblt_hook);
	DEFINE_NATIVE_OP(NATIVE_NQD_FILLRECT_HOOK, NQD_fillrect_hook);
	DEFINE_NATIVE_OP(NATIVE_NQD_UNKNOWN_HOOK, NQD_unknown_hook);
	DEFINE_NATIVE_OP(NATIVE_NQD_BITBLT, NQD_bitblt);
	DEFINE_NATIVE_OP(NATIVE_NQD_INVRECT, NQD_invrect);
	DEFINE_NATIVE_OP(NATIVE_NQD_FILLRECT, NQD_fillrect);
#undef DEFINE_NATIVE_OP
#endif

	// Initialize routine descriptors (if TVECT exists)
	for (int i = 0; i < NATIVE_OP_MAX; i++) {
		uint32 tvect = native_op[i].tvect;
		if (tvect)
			native_op[i].desc = new SheepRoutineDescriptor(0, tvect);
	}

	return true;
}


/*
 *  Delete generated thunks
 */

void ThunksExit(void)
{
	for (int i = 0; i < NATIVE_OP_MAX; i++) {
		SheepRoutineDescriptor *desc = native_op[i].desc;
		if (desc)
			delete desc;
	}
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
 *  Return the routine descriptor address of the native function
 */

uint32 NativeRoutineDescriptor(int selector)
{
	assert(selector < NATIVE_OP_MAX);
	SheepRoutineDescriptor * const desc = native_op[selector].desc;
	assert(desc != 0);
	return desc->addr();
}


/*
 *  Execute native code from EMUL_OP routine (real mode switch)
 */

void ExecuteNative(int selector)
{
	M68kRegisters r;
	Execute68k(NativeRoutineDescriptor(selector), &r);
}
