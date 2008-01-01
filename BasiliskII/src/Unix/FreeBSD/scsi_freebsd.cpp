/*
 *  scsi_freebsd.cpp - SCSI Manager, FreeBSD SCSI Driver implementation
 *  Copyright (C) 1999 Orlando Bassotto
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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
 *
 *  History:
 *    29-Jun-1999 Started
 *    05-Jul-1999 Changed from array to queue removing the limit of 8 
 *                devices.
 *                Implemented old SCSI management for FreeBSD 2.x.
 *                (Note: This implementation hasn't been tested;
 *                 I don't own anymore a machine with FreeBSD 2.x,
 *                 so if something goes wrong, please mail me to
 *                 future@mediabit.net).
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>

#ifdef CAM
#include <cam/cam.h>
#include <cam/cam_debug.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/scsi/scsi_pass.h>
#include <cam/scsi/scsi_message.h>
#include <camlib.h>
#else /* !CAM */
#include <sys/scsiio.h>
#include <scsi.h>
#endif /* !CAM */

#include "sysdeps.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "scsi.h"

#define DEBUG 0
#include "debug.h"


#undef u_int8_t
#define u_int8_t unsigned char

typedef struct _SCSIDevice {
	int		controller;		// SCSI Controller
	int		controller_bus;		// SCSI Controller Bus
	char		controller_name[33];	// SCSI Controller name
 	int		mac_unit;		// Macintosh SCSI ID (remapped)
	int		faked_controller;	// "Faked" SCSI Controller (Always 0)
	int		faked_unit;		// "Faked" SCSI ID
	int		unit;			// Real SCSI ID
	int		lun;			// Real SCSI LUN
	u_int8_t	vendor[16];		// SCSI Vendor
	u_int8_t	product[48];		// SCSI Product
	u_int8_t	revision[16];		// SCSI Revision
	char		device[33];		// SCSI Device
#ifdef CAM
	char		pass_device[33];	// SCSI Pass Device
#else /* !CAM */
	int		dev_fd;			// Device File Descriptor
#endif /* !CAM */
	void*		dev_ptr;		// Pointer to CAM/SCSI structure
	bool		enabled;		// Device enabled ?
	struct _SCSIDevice* next;		// Pointer to the next device
} SCSIDevice;

static int nDevices = 0;
static SCSIDevice* Devices = NULL;

static uint32 buffer_size;
static uint8* buffer = NULL;

static uint8 the_cmd[12];
static int the_cmd_len;

static SCSIDevice* CurrentDevice = NULL;

inline static SCSIDevice* _GetSCSIDeviceByID(int id)
{
	SCSIDevice* aux = Devices;
	while(aux) {
		if(aux->faked_unit==id) {
			return aux;
		}
		aux = aux->next;
	}
	return NULL;
}

inline static SCSIDevice* _GetSCSIDeviceByIDLUN(int id, int lun)
{
	SCSIDevice* aux = Devices;
	while(aux) {
		if(aux->faked_unit==id&&aux->lun==lun) {
			return aux;
		}
		aux = aux->next;
	}
	return NULL;
}

inline static SCSIDevice* _GetSCSIDeviceByMacID(int id)
{
	SCSIDevice* aux = Devices;
	while(aux) {
		if(aux->mac_unit==id) {
			return aux;
		}
		aux = aux->next;
	}
	return NULL;
}

inline static SCSIDevice* _GetSCSIDeviceByMacIDLUN(int id, int lun)
{
	SCSIDevice* aux = Devices;
	while(aux) {
		if(aux->mac_unit==id&&aux->lun==lun) {
			return aux;
		}
		aux = aux->next;
	}
	return NULL;
}

inline static SCSIDevice* _AllocNewDevice()
{
	SCSIDevice* aux;
	
	aux = new SCSIDevice;
	if(aux==NULL) return NULL;
	memset(aux, 0, sizeof(SCSIDevice));
	aux->next = Devices;
	Devices = aux;
	return aux;
}

#ifdef CAM
inline static struct cam_device* _GetCurrentSCSIDevice()
{
	if(CurrentDevice==NULL) return NULL;
	
	return (struct cam_device*)CurrentDevice->dev_ptr;
}
#else /* !CAM */
inline static struct scsireq* _GetCurrentSCSIDevice()
{
	if(CurrentDevice==NULL) return NULL;
	
	return (struct scsireq*)CurrentDevice->dev_ptr;
}
#endif /* !CAM */

