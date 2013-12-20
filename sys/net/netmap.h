/*
 * Copyright (C) 2011-2013 Matteo Landi, Luigi Rizzo. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 * 
 *   3. Neither the name of the authors nor the names of their contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY MATTEO LANDI AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTEO LANDI OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 *
 * Definitions of constants and the structures used by the netmap
 * framework, for the part visible to both kernel and userspace.
 * Detailed info on netmap is available with "man netmap" or at
 * 
 *	http://info.iet.unipi.it/~luigi/netmap/
 */

#ifndef _NET_NETMAP_H_
#define _NET_NETMAP_H_

/*
 * --- Netmap data structures ---
 *
 * The data structures used by netmap are shown below. Those in
 * capital letters are in an mmapp()ed area shared with userspace,
 * while others are private to the kernel.
 * Shared structures do not contain pointers but only memory
 * offsets, so that addressing is portable between kernel and userspace.


 softc
+----------------+
| standard fields|
| if_pspare[0] ----------+
+----------------+       |
                         |
+----------------+<------+
|(netmap_adapter)|
|                |                             netmap_kring
| tx_rings *--------------------------------->+---------------+
|                |       netmap_kring         | ring    *---------.
| rx_rings *--------->+---------------+       | nr_hwcur      |   |
+----------------+    | ring    *--------.    | nr_hwavail    |   V
                      | nr_hwcur      |  |    | selinfo       |   |
                      | nr_hwavail    |  |    +---------------+   .
                      | selinfo       |  |    |     ...       |   .
                      +---------------+  |    |(ntx+1 entries)|
                      |    ....       |  |    |               |
                      |(nrx+1 entries)|  |    +---------------+
                      |               |  |
   KERNEL             +---------------+  |
                                         |
  ====================================================================
                                         |
   USERSPACE                             |      NETMAP_RING
                                         +---->+-------------+
                                             / | cur         |
   NETMAP_IF  (nifp, one per file desc.)    /  | avail       |
    +---------------+                      /   | buf_ofs     |
    | ni_tx_rings   |                     /    +=============+
    | ni_rx_rings   |                    /     | buf_idx     | slot[0]
    |               |                   /      | len, flags  |
    |               |                  /       +-------------+
    +===============+                 /        | buf_idx     | slot[1]
    | txring_ofs[0] | (rel.to nifp)--'         | len, flags  |
    | txring_ofs[1] |                          +-------------+
  (num_rings+1 entries)                     (nr_num_slots entries)
    | txring_ofs[n] |                          | buf_idx     | slot[n-1]
    +---------------+                          | len, flags  |
    | rxring_ofs[0] |                          +-------------+
    | rxring_ofs[1] |
  (num_rings+1 entries)
    | txring_ofs[n] |
    +---------------+

 * The private descriptor ('softc' or 'adapter') of each interface
 * is extended with a "struct netmap_adapter" containing netmap-related
 * info (see description in dev/netmap/netmap_kernel.h.
 * Among other things, tx_rings and rx_rings point to the arrays of
 * "struct netmap_kring" which in turn reache the various
 * "struct netmap_ring", shared with userspace.

 * The NETMAP_RING is the userspace-visible replica of the NIC ring.
 * Each slot has the index of a buffer, its length and some flags.
 * In user space, the buffer address is computed as
 *	(char *)ring + buf_ofs + index*NETMAP_BUF_SIZE
 * In the kernel, buffers do not necessarily need to be contiguous,
 * and the virtual and physical addresses are derived through
 * a lookup table.
 *
 * struct netmap_slot:
 *
 * buf_idx	is the index of the buffer associated to the slot.
 * len		is the length of the payload
 * NS_BUF_CHANGED	must be set whenever userspace wants
 *		to change buf_idx (it might be necessary to
 *		reprogram the NIC slot)
 * NS_REPORT	must be set if we want the NIC to generate an interrupt
 *		when this slot is used. Leaving it to 0 improves
 *		performance.
 * NS_FORWARD	if set on a receive ring, and the device is in
 *		transparent mode, buffers released with the flag set
 *		will be forwarded to the 'other' side (host stack
 *		or NIC, respectively) on the next select() or ioctl()
 *
 *		The following will be supported from NETMAP_API = 5
 * NS_NO_LEARN	on a VALE switch, do not 'learn' the source port for
 *		this packet.
 * NS_INDIRECT	the netmap buffer contains a 64-bit pointer to
 *		the actual userspace buffer. This may be useful
 *		to reduce copies in a VM environment.
 * NS_MOREFRAG	Part of a multi-segment frame. The last (or only)
 *		segment must not have this flag.
 * NS_PORT_MASK	the high 8 bits of the flag, if not zero, indicate the
 *		destination port for the VALE switch, overriding
 *		the lookup table.
 */

