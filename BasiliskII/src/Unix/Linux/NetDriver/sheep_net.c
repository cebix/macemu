/*
 *  sheep_net.c - Linux driver for SheepShaver/Basilisk II networking (access to raw Ethernet packets)
 *
 *  sheep_net (C) 1999-2004 Mar"c" Hellwig and Christian Bauer
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
/* modversions.h redefines kernel symbols.  Now include other headers */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
#include <linux/sched/signal.h>
#endif

/* wrap up socket object allocation */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 00)
	#define compat_sk_alloc(_net, _family, _priority, _proto) \
		sk_alloc(_net, _family, _priority, _proto, 0)
#else
	#define compat_sk_alloc(_net, _family, _priority, _proto) \
		sk_alloc(_net, _family, _priority, _proto)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
#define LINUX_3_15
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
#define LINUX_26_35
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
#define LINUX_26_36
#endif

/* Compatibility glue */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#define LINUX_26_30

#else

/* determine whether to use checksummed versions of kernel symbols */
/***
#include <linux/config.h>
***/
#include "config.h"
#include <linux/autoconf.h>

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#endif

#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <net/sock.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/raw.h>
#include <linux/in.h>
#include <linux/wait.h>

MODULE_AUTHOR("Christian Bauer");
MODULE_DESCRIPTION("Pseudo ethernet device for emulators");

/* Compatibility glue */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define LINUX_26
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#define LINUX_24
#else
#define net_device device
typedef struct wait_queue *wait_queue_head_t;
#define init_waitqueue_head(x) *(x)=NULL
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define eth_hdr(skb) (skb)->mac.ethernet
#define skb_mac_header(skb) (skb)->mac.raw
#define ipip_hdr(skb) (skb)->h.ipiph
#endif

#ifdef LINUX_26
#define skt_set_dead(skt)		do {} while(0)
#define wmem_alloc				sk_wmem_alloc
#else
#define skt_set_dead(skt)		(skt)->dead = 1
#endif

#define DEBUG 0

#define bug printk
#if DEBUG
#define D(x) (x);
#else
#define D(x) ;
#endif


/* Constants */
#define SHEEP_NET_MINOR 198		/* Driver minor number */
#define MAX_QUEUE 32			/* Maximum number of packets in queue */
#define PROT_MAGIC 1520			/* Our "magic" protocol type */

#define ETH_ADDR_MULTICAST 0x1
#define ETH_ADDR_LOCALLY_DEFINED 0x02

#define SIOC_MOL_GET_IPFILTER SIOCDEVPRIVATE
#define SIOC_MOL_SET_IPFILTER (SIOCDEVPRIVATE + 1)

/* Prototypes */
static int sheep_net_open(struct inode *inode, struct file *f);
static int sheep_net_release(struct inode *inode, struct file *f);
static ssize_t sheep_net_read(struct file *f, char *buf, size_t count, loff_t *off);
static ssize_t sheep_net_write(struct file *f, const char *buf, size_t count, loff_t *off);
static unsigned int sheep_net_poll(struct file *f, struct poll_table_struct *wait);
#ifdef LINUX_26_36
static long sheep_net_ioctl(struct file *f, unsigned int code, unsigned long arg);
#else
static int sheep_net_ioctl(struct inode *inode, struct file *f, unsigned int code, unsigned long arg);
#endif
#ifdef LINUX_26
static int sheep_net_receiver(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *foo);
#else
static int sheep_net_receiver(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt);
#endif

/*
 *  Driver private variables
 */

struct SheepVars {
	/* IMPORTANT: the packet_type struct must go first. It no longer
	   (2.6) contains * a data field so we typecast to get the SheepVars
	   struct */
	struct packet_type pt;		/* Receiver packet type */
	struct net_device *ether;	/* The Ethernet device we're attached to */
	struct sock *skt;			/* Socket for communication with Ethernet card */
	struct sk_buff_head queue;	/* Receiver packet queue */
	wait_queue_head_t wait;		/* Wait queue for blocking read operations */
	u32 ipfilter;				/* Only receive IP packets destined for this address (host byte order) */
	char eth_addr[6];			/* Hardware address of the Ethernet card */
	char fake_addr[6];			/* Local faked hardware address (what SheepShaver sees) */
#ifdef LINUX_3_15
	atomic_t got_packet;
#endif
};


