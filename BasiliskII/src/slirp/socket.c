/*
 * Copyright (c) 1995 Danny Gasparovski.
 * 
 * Please read the file COPYRIGHT for the 
 * terms and conditions of the copyright.
 */

#define WANT_SYS_IOCTL_H
#include <stdlib.h>
#include <slirp.h>
#include "ip_icmp.h"
#include "main.h"
#ifdef __sun__
#include <sys/filio.h>
#endif
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#define DEBUG_HOST_RESOLVED_DNS 0

/**
 * DNS requests for these domain suffixes will be
 * looked up on the host to allow for host-supported DNS alternatives
 * (e.g. MDNS, hosts file, etc.)
 **/
static const char ** host_resolved_domain_suffixes = NULL;

#define HOST_DOMAIN_TTL 60 // In seconds.

#if DEBUG_HOST_RESOLVED_DNS
#define D(...) printf(__VA_ARGS__); fflush(stdout);
#else
#define D(...)
#endif

const char *PrefsFindStringC(const char *name, int index);

int prepare_host_domain_suffixes(char * buf) {
	/**
	 * Set up the list of domain suffixes to match from the host_domain prefs.
	 * Call first with buf NULL to figure out the size of buffer needed.
	 **/
	int pos = 0;
	const char ** host_resolved_domain_suffixes_pos = NULL;

	if (buf) {
		D("Setting up slirp host domain suffixes for matching:\n");
		host_resolved_domain_suffixes_pos = (const char **) buf;
	}

	// find out how many values there are
	int host_domain_count = 0;
	while (PrefsFindStringC("host_domain", host_domain_count) != NULL) {
		host_domain_count ++;
	}

	// leave space for the top array
	pos += (host_domain_count + 1) * sizeof(const char *);

	const char *str;
	int host_domain_num = 0;
	while ((str = PrefsFindStringC("host_domain", host_domain_num++)) != NULL) {
		if (str[0] == '\0') continue;
		if (buf) {
			const char * cur_entry = (const char *) (buf + pos);
			*host_resolved_domain_suffixes_pos = cur_entry;
			host_resolved_domain_suffixes_pos++;
		}
		
		// this is a suffix to match so it must have a leading dot
		if (str[0] != '.') {
			if (buf) buf[pos] = '.';
			pos++;
		}
		const char * str_pos = str;
		while (*str_pos != '\0') {
			if (buf) buf[pos] = tolower(*str_pos);
			++pos;
			++str_pos;
		}
		// domain to be checked will be FQDN so suffix must have a trailing dot
		if (str[strlen(str) - 1] != '.') {
			if (buf) buf[pos] = '.';
			pos++;
		}
		if (buf) {
			buf[pos] = '\0';
			D("   %d. %s\n", host_domain_num, *(host_resolved_domain_suffixes_pos-1));
		}
		pos++;
	}

	// end of list marker
	if (buf) *host_resolved_domain_suffixes_pos = NULL;

	return pos;
}

void load_host_domains() {
	const int size = prepare_host_domain_suffixes(NULL);
	char * buf = malloc(size);
	if (buf) {
		const int second_size = prepare_host_domain_suffixes(buf);
		assert(size == second_size);
		host_resolved_domain_suffixes = (const char **) buf;
	}
}

void unload_host_domains() {
	if (host_resolved_domain_suffixes) {
		free((char *) host_resolved_domain_suffixes);
		host_resolved_domain_suffixes = NULL;
	}
}

void
so_init()
{
	/* Nothing yet */
}


struct socket *
solookup(head, laddr, lport, faddr, fport)
	struct socket *head;
	struct in_addr laddr;
	u_int lport;
	struct in_addr faddr;
	u_int fport;
{
	struct socket *so;
	
	for (so = head->so_next; so != head; so = so->so_next) {
		if (so->so_lport == lport && 
		    so->so_laddr.s_addr == laddr.s_addr &&
		    so->so_faddr.s_addr == faddr.s_addr &&
		    so->so_fport == fport)
		   break;
	}
	
	if (so == head)
	   return (struct socket *)NULL;
	return so;
	
}

/*
 * Create a new socket, initialise the fields
 * It is the responsibility of the caller to
 * insque() it into the correct linked-list
 */
struct socket *
socreate()
{
  struct socket *so;
	
  so = (struct socket *)malloc(sizeof(struct socket));
  if(so) {
    memset(so, 0, sizeof(struct socket));
    so->so_state = SS_NOFDREF;
    so->s = -1;
  }
  return(so);
}

