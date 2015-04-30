/*
 *  ether_beos.cpp - Ethernet device driver, BeOS specific stuff
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *  Portions written by Marc Hellwig
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

#include <KernelKit.h>
#include <AppKit.h>
#include <StorageKit.h>
#include <support/List.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#ifdef __HAIKU__
#include <sys/select.h>
#include <netinet/in.h>
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "macos_util.h"
#include "ether.h"
#include "ether_defs.h"

#include "sheep_net.h"

#define DEBUG 0
#include "debug.h"

#define MONITOR 0


// List of attached protocols
struct NetProtocol {
	uint16 type;
	uint32 handler;
};

static BList prot_list;


// Global variables
static thread_id read_thread;				// Packet reception thread
static bool ether_thread_active = true;		// Flag for quitting the reception thread

static area_id buffer_area;					// Packet buffer area
static net_buffer *net_buffer_ptr;			// Pointer to packet buffer
static uint32 rd_pos;						// Current read position in packet buffer
static uint32 wr_pos;						// Current write position in packet buffer
static sem_id read_sem, write_sem;			// Semaphores to trigger packet reading/writing

static int fd = -1;							// UDP socket fd
static bool udp_tunnel = false;


// Prototypes
static status_t receive_proc(void *data);


/*
 *  Find protocol in list
 */

static NetProtocol *find_protocol(uint16 type)
{
	// All 802.2 types are the same
	if (type <= 1500)
		type = 0;

	// Search list (we could use hashing here but there are usually only three
	// handlers installed: 0x0000 for AppleTalk and 0x0800/0x0806 for TCP/IP)
	NetProtocol *p;
	for (int i=0; (p = (NetProtocol *)prot_list.ItemAt(i)) != NULL; i++)
		if (p->type == type)
			return p;
	return NULL;
}


/*
 *  Remove all protocols
 */

static void remove_all_protocols(void)
{
	NetProtocol *p;
	while ((p = (NetProtocol *)prot_list.RemoveItem((long)0)) != NULL)
		delete p;
}


/*
 *  Initialization
 */

