/*
 *  sheep_net.c - Linux driver for SheepShaver/Basilisk II networking (access to raw Ethernet packets)
 *
 *  sheep_net (C) 1999 Mar"c" Hellwig and Christian Bauer
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
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

#define DEBUG 0

#define bug printk
#if DEBUG
#define D(x) (x);
#else
#define D(x) ;
#endif


// Constants
#define SHEEP_NET_MINOR 198		// Driver minor number
#define MAX_QUEUE 32			// Maximum number of packets in queue
#define PROT_MAGIC 1520			// Our "magic" protocol type

// Prototypes
static int sheep_net_open(struct inode *inode, struct file *f);
static int sheep_net_release(struct inode *inode, struct file *f);
static ssize_t sheep_net_read(struct file *f, char *buf, size_t count, loff_t *off);
static ssize_t sheep_net_write(struct file *f, const char *buf, size_t count, loff_t *off);
static unsigned int sheep_net_poll(struct file *f, struct poll_table_struct *wait);
static int sheep_net_ioctl(struct inode *inode, struct file *f, unsigned int code, unsigned long arg);
static int sheep_net_receiver(struct sk_buff *skb, struct device *dev, struct packet_type *pt);


/*
 *  Driver private variables
 */

struct SheepVars {
	struct device *ether;		// The Ethernet device we're attached to
	struct sock *skt;			// Socket for communication with Ethernet card
	struct sk_buff_head queue;	// Receiver packet queue
	struct packet_type pt;		// Receiver packet type
	struct wait_queue *wait;	// Wait queue for blocking read operations
};


/*
 *  file_operations structure - has function pointers to the
 *  various entry points for device operations
 */

static struct file_operations sheep_net_fops = {
	NULL,	// llseek
	sheep_net_read,
	sheep_net_write,
	NULL,	// readdir
	sheep_net_poll,
	sheep_net_ioctl,
	NULL,	// mmap
	sheep_net_open,
	NULL,	// flush
	sheep_net_release,
	NULL,	// fsync
	NULL,	// fasync
	NULL,	// check_media_change
	NULL,	// revalidate
	NULL	// lock
};


/*
 *  miscdevice structure for driver initialization
 */

static struct miscdevice sheep_net_device = {
	SHEEP_NET_MINOR,	// minor number
	"sheep_net",		// name
	&sheep_net_fops,
	NULL,
	NULL
};


/*
 *  Initialize module
 */

int init_module(void)
{
	int ret;

	// Register driver
	ret = misc_register(&sheep_net_device);
	D(bug("Sheep net driver installed\n"));
	return ret;
}


/*
 *  Deinitialize module
 */

int cleanup_module(void)
{
	int ret;

	// Unregister driver
	ret = misc_deregister(&sheep_net_device);
	D(bug("Sheep net driver removed\n"));
	return ret;
}


/*
 *  Driver open() function
 */

static int sheep_net_open(struct inode *inode, struct file *f)
{
	struct SheepVars *v;
	D(bug("sheep_net: open\n"));

	// Must be opened with read permissions
	if ((f->f_flags & O_ACCMODE) == O_WRONLY)
		return -EPERM;

	// Allocate private variables
	v = (struct SheepVars *)f->private_data = kmalloc(sizeof(struct SheepVars), GFP_USER);
	if (v == NULL)
		return -ENOMEM;
	memset(v, 0, sizeof(struct SheepVars));
	skb_queue_head_init(&v->queue);

	// Yes, we're open
	MOD_INC_USE_COUNT;
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

	// Detach from Ethernet card
	if (v->ether) {
		dev_remove_pack(&v->pt);
		sk_free(v->skt);
		v->skt = NULL;
		v->ether = NULL;
	}

	// Empty packet queue
	while ((skb = skb_dequeue(&v->queue)) != NULL)
		dev_kfree_skb(skb);

	// Free private variables
	kfree(v);

	// Sorry, we're closed
	MOD_DEC_USE_COUNT;
	return 0;
}


/*
 *  Driver read() function
 */

static ssize_t sheep_net_read(struct file *f, char *buf, size_t count, loff_t *off)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	struct sk_buff *skb;
	sigset_t sigs;
	int i;
	D(bug("sheep_net: read\n"));

	for (;;) {

		// Get next packet from queue
		start_bh_atomic();
		skb = skb_dequeue(&v->queue);
		end_bh_atomic();
		if (skb != NULL || (f->f_flags & O_NONBLOCK))
			break;

		// No packet in queue and in blocking mode, so block
		interruptible_sleep_on(&v->wait);

		// Signal received? Then bail out
		signandsets(&sigs, &current->signal, &current->blocked);
		for (i=0; i<_NSIG_WORDS; i++) {
			if (sigs.sig[i])
				return -EINTR;
		}
	}
	if (skb == NULL)
		return -EAGAIN;

	// Pass packet to caller
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

