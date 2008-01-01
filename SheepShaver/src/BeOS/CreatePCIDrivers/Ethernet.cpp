/*
 *  Ethernet.cpp - SheepShaver ethernet PCI driver stub
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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

static int ether_open(queue_t *rdq, void *dev, int flag, int sflag, void *creds);
static int ether_close(queue_t *rdq, int flag, void *creds);
static int ether_wput(queue_t *q, msgb *mp);
static int ether_wsrv(queue_t *q);
static int ether_rput(queue_t *q, msgb *mp);
static int ether_rsrv(queue_t *q);

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

asm uint8 InitStreamModule(register void *theID)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_INIT
	mtctr	r0
	bctr
}


/*
 *  Terminate module
 */

asm void TerminateStreamModule(void)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_TERM
	mtctr	r0
	bctr
}


/*
 *  DLPI functions
 */

static asm int ether_open(register queue_t *rdq, register void *dev, register int flag, register int sflag, register void *creds)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_OPEN
	mtctr	r0
	bctr
}

static asm int ether_close(register queue_t *rdq, register int flag, register void *creds)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_CLOSE
	mtctr	r0
	bctr
}

static asm int ether_wput(register queue_t *q, register msgb *mp)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_WPUT
	mtctr	r0
	bctr
}

static asm int ether_rsrv(register queue_t *q)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_ETHER_RSRV
	mtctr	r0
	bctr
}
