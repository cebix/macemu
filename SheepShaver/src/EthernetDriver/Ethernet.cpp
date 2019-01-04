/*
 *  Ethernet.cpp - SheepShaver ethernet PCI driver stub
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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
#include <stddef.h>
#include "xlowmem.h"
#include "ether_defs.h"


// Macro for tail-calling native code from assembly functions
#define ASM_TAIL_CALL_NATIVE(NAME) \
	lwz		r0,XLM_##NAME(r0)	;\
	lwz		r2,XLM_TOC(r0)		;\
	mtctr	r0					;\
	bctr

// Macro for calling native code from assembly functions
#define ASM_CALL_NATIVE(NAME)	 \
	mflr	r0					;\
	stw		r2,12(r1)			;\
	stw		r0,8(r1)			;\
	stwu	r1,-64(r1)			;\
	lwz		r0,XLM_##NAME(r0)	;\
	lwz		r2,XLM_TOC(r0)		;\
	mtlr	r0					;\
	blrl						;\
	lwz		r0,64+8(r1)			;\
	lwz		r2,64+12(r1)		;\
	mtlr	r0					;\
	addi	r1,r1,64			;\
	blr


/*
 *  Driver Description structure
 */

struct DriverDescription {
	uint32 driverDescSignature;
	uint32 driverDescVersion;
	char nameInfoStr[32];
	uint32 version;
	uint32 driverRuntime;
	char driverName[32];
	uint32 driverDescReserved[8];
	uint32 nServices;
	uint32 serviceCategory;
	uint32 serviceType;
	uint32 serviceVersion;
};

#pragma export on
DriverDescription TheDriverDescription = {
	'mtej',
	0,
	"\pSheepShaver Ethernet",
	0x01008000,	// V1.0.0final
	4,			// kDriverIsUnderExpertControl
	"\penet",
	0, 0, 0, 0, 0, 0, 0, 0,
	1,
	'otan',
	0x000a0b01,	// Ethernet, Framing: Ethernet/EthernetIPX/802.2, IsDLPI
	0x01000000,	// V1.0.0
};
#pragma export off


/*
 *  install_info and related structures
 */

#ifdef BUILD_ETHER_FULL_DRIVER
#define ETHERDECL extern
#else
#define ETHERDECL static
#endif

ETHERDECL int ether_open(queue_t *rdq, void *dev, int flag, int sflag, void *creds);
ETHERDECL int ether_close(queue_t *rdq, int flag, void *creds);
ETHERDECL int ether_wput(queue_t *q, msgb *mp);
ETHERDECL int ether_wsrv(queue_t *q);
ETHERDECL int ether_rput(queue_t *q, msgb *mp);
ETHERDECL int ether_rsrv(queue_t *q);

struct ot_module_info {
	uint16 mi_idnum;
	char *mi_idname;
	int32 mi_minpsz; // Minimum packet size
	int32 mi_maxpsz; // Maximum packet size
	uint32 mi_hiwat; // Queue hi-water mark
	uint32 mi_lowat; // Queue lo-water mark
};

static ot_module_info module_information = {
	kEnetModuleID,
	"SheepShaver Ethernet",
	0,
	kEnetTSDU,
	6000,
	5000
};

typedef int (*putp_t)(queue_t *, msgb *);
typedef int (*srvp_t)(queue_t *);
typedef int (*openp_t)(queue_t *, void *, int, int, void *);
typedef int (*closep_t)(queue_t *, int, void *);

struct qinit {
	putp_t qi_putp;
	srvp_t qi_srvp;
	openp_t qi_qopen;
	closep_t qi_qclose;
	void *qi_qadmin;
	struct ot_module_info *qi_minfo;
	void *qi_mstat;
};

static qinit read_side = {
	NULL,
	ether_rsrv,
	ether_open,
	ether_close,
	NULL,
	&module_information,
	NULL
};

static qinit write_side = {
	ether_wput,
	NULL,
	ether_open,
	ether_close,
	NULL,
	&module_information,
	NULL
};

struct streamtab {
	struct qinit *st_rdinit;
	struct qinit *st_wrinit;
	struct qinit *st_muxrinit;
	struct qinit *st_muxwinit;
};

static streamtab the_streamtab = {
	&read_side,
	&write_side,
	NULL,
	NULL
};

struct install_info {
	struct streamtab *install_str;
	uint32 install_flags;
	uint32 install_sqlvl;
	char *install_buddy;
	void *ref_load;
	uint32 ref_count;
};

enum {
	kOTModIsDriver = 0x00000001,
	kOTModUpperIsDLPI = 0x00002000,
	SQLVL_MODULE = 3,
};

static install_info the_install_info = {
	&the_streamtab,
	kOTModIsDriver /*| kOTModUpperIsDLPI */,
	SQLVL_MODULE,
	NULL,
	NULL,
	0
};


// Prototypes for exported functions
extern "C" {
#pragma export on
extern uint32 ValidateHardware(void *theID);
extern install_info* GetOTInstallInfo();
extern uint8 InitStreamModule(void *theID);
extern void TerminateStreamModule(void);
#pragma export off
}


/*
 *  Validate that our hardware is available (always available)
 */

uint32 ValidateHardware(void *theID)
{
	return 0;
}


/*
 *  Return pointer to install_info structure
 */

install_info *GetOTInstallInfo(void)
{
	return &the_install_info;
}

/*
 *  Init module
 */

#ifdef BUILD_ETHER_FULL_DRIVER
asm bool NativeInitStreamModule(register void *theID)
{
	ASM_CALL_NATIVE(ETHER_INIT)
}
#else
asm uint8 InitStreamModule(register void *theID)
{
	ASM_TAIL_CALL_NATIVE(ETHER_INIT)
}
#endif


/*
 *  Terminate module
 */

#ifdef BUILD_ETHER_FULL_DRIVER
asm void NativeTerminateStreamModule(void)
{
	ASM_CALL_NATIVE(ETHER_TERM)
}
#else
asm void TerminateStreamModule(void)
{
	ASM_TAIL_CALL_NATIVE(ETHER_TERM)
}
#endif


/*
 *  DLPI functions
 */

#ifndef BUILD_ETHER_FULL_DRIVER
static asm int ether_open(register queue_t *rdq, register void *dev, register int flag, register int sflag, register void *creds)
{
	ASM_TAIL_CALL_NATIVE(ETHER_OPEN)
}

static asm int ether_close(register queue_t *rdq, register int flag, register void *creds)
{
	ASM_TAIL_CALL_NATIVE(ETHER_CLOSE)
}

static asm int ether_wput(register queue_t *q, register msgb *mp)
{
	ASM_TAIL_CALL_NATIVE(ETHER_WPUT)
}

static asm int ether_rsrv(register queue_t *q)
{
	ASM_TAIL_CALL_NATIVE(ETHER_RSRV)
}
#endif


/*
 *  Hooks to add-on low-level functions
 */

asm void AO_get_ethernet_address(register uint32)
{
	ASM_CALL_NATIVE(ETHER_AO_GET_HWADDR)
}

asm void AO_enable_multicast(register uint32 addr)
{
	ASM_CALL_NATIVE(ETHER_AO_ADD_MULTI)
}

asm void AO_disable_multicast(register uint32 addr)
{
	ASM_CALL_NATIVE(ETHER_AO_DEL_MULTI)
}

asm void AO_transmit_packet(register uint32 mp)
{
	ASM_CALL_NATIVE(ETHER_AO_SEND_PACKET)
}