/*
 *  file_operations structure - has function pointers to the
 *  various entry points for device operations
 */

static struct file_operations sheep_net_fops = {
	.owner		= THIS_MODULE,
	.read		= sheep_net_read,
	.write		= sheep_net_write,
	.poll		= sheep_net_poll,
#ifdef LINUX_26_36
	.unlocked_ioctl = sheep_net_ioctl,
#else
	.ioctl		= sheep_net_ioctl,
#endif
	.open		= sheep_net_open,
	.release	= sheep_net_release,
};


/*
 *  miscdevice structure for driver initialization
 */

static struct miscdevice sheep_net_device = {
	.minor		= SHEEP_NET_MINOR,	/* minor number */
	.name		= "sheep_net",		/* name */
	.fops		= &sheep_net_fops
};


/*
 * fake protocol to use a common socket
 */
static struct proto sheep_proto = {
	.name	  = "SHEEP",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct sock),
};


/*
 *  Initialize module
 */

int init_module(void)
{
	int ret;

	/* Register driver */
	ret = misc_register(&sheep_net_device);
	printk("sheep net: driver installed\n");
	return ret;
}


/*
 *  Deinitialize module
 */

void cleanup_module(void)
{
	/* Unregister driver */
	misc_deregister(&sheep_net_device);
	printk("sheep net: driver removed\n");
}


/*
 *  Driver open() function
 */

static int sheep_net_open(struct inode *inode, struct file *f)
{
	struct SheepVars *v;
	D(bug("sheep_net: open\n"));

	/* Must be opened with read permissions */
	if ((f->f_flags & O_ACCMODE) == O_WRONLY)
		return -EPERM;

	/* Allocate private variables */
	v = (struct SheepVars *)kmalloc(sizeof(struct SheepVars), GFP_USER);
	if (v == NULL)
		return -ENOMEM;

#ifndef LINUX_26_30
	lock_kernel();
#endif
	memset(v, 0, sizeof(struct SheepVars));
	skb_queue_head_init(&v->queue);
	init_waitqueue_head(&v->wait);
	v->fake_addr[0] = 'v'; /* "SheepShaver" */
	v->fake_addr[1] = 'r'; /*          ^ ^  */
	get_random_bytes(&v->fake_addr[2], 4);

	/* Put our stuff where we will be able to find it later */
	f->private_data = (void *)v;

	/* Yes, we're open */
#ifndef LINUX_26
	MOD_INC_USE_COUNT;
#endif
#ifndef LINUX_26_30
	unlock_kernel();
#endif
	return 0;
}


/*
 *  Driver release() function
 */

static int sheep_net_release(struct inode *inode, struct file *f)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	struct sk_buff *skb;
	D(bug("sheep_net: close\n"));

	/* Detach from Ethernet card */
	if (v->ether) {
		dev_remove_pack(&v->pt);
		sk_free(v->skt);
		v->skt = NULL;
#ifdef LINUX_24
		dev_put( v->ether );
#endif
		v->ether = NULL;
	}

	/* Empty packet queue */
	while ((skb = skb_dequeue(&v->queue)) != NULL)
		dev_kfree_skb(skb);

	/* Free private variables */
	kfree(v);

	/* Sorry, we're closed */
#ifndef LINUX_26
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


/*
 *  Check whether an Ethernet address is the local (attached) one or
 *  the fake one
 */

static inline int is_local_addr(struct SheepVars *v, void *a)
{
	return memcmp(a, v->eth_addr, 6) == 0;
}

static inline int is_fake_addr(struct SheepVars *v, void *a)
{
	return memcmp(a, v->fake_addr, 6) == 0;
}


/* 
 * Outgoing packet. Replace the fake enet addr with the real local one.
 */

static inline void do_demasq(struct SheepVars *v, u8 *p)
{
	memcpy(p, v->eth_addr, 6);
}

static void demasquerade(struct SheepVars *v, struct sk_buff *skb)
{
	u8 *p = skb_mac_header(skb);
	int proto = (p[12] << 8) | p[13];
	
	do_demasq(v, p + 6); /* source address */

	/* Need to fix ARP packets */
	if (proto == ETH_P_ARP) {
		if (is_fake_addr(v, p + 14 + 8)) /* sender HW-addr */
			do_demasq(v, p + 14 + 8);
	}

	/* ...and AARPs (snap code: 0x00,0x00,0x00,0x80,0xF3) */
	if (p[17] == 0 && p[18] == 0 && p[19] == 0 && p[20] == 0x80 && p[21] == 0xf3) {
		/* XXX: we should perhaps look for the 802 frame too */
		if (is_fake_addr(v, p + 30))
			do_demasq(v, p + 30); /* sender HW-addr */
	}
}