/*
 * remque and free a socket, clobber cache
 */
void
sofree(so)
	struct socket *so;
{
  if (so->so_emu==EMU_RSH && so->extra) {
	sofree(so->extra);
	so->extra=NULL;
  }
  if (so == tcp_last_so)
    tcp_last_so = &tcb;
  else if (so == udp_last_so)
    udp_last_so = &udb;
	
  m_free(so->so_m);
	
  if(so->so_next && so->so_prev) 
    remque(so);  /* crashes if so is not in a queue */

  free(so);
}

/*
 * Read from so's socket into sb_snd, updating all relevant sbuf fields
 * NOTE: This will only be called if it is select()ed for reading, so
 * a read() of 0 (or less) means it's disconnected
 */
int
soread(so)
	struct socket *so;
{
	int n, nn, lss, total;
	struct sbuf *sb = &so->so_snd;
	int len = sb->sb_datalen - sb->sb_cc;
	struct iovec iov[2];
	int mss = so->so_tcpcb->t_maxseg;
	
	DEBUG_CALL("soread");
	DEBUG_ARG("so = %lx", (long )so);
	
	/* 
	 * No need to check if there's enough room to read.
	 * soread wouldn't have been called if there weren't
	 */
	
	len = sb->sb_datalen - sb->sb_cc;
	
	iov[0].iov_base = sb->sb_wptr;
	if (sb->sb_wptr < sb->sb_rptr) {
		iov[0].iov_len = sb->sb_rptr - sb->sb_wptr;
		/* Should never succeed, but... */
		if (iov[0].iov_len > len)
		   iov[0].iov_len = len;
		if (iov[0].iov_len > mss)
		   iov[0].iov_len -= iov[0].iov_len%mss;
		n = 1;
	} else {
		iov[0].iov_len = (sb->sb_data + sb->sb_datalen) - sb->sb_wptr;
		/* Should never succeed, but... */
		if (iov[0].iov_len > len) iov[0].iov_len = len;
		len -= iov[0].iov_len;
		if (len) {
			iov[1].iov_base = sb->sb_data;
			iov[1].iov_len = sb->sb_rptr - sb->sb_data;
			if(iov[1].iov_len > len)
			   iov[1].iov_len = len;
			total = iov[0].iov_len + iov[1].iov_len;
			if (total > mss) {
				lss = total%mss;
				if (iov[1].iov_len > lss) {
					iov[1].iov_len -= lss;
					n = 2;
				} else {
					lss -= iov[1].iov_len;
					iov[0].iov_len -= lss;
					n = 1;
				}
			} else
				n = 2;
		} else {
			if (iov[0].iov_len > mss)
			   iov[0].iov_len -= iov[0].iov_len%mss;
			n = 1;
		}
	}
	
#ifdef HAVE_READV
	nn = readv(so->s, (struct iovec *)iov, n);
	DEBUG_MISC((dfd, " ... read nn = %d bytes\n", nn));
#else
	nn = recv(so->s, iov[0].iov_base, iov[0].iov_len,0);
#endif	
	if (nn <= 0) {
		if (nn < 0 && (errno == EINTR || errno == EAGAIN))
			return 0;
		else {
			DEBUG_MISC((dfd, " --- soread() disconnected, nn = %d, errno = %d-%s\n", nn, errno,strerror(errno)));
			sofcantrcvmore(so);
			tcp_sockclosed(sototcpcb(so));
			return -1;
		}
	}
	
#ifndef HAVE_READV
	/*
	 * If there was no error, try and read the second time round
	 * We read again if n = 2 (ie, there's another part of the buffer)
	 * and we read as much as we could in the first read
	 * We don't test for <= 0 this time, because there legitimately
	 * might not be any more data (since the socket is non-blocking),
	 * a close will be detected on next iteration.
	 * A return of -1 wont (shouldn't) happen, since it didn't happen above
	 */
	if (n == 2 && nn == iov[0].iov_len) {
            int ret;
            ret = recv(so->s, iov[1].iov_base, iov[1].iov_len,0);
            if (ret > 0)
                nn += ret;
        }
	
	DEBUG_MISC((dfd, " ... read nn = %d bytes\n", nn));
#endif
	
	/* Update fields */
	sb->sb_cc += nn;
	sb->sb_wptr += nn;
	if (sb->sb_wptr >= (sb->sb_data + sb->sb_datalen))
		sb->sb_wptr -= sb->sb_datalen;
	return nn;
}
	