/*
 * _Build_SCSI_Controller()
 *
 * This function builds a virtual SCSI Controller (Controller=0)
 * where keeps all the devices found, this is due the fact
 * I have two SCSI controllers in my PC. :-)
 * Use scsidump in contrib/ to see how is remapped your 
 * SCSI device (only if you have more than one controller, 
 * that's for sure :-).
 * If you have only one controller, remapping does not take act.
 */

#define GET_FREE_ID(id) \
	{ \
		for(int x=0;x<32;x++) { \
			if(!(busyIDs&(1<<(x+1)))) { \
				id = x; \
				break; \
			} \
		} \
	}
	
static void _Build_SCSI_Controller()
{
	unsigned int id = 0;
	unsigned long long busyIDs = 0x0ll;
	SCSIDevice* aux, * dev;
		
	// What IDs are busy?
	dev = Devices;
	while(dev) {
		dev->enabled = false;
		dev->faked_controller = 0;
		dev->faked_unit = dev->unit;
		busyIDs |= (1 << (dev->unit+1));
		dev = dev->next;
	}
	
	// Find out the duplicate IDs and remap them
	dev = Devices, aux = NULL;
	while(dev) {
		aux = dev;
		while(aux) {
			SCSIDevice* dev1, * dev2;
			
			dev1 = dev, dev2 = aux;
			
			if(dev1->controller!=dev2->controller&&
				dev1->unit==dev2->unit) {
				int free_id;
				GET_FREE_ID(free_id);
				busyIDs |= (1<<(free_id+1));
				dev1->faked_unit = free_id;
			}
			aux = aux->next;
		}
		dev = dev->next;
	}
	
	// Now reorder the queue 
#if 0
	dev = Devices;
	while(dev) {
		aux = dev;
		while(aux) {
			SCSIDevice* dev1, * dev2;
	
			dev1 = dev, dev2 = aux;
			if(dev1->faked_unit>dev2->faked_unit) {
				SCSIDevice tmp;
				
				memcpy(&tmp, dev1, sizeof(SCSIDevice));
				memcpy(dev1, dev2, sizeof(SCSIDevice));
				memcpy(dev2, &tmp, sizeof(SCSIDevice));
			}
			aux = aux->next;
		}
		dev = dev->next;
	}
#endif

	// Now open the selected SCSI devices :-)
	for(int n=0;n<8;n++) {
		char tmp[25];
		
		snprintf(tmp, sizeof(tmp), "scsi%d", n);
		const char* scsi = PrefsFindString(tmp);
		if(scsi) {
			int id, lun;
			
			// The format is: RemappedID (or FakedID)/LUN
			sscanf(scsi, "%d/%d", &id, &lun);
			
			SCSIDevice* dev = _GetSCSIDeviceByIDLUN(id, lun);
			if(dev==NULL) continue;
			dev->enabled = true;
			dev->mac_unit = n;

#ifdef CAM
			struct cam_device* cam;

			cam = cam_open_btl(dev->controller,
				dev->unit,
				dev->lun, O_RDWR, NULL);
			if(cam==NULL) {
				fprintf(stderr, "Failed to open %d:%d:%d = %s!!!\n",
					dev->controller, dev->unit, dev->lun,
					cam_errbuf);
			}
			dev->dev_ptr = (void*)cam;
#else /* !CAM */
			dev->dev_fd = scsi_open(dev->device, O_RDWR);
			if(dev->dev_fd<0) {
				perror("Failed to open %d:%d:%d");
			}
			else {
				dev->dev_ptr = (void*)scsireq_new();
			}
#endif /* !CAM */
		}
	}
}	

	
/*
 *  Initialization
 */