/*
 * Incoming packet. Replace the local enet addr with the fake one.
 */

static inline void do_masq(struct SheepVars *v, u8 *p)
{
	memcpy(p, v->fake_addr, 6);
}

static void masquerade(struct SheepVars *v, struct sk_buff *skb)
{
	u8 *p = skb_mac_header(skb);
	if (!(p[0] & ETH_ADDR_MULTICAST))
		do_masq(v, p); /* destination address */

	/* XXX: reverse ARP might need to be fixed */
}


/*
 *  Driver read() function
 */

static ssize_t sheep_net_read(struct file *f, char *buf, size_t count, loff_t *off)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	struct sk_buff *skb;

	D(bug("sheep_net: read\n"));

	for (;;) {

		/* Get next packet from queue */
		skb = skb_dequeue(&v->queue);
		if (skb != NULL || (f->f_flags & O_NONBLOCK))
			break;

		/* No packet in queue and in blocking mode, so block */
#ifdef LINUX_3_15
		atomic_set(&v->got_packet, 0);
		wait_event_interruptible(v->wait, atomic_read(&v->got_packet));
#else
		interruptible_sleep_on(&v->wait);
#endif

		/* Signal received? Then bail out */
		if (signal_pending(current))
			return -EINTR;
	}
	if (skb == NULL)
		return -EAGAIN;

	/* Pass packet to caller */
	if (count > skb->len)
		count = skb->len;
	if (copy_to_user(buf, skb->data, count))
		count = -EFAULT;
	dev_kfree_skb(skb);
	return count;
}


/*
 *  Driver write() function
 */

static inline void do_nothing(struct sk_buff *skb)
{
	return;
}

static ssize_t sheep_net_write(struct file *f, const char *buf, size_t count, loff_t *off)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	struct sk_buff *skb;
	char *p;
	D(bug("sheep_net: write\n"));

	/* Check packet size */
	if (count < sizeof(struct ethhdr))
		return -EINVAL;
	if (count > 1514) {
		printk("sheep_net_write: packet size %ld > 1514\n", count);
		count = 1514;
	}

	/* Interface active? */
	if (v->ether == NULL)
		return count;

	/* Allocate buffer for packet */
	skb = dev_alloc_skb(count);
	if (skb == NULL)
		return -ENOBUFS;

	/* Stuff packet in buffer */
	p = skb_put(skb, count);
	if (copy_from_user(p, buf, count)) {
		kfree_skb(skb);
		return -EFAULT;
	}

	/* Transmit packet */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
	atomic_add(skb->truesize, &v->skt->wmem_alloc);
#endif
	skb->sk = v->skt;
	skb->dev = v->ether;
	skb->priority = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
	skb->nh.raw = skb->h.raw = skb->data + v->ether->hard_header_len;
	skb->mac.raw = skb->data;
#else
	skb_reset_mac_header(skb);
	skb_set_transport_header(skb, v->ether->hard_header_len);
	skb_set_network_header(skb, v->ether->hard_header_len);
#endif

	/* Base the IP-filtering on the IP address in any outgoing ARP packets */
	if (eth_hdr(skb)->h_proto == htons(ETH_P_ARP)) {
		u8 *p = &skb->data[14+14];	/* source IP address */
		u32 ip = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
		if (ip != v->ipfilter) {
			v->ipfilter = ip;
			printk("sheep_net: ipfilter set to %d.%d.%d.%d\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
		}
	}

	/* Is this packet addressed solely to the local host? */
	if (is_local_addr(v, skb->data) && !(skb->data[0] & ETH_ADDR_MULTICAST)) {
		skb->destructor = do_nothing;
		skb->protocol = eth_type_trans(skb, v->ether);
		netif_rx(skb);
		return count;
	}
	/* Relay multicast for host process except ARP packet */
	if ((skb->data[0] & ETH_ADDR_MULTICAST) && (eth_hdr(skb)->h_proto != htons(ETH_P_ARP))) {
		/* We can't clone the skb since we will manipulate the data below */
		struct sk_buff *lskb = skb_copy(skb, GFP_ATOMIC);
		if (lskb) {
			lskb->protocol = eth_type_trans(lskb, v->ether);
			netif_rx(lskb);
		}
	}

	/* Outgoing packet (will be on the net) */
	demasquerade(v, skb);

	skb->destructor = do_nothing;
	skb->protocol = PROT_MAGIC;	/* Magic value (we can recognize the packet in sheep_net_receiver) */
	dev_queue_xmit(skb);
	return count;
}