/*
 * Get urgent data
 * 
 * When the socket is created, we set it SO_OOBINLINE,
 * so when OOB data arrives, we soread() it and everything
 * in the send buffer is sent as urgent data
 */
void
sorecvoob(so)
	struct socket *so;
{
	struct tcpcb *tp = sototcpcb(so);

	DEBUG_CALL("sorecvoob");
	DEBUG_ARG("so = %lx", (long)so);
	
	/*
	 * We take a guess at how much urgent data has arrived.
	 * In most situations, when urgent data arrives, the next
	 * read() should get all the urgent data.  This guess will
	 * be wrong however if more data arrives just after the
	 * urgent data, or the read() doesn't return all the 
	 * urgent data.
	 */
	soread(so);
	tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
	tp->t_force = 1;
	tcp_output(tp);
	tp->t_force = 0;
}

/*
 * Send urgent data
 * There's a lot duplicated code here, but...
 */
int
sosendoob(so)
	struct socket *so;
{
	struct sbuf *sb = &so->so_rcv;
	char buff[2048]; /* XXX Shouldn't be sending more oob data than this */
	
	int n, len;
	
	DEBUG_CALL("sosendoob");
	DEBUG_ARG("so = %lx", (long)so);
	DEBUG_ARG("sb->sb_cc = %d", sb->sb_cc);
	
	if (so->so_urgc > 2048)
	   so->so_urgc = 2048; /* XXXX */
	
	if (sb->sb_rptr < sb->sb_wptr) {
		/* We can send it directly */
		n = send(so->s, sb->sb_rptr, so->so_urgc, (MSG_OOB)); /* |MSG_DONTWAIT)); */
		so->so_urgc -= n;
		
		DEBUG_MISC((dfd, " --- sent %d bytes urgent data, %d urgent bytes left\n", n, so->so_urgc));
	} else {
		/* 
		 * Since there's no sendv or sendtov like writev,
		 * we must copy all data to a linear buffer then
		 * send it all
		 */
		len = (sb->sb_data + sb->sb_datalen) - sb->sb_rptr;
		if (len > so->so_urgc) len = so->so_urgc;
		memcpy(buff, sb->sb_rptr, len);
		so->so_urgc -= len;
		if (so->so_urgc) {
			n = sb->sb_wptr - sb->sb_data;
			if (n > so->so_urgc) n = so->so_urgc;
			memcpy((buff + len), sb->sb_data, n);
			so->so_urgc -= n;
			len += n;
		}
		n = send(so->s, buff, len, (MSG_OOB)); /* |MSG_DONTWAIT)); */
#ifdef DEBUG
		if (n != len)
		   DEBUG_ERROR((dfd, "Didn't send all data urgently XXXXX\n"));
#endif		
		DEBUG_MISC((dfd, " ---2 sent %d bytes urgent data, %d urgent bytes left\n", n, so->so_urgc));
	}
	
	sb->sb_cc -= n;
	sb->sb_rptr += n;
	if (sb->sb_rptr >= (sb->sb_data + sb->sb_datalen))
		sb->sb_rptr -= sb->sb_datalen;
	
	return n;
}

/*
 * Write data from so_rcv to so's socket, 
 * updating all sbuf field as necessary
 */
int
sowrite(so)
	struct socket *so;
{
	int  n,nn;
	struct sbuf *sb = &so->so_rcv;
	int len = sb->sb_cc;
	struct iovec iov[2];
	
	DEBUG_CALL("sowrite");
	DEBUG_ARG("so = %lx", (long)so);
	
	if (so->so_urgc) {
		sosendoob(so);
		if (sb->sb_cc == 0)
			return 0;
	}

	/*
	 * No need to check if there's something to write,
	 * sowrite wouldn't have been called otherwise
	 */
	
        len = sb->sb_cc;
	
	iov[0].iov_base = sb->sb_rptr;
	if (sb->sb_rptr < sb->sb_wptr) {
		iov[0].iov_len = sb->sb_wptr - sb->sb_rptr;
		/* Should never succeed, but... */
		if (iov[0].iov_len > len) iov[0].iov_len = len;
		n = 1;
	} else {
		iov[0].iov_len = (sb->sb_data + sb->sb_datalen) - sb->sb_rptr;
		if (iov[0].iov_len > len) iov[0].iov_len = len;
		len -= iov[0].iov_len;
		if (len) {
			iov[1].iov_base = sb->sb_data;
			iov[1].iov_len = sb->sb_wptr - sb->sb_data;
			if (iov[1].iov_len > len) iov[1].iov_len = len;
			n = 2;
		} else
			n = 1;
	}
	/* Check if there's urgent data to send, and if so, send it */

#ifdef HAVE_READV
	nn = writev(so->s, (const struct iovec *)iov, n);
	
