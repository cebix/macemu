/*
 *  NetPeek.cpp - Utility program for monitoring SheepNet add-on
 */

#include "sysdeps.h"
#include "sheep_net.h"

#include <stdio.h>

static area_id buffer_area;					// Packet buffer area
static net_buffer *net_buffer_ptr;			// Pointer to packet buffer

int main(void)
{
	area_id handler_buffer;
	if ((handler_buffer = find_area("packet buffer")) < B_NO_ERROR) {
		printf("Can't find packet buffer\n");
		return 10;
	}
	if ((buffer_area = clone_area("local packet buffer", &net_buffer_ptr, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA, handler_buffer)) < B_NO_ERROR) {
		printf("Can't clone packet buffer\n");
		return 10;
	}

	uint8 *p = net_buffer_ptr->ether_addr;
	printf("Ethernet address  : %02x %02x %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3], p[4], p[5]);
	printf("read_sem          : %d\n", net_buffer_ptr->read_sem);
	printf("read_ofs          : %d\n", net_buffer_ptr->read_ofs);
	printf("read_packet_size  : %d\n", net_buffer_ptr->read_packet_size);
	printf("read_packet_count : %d\n", net_buffer_ptr->read_packet_count);
	printf("write_sem         : %d\n", net_buffer_ptr->write_sem);
	printf("write_ofs         : %d\n", net_buffer_ptr->write_ofs);
	printf("write_packet_size : %d\n", net_buffer_ptr->write_packet_size);
	printf("write_packet_count: %d\n", net_buffer_ptr->write_packet_count);

	printf("\nRead packets:\n");
	for (int i=0; i<READ_PACKET_COUNT; i++) {
		net_packet *p = &net_buffer_ptr->read[i];
		printf("cmd   : %08lx\n", p->cmd);
		printf("length: %d\n", p->length);
	}
	printf("\nWrite packets:\n");
	for (int i=0; i<WRITE_PACKET_COUNT; i++) {
		net_packet *p = &net_buffer_ptr->write[i];
		printf("cmd   : %08lx\n", p->cmd);
		printf("length: %d\n", p->length);
	}
	return 0;
}