/*
 *  Driver poll() function
 */

static unsigned int sheep_net_poll(struct file *f, struct poll_table_struct *wait)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	D(bug("sheep_net: poll\n"));

	/* Packets in queue? Then return */
	if (!skb_queue_empty(&v->queue))
		return POLLIN | POLLRDNORM;

	/* Otherwise wait for packet */
	poll_wait(f, &v->wait, wait);
	if (!skb_queue_empty(&v->queue))
		return POLLIN | POLLRDNORM;
	else
		return 0;
}


/*
 *  Driver ioctl() function
 */

#ifdef LINUX_26_36
static long sheep_net_ioctl(struct file *f, unsigned int code, unsigned long arg)
#else
static int sheep_net_ioctl(struct inode *inode, struct file *f, unsigned int code, unsigned long arg)
#endif
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	D(bug("sheep_net: ioctl %04x\n", code));

	switch (code) {

		/* Attach to Ethernet card
		   arg: pointer to name of Ethernet device (char[20]) */
		case SIOCSIFLINK: {
			char name[20];
			int err;

			/* Already attached? */
			if (v->ether)
				return -EBUSY;

			/* Get Ethernet card name */
			if (copy_from_user(name, (void *)arg, 20))
				return -EFAULT;
			name[19] = 0;

			/* Find card */
#ifdef LINUX_26
			v->ether = dev_get_by_name(&init_net, name);
#elif defined(LINUX_24)
			v->ether = dev_get_by_name(name);
#else
			dev_lock_list();
			v->ether = dev_get(name);
#endif
			if (v->ether == NULL) {
				err = -ENODEV;
				goto error;
			}

			/* Is it Ethernet? */
			if (v->ether->type != ARPHRD_ETHER) {
				err = -EINVAL;
				goto error;
			}

			/* Remember the card's hardware address */
			memcpy(v->eth_addr, v->ether->dev_addr, 6);

			/* Allocate socket */
#ifdef LINUX_26
			v->skt = compat_sk_alloc(dev_net(v->ether), GFP_USER, 1, &sheep_proto);
#else
			v->skt = sk_alloc(0, GFP_USER, 1);
#endif
			if (v->skt == NULL) {
				err = -ENOMEM;
				goto error;
			}
			skt_set_dead(v->skt);

			/*initialize ipfilter*/
			v->ipfilter = 0;

			/* Attach packet handler */
			v->pt.type = htons(ETH_P_ALL);
			v->pt.dev = v->ether;
			v->pt.func = sheep_net_receiver;
#ifdef LINUX_26
			v->pt.af_packet_priv = v;
#endif
			dev_add_pack(&v->pt);
#ifndef LINUX_24
			dev_unlock_list();
#endif
			return 0;

error:
#ifdef LINUX_24
			if (v->ether)
				dev_put(v->ether);
#else
			dev_unlock_list();
#endif
			v->ether = NULL;
			return err;
		}

		/* Get hardware address of the sheep_net module
		   arg: pointer to buffer (6 bytes) to store address */
		case SIOCGIFADDR:
			if (copy_to_user((void *)arg, v->fake_addr, 6))
				return -EFAULT;
			return 0;

		/* Set the hardware address of the sheep_net module
		   arg: pointer to new address (6 bytes) */
		case SIOCSIFADDR:
			if (copy_from_user(v->fake_addr, (void*)arg, 6))
				return -EFAULT;
			return 0;

		/* Add multicast address
		   arg: pointer to address (6 bytes) */
		case SIOCADDMULTI: {
			char addr[6];
			if (v->ether == NULL)
				return -ENODEV;
			if (copy_from_user(addr, (void *)arg, 6))
				return -EFAULT;
			#ifdef LINUX_26_35
			return dev_mc_add(v->ether, addr);
			#else
			return dev_mc_add(v->ether, addr, 6, 0);
			#endif
		}

		/* Remove multicast address
		   arg: pointer to address (6 bytes) */
		case SIOCDELMULTI: {
			char addr[6];
			if (v->ether == NULL)
				return -ENODEV;
			if (copy_from_user(addr, (void *)arg, 6))
				return -EFAULT;
			#ifdef LINUX_26_35
			return dev_mc_del(v->ether, addr);
			#else
			return dev_mc_delete(v->ether, addr, 6, 0);
			#endif
		}

		/* Return size of first packet in queue */
		case FIONREAD: {
			int count = 0;
			struct sk_buff *skb;
#ifdef LINUX_24
			unsigned long flags;
			spin_lock_irqsave(&v->queue.lock, flags);
#else
			cli();
#endif
			skb = skb_peek(&v->queue);
			if (skb)
				count = skb->len;
#ifdef LINUX_24
			spin_unlock_irqrestore(&v->queue.lock, flags);
#else
			sti();
#endif
			return put_user(count, (int *)arg);
		}

		case SIOC_MOL_GET_IPFILTER:
			return put_user(v->ipfilter, (int *)arg);

		case SIOC_MOL_SET_IPFILTER:
			v->ipfilter = arg;
			return 0;

		default:
			return -ENOIOCTLCMD;
	}
}


