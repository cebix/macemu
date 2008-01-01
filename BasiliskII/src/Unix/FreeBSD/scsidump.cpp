/*
 *  scsidump.cpp - SCSI Dump (to see FreeBSD remappings)
 *  Compile as (CAM version): gcc -I/sys -DCAM -o scsidump scsidump.cpp -lcam
 *        (old SCSI version): gcc -o scsidump scsidump.cpp -lscsi
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
 *                Implemented old SCSI manager for FreeBSD 2.x.
 *                (Note: This implementation have been never tested;
 *                 I don't own anymore a machine with FreeBSD 2.x,
 *                 so if something goes wrong, please mail me at
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

/*
 * _Build_SCSI_Controller()
 *
 * This function builds a virtual SCSI Controller (Controller=0)
 * where keeps all the devices found, this is due the fact
 * I have two SCSI controllers in my PC. :-)
 * Use FreeBSD-SCSIDump in contrib/ to see how is remapped your 
 * SCSI device (only if you have more than one controller, 
 * that's for sure :-).
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
	
	// Find out the duplicate IDs and change them
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
	dev = Devices;
	while(dev) {
		aux = dev;
		while(aux) {
			SCSIDevice* dev1, * dev2;
	
			dev1 = dev, dev2 = aux;
/*			
			if(dev1->faked_unit>dev2->faked_unit) {
				SCSIDevice tmp;
				
				memcpy(&tmp, dev1, sizeof(SCSIDevice));
				memcpy(dev1, dev2, sizeof(SCSIDevice));
				memcpy(dev2, &tmp, sizeof(SCSIDevice));
			}
			*/
			aux = aux->next;
		}
		dev = dev->next;
	}
}	

#define SCSIReset()
	
/*
 *  Initialization
 */

void SCSIInit(void)
{
	// Find the SCSI hosts in the system
	// Filling out the SCSIDevices queue.
	// Stolen from camcontrol.c
	int bufsize, fd;
	int need_close = 0;
	int error = 0;
	int skip_device = 0;
	SCSIDevice* Dev, * dev, * PrevDev = NULL;
	
	nDevices = 0;

#ifdef CAM
	union ccb ccb;

	if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		fprintf(stderr, "Cannot open CAM device: %s\n", XPT_DEVICE);
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
	 * I only scan for the first bus, LUN 0, and the 
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
	fprintf(stderr, "Device                            RealID FkdID\n");
	fprintf(stderr, "----------------------------------------------\n");
		      // 012345678901234567890123456789012 0:0:0    0/0
	dev = Devices;
	while(dev) {
		char tmp[40];
		snprintf(tmp, sizeof(tmp), "%s %s %s",
			dev->vendor,
			dev->product,
			dev->revision);
		fprintf(stderr, "%-33s %d:%d:%d    %d/%d\n",
			tmp, dev->controller, dev->unit, dev->lun,
			dev->faked_unit, dev->lun);
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

int main()
{
	SCSIInit();
	SCSIExit();
	return 0;
}