	DEBUG_MISC((dfd, "  ... wrote nn = %d bytes\n", nn));
#else
	nn = send(so->s, iov[0].iov_base, iov[0].iov_len,0);
#endif
	/* This should never happen, but people tell me it does *shrug* */
	if (nn < 0 && (errno == EAGAIN || errno == EINTR))
		return 0;
	
	if (nn <= 0) {
		DEBUG_MISC((dfd, " --- sowrite disconnected, so->so_state = %x, errno = %d\n",
			so->so_state, errno));
		sofcantsendmore(so);
		tcp_sockclosed(sototcpcb(so));
		return -1;
	}
	
#ifndef HAVE_READV
	if (n == 2 && nn == iov[0].iov_len) {
            int ret;
            ret = send(so->s, iov[1].iov_base, iov[1].iov_len,0);
            if (ret > 0)
                nn += ret;
        }
        DEBUG_MISC((dfd, "  ... wrote nn = %d bytes\n", nn));
#endif
	
	/* Update sbuf */
	sb->sb_cc -= nn;
	sb->sb_rptr += nn;
	if (sb->sb_rptr >= (sb->sb_data + sb->sb_datalen))
		sb->sb_rptr -= sb->sb_datalen;
	
	/*
	 * If in DRAIN mode, and there's no more data, set
	 * it CANTSENDMORE
	 */
	if ((so->so_state & SS_FWDRAIN) && sb->sb_cc == 0)
		sofcantsendmore(so);
	
	return nn;
}

/*
 * recvfrom() a UDP socket
 */
void
sorecvfrom(so)
	struct socket *so;
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	
	DEBUG_CALL("sorecvfrom");
	DEBUG_ARG("so = %lx", (long)so);
	
	if (so->so_type == IPPROTO_ICMP) {   /* This is a "ping" reply */
	  char buff[256];
	  int len;
		
	  len = recvfrom(so->s, buff, 256, 0, 
			 (struct sockaddr *)&addr, &addrlen);
	  /* XXX Check if reply is "correct"? */
	  
	  if(len == -1 || len == 0) {
	    u_char code=ICMP_UNREACH_PORT;

	    if(errno == EHOSTUNREACH) code=ICMP_UNREACH_HOST;
	    else if(errno == ENETUNREACH) code=ICMP_UNREACH_NET;
	    
	    DEBUG_MISC((dfd," udp icmp rx errno = %d-%s\n",
			errno,strerror(errno)));
	    icmp_error(so->so_m, ICMP_UNREACH,code, 0,strerror(errno));
	  } else {
	    icmp_reflect(so->so_m);
	    so->so_m = 0; /* Don't m_free() it again! */
	  }
	  /* No need for this socket anymore, udp_detach it */
	  udp_detach(so);
	} else {                            	/* A "normal" UDP packet */
	  struct mbuf *m;
	  int len;
	  ioctlsockopt_t n;

	  if (!(m = m_get())) return;
	  m->m_data += if_maxlinkhdr;
		
	  /* 
	   * XXX Shouldn't FIONREAD packets destined for port 53,
	   * but I don't know the max packet size for DNS lookups
	   */
	  len = M_FREEROOM(m);
	  /* if (so->so_fport != htons(53)) { */
	  ioctlsocket(so->s, FIONREAD, &n);
	  
	  if (n > len) {
	    n = (m->m_data - m->m_dat) + m->m_len + n + 1;
	    m_inc(m, n);
	    len = M_FREEROOM(m);
	  }
	  /* } */
		
	  m->m_len = recvfrom(so->s, m->m_data, len, 0,
			      (struct sockaddr *)&addr, &addrlen);
	  DEBUG_MISC((dfd, " did recvfrom %d, errno = %d-%s\n", 
		      m->m_len, errno,strerror(errno)));
	  if(m->m_len<0) {
	    u_char code=ICMP_UNREACH_PORT;

	    if(errno == EHOSTUNREACH) code=ICMP_UNREACH_HOST;
	    else if(errno == ENETUNREACH) code=ICMP_UNREACH_NET;
	    
	    DEBUG_MISC((dfd," rx error, tx icmp ICMP_UNREACH:%i\n", code));
	    icmp_error(so->so_m, ICMP_UNREACH,code, 0,strerror(errno));
	    m_free(m);
	  } else {
	  /*
	   * Hack: domain name lookup will be used the most for UDP,
	   * and since they'll only be used once there's no need
	   * for the 4 minute (or whatever) timeout... So we time them
	   * out much quicker (10 seconds  for now...)
	   */
	    if (so->so_expire) {
	      if (so->so_fport == htons(53))
		so->so_expire = curtime + SO_EXPIREFAST;
	      else
		so->so_expire = curtime + SO_EXPIRE;
	    }

	    /*		if (m->m_len == len) {
	     *			m_inc(m, MINCSIZE);
	     *			m->m_len = 0;
	     *		}
	     */
	    
	    /* 
	     * If this packet was destined for CTL_ADDR,
	     * make it look like that's where it came from, done by udp_output
	     */
	    udp_output(so, m, &addr);
	  } /* rx error */
	} /* if ping packet */
}