struct netmap_slot {
	uint32_t buf_idx; /* buffer index */
	uint16_t len;	/* packet length, to be copied to/from the hw ring */
	uint16_t flags;	/* buf changed, etc. */
#define	NS_BUF_CHANGED	0x0001	/* must resync the map, buffer changed */
#define	NS_REPORT	0x0002	/* ask the hardware to report results
				 * e.g. by generating an interrupt
				 */
#define	NS_FORWARD	0x0004	/* pass packet to the other endpoint
				 * (host stack or device)
				 */
#define	NS_NO_LEARN	0x0008
#define	NS_INDIRECT	0x0010
#define	NS_MOREFRAG	0x0020
#define	NS_PORT_SHIFT	8
#define	NS_PORT_MASK	(0xff << NS_PORT_SHIFT)
};

/*
 * Netmap representation of a TX or RX ring (also known as "queue").
 * This is a queue implemented as a fixed-size circular array.
 * At the software level, two fields are important: avail and cur.
 *
 * In TX rings:
 *	avail	indicates the number of slots available for transmission.
 *		It is updated by the kernel after every netmap system call.
 *		It MUST BE decremented by the application when it appends a
 *		packet.
 *	cur	indicates the slot to use for the next packet
 *		to send (i.e. the "tail" of the queue).
 *		It MUST BE incremented by the application before
 *		netmap system calls to reflect the number of newly
 *		sent packets.
 *		It is checked by the kernel on netmap system calls
 *		(normally unmodified by the kernel unless invalid).
 *
 *   The kernel side of netmap uses two additional fields in its own
 *   private ring structure, netmap_kring:
 *	nr_hwcur is a copy of nr_cur on an NIOCTXSYNC.
 *	nr_hwavail is the number of slots known as available by the
 *		hardware. It is updated on an INTR (inc by the
 *		number of packets sent) and on a NIOCTXSYNC
 *		(decrease by nr_cur - nr_hwcur)
 *		A special case, nr_hwavail is -1 if the transmit
 *		side is idle (no pending transmits).
 *
 * In RX rings:
 *	avail	is the number of packets available (possibly 0).
 *		It MUST BE decremented by the application when it consumes
 *		a packet, and it is updated to nr_hwavail on a NIOCRXSYNC
 *	cur	indicates the first slot that contains a packet not
 *		processed yet (the "head" of the queue).
 *		It MUST BE incremented by the software when it consumes
 *		a packet.
 *	reserved	indicates the number of buffers before 'cur'
 *		that the application has still in use. Normally 0,
 *		it MUST BE incremented by the application when it
 *		does not return the buffer immediately, and decremented
 *		when the buffer is finally freed.
 *
 *   The kernel side of netmap uses two additional fields in the kring:
 *	nr_hwcur is a copy of nr_cur on an NIOCRXSYNC
 *	nr_hwavail is the number of packets available. It is updated
 *		on INTR (inc by the number of new packets arrived)
 *		and on NIOCRXSYNC (decreased by nr_cur - nr_hwcur).
 *
 * DATA OWNERSHIP/LOCKING:
 *	The netmap_ring is owned by the user program and it is only
 *	accessed or modified in the upper half of the kernel during
 *	a system call.
 *
 *	The netmap_kring is only modified by the upper half of the kernel.
 *
 * FLAGS
 *	NR_TIMESTAMP	updates the 'ts' field on each syscall. This is
 *			a global timestamp for all packets.
 *	NR_RX_TSTMP	if set, the last 64 byte in each buffer will
 *			contain a timestamp for the frame supplied by
 *			the hardware (if supported)
 *	NR_FORWARD	if set, the NS_FORWARD flag in each slot of the
 *			RX ring is checked, and if set the packet is
 *			passed to the other side (host stack or device,
 *			respectively). This permits bpf-like behaviour
 *			or transparency for selected packets.
 */
struct netmap_ring {
	/*
	 * nr_buf_base_ofs is meant to be used through macros.
	 * It contains the offset of the buffer region from this
	 * descriptor.
	 */
	const ssize_t	buf_ofs;
	const uint32_t	num_slots;	/* number of slots in the ring. */
	uint32_t	avail;		/* number of usable slots */
	uint32_t        cur;		/* 'current' r/w position */
	uint32_t	reserved;	/* not refilled before current */

	const uint16_t	nr_buf_size;
	uint16_t	flags;
#define	NR_TIMESTAMP	0x0002		/* set timestamp on *sync() */
#define	NR_FORWARD	0x0004		/* enable NS_FORWARD for ring */
#define	NR_RX_TSTMP	0x0008		/* set rx timestamp in slots */

