/*
 * Copyright (c) 1995 Danny Gasparovski.
 * 
 * Please read the file COPYRIGHT for the 
 * terms and conditions of the copyright.
 */

#ifndef _SBUF_H_
#define _SBUF_H_

#include <stddef.h>

#define sbflush(sb) sbdrop((sb),(sb)->sb_cc)
#define sbspace(sb) ((sb)->sb_datalen - (sb)->sb_cc)

struct sbuf {
	u_int	sb_cc;		/* actual chars in buffer */
	u_int	sb_datalen;	/* Length of data  */
	char	*sb_wptr;	/* write pointer. points to where the next
				 * bytes should be written in the sbuf */
	char	*sb_rptr;	/* read pointer. points to where the next
				 * byte should be read from the sbuf */
	char	*sb_data;	/* Actual data */
};

void sbfree(struct sbuf *);
void sbdrop(struct sbuf *, u_int);
void sbreserve(struct sbuf *, size_t);
void sbappend(struct socket *, struct mbuf *);
void sbappendsb(struct sbuf *, struct mbuf *);
void sbcopy(struct sbuf *, u_int, u_int, char *);

#endif
