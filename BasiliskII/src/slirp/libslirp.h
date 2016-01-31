#ifndef _LIBSLIRP_H
#define _LIBSLIRP_H

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <arpa/inet.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *ia);
#endif

#if defined _WIN32_WINNT && _WIN32_WINNT < _WIN32_WINNT_VISTA
#ifdef __cplusplus
#define restrict
#endif
const char* inet_ntop(int af, const void* restrict src, char* restrict dst, socklen_t size);
#endif

int slirp_init(void);

int slirp_select_fill(int *pnfds, 
					  fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_select_poll(fd_set *readfds, fd_set *writefds, fd_set *xfds);

void slirp_input(const uint8 *pkt, int pkt_len);

/* you must provide the following functions: */
int slirp_can_output(void);
void slirp_output(const uint8 *pkt, int pkt_len);

int slirp_redir(int is_udp, int host_port, 
                struct in_addr guest_addr, int guest_port);
int slirp_add_exec(int do_pty, const char *args, int addr_low_byte, 
                   int guest_port);

extern const char *tftp_prefix;
extern char slirp_hostname[33];

#ifdef __cplusplus
}
#endif

#endif