	struct timeval	ts;		/* time of last *sync() */

	/* the slots follow. This struct has variable size */
	struct netmap_slot slot[0];	/* array of slots. */
};


/*
 * Netmap representation of an interface and its queue(s).
 * There is one netmap_if for each file descriptor on which we want
 * to select/poll.  We assume that on each interface has the same number
 * of receive and transmit queues.
 * select/poll operates on one or all pairs depending on the value of
 * nmr_queueid passed on the ioctl.
 */
struct netmap_if {
	char		ni_name[IFNAMSIZ]; /* name of the interface. */
	const u_int	ni_version;	/* API version, currently unused */
	const u_int	ni_rx_rings;	/* number of rx rings */
	const u_int	ni_tx_rings;	/* if zero, same as ni_rx_rings */
	/*
	 * The following array contains the offset of each netmap ring
	 * from this structure. The first ni_tx_queues+1 entries refer
	 * to the tx rings, the next ni_rx_queues+1 refer to the rx rings
	 * (the last entry in each block refers to the host stack rings).
	 * The area is filled up by the kernel on NIOCREG,
	 * and then only read by userspace code.
	 */
	const ssize_t	ring_ofs[0];
};

#ifndef NIOCREGIF	
/*
 * ioctl names and related fields
 *
 * NIOCGINFO takes a struct ifreq, the interface name is the input,
 *	the outputs are number of queues and number of descriptor
 *	for each queue (useful to set number of threads etc.).
 *
 * NIOCREGIF takes an interface name within a struct ifreq,
 *	and activates netmap mode on the interface (if possible).
 *
 *	For vale ports, starting with NETMAP_API = 5,
 *	nr_tx_rings and nr_rx_rings specify how many software rings
 *	are created (0 means 1).
 *
 *	NIOCREGIF is also used to attach a NIC to a VALE switch.
 *	In this case the name is vale*:ifname, and "nr_cmd"
 *	is set to 'NETMAP_BDG_ATTACH' or 'NETMAP_BDG_DETACH'.
 *	nr_ringid specifies which rings should be attached, 0 means all,
 *	NETMAP_HW_RING + n means only the n-th ring.
 *	The process can terminate after the interface has been attached.
 *
 * NIOCUNREGIF unregisters the interface associated to the fd.
 *	this is deprecated and will go away.
 *
 * NIOCTXSYNC, NIOCRXSYNC synchronize tx or rx queues,
 *	whose identity is set in NIOCREGIF through nr_ringid
 *
 * NETMAP_API is the API version.
 */

/*
 * struct nmreq overlays a struct ifreq
 */
struct nmreq {
	char		nr_name[IFNAMSIZ];
	uint32_t	nr_version;	/* API version */
#define	NETMAP_API	4		/* current version */
	uint32_t	nr_offset;	/* nifp offset in the shared region */
	uint32_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */
	uint16_t	nr_ringid;	/* ring(s) we care about */
#define NETMAP_HW_RING	0x4000		/* low bits indicate one hw ring */
#define NETMAP_SW_RING	0x2000		/* process the sw ring */
#define NETMAP_NO_TX_POLL	0x1000	/* no automatic txsync on poll */
#define NETMAP_RING_MASK 0xfff		/* the ring number */
	uint16_t	nr_cmd;
#define NETMAP_BDG_ATTACH	1	/* attach the NIC */
#define NETMAP_BDG_DETACH	2	/* detach the NIC */
#define NETMAP_BDG_LOOKUP_REG	3	/* register lookup function */
#define NETMAP_BDG_LIST		4	/* get bridge's info */
#define NETMAP_REG_WITH_FLAGS 32 /* provide extra registration flags */
	uint16_t	nr_arg1;
#define NETMAP_BDG_HOST		1	/* attach the host stack on ATTACH */
#define NETMAP_PERSIST 0x1
	uint16_t	nr_arg2;
	uint32_t	spare2[3];
};

/*
 * FreeBSD uses the size value embedded in the _IOWR to determine
 * how much to copy in/out. So we need it to match the actual
 * data structure we pass. We put some spares in the structure
 * to ease compatibility with other versions
 */
#define NIOCGINFO	_IOWR('i', 145, struct nmreq) /* return IF info */
#define NIOCREGIF	_IOWR('i', 146, struct nmreq) /* interface register */
#define NIOCUNREGIF	_IO('i', 147) /* interface unregister */
#define NIOCTXSYNC	_IO('i', 148) /* sync tx queues */
#define NIOCRXSYNC	_IO('i', 149) /* sync rx queues */
#endif /* !NIOCREGIF */

#endif /* _NET_NETMAP_H_ */