// Commented structs from silv3rm00n's example code
// https://www.binarytides.com/dns-query-code-in-c-with-linux-sockets/

struct DNS_HEADER
{
    unsigned short id; // identification number
 
    unsigned char rd :1; // recursion desired
    unsigned char tc :1; // truncated message
    unsigned char aa :1; // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1; // query/response flag
 
    unsigned char rcode :4; // response code
    unsigned char cd :1; // checking disabled
    unsigned char ad :1; // authenticated data
    unsigned char z :1; // its z! reserved
    unsigned char ra :1; // recursion available
 
    unsigned short q_count; // number of question entries
    unsigned short ans_count; // number of answer entries
    unsigned short auth_count; // number of authority entries
    unsigned short add_count; // number of resource entries
};

struct QUESTION
{
    unsigned short qtype;
    unsigned short qclass;
};
 
#pragma pack(push, 1)
struct R_DATA
{
    unsigned short type;
    unsigned short _class;
    unsigned int ttl;
    unsigned short data_len;
};
#pragma pack(pop)

/** Create local variable varname of type vartype,
 * fill it from the buffer data, observing its length len,
 * and adjust data and len to reflect the remaining data */
#define POP_DATA(vartype, varname, data, len) \
	assert(len >= sizeof(vartype)); \
	vartype varname; \
	memcpy(&varname, data, sizeof(vartype)); \
	data += sizeof(vartype); \
	len -= sizeof(vartype)


/** Create local const char * varname pointing
 * to the C string in the buffer data, observing its length len,
 * and adjust data and len to reflect the remaining data */
#define POP_STR(varname, data, len) \
	const char * varname; \
	{ \
	int pop_str_len = strnlen(data, len); \
	if (pop_str_len == len) { \
		varname = NULL; \
	} else { \
		varname = data; \
	} \
	data += pop_str_len + 1; \
	len -= pop_str_len + 1; \
	}


static void inject_udp_packet_to_guest(struct socket * so, struct sockaddr_in addr, caddr_t packet_data, int packet_len) {
	struct mbuf *m;
	int len;
	
	/** This is like sorecvfrom(), but just adds a packet with the
	 * supplied data instead of reading the packet to add from the socket */

	if (!(m = m_get())) return;
	m->m_data += if_maxlinkhdr;
	
	len = M_FREEROOM(m);
	
	if (packet_len > len) {
		packet_len = (m->m_data - m->m_dat) + m->m_len + packet_len + 1;
		m_inc(m, packet_len);
		len = M_FREEROOM(m);
	}

	assert(len >= packet_len);
	m->m_len = packet_len;
	memcpy(m->m_data, packet_data, packet_len);

	udp_output(so, m, &addr);
}

/* Decode hostname from the format used in DNS
 e.g. "\009something\004else\003com" for "something.else.com." */
static char * decode_dns_name(const char * data) {

	int query_str_len = strlen(data);
	char * decoded_name_str = malloc(query_str_len + 1);
	if (decoded_name_str == NULL) { 
		D("decode_dns_name(): out of memory\n");
		return NULL; // oom
	}

	char * decoded_name_str_pos = decoded_name_str;
	while (*data != '\0') {
		int part_len = *data++;
		query_str_len--;
		if (query_str_len < part_len) {
			D("decode_dns_name(): part went off the end of the string\n");
			free(decoded_name_str);
			return NULL;
		}
		memcpy(decoded_name_str_pos, data, part_len);
		decoded_name_str_pos[part_len] = '.';
		decoded_name_str_pos += part_len + 1;
		query_str_len -= part_len;
		data += part_len;
	}
	*decoded_name_str_pos = '\0';
	return decoded_name_str;
}

