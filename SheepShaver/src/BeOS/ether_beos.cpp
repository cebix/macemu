/*
 *  ether_beos.cpp - SheepShaver Ethernet Device Driver (DLPI), BeOS specific stuff
 *
 *  SheepShaver (C) 1997-2008 Marc Hellwig and Christian Bauer
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
#include "ether.h"
#include "ether_defs.h"
#include "prefs.h"
#include "xlowmem.h"
#include "main.h"
#include "user_strings.h"
#include "sheep_net.h"

#define DEBUG 0
#include "debug.h"

#define STATISTICS 0
#define MONITOR 0


// Global variables
static thread_id read_thread;				// Packet receiver thread
static bool ether_thread_active = true;		// Flag for quitting the receiver thread

static area_id buffer_area;					// Packet buffer area
static net_buffer *net_buffer_ptr;			// Pointer to packet buffer
static sem_id read_sem, write_sem;			// Semaphores to trigger packet reading/writing
static uint32 rd_pos;						// Current read position in packet buffer
static uint32 wr_pos;						// Current write position in packet buffer

static bool net_open = false;				// Flag: initialization succeeded, network device open


// Prototypes
static status_t AO_receive_thread(void *data);


/*
 *  Initialize ethernet
 */

void EtherInit(void)
{
	// Do nothing if the user disabled the network
	if (PrefsFindBool("nonet"))
		return;

	// find net-server team
i_wanna_try_that_again:
	bool found_add_on = false;
	team_info t_info;
	int32 t_cookie = 0;
	image_info i_info;
	int32 i_cookie = 0;
	while (get_next_team_info(&t_cookie, &t_info) == B_NO_ERROR) {
		if (strstr(t_info.args,"net_server")!=NULL) {
			// check if sheep_net add-on is loaded
			while (get_next_image_info(t_info.team,&i_cookie,&i_info) == B_NO_ERROR) {
				if (strstr(i_info.name,"sheep_net")!=NULL) {
					found_add_on = true;
					break;					
				}
			}
		} 
		if (found_add_on) break;
	}
	if (!found_add_on) {

		// Search for sheep_net in network config file
		char str[1024];
		bool sheep_net_found = false;
		FILE *fin = fopen("/boot/home/config/settings/network", "r");
		while (!feof(fin)) {
			fgets(str, 1024, fin);
			if (strstr(str, "PROTOCOLS"))
				if (strstr(str, "sheep_net"))
					sheep_net_found = true;
		}
		fclose(fin);

		// It was found, so something else must be wrong
		if (sheep_net_found) {
			WarningAlert(GetString(STR_NO_NET_ADDON_WARN));
			return;
		}

		// Not found, inform the user
		if (!ChoiceAlert(GetString(STR_NET_CONFIG_MODIFY_WARN), GetString(STR_OK_BUTTON), GetString(STR_CANCEL_BUTTON)))
			return;

		// Change the network config file and restart the network
		fin = fopen("/boot/home/config/settings/network", "r");
		FILE *fout = fopen("/boot/home/config/settings/network.2", "w");
		bool global_found = false;
		bool modified = false;
		while (!feof(fin)) {
			str[0] = 0;
			fgets(str, 1024, fin);
			if (!global_found && strstr(str, "GLOBAL:")) {
				global_found = true;
			} else if (global_found && !modified && strstr(str, "PROTOCOLS")) {
				str[strlen(str)-1] = 0;
				strcat(str, " sheep_net\n");
				modified = true;
			} else if (global_found && !modified && strlen(str) > 2 && str[strlen(str) - 2] == ':') {
				fputs("\tPROTOCOLS = sheep_net\n", fout);
				modified = true;
			}
			fputs(str, fout);
		}
		if (!modified)
			fputs("\tPROTOCOLS = sheep_net\n", fout);
		fclose(fout);
		fclose(fin);
		remove("/boot/home/config/settings/network.orig");
		rename("/boot/home/config/settings/network", "/boot/home/config/settings/network.orig");
		rename("/boot/home/config/settings/network.2", "/boot/home/config/settings/network");

		app_info ai;
		if (be_roster->GetAppInfo("application/x-vnd.Be-NETS", &ai) == B_OK) {
			BMessenger msg(NULL, ai.team);
			if (msg.IsValid()) {
				while (be_roster->IsRunning("application/x-vnd.Be-NETS")) {
					msg.SendMessage(B_QUIT_REQUESTED);
					snooze(500000);
				}
			}
		}
		BPath path;
		find_directory(B_BEOS_BOOT_DIRECTORY, &path);
		path.Append("Netscript");
		char *argv[3] = {"/bin/sh", (char *)path.Path(), NULL};
		thread_id net_server = load_image(2, argv, environ);
		resume_thread(net_server);
		status_t l;
		wait_for_thread(net_server, &l);
		goto i_wanna_try_that_again;
	}

	// Set up communications with add-on
	area_id handler_buffer;
	if ((handler_buffer = find_area("packet buffer")) < B_NO_ERROR) {
		WarningAlert(GetString(STR_NET_ADDON_INIT_FAILED));
		return;
	}
	if ((buffer_area = clone_area("local packet buffer", &net_buffer_ptr, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA, handler_buffer)) < B_NO_ERROR) {
		D(bug("EtherInit: couldn't clone packet area\n"));
		WarningAlert(GetString(STR_NET_ADDON_CLONE_FAILED));
		return;
	}
	if ((read_sem = create_sem(0, "ether read")) < B_NO_ERROR) {
		printf("FATAL: can't create Ethernet semaphore\n");
		return;
	}
	net_buffer_ptr->read_sem = read_sem;
	write_sem = net_buffer_ptr->write_sem;
	read_thread = spawn_thread(AO_receive_thread, "ether read", B_URGENT_DISPLAY_PRIORITY, NULL);
	resume_thread(read_thread);
	for (int i=0; i<WRITE_PACKET_COUNT; i++)
		net_buffer_ptr->write[i].cmd = IN_USE | (ACTIVATE_SHEEP_NET << 8);
	rd_pos = wr_pos = 0;
	release_sem(write_sem);

	// Everything OK
	net_open = true;
}