static ssize_t sheep_net_write(struct file *f, const char *buf, size_t count, loff_t *off)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	struct sk_buff *skb;
	char *p;
	D(bug("sheep_net: write\n"));

	// Check packet size
	if (count < sizeof(struct ethhdr))
		return -EINVAL;
	if (count > 1514)
		count = 1514;

	// Interface active?
	if (v->ether == NULL)
		return count;

	// Allocate buffer for packet
	skb = dev_alloc_skb(count);
	if (skb == NULL)
		return -ENOBUFS;

	// Stuff packet in buffer
	p = skb_put(skb, count);
	if (copy_from_user(p, buf, count)) {
		kfree_skb(skb);
		return -EFAULT;
	}

	// Transmit packet
	dev_lock_list();
	atomic_add(skb->truesize, &v->skt->wmem_alloc);
	skb->sk = v->skt;
	skb->dev = v->ether;
	skb->priority = 0;
	skb->protocol = PROT_MAGIC;	// "Magic" protocol value to recognize our packets in sheep_net_receiver()
	skb->nh.raw = skb->h.raw = skb->data + v->ether->hard_header_len;
	dev_unlock_list();
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

	// Packets in queue? Then return
	start_bh_atomic();
	if (!skb_queue_empty(&v->queue)) {
		end_bh_atomic();
		return POLLIN | POLLRDNORM;
	}

	// Otherwise wait for packet
	poll_wait(f, &v->wait, wait);
	if (!skb_queue_empty(&v->queue)) {
		end_bh_atomic();
		return POLLIN | POLLRDNORM;
	} else {
		end_bh_atomic();
		return 0;
	}
}


/*
 *  Driver ioctl() function
 */

static int sheep_net_ioctl(struct inode *inode, struct file *f, unsigned int code, unsigned long arg)
{
	struct SheepVars *v = (struct SheepVars *)f->private_data;
	D(bug("sheep_net: ioctl %04x\n", code));

	switch (code) {

		// Attach to Ethernet card
		// arg: pointer to name of Ethernet device (char[8])
		case SIOCSIFLINK: {
			char name[8];

			// Already attached?
			if (v->ether)
				return -EBUSY;

			// Get Ethernet card name
			if (copy_from_user(name, (void *)arg, 8))
				return -EFAULT;
			name[7] = 0;

			// Find card
			dev_lock_list();
			v->ether = dev_get(name);
			if (v->ether == NULL) {
				dev_unlock_list();
				return -ENODEV;
			}

			// Is it Ethernet?
			if (v->ether->type != ARPHRD_ETHER) {
				v->ether = NULL;
				dev_unlock_list();
				return -EINVAL;
			}

			// Allocate socket
			v->skt = sk_alloc(0, GFP_USER, 1);
			if (v->skt == NULL) {
				v->ether = NULL;
				dev_unlock_list();
				return -ENOMEM;
			}
			v->skt->dead = 1;

			// Attach packet handler
			v->pt.type = htons(ETH_P_ALL);
			v->pt.dev = v->ether;
			v->pt.func = sheep_net_receiver;
			v->pt.data = v;
			dev_add_pack(&v->pt);
			dev_unlock_list();
			return 0;
		}

		// Get hardware address of Ethernet card
		// arg: pointer to buffer (6 bytes) to store address
		case SIOCGIFADDR:
			if (v->ether == NULL)
				return -ENODEV;
			if (copy_to_user((void *)arg, v->ether->dev_addr, 6))
				return -EFAULT;
			return 0;

		// Add multicast address
		// arg: pointer to address (6 bytes)
		case SIOCADDMULTI: {
			char addr[6];
			if (v->ether == NULL)
				return -ENODEV;
			if (copy_from_user(addr, (void *)arg, 6))
				return -EFAULT;
			return dev_mc_add(v->ether, addr, 6, 0);
		}

		// Remove multicast address
		// arg: pointer to address (6 bytes)
		case SIOCDELMULTI: {
			char addr[6];
			if (v->ether == NULL)
				return -ENODEV;
			if (copy_from_user(addr, (void *)arg, 6))
				return -EFAULT;
			return dev_mc_delete(v->ether, addr, 6, 0);
		}

		// Return size of first packet in queue
		case FIONREAD: {
			int count = 0;
			struct sk_buff *skb;
			start_bh_atomic();
			skb = skb_peek(&v->queue);
			if (skb)
				count = skb->len;
			end_bh_atomic();
			return put_user(count, (int *)arg);
		}

		default:
			return -ENOIOCTLCMD;
	}
}


/*
 *  Packet receiver function
 */

static int sheep_net_receiver(struct sk_buff *skb, struct device *dev, struct packet_type *pt)
{
	struct SheepVars *v = (struct SheepVars *)pt->data;
	D(bug("sheep_net: packet received\n"));

	// Packet sent by us? Then discard
	if (skb->protocol == PROT_MAGIC) {
		kfree_skb(skb);
		return 0;
	}

	// Discard packets if queue gets too full
	if (skb_queue_len(&v->queue) > MAX_QUEUE) {
		kfree_skb(skb);
		return 0;
	}

	// We also want the Ethernet header
	skb_push(skb, skb->data - skb->mac.raw);

	// Enqueue packet
	start_bh_atomic();
	skb_queue_tail(&v->queue, skb);
	end_bh_atomic();

	// Unblock blocked read
	wake_up(&v->wait);
	return 0;
}