/** Take a look at a UDP DNS request the client has made and see if we want to resolve it internally.
 * Returns true if the request has been internally and can be dropped,
 *         false otherwise
 **/
static bool resolve_dns_request(struct socket * so, struct sockaddr_in addr, caddr_t data, int len) {
	bool drop_dns_request = false;

	D("Checking outgoing DNS UDP packet\n");

	if (len < sizeof(struct DNS_HEADER)) {
		D("Packet too short for DNS header\n");
		return false;
	}

	const caddr_t packet = data;
	const int packet_len = len;

	POP_DATA(struct DNS_HEADER, h, data, len);

	if (h.qr != 0) {
		D("DNS packet is not a request\n");
		return false;
	}

	if (ntohs(h.q_count) == 0) {
		D("DNS request has no queries\n");
		return false;
	}

	if (ntohs(h.q_count) > 1) {
		D("DNS request has multiple queries (not supported)\n");
		return false;
	}
		
	if (ntohs(h.ans_count != 0) || ntohs(h.auth_count != 0) || ntohs(h.add_count != 0)) {
		D("DNS request has unsupported extra contents\n");
		return false;
	}

	if (len == 0) {
		D("Packet too short for DNS query string\n");
		return false;
	}

	POP_STR(original_query_str, data, len);
	if (original_query_str == NULL) {
		// went off end of packet
		D("Unterminated DNS query string\n");
		return false;
	}

	char * decoded_name_str = decode_dns_name(original_query_str);
	if (decoded_name_str == NULL) {
		D("Error while decoding DNS query string");
		return false;
	}

	D("DNS host query for %s\n", decoded_name_str);

	POP_DATA(struct QUESTION, qinfo, data, len);

	if (ntohs(qinfo.qtype) != 1 /* type A */ || ntohs(qinfo.qclass) != 1 /* class IN */ ) {
		D("DNS host query for %s: Request isn't the supported type (INET A query)\n", decoded_name_str);
		free(decoded_name_str);
		return false;
	}

	D("DNS host query for %s: Request is eligible to check for host resolution suffix\n", decoded_name_str);

	const char * matched_suffix = NULL;

	for (const char ** suffix_ptr = host_resolved_domain_suffixes; *suffix_ptr != NULL; suffix_ptr++) {
		const char * suffix = *suffix_ptr;

		// ends with suffix?
		int suffix_pos = strlen(decoded_name_str) - strlen(suffix);
		if (suffix_pos > 0 && strcmp(decoded_name_str + suffix_pos, suffix) == 0) {
			matched_suffix = suffix;
			break;
		}

		// also check if the domain exactly matched the one the suffix is for
		if (strcmp(decoded_name_str, suffix + 1) == 0) {
			matched_suffix = suffix;
			break;
		}
	}

	if (matched_suffix == NULL) {
		D("DNS host query for %s: No suffix matched\n", decoded_name_str);
	} else {

		D("DNS host query for %s: Matched for suffix: %s\n", decoded_name_str, matched_suffix);

		// we are going to take this request and resolve it on the host
		drop_dns_request = true;

		D("DNS host query for %s: Doing lookup on host\n", decoded_name_str);

		int results_count = 0;
		struct hostent * host_lookup_result = gethostbyname(decoded_name_str);

		if (host_lookup_result && host_lookup_result->h_addrtype == AF_INET) {

			D("DNS host query for %s: Host response has results for AF_INET\n", decoded_name_str);

			for (char ** addr_entry = host_lookup_result->h_addr_list; *addr_entry != NULL; addr_entry++) {
				++results_count;
			}
		}

		D("DNS host query for %s: result count %d\n", decoded_name_str, results_count);

		int original_query_str_size = strlen(original_query_str) + 1;
		int response_size = packet_len + results_count * (original_query_str_size + sizeof(struct R_DATA) + sizeof(struct in_addr));

		caddr_t response_packet = malloc(response_size);
		if (response_packet == NULL) {
			D("DNS host query for %s: Out of memory while allocating DNS response packet\n", decoded_name_str);
		} else {
			D("DNS host query for %s: Preparing DNS response\n", decoded_name_str);
			
			// use the request DNS header as our starting point for the response
			h.qr = 1;
			h.ans_count = htons(results_count);
			memcpy(response_packet, &h, sizeof(struct DNS_HEADER));

			// other sections verbatim out of the request
			memcpy(response_packet + sizeof(struct DNS_HEADER), packet + sizeof(struct DNS_HEADER), packet_len - sizeof(struct DNS_HEADER));

			int response_pos = packet_len;

			if (results_count > 0) {
				for (char ** addr_entry = host_lookup_result->h_addr_list; *addr_entry != NULL; addr_entry++) {
					// answer string is verbatim from question
					memcpy(response_packet + response_pos, original_query_str, original_query_str_size);

					response_pos += original_query_str_size;

					struct R_DATA resource;
					resource.type = htons(1);
					resource._class = htons(1);
					resource.ttl = htonl(HOST_DOMAIN_TTL);
					resource.data_len = htons(sizeof(struct in_addr));

					memcpy(response_packet + response_pos, &resource, sizeof(struct R_DATA));
					response_pos += sizeof(struct R_DATA);

					struct in_addr * cur_addr = (struct in_addr *)*addr_entry;
					memcpy(response_packet + response_pos, cur_addr, sizeof(struct in_addr));
					response_pos += sizeof(struct in_addr);
				}
			}

			assert(response_pos == response_size);

			D("DNS host query for %s: Injecting DNS response directly to guest\n", decoded_name_str);
			inject_udp_packet_to_guest(so, addr, response_packet, response_size);

			free(response_packet);
		}

	}

	free(decoded_name_str);
	
	D("DNS host request drop: %s\n", drop_dns_request? "yes" : "no");
	return drop_dns_request;
}