/*
 *  Packet receiver function
 */

#ifdef LINUX_26
static int sheep_net_receiver(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *foo)
#else
static int sheep_net_receiver(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
#endif
{
#ifdef LINUX_26
	struct SheepVars *v = (struct SheepVars *)pt->af_packet_priv;
#else
	struct SheepVars *v = (struct SheepVars *)pt;
#endif
	struct sk_buff *skb2;
	int fake;
	int multicast;
	D(bug("sheep_net: packet received\n"));

	multicast = (eth_hdr(skb)->h_dest[0] & ETH_ADDR_MULTICAST);
	fake = is_fake_addr(v, &eth_hdr(skb)->h_dest);

	/* Packet sent by us? Then discard */
	if (is_fake_addr(v, &eth_hdr(skb)->h_source) || skb->protocol == PROT_MAGIC)
		goto drop;

	/* If the packet is not meant for this host, discard it */
	if (!is_local_addr(v, &eth_hdr(skb)->h_dest) && !multicast && !fake)
		goto drop;

	/* Discard packets if queue gets too full */
	if (skb_queue_len(&v->queue) > MAX_QUEUE)
		goto drop;

	/* Apply any filters here (if fake is true, then we *know* we want this packet) */
	if (!fake) {
		if ((skb->protocol == htons(ETH_P_IP))
		 && (!v->ipfilter || (ntohl(ip_hdr(skb)->daddr) != v->ipfilter && !multicast))) {
#if DEBUG
			char source[16];
			snprintf(source, 16, "%pI4", &ip_hdr(skb)->saddr);
			D(bug("sheep_net: drop incoming IP packet %s for filter %d.%d.%d.%d\n",
					 source,
					 (v->ipfilter >> 24) & 0xff, (v->ipfilter >> 16) & 0xff, (v->ipfilter >> 8) & 0xff, v->ipfilter & 0xff));
#endif
			goto drop;
		}
#if DEBUG
		else{
			if(!multicast){
				char source[16];
				snprintf(source, 16, "%pI4", &ip_hdr(skb)->saddr);
				D(bug("sheep_net: retain incoming unicast IP packet %s\n", source));
			}
		}
#endif
	}

	/* Masquerade (we are typically a clone - best to make a real copy) */
	skb2 = skb_copy(skb, GFP_ATOMIC);
	if (!skb2)
		goto drop;
	kfree_skb(skb);
	skb = skb2;
	masquerade(v, skb);

	/* We also want the Ethernet header */
	skb_push(skb, skb->data - skb_mac_header(skb));

	/* Enqueue packet */
	skb_queue_tail(&v->queue, skb);

	/* Unblock blocked read */
#ifdef LINUX_3_15

	atomic_set(&v->got_packet, 1);

	wake_up_interruptible(&v->wait);

#else

	wake_up(&v->wait);

#endif
	return 0;

drop:
	kfree_skb(skb);
	return 0;
}

MODULE_LICENSE("GPL");
