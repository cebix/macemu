#ifndef _ETHER_WINDOWS_H_
#define _ETHER_WINDOWS_H_

void enqueue_packet( const uint8 *buf, int sz );

#ifdef SHEEPSHAVER
extern uint8 ether_addr[6];
#endif

#endif // _ETHER_WINDOWS_H_