/*
 * sendto() a socket
 */
int
sosendto(so, m)
	struct socket *so;
	struct mbuf *m;
{
	int ret;
	struct sockaddr_in addr;

	DEBUG_CALL("sosendto");
	DEBUG_ARG("so = %lx", (long)so);
	DEBUG_ARG("m = %lx", (long)m);
	
        addr.sin_family = AF_INET;
	if ((so->so_faddr.s_addr & htonl(0xffffff00)) == special_addr.s_addr) {
	  /* It's an alias */
	  switch(ntohl(so->so_faddr.s_addr) & 0xff) {
	  case CTL_DNS:
	    addr.sin_addr = dns_addr;
		if (host_resolved_domain_suffixes != NULL) {
			if (resolve_dns_request(so, addr, m->m_data, m->m_len))
			return 0;
		}
	    break;
	  case CTL_ALIAS:
	  default:
	    addr.sin_addr = loopback_addr;
	    break;
	  }
	} else
	  addr.sin_addr = so->so_faddr;
	addr.sin_port = so->so_fport;

	DEBUG_MISC((dfd, " sendto()ing, addr.sin_port=%d, addr.sin_addr.s_addr=%.16s\n", ntohs(addr.sin_port), inet_ntoa(addr.sin_addr)));
	
	/* Don't care what port we get */
	ret = sendto(so->s, m->m_data, m->m_len, 0,
		     (struct sockaddr *)&addr, sizeof (struct sockaddr));
	if (ret < 0)
		return -1;
	
	/*
	 * Kill the socket if there's no reply in 4 minutes,
	 * but only if it's an expirable socket
	 */
	if (so->so_expire)
		so->so_expire = curtime + SO_EXPIRE;
	so->so_state = SS_ISFCONNECTED; /* So that it gets select()ed */
	return 0;
}

/*
 * XXX This should really be tcp_listen
 */