void SCSIInit(void)
{
	// Finds the SCSI hosts in the system filling the SCSIDevices queue.
	// "Stolen" from camcontrol.c
	//    Copyright (C) 1997-99 Kenneth D. Merry
	// Old SCSI detection "stolen" from scsi.c
	//    Copyright (C) 1993 Julian Elischer
        //
	int bufsize, fd;
	int need_close = 0;
	int error = 0;
	int skip_device = 0;
	SCSIDevice* Dev, * dev, * PrevDev = NULL;
	
	nDevices = 0;

	if(PrefsFindBool("noscsi"))
		goto no_scsi;

#ifdef CAM
	union ccb ccb;
	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		fprintf(stderr, "WARNING: Cannot open CAM device %s (%s)\n", XPT_DEVICE, strerror(errno));
		goto no_scsi;
	}
	
	memset(&(&ccb.ccb_h)[1], 0, sizeof(struct ccb_dev_match)-sizeof(struct ccb_hdr));
	ccb.ccb_h.func_code = XPT_DEV_MATCH;
	bufsize = sizeof(struct dev_match_result) * 100;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
	ccb.cdm.num_matches = 0;
	
	ccb.cdm.num_patterns = 0;
	ccb.cdm.pattern_buf_len = 0;
	
	do {
		Dev = _AllocNewDevice();	
		if(ioctl(fd, CAMIOCOMMAND, &ccb)==-1) {
			fprintf(stderr, "Error sending CAMIOCOMMAND ioctl\n");
			return;
		}
		
		if((ccb.ccb_h.status != CAM_REQ_CMP)
		|| ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
		 && (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
		 	fprintf(stderr, "Got CAM error %#x, CDM error %d\n",
		 		ccb.ccb_h.status, ccb.cdm.status);
		 	return;
		}
		 
		char current_controller_name[33];
		int current_controller = -1;
		for(int i=0;i<ccb.cdm.num_matches;i++) {
			switch(ccb.cdm.matches[i].type) {
			case DEV_MATCH_BUS:
			{
				struct bus_match_result* bus_result;
								
				bus_result = &ccb.cdm.matches[i].result.bus_result;
				
				if(bus_result->path_id==-1) break;
				Dev->controller = bus_result->path_id;
				snprintf(Dev->controller_name, sizeof(Dev->controller_name), "%s%d", 
					bus_result->dev_name,
					bus_result->unit_number);
				strncpy(current_controller_name, Dev->controller_name, sizeof(current_controller_name));
				current_controller = Dev->controller;
				Dev->controller_bus = bus_result->bus_id;
				break;
			}
			case DEV_MATCH_DEVICE:
			{
				struct device_match_result* dev_result;
				char tmpstr[256];
				
				dev_result = &ccb.cdm.matches[i].result.device_result;
				if(current_controller==-1||dev_result->target_id==-1) {
					skip_device = 1;
					break;
				}
				else skip_device = 0;
				
				cam_strvis(Dev->vendor, (u_int8_t*)dev_result->inq_data.vendor,
					sizeof(dev_result->inq_data.vendor),
					sizeof(Dev->vendor));
				cam_strvis(Dev->product, (u_int8_t*)dev_result->inq_data.product,
					sizeof(dev_result->inq_data.product),
					sizeof(Dev->product));
				cam_strvis(Dev->revision, (u_int8_t*)dev_result->inq_data.revision,
					sizeof(dev_result->inq_data.revision),
					sizeof(Dev->revision));
				strncpy(Dev->controller_name, current_controller_name, sizeof(Dev->controller_name));
				Dev->controller = current_controller;
				Dev->unit = dev_result->target_id;
				Dev->lun = dev_result->target_lun;
				break;
			}
			case DEV_MATCH_PERIPH:
			{
				struct periph_match_result* periph_result;
				
				periph_result = &ccb.cdm.matches[i].result.periph_result;
				
				if(skip_device != 0) break;
				
				if(need_close==1) {
					snprintf(Dev->device, sizeof(Dev->device), "%s%d*",
						periph_result->periph_name,
						periph_result->unit_number);
					need_close = 0;
				}
				else if(need_close==0) {
					snprintf(Dev->pass_device, sizeof(Dev->pass_device), "%s%d",
						periph_result->periph_name,
						periph_result->unit_number);
					need_close++;
					break;
				}
				else {
					need_close = 0;
				}
				PrevDev = Dev;
				Dev = _AllocNewDevice();
				break;
			}
			}
		}
	} while (ccb.ccb_h.status == CAM_REQ_CMP
	     && ccb.cdm.status == CAM_DEV_MATCH_MORE);

	/* Remove last one (ugly coding) */
	Devices = PrevDev;
	delete Dev;
end_loop:
	close(fd);
#else /* !CAM */
	/*
	 * FreeBSD 2.x SCSI management is quiet different and 
	 * unfortunatly not flexible as CAM library in FreeBSD 3.x...
	 * I probe only the first bus, LUN 0, and the 
	 * first 8 devices only.
	 */
	u_char* inq_buf;
	scsireq_t* scsireq;
	struct scsi_addr scsi;
	int ssc_fd;
	
	if((ssc_fd=open("/dev/ssc", O_RDWR))==-1) {
		fprintf(stderr, "Cannot open SCSI manager: /dev/ssc\n");
		SCSIReset();
		return;
	}	
	
	inq_buf = (u_char*)malloc(96);
	if(inq_buf==NULL) {
		perror("malloc failed");
		SCSIReset();
		return;
	}
	
	scsireq = scsireq_build((scsireq_t*)dev->dev_ptr,
				96, inq_buf, SCCMD_READ,
				"12 0 0 0 v 0", 96);
	
	addr.scbus = 0;
	addr.lun = 0;

	for(int n=0;n<8;n++) {
		addr.target = n;
		
		if(ioctl(ssc_fd, SCIOCADDR, &addr) != -1) {
			Dev = _AllocNewDevice();
			Dev->controller = addr.scbus;
			Dev->lun = addr.lun;
			Dev->unit = addr.target;

			struct scsi_devinfo devInfo;
			devInfo.addr = addr;
			if(ioctl(ssc_fd, SCIOCGETDEVINFO, &devInfo) != -1) {
				strncpy(Dev->device, devInfo.devname, sizeof(Dev->device));
			}
			strncpy(Dev->controller_name, "FreeBSD 2.x SCSI Manager", sizeof(Dev->controller_name));
			if(scsireq_enter(ssc_fd, scsireq)!=-1) {
				Dev->vendor[sizeof(Dev->vendor)-1] = 0;
				Dev->product[sizeof(Dev->product)-1] = 0;
				Dev->revision[sizeof(Dev->revision)-1] = 0;
				
				scsireq_decode(scsireq, "s8 c8 c16 c4",
					Dev->vendor, Dev->product, Dev->revision);
			}			
		}
	}
	free(inq_buf);
	close(ssc_fd);
#endif /* !CAM */
	_Build_SCSI_Controller();

	// Print out the periph with ID:LUNs
	fprintf(stderr, "Device                            RealID FkdID  MacID Enabled\n");
	fprintf(stderr, "-------------------------------------------------------------\n");
		      // 012345678901234567890123456789012 0:0:0    0/0    0:0 Yes
	dev = Devices;
	while(dev) {
		char tmp[40];
		snprintf(tmp, sizeof(tmp), "%s %s %s",
			dev->vendor,
			dev->product,
			dev->revision);
		fprintf(stderr, "%-33s %d:%d:%d    %d/%d    %d:%d %s\n",
			tmp, dev->controller, dev->unit, dev->lun,
			dev->faked_unit, dev->lun,
			dev->mac_unit, dev->lun, dev->enabled?"Yes":"No");
		dev = dev->next;
	}

no_scsi:
	// Reset SCSI bus
	SCSIReset();
}