/*
 *  Exit ethernet
 */

void EtherExit(void)
{
	if (net_open) {

		// Close communications with add-on
		for (int i=0; i<WRITE_PACKET_COUNT; i++)
			net_buffer_ptr->write[i].cmd = IN_USE | (DEACTIVATE_SHEEP_NET << 8);
		release_sem(write_sem);

		// Quit receiver thread
		ether_thread_active = false;
		status_t result;
		release_sem(read_sem);
		while (wait_for_thread(read_thread, &result) == B_INTERRUPTED) ;

		delete_sem(read_sem);
		delete_area(buffer_area);
	}
	
#if STATISTICS
	// Show statistics
	printf("%ld messages put on write queue\n", num_wput);
	printf("%ld error acks\n", num_error_acks);
	printf("%ld packets transmitted (%ld raw, %ld normal)\n", num_tx_packets, num_tx_raw_packets, num_tx_normal_packets);
	printf("%ld tx packets dropped because buffer full\n", num_tx_buffer_full);
	printf("%ld packets received\n", num_rx_packets);
	printf("%ld packets passed upstream (%ld Fast Path, %ld normal)\n", num_rx_fastpath + num_unitdata_ind, num_rx_fastpath, num_unitdata_ind);
	printf("EtherIRQ called %ld times\n", num_ether_irq);
	printf("%ld rx packets dropped due to low memory\n", num_rx_no_mem);
	printf("%ld rx packets dropped because no stream found\n", num_rx_dropped);
	printf("%ld rx packets dropped because stream not ready\n", num_rx_stream_not_ready);
	printf("%ld rx packets dropped because no memory for unitdata_ind\n", num_rx_no_unitdata_mem);
#endif
}


/*
 *  Ask add-on for ethernet hardware address
 */