struct socket *
solisten(port, laddr, lport, flags)
	u_int port;
	u_int32_t laddr;
	u_int lport;
	int flags;
{
	struct sockaddr_in addr;
	struct socket *so;
	int s;
	socklen_t addrlen = sizeof(addr);
	int opt = 1;

	DEBUG_CALL("solisten");
	DEBUG_ARG("port = %d", port);
	DEBUG_ARG("laddr = %x", laddr);
	DEBUG_ARG("lport = %d", lport);
	DEBUG_ARG("flags = %x", flags);
	
	if ((so = socreate()) == NULL) {
	  /* free(so);      Not sofree() ??? free(NULL) == NOP */
	  return NULL;
	}
	
	/* Don't tcp_attach... we don't need so_snd nor so_rcv */
	if ((so->so_tcpcb = tcp_newtcpcb(so)) == NULL) {
		free(so);
		return NULL;
	}
	insque(so,&tcb);
	
	/* 
	 * SS_FACCEPTONCE sockets must time out.
	 */
	if (flags & SS_FACCEPTONCE)
	   so->so_tcpcb->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT*2;
	
	so->so_state = (SS_FACCEPTCONN|flags);
	so->so_lport = lport; /* Kept in network format */
	so->so_laddr.s_addr = laddr; /* Ditto */
	
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = port;
	
	if (((s = socket(AF_INET,SOCK_STREAM,0)) < 0) ||
	    (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(int)) < 0) ||
	    (bind(s,(struct sockaddr *)&addr, sizeof(addr)) < 0) ||
	    (listen(s,1) < 0)) {
		int tmperrno = errno; /* Don't clobber the real reason we failed */
		
		close(s);
		sofree(so);
		/* Restore the real errno */
#ifdef _WIN32
		WSASetLastError(tmperrno);
#else
		errno = tmperrno;
#endif
		return NULL;
	}
	setsockopt(s,SOL_SOCKET,SO_OOBINLINE,(char *)&opt,sizeof(int));
	
	getsockname(s,(struct sockaddr *)&addr,&addrlen);
	so->so_fport = addr.sin_port;
	if (addr.sin_addr.s_addr == 0 || addr.sin_addr.s_addr == loopback_addr.s_addr)
	   so->so_faddr = alias_addr;
	else
	   so->so_faddr = addr.sin_addr;

	so->s = s;
	return so;
}

/* 
 * Data is available in so_rcv
 * Just write() the data to the socket
 * XXX not yet...
 */
void
sorwakeup(so)
	struct socket *so;
{
/*	sowrite(so); */
/*	FD_CLR(so->s,&writefds); */
}
	
/*
 * Data has been freed in so_snd
 * We have room for a read() if we want to
 * For now, don't read, it'll be done in the main loop
 */
void
sowwakeup(so)
	struct socket *so;
{
	/* Nothing, yet */
}

/*
 * Various session state calls
 * XXX Should be #define's
 * The socket state stuff needs work, these often get call 2 or 3
 * times each when only 1 was needed
 */
void
soisfconnecting(so)
	register struct socket *so;
{
	so->so_state &= ~(SS_NOFDREF|SS_ISFCONNECTED|SS_FCANTRCVMORE|
			  SS_FCANTSENDMORE|SS_FWDRAIN);
	so->so_state |= SS_ISFCONNECTING; /* Clobber other states */
}

void
soisfconnected(so)
        register struct socket *so;
{
	so->so_state &= ~(SS_ISFCONNECTING|SS_FWDRAIN|SS_NOFDREF);
	so->so_state |= SS_ISFCONNECTED; /* Clobber other states */
}

void
sofcantrcvmore(so)
	struct  socket *so;
{
	if ((so->so_state & SS_NOFDREF) == 0) {
		shutdown(so->s,0);
		if(global_writefds) {
		  FD_CLR(so->s,global_writefds);
		}
	}
	so->so_state &= ~(SS_ISFCONNECTING);
	if (so->so_state & SS_FCANTSENDMORE)
	   so->so_state = SS_NOFDREF; /* Don't select it */ /* XXX close() here as well? */
	else
	   so->so_state |= SS_FCANTRCVMORE;
}

void
sofcantsendmore(so)
	struct socket *so;
{
	if ((so->so_state & SS_NOFDREF) == 0) {
            shutdown(so->s,1);           /* send FIN to fhost */
            if (global_readfds) {
                FD_CLR(so->s,global_readfds);
            }
            if (global_xfds) {
                FD_CLR(so->s,global_xfds);
            }
	}
	so->so_state &= ~(SS_ISFCONNECTING);
	if (so->so_state & SS_FCANTRCVMORE)
	   so->so_state = SS_NOFDREF; /* as above */
	else
	   so->so_state |= SS_FCANTSENDMORE;
}

void
soisfdisconnected(so)
	struct socket *so;
{
/*	so->so_state &= ~(SS_ISFCONNECTING|SS_ISFCONNECTED); */
/*	close(so->s); */
/*	so->so_state = SS_ISFDISCONNECTED; */
	/*
	 * XXX Do nothing ... ?
	 */
}

/*
 * Set write drain mode
 * Set CANTSENDMORE once all data has been write()n
 */
void
sofwdrain(so)
	struct socket *so;
{
	if (so->so_rcv.sb_cc)
		so->so_state |= SS_FWDRAIN;
	else
		sofcantsendmore(so);
}