/*
 *  Deinitialization
 */

void SCSIExit(void)
{
	SCSIDevice* aux;
	while(Devices) {
		aux = Devices->next;
		if(Devices->dev_ptr!=NULL) {
#ifdef CAM
			cam_close_device((struct cam_device*)Devices->dev_ptr);
#else /* !CAM */
			free(Devices->dev_ptr); // Is this right?
			close(Devices->dev_fd); // And this one?
#endif /* !CAM */
		}
		delete Devices;
		Devices = aux;
	}
	nDevices = 0;
}


/*
 *  Set SCSI command to be sent by scsi_send_cmd()
 */

void scsi_set_cmd(int cmd_length, uint8 *cmd)
{
	the_cmd_len = cmd_length;
	memset(the_cmd, 0, sizeof(the_cmd));
	memcpy(the_cmd, cmd, the_cmd_len);
}


/*
 *  Check for presence of SCSI target
 */

bool scsi_is_target_present(int id)
{
	return (_GetSCSIDeviceByMacID(id)!=NULL&&_GetSCSIDeviceByMacID(id)->enabled);
}


/*
 *  Set SCSI target (returns false on error)
 */

bool scsi_set_target(int id, int lun)
{
	SCSIDevice* dev;

	dev = _GetSCSIDeviceByMacIDLUN(id, lun);
	if(dev==NULL) return false;
	CurrentDevice = dev;
	return true;
}


/*
 *  Send SCSI command to active target (scsi_set_command() must have been called),
 *  read/write data according to S/G table (returns false on error)
 */

static bool try_buffer(int size)
{
	if(size <= buffer_size) {
		return true;
	}
	
	D(bug("Allocating buffer of %d bytes.\n", size));
	uint8* new_buffer = (uint8*)valloc(size);
	if(new_buffer==NULL) {
		return false;
	}
	if(buffer!=NULL) free(buffer);
	buffer = new_buffer;
	buffer_size = size;
	return true;
}