void AO_get_ethernet_address(uint32 arg)
{
	uint8 *addr = Mac2HostAddr(arg);
	if (net_open) {
		OTCopy48BitAddress(net_buffer_ptr->ether_addr, addr);
	} else {
		addr[0] = 0x12;
		addr[1] = 0x34;
		addr[2] = 0x56;
		addr[3] = 0x78;
		addr[4] = 0x9a;
		addr[5] = 0xbc;
	}
	D(bug("AO_get_ethernet_address: got address %02x%02x%02x%02x%02x%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));
}


/*
 *  Tell add-on to enable multicast address
 */

void AO_enable_multicast(uint32 addr)
{
	D(bug("AO_enable_multicast\n"));
	if (net_open) {
		net_packet *p = &net_buffer_ptr->write[wr_pos];
		if (p->cmd & IN_USE) {
			D(bug("WARNING: couldn't enable multicast address\n"));
		} else {
			Mac2host_memcpy(p->data, addr, 6);
			p->length = 6;
			p->cmd = IN_USE | (ADD_MULTICAST << 8);
			wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
			release_sem(write_sem);
		}
	}
}


/*
 *  Tell add-on to disable multicast address
 */

void AO_disable_multicast(uint32 addr)
{
	D(bug("AO_disable_multicast\n"));
	if (net_open) {
		net_packet *p = &net_buffer_ptr->write[wr_pos];
		if (p->cmd & IN_USE) {
			D(bug("WARNING: couldn't enable multicast address\n"));
		} else {
			Mac2host_memcpy(p->data, addr, 6);
			p->length = 6;
			p->cmd = IN_USE | (REMOVE_MULTICAST << 8);
			wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
			release_sem(write_sem);
		}
		D(bug("WARNING: couldn't disable multicast address\n"));
	}
}


/*
 *  Tell add-on to transmit one packet
 */

void AO_transmit_packet(uint32 mp_arg)
{
	D(bug("AO_transmit_packet\n"));
	if (net_open) {
		net_packet *p = &net_buffer_ptr->write[wr_pos];
		if (p->cmd & IN_USE) {
			D(bug("WARNING: couldn't transmit packet (buffer full)\n"));
			num_tx_buffer_full++;
		} else {
			D(bug(" write packet pos %d\n", i));
			num_tx_packets++;

			// Copy packet to buffer
			uint8 *start;
			uint8 *bp = start = p->data;
			mblk_t *mp = Mac2HostAddr(mp_arg);
			while (mp) {
				uint32 size = mp->b_wptr - mp->b_rptr;
				memcpy(bp, mp->b_rptr, size);
				bp += size;
				mp = mp->b_cont;
			}

#if MONITOR
			bug("Sending Ethernet packet:\n");
			for (int i=0; i<(uint32)(bp - start); i++) {
				bug("%02lx ", start[i]);
			}
			bug("\n");
#endif

			// Notify add-on
			p->length = (uint32)(bp - start);
			p->cmd = IN_USE | (SHEEP_PACKET << 8);
			wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
			release_sem(write_sem);
		}
	}
}


/*
 *  Packet reception thread
 */

static status_t AO_receive_thread(void *data)
{
	while (ether_thread_active) {
		if (net_buffer_ptr->read[rd_pos].cmd & IN_USE) {
			if (ether_driver_opened) {
				D(bug(" packet received, triggering Ethernet interrupt\n"));
				SetInterruptFlag(INTFLAG_ETHER);
				TriggerInterrupt();
			}
		}
		acquire_sem_etc(read_sem, 1, B_TIMEOUT, 25000);
	}
	return 0;
}


/*
 *  Ethernet interrupt
 */

void EtherIRQ(void)
{
	D(bug("EtherIRQ\n"));
	num_ether_irq++;
	OTEnterInterrupt();

	// Send received packets to OpenTransport
	net_packet *p = &net_buffer_ptr->read[rd_pos];
	while (p->cmd & IN_USE) {
		if ((p->cmd >> 8) == SHEEP_PACKET) {
			num_rx_packets++;
			D(bug(" read packet pos %d\n", i));
			uint32 size = p->length;

#if MONITOR
			bug("Receiving Ethernet packet:\n");
			for (int i=0; i<size; i++) {
				bug("%02lx ", p->data[i]);
			}
			bug("\n");
#endif

			// Wrap packet in message block
			//!! maybe use esballoc()
			mblk_t *mp;
			if ((mp = allocb(size, 0)) != NULL) {
				D(bug(" packet data at %p\n", (void *)mp->b_rptr));
				memcpy(mp->b_rptr, p->data, size);
				mp->b_wptr += size;
				ether_packet_received(mp);
			} else {
				D(bug("WARNING: Cannot allocate mblk for received packet\n"));
				num_rx_no_mem++;
			}
		}
		p->cmd = 0;	// Free packet
		rd_pos = (rd_pos + 1) % READ_PACKET_COUNT;
		p = &net_buffer_ptr->read[rd_pos];
	}
	OTLeaveInterrupt();
}