bool ether_init(void)
{
	// Do nothing if no Ethernet device specified
	if (PrefsFindString("ether") == NULL)
		return false;

	// Find net_server team
i_wanna_try_that_again:
	bool found_add_on = false;
	team_info t_info;
	int32 t_cookie = 0;
	image_info i_info;
	int32 i_cookie = 0;
	while (get_next_team_info(&t_cookie, &t_info) == B_NO_ERROR) {
		if (strstr(t_info.args,"net_server")!=NULL) {

			// Check if sheep_net add-on is loaded
			while (get_next_image_info(t_info.team, &i_cookie, &i_info) == B_NO_ERROR) {
				if (strstr(i_info.name, "sheep_net") != NULL) {
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
			return false;
		}

		// Not found, inform the user
		if (!ChoiceAlert(GetString(STR_NET_CONFIG_MODIFY_WARN), GetString(STR_OK_BUTTON), GetString(STR_CANCEL_BUTTON)))
			return false;

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
		const char *argv[3] = {"/bin/sh", path.Path(), NULL};
		thread_id net_server = load_image(2, argv, (const char **)environ);
		resume_thread(net_server);
		status_t l;
		wait_for_thread(net_server, &l);
		goto i_wanna_try_that_again;
	}

	// Set up communications with add-on
	area_id handler_buffer;
	if ((handler_buffer = find_area("packet buffer")) < B_NO_ERROR) {
		WarningAlert(GetString(STR_NET_ADDON_INIT_FAILED));
		return false;
	}
	if ((buffer_area = clone_area("local packet buffer", (void **)&net_buffer_ptr, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA, handler_buffer)) < B_NO_ERROR) {
		D(bug("EtherInit: couldn't clone packet area\n"));
		WarningAlert(GetString(STR_NET_ADDON_CLONE_FAILED));
		return false;
	}
	if ((read_sem = create_sem(0, "ether read")) < B_NO_ERROR) {
		printf("FATAL: can't create Ethernet semaphore\n");
		return false;
	}
	net_buffer_ptr->read_sem = read_sem;
	write_sem = net_buffer_ptr->write_sem;
	read_thread = spawn_thread(receive_proc, "Ethernet Receiver", B_URGENT_DISPLAY_PRIORITY, NULL);
	resume_thread(read_thread);
	for (int i=0; i<WRITE_PACKET_COUNT; i++)
		net_buffer_ptr->write[i].cmd = IN_USE | (ACTIVATE_SHEEP_NET << 8);
	rd_pos = wr_pos = 0;
	release_sem(write_sem);

	// Get Ethernet address
	memcpy(ether_addr, net_buffer_ptr->ether_addr, 6);
	D(bug("Ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

	// Everything OK
	return true;
}


/*
 *  Deinitialization
 */

void ether_exit(void)
{
	// Close communications with add-on
	for (int i=0; i<WRITE_PACKET_COUNT; i++)
		net_buffer_ptr->write[i].cmd = IN_USE | (DEACTIVATE_SHEEP_NET << 8);
	release_sem(write_sem);

	// Quit reception thread
	ether_thread_active = false;
	status_t result;
	release_sem(read_sem);
	wait_for_thread(read_thread, &result);

	delete_sem(read_sem);
	delete_area(buffer_area);

	// Remove all protocols
	remove_all_protocols();
}


/*
 *  Reset
 */

void ether_reset(void)
{
	remove_all_protocols();
}


/*
 *  Add multicast address
 */

int16 ether_add_multicast(uint32 pb)
{
	net_packet *p = &net_buffer_ptr->write[wr_pos];
	if (p->cmd & IN_USE) {
		D(bug("WARNING: Couldn't enable multicast address\n"));
	} else {
		Mac2Host_memcpy(p->data, pb + eMultiAddr, 6);
		p->length = 6;
		p->cmd = IN_USE | (ADD_MULTICAST << 8);
		wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
		release_sem(write_sem);
	}
	return noErr;
}


/*
 *  Delete multicast address
 */

int16 ether_del_multicast(uint32 pb)
{
	net_packet *p = &net_buffer_ptr->write[wr_pos];
	if (p->cmd & IN_USE) {
		D(bug("WARNING: Couldn't enable multicast address\n"));
	} else {
		Mac2Host_memcpy(p->data, pb + eMultiAddr, 6);
		p->length = 6;
		p->cmd = IN_USE | (REMOVE_MULTICAST << 8);
		wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
		release_sem(write_sem);
	}
	return noErr;
}


/*
 *  Attach protocol handler
 */

int16 ether_attach_ph(uint16 type, uint32 handler)
{
	// Already attached?
	NetProtocol *p = find_protocol(type);
	if (p != NULL)
		return lapProtErr;
	else {
		// No, create and attach
		p = new NetProtocol;
		p->type = type;
		p->handler = handler;
		prot_list.AddItem(p);
		return noErr;
	}
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	NetProtocol *p = find_protocol(type);
	if (p != NULL) {
		prot_list.RemoveItem(p);
		delete p;
		return noErr;
	} else
		return lapProtErr;
}


/*
 *  Transmit raw ethernet packet
 */

int16 ether_write(uint32 wds)
{
	net_packet *p = &net_buffer_ptr->write[wr_pos];
	if (p->cmd & IN_USE) {
		D(bug("WARNING: Couldn't transmit packet (buffer full)\n"));
	} else {

		// Copy packet to buffer
		int len = ether_wds_to_buffer(wds, p->data);

#if MONITOR
		bug("Sending Ethernet packet:\n");
		for (int i=0; i<len; i++) {
			bug("%02x ", p->data[i]);
		}
		bug("\n");
#endif

		// Notify add-on
		p->length = len;
		p->cmd = IN_USE | (SHEEP_PACKET << 8);
		wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
		release_sem(write_sem);
	}
	return noErr;
}


/*
 *  Packet reception thread (non-UDP)
 */

static status_t receive_proc(void *data)
{
	while (ether_thread_active) {
		if (net_buffer_ptr->read[rd_pos].cmd & IN_USE) {
			D(bug(" packet received, triggering Ethernet interrupt\n"));
			SetInterruptFlag(INTFLAG_ETHER);
			TriggerInterrupt();
		}
		acquire_sem_etc(read_sem, 1, B_TIMEOUT, 25000);
	}
	return 0;
}


/*
 *  Packet reception thread (UDP)
 */

static status_t receive_proc_udp(void *data)
{
	while (ether_thread_active) {
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (select(fd+1, &readfds, NULL, NULL, &timeout) > 0) {
			D(bug(" packet received, triggering Ethernet interrupt\n"));
			SetInterruptFlag(INTFLAG_ETHER);
			TriggerInterrupt();
		}
	}
	return 0;
}


/*
 *  Start UDP packet reception thread
 */

bool ether_start_udp_thread(int socket_fd)
{
	fd = socket_fd;
	udp_tunnel = true;
	ether_thread_active = true;
	read_thread = spawn_thread(receive_proc_udp, "UDP Receiver", B_URGENT_DISPLAY_PRIORITY, NULL);
	resume_thread(read_thread);
	return true;
}


/*
 *  Stop UDP packet reception thread
 */

void ether_stop_udp_thread(void)
{
	ether_thread_active = false;
	status_t result;
	wait_for_thread(read_thread, &result);
}


/*
 *  Ethernet interrupt - activate deferred tasks to call IODone or protocol handlers
 */

void EtherInterrupt(void)
{
	D(bug("EtherIRQ\n"));
	EthernetPacket ether_packet;
	uint32 packet = ether_packet.addr();

	if (udp_tunnel) {

		ssize_t length;

		// Read packets from socket and hand to ether_udp_read() for processing
		while (true) {
			struct sockaddr_in from;
			socklen_t from_len = sizeof(from);
			length = recvfrom(fd, Mac2HostAddr(packet), 1514, 0, (struct sockaddr *)&from, &from_len);
			if (length < 14)
				break;
			ether_udp_read(packet, length, &from);
		}

	} else {

		// Call protocol handler for received packets
		net_packet *p = &net_buffer_ptr->read[rd_pos];
		while (p->cmd & IN_USE) {
			if ((p->cmd >> 8) == SHEEP_PACKET) {
				Host2Mac_memcpy(packet, p->data, p->length);
#if MONITOR
				bug("Receiving Ethernet packet:\n");
				for (int i=0; i<p->length; i++) {
					bug("%02x ", ReadMacInt8(packet + i));
				}
				bug("\n");
#endif
				// Get packet type
				uint16 type = ReadMacInt16(packet + 12);

				// Look for protocol
				NetProtocol *prot = find_protocol(type);
				if (prot == NULL)
					goto next;

				// No default handler
				if (prot->handler == 0)
					goto next;

				// Copy header to RHA
				Mac2Mac_memcpy(ether_data + ed_RHA, packet, 14);
				D(bug(" header %08lx%04lx %08lx%04lx %04lx\n", ReadMacInt32(ether_data + ed_RHA), ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6), ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

				// Call protocol handler
				M68kRegisters r;
				r.d[0] = type;									// Packet type
				r.d[1] = p->length - 14;						// Remaining packet length (without header, for ReadPacket)
				r.a[0] = packet + 14;							// Pointer to packet (Mac address, for ReadPacket)
				r.a[3] = ether_data + ed_RHA + 14;				// Pointer behind header in RHA
				r.a[4] = ether_data + ed_ReadPacket;			// Pointer to ReadPacket/ReadRest routines
				D(bug(" calling protocol handler %08lx, type %08lx, length %08lx, data %08lx, rha %08lx, read_packet %08lx\n", prot->handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
				Execute68k(prot->handler, &r);
			}
next:		p->cmd = 0;	// Free packet
			rd_pos = (rd_pos + 1) % READ_PACKET_COUNT;
			p = &net_buffer_ptr->read[rd_pos];
		}
	}
	D(bug(" EtherIRQ done\n"));
}