bool scsi_send_cmd(size_t data_length, bool reading, int sg_size, uint8 **sg_ptr, uint32 *sg_len, uint16 *stat, uint32 timeout)
{
	int value = 0;
#ifdef CAM
#ifdef VERBOSE_CAM_DEBUG
	D(bug("Sending command %x (len=%d) to SCSI Device %d:%d:%d\n", the_cmd[0],
		the_cmd_len,
		CurrentDevice->controller,
		CurrentDevice->unit,
		CurrentDevice->lun));
	D(bug("DataLength: %d\n", data_length));
	D(bug("Reading: %d\n", reading));
	D(bug("SG Size: %d\n", sg_size));
	D(bug("Timeout: %d\n", timeout));
#endif /* VERBOSE_CAM_DEBUG */
#endif /* CAM */
	if(!try_buffer(data_length)) {
		char str[256];
		sprintf(str, GetString(STR_SCSI_BUFFER_ERR), data_length);
		ErrorAlert(str);
		return false;
	}
	
	if(!reading) {
		uint8* buffer_ptr = buffer;
		for(int i=0;i<sg_size;i++) {
			uint32 len = sg_len[i];
			memcpy(buffer, sg_ptr[i], len);
			buffer_ptr += len;
		}
	}
	
	if(the_cmd[0] == 0x03) {
		// faked cmd
		*stat = 0;
		return true;
	}


#ifdef CAM
	struct cam_device* device = _GetCurrentSCSIDevice();
	if(device==NULL) return false;

	union ccb ccb;

	memset(&ccb, 0, sizeof(ccb));

	int dir_flags = CAM_DIR_NONE;
	if(data_length>0) {
		dir_flags = reading?CAM_DIR_IN:CAM_DIR_OUT;
	}

	ccb.ccb_h.path_id = CurrentDevice->controller;
	ccb.ccb_h.target_id = CurrentDevice->unit;
	ccb.ccb_h.target_lun = CurrentDevice->lun;
	
	cam_fill_csio(&ccb.csio,
			0,
			NULL,
			dir_flags,
			MSG_SIMPLE_Q_TAG,
			(u_int8_t*)buffer,
			data_length,
			SSD_FULL_SIZE,
			the_cmd_len,
			(timeout?timeout:50)*1000);

	ccb.ccb_h.flags |= CAM_DEV_QFRZDIS;

	memcpy(ccb.csio.cdb_io.cdb_bytes, the_cmd, the_cmd_len);

	if(cam_send_ccb(device, &ccb)<0) {
		fprintf(stderr, "%d:%d:%d ", CurrentDevice->controller,
			CurrentDevice->unit, CurrentDevice->lun);
		perror("cam_send_ccb");
		return false;
	}

	value = ccb.ccb_h.status;
	*stat = ccb.csio.scsi_status;

	if((value & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		char tmp[4096];
		if((value & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR) {			
			scsi_sense_string(device, &ccb.csio, tmp, sizeof(tmp));
			fprintf(stderr, "SCSI Status Error:\n%s\n", tmp);
			return false;
		}
	}
#else /* !CAM */
	struct scsireq* scsireq = _GetCurrentSCSIDevice();
	if(device==NULL) return false;
	
	int dir_flags = 0x00;
	if(data_length>0) dir_flags = reading?SCCMD_READ:SCCMD_WRITE;
	
	scsireq_reset(scsireq);
	scsireq->timeout = (timeout?timeout:50)*1000;
	scsireq_build(scsireq, data_length,
			(caddr_t)buffer, dir_flags,
			"0");
	memcpy(scsireq->cmd, the_cmd, scsireq->cmdlen = the_cmd_len);
	
	int result = scsi_enter(dev->dev_fd, scsireq);
	if(SCSIREQ_ERROR(result)) {
		scsi_debug(stderr, result, scsireq);
	}
	*stat = scsireq->status;
#endif /* !CAM */

	if(reading) {
		uint8* buffer_ptr = buffer;
		for(int i=0;i<sg_size;i++) {
			uint32 len = sg_len[i];
			memcpy(sg_ptr[i], buffer_ptr, len);
#ifdef CAM
#ifdef VERBOSE_CAM_DEBUG
			static char line[16];
			for(int r=0, x=0;x<len;x++) {
				if(x!=0&&x%16==0) { D(bug("%s\n", line)); r = 0; } 
				line[r++] = isprint(sg_ptr[i][x])?sg_ptr[i][x]:'.';
				line[r] = 0;
				D(bug("%02x ", sg_ptr[i][x]));
			}
#endif /* VERBOSE_CAM_DEBUG */
#endif /* CAM */
			buffer_ptr += len;
		}
	}
	return true;
}
