#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/sctp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/prefetch.h>
#include "rnpvf_compat.h"

#include "rnpvf.h"

#define FIX_VEB_BUG
#ifdef FIX_VF_BUG
#define CONFIG_BAR4_PFVFNUM 0
#else
#define CONFIG_BAR4_PFVFNUM 1
#endif
char rnpvf_driver_name[] = "rnpvf";
static const char rnpvf_driver_string[] =
	"Mucse(R) 10/40G Gigabit PCI Express Virtual Function Network Driver";

#define DRV_VERSION "0.2.1"
const char rnpvf_driver_version[] = DRV_VERSION;
static const char rnpvf_copyright[] =
	"Copyright (c) 2020 - 2023 Mucse Corporation.";

extern const struct rnpvf_info rnp_n10_vf_info;
extern const struct rnpvf_info rnp_n500_vf_info;

static const struct rnpvf_info *rnpvf_info_tbl[] = {
	[board_n10] = &rnp_n10_vf_info,
	[board_n500] = &rnp_n500_vf_info,
};

#define N10_BOARD	 board_n10
#define N500_BOARD       board_n500

static unsigned int fix_eth_name;
module_param(fix_eth_name, uint, 0000);
MODULE_PARM_DESC(fix_eth_name, "set eth adapter name to rnpvfXX");
static struct pci_device_id rnpvf_pci_tbl[] = {
	{PCI_DEVICE(0x8848, 0x1080), .driver_data = N10_BOARD},
	//{PCI_DEVICE(0x8848, 0x8309), .driver_data = N500_BOARD},
	{PCI_DEVICE(0x8848, 0x1081), .driver_data = N10_BOARD },
	//{PCI_DEVICE(0x1dab, 0x8001), .driver_data = N10_BOARD},
	//{PCI_DEVICE(0x1dab, 0x8002), .driver_data = N10_BOARD},
	/* required last entry */
	{
		0,
	},
};

//static unsigned int irq_mode = 0;
//module_param(irq_mode, uint, 0);
//MODULE_PARM_DESC(irq_mode, "set eth irq mode (0:msix 1 msi 2 legacy)");

MODULE_DEVICE_TABLE(pci, rnpvf_pci_tbl);

MODULE_AUTHOR("Mucse Corporation, <mucse@mucse.com>");
MODULE_DESCRIPTION("Mucse(R) N10/N500/N400 Virtual Function Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#define DEFAULT_MSG_ENABLE (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK)
static int debug = -1;
module_param(debug, int, 0000);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

static int pci_using_hi_dma;

/* forward decls */
static void rnpvf_set_itr(struct rnpvf_q_vector *q_vector);
static void rnpvf_free_all_rx_resources(struct rnpvf_adapter *adapter);


#define RNPVF_XDP_PASS 0
#define RNPVF_XDP_CONSUMED 1
#define RNPVF_XDP_TX 2


#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT
static bool rnpvf_alloc_mapped_skb(struct rnpvf_ring *rx_ring,
                                 struct rnpvf_rx_buffer *bi);
#else /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
static void rnpvf_pull_tail(struct sk_buff *skb);
#ifdef OPTM_WITH_LPAGE
static bool rnpvf_alloc_mapped_page(struct rnpvf_ring *rx_ring, struct rnpvf_rx_buffer *bi,
                                  union rnp_rx_desc *rx_desc, u16 bufsz,
                                  u64 fun_id);

static void rnpvf_put_rx_buffer(struct rnpvf_ring *rx_ring,
                              struct rnpvf_rx_buffer *rx_buffer);
#else /* OPTM_WITH_LPAGE */
static bool rnpvf_alloc_mapped_page(struct rnpvf_ring *rx_ring,
                                  struct rnpvf_rx_buffer *bi);
static void rnpvf_put_rx_buffer(struct rnpvf_ring *rx_ring,
                              struct rnpvf_rx_buffer *rx_buffer,
                              struct sk_buff *skb);
#endif /* OPTM_WITH_LPAGE */

#endif /* CONFIG_RNP_DISABLE_PACKET_SPLIT */

/**
 * rnpvf_set_ivar - set IVAR registers - maps interrupt causes to vectors
 * @adapter: pointer to adapter struct
 * @direction: 0 for Rx, 1 for Tx, -1 for other causes
 * @queue: queue to map the corresponding interrupt to
 * @msix_vector: the vector to map to the corresponding queue
 */
static void rnpvf_set_ring_vector(struct rnpvf_adapter *adapter,
		u8 rnpvf_queue,
		u8 rnpvf_msix_vector)
{
	struct rnpvf_hw *hw = &adapter->hw;
	u32 data = 0;

	data = hw->vfnum << 24;
	data |= (rnpvf_msix_vector << 8);
	data |= (rnpvf_msix_vector << 0);
	DPRINTK(IFUP, INFO, 
		"Set Ring-Vector queue:%d (reg:0x%x) <-- Rx-MSIX:%d, Tx-MSIX:%d\n",
		rnpvf_queue,
		RING_VECTOR(rnpvf_queue),
		rnpvf_msix_vector,
		rnpvf_msix_vector);

	rnpvf_wr_reg(hw->ring_msix_base + RING_VECTOR(rnpvf_queue), data);
}

void rnpvf_unmap_and_free_tx_resource(struct rnpvf_ring *ring,
									  struct rnpvf_tx_buffer *tx_buffer)
{
	if (tx_buffer->skb) {
		dev_kfree_skb_any(tx_buffer->skb);
		if (dma_unmap_len(tx_buffer, len))
			dma_unmap_single(ring->dev,
					dma_unmap_addr(tx_buffer, dma),
					dma_unmap_len(tx_buffer, len),
					DMA_TO_DEVICE);
	} else if (dma_unmap_len(tx_buffer, len)) {
		dma_unmap_page(ring->dev,
				dma_unmap_addr(tx_buffer, dma),
				dma_unmap_len(tx_buffer, len),
				DMA_TO_DEVICE);
	}
	tx_buffer->next_to_watch = NULL;
	tx_buffer->skb = NULL;
	dma_unmap_len_set(tx_buffer, len, 0);
	/* tx_buffer must be completely set up in the transmit path */
}

static void rnpvf_tx_timeout(struct net_device *netdev);

/**
 * rnpvf_clean_tx_irq - Reclaim resources after transmit completes
 * @q_vector: board private structure
 * @tx_ring: tx ring to clean
 **/
static bool rnpvf_clean_tx_irq(struct rnpvf_q_vector *q_vector,
							   struct rnpvf_ring *tx_ring)
{
	struct rnpvf_adapter *adapter = q_vector->adapter;
	struct rnpvf_tx_buffer *tx_buffer;
	struct rnp_tx_desc *tx_desc;
	unsigned int total_bytes = 0, total_packets = 0;
	// unsigned int budget = RNPVF_DEFAULT_TX_WORK;
	unsigned int budget = adapter->tx_work_limit;
	unsigned int i = tx_ring->next_to_clean;

	if (test_bit(__RNPVF_DOWN, &adapter->state))
		return true;
	tx_ring->tx_stats.poll_count++;
	tx_buffer = &tx_ring->tx_buffer_info[i];
	tx_desc = RNPVF_TX_DESC(tx_ring, i);
	i -= tx_ring->count;

	do {
		struct rnp_tx_desc *eop_desc = tx_buffer->next_to_watch;

		/* if next_to_watch is not set then there is no work pending */
		if (!eop_desc)
			break;

		/* prevent any other reads prior to eop_desc */
		rmb();
		//read_barrier_depends();

		/* if eop DD is not set pending work has not been completed */
		if (!(eop_desc->cmd & cpu_to_le16(RNP_TXD_STAT_DD)))
			break;

		/* clear next_to_watch to prevent false hangs */
		tx_buffer->next_to_watch = NULL;

		/* update the statistics for this packet */
		total_bytes += tx_buffer->bytecount;
		total_packets += tx_buffer->gso_segs;

		/* free the skb */
		dev_kfree_skb_any(tx_buffer->skb);

		/* unmap skb header data */
		dma_unmap_single(tx_ring->dev,
				dma_unmap_addr(tx_buffer, dma),
				dma_unmap_len(tx_buffer, len),
				DMA_TO_DEVICE);

		/* clear tx_buffer data */
		tx_buffer->skb = NULL;
		dma_unmap_len_set(tx_buffer, len, 0);

		/* unmap remaining buffers */
		while (tx_desc != eop_desc) {
			tx_buffer++;
			tx_desc++;
			i++;
			if (unlikely(!i)) {
				i -= tx_ring->count;
				tx_buffer = tx_ring->tx_buffer_info;
				tx_desc = RNPVF_TX_DESC(tx_ring, 0);
			}

			/* unmap any remaining paged data */
			if (dma_unmap_len(tx_buffer, len)) {
				dma_unmap_page(tx_ring->dev,
						dma_unmap_addr(tx_buffer, dma),
						dma_unmap_len(tx_buffer, len),
						DMA_TO_DEVICE);
				dma_unmap_len_set(tx_buffer, len, 0);
			}
		}

		/* move us one more past the eop_desc for start of next pkt */
		tx_buffer++;
		tx_desc++;
		i++;
		if (unlikely(!i)) {
			i -= tx_ring->count;
			tx_buffer = tx_ring->tx_buffer_info;
			tx_desc = RNPVF_TX_DESC(tx_ring, 0);
		}

		/* issue prefetch for next Tx descriptor */
		prefetch(tx_desc);

		/* update budget accounting */
		budget--;
	} while (likely(budget));

	i += tx_ring->count;
	tx_ring->next_to_clean = i;
	u64_stats_update_begin(&tx_ring->syncp);
	tx_ring->stats.bytes += total_bytes;
	tx_ring->stats.packets += total_packets;
	u64_stats_update_end(&tx_ring->syncp);
	q_vector->tx.total_bytes += total_bytes;
	q_vector->tx.total_packets += total_packets;

	netdev_tx_completed_queue(txring_txq(tx_ring), total_packets, total_bytes);

	if (!(q_vector->vector_flags & RNPVF_QVECTOR_FLAG_REDUCE_TX_IRQ_MISS)) {
#define TX_WAKE_THRESHOLD (DESC_NEEDED * 2)
		if (unlikely(total_packets && netif_carrier_ok(tx_ring->netdev) &&
					(rnpvf_desc_unused(tx_ring) >= TX_WAKE_THRESHOLD))) {
			/* Make sure that anybody stopping the queue after this
			 * sees the new next_to_clean.
			 */
			smp_mb();
			if (__netif_subqueue_stopped(tx_ring->netdev, tx_ring->queue_index) &&
					!test_bit(__RNPVF_DOWN, &adapter->state)) {
				netif_wake_subqueue(tx_ring->netdev, tx_ring->queue_index);
				++tx_ring->tx_stats.restart_queue;
			}
		}
	}

	return !!budget;
}

static inline void rnpvf_rx_hash(struct rnpvf_ring *ring,
		union rnp_rx_desc *rx_desc,
		struct sk_buff *skb)
{
	int rss_type;

	if (!(ring->netdev->features & NETIF_F_RXHASH))
		return;

#define RNPVF_RSS_TYPE_MASK 0xc0
        rss_type = rx_desc->wb.cmd & RNPVF_RSS_TYPE_MASK;
        skb_set_hash(skb, le32_to_cpu(rx_desc->wb.rss_hash),
                     rss_type ? PKT_HASH_TYPE_L4 : PKT_HASH_TYPE_L3);
/*
#if defined(RHEL_RELEASE_CODE) && RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(7,1)
		skb->rxhash = le32_to_cpu(rx_desc->wb.rss_hash);
#else
		skb->hash = le32_to_cpu(rx_desc->wb.rss_hash);
#endif
*/
}

/**
 * rnpvf_rx_checksum - indicate in skb if hw indicated a good cksum
 * @ring: structure containing ring specific data
 * @rx_desc: current Rx descriptor being processed
 * @skb: skb currently being received and modified
 **/
static inline void rnpvf_rx_checksum(struct rnpvf_ring *ring,
		union rnp_rx_desc *rx_desc,
		struct sk_buff *skb)
{
	bool encap_pkt = false;

	skb_checksum_none_assert(skb);

	/* Rx csum disabled */
	if (!(ring->netdev->features & NETIF_F_RXCSUM))
		return;

        /* vxlan packet handle ? */

        if (!(ring->ring_flags & RNPVF_RING_NO_TUNNEL_SUPPORT)) {
                if (rnpvf_get_stat(rx_desc, RNP_RXD_STAT_TUNNEL_MASK) ==
                                RNP_RXD_STAT_TUNNEL_VXLAN) {
                        encap_pkt = true;
#if defined(HAVE_UDP_ENC_RX_OFFLOAD) || defined(HAVE_VXLAN_RX_OFFLOAD)
                        skb->encapsulation = 1;
#endif /* HAVE_UDP_ENC_RX_OFFLOAD || HAVE_VXLAN_RX_OFFLOAD */
                        skb->ip_summed = CHECKSUM_NONE;
                }
        }

	/* if L3/L4  error:ignore errors from veb(other vf) */
	if (unlikely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_ERR_MASK)
					 )) {
		ring->rx_stats.csum_err++;
		return;
	}
	ring->rx_stats.csum_good++;
	/* It must be a TCP or UDP packet with a valid checksum */
	skb->ip_summed = CHECKSUM_UNNECESSARY;
        if (encap_pkt) {
#ifdef HAVE_SKBUFF_CSUM_LEVEL
                /* If we checked the outer header let the stack know */
                skb->csum_level = 1;
#endif /* HAVE_SKBUFF_CSUM_LEVEL */
        }

}

static inline void rnpvf_update_rx_tail(struct rnpvf_ring *rx_ring, u32 val)
{
	rx_ring->next_to_use = val;

#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
	/* update next to alloc since we have filled the ring */
	rx_ring->next_to_alloc = val;
#endif
	/*
	 * Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64).
	 */
	wmb();
	rnpvf_wr_reg(rx_ring->tail, val);
}


#ifndef OPTM_WITH_LPAGE
/**
 * rnpvf_alloc_rx_buffers - Replace used receive buffers
 * @rx_ring: ring to place buffers on
 * @cleaned_count: number of buffers to replace
 **/
void rnpvf_alloc_rx_buffers(struct rnpvf_ring *rx_ring, u16 cleaned_count)
{
        union rnp_rx_desc *rx_desc;
        struct rnpvf_rx_buffer *bi;
        u16 i = rx_ring->next_to_use;
        u64 fun_id = ((u64)(rx_ring->vfnum) << (32 + 24));
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
        u16 bufsz;
#endif
        /* nothing to do */
        if (!cleaned_count)
                return;

        rx_desc = RNPVF_RX_DESC(rx_ring, i);

        BUG_ON(rx_desc == NULL);

        bi = &rx_ring->rx_buffer_info[i];

        BUG_ON(bi == NULL);

        i -= rx_ring->count;
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
        bufsz = rnpvf_rx_bufsz(rx_ring);
#endif

        do {
#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT
                if (!rnpvf_alloc_mapped_skb(rx_ring, bi))
                        break;
#else
                if (!rnpvf_alloc_mapped_page(rx_ring, bi))
                        break;

                dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
                                bi->page_offset, bufsz,
                                DMA_FROM_DEVICE);
#endif

                /*
                 * Refresh the desc even if buffer_addrs didn't change
                 * because each write-back erases this info.
                 */
#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT
                rx_desc->pkt_addr = cpu_to_le64(bi->dma + fun_id);
#else
                rx_desc->pkt_addr =
                        cpu_to_le64(bi->dma + bi->page_offset + fun_id);

                //printk("%d rx_desc page_offset %x\n", i, bi->page_offset);
#endif
                /* clean dd */
                rx_desc->cmd = 0;

                rx_desc++;
                bi++;
                i++;
                if (unlikely(!i)) {
                        rx_desc = RNPVF_RX_DESC(rx_ring, 0);
                        bi = rx_ring->rx_buffer_info;
                        i -= rx_ring->count;
                }

                /* clear the hdr_addr for the next_to_use descriptor */
                // rx_desc->cmd = 0;
                cleaned_count--;
        } while (cleaned_count);

        i += rx_ring->count;

        if (rx_ring->next_to_use != i)
                rnpvf_update_rx_tail(rx_ring, i);
}
#endif

#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
/**
 * rnpvf_reuse_rx_page - page flip buffer and store it back on the ring
 * @rx_ring: rx descriptor ring to store buffers on
 * @old_buff: donor buffer to have page reused
 *
 * Synchronizes page for reuse by the adapter
 **/
static void rnpvf_reuse_rx_page(struct rnpvf_ring *rx_ring,
                              struct rnpvf_rx_buffer *old_buff)
{
        struct rnpvf_rx_buffer *new_buff;
        u16 nta = rx_ring->next_to_alloc;

        new_buff = &rx_ring->rx_buffer_info[nta];

        /* update, and store next to alloc */
        nta++;
        rx_ring->next_to_alloc = (nta < rx_ring->count) ? nta : 0;

        /*
         * Transfer page from old buffer to new buffer.
         * Move each member individually to avoid possible store
         * forwarding stalls and unnecessary copy of skb.
         */
        new_buff->dma = old_buff->dma;
        new_buff->page = old_buff->page;
        new_buff->page_offset = old_buff->page_offset;
        new_buff->pagecnt_bias = old_buff->pagecnt_bias;
}
#endif

static inline bool rnpvf_page_is_reserved(struct page *page)
{
        return (page_to_nid(page) != numa_mem_id()) || page_is_pfmemalloc(page);
}

#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
static bool rnpvf_can_reuse_rx_page(struct rnpvf_rx_buffer *rx_buffer)
{

        unsigned int pagecnt_bias = rx_buffer->pagecnt_bias;
        struct page *page = rx_buffer->page;

#ifdef OPTM_WITH_LPAGE
        return false;
#endif
        /* avoid re-using remote pages */
        if (unlikely(rnpvf_page_is_reserved(page)))
                return false;

#if (PAGE_SIZE < 8192)
        /* if we are only owner of page we can reuse it */
#ifdef HAVE_PAGE_COUNT_BULK_UPDATE
        if (unlikely((page_ref_count(page) - pagecnt_bias) > 1))
#else
        if (unlikely((page_count(page) - pagecnt_bias) > 1))
#endif
                return false;
#else

        /*
         * The last offset is a bit aggressive in that we assume the
         * worst case of FCoE being enabled and using a 3K buffer.
         * However this should have minimal impact as the 1K extra is
         * still less than one buffer in size.
         */
#define RNPVF_LAST_OFFSET (SKB_WITH_OVERHEAD(PAGE_SIZE) - RNPVF_RXBUFFER_2K)
        if (rx_buffer->page_offset > RNPVF_LAST_OFFSET)
                return false;
#endif


#ifdef HAVE_PAGE_COUNT_BULK_UPDATE
        /* If we have drained the page fragment pool we need to update
         * the pagecnt_bias and page count so that we fully restock the
         * number of references the driver holds.
         */
        if (unlikely(pagecnt_bias == 1)) {
                page_ref_add(page, USHRT_MAX - 1);
                rx_buffer->pagecnt_bias = USHRT_MAX;
        }
#else
        /*
         * Even if we own the page, we are not allowed to use atomic_set()
         * This would break get_page_unless_zero() users.
         */
        if (likely(!pagecnt_bias)) {
                page_ref_inc(page);
                rx_buffer->pagecnt_bias = 1;
        }
#endif

        return true;
}
#endif

#if (PAGE_SIZE < 8192)
#define RNPVF_MAX_2K_FRAME_BUILD_SKB (RNPVF_RXBUFFER_1536 - NET_IP_ALIGN)
#define RNPVF_2K_TOO_SMALL_WITH_PADDING                                          \
        ((NET_SKB_PAD + RNPVF_RXBUFFER_1536) > SKB_WITH_OVERHEAD(RNPVF_RXBUFFER_2K))

static inline int rnpvf_compute_pad(int rx_buf_len)
{
        int page_size, pad_size;

        page_size = ALIGN(rx_buf_len, PAGE_SIZE / 2);
        pad_size = SKB_WITH_OVERHEAD(page_size) - rx_buf_len;

        return pad_size;
}

static inline int rnpvf_skb_pad(void)
{
        int rx_buf_len;

        /* If a 2K buffer cannot handle a standard Ethernet frame then
         * optimize padding for a 3K buffer instead of a 1.5K buffer.
         *
         * For a 3K buffer we need to add enough padding to allow for
         * tailroom due to NET_IP_ALIGN possibly shifting us out of
         * cache-line alignment.
         */
        if (RNPVF_2K_TOO_SMALL_WITH_PADDING)
                rx_buf_len = RNPVF_RXBUFFER_3K + SKB_DATA_ALIGN(NET_IP_ALIGN);
        else
                rx_buf_len = RNPVF_RXBUFFER_1536;

        /* if needed make room for NET_IP_ALIGN */
        rx_buf_len -= NET_IP_ALIGN;
        return rnpvf_compute_pad(rx_buf_len);
}

#define RNPVF_SKB_PAD rnpvf_skb_pad()
#else /* PAGE_SIZE < 8192 */
#define RNPVF_SKB_PAD (NET_SKB_PAD + NET_IP_ALIGN)
#endif

/**
 * rnp_clean_rx_ring - Free Rx Buffers per Queue
 * @rx_ring: ring to free buffers from
 **/
static void rnpvf_clean_rx_ring(struct rnpvf_ring *rx_ring)
{
        u16 i = rx_ring->next_to_clean;
        struct rnpvf_rx_buffer *rx_buffer = &rx_ring->rx_buffer_info[i];
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
        DEFINE_DMA_ATTRS(attrs);
        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);
#endif

/*
#ifdef HAVE_AF_XDP_ZC_SUPPORT
        if (rx_ring->xsk_umem) {
                rnp_xsk_clean_rx_ring(rx_ring);
                goto skip_free;
        }

#endif
*/
        /* Free all the Rx ring sk_buffs */
#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT
        while (i != rx_ring->next_to_use) {
#else
        while (i != rx_ring->next_to_alloc) {
#endif
                if (rx_buffer->skb) {
                        struct sk_buff *skb = rx_buffer->skb;
//#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
//                        /* no need this */
//                        if (RNP_CB(skb)->page_released)
//                                dma_unmap_page_attrs(rx_ring->dev,
//                                                     RNP_CB(skb)->dma,
//                                                     rnp_rx_pg_size(rx_ring),
//                                                     DMA_FROM_DEVICE,
//#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
//                                                     &attrs);
//#else
//                                                     RNP_RX_DMA_ATTR);
//#endif
//#else
//                        /* We need to clean up RSC frag lists */
//                        skb = rnp_merge_active_tail(skb);
//                        if (rnp_close_active_frag_list(skb))
//                                dma_unmap_single(rx_ring->dev,
//                                                 RNP_CB(skb)->dma,
//                                                 rx_ring->rx_buf_len,
//                                                 DMA_FROM_DEVICE);
//                        RNP_CB(skb)->dma = 0;
//#endif /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
                        dev_kfree_skb(skb);
                        rx_buffer->skb = NULL;
                }

#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
                /* Invalidate cache lines that may have been written to by
                 * device so that we avoid corrupting memory.
                 */
                dma_sync_single_range_for_cpu(rx_ring->dev,
                                              rx_buffer->dma,
                                              rx_buffer->page_offset,
                                              rnpvf_rx_bufsz(rx_ring),
                                              DMA_FROM_DEVICE);

                /* free resources associated with mapping */
                dma_unmap_page_attrs(rx_ring->dev, rx_buffer->dma,
                                     rnpvf_rx_pg_size(rx_ring),
                                     DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                     &attrs);
#else
                                     RNPVF_RX_DMA_ATTR);
#endif


                __page_frag_cache_drain(rx_buffer->page,
                                        rx_buffer->pagecnt_bias);
#else /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
                if (rx_buffer->dma) {
                        dma_unmap_single(rx_ring->dev,
                                         rx_buffer->dma,
                                         rx_ring->rx_buf_len,
                                         DMA_FROM_DEVICE);
                        rx_buffer->dma = 0;
                }
#endif /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
                /* now this page is not used */
                rx_buffer->page = NULL;
                i++;
                rx_buffer++;
                if (i == rx_ring->count) {
                        i = 0;
                        rx_buffer = rx_ring->rx_buffer_info;
                }
        }

#ifdef HAVE_AF_XDP_ZC_SUPPORT
//skip_free:
#endif
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
        rx_ring->next_to_alloc = 0;
        rx_ring->next_to_clean = 0;
        rx_ring->next_to_use = 0;
#endif

}


#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
static inline unsigned int rnpvf_rx_offset(struct rnpvf_ring *rx_ring)
{
        return ring_uses_build_skb(rx_ring) ? RNPVF_SKB_PAD : 0;
}

#ifdef OPTM_WITH_LPAGE
static bool rnpvf_alloc_mapped_page(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *bi, union rnp_rx_desc *rx_desc,
                u16 bufsz, u64 fun_id)
{
        struct page *page = bi->page;
        dma_addr_t dma;
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);
#endif

        /* since we are recycling buffers we should seldom need to alloc */
        if (likely(page))
                return true;

        page = dev_alloc_pages(RNPVF_ALLOC_PAGE_ORDER);
        //page = dev_alloc_pages(rnp_rx_pg_order(rx_ring));
        if (unlikely(!page)) {
                rx_ring->rx_stats.alloc_rx_page_failed++;
                return false;
        }

        bi->page_offset = rnpvf_rx_offset(rx_ring);

        /* map page for use */
        dma = dma_map_page_attrs(rx_ring->dev, page,
                                 bi->page_offset,
                                 bufsz,
                                 DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                 &attrs);
#else
                                 RNPVF_RX_DMA_ATTR);
#endif

        /*
         * if mapping failed free memory back to system since
         * there isn't much point in holding memory we can't use
         */
        if (dma_mapping_error(rx_ring->dev, dma)) {
                //__free_pages(page, rnp_rx_pg_order(rx_ring));
                __free_pages(page, RNPVF_ALLOC_PAGE_ORDER);
                printk("map failed\n");

                rx_ring->rx_stats.alloc_rx_page_failed++;
                return false;
        }
        bi->dma = dma;
        bi->page = page;
        //bi->page_offset = rnp_rx_offset(rx_ring);
        bi->page_offset = rnpvf_rx_offset(rx_ring);
//#ifdef HAVE_PAGE_COUNT_BULK_UPDATE
        page_ref_add(page, USHRT_MAX - 1);
        bi->pagecnt_bias = USHRT_MAX;
        //printk("page ref_count is %x\n", page_ref_count(page));
//#else
//      bi->pagecnt_bias = 1;
//#endif
        rx_ring->rx_stats.alloc_rx_page++;

        /* sync the buffer for use by the device */
        dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
                        0, bufsz,
                        DMA_FROM_DEVICE);

        /*
        * Refresh the desc even if buffer_addrs didn't change
        * because each write-back erases this info.
        */
        //printk("first dma %llx\n", bi->dma);
        rx_desc->pkt_addr = cpu_to_le64(bi->dma + fun_id);


        return true;
}

static void rnpvf_put_rx_buffer(struct rnpvf_ring *rx_ring,
                              struct rnpvf_rx_buffer *rx_buffer)
{
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);

#endif
        if (rnpvf_can_reuse_rx_page(rx_buffer)) {
                /* hand second half of page back to the ring */
                rnpvf_reuse_rx_page(rx_ring, rx_buffer);
        } else {
                /* we are not reusing the buffer so unmap it */
                dma_unmap_page_attrs(rx_ring->dev, rx_buffer->dma,
                                     rnpvf_rx_bufsz(rx_ring),
                                     DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                     &attrs);
#else
                                     RNPVF_RX_DMA_ATTR);
#endif
                // maybe error ?
                // printk("free this page %d\n", rx_buffer->pagecnt_bias);
                __page_frag_cache_drain(rx_buffer->page,
                                        rx_buffer->pagecnt_bias);
        }

        /* clear contents of rx_buffer */
        rx_buffer->page = NULL;
        //rx_buffer->skb = NULL;
}


/**
 * rnpvf_alloc_rx_buffers - Replace used receive buffers
 * @rx_ring: ring to place buffers on
 * @cleaned_count: number of buffers to replace
 **/
void rnpvf_alloc_rx_buffers(struct rnpvf_ring *rx_ring, u16 cleaned_count)
{
        union rnp_rx_desc *rx_desc;
        struct rnpvf_rx_buffer *bi;
        u16 i = rx_ring->next_to_use;
        u64 fun_id = ((u64)(rx_ring->vfnum) << (32 + 24));
        u16 bufsz;
        /* nothing to do */
        if (!cleaned_count)
                return;

        rx_desc = RNPVF_RX_DESC(rx_ring, i);

        BUG_ON(rx_desc == NULL);

        bi = &rx_ring->rx_buffer_info[i];

        BUG_ON(bi == NULL);

        //printk("start from %d\n", i);
        i -= rx_ring->count;
        bufsz = rnpvf_rx_bufsz(rx_ring);

        do {

                int count = 1;
                struct page *page;

                // alloc page and init first rx_desc
                if (!rnpvf_alloc_mapped_page(rx_ring, bi, rx_desc, bufsz, fun_id))
                        break;
                page = bi->page;

                rx_desc->cmd = 0;

                rx_desc++;
                i++;
                bi++;

                if (unlikely(!i)) {
                        rx_desc = RNPVF_RX_DESC(rx_ring, 0);
                        bi = rx_ring->rx_buffer_info;
                        i -= rx_ring->count;
                }

                rx_desc->cmd = 0;

                cleaned_count--;

                while (count < rx_ring->rx_page_buf_nums && cleaned_count) {

                        //dma_addr_t dma = bi->dma;
                        dma_addr_t dma;

#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                        DEFINE_DMA_ATTRS(attrs);

                        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
                        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);
#endif

                        bi->page_offset = rx_ring->rx_per_buf_mem * count + rnpvf_rx_offset(rx_ring);
                        /* map page for use */
                        dma = dma_map_page_attrs(rx_ring->dev, page, bi->page_offset,
                                                bufsz,
                                                DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                                &attrs);
#else


                                                RNPVF_RX_DMA_ATTR);
#endif

                        if (dma_mapping_error(rx_ring->dev, dma)) {
                                printk("map second error\n");
                                rx_ring->rx_stats.alloc_rx_page_failed++;
                                break;
                        }
                        //printk("%d dma is %llx\n", i + rx_ring->count, dma);

                        bi->dma = dma;
                        bi->page = page;

                        page_ref_add(page, USHRT_MAX);
                        bi->pagecnt_bias = USHRT_MAX;

                        /* sync the buffer for use by the device */
                        dma_sync_single_range_for_device(rx_ring->dev, bi->dma,
                                                         0, bufsz,
                                                         DMA_FROM_DEVICE);

                        /*
                         * Refresh the desc even if buffer_addrs didn't change
                         * because each write-back erases this info.
                         */
                        //printk("second %d dma %llx\n", count, bi->dma);
                        rx_desc->pkt_addr =
                                cpu_to_le64(bi->dma + fun_id);
                        //cpu_to_le64(bi->dma + bi->page_offset + fun_id);
                        //printk("rx_desc is %llx\n", rx_desc->pkt_addr);
                        //printk("%d rx_desc page_offset %x\n", i, bi->page_offset);
                        /* clean dd */
                        rx_desc->cmd = 0;

                        rx_desc++;
                        bi++;
                        i++;
                        if (unlikely(!i)) {
                                rx_desc = RNPVF_RX_DESC(rx_ring, 0);
                                bi = rx_ring->rx_buffer_info;
                                i -= rx_ring->count;
                        }
                        count++;
                        /* clear the hdr_addr for the next_to_use descriptor */
                        // rx_desc->cmd = 0;
                        cleaned_count--;
                }
        } while (cleaned_count);

        i += rx_ring->count;

        if (rx_ring->next_to_use != i)
                rnpvf_update_rx_tail(rx_ring, i);
}

#else


static bool rnpvf_alloc_mapped_page(struct rnpvf_ring *rx_ring,
                                  struct rnpvf_rx_buffer *bi)
{
        struct page *page = bi->page;
        dma_addr_t dma;
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);
#endif

        /* since we are recycling buffers we should seldom need to alloc */
        if (likely(page))
                return true;

        page = dev_alloc_pages(rnpvf_rx_pg_order(rx_ring));
        if (unlikely(!page)) {
                rx_ring->rx_stats.alloc_rx_page_failed++;
                return false;
        }

        /* map page for use */
        dma = dma_map_page_attrs(rx_ring->dev, page, 0, rnpvf_rx_pg_size(rx_ring),
                                 DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                 &attrs);
#else
                                 RNPVF_RX_DMA_ATTR);
#endif

        /*
         * if mapping failed free memory back to system since
         * there isn't much point in holding memory we can't use
         */
        if (dma_mapping_error(rx_ring->dev, dma)) {
                __free_pages(page, rnpvf_rx_pg_order(rx_ring));
                printk("map failed\n");

                rx_ring->rx_stats.alloc_rx_page_failed++;
                return false;
        }
        bi->dma = dma;
        bi->page = page;
        bi->page_offset = rnpvf_rx_offset(rx_ring);
#ifdef HAVE_PAGE_COUNT_BULK_UPDATE
        page_ref_add(page, USHRT_MAX - 1);
        bi->pagecnt_bias = USHRT_MAX;
        //printk("page ref_count is %x\n", page_ref_count(page));
#else
        bi->pagecnt_bias = 1;
#endif
        rx_ring->rx_stats.alloc_rx_page++;

        return true;
}

static void rnpvf_put_rx_buffer(struct rnpvf_ring *rx_ring,
                              struct rnpvf_rx_buffer *rx_buffer,
                              struct sk_buff *skb)
{
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
        dma_set_attr(DMA_ATTR_WEAK_ORDERING, &attrs);

#endif
        if (rnpvf_can_reuse_rx_page(rx_buffer)) {
                /* hand second half of page back to the ring */
                rnpvf_reuse_rx_page(rx_ring, rx_buffer);
        } else {
                /* we are not reusing the buffer so unmap it */
                dma_unmap_page_attrs(rx_ring->dev, rx_buffer->dma,
                                     rnpvf_rx_pg_size(rx_ring), DMA_FROM_DEVICE,
#if defined(HAVE_STRUCT_DMA_ATTRS) && defined(HAVE_SWIOTLB_SKIP_CPU_SYNC)
                                     &attrs);
#else
                                     RNPVF_RX_DMA_ATTR);
#endif
                __page_frag_cache_drain(rx_buffer->page,
                                        rx_buffer->pagecnt_bias);
        }

        /* clear contents of rx_buffer */
        rx_buffer->page = NULL;
        rx_buffer->skb = NULL;
}


#endif /* OPTM_WITH_LPAGE */
#endif

/* drop this packets if error */
static bool rnpvf_check_csum_error(struct rnpvf_ring *rx_ring,
                                union rnp_rx_desc *rx_desc,
                                unsigned int size,
                                unsigned int *driver_drop_packets)
{
        bool err = false;

        struct net_device *netdev = rx_ring->netdev;
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

        if ((netdev->features & NETIF_F_RXCSUM) &&
		(!(adapter->priv_flags & RNPVF_PRIV_FLAG_FCS_ON))) {
                if (unlikely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_ERR_MASK))) {
                        rx_debug_printk("rx error: VEB:%s mark:0x%x cmd:0x%x\n",
                                        (rx_ring->q_vector->adapter->flags &
                                         RNP_FLAG_SRIOV_ENABLED) ?
                                                "On" :
                                                "Off",
                                        rx_desc->wb.mark, rx_desc->wb.cmd);
                        /* push this packet to stack if in promisc mode */
                        rx_ring->rx_stats.csum_err++;

                        if ((!(netdev->flags & IFF_PROMISC) &&
                             (!(netdev->features & NETIF_F_RXALL)))) {
				if (rx_ring->ring_flags & RNPVF_RING_CHKSM_FIX) {
					err = true;
					goto skip_fix;
				}
				// if not ipv4 with l4 error, we should ignore l4 csum error
				if (unlikely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_L4_MASK)
							&& (!(rx_desc->wb.rev1 & RNP_RX_L3_TYPE_MASK)))) {
					rx_ring->rx_stats.csum_err--;
					goto skip_fix;
				}


                                if (unlikely(rnpvf_test_staterr(
                                            rx_desc, RNP_RXD_STAT_SCTP_MASK))) {
					if (size > 60) {
						err = true;
						//      return true;
					} else {
						/* sctp less than 60 hw report err by mistake */
						rx_ring->rx_stats.csum_err--;
					}
                                } else {
                                        err = true;
                                }
                        }
                }
        }

skip_fix:
        if (err) {
                u32 ntc = rx_ring->next_to_clean + 1;
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
                struct rnpvf_rx_buffer *rx_buffer;
#if (PAGE_SIZE < 8192)
                unsigned int truesize = rnpvf_rx_pg_size(rx_ring) / 2;
#else
                unsigned int truesize = ring_uses_build_skb(rx_ring) ?
                        SKB_DATA_ALIGN(RNPVF_SKB_PAD + size) :
                        SKB_DATA_ALIGN(size);
#endif

                // if eop add drop_packets
                if (likely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_EOP)))
                        *driver_drop_packets = *driver_drop_packets + 1;

                /* we are reusing so sync this buffer for CPU use */
                rx_buffer = &rx_ring->rx_buffer_info[rx_ring->next_to_clean];
                dma_sync_single_range_for_cpu(rx_ring->dev, rx_buffer->dma,
                                rx_buffer->page_offset, size,
                                DMA_FROM_DEVICE);

                // no-need minis ,we don't send to os stack
                //rx_buffer->pagecnt_bias--;

#if (PAGE_SIZE < 8192)
                rx_buffer->page_offset ^= truesize;
#else
                rx_buffer->page_offset += truesize;
#endif
                // fix me
#ifdef OPTM_WITH_LPAGE
                rnpvf_put_rx_buffer(rx_ring, rx_buffer);
#else
                rnpvf_put_rx_buffer(rx_ring, rx_buffer, NULL);
#endif
#endif
                // update to the next desc
                ntc = (ntc < rx_ring->count) ? ntc : 0;
                rx_ring->next_to_clean = ntc;

        }
        return err;
}

/**
 * rnpvf_process_skb_fields - Populate skb header fields from Rx descriptor
 * @rx_ring: rx descriptor ring packet is being transacted on
 * @rx_desc: pointer to the EOP Rx descriptor
 * @skb: pointer to current skb being populated
 *
 * This function checks the ring, descriptor, and packet information in
 * order to populate the hash, checksum, VLAN, timestamp, protocol, and
 * other fields within the skb.
 **/
static void rnpvf_process_skb_fields(struct rnpvf_ring *rx_ring,
		union rnp_rx_desc *rx_desc,
		struct sk_buff *skb)
{
	struct net_device *dev = rx_ring->netdev;

	//rnpvf_update_rsc_stats(rx_ring, skb);

	rnpvf_rx_hash(rx_ring, rx_desc, skb);

	rnpvf_rx_checksum(rx_ring, rx_desc, skb);

	/* remove vlan if pf set a vlan */
#ifdef NETIF_F_HW_VLAN_CTAG_RX	
	if (((dev->features & NETIF_F_HW_VLAN_CTAG_RX)
#ifdef NETIF_F_HW_VLAN_STAG_RX
		|| (dev->features & NETIF_F_HW_VLAN_STAG_RX)) &&
#else
	) &&
#endif
#else
	if ((dev->features & NETIF_F_HW_VLAN_RX) &&
#endif
		rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_VLAN_VALID) &&
		!(cpu_to_le16(rx_desc->wb.rev1) & VEB_VF_IGNORE_VLAN)) {
		u16 vid = le16_to_cpu(rx_desc->wb.vlan);
		// check vlan type
		if (rx_ring->ring_flags & RNPVF_RING_STAGS_SUPPORT) {
			if (rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_STAG)) {
				__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021AD), vid);

			} else {
				__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vid);
			}
		} else {
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vid);
		}
		// should put innver vlan in if has outer vlan
		// skb = __vlan_hwaccel_push_inside(skb);
		// vid = le16_to_cpu(rx_desc->wb.mark); 
		// __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021AD), vid);

		rx_ring->rx_stats.vlan_remove++;
		}

	skb_record_rx_queue(skb, rx_ring->queue_index);

	skb->protocol = eth_type_trans(skb, dev);
}

static void rnpvf_rx_skb(struct rnpvf_q_vector *q_vector, struct sk_buff *skb)
{
	struct rnpvf_adapter *adapter = q_vector->adapter;

	if (!(adapter->flags & RNPVF_FLAG_IN_NETPOLL))
		napi_gro_receive(&q_vector->napi, skb);
	else
		netif_rx(skb);
}

#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT

#ifdef FIX_VEB_BUG

static bool rnpvf_check_src_mac(struct sk_buff *skb, struct net_device *netdev)
{
	char *data = (char *) skb->data;
	bool ret = false;
	struct netdev_hw_addr *ha;

	if (is_multicast_ether_addr(data)) {
		if (0 == memcmp(data + netdev->addr_len, netdev->dev_addr, netdev->addr_len)) {
			dev_kfree_skb_any(skb);
			ret = true;
		}
		// if src mac equal own mac
		netdev_for_each_uc_addr(ha, netdev) {
			if (0 == memcmp(data + netdev->addr_len, ha->addr, netdev->addr_len)) {
				dev_kfree_skb_any(skb);
				//printk("drop own packets\n");
				ret = true;
			}
		}
	}
	return ret;
}

#endif
/**
 * rnpvf_get_headlen - determine size of header for RSC/LRO/GRO/FCOE
 * @data: pointer to the start of the headers
 * @max_len: total length of section to find headers in
 *
 * This function is meant to determine the length of headers that will
 * be recognized by hardware for LRO, GRO, and RSC offloads.  The main
 * motivation of doing this is to only perform one pull for IPv4 TCP
 * packets so that we can do basic things like calculating the gso_size
 * based on the average data per packet.
 **/
static unsigned int rnpvf_get_headlen(unsigned char *data, unsigned int max_len)
{
	union {
		unsigned char *network;
		/* l2 headers */
		struct ethhdr *eth;
		struct vlan_hdr *vlan;
		/* l3 headers */
		struct iphdr *ipv4;
		struct ipv6hdr *ipv6;
	} hdr;
	__be16 protocol;
	u8 nexthdr = 0; /* default to not TCP */
	u8 hlen;

	/* this should never happen, but better safe than sorry */
	if (max_len < ETH_HLEN)
		return max_len;

	/* initialize network frame pointer */
	hdr.network = data;

	/* set first protocol and move network header forward */
	protocol = hdr.eth->h_proto;
	hdr.network += ETH_HLEN;

	/* handle any vlan tag if present */
	if (protocol == htons(ETH_P_8021Q)) {
		if ((hdr.network - data) > (max_len - VLAN_HLEN))
			return max_len;

		protocol = hdr.vlan->h_vlan_encapsulated_proto;
		hdr.network += VLAN_HLEN;
	}

	/* handle L3 protocols */
	if (protocol == htons(ETH_P_IP)) {
		if ((hdr.network - data) > (max_len - sizeof(struct iphdr)))
			return max_len;

		/* access ihl as a u8 to avoid unaligned access on ia64 */
		hlen = (hdr.network[0] & 0x0F) << 2;

		/* verify hlen meets minimum size requirements */
		if (hlen < sizeof(struct iphdr))
			return hdr.network - data;

		/* record next protocol if header is present */
		if (!(hdr.ipv4->frag_off & htons(IP_OFFSET)))
			nexthdr = hdr.ipv4->protocol;
	} else if (protocol == htons(ETH_P_IPV6)) {
		if ((hdr.network - data) > (max_len - sizeof(struct ipv6hdr)))
			return max_len;

		/* record next protocol */
		nexthdr = hdr.ipv6->nexthdr;
		hlen = sizeof(struct ipv6hdr);
	} else {
		return hdr.network - data;
	}

	/* relocate pointer to start of L4 header */
	hdr.network += hlen;

	/* finally sort out TCP/UDP */
	if (nexthdr == IPPROTO_TCP) {
		if ((hdr.network - data) > (max_len - sizeof(struct tcphdr)))
			return max_len;

		/* access doff as a u8 to avoid unaligned access on ia64 */
		hlen = (hdr.network[12] & 0xF0) >> 2;

		/* verify hlen meets minimum size requirements */
		if (hlen < sizeof(struct tcphdr))
			return hdr.network - data;

		hdr.network += hlen;
	} else if (nexthdr == IPPROTO_UDP) {
		if ((hdr.network - data) > (max_len - sizeof(struct udphdr)))
			return max_len;

		hdr.network += sizeof(struct udphdr);
	}

	/*
	 * If everything has gone correctly hdr.network should be the
	 * data section of the packet and will be the end of the header.
	 * If not then it probably represents the end of the last recognized
	 * header.
	 */
	if ((hdr.network - data) < max_len)
		return hdr.network - data;
	else
		return max_len;
}
/**
 * rnpvf_pull_tail - rnp specific version of skb_pull_tail
 * @rx_ring: rx descriptor ring packet is being transacted on
 * @skb: pointer to current skb being adjusted
 *
 * This function is an rnp specific version of __pskb_pull_tail.  The
 * main difference between this version and the original function is that
 * this function can make several assumptions about the state of things
 * that allow for significant optimizations versus the standard function.
 * As a result we can do things like drop a frag and maintain an accurate
 * truesize for the skb.
 */
static void rnpvf_pull_tail(struct sk_buff *skb)
{
	// struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[0];
	skb_frag_t *frag = &skb_shinfo(skb)->frags[0];
	unsigned char *va;
	unsigned int pull_len;

	/*
	 * it is valid to use page_address instead of kmap since we are
	 * working with pages allocated out of the lomem pool per
	 * alloc_page(GFP_ATOMIC)
	 */
	va = skb_frag_address(frag);

	/*
	 * we need the header to contain the greater of either ETH_HLEN or
	 * 60 bytes if the skb->len is less than 60 for skb_pad.
	 */
	pull_len = rnpvf_get_headlen(va, RNPVF_RX_HDR_SIZE);

	/* align pull length to size of long to optimize memcpy performance */
	skb_copy_to_linear_data(skb, va, ALIGN(pull_len, sizeof(long)));

	/* update all of the pointers */
	skb_frag_size_sub(frag, pull_len);
	skb_frag_off_add(frag, pull_len);
	// frag->page_offset += pull_len;
	skb->data_len -= pull_len;
	skb->tail += pull_len;
}
/**
 * rnpvf_cleanup_headers - Correct corrupted or empty headers
 * @rx_ring: rx descriptor ring packet is being transacted on
 * @rx_desc: pointer to the EOP Rx descriptor
 * @skb: pointer to current skb being fixed
 *
 * Check for corrupted packet headers caused by senders on the local L2
 * embedded NIC switch not setting up their Tx Descriptors right.  These
 * should be very rare.
 *
 * Also address the case where we are pulling data in on pages only
 * and as such no data is present in the skb header.
 *
 * In addition if skb is not at least 60 bytes we need to pad it so that
 * it is large enough to qualify as a valid Ethernet frame.
 *
 * Returns true if an error was encountered and skb was freed.
 **/
static bool rnpvf_cleanup_headers(struct rnpvf_ring *rx_ring,
		union rnp_rx_desc *rx_desc,
		struct sk_buff *skb)
{
#ifdef OPTM_WITH_LPAGE
#else
        /* XDP packets use error pointer so abort at this point */
        if (IS_ERR(skb))
                return true;
#endif

	//struct net_device *netdev = rx_ring->netdev;
	//struct rnpvf_adapter *adapter = netdev_priv(netdev);

	/* verify that the packet does not have any known errors */
	/* do not do this in rx all mode or mac in fcs mode */
//	if ((netdev->features & NETIF_F_RXCSUM) &&
//		(!(adapter->priv_flags & RNPVF_PRIV_FLAG_FCS_ON))) {
//		if (unlikely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_ERR_MASK)
//						 )) {
//			rx_debug_printk("csum err\n");
//			rx_ring->rx_stats.csum_err++;
//			if ((!(netdev->flags & IFF_PROMISC)) &&
//				(!(netdev->features & NETIF_F_RXALL))) {
//                                /* sctp less than 60 hw report err by mistake */
//                                if (unlikely(rnpvf_test_staterr(
//                                            rx_desc, RNP_RXD_STAT_SCTP_MASK))) {
//                                        if (skb->len > 60) {
//                                                dev_kfree_skb_any(skb);
//                                                return true;
//                                        }
//                                        rx_ring->rx_stats.csum_err--;
//                                } else {
//                                        dev_kfree_skb_any(skb);
//                                        return true;
//                                }
//			}
//		}
//	}

	/* place header in linear portion of buffer */
	//if (skb_is_nonlinear(skb))
	if (!skb_headlen(skb))
		rnpvf_pull_tail(skb);

	if (eth_skb_pad(skb))
		return true;
	/* if skb_pad returns an error the skb was freed */
	/*
	if (unlikely(skb->len < 60)) {
		int pad_len = 60 - skb->len;

		if (skb_pad(skb, pad_len))
			return true;
		__skb_put(skb, pad_len);
	}*/
#ifdef FIX_VEB_BUG
	if (!(rx_ring->ring_flags & RNPVF_RING_VEB_MULTI_FIX))
		return rnpvf_check_src_mac(skb, rx_ring->netdev);
	else
		return false;
#endif

	return false;
}
/**
 * rnpvf_add_rx_frag - Add contents of Rx buffer to sk_buff
 * @rx_ring: rx descriptor ring to transact packets on
 * @rx_buffer: buffer containing page to add
 * @skb: sk_buff to place the data into
 * @size: size of data
 *
 * This function will add the data contained in rx_buffer->page to the skb.
 * This is done either through a direct copy if the data in the buffer is
 * less than the skb header size, otherwise it will just attach the page as
 * a frag to the skb.
 *
 * The function will then update the page offset if necessary and return
 * true if the buffer can be reused by the adapter.
 **/
static void rnpvf_add_rx_frag(struct rnpvf_ring *rx_ring,
                            struct rnpvf_rx_buffer *rx_buffer,
                            struct sk_buff *skb, unsigned int size)
{
#if (PAGE_SIZE < 8192)
        unsigned int truesize = rnpvf_rx_pg_size(rx_ring) / 2;
#else
        unsigned int truesize = ring_uses_build_skb(rx_ring) ?
                                        SKB_DATA_ALIGN(RNPVF_SKB_PAD + size) :
                                        SKB_DATA_ALIGN(size);
#endif

        skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, rx_buffer->page,
                        rx_buffer->page_offset, size, truesize);

#if (PAGE_SIZE < 8192)
        rx_buffer->page_offset ^= truesize;
#else
        rx_buffer->page_offset += truesize;
#endif
}
#endif

#ifdef OPTM_WITH_LPAGE
#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC
static struct sk_buff *rnpvf_build_skb(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *rx_buffer,
                union rnp_rx_desc *rx_desc,
                unsigned int size)
{
        void *va = page_address(rx_buffer->page) + rx_buffer->page_offset;
        unsigned int truesize =
                SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) +
                SKB_DATA_ALIGN(size + RNPVF_SKB_PAD);
        struct sk_buff *skb;

        /* prefetch first cache line of first page */
        prefetch(va);
#if L1_CACHE_BYTES < 128
        prefetch(va + L1_CACHE_BYTES);
#endif

        /* build an skb around the page buffer */
        skb = build_skb(va - RNPVF_SKB_PAD, truesize);
        if (unlikely(!skb))
                return NULL;

        /* update pointers within the skb to store the data */
        skb_reserve(skb, RNPVF_SKB_PAD);
        __skb_put(skb, size);
        /* record DMA address if this is the start of a
         * chain of buffers
         */
        /* if (!rnp_test_staterr(rx_desc, RNP_RXD_STAT_EOP))
         * RNP_CB(skb)->dma = rx_buffer->dma;
         */
        //check_udp_chksum((void *)skb->data, rx_buffer);
        /* update buffer offset */
        // no need this , we not use this page again
        //rx_buffer->page_offset += truesize;

        return skb;
}

#endif /* HAVE_SWIOTLB_SKIP_CPU_SYNC */

static struct rnpvf_rx_buffer *rnpvf_get_rx_buffer(struct rnpvf_ring *rx_ring,
                                               union rnp_rx_desc *rx_desc,
                                               const unsigned int size)
{
        struct rnpvf_rx_buffer *rx_buffer;

        rx_buffer = &rx_ring->rx_buffer_info[rx_ring->next_to_clean];
        prefetchw(rx_buffer->page);

        rx_buf_dump("rx buf",
                    page_address(rx_buffer->page) + rx_buffer->page_offset,
                    rx_desc->wb.len);

        /* we are reusing so sync this buffer for CPU use */
        dma_sync_single_range_for_cpu(rx_ring->dev, rx_buffer->dma,
                                      0, size,
                                      DMA_FROM_DEVICE);
        /* skip_sync: */
        rx_buffer->pagecnt_bias--;

        return rx_buffer;
}

/**
 * rnpvf_is_non_eop - process handling of non-EOP buffers
 * @rx_ring: Rx ring being processed
 * @rx_desc: Rx descriptor for current buffer
 * @skb: Current socket buffer containing buffer in progress
 *
 * This function updates next to clean.  If the buffer is an EOP buffer
 * this function exits returning false, otherwise it will place the
 * sk_buff in the next buffer to be chained and return true indicating
 * that this is in fact a non-EOP buffer.
 **/
static bool rnpvf_is_non_eop(struct rnpvf_ring *rx_ring, union rnp_rx_desc *rx_desc)
{
        u32 ntc = rx_ring->next_to_clean + 1;
        /* fetch, update, and store next to clean */
        ntc = (ntc < rx_ring->count) ? ntc : 0;
        rx_ring->next_to_clean = ntc;

        prefetch(RNPVF_RX_DESC(rx_ring, ntc));

        /* if we are the last buffer then there is nothing else to do */
        if (likely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_EOP)))
                return false;
        /* place skb in next buffer to be received */

        return true;
}





static struct sk_buff *rnpvf_construct_skb(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *rx_buffer,
                union rnp_rx_desc *rx_desc,
                unsigned int size)
{
        void *va = page_address(rx_buffer->page) + rx_buffer->page_offset;
        unsigned int truesize =
                SKB_DATA_ALIGN(size);
        unsigned int headlen;
        struct sk_buff *skb;

        /* prefetch first cache line of first page */
        prefetch(va);
#if L1_CACHE_BYTES < 128
        prefetch(va + L1_CACHE_BYTES);
#endif
        /* Note, we get here by enabling legacy-rx via:
         *
         *    ethtool --set-priv-flags <dev> legacy-rx on
         *
         * In this mode, we currently get 0 extra XDP headroom as
         * opposed to having legacy-rx off, where we process XDP
         * packets going to stack via rnpvf_build_skb(). The latter
         * provides us currently with 192 bytes of headroom.
         *
         * For rnp_construct_skb() mode it means that the
         * xdp->data_meta will always point to xdp->data, since
         * the helper cannot expand the head. Should this ever
         * change in future for legacy-rx mode on, then lets also
         * add xdp->data_meta handling here.
         */

        /* allocate a skb to store the frags */
        skb = napi_alloc_skb(&rx_ring->q_vector->napi, RNPVF_RX_HDR_SIZE);
        if (unlikely(!skb))
                return NULL;

        prefetchw(skb->data);


        /* Determine available headroom for copy */
        headlen = size;
        if (headlen > RNPVF_RX_HDR_SIZE)
                headlen = rnpvf_get_headlen(va, RNPVF_RX_HDR_SIZE);
                //headlen = eth_get_headlen(skb->dev, va, RNP_RX_HDR_SIZE);

        /* align pull length to size of long to optimize memcpy performance */
        memcpy(__skb_put(skb, headlen), va, ALIGN(headlen, sizeof(long)));

        /* update all of the pointers */
        size -= headlen;

        if (size) {
                /*
                 * if (!rnp_test_staterr(rx_desc, RNP_RXD_STAT_EOP))
                 * RNP_CB(skb)->dma = rx_buffer->dma;
                 */

                skb_add_rx_frag(skb, 0, rx_buffer->page,
                                (va + headlen) - page_address(rx_buffer->page), size,
                                truesize);
                rx_buffer->page_offset += truesize;
        } else {
                //memcpy(__skb_put(skb, size), xdp->data,
                //              ALIGN(size, sizeof(long)));
                rx_buffer->pagecnt_bias++;
        }

        //printk("size is %d\n", size);
        return skb;
}


/**
 * rnp_clean_rx_irq - Clean completed descriptors from Rx ring - bounce buf
 * @q_vector: structure containing interrupt and ring information
 * @rx_ring: rx descriptor ring to transact packets on
 * @budget: Total limit on number of packets to process
 *
 * This function provides a "bounce buffer" approach to Rx interrupt
 * processing.  The advantage to this is that on systems that have
 * expensive overhead for IOMMU access this provides a means of avoiding
 * it by maintaining the mapping of the page to the syste.
 *
 * Returns amount of work completed.
 **/

static int rnpvf_clean_rx_irq(struct rnpvf_q_vector *q_vector,
                struct rnpvf_ring *rx_ring, int budget)
{
        unsigned int total_rx_bytes = 0, total_rx_packets = 0;
        unsigned int err_packets = 0;
        unsigned int driver_drop_packets = 0;
        struct sk_buff *skb = rx_ring->skb;
        struct rnpvf_adapter *adapter = q_vector->adapter;
        u16 cleaned_count = rnpvf_desc_unused(rx_ring);

        /*
         * #ifdef HAVE_XDP_BUFF_RXQ
         * xdp.rxq = &rx_ring->xdp_rxq;
         * #endif
         */
        while (likely(total_rx_packets < budget)) {
                union rnp_rx_desc *rx_desc;
                struct rnpvf_rx_buffer *rx_buffer;
                //struct sk_buff *skb;
                unsigned int size;

                /* return some buffers to hardware, one at a time is too slow */
                if (cleaned_count >= RNPVF_RX_BUFFER_WRITE) {
                        rnpvf_alloc_rx_buffers(rx_ring, cleaned_count);
                        cleaned_count = 0;
                }
                rx_desc = RNPVF_RX_DESC(rx_ring, rx_ring->next_to_clean);

                rx_buf_dump("rx-desc:", rx_desc, sizeof(*rx_desc));
                // buf_dump("rx-desc:", rx_desc, sizeof(*rx_desc));
                rx_debug_printk("  dd set: %s\n",
                                (rx_desc->wb.cmd & RNP_RXD_STAT_DD) ? "Yes" :
                                                                      "No");

                if (!rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_DD))
                        break;

                rx_debug_printk(
                        "queue:%d  rx-desc:%d has-data len:%d next_to_clean %d\n",
                        rx_ring->rnp_queue_idx, rx_ring->next_to_clean,
                        rx_desc->wb.len, rx_ring->next_to_clean);

                /* handle padding */
                if ((adapter->priv_flags & RNPVF_PRIV_FLAG_FT_PADDING) &&
                    (!(adapter->priv_flags & RNPVF_PRIV_FLAG_PADDING_DEBUG))) {
                        if (likely(rnpvf_test_staterr(rx_desc,
                                                    RNP_RXD_STAT_EOP))) {
                                size = le16_to_cpu(rx_desc->wb.len) -
                                       le16_to_cpu(rx_desc->wb.padding_len);
                        } else {
                                size = le16_to_cpu(rx_desc->wb.len);
                        }
                } else {
                        /* size should not zero */
                        size = le16_to_cpu(rx_desc->wb.len);
                }

                if (!size)
                        break;

                /*
                 * should check csum err
                 * maybe one packet use mutiple descs
                 * no problems hw set all csum_err in mutiple descs
                 * maybe BUG if the last sctp desc less than 60
                 */
                if (rnpvf_check_csum_error(rx_ring, rx_desc, size, &driver_drop_packets)) {
                        cleaned_count++;
                        err_packets++;
                        if (err_packets + total_rx_packets > budget)
                                break;
                        continue;
                }
                /* This memory barrier is needed to keep us from reading
                 * any other fields out of the rx_desc until we know the
                 * descriptor has been written back
                 */
                dma_rmb();

                rx_buffer = rnpvf_get_rx_buffer(rx_ring, rx_desc, size);

                if (skb) {
                        rnpvf_add_rx_frag(rx_ring, rx_buffer, skb, size);
#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC
                } else if (ring_uses_build_skb(rx_ring)) {
                        skb = rnpvf_build_skb(rx_ring, rx_buffer, rx_desc, size);
#endif
                } else {
                        skb = rnpvf_construct_skb(rx_ring, rx_buffer,
                                                rx_desc, size);
                }

                /* exit if we failed to retrieve a buffer */
                if (!skb) {
                        rx_ring->rx_stats.alloc_rx_buff_failed++;
                        rx_buffer->pagecnt_bias++;
                        break;
                }
                //if (module_enable_ptp && adapter->ptp_rx_en &&
                //   adapter->flags2 & RNP_FLAG2_PTP_ENABLED)
                //      rnp_ptp_get_rx_hwstamp(adapter, rx_desc, skb);

                rnpvf_put_rx_buffer(rx_ring, rx_buffer);
                cleaned_count++;

                /* place incomplete frames back on ring for completion */
                if (rnpvf_is_non_eop(rx_ring, rx_desc))
                        continue;

                /* verify the packet layout is correct */
                if (rnpvf_cleanup_headers(rx_ring, rx_desc, skb)) {
                        //skb = NULL;
                        skb = NULL;
                        continue;
                }

                /* probably a little skewed due to removing CRC */
                total_rx_bytes += skb->len;

                /* populate checksum, timestamp, VLAN, and protocol */
                rnpvf_process_skb_fields(rx_ring, rx_desc, skb);


                //rx_buf_dump("rx-data:", skb->data, skb->len);



                rnpvf_rx_skb(q_vector, skb);
                skb = NULL;

                /* update budget accounting */
                total_rx_packets++;
        }

        rx_ring->skb = skb;
        //if (xdp_xmit) {
        //struct rnp_ring *ring = adapter->xdp_ring[smp_processor_id()];
        //
        // /* Force memory writes to complete before letting h/w
        //  * know there are new descriptors to fetch.
        //  */
        //wmb();
        //writel(ring->next_to_use, ring->tail);
        //
        //xdp_do_flush_map();
        //}

        u64_stats_update_begin(&rx_ring->syncp);
        rx_ring->stats.packets += total_rx_packets;
        rx_ring->stats.bytes += total_rx_bytes;
        rx_ring->rx_stats.driver_drop_packets += driver_drop_packets;
        //rx_ring->rx_stats.rx_clean_count += total_rx_packets;
        //rx_ring->rx_stats.rx_clean_times++;
        //if (rx_ring->rx_stats.rx_clean_times > 10) {
        //        rx_ring->rx_stats.rx_clean_times = 0;
        //        rx_ring->rx_stats.rx_clean_count = 0;
        //}
        u64_stats_update_end(&rx_ring->syncp);
        q_vector->rx.total_packets += total_rx_packets;
        q_vector->rx.total_bytes += total_rx_bytes;

        //printk("clean rx irq %d\n", total_rx_packets);
        if (total_rx_packets >= budget)
                rx_ring->rx_stats.poll_again_count++;

        //if (cleaned_count)
                //rnp_alloc_rx_buffers(rx_ring, cleaned_count);

        return total_rx_packets;
}


#else


/**
 * rnpvf_is_non_eop - process handling of non-EOP buffers
 * @rx_ring: Rx ring being processed
 * @rx_desc: Rx descriptor for current buffer
 * @skb: Current socket buffer containing buffer in progress
 *
 * This function updates next to clean.  If the buffer is an EOP buffer
 * this function exits returning false, otherwise it will place the
 * sk_buff in the next buffer to be chained and return true indicating
 * that this is in fact a non-EOP buffer.
 **/
static bool rnpvf_is_non_eop(struct rnpvf_ring *rx_ring, union rnp_rx_desc *rx_desc,
                           struct sk_buff *skb)
{
        u32 ntc = rx_ring->next_to_clean + 1;
//#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT
//        struct sk_buff *next_skb;
//#endif
        /* fetch, update, and store next to clean */
        ntc = (ntc < rx_ring->count) ? ntc : 0;
        rx_ring->next_to_clean = ntc;

        prefetch(RNPVF_RX_DESC(rx_ring, ntc));

        /* if we are the last buffer then there is nothing else to do */
        if (likely(rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_EOP)))
                return false;
#ifdef CONFIG_RNP_RNP_DISABLE_PACKET_SPLIT
        //next_skb = rx_ring->rx_buffer_info[ntc].skb;

        //rnp_add_active_tail(skb, next_skb);
        //RNP_CB(next_skb)->head = skb;
        printk("error spilt detect in disable split mode\n");
#else
        /* place skb in next buffer to be received */
        rx_ring->rx_buffer_info[ntc].skb = skb;
#endif
        rx_ring->rx_stats.non_eop_descs++;

        return true;
}



#ifdef CONFIG_RNP_DISABLE_PACKET_SPLIT

static bool rnpvf_alloc_mapped_skb(struct rnpvf_ring *rx_ring,
                                 struct rnpvf_rx_buffer *bi)
{
        struct sk_buff *skb = bi->skb;
        dma_addr_t dma = bi->dma;

        if (unlikely(dma))
                return true;

        if (likely(!skb)) {
                skb = netdev_alloc_skb_ip_align(rx_ring->netdev,
                                                rx_ring->rx_buf_len);
                if (unlikely(!skb)) {
                        rx_ring->rx_stats.alloc_rx_buff_failed++;
                        return false;
                }

                bi->skb = skb;
        }
        dma = dma_map_single(rx_ring->dev, skb->data, rx_ring->rx_buf_len,
                             DMA_FROM_DEVICE);

        /*
         * if mapping failed free memory back to system since
         * there isn't much point in holding memory we can't use
         */
        if (dma_mapping_error(rx_ring->dev, dma)) {
                dev_kfree_skb_any(skb);
                bi->skb = NULL;

                rx_ring->rx_stats.alloc_rx_buff_failed++;
                return false;
        }

        bi->dma = dma;
        return true;
}


/**
 * rnp_clean_rx_irq - Clean completed descriptors from Rx ring - legacy
 * @q_vector: structure containing interrupt and ring information
 * @rx_ring: rx descriptor ring to transact packets on
 * @budget: Total limit on number of packets to process
 *
 * This function provides a legacy approach to Rx interrupt
 * handling.  This version will perform better on systems with a low cost
 * dma mapping API.
 *
 * Returns amount of work completed.
 **/
static int rnpvf_clean_rx_irq(struct rnpvf_q_vector *q_vector,
                            struct rnpvf_ring *rx_ring, int budget)
{
        unsigned int total_rx_bytes = 0, total_rx_packets = 0;
        //struct net_device *netdev = rx_ring->netdev;
        struct rnpvf_adapter *adapter = q_vector->adapter;
        unsigned int driver_drop_packets = 0;
        unsigned int err_packets = 0;
        u16 len = 0;
        u16 cleaned_count = rnpvf_desc_unused(rx_ring);

        while (likely(total_rx_packets < budget)) {
                struct rnpvf_rx_buffer *rx_buffer;
                union rnp_rx_desc *rx_desc;
                struct sk_buff *skb;
                u16 ntc;

                /* return some buffers to hardware, one at a time is too slow */
                if (cleaned_count >= RNPVF_RX_BUFFER_WRITE) {
                        rnpvf_alloc_rx_buffers(rx_ring, cleaned_count);
                        cleaned_count = 0;
                }

                ntc = rx_ring->next_to_clean;
                rx_desc = RNPVF_RX_DESC(rx_ring, ntc);
                rx_buffer = &rx_ring->rx_buffer_info[ntc];

                if (!rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_DD))
                        break;
                /*
                 * if (!rx_desc->wb.upper.length)
                 *  break;
                 */

                /* This memory barrier is needed to keep us from reading
                 * any other fields out of the rx_desc until we know the
                 * descriptor has been written back
                 */
                dma_rmb();

                skb = rx_buffer->skb;

                prefetch(skb->data);

                /* handle padding */
                if ((adapter->priv_flags & RNPVF_PRIV_FLAG_FT_PADDING) &&
                    (!(adapter->priv_flags & RNPVF_PRIV_FLAG_PADDING_DEBUG))) {
                        if (likely(rnpvf_test_staterr(rx_desc,
                                                    RNP_RXD_STAT_EOP))) {
                                len = le16_to_cpu(rx_desc->wb.len) -
                                       le16_to_cpu(rx_desc->wb.padding_len);
                        } else {
                                len = le16_to_cpu(rx_desc->wb.len);
                        }
                } else {
                        /* size should not zero */
                        len = le16_to_cpu(rx_desc->wb.len);
                }

                if (rnpvf_check_csum_error(rx_ring, rx_desc, len, &driver_drop_packets)) {
                        dev_kfree_skb_any(skb);
                        cleaned_count++;
                        err_packets++;
                        if (err_packets + total_rx_packets > budget)
                                break;
                        continue;
                }

                // todo check csum error
                //len = le16_to_cpu(rx_desc->wb.len);
                /* pull the header of the skb in */
                __skb_put(skb, len);

                /*
                 * Delay unmapping of the first packet. It carries the
                 * header information, HW may still access the header after
                 * the writeback.  Only unmap it when EOP is reached
                 */
                /* no need to delay unmap */
                //if (!RNP_CB(skb)->head) {
                //      RNP_CB(skb)->dma = rx_buffer->dma;
                //} else {
                        //skb = rnp_merge_active_tail(skb);
                        //dma_unmap_single(rx_ring->dev, rx_buffer->dma,
                        //               rx_ring->rx_buf_len, DMA_FROM_DEVICE);
                //}
                dma_unmap_single(rx_ring->dev, rx_buffer->dma,
                                rx_ring->rx_buf_len, DMA_FROM_DEVICE);

                // todo merge skb tail mode ?
                /* clear skb reference in buffer info structure */
                rx_buffer->skb = NULL;
                rx_buffer->dma = 0;

                cleaned_count++;

                if (rnpvf_is_non_eop(rx_ring, rx_desc, skb))
                        continue;

                /* unmap first */
                //dma_unmap_single(rx_ring->dev, RNP_CB(skb)->dma,
                //               rx_ring->rx_buf_len, DMA_FROM_DEVICE);

                //RNP_CB(skb)->dma = 0;

                //if (rnp_close_active_frag_list(skb) &&
                //    !RNP_CB(skb)->append_cnt) {
                //      /* if we got here without RSC the packet is invalid */
                //      dev_kfree_skb_any(skb);
                //      continue;
                //}

                /* ERR_MASK will only have valid bits if EOP set */
                //if (unlikely(rnp_test_staterr(rx_desc, RNP_RXD_STAT_ERR_MASK) &&
                //           !(netdev->features & NETIF_F_RXALL))){
                //      dev_kfree_skb_any(skb);
                //      continue;
                //}

                /* probably a little skewed due to removing CRC */
                total_rx_bytes += skb->len;

                /* populate checksum, timestamp, VLAN, and protocol */
                rnpvf_process_skb_fields(rx_ring, rx_desc, skb);

                rnpvf_rx_skb(q_vector, skb);

                /* update budget accounting */
                total_rx_packets++;
        }

        u64_stats_update_begin(&rx_ring->syncp);
        rx_ring->stats.packets += total_rx_packets;
        rx_ring->stats.bytes += total_rx_bytes;
        rx_ring->rx_stats.driver_drop_packets += driver_drop_packets;
        //rx_ring->rx_stats.rx_clean_count += total_rx_packets;
        //rx_ring->rx_stats.rx_clean_times++;
        //if (rx_ring->rx_stats.rx_clean_times > 10) {
         //       rx_ring->rx_stats.rx_clean_times = 0;
          //      rx_ring->rx_stats.rx_clean_count = 0;
        //}
        u64_stats_update_end(&rx_ring->syncp);
        q_vector->rx.total_packets += total_rx_packets;
        q_vector->rx.total_bytes += total_rx_bytes;

        /* maybe not good here */
        //if (cleaned_count)
        //      rnp_alloc_rx_buffers(rx_ring, cleaned_count);

        if (total_rx_packets >= budget)
                rx_ring->rx_stats.poll_again_count++;

        return total_rx_packets;
}


#else

#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC
static struct sk_buff *rnpvf_build_skb(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *rx_buffer,
                struct xdp_buff *xdp,
                union rnp_rx_desc *rx_desc)
{
#ifdef HAVE_XDP_BUFF_DATA_META
        unsigned int metasize = xdp->data - xdp->data_meta;
        void *va = xdp->data_meta;
#else
        void *va = xdp->data;
#endif
#if (PAGE_SIZE < 8192)
        unsigned int truesize = rnpvf_rx_pg_size(rx_ring) / 2;
#else
        unsigned int truesize =
                SKB_DATA_ALIGN(sizeof(struct skb_shared_info)) +
                SKB_DATA_ALIGN(xdp->data_end - xdp->data_hard_start);
#endif
        struct sk_buff *skb;

        /* prefetch first cache line of first page */
        prefetch(va);
#if L1_CACHE_BYTES < 128
        prefetch(va + L1_CACHE_BYTES);
#endif

        /* build an skb around the page buffer */
        skb = build_skb(xdp->data_hard_start, truesize);
        if (unlikely(!skb))
                return NULL;

        /* update pointers within the skb to store the data */
        skb_reserve(skb, xdp->data - xdp->data_hard_start);
        __skb_put(skb, xdp->data_end - xdp->data);
#ifdef HAVE_XDP_BUFF_DATA_META
        if (metasize)
                skb_metadata_set(skb, metasize);
#endif
        /* record DMA address if this is the start of a
         * chain of buffers
         */
        /* if (!rnp_test_staterr(rx_desc, RNP_RXD_STAT_EOP))
         * RNP_CB(skb)->dma = rx_buffer->dma;
         */

        //check_udp_chksum((void *)skb->data, rx_buffer);
        /* update buffer offset */
#if (PAGE_SIZE < 8192)
        rx_buffer->page_offset ^= truesize;
#else
        rx_buffer->page_offset += truesize;
#endif

        return skb;
}

#endif /* HAVE_SWIOTLB_SKIP_CPU_SYNC */

static void rnpvf_rx_buffer_flip(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *rx_buffer,
                unsigned int size)
{
#if (PAGE_SIZE < 8192)
        unsigned int truesize = rnpvf_rx_pg_size(rx_ring) / 2;

        rx_buffer->page_offset ^= truesize;
#else
        unsigned int truesize = ring_uses_build_skb(rx_ring) ?
                SKB_DATA_ALIGN(RNPVF_SKB_PAD + size) :
                SKB_DATA_ALIGN(size);

        rx_buffer->page_offset += truesize;
#endif
}

static struct rnpvf_rx_buffer *rnpvf_get_rx_buffer(struct rnpvf_ring *rx_ring,
                                               union rnp_rx_desc *rx_desc,
                                               struct sk_buff **skb,
                                               const unsigned int size)
{
        struct rnpvf_rx_buffer *rx_buffer;

        rx_buffer = &rx_ring->rx_buffer_info[rx_ring->next_to_clean];
        prefetchw(rx_buffer->page);
        *skb = rx_buffer->skb;

        rx_buf_dump("rx buf",
                    page_address(rx_buffer->page) + rx_buffer->page_offset,
                    rx_desc->wb.len);

        /* we are reusing so sync this buffer for CPU use */
        dma_sync_single_range_for_cpu(rx_ring->dev, rx_buffer->dma,
                                      rx_buffer->page_offset, size,
                                      DMA_FROM_DEVICE);
        /* skip_sync: */
        // ??
        rx_buffer->pagecnt_bias--;

        return rx_buffer;
}



static struct sk_buff *rnpvf_construct_skb(struct rnpvf_ring *rx_ring,
                struct rnpvf_rx_buffer *rx_buffer,
                struct xdp_buff *xdp,
                union rnp_rx_desc *rx_desc)
{
        unsigned int size = xdp->data_end - xdp->data;
#if (PAGE_SIZE < 8192)
        unsigned int truesize = rnpvf_rx_pg_size(rx_ring) / 2;
#else
        unsigned int truesize =
                SKB_DATA_ALIGN(xdp->data_end - xdp->data_hard_start);
#endif
        struct sk_buff *skb;

        /* prefetch first cache line of first page */
        prefetch(xdp->data);
#if L1_CACHE_BYTES < 128
        prefetch(xdp->data + L1_CACHE_BYTES);
#endif
        /* Note, we get here by enabling legacy-rx via:
         *
         *    ethtool --set-priv-flags <dev> legacy-rx on
         *
         * In this mode, we currently get 0 extra XDP headroom as
         * opposed to having legacy-rx off, where we process XDP
         * packets going to stack via rnpvf_build_skb(). The latter
         * provides us currently with 192 bytes of headroom.
         *
         * For rnp_construct_skb() mode it means that the
         * xdp->data_meta will always point to xdp->data, since
         * the helper cannot expand the head. Should this ever
         * change in future for legacy-rx mode on, then lets also
         * add xdp->data_meta handling here.
         */

        /* allocate a skb to store the frags */
        skb = napi_alloc_skb(&rx_ring->q_vector->napi, RNPVF_RX_HDR_SIZE);
        if (unlikely(!skb))
                return NULL;

        prefetchw(skb->data);

        if (size > RNPVF_RX_HDR_SIZE) {
                /*
                 * if (!rnp_test_staterr(rx_desc, RNP_RXD_STAT_EOP))
                 * RNP_CB(skb)->dma = rx_buffer->dma;
                 */

                skb_add_rx_frag(skb, 0, rx_buffer->page,
                                xdp->data - page_address(rx_buffer->page), size,
                                truesize);
#if (PAGE_SIZE < 8192)
                rx_buffer->page_offset ^= truesize;
#else
                rx_buffer->page_offset += truesize;
#endif
        } else {
                memcpy(__skb_put(skb, size), xdp->data,
                                ALIGN(size, sizeof(long)));
                rx_buffer->pagecnt_bias++;
        }

        //printk("size is %d\n", size);
        return skb;
}

/**
 * rnp_clean_rx_irq - Clean completed descriptors from Rx ring - bounce buf
 * @q_vector: structure containing interrupt and ring information
 * @rx_ring: rx descriptor ring to transact packets on
 * @budget: Total limit on number of packets to process
 *
 * This function provides a "bounce buffer" approach to Rx interrupt
 * processing.  The advantage to this is that on systems that have
 * expensive overhead for IOMMU access this provides a means of avoiding
 * it by maintaining the mapping of the page to the syste.
 *
 * Returns amount of work completed.
 **/
static int rnpvf_clean_rx_irq(struct rnpvf_q_vector *q_vector,
                struct rnpvf_ring *rx_ring, int budget)
{
        unsigned int total_rx_bytes = 0, total_rx_packets = 0;
        unsigned int err_packets = 0;
        unsigned int driver_drop_packets = 0;
        struct rnpvf_adapter *adapter = q_vector->adapter;
        u16 cleaned_count = rnpvf_desc_unused(rx_ring);
        bool xdp_xmit = false;
        struct xdp_buff xdp;

        xdp.data = NULL;
        xdp.data_end = NULL;

        /*
         * #ifdef HAVE_XDP_BUFF_RXQ
         * xdp.rxq = &rx_ring->xdp_rxq;
         * #endif
         */
        while (likely(total_rx_packets < budget)) {
                union rnp_rx_desc *rx_desc;
                struct rnpvf_rx_buffer *rx_buffer;
                struct sk_buff *skb;
                unsigned int size;

                /* return some buffers to hardware, one at a time is too slow */
                if (cleaned_count >= RNPVF_RX_BUFFER_WRITE) {
                        rnpvf_alloc_rx_buffers(rx_ring, cleaned_count);
                        cleaned_count = 0;
                }
                rx_desc = RNPVF_RX_DESC(rx_ring, rx_ring->next_to_clean);

                rx_buf_dump("rx-desc:", rx_desc, sizeof(*rx_desc));
                // buf_dump("rx-desc:", rx_desc, sizeof(*rx_desc));
                rx_debug_printk("  dd set: %s\n",
                                (rx_desc->wb.cmd & RNP_RXD_STAT_DD) ? "Yes" :
                                                                      "No");

                if (!rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_DD))
                        break;

                rx_debug_printk(
                        "queue:%d  rx-desc:%d has-data len:%d next_to_clean %d\n",
                        rx_ring->rnp_queue_idx, rx_ring->next_to_clean,
                        rx_desc->wb.len, rx_ring->next_to_clean);

                /* handle padding */
                if ((adapter->priv_flags & RNPVF_PRIV_FLAG_FT_PADDING) &&
                    (!(adapter->priv_flags & RNPVF_PRIV_FLAG_PADDING_DEBUG))) {
                        if (likely(rnpvf_test_staterr(rx_desc,
                                                    RNP_RXD_STAT_EOP))) {
                                size = le16_to_cpu(rx_desc->wb.len) -
                                       le16_to_cpu(rx_desc->wb.padding_len);
                        } else {
                                size = le16_to_cpu(rx_desc->wb.len);
                        }
                } else {
                        /* size should not zero */
                        size = le16_to_cpu(rx_desc->wb.len);
                }

                if (!size)
                        break;

                /*
                 * should check csum err
                 * maybe one packet use mutiple descs
                 * no problems hw set all csum_err in mutiple descs
                 * maybe BUG if the last sctp desc less than 60
                 */
                if (rnpvf_check_csum_error(rx_ring, rx_desc, size, &driver_drop_packets)) {
                        cleaned_count++;
                        err_packets++;
                        if (err_packets + total_rx_packets > budget)
                                break;
                        continue;
                }
                /* This memory barrier is needed to keep us from reading
                 * any other fields out of the rx_desc until we know the
                 * descriptor has been written back
                 */
                dma_rmb();

                rx_buffer = rnpvf_get_rx_buffer(rx_ring, rx_desc, &skb, size);

                if (!skb) {
                        xdp.data = page_address(rx_buffer->page) +
                                   rx_buffer->page_offset;
#ifdef HAVE_XDP_BUFF_DATA_META
                        xdp.data_meta = xdp.data;
#endif
                        xdp.data_hard_start = xdp.data - rnpvf_rx_offset(rx_ring);
                        xdp.data_end = xdp.data + size;
                        /* call  xdp hook  use this to support xdp hook */
                        // skb = rnp_run_xdp(adapter, rx_ring, &xdp);
                }

                if (IS_ERR(skb)) {
                        if (PTR_ERR(skb) == -RNPVF_XDP_TX) {
                                xdp_xmit = true;
                                rnpvf_rx_buffer_flip(rx_ring, rx_buffer, size);
                        } else {
                                rx_buffer->pagecnt_bias++;
                        }
                        total_rx_packets++;
                        total_rx_bytes += size;
                } else if (skb) {
                        rnpvf_add_rx_frag(rx_ring, rx_buffer, skb, size);
#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC
                } else if (ring_uses_build_skb(rx_ring)) {
                        skb = rnpvf_build_skb(rx_ring, rx_buffer, &xdp, rx_desc);
#endif
                } else {
                        skb = rnpvf_construct_skb(rx_ring, rx_buffer, &xdp,
                                                rx_desc);
                }

                /* exit if we failed to retrieve a buffer */
                if (!skb) {
                        rx_ring->rx_stats.alloc_rx_buff_failed++;
                        rx_buffer->pagecnt_bias++;
                        break;
                }
                //if (module_enable_ptp && adapter->ptp_rx_en &&
                 //   adapter->flags2 & RNP_FLAG2_PTP_ENABLED)
                  //      rnp_ptp_get_rx_hwstamp(adapter, rx_desc, skb);

                rnpvf_put_rx_buffer(rx_ring, rx_buffer, skb);
                cleaned_count++;

                /* place incomplete frames back on ring for completion */
                if (rnpvf_is_non_eop(rx_ring, rx_desc, skb))
                        continue;

                /* verify the packet layout is correct */
                if (rnpvf_cleanup_headers(rx_ring, rx_desc, skb)) {
                        //skb = NULL;
                        continue;
                }

                /* probably a little skewed due to removing CRC */
                total_rx_bytes += skb->len;

                /* populate checksum, timestamp, VLAN, and protocol */
                rnpvf_process_skb_fields(rx_ring, rx_desc, skb);


                //rx_buf_dump("rx-data:", skb->data, skb->len);

                rnpvf_rx_skb(q_vector, skb);

                /* update budget accounting */
                total_rx_packets++;
        }

        //if (xdp_xmit) {
        //struct rnp_ring *ring = adapter->xdp_ring[smp_processor_id()];
        //
        // /* Force memory writes to complete before letting h/w
        //  * know there are new descriptors to fetch.
        //  */
        //wmb();
        //writel(ring->next_to_use, ring->tail);
        //
        //xdp_do_flush_map();
        //}

        u64_stats_update_begin(&rx_ring->syncp);
        rx_ring->stats.packets += total_rx_packets;
        rx_ring->stats.bytes += total_rx_bytes;
        rx_ring->rx_stats.driver_drop_packets += driver_drop_packets;
        //rx_ring->rx_stats.rx_clean_count += total_rx_packets;
        //rx_ring->rx_stats.rx_clean_times++;
        //if (rx_ring->rx_stats.rx_clean_times > 10) {
        //       rx_ring->rx_stats.rx_clean_times = 0;
	//       rx_ring->rx_stats.rx_clean_count = 0;
        //}
        u64_stats_update_end(&rx_ring->syncp);
        q_vector->rx.total_packets += total_rx_packets;
        q_vector->rx.total_bytes += total_rx_bytes;

        //printk("clean rx irq %d\n", total_rx_packets);
        if (total_rx_packets >= budget)
                rx_ring->rx_stats.poll_again_count++;
        return total_rx_packets;
}
#endif /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
#endif



/**
 * rnpvf_configure_msix - Configure MSI-X hardware
 * @adapter: board private structure
 *
 * rnpvf_configure_msix sets up the hardware to properly generate MSI-X
 * interrupts.
 **/
static void rnpvf_configure_msix(struct rnpvf_adapter *adapter)
{
	struct rnpvf_q_vector *q_vector;
	//struct rnpvf_hw *hw = &adapter->hw;
	int i;

	//rnpvf_dbg("[%s] num_q_vectors:%d\n", __func__, adapter->num_q_vectors);

	// for pf<->vf mbx. use vector0
	//hw->mbx.ops.configure(hw, adapter->msix_entries[0].entry, true);

	/*
	 * configure ring-msix Registers table
	 */
	for (i = 0; i < adapter->num_q_vectors; i++) {
		struct rnpvf_ring *ring;

		q_vector = adapter->q_vector[i];

		rnpvf_for_each_ring(ring, q_vector->rx) {
			rnpvf_set_ring_vector(
				adapter, ring->rnpvf_msix_off, q_vector->v_idx);
		}
	}
}

enum latency_range
{
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};
static inline void rnpvf_irq_enable_queues(struct rnpvf_q_vector *q_vector)
{
	struct rnpvf_ring *ring;

	rnpvf_for_each_ring(ring, q_vector->rx)
	{
		// clear irq
		rnpvf_wr_reg(ring->dma_int_clr, RX_INT_MASK | TX_INT_MASK);
		/* we need this */
		wmb();
#ifdef CONFIG_RNP_DISABLE_TX_IRQ
		rnpvf_wr_reg(ring->dma_int_mask, ~(RX_INT_MASK));
#else
		rnpvf_wr_reg(ring->dma_int_mask, ~(RX_INT_MASK | TX_INT_MASK));
#endif
	}
}

static inline void rnpvf_irq_disable_queues(struct rnpvf_q_vector *q_vector)
{
	struct rnpvf_ring *ring;

	rnpvf_for_each_ring(ring, q_vector->tx)
	{
		rnpvf_wr_reg(ring->dma_int_mask, (RX_INT_MASK | TX_INT_MASK));
		// rnpvf_wr_reg(ring->dma_int_clr, RX_INT_MASK|TX_INT_MASK);
	}
}
/**
 * rnpvf_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/
static inline void rnpvf_irq_enable(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_q_vectors; i++)
		rnpvf_irq_enable_queues(adapter->q_vector[i]);
}

static irqreturn_t rnpvf_msix_other(int irq, void *data)
{
	struct rnpvf_adapter *adapter = data;
	//struct pci_dev *pdev = adapter->pdev;
	struct rnpvf_hw *hw = &adapter->hw;
	//u32 msg;
	//bool got_ack = false;

	dbg("\n\n !!! %s irq-comming !!!\n", __func__);
	//dev_info(&adapter->pdev->dev, "irq-comming\n");

	/* link is down by pf */
	// check is vf poll 
	if (test_bit(__RNPVF_MBX_POLLING, &adapter->state))	
		goto NO_WORK_DONE;
	//spin_lock_bh(&adapter->mbx_lock);
	//dev_info(&pdev->dev, "lock\n");
	//dev_info(&adapter->pdev->dev, "no polling irq-comming\n");
	if (!hw->mbx.ops.check_for_rst(hw, false)) {
		if (test_bit(__RNPVF_REMOVE, &adapter->state)) {
			printk("rnpvf is removed\n");
		} else {
			// only link status and mtu call this 
			// mod_timer(&adapter->watchdog_timer, round_jiffies(jiffies + 1));
			//dev_info(&pdev->dev, "link status\n");
			//adapter->link_up = false;
		}
	}
	//spin_unlock_bh(&adapter->mbx_lock);
NO_WORK_DONE:
	//dev_info(&adapter->pdev->dev, "irq-done\n");
	
	return IRQ_HANDLED;
}

static void rnpvf_htimer_start(struct rnpvf_q_vector *q_vector)
{
	unsigned long ns = q_vector->irq_check_usecs * NSEC_PER_USEC / 2;	

	hrtimer_start_range_ns(&q_vector->irq_miss_check_timer, ns_to_ktime(ns),
				       ns, HRTIMER_MODE_REL);
}


static void rnpvf_htimer_stop(struct rnpvf_q_vector *q_vector)
{
	hrtimer_cancel(&q_vector->irq_miss_check_timer);
}


static irqreturn_t rnpvf_intr(int irq, void *data)
{
        struct rnpvf_adapter *adapter = data;
        struct rnpvf_q_vector *q_vector = adapter->q_vector[0];
	struct rnpvf_hw *hw = &adapter->hw;
        // todo 
        /* handle data */
        // in this mode only 1 q_vector is used 
        if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_IRQ_MISS_CHECK)
                rnpvf_htimer_stop(q_vector);

        /*  disabled interrupts (on this vector) for us */
        rnpvf_irq_disable_queues(q_vector);

        if (q_vector->rx.ring || q_vector->tx.ring)
                napi_schedule_irqoff(&q_vector->napi);

	dbg("\n\n !!! %s irq-comming !!!\n", __func__);

	/* link is down by pf */
	// check is vf poll 
	if (test_bit(__RNPVF_MBX_POLLING, &adapter->state))	
		goto WORK_DONE;
	if (!hw->mbx.ops.check_for_rst(hw, false)) {
		if (test_bit(__RNPVF_REMOVE, &adapter->state)) {
			printk("rnpvf is removed\n");
		} else {
			// only link status and mtu call this 
			//dev_info(&pdev->dev, "link status\n");
			//adapter->link_up = false;
		}
	}
WORK_DONE:
	return IRQ_HANDLED;

}

static irqreturn_t rnpvf_msix_clean_rings(int irq, void *data)
{
	struct rnpvf_q_vector *q_vector = data;

	if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_IRQ_MISS_CHECK)
		rnpvf_htimer_stop(q_vector);
	/*  disabled interrupts (on this vector) for us */
	rnpvf_irq_disable_queues(q_vector);

	if (q_vector->rx.ring || q_vector->tx.ring)
		napi_schedule(&q_vector->napi);



	return IRQ_HANDLED;
}

void update_rx_count(int cleaned, struct rnpvf_q_vector *q_vector)
{
        struct rnpvf_adapter *adapter = q_vector->adapter;
#if 0
        struct rnp_ring_container *ring_container = &q_vector->rx;
        unsigned long next_update = jiffies;
        unsigned int packets, bytes;

        if ((cleaned) && (cleaned < 10)) {
                q_vector->new_rx_count = 1;
                goto clear_counts;
        }

        if (time_after(next_update, ring_container->next_update))
                goto clear_counts;

        packets = ring_container->total_packets;
        bytes = ring_container->total_bytes;

        if (packets < 5) {
                q_vector->new_rx_count /= 2;

        } else if (packets < 20) {
                //q_vector->new_rx_count = 1;
                q_vector->new_rx_count *= 2;
                if (q_vector->new_rx_count > 64)
                        q_vector->new_rx_count = 128;

                if (q_vector->old_rx_count != q_vector->new_rx_count) {
                        printk("%d change large %x, packets %d\n", q_vector->v_idx, q_vector->new_rx_count, packets);
                        printk("old %d new %d\n", q_vector->old_rx_count, q_vector->new_rx_count);
                }
        } else if (packets < 40) {
                // 48 - 96
                // do nothing
        } else {
                //q_vector->small_times = 0;
                //q_vector->new_rx_count -= (1 << (q_vector->large_times++));
                q_vector->new_rx_count /= 2;
                if (q_vector->new_rx_count == 0)
                        q_vector->new_rx_count = 1;
                if (q_vector->old_rx_count != q_vector->new_rx_count)
                        printk("%d change small %x, packets %d\n", q_vector->v_idx, q_vector->new_rx_count, packets);

        }


clear_counts:
        /* write back value */
        //ring_container->itr = itr;

        /* next update should occur within next jiffy */
        ring_container->next_update = next_update + 1;
        ring_container->total_bytes = 0;
        ring_container->total_packets = 0;
#else
        if ((cleaned) && (cleaned != q_vector->new_rx_count)) {
                //q_vector->new_rx_count = cleaned;
                if (cleaned < 5) {
                        q_vector->small_times = 0;
                        q_vector->large_times = 0;
                        q_vector->too_small_times++;
                        //q_vector->middle_time = 0;
                        if (q_vector->too_small_times >= 2) {
                                q_vector->new_rx_count = 1;
                        } else {
                                //printk("%d delay change to 1 %d\n", q_vector->v_idx, q_vector->too_small_times);
                        }
                        //if (q_vector->old_rx_count != q_vector->new_rx_count) {
                         //       printk("%d change to 1 %d \n", q_vector->v_idx, cleaned);
                          //      printk("old %d new %d\n", q_vector->old_rx_count, q_vector->new_rx_count);
                        //}
                } else if (cleaned < 30) {
                        q_vector->too_small_times = 0;
                        q_vector->middle_time++;
                        // count is 10 - 40
                        // try to keep in this stage
                        //if (q_vector->middle_time >= 2) {
                                //q_vector->new_rx_count =  cleaned - 2;
                                if (cleaned < q_vector->new_rx_count) {
                                        //change small
                                        q_vector->small_times = 0;
                                        q_vector->new_rx_count -= (1 << (q_vector->large_times++));
                                        if (q_vector->new_rx_count < 0)
                                                q_vector->new_rx_count = 1;
                                        //printk("%d change small %d %d\n", q_vector->v_idx, cleaned, q_vector->new_rx_count);

                                } else {
                                        q_vector->large_times = 0;

                                        if (cleaned > 30) {
                                                if (q_vector->new_rx_count == (cleaned - 4)) {

                                                } else {
                                                        q_vector->new_rx_count += (1 << (q_vector->small_times++));
                                                }
                                                // should no more than q_vector
                                                if (q_vector->new_rx_count >= cleaned) {
                                                        q_vector->new_rx_count =  cleaned - 4;
                                                        q_vector->small_times = 0;
                                                }


                                        } else {
                                                if (q_vector->new_rx_count == (cleaned - 1)) {

                                                } else {
                                                        q_vector->new_rx_count += (1 << (q_vector->small_times++));
                                                }
                                                // should no more than q_vector
                                                if (q_vector->new_rx_count >= cleaned) {
                                                        q_vector->new_rx_count =  cleaned - 1;
                                                        q_vector->small_times = 0;
                                                }


                                        }
                                        //change small
                                        //printk("%d change large %d %d\n", q_vector->v_idx, cleaned, q_vector->new_rx_count);
                                }
                        //}
                } else {
                        //printk("%d change to 128 %d", q_vector->new_rx_count, cleaned);
                        //q_vector->middle_time = 0;
                        q_vector->too_small_times = 0;
                        q_vector->new_rx_count = max_t(int, 64, adapter->rx_frames);
                        q_vector->small_times = 0;
                        q_vector->large_times = 0;
                        // 40 - 64
                }
        }
        /*
           if ((cleaned) && (cleaned != q_vector->new_rx_count)) {
        //q_vector->new_rx_count = cleaned;
        if (cleaned < 10) {
        q_vector->new_rx_count = 1;
        q_vector->small_times = 0;
        q_vector->large_times = 0;

        } else if (cleaned < q_vector->new_rx_count) {
        // count is large
        q_vector->small_times = 0;
        q_vector->new_rx_count -= (1 << (q_vector->large_times++));

        if (q_vector->new_rx_count < 0)
        q_vector->new_rx_count = 1;
        printk("change small %x %x\n", cleaned, q_vector->new_rx_count);
        } else {
        q_vector->large_times = 0;
        q_vector->new_rx_count += (1 << (q_vector->small_times++));
        if (q_vector->new_rx_count > 32)
        q_vector->new_rx_count = 128;
        //q_vector->new_rx_count += 4;
        printk("change large %x %x\n", cleaned, q_vector->new_rx_count);
        //printk("update to 128\n");
        }
        }
        */
#endif
}

static void rnpvf_check_restart_tx(struct rnpvf_q_vector *q_vector,
                        struct rnpvf_ring *tx_ring)
{

        struct rnpvf_adapter *adapter = q_vector->adapter;
#define TX_WAKE_THRESHOLD (DESC_NEEDED * 2)
        if (likely(netif_carrier_ok(tx_ring->netdev) &&
                     (rnpvf_desc_unused(tx_ring) >= TX_WAKE_THRESHOLD))) {
                /* Make sure that anybody stopping the queue after this
                 * sees the new next_to_clean.
                 */
                smp_mb();
#ifdef HAVE_TX_MQ
                if (__netif_subqueue_stopped(tx_ring->netdev,
                                             tx_ring->queue_index) &&
                    !test_bit(__RNPVF_DOWN, &adapter->state)) {
                        netif_wake_subqueue(tx_ring->netdev,
                                            tx_ring->queue_index);
                        ++tx_ring->tx_stats.restart_queue;
                }
#else
                if (__netif_queue_stopped(tx_ring->netdev) &&
                    !test_bit(__RNPVF_DOWN, &adapter->state)) {
                        netif_wake_queue(tx_ring->netdev);
                        ++tx_ring->tx_stats.restart_queue;
                }

#endif
        }

}



/**
 * rnpvf_poll - NAPI polling calback
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function will clean more than one or more rings associated with a
 * q_vector.
 **/
static int rnpvf_poll(struct napi_struct *napi, int budget)
{
	struct rnpvf_q_vector *q_vector =
		container_of(napi, struct rnpvf_q_vector, napi);
	struct rnpvf_adapter *adapter = q_vector->adapter;
	struct rnpvf_ring *ring;
	int per_ring_budget, work_done = 0;
	bool clean_complete = true;
	int cleaned_total = 0;

#ifdef CONFIG_RNP_DCA
	if (adapter->flags & RNP_FLAG_DCA_ENABLED)
		rnpvf_update_dca(q_vector);
#endif

	rnpvf_for_each_ring(ring, q_vector->tx) clean_complete &=
		!!rnpvf_clean_tx_irq(q_vector, ring);

	/* attempt to distribute budget to each queue fairly, but don't allow
	 * the budget to go below 1 because we'll exit polling
	 */
	if (q_vector->rx.count > 1)
		per_ring_budget = max(budget / q_vector->rx.count, 1);
	else
		per_ring_budget = budget;

	rnpvf_for_each_ring(ring, q_vector->rx) {
		int cleaned = 0;

		cleaned = rnpvf_clean_rx_irq(q_vector, ring, per_ring_budget);

		work_done += cleaned;
		cleaned_total += cleaned;

		if (cleaned >= per_ring_budget)
			clean_complete = false;
	}

	//force irq stop
	if (test_bit(__RNPVF_DOWN, &adapter->state))
		clean_complete = true;
	
	if (!(q_vector->vector_flags & RNPVF_QVECTOR_FLAG_ITR_FEATURE))
		update_rx_count(cleaned_total, q_vector);

	/* If all work not completed, return budget and keep polling */
	if (!clean_complete)
		return budget;

	/* all work done, exit the polling mode */
	if (likely(napi_complete_done(napi, work_done))) {
		/* try to do itr handle */
		if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_ITR_FEATURE)
			rnpvf_set_itr(q_vector);

		if (!test_bit(__RNPVF_DOWN, &adapter->state)) {
			rnpvf_irq_enable_queues(q_vector);
			smp_mb();
			/* we need this to ensure irq start before tx start */
			if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_REDUCE_TX_IRQ_MISS) {
				rnpvf_for_each_ring(ring, q_vector->tx) {
					rnpvf_check_restart_tx(q_vector, ring);
					if (q_vector->new_rx_count != q_vector->old_rx_count) {
						ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_PKTCNT, q_vector->new_rx_count);
						//ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_TIMER,
						//              q_vector->new_usesc * 500);
						q_vector->old_rx_count = q_vector->new_rx_count;
					}
				}
			}
		}
         }

	// setup itr
	if (!test_bit(__RNPVF_DOWN, &adapter->state)) {
		//rnpvf_irq_enable_queues(adapter, q_vector);
		//rnpvf_htimer_stop(q_vector);
		if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_IRQ_MISS_CHECK)
			rnpvf_htimer_start(q_vector);

	}
	return 0;
}

/**
 * rnpvf_request_msix_irqs - Initialize MSI-X interrupts
 * @adapter: board private structure
 *
 * rnpvf_request_msix_irqs allocates MSI-X vectors and requests
 * interrupts from the kernel.
 **/
static int rnpvf_request_msix_irqs(struct rnpvf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err;
	int i = 0;

	DPRINTK(IFUP, INFO, "num_q_vectors:%d\n", adapter->num_q_vectors);

	for (i = 0; i < adapter->num_q_vectors; i++) {
		struct rnpvf_q_vector *q_vector = adapter->q_vector[i];
		struct msix_entry *entry =
			&adapter->msix_entries[i + adapter->vector_off];

		if (q_vector->tx.ring && q_vector->rx.ring) {
			snprintf(q_vector->name,
					 sizeof(q_vector->name) - 1,
					 "%s-%s-%d-%d",
					 netdev->name,
					 "TxRx",
					 i,
					 q_vector->v_idx);
		} else {
			WARN(!(q_vector->tx.ring && q_vector->rx.ring),
				 "%s vector%d tx rx is null, v_idx:%d\n",
				 netdev->name,
				 i,
				 q_vector->v_idx);
			/* skip this unused q_vector */
			continue;
		}
		err = request_irq(entry->vector,
				&rnpvf_msix_clean_rings,
				0,
				q_vector->name,
				q_vector);
		if (err) {
			rnpvf_err("%s:request_irq failed for MSIX interrupt:%d "
					"Error: %d\n",
					netdev->name,
					entry->vector,
					err);
			goto free_queue_irqs;
		}
		irq_set_affinity_hint(entry->vector, &q_vector->affinity_mask);
	}


	return 0;

free_queue_irqs:
	while (i) {
		i--;
		irq_set_affinity_hint(
			adapter->msix_entries[i + adapter->vector_off].vector, NULL);
		free_irq(adapter->msix_entries[i + adapter->vector_off].vector,
				 adapter->q_vector[i]);
	}
	return err;
}

static int rnpvf_free_msix_irqs(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_q_vectors; i++) {
		struct rnpvf_q_vector *q_vector = adapter->q_vector[i];
		struct msix_entry *entry =
			&adapter->msix_entries[i + adapter->vector_off];

		/* free only the irqs that were actually requested */
		if (!q_vector->rx.ring && !q_vector->tx.ring)
			continue;

		/* clear the affinity_mask in the IRQ descriptor */
		irq_set_affinity_hint(entry->vector, NULL);
		DPRINTK(IFDOWN, INFO, "free irq %s\n", q_vector->name);
		free_irq(entry->vector, q_vector);
	}

	return 0;
}

#ifdef DISABLE_RX_IRQ
int rx_poll_thread_handler(void *data)
{
	int i;
	struct rnpvf_adapter *adapter = data;

	printk("%s  %s running...\n", __func__, adapter->name);

	do {
		for (i = 0; i < adapter->num_q_vectors; i++)
			rnpvf_msix_clean_rings(0, adapter->q_vector[i]);

		msleep(20);
	} while (!kthread_should_stop() && adapter->quit_poll_thread != true);

	printk("%s  %s stoped\n", __func__, adapter->name);

	return 0;
}
#endif

/**
 * rnpvf_update_itr - update the dynamic ITR value based on statistics
 * @q_vector: structure containing interrupt and ring information
 * @ring_container: structure containing ring performance data
 *
 *      Stores a new ITR value based on packets and byte
 *      counts during the last interrupt.  The advantage of per interrupt
 *      computation is faster updates and more accurate ITR for the current
 *      traffic pattern.  Constants in this function were computed
 *      based on theoretical maximum wire speed and thresholds were set based
 *      on testing data as well as attempting to minimize response time
 *      while increasing bulk throughput.
 **/
static void rnpvf_update_itr(struct rnpvf_q_vector *q_vector,
		struct rnpvf_ring_container *ring_container, int type)
{

        unsigned int itr =
                RNPVF_ITR_ADAPTIVE_MIN_USECS | RNPVF_ITR_ADAPTIVE_LATENCY;
        unsigned int avg_wire_size, packets, bytes;
        unsigned int packets_old;
        unsigned long next_update = jiffies;
        u32 old_itr;
        u16 add_itr, add = 0;
        // 0 is tx ;1 is rx
        if (type)
                old_itr = q_vector->itr_rx;
        else
                old_itr = q_vector->itr_tx;


        /* If we don't have any rings just leave ourselves set for maximum
         * possible latency so we take ourselves out of the equation.
         */
        if (!ring_container->ring)
                return;

        packets_old = ring_container->total_packets_old;
        packets = ring_container->total_packets;
        bytes = ring_container->total_bytes;
        add_itr = ring_container->add_itr;
        //add_itr = 0;
        /* If Rx and there are 1 to 23 packets and bytes are less than
         * 12112 assume insufficient data to use bulk rate limiting
         * approach. Instead we will focus on simply trying to target
         * receiving 8 times as much data in the next interrupt.
         */
        //printk("%d packets %d bytes %d %d \n", q_vector->v_idx, packets, bytes, old_itr >> 2);

        // it is not rx irq
        if (!packets)
                return;

        if (packets && packets < 24 && bytes < 12112) {
                itr = RNPVF_ITR_ADAPTIVE_LATENCY;


                //avg_wire_size = (bytes + packets * 24) * 2;
                avg_wire_size = (bytes + packets * 24);
                //avg_wire_size = clamp_t(unsigned int,
                //              avg_wire_size, 2560, 12800);
                        //      we minimu is 2us in latency mode
                        //      200 us
                avg_wire_size = clamp_t(unsigned int,
                                avg_wire_size, 128, 12800);

                goto adjust_for_speed;
        }

        /* Less than 48 packets we can assume that our current interrupt delay
         * is only slightly too low. As such we should increase it by a small
         * fixed amount.
         */
        if (packets < 48) {
                // if we add in the last itr
                if (add_itr) {
                        // we really get more packets
                        // add more again
                        if (packets_old < packets) {
                                itr = (old_itr >> 2) + RNPVF_ITR_ADAPTIVE_MIN_INC;
                                if (itr > RNPVF_ITR_ADAPTIVE_MAX_USECS)
                                        itr = RNPVF_ITR_ADAPTIVE_MAX_USECS;
                                add = 1;

                                //printk("%d add %d %d\n", packets, itr, q_vector->v_idx);

                                if (packets < 8)
                                        itr += RNPVF_ITR_ADAPTIVE_LATENCY;
                                else
                                        itr += ring_container->itr & RNPVF_ITR_ADAPTIVE_LATENCY;

                        } else {
                                // we add itr before ,but not get more packets
                                // minis
                                itr = (old_itr >> 2) - RNPVF_ITR_ADAPTIVE_MIN_INC;
                                if (itr < RNPVF_ITR_ADAPTIVE_MIN_USECS)
                                        itr = RNPVF_ITR_ADAPTIVE_MIN_USECS;
                                //printk("%d minis %d %d\n", packets, itr, q_vector->v_idx);
                        }

                } else {
                        // we not add before, add itr
                        add = 1;
                        itr = (old_itr >> 2) + RNPVF_ITR_ADAPTIVE_MIN_INC;
                        if (itr > RNPVF_ITR_ADAPTIVE_MAX_USECS)
                                itr = RNPVF_ITR_ADAPTIVE_MAX_USECS;


                        //printk("%d add here %d %d\n", packets, itr, q_vector->v_idx);
                        // try to detect packets add or not

                        /* If sample size is 0 - 7 we should probably switch
                         * to latency mode instead of trying to control
                         * things as though we are in bulk.
                         *
                         * Otherwise if the number of packets is less than 48
                         * we should maintain whatever mode we are currently
                         * in. The range between 8 and 48 is the cross-over
                         * point between latency and bulk traffic.
                         */
                        if (packets < 8)
                                itr += RNPVF_ITR_ADAPTIVE_LATENCY;
                        else
                                itr += ring_container->itr & RNPVF_ITR_ADAPTIVE_LATENCY;

                }
                goto clear_counts;
        }

        /* Between 48 and 96 is our "goldilocks" zone where we are working
         * out "just right". Just report that our current ITR is good for us.
         */
        if (packets < 96) {
                itr = old_itr >> 2;
                goto clear_counts;
        }
        /* If packet count is 96 or greater we are likely looking at a slight
         * overrun of the delay we want. Try halving our delay to see if that
         * will cut the number of packets in half per interrupt.
         */
        if (packets < 256) {
                itr = old_itr >> 3;
                if (itr < RNPVF_ITR_ADAPTIVE_MIN_USECS)
                        itr = RNPVF_ITR_ADAPTIVE_MIN_USECS;
                goto clear_counts;
        }

        /* The paths below assume we are dealing with a bulk ITR since number
         * of packets is 256 or greater. We are just going to have to compute
         * a value and try to bring the count under control, though for smaller
         * packet sizes there isn't much we can do as NAPI polling will likely
         * be kicking in sooner rather than later.
         */
        itr = RNPVF_ITR_ADAPTIVE_BULK;

        /* If packet counts are 256 or greater we can assume we have a gross
         * overestimation of what the rate should be. Instead of trying to fine
         * tune it just use the formula below to try and dial in an exact value
         * give the current packet size of the frame.
         */
        avg_wire_size = bytes / packets;

        /* The following is a crude approximation of:
         *  wmem_default / (size + overhead) = desired_pkts_per_int
         *  rate / bits_per_byte / (size + ethernet overhead) = pkt_rate
         *  (desired_pkt_rate / pkt_rate) * usecs_per_sec = ITR value
         *
         * Assuming wmem_default is 212992 and overhead is 640 bytes per
         * packet, (256 skb, 64 headroom, 320 shared info), we can reduce the
         * formula down to
         *
         *  (170 * (size + 24)) / (size + 640) = ITR
         *
         * We first do some math on the packet size and then finally bitshift
         * by 8 after rounding up. We also have to account for PCIe link speed
         * difference as ITR scales based on this.
         */
        if (avg_wire_size <= 60) {
                /* Start at 50k ints/sec */
                avg_wire_size = 5120;
        } else if (avg_wire_size <= 316) {
                /* 50K ints/sec to 16K ints/sec */
                avg_wire_size *= 40;
                avg_wire_size += 2720;
        } else if (avg_wire_size <= 1084) {
                /* 16K ints/sec to 9.2K ints/sec */
                avg_wire_size *= 15;
                avg_wire_size += 11452;
        } else if (avg_wire_size <= 1980) {
                /* 9.2K ints/sec to 8K ints/sec */
                avg_wire_size *= 5;
                avg_wire_size += 22420;
        } else {
                /* plateau at a limit of 8K ints/sec */
                avg_wire_size = 32256;
        }

adjust_for_speed:
        /* Resultant value is 256 times larger than it needs to be. This
         * gives us room to adjust the value as needed to either increase
         * or decrease the value based on link speeds of 10G, 2.5G, 1G, etc.
         *
         * Use addition as we have already recorded the new latency flag
         * for the ITR value.
         */
        switch (q_vector->adapter->link_speed) {
        case RNP_LINK_SPEED_10GB_FULL:
        case RNP_LINK_SPEED_100_FULL:
        //case RNP_LINK_SPEED_1GB_FULL:
        default:
                itr += DIV_ROUND_UP(avg_wire_size,
                                    RNPVF_ITR_ADAPTIVE_MIN_INC * 256) *
                       RNPVF_ITR_ADAPTIVE_MIN_INC;
                break;
        //case RNP_LINK_SPEED_2_5GB_FULL:
        case RNP_LINK_SPEED_1GB_FULL:
        case RNP_LINK_SPEED_10_FULL:
                itr += DIV_ROUND_UP(avg_wire_size,
                                    RNPVF_ITR_ADAPTIVE_MIN_INC * 64) *
                       RNPVF_ITR_ADAPTIVE_MIN_INC;
                break;
        }

        /* In the case of a latency specific workload only allow us to
         * reduce the ITR by at most 2us. By doing this we should dial
         * in so that our number of interrupts is no more than 2x the number
         * of packets for the least busy workload. So for example in the case
         * of a TCP worload the ack packets being received would set the
         * the interrupt rate as they are a latency specific workload.
         */
        if ((itr & RNPVF_ITR_ADAPTIVE_LATENCY) && itr < ring_container->itr)
                itr = ring_container->itr - RNPVF_ITR_ADAPTIVE_MIN_INC;

clear_counts:
        //printk("%d update itr %d\n", q_vector->v_idx, itr);
        /* write back value */
        ring_container->itr = itr;

        /* next update should occur within next jiffy */
        ring_container->next_update = next_update + 1;

        ring_container->total_bytes = 0;
        ring_container->total_packets_old = packets;
        ring_container->add_itr = add;
        ring_container->total_packets = 0;
}


/**
 * rnpvf_write_eitr - write EITR register in hardware specific way
 * @q_vector: structure containing interrupt and ring information
 *
 * This function is made to be called by ethtool and by the driver
 * when it needs to update EITR registers at runtime.  Hardware
 * specific quirks/differences are taken care of here.
 */
void rnpvf_write_eitr_rx(struct rnpvf_q_vector *q_vector)
{
        struct rnpvf_adapter *adapter = q_vector->adapter;
        struct rnpvf_hw *hw = &adapter->hw;
        //int v_idx = q_vector->v_idx;
        // u32 itr_reg = q_vector->itr & RNP_MAX_EITR;
        u32 itr_reg = q_vector->itr_rx >> 2;
        struct rnpvf_ring *ring;

        //printk("update %d itr %d\n", q_vector->v_idx, itr_reg);
        itr_reg = itr_reg * hw->usecstocount; // 150M

        rnpvf_for_each_ring(ring, q_vector->rx) {
                ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_TIMER,
                     itr_reg);
        }
        /*rnp_for_each_ring(ring, q_vector->tx) {
                ring_wr32(ring, RNP_DMA_REG_TX_INT_DELAY_TIMER,
                     itr_reg);
        }*/
}


static void rnpvf_set_itr(struct rnpvf_q_vector *q_vector)
{

	//u32 new_itr_tx;
	u32 new_itr_rx;


	//rnp_update_itr(q_vector, &q_vector->tx, 0);
	rnpvf_update_itr(q_vector, &q_vector->rx, 1);

	/* use the smallest value of new ITR delay calculations */
	//new_itr = min(q_vector->rx.itr, q_vector->tx.itr);
	//printk("rx itr %x , tx itr %x\n", q_vector->rx.itr, q_vector->tx.itr);

//      new_itr_tx = q_vector->tx.itr;
        new_itr_rx = q_vector->rx.itr;
        /* Clear latency flag if set, shift into correct position */
//      new_itr_tx &= ~RNP_ITR_ADAPTIVE_LATENCY;
        new_itr_rx &= RNPVF_ITR_ADAPTIVE_MASK_USECS;
        /* in 2us unit */
//      new_itr_tx <<= 2;
        new_itr_rx <<= 2;

//      if (new_itr_tx != q_vector->itr_tx) {
//              /* save the algorithm value here */
//              q_vector->itr_tx = new_itr_tx;
//              rnp_write_eitr_tx(q_vector);
//      }
        //printk("old %d new %d\n", q_vector->itr_rx, new_itr_rx);
        if (new_itr_rx != q_vector->itr_rx) {
                /* save the algorithm value here */
                q_vector->itr_rx = new_itr_rx;
                rnpvf_write_eitr_rx(q_vector);
        }



}

/**
 * rnpvf_request_irq - initialize interrupts
 * @adapter: board private structure
 *
 * Attempts to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static int rnpvf_request_irq(struct rnpvf_adapter *adapter)
{
	int err;

#ifdef DISABLE_RX_IRQ
	adapter->rx_poll_thread =
		kthread_run(rx_poll_thread_handler, adapter, adapter->name);
	if (!adapter->rx_poll_thread) {
		rnpvf_err("kthread_run faild!\n");
		return -EIO;
	}
	return 0;
#endif

        if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
                err = rnpvf_request_msix_irqs(adapter);
        } else if (adapter->flags & RNPVF_FLAG_MSI_ENABLED) {
                /* in this case one for all */
                err = request_irq(adapter->pdev->irq, rnpvf_intr, 0,
                                adapter->netdev->name, adapter);
        } else {
                err = request_irq(adapter->pdev->irq, rnpvf_intr, IRQF_SHARED,
                                adapter->netdev->name, adapter);
        }
	if (err)
		rnpvf_err("request_irq failed, Error %d\n", err);

	return err;
}

static void rnpvf_free_irq(struct rnpvf_adapter *adapter)
{
	//int i;
	//u32 msgbuf[2];
	//struct rnpvf_hw *hw = &adapter->hw;

	//msgbuf[0] = RNP_PF_REMOVE;

#ifdef DISABLE_RX_IRQ
	return;
#endif
	if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
		rnpvf_free_msix_irqs(adapter);
	} else if (adapter->flags & RNPVF_FLAG_MSI_ENABLED) {
                /* in this case one for all */
                free_irq(adapter->pdev->irq, adapter);
        } else {
                free_irq(adapter->pdev->irq, adapter);
        }

}

/**
 * rnpvf_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/
static inline void rnpvf_irq_disable(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_q_vectors; i++) {
		rnpvf_irq_disable_queues(adapter->q_vector[i]);
		if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
			synchronize_irq(adapter->msix_entries[i + adapter->vector_off].vector);
                } else {
                        synchronize_irq(adapter->pdev->irq);
                }
	
	}
}

/**
 * rnpvf_configure_tx_ring - Configure 8259x Tx ring after Reset
 * @adapter: board private structure
 * @ring: structure containing ring specific data
 *
 * Configure the Tx descriptor ring after a reset.
 **/
void rnpvf_configure_tx_ring(struct rnpvf_adapter *adapter,
		struct rnpvf_ring *ring)
{
	struct rnpvf_hw *hw = &adapter->hw;

	/* disable queue to avoid issues while updating state */
        if (!(ring->ring_flags & RNPVF_RING_SKIP_TX_START))
		ring_wr32(ring, RNP_DMA_TX_START, 0);


	ring_wr32(ring, RNP_DMA_REG_TX_DESC_BUF_BASE_ADDR_LO, (u32)ring->dma);
	/* dma high address is used for vfnum */
	ring_wr32(ring,
		 RNP_DMA_REG_TX_DESC_BUF_BASE_ADDR_HI,
		 (u32)(((u64)ring->dma) >> 32) | (hw->vfnum << 24));
	ring_wr32(ring, RNP_DMA_REG_TX_DESC_BUF_LEN, ring->count);

	// tail <= head
	ring->next_to_clean = ring_rd32(ring, RNP_DMA_REG_TX_DESC_BUF_HEAD);
	ring->next_to_use = ring->next_to_clean;
	ring->tail = ring->ring_addr + RNP_DMA_REG_TX_DESC_BUF_TAIL;
	rnpvf_wr_reg(ring->tail, ring->next_to_use);

	ring_wr32(ring,
		 RNP_DMA_REG_TX_DESC_FETCH_CTRL,
		 (8 << 0)	/*max_water_flow*/
		 | (TSRN10_TX_DEFAULT_BURST << 16)); /*max-num_descs_peer_read*/

	ring_wr32(ring,
		 RNP_DMA_REG_TX_INT_DELAY_TIMER,
		 adapter->tx_usecs * hw->usecstocount); // tx-timeout-irq
	ring_wr32(ring,
		 RNP_DMA_REG_TX_INT_DELAY_PKTCNT,
		 adapter->tx_frames); // tx-count-irq

	ring_wr32(ring,
		 RNP_DMA_REG_TX_FLOW_CTRL_TH,
		 0x0); // flow control: bytes-peer-ctrl-tm-clk. 0:no-control
        // enable queue
        if (!(ring->ring_flags & RNPVF_RING_SKIP_TX_START)) {
                /* n500 should wait tx_ready before open tx start */
                int timeout = 0;
                u32 status = 0;

                do {
                        status = ring_rd32(ring, RNP_DMA_TX_READY);
                        usleep_range(100, 200);
                        timeout++;
                        rnpvf_dbg("wait %d tx ready to 1\n", ring->rnpvf_queue_idx);
                } while ((status != 1) && (timeout < 100));

                        if (timeout >= 100)
                                printk("wait tx ready timeout\n");
                ring_wr32(ring, RNP_DMA_TX_START, 1);
        }


#if 0
	/* reinitialize flowdirector state */
	if (adapter->flags & RNP_FLAG_FDIR_HASH_CAPABLE) {
		ring->atr_sample_rate = adapter->atr_sample_rate;
		ring->atr_count = 0;
		set_bit(__RNP_TX_FDIR_INIT_DONE, &ring->state);
	} else {
		ring->atr_sample_rate = 0;
	}
	/* initialize XPS */
	if (!test_and_set_bit(__RNP_TX_XPS_INIT_DONE, &ring->state)) {
		struct rnpvf_q_vector *q_vector = ring->q_vector;

		if (q_vector)
			netif_set_xps_queue(adapter->netdev,
					    &q_vector->affinity_mask,
					    ring->queue_index);
	}

	clear_bit(__RNP_HANG_CHECK_ARMED, &ring->state);
#endif
	// enable queue
	//wr32(hw, RNP_DMA_TX_START(queue_idx), 1);
}

/**
 * rnpvf_configure_tx - Configure 82599 VF Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void rnpvf_configure_tx(struct rnpvf_adapter *adapter)
{
	u32 i;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < (adapter->num_tx_queues); i++)
		rnpvf_configure_tx_ring(adapter, adapter->tx_ring[i]);
}

#define RNP_SRRCTL_BSIZEHDRSIZE_SHIFT 2

__maybe_unused static void rnpvf_configure_srrctl(struct rnpvf_adapter *adapter, int index)
{
#if 0
	struct rnpvf_ring *rx_ring;
	struct rnpvf_hw *hw = &adapter->hw;
	u32 srrctl;
	rx_ring = &adapter->rx_ring[index];

	srrctl = RNP_SRRCTL_DROP_EN;

	srrctl |= RNP_SRRCTL_DESCTYPE_ADV_ONEBUF;

	srrctl |= ALIGN(rx_ring->rx_buf_len, 1024) >>
		  RNP_SRRCTL_BSIZEPKT_SHIFT;

	RNP_WRITE_REG(hw, RNP_VFSRRCTL(index), srrctl);
#endif
}
void rnpvf_disable_rx_queue(struct rnpvf_adapter *adapter,
		struct rnpvf_ring *ring)
{
	ring_wr32(ring, RNP_DMA_RX_START, 0);
}

void rnpvf_enable_rx_queue(struct rnpvf_adapter *adapter,
		struct rnpvf_ring *ring)
{
	ring_wr32(ring, RNP_DMA_RX_START, 1);
}

void rnpvf_configure_rx_ring(struct rnpvf_adapter *adapter,
		struct rnpvf_ring *ring)
{
	struct rnpvf_hw *hw = &adapter->hw;
	u64 desc_phy = ring->dma;

	/* disable queue to avoid issues while updating state */
	rnpvf_disable_rx_queue(adapter, ring);

	/* set descripts registers*/
	ring_wr32(ring, RNP_DMA_REG_RX_DESC_BUF_BASE_ADDR_LO, (u32)desc_phy);
	/* dma address high bits is used */
	ring_wr32(ring,
		 RNP_DMA_REG_RX_DESC_BUF_BASE_ADDR_HI,
		 ((u32)(desc_phy >> 32)) | (hw->vfnum << 24));
	ring_wr32(ring, RNP_DMA_REG_RX_DESC_BUF_LEN, ring->count);

	ring->tail = ring->ring_addr + RNP_DMA_REG_RX_DESC_BUF_TAIL;
	ring->next_to_clean = ring_rd32(ring, RNP_DMA_REG_RX_DESC_BUF_HEAD);
	ring->next_to_use = ring->next_to_clean;

#define SCATER_SIZE (96)
	if (ring->ring_flags & RNPVF_RING_SCATER_SETUP) {
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
		ring_wr32(ring, PCI_DMA_REG_RX_SCATTER_LENGH, SCATER_SIZE);
#else
		ring_wr32(ring, PCI_DMA_REG_RX_SCATTER_LENGH, ((ring->rx_buf_len + 15) >> 4));
#endif
	}

	ring_wr32(ring,
		 RNP_DMA_REG_RX_DESC_FETCH_CTRL,
		 0 | (TSRN10_RX_DEFAULT_LINE << 0)	   /*rx-desc-flow*/
		 | (TSRN10_RX_DEFAULT_BURST << 16) /*max-read-desc-cnt*/
	);

	if (ring->ring_flags & RNPVF_RING_IRQ_MISS_FIX)
		ring_wr32(ring, RNP_DMA_INT_TRIG, TX_INT_MASK | RX_INT_MASK);

	ring_wr32(ring,
		 RNP_DMA_REG_RX_INT_DELAY_TIMER,
		 adapter->rx_usecs * hw->usecstocount);
	ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_PKTCNT, adapter->rx_frames);

	rnpvf_alloc_rx_buffers(ring, rnpvf_desc_unused(ring));
	/* enable receive descriptor ring */
	//wr32(hw, RNP_DMA_RX_START(q_idx), 1);

}

static void rnpvf_set_rx_buffer_len(struct rnpvf_adapter *adapter)
{
        //struct rnp_hw *hw = &adapter->hw;
        struct net_device *netdev = adapter->netdev;
        int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN * 3;
        struct rnpvf_ring *rx_ring;
        int i;
        //u32 mhadd, hlreg0;
        // int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;

        if (max_frame < (ETH_FRAME_LEN + ETH_FCS_LEN))
                max_frame = (ETH_FRAME_LEN + ETH_FCS_LEN);

        for (i = 0; i < adapter->num_rx_queues; i++) {
                rx_ring = adapter->rx_ring[i];
#ifndef CONFIG_RNP_DISABLE_PACKET_SPLIT
                clear_bit(__RNPVF_RX_3K_BUFFER, &rx_ring->state);
                clear_bit(__RNPVF_RX_BUILD_SKB_ENABLED, &rx_ring->state);
#ifdef HAVE_SWIOTLB_SKIP_CPU_SYNC

                set_bit(__RNPVF_RX_BUILD_SKB_ENABLED, &rx_ring->state);
                //hw_dbg(hw, "set build skb\n");

//#if (PAGE_SIZE < 8192)
//              if (RNP_2K_TOO_SMALL_WITH_PADDING ||
//                  (max_frame > (ETH_FRAME_LEN + ETH_FCS_LEN)))
//                      ;
                        //      set_bit(__RNP_RX_3K_BUFFER, &rx_ring->state);
//#endif

#else /* !HAVE_SWIOTLB_SKIP_CPU_SYNC */
                /* fixed this */
                //hw_dbg(hw, "set construct skb\n");

#endif /* HAVE_SWIOTLB_SKIP_CPU_SYNC */

#ifdef OPTM_WITH_LPAGE
                rx_ring->rx_page_buf_nums = RNPVF_PAGE_BUFFER_NUMS(rx_ring);
                // we can fixed 2k ?
                rx_ring->rx_per_buf_mem = RNPVF_RXBUFFER_2K;
#endif

#else
                // should relative with mtu
		// fixme 
                rx_ring->rx_buf_len = max_frame;
#endif /* CONFIG_RNP_DISABLE_PACKET_SPLIT */
        }

#if 0
	struct rnpvf_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	int i;
	u16 rx_buf_len;

	/* notify the PF of our intent to use this size of frame */
	rnpvf_rlpml_set_vf(hw, max_frame);

	/* PF will allow an extra 4 bytes past for vlan tagged frames */
	max_frame += VLAN_HLEN;
#endif
}

/**
 * rnpvf_configure_rx - Configure 82599 VF Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void rnpvf_configure_rx(struct rnpvf_adapter *adapter)
{
	int i;

	/* set_rx_buffer_len must be called before ring initialization */
	rnpvf_set_rx_buffer_len(adapter);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	for (i = 0; i < adapter->num_rx_queues; i++)
		rnpvf_configure_rx_ring(adapter, adapter->rx_ring[i]);
}

#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef NETIF_F_HW_VLAN_CTAG_TX
static int rnpvf_vlan_rx_add_vid(struct net_device *netdev,
                               __always_unused __be16 proto, u16 vid)
#else /* !NETIF_F_HW_VLAN_CTAG_TX */
static int rnpvf_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
#endif /* NETIF_F_HW_VLAN_CTAG_TX */
#else /* !HAVE_INT_NDO_VLAN_RX_ADD_VID */
static void rnpvf_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
#endif /* HAVE_INT_NDO_VLAN_RX_ADD_VID */
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	struct rnp_mbx_info *mbx = &hw->mbx;
	int err = 0;

	if ((vid) && (adapter->vf_vlan) && (vid != adapter->vf_vlan)) {
		dev_err(&adapter->pdev->dev, "only 1 vlan for vf or pf set vlan already\n");
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
		//return -EACCES;
		// support qinq
		return 0;
#else
		return;
#endif
	}
	// vid zero nothing todo, only do this if not setup vlan before
	if ((vid) && (!adapter->vf_vlan)) {
		spin_lock_bh(&adapter->mbx_lock);
		set_bit(__RNPVF_MBX_POLLING, &adapter->state);
		/* add VID to filter table */
		err = hw->mac.ops.set_vfta(hw, vid, 0, true);
		clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
		spin_unlock_bh(&adapter->mbx_lock);
		adapter->vf_vlan = vid;
	}

	/* translate error return types so error makes sense */
	if (err == RNP_ERR_MBX) {
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID

		return -EIO;
#else
		return;
#endif
	}

	if (err == RNP_ERR_INVALID_ARGUMENT) {
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
		return -EACCES;
#else
		return;
#endif
	}
#ifndef HAVE_VLAN_RX_REGISTER
	set_bit(vid, adapter->active_vlans);
#endif

	if (vid) {

		hw->ops.set_veb_vlan(hw, vid, VFNUM(mbx, hw->vfnum));
		/*
		if (hw->dma_version >= 0x20201231) {
			int port;

			for (port = 0; port < 4; port++) {
				wr32(hw, RNP_DMA_PORT_VEB_VID_TBL(port, VFNUM(mbx, hw->vfnum)), vid);
			}
		} else {
			wr32(
					hw, RNP_DMA_PORT_VEB_VID_TBL(adapter->port, VFNUM(mbx, hw->vfnum)), vid);
		}
		*/
	}
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
	return err;
#endif
}

#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef NETIF_F_HW_VLAN_CTAG_RX
static int rnpvf_vlan_rx_kill_vid(struct net_device *netdev,
                __always_unused __be16 proto, u16 vid)
#else /* !NETIF_F_HW_VLAN_CTAG_RX */
static int rnpvf_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
#endif /* NETIF_F_HW_VLAN_CTAG_RX */
#else
static void rnpvf_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
#endif
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	struct rnp_mbx_info *mbx = &hw->mbx;
	int err = -EOPNOTSUPP;

	if ((vid) && (vid != adapter->vf_vlan)) {
		dev_err(&adapter->pdev->dev, "delete no valid vlan\n");
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
		return 0;
#else
		return;
#endif
	}
#ifdef HAVE_VLAN_RX_REGISTER
        if (!test_bit(__RNPVF_DOWN, &adapter->state))
                rnpvf_irq_disable(adapter);

        vlan_group_set_device(adapter->vlgrp, vid, NULL);

        if (!test_bit(__RNPVF_DOWN, &adapter->state))
                rnpvf_irq_enable(adapter);

#endif /* HAVE_VLAN_RX_REGISTER */

	if (vid) {
		spin_lock_bh(&adapter->mbx_lock);
		set_bit(__RNPVF_MBX_POLLING, &adapter->state);
		/* remove VID from filter table */
		err = hw->mac.ops.set_vfta(hw, vid, 0, false);
		clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
		spin_unlock_bh(&adapter->mbx_lock);
		adapter->vf_vlan = 0;
		// clean veb
		hw->ops.set_veb_vlan(hw, 0, VFNUM(mbx, hw->vfnum));
	}

#ifndef HAVE_VLAN_RX_REGISTER
	clear_bit(vid, adapter->active_vlans);
#endif

	/*
	if (hw->dma_version >= 0x20201231) {
		int port;

		for (port = 0; port < 4; port++) {
			wr32(hw, RNP_DMA_PORT_VEB_VID_TBL(port, VFNUM(mbx, hw->vfnum)), 0);
		}
	} else {
		wr32(hw,
			RNP_DMA_PORT_VEB_VID_TBL(adapter->port, VFNUM(mbx, hw->vfnum)),
			0);
	}*/
	/*
	for_each_set_bit(vid, adapter->active_vlans, VLAN_N_VID)
	{
		if (hw->dma_version >= 0x20201231) {
			int port;

			for (port = 0; port < 4; port++) {
				wr32(hw, RNP_DMA_PORT_VEB_VID_TBL(port, VFNUM(mbx, hw->vfnum)), vid);
			}
		} else {
			wr32(hw,
				 RNP_DMA_PORT_VEB_VID_TBL(adapter->port, VFNUM(mbx, hw->vfnum)),
				 vid);
		}
	}
	*/
	// FIXME
	//  remove is after rnpvf_down
	//  error after rnpvf_down(other irq is free)
#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
	return 0;
#endif
}
#endif
/**
 * rnpvf_vlan_strip_disable - helper to disable hw vlan stripping
 * @adapter: driver data
 */
__maybe_unused static void rnpvf_vlan_strip_disable(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;

	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	hw->mac.ops.set_vlan_strip(hw, false);
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);
}

/**
 * rnpvf_vlan_strip_enable - helper to enable hw vlan stripping
 * @adapter: driver data
 */
__maybe_unused static s32 rnpvf_vlan_strip_enable(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;
	int err;
	
	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	err = hw->mac.ops.set_vlan_strip(hw, true);
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);

	return err;
}

static void rnpvf_restore_vlan(struct rnpvf_adapter *adapter)
{
#ifndef HAVE_VLAN_RX_REGISTER
        u16 vid;
#endif

#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef NETIF_F_HW_VLAN_CTAG_TX
        rnpvf_vlan_rx_add_vid(adapter->netdev,
                               htons(ETH_P_8021Q), 0);
#else /* !NETIF_F_HW_VLAN_CTAG_TX */
        rnpvf_vlan_rx_add_vid(adapter->netdev, 0);
#endif /* NETIF_F_HW_VLAN_CTAG_TX */
#else /* !HAVE_INT_NDO_VLAN_RX_ADD_VID */
        rnpvf_vlan_rx_add_vid(adapter->netdev, 0);
#endif /* HAVE_INT_NDO_VLAN_RX_ADD_VID */

#ifndef HAVE_VLAN_RX_REGISTER
        for_each_set_bit(vid, adapter->active_vlans, VLAN_N_VID) {

#ifdef HAVE_INT_NDO_VLAN_RX_ADD_VID
#ifdef NETIF_F_HW_VLAN_CTAG_TX
                rnpvf_vlan_rx_add_vid(adapter->netdev,
                                htons(ETH_P_8021Q), vid);
#else /* !NETIF_F_HW_VLAN_CTAG_TX */
                rnpvf_vlan_rx_add_vid(adapter->netdev, vid);
#endif /* NETIF_F_HW_VLAN_CTAG_TX */
#else /* !HAVE_INT_NDO_VLAN_RX_ADD_VID */
                rnpvf_vlan_rx_add_vid(adapter->netdev, vid);
#endif /* HAVE_INT_NDO_VLAN_RX_ADD_VID */
        }
#endif /* HAVE_VLAN_RX_REGISTER */

	//for_each_set_bit(vid, adapter->active_vlans, VLAN_N_VID)
	//	rnpvf_vlan_rx_add_vid(adapter->netdev, htons(ETH_P_8021Q), vid);
}

static int rnpvf_write_uc_addr_list(struct net_device *netdev)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	int count = 0;

	if ((netdev_uc_count(netdev)) > 10) {
		pr_err("Too many unicast filters - No Space\n");
		return -ENOSPC;
	}

	if (!netdev_uc_empty(netdev)) {
		struct netdev_hw_addr *ha;

		netdev_for_each_uc_addr(ha, netdev)
		{
			spin_lock_bh(&adapter->mbx_lock);
			set_bit(__RNPVF_MBX_POLLING, &adapter->state);
			hw->mac.ops.set_uc_addr(hw, ++count, ha->addr);
			clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
			spin_unlock_bh(&adapter->mbx_lock);
			udelay(200);
		}
	} else {
		/*
		 * If the list is empty then send message to PF driver to
		 * clear all macvlans on this VF.
		 */
		spin_lock_bh(&adapter->mbx_lock);
		set_bit(__RNPVF_MBX_POLLING, &adapter->state);
		hw->mac.ops.set_uc_addr(hw, 0, NULL);
		clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
		spin_unlock_bh(&adapter->mbx_lock);
		udelay(200);
	}

	return count;
}

/**
 * rnpvf_set_rx_mode - Multicast and unicast set
 * @netdev: network interface device structure
 *
 * The set_rx_method entry point is called whenever the multicast address
 * list, unicast address list or the network interface flags are updated.
 * This routine is responsible for configuring the hardware for proper
 * multicast mode and configuring requested unicast filters.
 **/
static void rnpvf_set_rx_mode(struct net_device *netdev)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	netdev_features_t features = netdev->features;
#endif

	spin_lock_bh(&adapter->mbx_lock);
	/* reprogram multicast list */
	hw->mac.ops.update_mc_addr_list(hw, netdev);
	spin_unlock_bh(&adapter->mbx_lock);

	rnpvf_write_uc_addr_list(netdev);

	// setup vlan strip status
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	// in rx vlan on or pf set vf a vlan 
        if ((features & NETIF_F_HW_VLAN_CTAG_RX) || 
		(adapter->flags & RNPVF_FLAG_PF_SET_VLAN))
		rnpvf_vlan_strip_enable(adapter);
        else
		rnpvf_vlan_strip_disable(adapter);
#endif	

}

static void rnpvf_napi_enable_all(struct rnpvf_adapter *adapter)
{
	int q_idx;

	for (q_idx = 0; q_idx < adapter->num_q_vectors; q_idx++)
		napi_enable(&adapter->q_vector[q_idx]->napi);
}

static void rnpvf_napi_disable_all(struct rnpvf_adapter *adapter)
{
	int q_idx;

	for (q_idx = 0; q_idx < adapter->num_q_vectors; q_idx++)
		napi_disable(&adapter->q_vector[q_idx]->napi);
}

#if 0
static void rnpvf_configure_dcb(struct rnpvf_adapter *adapter)
{
}

static int rnpvf_hpbthresh(struct rnpvf_adapter *adapter, int pb)
{
}

static void rnpvf_pbthresh_setup(struct rnpvf_adapter *adapter)
{
}
static void rnpvf_pbthresh_setup(struct rnpvf_adapter *adapter)
{
}
#endif

static void rnpvf_configure_veb(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;
	struct rnp_mbx_info *mbx = &hw->mbx;
	u8 vfnum = VFNUM(mbx, hw->vfnum);
	u32 ring;
	u8 *mac;

	if (is_valid_ether_addr(hw->mac.addr))
		mac = hw->mac.addr;
	else
		mac = hw->mac.perm_addr;

	ring = adapter->rx_ring[0]->rnpvf_queue_idx;
	ring |= ((0x80 | vfnum) << 8);

	hw->ops.set_veb_mac(hw, mac, vfnum, ring);

}

static void rnpvf_configure(struct rnpvf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	rnpvf_set_rx_mode(netdev);

	rnpvf_restore_vlan(adapter);

	rnpvf_configure_tx(adapter);
	rnpvf_configure_rx(adapter);

	rnpvf_configure_veb(adapter);
}

#define RNP_MAX_RX_DESC_POLL 10

static void rnpvf_save_reset_stats(struct rnpvf_adapter *adapter)
{
	/* Only save pre-reset stats if there are some */
	if (adapter->stats.vfgprc || adapter->stats.vfgptc) {
		adapter->stats.saved_reset_vfgprc +=
			adapter->stats.vfgprc - adapter->stats.base_vfgprc;
		adapter->stats.saved_reset_vfgptc +=
			adapter->stats.vfgptc - adapter->stats.base_vfgptc;
		adapter->stats.saved_reset_vfgorc +=
			adapter->stats.vfgorc - adapter->stats.base_vfgorc;
		adapter->stats.saved_reset_vfgotc +=
			adapter->stats.vfgotc - adapter->stats.base_vfgotc;
		adapter->stats.saved_reset_vfmprc +=
			adapter->stats.vfmprc - adapter->stats.base_vfmprc;
	}
}

static void rnpvf_init_last_counter_stats(struct rnpvf_adapter *adapter)
{
#if 0
	struct rnpvf_hw *hw = &adapter->hw;

	adapter->stats.last_vfgprc = RNP_READ_REG(hw, RNP_VFGPRC);
#endif
}

__maybe_unused static void rnpvf_negotiate_api(struct rnpvf_adapter *adapter)
{
}

static void rnpvf_up_complete(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;
	int i;

	rnpvf_configure_msix(adapter);

	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);

	if (is_valid_ether_addr(hw->mac.addr))
		hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0);
	else
		hw->mac.ops.set_rar(hw, 0, hw->mac.perm_addr, 0);

	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);

	rnpvf_napi_enable_all(adapter);

	/*clear any pending interrupts*/
	rnpvf_irq_enable(adapter);

	/* enable transmits */
	netif_tx_start_all_queues(adapter->netdev);

	rnpvf_save_reset_stats(adapter);
	rnpvf_init_last_counter_stats(adapter);

	hw->mac.get_link_status = 1;
	mod_timer(&adapter->watchdog_timer, jiffies);

	// maybe pf call set vf mac
	//call_netdevice_notifiers(NETDEV_CHANGEADDR, adapter->netdev);
	clear_bit(__RNPVF_DOWN, &adapter->state);
	// open rx start at last
	for (i = 0; i < adapter->num_rx_queues; i++) {
		rnpvf_enable_rx_queue(adapter, adapter->rx_ring[i]);
	}
}

void rnpvf_reinit_locked(struct rnpvf_adapter *adapter)
{
	WARN_ON(in_interrupt());
	/* put off any impending NetWatchDogTimeout */
	// adapter->netdev->trans_start = jiffies;

	while (test_and_set_bit(__RNPVF_RESETTING, &adapter->state))
		usleep_range(1000, 2000);

	rnpvf_down(adapter);

	rnpvf_reset(adapter);

	rnpvf_up(adapter);

	clear_bit(__RNPVF_RESETTING, &adapter->state);
}

void rnpvf_up(struct rnpvf_adapter *adapter)
{

	// rnpvf_negotiate_api(adapter);

	// rnpvf_reset_queues(adapter);

	rnpvf_configure(adapter);

	rnpvf_up_complete(adapter);
}

void rnpvf_reset(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;

	if (hw->mac.ops.reset_hw(hw))
		hw_dbg(hw, "PF still resetting\n");
	else
		hw->mac.ops.init_hw(hw);

	if (is_valid_ether_addr(adapter->hw.mac.addr)) {
		eth_hw_addr_set(netdev, adapter->hw.mac.addr);
		//memcpy(netdev->dev_addr, adapter->hw.mac.addr, netdev->addr_len);
		memcpy(netdev->perm_addr, adapter->hw.mac.addr, netdev->addr_len);
	}
}

/**
 * rnpvf_clean_rx_ring - Free Rx Buffers per Queue
 * @adapter: board private structure
 * @rx_ring: ring to free buffers from
 **/
 #if 0
static void rnpvf_clean_rx_ring(struct rnpvf_adapter *adapter,
								struct rnpvf_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	unsigned long size;
	u16 i;

	BUG_ON(rx_ring == NULL);

	/* ring already cleared, nothing to do */
	if (!rx_ring->rx_buffer_info)
		return;

	/* Free all the Rx ring sk_buffs */
	for (i = 0; i < rx_ring->count; i++) {
		struct rnpvf_rx_buffer *rx_buffer;

		rx_buffer = &rx_ring->rx_buffer_info[i];
		if (rx_buffer->skb) {
			struct sk_buff *skb = rx_buffer->skb;
			if (RNPVF_CB(skb)->page_released) {
				dma_unmap_page(dev,
							   RNPVF_CB(skb)->dma,
							   rnpvf_rx_bufsz(rx_ring),
							   DMA_FROM_DEVICE);
				RNPVF_CB(skb)->page_released = false;
			}
			dev_kfree_skb(skb);
		}
		rx_buffer->skb = NULL;
		if (rx_buffer->dma)
			dma_unmap_page(dev,
						   rx_buffer->dma,
						   rnpvf_rx_pg_size(rx_ring),
						   DMA_FROM_DEVICE);
		rx_buffer->dma = 0;
		if (rx_buffer->page)
			__free_pages(rx_buffer->page, rnpvf_rx_pg_order(rx_ring));
		rx_buffer->page = NULL;
	}

	size = sizeof(struct rnpvf_rx_buffer) * rx_ring->count;
	memset(rx_ring->rx_buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_alloc = 0;
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
}
#endif
/**
 * rnpvf_clean_tx_ring - Free Tx Buffers
 * @adapter: board private structure
 * @tx_ring: ring to be cleaned
 **/
static void rnpvf_clean_tx_ring(struct rnpvf_adapter *adapter,
								struct rnpvf_ring *tx_ring)
{
	struct rnpvf_tx_buffer *tx_buffer_info;
	unsigned long size;
	u16 i;

	BUG_ON(tx_ring == NULL);

	/* ring already cleared, nothing to do */
	if (!tx_ring->tx_buffer_info)
		return;

	/* Free all the Tx ring sk_buffs */
	for (i = 0; i < tx_ring->count; i++) {
		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		rnpvf_unmap_and_free_tx_resource(tx_ring, tx_buffer_info);
	}

	netdev_tx_reset_queue(txring_txq(tx_ring));

	size = sizeof(struct rnpvf_tx_buffer) * tx_ring->count;
	memset(tx_ring->tx_buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
}

/**
 * rnpvf_clean_all_rx_rings - Free Rx Buffers for all queues
 * @adapter: board private structure
 **/
static void rnpvf_clean_all_rx_rings(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		rnpvf_clean_rx_ring(adapter->rx_ring[i]);
}

/**
 * rnpvf_clean_all_tx_rings - Free Tx Buffers for all queues
 * @adapter: board private structure
 **/
static void rnpvf_clean_all_tx_rings(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		rnpvf_clean_tx_ring(adapter, adapter->tx_ring[i]);
}

__maybe_unused static void rnpvf_fdir_filter_exit(struct rnpvf_adapter *adapter)
{
}

void rnpvf_down(struct rnpvf_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int i;

	/* signal that we are down to the interrupt handler */
	set_bit(__RNPVF_DOWN, &adapter->state);
	set_bit(__RNPVF_LINK_DOWN, &adapter->state);

	/* disable all enabled rx queues */
	for (i = 0; i < adapter->num_rx_queues; i++)
		rnpvf_disable_rx_queue(adapter, adapter->rx_ring[i]);

	usleep_range(1000, 2000);

	netif_tx_stop_all_queues(netdev);

	/* call carrier off first to avoid false dev_watchdog timeouts */
	netif_carrier_off(netdev);

	netif_tx_disable(netdev);

	rnpvf_irq_disable(adapter);

	rnpvf_napi_disable_all(adapter);

	// del_timer_sync(&adapter->watchdog_timer);
	/* can't call flush scheduled work here because it can deadlock
	 * if linkwatch_event tries to acquire the rtnl_lock which we are
	 * holding */
	//while (adapter->flags & RNPVF_FLAG_IN_WATCHDOG_TASK)
	//	msleep(1);
	// should wait tx head == tail
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct rnpvf_ring *tx_ring = adapter->tx_ring[i];

		if (!(tx_ring->ring_flags & RNPVF_RING_SKIP_TX_START)) {
			//u64 tx_next_to_use;
			//u64 tx_next_to_clean;
			int head, tail;
			int timeout = 0;

			head = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_HEAD);
			tail = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_TAIL);

			//tx_next_to_clean = tx_ring->next_to_clean;
			//tx_next_to_use = tx_ring->next_to_use;

			//while (tx_next_to_clean != tx_next_to_use) {
			while (head != tail) {
				//printk("should wait all tx done %d \n", tx_ring->queue_index);
				usleep_range(10000, 20000);

				head = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_HEAD);
				tail = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_TAIL);
				//tx_next_to_clean = tx_ring->next_to_clean;
				//tx_next_to_use = tx_ring->next_to_use;
				timeout++;
				if (timeout >= 100) {
					printk("vf wait tx done timeout\n");
					break;
				}
			}
			/*
			   if (timeout >= 100) { 
			//head = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_HEAD);
			//tail = ring_rd32(tx_ring, RNP_DMA_REG_TX_DESC_BUF_TAIL);
			} */
		}
	} 

	/* disable transmits in the hardware now that interrupts are off */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct rnpvf_ring *tx_ring = adapter->tx_ring[i];

		if (!(tx_ring->ring_flags & RNPVF_RING_SKIP_TX_START))
			ring_wr32(tx_ring, RNP_DMA_TX_START, 0);
	}

	netif_carrier_off(netdev);

	/* why send reset to pf ? */
	//if (!pci_channel_offline(adapter->pdev))
		//rnpvf_reset(adapter);

	rnpvf_clean_all_tx_rings(adapter);
	rnpvf_clean_all_rx_rings(adapter);
}

#ifdef HAVE_NDO_SET_FEATURES
#ifdef HAVE_RHEL6_NET_DEVICE_OPS_EXT
static u32 rnpvf_fix_features(struct net_device *netdev, u32 features)
#else
static netdev_features_t rnpvf_fix_features(struct net_device *netdev,
                                          netdev_features_t features)
#endif
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

	/* If Rx checksum is disabled, then RSC/LRO should also be disabled */
	if (!(features & NETIF_F_RXCSUM))
		features &= ~NETIF_F_LRO;
	/* vf not support change vlan filter */
#ifdef NETIF_F_HW_VLAN_CTAG_FILTER
	if ((netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER) !=
		(features & NETIF_F_HW_VLAN_CTAG_FILTER)) {
		if (netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER)
			features |= NETIF_F_HW_VLAN_CTAG_FILTER;
		else
			features &= ~NETIF_F_HW_VLAN_CTAG_FILTER;
	}
#endif
	if (adapter->flags & RNPVF_FLAG_PF_SET_VLAN) {
		// if in this mode , close tx/rx vlan offload
#ifdef NETIF_F_HW_VLAN_CTAG_RX
		features &= ~NETIF_F_HW_VLAN_CTAG_RX;
#endif
#ifdef NETIF_F_HW_VLAN_CTAG_TX
		features &= ~NETIF_F_HW_VLAN_CTAG_TX;
#endif

	}
	return features;
}

#ifdef HAVE_RHEL6_NET_DEVICE_OPS_EXT
static int rnpvf_set_features(struct net_device *netdev, u32 features)
#else
static int rnpvf_set_features(struct net_device *netdev,
                            netdev_features_t features)
#endif
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	netdev_features_t changed = netdev->features ^ features;
#endif
	bool need_reset = false;
	int err = 0;

	netdev->features = features;
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	if (changed & NETIF_F_HW_VLAN_CTAG_RX) {
		if (features & NETIF_F_HW_VLAN_CTAG_RX) {
			if ((!rnpvf_vlan_strip_enable(adapter)))
				features &= ~NETIF_F_HW_VLAN_CTAG_RX;
		} else {
			rnpvf_vlan_strip_disable(adapter);
		}
	}
#endif

	netdev->features = features;

	if (need_reset)
		rnpvf_reset(adapter);

	return err;
}
#endif
/**
 * rnpvf_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/
__maybe_unused static void rnpvf_tx_timeout(struct net_device *netdev)
{
	//struct rnpvf_adapter *adapter = netdev_priv(netdev);

	// todo 
	/* Do the reset outside of interrupt context */
	//schedule_work(&adapter->reset_task);
}


/**
 * rnpvf_sw_init - Initialize general software structures
 * (struct rnpvf_adapter)
 * @adapter: board private structure to initialize
 *
 * rnpvf_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int rnpvf_sw_init(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	struct net_device *netdev = adapter->netdev;
	int err;

	/* PCI config space info */
	hw->pdev = pdev;

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	hw->mbx.ops.init_params(hw);

	/* assume legacy case in which PF would only give VF 2 queues */
	/*
	}*/

	

	/*initialization default pause flow */
	hw->fc.requested_mode = rnp_fc_none;
	hw->fc.current_mode = rnp_fc_none;

	/* now vf other irq handler is not regist */
	err = hw->mac.ops.reset_hw(hw);
	if (err) {
		dev_info(&pdev->dev,
				"PF still in reset state.  Is the PF interface up?\n");
		hw->adapter_stopped = false;
		hw->link = false;
		hw->speed = 0;
		hw->usecstocount = 500;
		return err;
	} else {
		err = hw->mac.ops.init_hw(hw);
		if (err) {
			pr_err("init_shared_code failed: %d\n", err);
			goto out;
		}
		err = hw->mac.ops.get_mac_addr(hw, hw->mac.addr);
		if (err)
			dev_info(&pdev->dev, "Error reading MAC address\n");
		else if (is_zero_ether_addr(adapter->hw.mac.addr))
			dev_info(&pdev->dev,
					 "MAC address not assigned by administrator.\n");
		eth_hw_addr_set(netdev, hw->mac.addr);
		//memcpy(netdev->dev_addr, hw->mac.addr, netdev->addr_len);
	}

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		dev_info(&pdev->dev, "Assigning random MAC address\n");
		eth_hw_addr_random(netdev);
		memcpy(hw->mac.addr, netdev->dev_addr, netdev->addr_len);
	}
	/* get info from pf */
	err = hw->mac.ops.get_queues(hw);
	if (err) {
		// fixme in n500?
		dev_info(&pdev->dev,
				"Get queue info error, use default one \n");
		hw->mac.max_tx_queues = MAX_TX_QUEUES;
		hw->mac.max_rx_queues = MAX_RX_QUEUES;
		/*
		if (hw->mode == MODE_NIC_MODE_8PORT_10G) {
			hw->queue_ring_base = (hw->vfnum & VF_NUM_MASK) * MAX_RX_QUEUES +
								  adapter->bd_number; // FIXME
		} else if (hw->mode == MODE_NIC_MODE_4PORT_10G) {
			hw->queue_ring_base = (hw->vfnum & VF_NUM_MASK) * MAX_RX_QUEUES +
								  adapter->bd_number; // FIXME
		} else {
		*/
		hw->queue_ring_base = (hw->vfnum & VF_NUM_MASK) * MAX_RX_QUEUES;

		//}
	}

	dev_info(&pdev->dev, "queue_ring_base %d num %d\n", hw->queue_ring_base, hw->mac.max_tx_queues);
	err = hw->mac.ops.get_mtu(hw);
	if (err) {
		dev_info(&pdev->dev, "Get mtu error ,use default one\n");
		hw->mtu = 1500;
	}
	/* lock to protect mailbox accesses */
	spin_lock_init(&adapter->mbx_lock);

	/* Enable dynamic interrupt throttling rates */
	// adapter->rx_itr_setting = 1;
	// adapter->tx_itr_setting = 1;

	/* set default ring sizes */
	adapter->tx_ring_item_count = hw->tx_items_count;
	adapter->rx_ring_item_count = hw->rx_items_count;
	adapter->dma_channels = min_t(int, hw->mac.max_tx_queues, hw->mac.max_rx_queues);
	DPRINTK(PROBE, INFO, "tx parameters %d, rx parameters %d\n", 
			adapter->tx_ring_item_count,
			adapter->rx_ring_item_count);

	/* set default tx/rx soft count */
        adapter->adaptive_rx_coal = 1;
        adapter->adaptive_tx_coal = 1;
	adapter->napi_budge = RNPVF_DEFAULT_RX_WORK;
	adapter->tx_work_limit = RNPVF_DEFAULT_TX_WORK;
	adapter->rx_usecs = RNPVF_PKT_TIMEOUT;
	adapter->rx_frames = RNPVF_RX_PKT_POLL_BUDGET;
	adapter->tx_usecs = RNPVF_PKT_TIMEOUT_TX;
	adapter->tx_frames = RNPVF_TX_PKT_POLL_BUDGET;

	set_bit(__RNPVF_DOWN, &adapter->state);
	return 0;

out:
	return err;
}

static int rnpvf_acquire_msix_vectors(struct rnpvf_adapter *adapter,
									  int vectors)
{
	int err = 0;
	int vector_threshold;

	/* We'll want at least 2 (vector_threshold):
	 * 1) TxQ[0] + RxQ[0] handler
	 * 2) Other (Link Status Change, etc.)
	 */
	vector_threshold = MIN_MSIX_COUNT;

	/* The more we get, the more we will assign to Tx/Rx Cleanup
	 * for the separate queues...where Rx Cleanup >= Tx Cleanup.
	 * Right now, we simply care about how many we'll get; we'll
	 * set them up later while requesting irq's.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	err = pci_enable_msix_range(
		adapter->pdev, adapter->msix_entries, vectors, vectors);
	if (err > 0) { /* Success or a nasty failure. */
		vectors = err;
		err = 0;
	}
#else
	err = pci_enable_msix(adapter->pdev, adapter->msix_entries, vectors);
#endif
	DPRINTK(PROBE, INFO, "err:%d, vectors:%d\n",err, vectors);
	if (err < 0) {
		dev_err(&adapter->pdev->dev, "Unable to allocate MSI-X interrupts\n");
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else {
		/*
		 * Adjust for only the vectors we'll use, which is minimum
		 * of max_msix_q_vectors + NON_Q_VECTORS, or the number of
		 * vectors we were allocated.
		 */
		adapter->num_msix_vectors = vectors;
	}

	return err;
}

/**
 * rnpvf_set_num_queues - Allocate queues for device, feature dependent
 * @adapter: board private structure to initialize
 *
 * This is the top level queue allocation routine.  The order here is very
 * important, starting with the "most" number of features turned on at once,
 * and ending with the smallest set of features.  This way large combinations
 * can be allocated if they're turned on, and smaller combinations are the
 * fallthrough conditions.
 *
 **/
static void rnpvf_set_num_queues(struct rnpvf_adapter *adapter)
{
	/* Start with base case */
	adapter->num_rx_queues = adapter->dma_channels;
	adapter->num_tx_queues = adapter->dma_channels; 
}

/**
 * rnpvf_set_interrupt_capability - set MSI-X or FAIL if not supported
 * @adapter: board private structure to initialize
 *
 * Attempt to configure the interrupts using the best available
 * capabilities of the hardware and the kernel.
 **/
static int rnpvf_set_interrupt_capability(struct rnpvf_adapter *adapter)
{
	int err = 0;
	int vector, v_budget;
	int irq_mode_back = adapter->irq_mode;
	/*
	 * It's easy to be greedy for MSI-X vectors, but it really
	 * doesn't do us much good if we have a lot more vectors
	 * than CPU's.  So let's be conservative and only ask for
	 * (roughly) the same number of vectors as there are CPU's.
	 * The default is to use pairs of vectors.
	 */
	v_budget = max(adapter->num_rx_queues, adapter->num_tx_queues);
	v_budget = min_t(int, v_budget, num_online_cpus());
	v_budget += NON_Q_VECTORS;
	v_budget = min_t(int, v_budget, MAX_MSIX_VECTORS);


	if (adapter->irq_mode == irq_mode_msix) {
		/* A failure in MSI-X entry allocation isn't fatal, but it does
		 * mean we disable MSI-X capabilities of the adapter. */
		adapter->msix_entries =
			kcalloc(v_budget, sizeof(struct msix_entry), GFP_KERNEL);
		if (!adapter->msix_entries) {
			err = -ENOMEM;
			goto out;
		}

		for (vector = 0; vector < v_budget; vector++)
			adapter->msix_entries[vector].entry = vector;

		err = rnpvf_acquire_msix_vectors(adapter, v_budget);
		if (!err) {
			adapter->vector_off = NON_Q_VECTORS;
			adapter->num_q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
			DPRINTK(PROBE, INFO, "adapter%d alloc vectors: cnt:%d [%d~%d] num_msix_vectors:%d\n",
					adapter->bd_number,
					v_budget,
					adapter->vector_off,
					adapter->vector_off + v_budget - 1,
					adapter->num_msix_vectors);
			adapter->flags |= RNPVF_FLAG_MSIX_ENABLED;
			goto out; 
		}
		kfree(adapter->msix_entries);

		if (adapter->flags & RNPVF_FLAG_MSI_CAPABLE) {
			adapter->irq_mode = irq_mode_msi;
			pr_info("acquire msix failed, try to use msi\n");
		}

	} else {
		pr_info("adapter not in msix mode\n");

	}

        // if has msi capability or set irq_mode
        if (adapter->irq_mode == irq_mode_msi) {
                err = pci_enable_msi(adapter->pdev);
                if (err) {
                        pr_info("Failed to allocate MSI interrupt, falling back to legacy. Error");
                } else {
                        /* msi mode use only 1 irq */
                        adapter->flags |= RNPVF_FLAG_MSI_ENABLED;
                }

        }
        /* write back origin irq_mode */
        adapter->irq_mode = irq_mode_back;
        /* legacy and msi only 1 vectors */
        adapter->num_q_vectors = 1;

out:
	return err;
}

static void rnpvf_add_ring(struct rnpvf_ring *ring,
		struct rnpvf_ring_container *head)
{
	ring->next = head->ring;
	head->ring = ring;
	head->count++;
}

static enum hrtimer_restart irq_miss_check(struct hrtimer *hrtimer)
{
	struct rnpvf_q_vector *q_vector;
	struct rnpvf_ring *ring;
	struct rnp_tx_desc *eop_desc;
	struct rnpvf_adapter *adapter;
	struct rnpvf_hw *hw;

	int tx_next_to_clean;
	int tx_next_to_use;

	struct rnpvf_tx_buffer *tx_buffer;
	union rnp_rx_desc *rx_desc;

	q_vector = container_of(hrtimer, struct rnpvf_q_vector, irq_miss_check_timer);
	adapter = q_vector->adapter;
	hw = &adapter->hw;

	/* If we're already down or resetting, just bail */
	if (test_bit(__RNPVF_DOWN, &adapter->state) ||
			test_bit(__RNPVF_RESETTING, &adapter->state))
		goto do_self_napi;

	rnpvf_irq_disable_queues(q_vector);
	//check down desc ?
	/*
	   rnp_for_each_ring(ring, q_vector->tx)
	   rnp_wr_reg(ring->dma_int_mask, (RX_INT_MASK | TX_INT_MASK));
	   */
	// check tx irq miss
	rnpvf_for_each_ring(ring, q_vector->tx) {
		tx_next_to_clean = ring->next_to_clean;
		tx_next_to_use = ring->next_to_use;
		// have work to do
		if (tx_next_to_use != tx_next_to_clean) {
			tx_buffer = &ring->tx_buffer_info[tx_next_to_clean];
			eop_desc = tx_buffer->next_to_watch;
			// have tx done 
			if (eop_desc) {
				if ((eop_desc->cmd & cpu_to_le16(RNP_TXD_STAT_DD))) {
					if (q_vector->new_rx_count != q_vector->old_rx_count) {
						ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_PKTCNT, q_vector->new_rx_count);
						//ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_TIMER,
						//              q_vector->new_usesc * 500);
						q_vector->old_rx_count = q_vector->new_rx_count;
					}
					// close irq 
					// printk("call irq self\n");
					napi_schedule_irqoff(&q_vector->napi);
					goto do_self_napi;
				}
			}
		}
	}

	//check rx irq
	rnpvf_for_each_ring(ring, q_vector->rx) {
		rx_desc = RNPVF_RX_DESC(ring, ring->next_to_clean);

		if (rx_desc) {
			if (rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_DD)) {
				// should check len 
				unsigned int size;

				size = le16_to_cpu(rx_desc->wb.len)- le16_to_cpu(rx_desc->wb.padding_len);

				if (size) {
					if (q_vector->new_rx_count != q_vector->old_rx_count) {
						ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_PKTCNT, q_vector->new_rx_count);
						//ring_wr32(ring, RNP_DMA_REG_RX_INT_DELAY_TIMER,
						//              q_vector->new_usesc * 500);
						q_vector->old_rx_count = q_vector->new_rx_count;
					}
					napi_schedule_irqoff(&q_vector->napi);
				} else {
					adapter->flags |= RNPVF_FLAG_PF_RESET_REQ;
					// send pf reset mbx
				}
				goto do_self_napi;
			}
		}
	}
	rnpvf_irq_enable_queues(q_vector);
do_self_napi:
	return HRTIMER_NORESTART;
}


static int rnpvf_alloc_q_vector(struct rnpvf_adapter *adapter,
		int eth_queue_idx,
		int rnpvf_vector,
		int rnpvf_queue,
		int r_count,
		int step)
{
	struct rnpvf_q_vector *q_vector;
	struct rnpvf_ring *ring;
	struct rnpvf_hw *hw = &adapter->hw;
	int node = NUMA_NO_NODE;
	int cpu = -1;
	int ring_count, size;
	int txr_count, rxr_count, idx;
	int rxr_idx = rnpvf_queue, txr_idx = rnpvf_queue;

	DPRINTK(PROBE, INFO, "eth_queue_idx:%d rnpvf_vector:%d(off:%d) ring:%d "
			  "ring_cnt:%d, step:%d\n",
			  eth_queue_idx,
			  rnpvf_vector,
			  adapter->vector_off,
			  rnpvf_queue,
			  r_count,
			  step);

	txr_count = rxr_count = r_count;

	ring_count = txr_count + rxr_count;
	size = sizeof(struct rnpvf_q_vector) +
		   (sizeof(struct rnpvf_ring) * ring_count);

	if (cpu_online(rnpvf_vector)) {
		cpu = rnpvf_vector;
		node = cpu_to_node(cpu);
	}

	/* allocate q_vector and rings */
	q_vector = kzalloc_node(size, GFP_KERNEL, node);
	if (!q_vector)
		q_vector = kzalloc(size, GFP_KERNEL);
	if (!q_vector)
		return -ENOMEM;

	/* setup affinity mask and node */
	if (cpu != -1)
		cpumask_set_cpu(cpu, &q_vector->affinity_mask);
	q_vector->numa_node = node;

	
	netif_napi_add(
		adapter->netdev, &q_vector->napi, rnpvf_poll, adapter->napi_budge);

	/* tie q_vector and adapter together */
	adapter->q_vector[rnpvf_vector - adapter->vector_off] = q_vector;
	q_vector->adapter = adapter;
	q_vector->v_idx = rnpvf_vector;

	/* initialize pointer to rings */
	ring = q_vector->ring;

	for (idx = 0; idx < txr_count; idx++) {
		/* assign generic ring traits */
		ring->dev = &adapter->pdev->dev;
		ring->netdev = adapter->netdev;

		/* configure backlink on ring */
		ring->q_vector = q_vector;

		/* update q_vector Tx values */
		rnpvf_add_ring(ring, &q_vector->tx);

		/* apply Tx specific ring traits */
		ring->count = adapter->tx_ring_item_count;
		ring->queue_index = eth_queue_idx + idx;
		ring->rnpvf_queue_idx = txr_idx;

                if (hw->board_type == rnp_board_n10) {
                        ring->ring_flags |= RNPVF_RING_SKIP_TX_START;
			ring->ring_addr = hw->hw_addr + RNP_RING_BASE_N10 + RNP_RING_OFFSET(txr_idx);
			ring->rnpvf_msix_off = txr_idx;
                }
                if (hw->board_type == rnp_board_n500) {
                        /* n500 not support tunnel */
                        ring->ring_flags |= RNPVF_RING_NO_TUNNEL_SUPPORT;
                        /* n500 fixed ring size change from large to small */
                        ring->ring_flags |= RNPVF_RING_SIZE_CHANGE_FIX;
			ring->ring_flags |= RNPVF_RING_CHKSM_FIX;
			/* n500 vf use this */
			ring->ring_addr = hw->hw_addr + RNP_RING_BASE_N500;
			ring->ring_flags |= RNPVF_RING_VEB_MULTI_FIX;
			ring->rnpvf_msix_off = 0;
                }
		ring->dma_int_stat = ring->ring_addr + RNP_DMA_INT_STAT;
		//ring->hw_addr = hw->hw_addr;
		ring->dma_int_mask = ring->dma_int_stat + 4;
		ring->dma_int_clr = ring->dma_int_stat + 8;
		ring->device_id = adapter->pdev->device;

		ring->vfnum = hw->vfnum;


		/* assign ring to adapter */
		adapter->tx_ring[ring->queue_index] = ring;
		dbg("adapter->tx_ringp[%d] <= %p\n", ring->queue_index, ring);

		/* update count and index */
		txr_idx += step;

		DPRINTK(PROBE, INFO, "vector[%d] <--RNP TxRing:%d, eth_queue:%d\n",
				  rnpvf_vector,
				  ring->rnpvf_queue_idx,
				  ring->queue_index);

		/* push pointer to next ring */
		ring++;
	}

	for (idx = 0; idx < rxr_count; idx++) {
		/* assign generic ring traits */
		ring->dev = &adapter->pdev->dev;
		ring->netdev = adapter->netdev;

		/* configure backlink on ring */
		ring->q_vector = q_vector;

		/* update q_vector Rx values */
		rnpvf_add_ring(ring, &q_vector->rx);

		/* apply Rx specific ring traits */
		ring->count = adapter->rx_ring_item_count;
		ring->queue_index = eth_queue_idx + idx;
		ring->rnpvf_queue_idx = rxr_idx;

                if (hw->board_type == rnp_board_n10) {
			ring->ring_addr = hw->hw_addr + RNP_RING_BASE_N10 + RNP_RING_OFFSET(rxr_idx);
			ring->rnpvf_msix_off = rxr_idx;

                } else if (hw->board_type == rnp_board_n500) {
                        /* n500 fixed ring size change from large to small */
                        ring->ring_flags |= RNPVF_RING_SIZE_CHANGE_FIX;
                        ring->ring_flags |= RNPVF_RING_SCATER_SETUP;
                        ring->ring_flags |= RNPVF_RING_NO_TUNNEL_SUPPORT;
                        ring->ring_flags |= RNPVF_RING_STAGS_SUPPORT;
			ring->ring_flags |= RNPVF_RING_VEB_MULTI_FIX;
			ring->ring_flags |= RNPVF_RING_IRQ_MISS_FIX;
			ring->ring_addr = hw->hw_addr + RNP_RING_BASE_N500;
			ring->rnpvf_msix_off = 0;
                }
		ring->dma_int_stat = ring->ring_addr + RNP_DMA_INT_STAT;
		ring->dma_int_mask = ring->dma_int_stat + 4;
		ring->dma_int_clr = ring->dma_int_stat + 8;
		ring->device_id = adapter->pdev->device;
		ring->vfnum = hw->vfnum;



		/* assign ring to adapter */
		adapter->rx_ring[ring->queue_index] = ring;
		DPRINTK(PROBE, INFO, "vector[%d] <--RNP RxRing:%d, eth_queue:%d\n",
				  rnpvf_vector,
				  ring->rnpvf_queue_idx,
				  ring->queue_index);

		/* update count and index */
		rxr_idx += step;

		/* push pointer to next ring */
		ring++;
	}

	if (hw->board_type == rnp_board_n10) {
		q_vector->vector_flags |= RNPVF_QVECTOR_FLAG_IRQ_MISS_CHECK;
		q_vector->vector_flags |= RNPVF_QVECTOR_FLAG_REDUCE_TX_IRQ_MISS;
		/* initialize timer */
		q_vector->irq_check_usecs = 1000;
		hrtimer_init(&q_vector->irq_miss_check_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		q_vector->irq_miss_check_timer.function = irq_miss_check; /* initialize NAPI */
	} else if (hw->board_type == rnp_board_n500) {
		q_vector->vector_flags |= RNPVF_QVECTOR_FLAG_ITR_FEATURE;


	}

	return 0;
}

static void rnpvf_free_q_vector(struct rnpvf_adapter *adapter, int v_idx)
{
	struct rnpvf_q_vector *q_vector;
	struct rnpvf_ring *ring;

	dbg("v_idx:%d\n", v_idx);

	q_vector = adapter->q_vector[v_idx];

	rnpvf_for_each_ring(ring, q_vector->tx)
		adapter->tx_ring[ring->queue_index] = NULL;

	rnpvf_for_each_ring(ring, q_vector->rx)
		adapter->rx_ring[ring->queue_index] = NULL;

	adapter->q_vector[v_idx] = NULL;
	netif_napi_del(&q_vector->napi);

	if (q_vector->vector_flags & RNPVF_QVECTOR_FLAG_IRQ_MISS_CHECK)
		rnpvf_htimer_stop(q_vector);


	/*
	 * rnpvf_get_stats64() might access the rings on this vector,
	 * we must wait a grace period before freeing it.
	 */
	kfree_rcu(q_vector, rcu);
}

/**
 * rnpvf_alloc_q_vectors - Allocate memory for interrupt vectors
 * @adapter: board private structure to initialize
 *
 * We allocate one q_vector per queue interrupt.  If allocation fails we
 * return -ENOMEM.
 **/
static int rnpvf_alloc_q_vectors(struct rnpvf_adapter *adapter)
{
	int vector_idx = adapter->vector_off;
	int ring_idx = adapter->hw.queue_ring_base;
	int ring_remaing =
		min_t(int, adapter->num_tx_queues, adapter->num_rx_queues);
	int ring_step = 1; // only 2port support
	int err, ring_cnt,
		vector_remaing = adapter->num_msix_vectors - NON_Q_VECTORS;
	int eth_queue_idx = 0;

	BUG_ON(ring_remaing == 0);
	BUG_ON(vector_remaing == 0);

	//rnpvf_dbg("queue_ring_base is %d\n", ring_idx);

	for (; ring_remaing > 0 && vector_remaing > 0; vector_remaing--) {
		ring_cnt = DIV_ROUND_UP(ring_remaing, vector_remaing);

		err = rnpvf_alloc_q_vector(
			adapter, eth_queue_idx, vector_idx, ring_idx, ring_cnt, ring_step);
		if (err)
			goto err_out;

		ring_idx += ring_step * ring_cnt;
		ring_remaing -= ring_cnt;
		vector_idx++;
		eth_queue_idx += ring_cnt;
	}

	return 0;

err_out:
	vector_idx -= adapter->vector_off;
	while (vector_idx--)
		rnpvf_free_q_vector(adapter, vector_idx);
	return -ENOMEM;
}

/**
 * rnpvf_free_q_vectors - Free memory allocated for interrupt vectors
 * @adapter: board private structure to initialize
 *
 * This function frees the memory allocated to the q_vectors.  In addition if
 * NAPI is enabled it will delete any references to the NAPI struct prior
 * to freeing the q_vector.
 **/
static void rnpvf_free_q_vectors(struct rnpvf_adapter *adapter)
{
	int i, v_idx = adapter->num_q_vectors;

	adapter->num_rx_queues = 0;
	adapter->num_tx_queues = 0;
	adapter->num_q_vectors = 0;

	for (i = 0; i < v_idx; i++)
		rnpvf_free_q_vector(adapter, i);
}

/**
 * rnpvf_reset_interrupt_capability - Reset MSIX setup
 * @adapter: board private structure
 *
 **/
static void rnpvf_reset_interrupt_capability(struct rnpvf_adapter *adapter)
{
	if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
		pci_disable_msix(adapter->pdev);
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else if (adapter->flags & RNPVF_FLAG_MSI_ENABLED) {
		pci_disable_msi(adapter->pdev);

	}
}

/**
 * rnpvf_init_interrupt_scheme - Determine if MSIX is supported and init
 * @adapter: board private structure to initialize
 *
 **/
int rnpvf_init_interrupt_scheme(struct rnpvf_adapter *adapter)
{
	int err;

	/* Number of supported queues */
	rnpvf_set_num_queues(adapter);

	err = rnpvf_set_interrupt_capability(adapter);
	if (err) {
		hw_dbg(&adapter->hw, "Unable to setup interrupt capabilities\n");
		goto err_set_interrupt;
	}

	err = rnpvf_alloc_q_vectors(adapter);
	if (err) {
		hw_dbg(&adapter->hw,
			   "Unable to allocate memory for queue "
			   "vectors\n");
		goto err_alloc_q_vectors;
	}

	hw_dbg(&adapter->hw,
		   "Multiqueue %s: Rx Queue count = %u, "
		   "Tx Queue count = %u\n",
		   (adapter->num_rx_queues > 1) ? "Enabled" : "Disabled",
		   adapter->num_rx_queues,
		   adapter->num_tx_queues);

	set_bit(__RNPVF_DOWN, &adapter->state);

	return 0;
err_alloc_q_vectors:
	rnpvf_reset_interrupt_capability(adapter);
err_set_interrupt:
	return err;
}

/**
 * rnpvf_clear_interrupt_scheme - Clear the current interrupt scheme settings
 * @adapter: board private structure to clear interrupt scheme on
 *
 * We go through and clear interrupt specific resources and reset the structure
 * to pre-load conditions
 **/
void rnpvf_clear_interrupt_scheme(struct rnpvf_adapter *adapter)
{
	adapter->num_tx_queues = 0;
	adapter->num_rx_queues = 0;

	rnpvf_free_q_vectors(adapter);
	rnpvf_reset_interrupt_capability(adapter);
}

#define UPDATE_VF_COUNTER_32bit(reg, last_counter, counter) \
	{                                                       \
		u32 current_counter = RNP_READ_REG(hw, reg);        \
		if (current_counter < last_counter)                 \
			counter += 0x100000000LL;                       \
		last_counter = current_counter;                     \
		counter &= 0xFFFFFFFF00000000LL;                    \
		counter |= current_counter;                         \
	}

#define UPDATE_VF_COUNTER_36bit(reg_lsb, reg_msb, last_counter, counter) \
	{                                                                    \
		u64 current_counter_lsb = RNP_READ_REG(hw, reg_lsb);             \
		u64 current_counter_msb = RNP_READ_REG(hw, reg_msb);             \
		u64 current_counter =                                            \
			(current_counter_msb << 32) | current_counter_lsb;           \
		if (current_counter < last_counter)                              \
			counter += 0x1000000000LL;                                   \
		last_counter = current_counter;                                  \
		counter &= 0xFFFFFFF000000000LL;                                 \
		counter |= current_counter;                                      \
	}
/**
 * rnpvf_update_stats - Update the board statistics counters.
 * @adapter: board private structure
 **/
void rnpvf_update_stats(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw_stats_own *hw_stats = &adapter->hw_stats;
	int i;
	struct net_device_stats *net_stats = &adapter->netdev->stats;

	//if (!adapter->link_up)
	//	return;
        net_stats->tx_packets = 0;
        net_stats->tx_bytes = 0;
        //net_stats->tx_dropped = 0;
        //net_stats->tx_errors = 0;

        net_stats->rx_packets = 0;
        net_stats->rx_bytes = 0;
        net_stats->rx_dropped = 0;
        net_stats->rx_errors = 0;


	hw_stats->vlan_add_cnt = 0;
	hw_stats->vlan_strip_cnt = 0;
	hw_stats->csum_err = 0;
	hw_stats->csum_good = 0;
	for (i = 0; i < adapter->num_q_vectors; i++) {
		struct rnpvf_ring *ring;
		struct rnpvf_q_vector *q_vector = adapter->q_vector[i];

		rnpvf_for_each_ring(ring, q_vector->tx) {
			hw_stats->vlan_add_cnt += ring->tx_stats.vlan_add;
			net_stats->tx_packets += ring->stats.packets;
			net_stats->tx_bytes += ring->stats.bytes;	
		}

		rnpvf_for_each_ring(ring, q_vector->rx) {
			hw_stats->csum_err += ring->rx_stats.csum_err;
			hw_stats->csum_good += ring->rx_stats.csum_good;
			hw_stats->vlan_strip_cnt += ring->rx_stats.vlan_remove;
			net_stats->rx_packets += ring->stats.packets;
			net_stats->rx_bytes += ring->stats.bytes;	
			net_stats->rx_errors += ring->rx_stats.csum_err;
		}
	}
}


static void rnpvf_reset_pf_request(struct rnpvf_adapter *adapter)
{
	struct rnpvf_hw *hw = &adapter->hw;


	if (!(adapter->flags & RNPVF_FLAG_PF_RESET_REQ))
		return;

	adapter->flags &= (~RNPVF_FLAG_PF_RESET_REQ);
	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	hw->mac.ops.req_reset_pf(hw);
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);

}
static int rnpvf_reset_subtask(struct rnpvf_adapter *adapter)
{

	if (!(adapter->flags & RNPVF_FLAG_PF_RESET))
		return 0;
	// check reset flags
	/* If we're already down or resetting, just bail */
	if (test_bit(__RNPVF_DOWN, &adapter->state) ||
		test_bit(__RNPVF_RESETTING, &adapter->state))
		return 0;

	adapter->tx_timeout_count++;

	rtnl_lock();
	rnpvf_reinit_locked(adapter);
	rtnl_unlock();

	adapter->flags &= (~RNPVF_FLAG_PF_RESET);
	
	return 1;
}

/**
 * rnpvf_watchdog - Timer Call-back
 * @data: pointer to adapter cast into an unsigned long
 **/
static void rnpvf_watchdog(struct timer_list *t)
{
	struct rnpvf_adapter *adapter = from_timer(adapter, t, watchdog_timer);

	/*
	 * Do the watchdog outside of interrupt context due to the lovely
	 * delays that some of the newer hardware requires
	 */

	if (test_bit(__RNPVF_DOWN, &adapter->state))
		goto watchdog_short_circuit;

#if 0
	/* get one bit for every active tx/rx interrupt vector */
	for (i = 0; i < adapter->num_msix_vectors - NON_Q_VECTORS; i++) {
		struct rnpvf_q_vector *qv = adapter->q_vector[i];
		if (qv->rx.ring || qv->tx.ring)
			eics |= 1 << i;
	}
#endif

watchdog_short_circuit:
	if (!test_bit(__RNPVF_REMOVE, &adapter->state)) 
		schedule_work(&adapter->watchdog_task);
}


__maybe_unused static void rnpvf_reset_task(struct work_struct *work)
{
	struct rnpvf_adapter *adapter;

	adapter = container_of(work, struct rnpvf_adapter, reset_task);

	/* If we're already down or resetting, just bail */
	if (test_bit(__RNPVF_DOWN, &adapter->state) ||
		test_bit(__RNPVF_RESETTING, &adapter->state))
		return;

	adapter->tx_timeout_count++;

	rnpvf_reinit_locked(adapter);
}

static void rnpvf_check_hang_subtask(struct rnpvf_adapter *adapter)
{
         int i;
         struct rnpvf_ring *tx_ring;
         u64 tx_next_to_clean_old;
         u64 tx_next_to_clean;
         u64 tx_next_to_use;
         struct rnpvf_ring *rx_ring;
         u64 rx_next_to_clean_old;
         u64 rx_next_to_clean;
         union rnp_rx_desc *rx_desc;

         /* If we're down or resetting, just bail */
         if (test_bit(__RNPVF_DOWN, &adapter->state) ||
             test_bit(__RNPVF_RESETTING, &adapter->state))
                 return;

         /* Force detection of hung controller */
	 /*
	    if (netif_carrier_ok(adapter->netdev)) {
	    for (i = 0; i < adapter->num_tx_queues; i++)
	    set_check_for_tx_hang(adapter->tx_ring[i]);
	    }
	 */
         // check if we lost tx irq ?
         for (i = 0; i < adapter->num_tx_queues; i++) {
                 tx_ring = adapter->tx_ring[i];
                 /* get the last next_to_clean */
                 tx_next_to_clean_old = tx_ring->tx_stats.tx_next_to_clean;
                 tx_next_to_clean = tx_ring->next_to_clean;
                 tx_next_to_use = tx_ring->next_to_use;

                 /* if we have tx desc to clean */
                 if (tx_next_to_use != tx_next_to_clean) {

                         if (tx_next_to_clean == tx_next_to_clean_old) {
                                 tx_ring->tx_stats.tx_equal_count++;
                                 if (tx_ring->tx_stats.tx_equal_count > 2) {
                                 /* maybe not so good */
                                         struct rnpvf_q_vector *q_vector = tx_ring->q_vector;

                                         /* stats */
                                         //printk("maybe tx irq miss happen!!! \n");
                                         if (q_vector->rx.ring || q_vector->tx.ring)
                                                 napi_schedule_irqoff(&q_vector->napi);

                                         tx_ring->tx_stats.tx_irq_miss++;
                                         tx_ring->tx_stats.tx_equal_count = 0;
                                 }
                         } else {
                                 tx_ring->tx_stats.tx_equal_count = 0;
                         }
                         /* update */
                         /* record this next_to_clean */
                         tx_ring->tx_stats.tx_next_to_clean = tx_next_to_clean;
                 } else {
                         /* clean record to -1 */
                         tx_ring->tx_stats.tx_next_to_clean = -1;

                 }

         }
         // check if we lost rx irq
         for (i = 0; i < adapter->num_rx_queues; i++) {
                 rx_ring = adapter->rx_ring[i];
                 /* get the last next_to_clean */
                 rx_next_to_clean_old = rx_ring->rx_stats.rx_next_to_clean;
                 /* get the now clean */
                 rx_next_to_clean = rx_ring->next_to_clean;

                 // if rx clean stopped
                 // maybe not so good
                 if (rx_next_to_clean == rx_next_to_clean_old) {
                         rx_ring->rx_stats.rx_equal_count++;

                         if ((rx_ring->rx_stats.rx_equal_count > 2)
                                 && (rx_ring->rx_stats.rx_equal_count < 5)) {
                                 // check if dd in the clean rx desc
                                 rx_desc = RNPVF_RX_DESC(rx_ring, rx_ring->next_to_clean);
                                 if (rnpvf_test_staterr(rx_desc, RNP_RXD_STAT_DD)) {
					 struct rnpvf_q_vector *q_vector = rx_ring->q_vector;
					 unsigned int size;

					 size = le16_to_cpu(rx_desc->wb.len) -
						 le16_to_cpu(rx_desc->wb.padding_len);
					 if (size) {
						 rx_ring->rx_stats.rx_irq_miss++;
						 if (q_vector->rx.ring || q_vector->tx.ring)
							 napi_schedule_irqoff(&q_vector->napi);
					 }
				 }
                                 //rx_ring->rx_stats.rx_equal_count = 0;
                         }
                         if (rx_ring->rx_stats.rx_equal_count > 1000)
                                 rx_ring->rx_stats.rx_equal_count = 0;
                 } else {
                         rx_ring->rx_stats.rx_equal_count = 0;
                 }
                 // update new clean
                 rx_ring->rx_stats.rx_next_to_clean = rx_next_to_clean;
         }

}

/**
 * rnpvf_watchdog_task - worker thread to bring link up
 * @work: pointer to work_struct containing our data
 **/
static void rnpvf_watchdog_task(struct work_struct *work)
{
	struct rnpvf_adapter *adapter =
		container_of(work, struct rnpvf_adapter, watchdog_task);
	struct net_device *netdev = adapter->netdev;
	struct rnpvf_hw *hw = &adapter->hw;
	u32 link_speed = adapter->link_speed;
	bool link_up = adapter->link_up;
	s32 need_reset;

	adapter->flags |= RNPVF_FLAG_IN_WATCHDOG_TASK;

	rnpvf_reset_pf_request(adapter);

	if (rnpvf_reset_subtask(adapter)) {
		// reset will change mtu mac 
		adapter->flags &= ~RNPVF_FLAG_PF_UPDATE_MTU;
		adapter->flags &= ~RNPVF_FLAG_PF_UPDATE_VLAN;
		goto pf_has_reset;
	}
	
	// send reset pf request
	/*
	 * Always check the link on the watchdog because we have
	 * no LSC interrupt
	 */
	//spin_lock_bh(&adapter->mbx_lock);

	need_reset = hw->mac.ops.check_link(hw, &link_speed, &link_up, false);

	//spin_unlock_bh(&adapter->mbx_lock);

	if (need_reset) {
		adapter->link_up = link_up;
		adapter->link_speed = link_speed;
		netif_carrier_off(netdev);
		netif_tx_stop_all_queues(netdev);
		schedule_work(&adapter->reset_task);
		goto pf_has_reset;
	}
	adapter->link_up = link_up;
	adapter->link_speed = link_speed;

	/*
	if (adapter->flags & RNPVF_FLAG_PF_UPDATE_MAC) {
		printk("update pf setup mac\n");
		rtnl_lock();
		call_netdevice_notifiers(NETDEV_CHANGEADDR, adapter->netdev);
		rtnl_unlock();
		adapter->flags &= ~RNPVF_FLAG_PF_UPDATE_MAC;
	} */

	// should check vf down
	// if we ready down
	if (test_bit(__RNPVF_DOWN, &adapter->state)) {
		// only once
		if (test_bit(__RNPVF_LINK_DOWN, &adapter->state)) {
			clear_bit(__RNPVF_LINK_DOWN, &adapter->state);
			dev_info(&adapter->pdev->dev, "NIC Link is Down\n");
		}
		goto skip_link_check;
	}

	if (link_up) {
		if (!netif_carrier_ok(netdev)) {
			char *link_speed_string;
			switch (link_speed) {
			case RNP_LINK_SPEED_40GB_FULL:
				link_speed_string = "40 Gbps";
				break;
			case RNP_LINK_SPEED_25GB_FULL:
				link_speed_string = "25 Gbps";
				break;
			case RNP_LINK_SPEED_10GB_FULL:
				link_speed_string = "10 Gbps";
				break;
			case RNP_LINK_SPEED_1GB_FULL:
				link_speed_string = "1 Gbps";
				break;
			case RNP_LINK_SPEED_100_FULL:
				link_speed_string = "100 Mbps";
				break;
			default:
				link_speed_string = "unknown speed";
				break;
			}
			dev_info(
				&adapter->pdev->dev, "NIC Link is Up, %s\n", link_speed_string);
			netif_carrier_on(netdev);
			netif_tx_wake_all_queues(netdev);
		}
	} else {
		adapter->link_up = false;
		adapter->link_speed = 0;
		if (netif_carrier_ok(netdev)) {
			dev_info(&adapter->pdev->dev, "NIC Link is Down\n");
			netif_carrier_off(netdev);
			netif_tx_stop_all_queues(netdev);
		}
	}
skip_link_check:
	// update mtu here
	if (adapter->flags & RNPVF_FLAG_PF_UPDATE_MTU) {
		//dev_info(&adapter->pdev->dev, "update mtu to %d\n", hw->mtu);
		adapter->flags &= ~RNPVF_FLAG_PF_UPDATE_MTU;
		if (netdev->mtu > hw->mtu) {
			netdev->mtu = hw->mtu;
			rtnl_lock();
			call_netdevice_notifiers(NETDEV_CHANGEMTU, adapter->netdev);
			rtnl_unlock();
			//schedule_work(&adapter->reset_task);
		}
	}
	if (adapter->flags & RNPVF_FLAG_PF_UPDATE_VLAN) {
		printk("update vlan\n");
		adapter->flags &= ~RNPVF_FLAG_PF_UPDATE_VLAN;
		// should setup rx mode again
		rnpvf_set_rx_mode(adapter->netdev);
	}
	
	rnpvf_check_hang_subtask(adapter);
	// check tx irq miss and rx irq miss

	rnpvf_update_stats(adapter);

pf_has_reset:
	/* Reset the timer */
	//if (!test_bit(__RNPVF_DOWN, &adapter->state))
	mod_timer(&adapter->watchdog_timer, round_jiffies(jiffies + (2 * HZ)));

	adapter->flags &= ~RNPVF_FLAG_IN_WATCHDOG_TASK;
}

/**
 * rnpvf_free_tx_resources - Free Tx Resources per Queue
 * @adapter: board private structure
 * @tx_ring: Tx descriptor ring for a specific queue
 *
 * Free all transmit software resources
 **/
void rnpvf_free_tx_resources(struct rnpvf_adapter *adapter,
							 struct rnpvf_ring *tx_ring)
{
	BUG_ON(tx_ring == NULL);

	rnpvf_clean_tx_ring(adapter, tx_ring);

	vfree(tx_ring->tx_buffer_info);
	tx_ring->tx_buffer_info = NULL;

	/* if not set, then don't free */
	if (!tx_ring->desc)
		return;

	dma_free_coherent(tx_ring->dev, tx_ring->size, tx_ring->desc, tx_ring->dma);

	tx_ring->desc = NULL;
}

/**
 * rnpvf_free_all_tx_resources - Free Tx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
static void rnpvf_free_all_tx_resources(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		// if (adapter->tx_ring[i].desc)
		rnpvf_free_tx_resources(adapter, adapter->tx_ring[i]);
}

/**
 * rnpvf_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 * @tx_ring:    tx descriptor ring (for a specific queue) to setup
 *
 * Return 0 on success, negative on failure
 **/
int rnpvf_setup_tx_resources(struct rnpvf_adapter *adapter,
		struct rnpvf_ring *tx_ring)
{
	struct device *dev = tx_ring->dev;
	int orig_node = dev_to_node(dev);
	int numa_node = NUMA_NO_NODE;
	int size;

	size = sizeof(struct rnpvf_tx_buffer) * tx_ring->count;

	if (tx_ring->q_vector)
		numa_node = tx_ring->q_vector->numa_node;

	dbg("%s size:%d count:%d\n", __func__, size, tx_ring->count);
	tx_ring->tx_buffer_info = vzalloc_node(size, numa_node);
	if (!tx_ring->tx_buffer_info)
		tx_ring->tx_buffer_info = vzalloc(size);
	if (!tx_ring->tx_buffer_info)
		goto err_buffer;

	/* round up to nearest 4K */
	tx_ring->size = tx_ring->count * sizeof(struct rnp_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);

	set_dev_node(dev, numa_node);
	tx_ring->desc =
		dma_alloc_coherent(dev, tx_ring->size, &tx_ring->dma, GFP_KERNEL);
	set_dev_node(dev, orig_node);
	if (!tx_ring->desc)
		tx_ring->desc =
			dma_alloc_coherent(dev, tx_ring->size, &tx_ring->dma, GFP_KERNEL);
	if (!tx_ring->desc)
		goto err;
	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	DPRINTK(IFUP, INFO, "%d TxRing:%d, vector:%d ItemCounts:%d "
			  "desc:%p(0x%llx) node:%d\n",
			  tx_ring->queue_index,
			  tx_ring->rnpvf_queue_idx,
			  tx_ring->q_vector->v_idx,
			  tx_ring->count,
			  tx_ring->desc,
			  tx_ring->dma,
			  numa_node);
	return 0;

err:
	rnpvf_err(
		"%s [SetupTxResources] ERROR: #%d TxRing:%d, vector:%d ItemCounts:%d\n",
		tx_ring->netdev->name,
		tx_ring->queue_index,
		tx_ring->rnpvf_queue_idx,
		tx_ring->q_vector->v_idx,
		tx_ring->count);
	vfree(tx_ring->tx_buffer_info);
err_buffer:
	tx_ring->tx_buffer_info = NULL;
	dev_err(dev, "Unable to allocate memory for the Tx descriptor ring\n");
	return -ENOMEM;
}

/**
 * rnpvf_setup_all_tx_resources - allocate all queues Tx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int rnpvf_setup_all_tx_resources(struct rnpvf_adapter *adapter)
{
	int i, err = 0;

	dbg("adapter->num_tx_queues:%d, adapter->tx_ring[0]:%p\n",
		adapter->num_tx_queues,
		adapter->tx_ring[0]);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		BUG_ON(adapter->tx_ring[i] == NULL);
		err = rnpvf_setup_tx_resources(adapter, adapter->tx_ring[i]);
		if (!err)
			continue;
		hw_dbg(&adapter->hw, "Allocation for Tx Queue %u failed\n", i);
		goto err_setup_tx;
	}

	return 0;

err_setup_tx:
	/* rewind the index freeing the rings as we go */
	while (i--)
		rnpvf_free_tx_resources(adapter, adapter->tx_ring[i]);
	return err;
}

/**
 * rnpvf_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 * @rx_ring:    rx descriptor ring (for a specific queue) to setup
 *
 * Returns 0 on success, negative on failure
 **/
int rnpvf_setup_rx_resources(struct rnpvf_adapter *adapter,
							 struct rnpvf_ring *rx_ring)
{
	struct device *dev = rx_ring->dev;
	int orig_node = dev_to_node(dev);
	int numa_node = -1;
	int size;

	BUG_ON(rx_ring == NULL);

	size = sizeof(struct rnpvf_rx_buffer) * rx_ring->count;

	if (rx_ring->q_vector)
		numa_node = rx_ring->q_vector->numa_node;

	rx_ring->rx_buffer_info = vzalloc_node(size, numa_node);
	if (!rx_ring->rx_buffer_info)
		rx_ring->rx_buffer_info = vzalloc(size);
	if (!rx_ring->rx_buffer_info)
		goto alloc_buffer;

	/* Round up to nearest 4K */
	rx_ring->size = rx_ring->count * sizeof(union rnp_rx_desc);
	rx_ring->size = ALIGN(rx_ring->size, 4096);

	set_dev_node(dev, numa_node);
	rx_ring->desc = dma_alloc_coherent(
		&adapter->pdev->dev, rx_ring->size, &rx_ring->dma, GFP_KERNEL);
	set_dev_node(dev, orig_node);
	if (!rx_ring->desc) {
		vfree(rx_ring->rx_buffer_info);
		rx_ring->rx_buffer_info = NULL;
		goto alloc_failed;
	}

	memset(rx_ring->desc, 0, rx_ring->size);
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	DPRINTK(IFUP, INFO, "%d RxRing:%d, vector:%d ItemCounts:%d "
			  "desc:%p(0x%llx) node:%d\n",
			  rx_ring->queue_index,
			  rx_ring->rnpvf_queue_idx,
			  rx_ring->q_vector->v_idx,
			  rx_ring->count,
			  rx_ring->desc,
			  rx_ring->dma,
			  numa_node);

	return 0;
alloc_failed:
	rnpvf_err(
			"%s [SetupTxResources] ERROR: #%d RxRing:%d, vector:%d ItemCounts:%d\n",
			rx_ring->netdev->name,
			rx_ring->queue_index,
			rx_ring->rnpvf_queue_idx,
			rx_ring->q_vector->v_idx,
			rx_ring->count);
	vfree(rx_ring->tx_buffer_info);
alloc_buffer:
	rx_ring->tx_buffer_info = NULL;
	dev_err(dev, "Unable to allocate memory for the Rx descriptor ring\n");

	return -ENOMEM;
}

/**
 * rnpvf_setup_all_rx_resources - allocate all queues Rx resources
 * @adapter: board private structure
 *
 * If this function returns with an error, then it's possible one or
 * more of the rings is populated (while the rest are not).  It is the
 * callers duty to clean those orphaned rings.
 *
 * Return 0 on success, negative on failure
 **/
static int rnpvf_setup_all_rx_resources(struct rnpvf_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		BUG_ON(adapter->rx_ring[i] == NULL);

		err = rnpvf_setup_rx_resources(adapter, adapter->rx_ring[i]);
		if (!err)
			continue;
		hw_dbg(&adapter->hw, "Allocation for Rx Queue %u failed\n", i);
		goto err_setup_rx;
	}

	return 0;

err_setup_rx:
	/* rewind the index freeing the rings as we go */
	while (i--)
		rnpvf_free_rx_resources(adapter, adapter->rx_ring[i]);
	return err;
}

/**
 * rnpvf_free_rx_resources - Free Rx Resources
 * @adapter: board private structure
 * @rx_ring: ring to clean the resources from
 *
 * Free all receive software resources
 **/
void rnpvf_free_rx_resources(struct rnpvf_adapter *adapter,
							 struct rnpvf_ring *rx_ring)
{
	struct pci_dev *pdev = adapter->pdev;

	rnpvf_clean_rx_ring(rx_ring);

	vfree(rx_ring->rx_buffer_info);
	rx_ring->rx_buffer_info = NULL;

	/* if not set, then don't free */
	if (!rx_ring->desc)
		return;

	dma_free_coherent(&pdev->dev, rx_ring->size, rx_ring->desc, rx_ring->dma);

	rx_ring->desc = NULL;
}

/**
 * rnpvf_free_all_rx_resources - Free Rx Resources for All Queues
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/
static void rnpvf_free_all_rx_resources(struct rnpvf_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		// if (adapter->rx_ring[i].desc)
		rnpvf_free_rx_resources(adapter, adapter->rx_ring[i]);
}

/**
 * rnpvf_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 *
 * Returns 0 on success, negative on failure
 **/
static int rnpvf_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	//int max_frame = new_mtu + ETH_HLEN + 2 * ETH_FCS_LEN;
	//int max_possible_frame = MAXIMUM_ETHERNET_VLAN_SIZE;

	//max_possible_frame = RNPVF_MAX_JUMBO_FRAME_SIZE;

	/* MTU < 68 is an error and causes problems on some kernels */
	//if ((new_mtu < 64) || (max_frame > max_possible_frame))
	//	return -EINVAL;

//	if (hw->mac.ops.set_mtu(hw, new_mtu)) {
	if (new_mtu > hw->mtu) {
		dev_info(&adapter->pdev->dev, "PF limit vf mtu setup too large %d \n", hw->mtu);
		return -EINVAL;

	} else {
		hw_dbg(&adapter->hw, "changing MTU from %d to %d\n", netdev->mtu, new_mtu);
		/* must set new MTU before calling down or up */
		netdev->mtu = new_mtu;
	}

	if (netif_running(netdev))
		rnpvf_reinit_locked(adapter);

	return 0;
}

//#define RNP_SAMPING_1SEC_INTERNAL (180000000ULL)
//int rnpvf_setup_tx_maxrate(struct rnpvf_ring *tx_ring, u64 max_rate)
//{
//	u16 dma_ring_idx = tx_ring->rnpvf_queue_idx;
//
//	/* set hardware samping internal 1S */
//	rnpvf_wr_reg(tx_ring->hw_addr + RNP_DMA_REG_TX_FLOW_CTRL_TM(dma_ring_idx),
//				 RNP_SAMPING_1SEC_INTERNAL / 10);
//	rnpvf_wr_reg(tx_ring->hw_addr + RNP_DMA_REG_TX_FLOW_CTRL_TH(dma_ring_idx),
//				 (max_rate / 10) * 3);
//
//	return 0;
//}

/**
 * rnp_tx_maxrate - callback to set the maximum per-queue bitrate
 * @netdev: network interface device structure
 * @queue_index: Tx queue to set
 * @maxrate: desired maximum transmit bitrate Mbps
 **/
//static int
//rnpvf_tx_maxrate(struct net_device *netdev, int queue_index, u32 maxrate)
//{
//	struct rnpvf_adapter *adapter = netdev_priv(netdev);
//	struct rnpvf_ring *tx_ring = adapter->tx_ring[queue_index];
//	u64 real_rate = 0;
//
//	rnpvf_dbg("%s: queue:%d maxrate:%d\n", __func__, queue_index, maxrate);
//	if (!maxrate)
//		return rnpvf_setup_tx_maxrate(tx_ring, 0);
//	/* we need turn it to bytes/s */
//	real_rate = (maxrate * 1000 * 1000) / 8;
//	rnpvf_setup_tx_maxrate(tx_ring, real_rate);
//
//	return 0;
//}

/**
 * rnpvf_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
int rnpvf_open(struct net_device *netdev)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	int err;

	DPRINTK(IFUP, INFO, "ifup\n");

	/* A previous failure to open the device because of a lack of
	 * available MSIX vector resources may have reset the number
	 * of msix vectors variable to zero.  The only way to recover
	 * is to unload/reload the driver and hope that the system has
	 * been able to recover some MSIX vector resources.
	 */
	if (!adapter->num_msix_vectors)
		return -ENOMEM;

	/* disallow open during test */
	if (test_bit(__RNPVF_TESTING, &adapter->state))
		return -EBUSY;

	if (hw->adapter_stopped) {
		rnpvf_reset(adapter);
		/* if adapter is still stopped then PF isn't up and
		 * the vf can't start. */
		if (hw->adapter_stopped) {
			err = RNP_ERR_MBX;
			dev_err(
				&hw->pdev->dev,
				"%s(%s):error: Unable to start - perhaps the PF Driver isn't "
				"up yet\n",
				adapter->name,
				netdev->name);
			goto err_setup_reset;
		}
	}

	netif_carrier_off(netdev);

	/* allocate transmit descriptors */
	err = rnpvf_setup_all_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = rnpvf_setup_all_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	rnpvf_configure(adapter);

	/* clear any pending interrupts, may auto mask */
	err = rnpvf_request_irq(adapter);
	if (err)
		goto err_req_irq;

	/* Notify the stack of the actual queue counts. */
	err = netif_set_real_num_tx_queues(netdev, adapter->num_tx_queues);
	if (err)
		goto err_set_queues;

	err = netif_set_real_num_rx_queues(netdev, adapter->num_rx_queues);
	if (err)
		goto err_set_queues;

	rnpvf_up_complete(adapter);

	return 0;

err_set_queues:
	rnpvf_free_irq(adapter);
err_req_irq:

err_setup_rx:
	rnpvf_free_all_rx_resources(adapter);
err_setup_tx:
	rnpvf_free_all_tx_resources(adapter);

err_setup_reset:

	return err;
}

/**
 * rnpvf_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
int rnpvf_close(struct net_device *netdev)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

	DPRINTK(IFDOWN, INFO, "ifdown\n");

	rnpvf_down(adapter);
	rnpvf_free_irq(adapter);

	rnpvf_free_all_tx_resources(adapter);
	rnpvf_free_all_rx_resources(adapter);

	return 0;
}

void rnpvf_tx_ctxtdesc(struct rnpvf_ring *tx_ring,
		u16 mss_seg_len,
		u8 l4_hdr_len,
		u8 tunnel_hdr_len,
		int ignore_vlan,
		u16 type_tucmd,
		bool crc_pad)
{
	struct rnp_tx_ctx_desc *context_desc;
	u16 i = tx_ring->next_to_use;
	struct rnpvf_adapter *adapter = RING2ADAPT(tx_ring);
	struct rnpvf_hw *hw = &adapter->hw;
	struct rnp_mbx_info *mbx = &hw->mbx;
	u8 vfnum = VFNUM(mbx, hw->vfnum);

	context_desc = RNPVF_TX_CTXTDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	/* set bits to identify this as an advanced context descriptor */
	type_tucmd |= RNP_TXD_CMD_RS | RNP_TXD_CTX_CTRL_DESC;

	if (adapter->priv_flags & RNPVF_PRIV_FLAG_TX_PADDING) {
		if (!crc_pad)
			type_tucmd |= RNP_TXD_MTI_CRC_PAD_CTRL; // close mac padding
	}

	context_desc->mss_len = cpu_to_le16(mss_seg_len);
	//context_desc->vfnum = 0x80 | adapter->hw.vfnum;
	context_desc->vfnum = 0x80 | vfnum;
	//printk("vf num is %d\n", adapter->hw.vfnum);
	context_desc->l4_hdr_len = l4_hdr_len;

	//context_desc->vf_veb_flags = VF_VEB_MARK;
	if (ignore_vlan)
		context_desc->vf_veb_flags |= VF_IGNORE_VLAN;

	context_desc->tunnel_hdr_len = tunnel_hdr_len;
	//context_desc->inner_vlan = cpu_to_le16(inner_vlan_tag);
	context_desc->cmd = cpu_to_le16(type_tucmd);
	buf_dump_line("ctx  ", __LINE__, context_desc, sizeof(*context_desc));
}

static int rnpvf_tso(struct rnpvf_ring *tx_ring,
					 struct rnpvf_tx_buffer *first,
					 u8 *hdr_len)
{
	struct sk_buff *skb = first->skb;
	struct net_device *netdev = tx_ring->netdev;
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		struct udphdr *udp;
		unsigned char *hdr;
	} l4;
	u32 paylen, l4_offset;
	int err;
	u8 *inner_mac;
	u16 gso_segs, gso_size;
	u16 gso_need_pad;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (!skb_is_gso(skb))
		return 0;

	err = skb_cow_head(skb, 0);
	if (err < 0)
		return err;

	inner_mac = skb->data;
	ip.hdr = skb_network_header(skb);
	l4.hdr = skb_transport_header(skb);

	//first->cmd_flags |= RNP_TXD_FLAG_TSO | RNP_TXD_IP_CSUM | RNP_TXD_L4_CSUM |
	//					RNP_TXD_L4_TYPE_TCP;

	/* initialize outer IP header fields */
	if (ip.v4->version == 4) {
		/* IP header will have to cancel out any data that
		 * is not a part of the outer IP header
		 */
		ip.v4->check = 0x0000;
	} else {
		ip.v6->payload_len = 0;
	}
#ifdef HAVE_ENCAP_TSO_OFFLOAD
	if (skb_shinfo(skb)->gso_type &
		(SKB_GSO_GRE |
#ifdef NETIF_F_GSO_PARTIAL
		 SKB_GSO_GRE_CSUM |
#endif
		 SKB_GSO_UDP_TUNNEL | SKB_GSO_UDP_TUNNEL_CSUM)) {
#ifndef NETIF_F_GSO_PARTIAL
		if (skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM) {
#else
		if (!(skb_shinfo(skb)->gso_type & SKB_GSO_PARTIAL) &&
			(skb_shinfo(skb)->gso_type & SKB_GSO_UDP_TUNNEL_CSUM)) {
#endif
		}
		inner_mac = skb_inner_mac_header(skb);
		first->tunnel_hdr_len = inner_mac - skb->data;

		if (skb_shinfo(skb)->gso_type &
				(SKB_GSO_UDP_TUNNEL | SKB_GSO_UDP_TUNNEL_CSUM)) {

			first->cmd_flags |= RNP_TXD_TUNNEL_VXLAN;
			l4.udp->check = 0;
		} else {
			first->cmd_flags |= RNP_TXD_TUNNEL_NVGRE;
		}
		dbg("set outer l4.udp to 0\n");

		/* reset pointers to inner headers */
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_inner_transport_header(skb);
	}

#endif /* HAVE_ENCAP_TSO_OFFLOAD */
	if (ip.v4->version == 4) {
		/* IP header will have to cancel out any data that
		 * is not a part of the outer IP header
		 */
		ip.v4->check = 0x0000;

	} else {
		ip.v6->payload_len = 0;
		/* set ipv6 type */
		first->cmd_flags |= (RNP_TXD_FLAG_IPv6);
	}

	/* determine offset of inner transport header */
	l4_offset = l4.hdr - skb->data;

	paylen = skb->len - l4_offset;
	dbg("before l4 checksum is %x\n", l4.tcp->check);

        if (skb->csum_offset == offsetof(struct tcphdr, check)) {
                dbg("tcp before l4 checksum is %x\n", l4.tcp->check);
                first->cmd_flags |= RNP_TXD_L4_TYPE_TCP;
                /* compute length of segmentation header */
                *hdr_len = (l4.tcp->doff * 4) + l4_offset;
                csum_replace_by_diff(&l4.tcp->check,
                                (__force __wsum)htonl(paylen));
                dbg("tcp l4 checksum is %x\n", l4.tcp->check);
        } else {
                dbg("paylen is %x\n", paylen);
                //first->tx_flags |= RNP_TXD_L4_TYPE_UDP;
                first->cmd_flags |= RNP_TXD_L4_TYPE_UDP;
                /* compute length of segmentation header */
                dbg("udp before l4 checksum is %x\n", l4.udp->check);
                *hdr_len = sizeof(*l4.udp) + l4_offset;
                csum_replace_by_diff(&l4.udp->check,
                                (__force __wsum)htonl(paylen));
                dbg("udp l4 checksum is %x\n", l4.udp->check);
        }

	dbg("l4 checksum is %x\n", l4.tcp->check);

	first->mac_ip_len = l4.hdr - ip.hdr;
	first->mac_ip_len |= (ip.hdr - inner_mac) << 9;

	/* compute header lengths */
	//*hdr_len = (l4.tcp->doff * 4) + l4_offset;
	/* pull values out of skb_shinfo */
	gso_size = skb_shinfo(skb)->gso_size;
	gso_segs = skb_shinfo(skb)->gso_segs;

#ifndef HAVE_NDO_FEATURES_CHECK
	/* too small a TSO segment size causes problems */
	if (gso_size < 64) {
		gso_size = 64;
		gso_segs = DIV_ROUND_UP(skb->len - *hdr_len, 64);
	}
#endif  
	// if we close padding check gso confition
        if (adapter->priv_flags & RNPVF_PRIV_FLAG_TX_PADDING) {
                if ((gso_need_pad = (first->skb->len - *hdr_len) % gso_size)) {
                        if ((gso_need_pad + *hdr_len) <= 60) {
                                gso_need_pad = 60 - (gso_need_pad + *hdr_len);
                                first->gso_need_padding = !!gso_need_pad;
                        }
                }
        }

	/* update gso size and bytecount with header size */
	/* to fix tx status */
	first->gso_segs = gso_segs;
	first->bytecount += (first->gso_segs - 1) * *hdr_len;

	first->mss_len_vf_num |= (gso_size | ((l4.tcp->doff * 4) << 24));
	// rnpvf_tx_ctxtdesc(tx_ring,skb_shinfo(skb)->gso_size ,l4len, 0, 0,
	// type_tucmd);

	first->cmd_flags |= RNP_TXD_FLAG_TSO | RNP_TXD_IP_CSUM | RNP_TXD_L4_CSUM;
	first->ctx_flag = true;
	return 1;
}

static int rnpvf_tx_csum(struct rnpvf_ring *tx_ring,
		struct rnpvf_tx_buffer *first)
{
	struct sk_buff *skb = first->skb;
	u8 l4_proto = 0;
	u8 ip_len = 0;
	u8 mac_len = 0;
	u8 *inner_mac = skb->data;
	u8 *exthdr;
	__be16 frag_off;
	union {
		struct iphdr *v4;
		struct ipv6hdr *v6;
		unsigned char *hdr;
	} ip;
	union {
		struct tcphdr *tcp;
		struct udphdr *udp;
		unsigned char *hdr;
	} l4;

	if (skb->ip_summed != CHECKSUM_PARTIAL) {
		return 0;
	}

	ip.hdr = skb_network_header(skb);
	l4.hdr = skb_transport_header(skb);

	inner_mac = skb->data;

#ifdef HAVE_ENCAP_CSUM_OFFLOAD
	/* outer protocol */
	if (skb->encapsulation) {
		/* define outer network header type */
		if (ip.v4->version == 4) {
			l4_proto = ip.v4->protocol;
		} else {
			exthdr = ip.hdr + sizeof(*ip.v6);
			l4_proto = ip.v6->nexthdr;
			if (l4.hdr != exthdr)
				ipv6_skip_exthdr(skb, exthdr - skb->data, &l4_proto, &frag_off);
		}

		/* define outer transport */
		switch (l4_proto) {
			case IPPROTO_UDP:
				l4.udp->check = 0;
				first->cmd_flags |= RNP_TXD_TUNNEL_VXLAN;

				break;
#ifdef HAVE_GRE_ENCAP_OFFLOAD
			case IPPROTO_GRE:

				first->cmd_flags |= RNP_TXD_TUNNEL_NVGRE;
				/* There was a long-standing issue in GRE where GSO
				 * was not setting the outer transport header unless
				 * a GRE checksum was requested. This was fixed in
				 * the 4.6 version of the kernel.  In the 4.7 kernel
				 * support for GRE over IPv6 was added to GSO.  So we
				 * can assume this workaround for all IPv4 headers
				 * without impacting later versions of the GRE.
				 */
				if (ip.v4->version == 4)
					l4.hdr = ip.hdr + (ip.v4->ihl * 4);
				break;
#endif
			default:
				skb_checksum_help(skb);
				return -1;
		}

		/* switch IP header pointer from outer to inner header */
		ip.hdr = skb_inner_network_header(skb);
		l4.hdr = skb_inner_transport_header(skb);

		inner_mac = skb_inner_mac_header(skb);
		first->tunnel_hdr_len = inner_mac - skb->data;
		first->ctx_flag = true;
		dbg("tunnel length is %d\n", first->tunnel_hdr_len);
	}
#endif /* HAVE_ENCAP_CSUM_OFFLOAD */

	mac_len = (ip.hdr - inner_mac); // mac length
	dbg("inner checksum needed %d", skb_checksum_start_offset(skb));
	dbg("skb->encapsulation %d\n", skb->encapsulation);
	ip_len = (l4.hdr - ip.hdr);
	if (ip.v4->version == 4) {
		l4_proto = ip.v4->protocol;
		// first->cmd_flags |= RNP_TXD_FLAG_IPv4;
	} else {
		exthdr = ip.hdr + sizeof(*ip.v6);
		l4_proto = ip.v6->nexthdr;
		if (l4.hdr != exthdr)
			ipv6_skip_exthdr(skb, exthdr - skb->data, &l4_proto, &frag_off);
		first->cmd_flags |= RNP_TXD_FLAG_IPv6;
	}
	/* Enable L4 checksum offloads */
	switch (l4_proto) {
		case IPPROTO_TCP:
			first->cmd_flags |= RNP_TXD_L4_TYPE_TCP | RNP_TXD_L4_CSUM;
			break;
		case IPPROTO_SCTP:
			first->cmd_flags |= RNP_TXD_L4_TYPE_SCTP | RNP_TXD_L4_CSUM;
			break;
		case IPPROTO_UDP:
			first->cmd_flags |= RNP_TXD_L4_TYPE_UDP | RNP_TXD_L4_CSUM;
			break;
		default:
			skb_checksum_help(skb);
			return 0;
	}
        if ((tx_ring->ring_flags & RNPVF_RING_NO_TUNNEL_SUPPORT) && (first->ctx_flag)) {
                /* if not support tunnel */
                // clean tunnel type
                first->cmd_flags &= (~RNP_TXD_TUNNEL_MASK);
                // add tunnel_hdr_len to mac_len
                mac_len += first->tunnel_hdr_len;
                // clean ctx
                first->tunnel_hdr_len = 0;
                first->ctx_flag = false;
        }

	dbg("mac length is %d\n", mac_len);
	dbg("ip length is %d\n", ip_len);
	first->mac_ip_len = (mac_len << 9) | ip_len;
	return 0;
}

static void rnpvf_tx_map(struct rnpvf_ring *tx_ring,
		struct rnpvf_tx_buffer *first,
		const u8 hdr_len)
{
	struct sk_buff *skb = first->skb;
	struct rnpvf_tx_buffer *tx_buffer;
	struct rnp_tx_desc *tx_desc;
	// struct skb_frag_struct *frag;
	skb_frag_t *frag;
	dma_addr_t dma;
	unsigned int data_len, size;
	u16 vlan = first->vlan;
	u16 cmd = first->cmd_flags;
	u16 i = tx_ring->next_to_use;
	u64 fun_id = ((u64)(tx_ring->vfnum) << (32 + 24));

	tx_desc = RNPVF_TX_DESC(tx_ring, i);

	// rnpvf_tx_olinfo_status(tx_desc, first->tx_flags, skb->len - hdr_len);
	tx_desc->blen = cpu_to_le16(skb->len - hdr_len); /* maybe no-use */
	tx_desc->vlan = cpu_to_le16(vlan);
	tx_desc->cmd = cpu_to_le16(cmd);
	tx_desc->mac_ip_len = first->mac_ip_len;

	size = skb_headlen(skb);
	data_len = skb->data_len;

	dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);

	tx_buffer = first;

	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		if (dma_mapping_error(tx_ring->dev, dma))
			goto dma_error;

		/* record length, and DMA address */
		dma_unmap_len_set(tx_buffer, len, size);
		dma_unmap_addr_set(tx_buffer, dma, dma);

		// 1st desc
		tx_desc->pkt_addr = cpu_to_le64(dma | fun_id);

		while (unlikely(size > RNPVF_MAX_DATA_PER_TXD)) {
			tx_desc->cmd = cpu_to_le16(cmd);
			tx_desc->blen = cpu_to_le16(RNPVF_MAX_DATA_PER_TXD);
			//==== desc==
			buf_dump_line("tx0  ", __LINE__, tx_desc, sizeof(*tx_desc));
			i++;
			tx_desc++;
			if (i == tx_ring->count) {
				tx_desc = RNPVF_TX_DESC(tx_ring, 0);
				i = 0;
			}

			dma += RNPVF_MAX_DATA_PER_TXD;
			size -= RNPVF_MAX_DATA_PER_TXD;

			tx_desc->pkt_addr = cpu_to_le64(dma | fun_id);
		}

		buf_dump_line("tx1  ", __LINE__, tx_desc, sizeof(*tx_desc));
		if (likely(!data_len)) // if not sg break
			break;
		tx_desc->cmd = cpu_to_le16(cmd);
		tx_desc->blen = cpu_to_le16(size);
		buf_dump_line("tx2  ", __LINE__, tx_desc, sizeof(*tx_desc));

		//==== frag==
		i++;
		tx_desc++;
		if (i == tx_ring->count) {
			tx_desc = RNPVF_TX_DESC(tx_ring, 0);
			i = 0;
		}
		tx_desc->cmd = RNP_TXD_CMD_RS;
		tx_desc->mac_ip_len = 0;

		size = skb_frag_size(frag);

		data_len -= size;

		dma = skb_frag_dma_map(tx_ring->dev, frag, 0, size, DMA_TO_DEVICE);

		tx_buffer = &tx_ring->tx_buffer_info[i];
	}

	/* write last descriptor with RS and EOP bits */
	tx_desc->cmd = cpu_to_le16(cmd | RNP_TXD_CMD_EOP | RNP_TXD_CMD_RS);
	tx_desc->blen = cpu_to_le16(size);
	buf_dump_line("tx3  ", __LINE__, tx_desc, sizeof(*tx_desc));
	// tx_desc->mac_len =skb_network_header(skb) - skb_mac_header(skb);
	// tx_desc->ip_len = skb_network_header_len(skb);

	netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

	/* set the timestamp */
	first->time_stamp = jiffies;

	/*
	 * Force memory writes to complete before letting h/w know there
	 * are new descriptors to fetch.  (Only applicable for weak-ordered
	 * memory model archs, such as IA-64).
	 *
	 * We also need this memory barrier to make certain all of the
	 * status bits have been updated before next_to_watch is written.
	 */
	wmb();

	/* set next_to_watch value indicating a packet is present */
	first->next_to_watch = tx_desc;

	buf_dump_line("tx4  ", __LINE__, tx_desc, sizeof(*tx_desc));
	i++;
	if (i == tx_ring->count)
		i = 0;

	tx_ring->next_to_use = i;

	/* notify HW of packet */
	rnpvf_wr_reg(tx_ring->tail, i);

	return;
dma_error:
	dev_err(tx_ring->dev, "TX DMA map failed\n");

	/* clear dma mappings for failed tx_buffer_info map */
	for (;;) {
		tx_buffer = &tx_ring->tx_buffer_info[i];
		rnpvf_unmap_and_free_tx_resource(tx_ring, tx_buffer);
		if (tx_buffer == first)
			break;
		if (i == 0)
			i = tx_ring->count;
		i--;
	}

	tx_ring->next_to_use = i;
}

static int __rnpvf_maybe_stop_tx(struct rnpvf_ring *tx_ring, int size)
{
	struct rnpvf_adapter *adapter = netdev_priv(tx_ring->netdev);

	dbg("stop subqueue\n");
	netif_stop_subqueue(tx_ring->netdev, tx_ring->queue_index);
	/* Herbert's original patch had:
	 *  smp_mb__after_netif_stop_queue();
	 * but since that doesn't exist yet, just open code it. */
	smp_mb();

	/* We need to check again in a case another CPU has just
	 * made room available. */
	if (likely(rnpvf_desc_unused(tx_ring) < size))
		return -EBUSY;

	/* A reprieve! - use start_queue because it doesn't call schedule */
	netif_start_subqueue(tx_ring->netdev, tx_ring->queue_index);
	++adapter->restart_queue;
	return 0;
}

void rnpvf_maybe_tx_ctxtdesc(struct rnpvf_ring *tx_ring,
							 struct rnpvf_tx_buffer *first,
							 int ignore_vlan,
							 u16 type_tucmd)
{
	if (first->ctx_flag) {
		rnpvf_tx_ctxtdesc(tx_ring,
				first->mss_len,
				first->l4_hdr_len,
				first->tunnel_hdr_len,
				ignore_vlan,
				type_tucmd,
				first->gso_need_padding);
	}
}

static int rnpvf_maybe_stop_tx(struct rnpvf_ring *tx_ring, int size)
{
	if (likely(RNPVF_DESC_UNUSED(tx_ring) >= size))
		return 0;
	return __rnpvf_maybe_stop_tx(tx_ring, size);
}
#ifdef FIX_VEB_BUG

static void rnpvf_force_src_mac(struct sk_buff *skb, struct net_device *netdev)
{
	u8 *data = skb->data;
	bool ret = false;
	struct netdev_hw_addr *ha;

	// check the first u8
	// force all src mac to myself
	if (is_multicast_ether_addr(data)) {
		if (0 == memcmp(data + netdev->addr_len, netdev->dev_addr, netdev->addr_len)) {
			ret = true;
			goto DONE;
		}
		netdev_for_each_uc_addr(ha, netdev) {
			if (0 == memcmp(data + netdev->addr_len, ha->addr, netdev->addr_len)) {
				//printk("drop own packets\n");
				ret = true;
				// if it is src mac, nothing todo
				goto DONE;
			}
		}
		/* if not src mac, force to src mac */
		if (!ret)
			memcpy(data + netdev->addr_len, netdev->dev_addr, netdev->addr_len);
	}
DONE:
	return;

}
#endif

netdev_tx_t rnpvf_xmit_frame_ring(struct sk_buff *skb,
		struct rnpvf_adapter *adapter,
		struct rnpvf_ring *tx_ring,
		bool tx_padding)
{
	struct rnpvf_tx_buffer *first;
	int tso;
	u16 cmd = RNP_TXD_CMD_RS;
	u16 vlan = 0;
	unsigned short f;
	u16 count = TXD_USE_COUNT(skb_headlen(skb));
	__be16 protocol = skb->protocol;
	u8 hdr_len = 0;
	int ignore_vlan = 0;

	dbg("=== begin ====\n");

	rnpvf_skb_dump(skb, true);

	dbg("skb:%p, skb->len:%d  headlen:%d, data_len:%d, tx_ring->next_to_use:%d "
		"count:%d\n",
		skb,
		skb->len,
		skb_headlen(skb),
		skb->data_len,
		tx_ring->next_to_use,
		tx_ring->count);
	/*
	 * need: 1 descriptor per page * PAGE_SIZE/RNPVF_MAX_DATA_PER_TXD,
	 *       + 1 desc for skb_headlen/RNPVF_MAX_DATA_PER_TXD,
	 *       + 2 desc gap to keep tail from touching head,
	 *       + 1 desc for context descriptor,
	 * otherwise try next time
	 */
	for (f = 0; f < skb_shinfo(skb)->nr_frags; f++) {
		skb_frag_t *frag_temp = &skb_shinfo(skb)->frags[f];
		// count += TXD_USE_COUNT(skb_shinfo(skb)->frags[f].size);
		count += TXD_USE_COUNT(skb_frag_size(frag_temp));
		dbg(" #%d frag: size:%d\n", f, skb_shinfo(skb)->frags[f].size);
	}

	if (rnpvf_maybe_stop_tx(tx_ring, count + 3)) {
		tx_ring->tx_stats.tx_busy++;
		return NETDEV_TX_BUSY;
	}
	dbg("xx %p\n", tx_ring->tx_buffer_info);

	/* patch force send src mac to this netdev->mac */
#ifdef FIX_VEB_BUG
	if (!(tx_ring->ring_flags & RNPVF_RING_VEB_MULTI_FIX))
		rnpvf_force_src_mac(skb, tx_ring->netdev);
#endif
	/* record the location of the first descriptor for this packet */
	first = &tx_ring->tx_buffer_info[tx_ring->next_to_use];
	first->skb = skb;
	first->bytecount = skb->len;
	first->gso_segs = 1;

        first->mss_len_vf_num = 0;
        first->inner_vlan_tunnel_len = 0;

	if (adapter->priv_flags & RNPVF_PRIV_FLAG_TX_PADDING) {
		first->ctx_flag = true;
		first->gso_need_padding = tx_padding;
	}

	/* if we have a HW VLAN tag being added default to the HW one */

	if (adapter->flags & RNPVF_FLAG_PF_SET_VLAN) {
		// in this mode , driver insert vlan
		vlan |= adapter->vf_vlan;
		cmd |= RNP_TXD_VLAN_VALID | RNP_TXD_VLAN_CTRL_INSERT_VLAN;

	} else { //normal mode
		if (skb_vlan_tag_present(skb)) {
#ifndef NO_SKB_VLAN_PROTO
			if (skb->vlan_proto != htons(ETH_P_8021Q)) {
				/* veb only use ctags */
				vlan |= skb_vlan_tag_get(skb);
				cmd |= RNP_TXD_SVLAN_TYPE |
					RNP_TXD_VLAN_CTRL_INSERT_VLAN;
			} else {
#endif
				vlan |= skb_vlan_tag_get(skb);
				cmd |= RNP_TXD_VLAN_VALID |
					RNP_TXD_VLAN_CTRL_INSERT_VLAN;
#ifndef NO_SKB_VLAN_PROTO
			}
#endif
			//vlan |= skb_vlan_tag_get(skb);
			//cmd |= RNP_TXD_VLAN_VALID | RNP_TXD_VLAN_CTRL_INSERT_VLAN;
			tx_ring->tx_stats.vlan_add++;
			/* else if it is a SW VLAN check the next protocol and store the tag */
		} else if (protocol == __constant_htons(ETH_P_8021Q)) {
			struct vlan_hdr *vhdr, _vhdr;
			vhdr = skb_header_pointer(skb, ETH_HLEN, sizeof(_vhdr), &_vhdr);
			if (!vhdr)
				goto out_drop;

			protocol = vhdr->h_vlan_encapsulated_proto;
			vlan = ntohs(vhdr->h_vlan_TCI);
			cmd |= RNP_TXD_VLAN_VALID | RNP_TXD_VLAN_CTRL_NOP;
			ignore_vlan = 1;
		}
	}

	// skb_tx_timestamp(skb);

	/* record initial flags and protocol */
	first->cmd_flags = cmd;
	first->vlan = vlan;
	first->protocol = protocol;
	/* default len should not 0 (hw request) */
	first->mac_ip_len = 20;
	first->tunnel_hdr_len = 0;
	//first->ctx_flag = true;

	tso = rnpvf_tso(tx_ring, first, &hdr_len);
	if (tso < 0) {
		goto out_drop;
	} else if (!tso) {
		rnpvf_tx_csum(tx_ring, first);
	}
	/* vf should always send ctx with vf_num*/
	first->ctx_flag = true;
	/* add control desc */
	rnpvf_maybe_tx_ctxtdesc(tx_ring, first, ignore_vlan, 0);

	rnpvf_tx_map(tx_ring, first, hdr_len);

	rnpvf_maybe_stop_tx(tx_ring, DESC_NEEDED);

	dbg("=== end ====\n\n\n\n");
	return NETDEV_TX_OK;

out_drop:
	dev_kfree_skb_any(first->skb);
	first->skb = NULL;

	return NETDEV_TX_OK;
}

static bool check_sctp_no_padding(struct sk_buff *skb)
{
        bool no_padding = false;
        u8 l4_proto = 0;
        u8 *exthdr;
        __be16 frag_off;
        union {
                struct iphdr *v4;
                struct ipv6hdr *v6;
                unsigned char *hdr;
        } ip;
        union {
                struct tcphdr *tcp;
                struct udphdr *udp;
                unsigned char *hdr;
        } l4;

        ip.hdr = skb_network_header(skb);
        l4.hdr = skb_transport_header(skb);

        if (ip.v4->version == 4) {
                l4_proto = ip.v4->protocol;
        } else {
                exthdr = ip.hdr + sizeof(*ip.v6);
                l4_proto = ip.v6->nexthdr;
                if (l4.hdr != exthdr)
                        ipv6_skip_exthdr(skb, exthdr - skb->data, &l4_proto,
                                        &frag_off);
        }
        // sctp set no_padding to true
        switch (l4_proto) {
        case IPPROTO_SCTP:
                no_padding = true;
                break;
        default:

                break;
        }
        // todo
        return no_padding;
}


static int rnpvf_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_ring *tx_ring;
	bool tx_padding = false;
	
	/*
	 * The minimum packet size for olinfo paylen is 17 so pad the skb
	 * in order to meet this minimum size requirement.
	 */
	/* for sctp packet, padding 0 change the crc32c */
	/* padding is done by hw */
	/*
	if (unlikely(skb->len < RNPVF_MIN_MTU)) {
		if (skb_pad(skb, RNPVF_MIN_MTU - skb->len))
			return NETDEV_TX_OK;
		skb->len = RNPVF_MIN_MTU;
		skb_set_tail_pointer(skb, RNPVF_MIN_MTU);
	}
	*/

	if (!netif_carrier_ok(netdev)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	if (adapter->priv_flags & RNPVF_PRIV_FLAG_TX_PADDING) {
		if (skb->len < 60) {
			if (!check_sctp_no_padding(skb)) {
				if (skb_put_padto(skb, 60))
					return NETDEV_TX_OK;

			} else {
				// if sctp smaller than 60, never padding
				tx_padding = true;
			}
		}

	} else {
		if (skb_put_padto(skb, 17))
			return NETDEV_TX_OK;
	}

	tx_ring = adapter->tx_ring[skb->queue_mapping];
	dbg("xmi:queue_mapping:%d ring:%p\n", skb->queue_mapping, tx_ring);
	return rnpvf_xmit_frame_ring(skb, adapter, tx_ring, tx_padding);
}

/**
 * rnpvf_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int rnpvf_set_mac(struct net_device *netdev, void *p)
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	struct rnpvf_hw *hw = &adapter->hw;
	struct sockaddr *addr = p;
	s32 ret_val;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	ret_val = hw->mac.ops.set_rar(hw, 0, addr->sa_data, 0);
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);
	if (0 != ret_val) {
		/* set mac failed */
		dev_err(&adapter->pdev->dev, "pf not allowed reset mac\n");
		return -EADDRNOTAVAIL;

	} else {
		eth_hw_addr_set(netdev, addr->sa_data);
		//memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
		memcpy(hw->mac.addr, addr->sa_data, netdev->addr_len);
		rnpvf_configure_veb(adapter);
	}

	return 0;
}

void remove_mbx_irq(struct rnpvf_adapter *adapter)
{
	u32 msgbuf[2];
	struct rnpvf_hw *hw = &adapter->hw;

	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	msgbuf[0] = RNP_PF_REMOVE;
	adapter->hw.mbx.ops.write_posted(hw, msgbuf, 1, false);
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);
	mdelay(10);

        /* mbx */
	if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
		adapter->hw.mbx.ops.configure(
				&adapter->hw, adapter->msix_entries[0].entry, false);
		free_irq(adapter->msix_entries[0].vector, adapter);
	}
}

static void rnp_get_link_status(struct rnpvf_adapter *adapter)
{
        struct rnpvf_hw *hw = &adapter->hw;
	u32 msgbuf[3];
	s32 ret_val = -1;

	spin_lock_bh(&adapter->mbx_lock);
	set_bit(__RNPVF_MBX_POLLING, &adapter->state);
	msgbuf[0] = RNP_PF_GET_LINK;
	adapter->hw.mbx.ops.write_posted(hw, msgbuf, 1, false);
	mdelay(2);
	ret_val =
		adapter->hw.mbx.ops.read_posted(hw, msgbuf, 2, false);
	if (ret_val == 0) {
		if (msgbuf[1] & RNP_PF_LINK_UP) {
			hw->link = true;
			hw->speed = msgbuf[1] & 0xffff;

		} else {
			hw->link = false;
			hw->speed = 0;
		}
	} else {
		printk("[rpnvf] error! mbx GET_LINK faild!\n");
	}
	clear_bit(__RNPVF_MBX_POLLING, &adapter->state);
	spin_unlock_bh(&adapter->mbx_lock);
}


// 
int register_mbx_irq(struct rnpvf_adapter *adapter)
{
        struct rnpvf_hw *hw = &adapter->hw;
        struct net_device *netdev = adapter->netdev;
        int err = 0;

        /* for mbx:vector0 */
	if (adapter->flags & RNPVF_FLAG_MSIX_ENABLED) {
		err = request_irq(adapter->msix_entries[0].vector,
				rnpvf_msix_other,
				0,
				netdev->name,
				adapter);
		if (err) {
			dev_err(&adapter->pdev->dev, "request_irq for msix_other failed: %d\n", err);
			goto err_mbx;
		}
		hw->mbx.ops.configure(hw, adapter->msix_entries[0].entry, true);
	}

	rnp_get_link_status(adapter);
err_mbx:
        return err;

}

static int rnpvf_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct rnpvf_adapter *adapter = pci_get_drvdata(pdev);
	struct net_device *netdev = adapter->netdev;
#ifdef CONFIG_PM
	int retval = 0;
#endif

	netif_device_detach(netdev);

	if (netif_running(netdev)) {
		rtnl_lock();
		rnpvf_down(adapter);
		rnpvf_free_irq(adapter);
		rnpvf_free_all_tx_resources(adapter);
		rnpvf_free_all_rx_resources(adapter);
		rtnl_unlock();
	}

	remove_mbx_irq(adapter);
	rnpvf_clear_interrupt_scheme(adapter);

#ifdef CONFIG_PM
	retval = pci_save_state(pdev);
	if (retval)
		return retval;

#endif
	pci_disable_device(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int rnpvf_resume(struct pci_dev *pdev)
{
	struct rnpvf_adapter *adapter = pci_get_drvdata(pdev);
	struct net_device *netdev = adapter->netdev;
	u32 err;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	/*
	 * pci_restore_state clears dev->state_saved so call
	 * pci_save_state to restore it.
	 */
	pci_save_state(pdev);

	err = pcim_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device from suspend\n");
		return err;
	}
	pci_set_master(pdev);

	rtnl_lock();
	err = rnpvf_init_interrupt_scheme(adapter);
	rtnl_unlock();
	register_mbx_irq(adapter);

	if (err) {
		dev_err(&pdev->dev, "Cannot initialize interrupts\n");
		return err;
	}

	rnpvf_reset(adapter);

	if (netif_running(netdev)) {
		err = rnpvf_open(netdev);
		if (err)
			return err;
	}

	netif_device_attach(netdev);

	return err;
}

#endif /* CONFIG_PM */
static void rnpvf_shutdown(struct pci_dev *pdev)
{
	rnpvf_suspend(pdev, PMSG_SUSPEND);
}

//#if defined(RHEL_RELEASE_CODE)
// void
//#else
// static struct rtnl_link_stats64 *
//#endif
// rnpvf_get_stats64(struct net_device *netdev, struct rtnl_link_stats64 *stats)
#ifdef HAVE_NDO_GET_STATS64
#ifdef HAVE_VOID_NDO_GET_STATS64
static void rnpvf_get_stats64(struct net_device *netdev,
							  struct rtnl_link_stats64 *stats)
#else
static struct rtnl_link_stats64 *
rnpvf_get_stats64(struct net_device *netdev, struct rtnl_link_stats64 *stats)
#endif
{
	struct rnpvf_adapter *adapter = netdev_priv(netdev);
	int i;
	u64 ring_csum_err = 0;
	u64 ring_csum_good = 0;

	rcu_read_lock();
	for (i = 0; i < adapter->num_rx_queues; i++) {
		struct rnpvf_ring *ring = adapter->rx_ring[i];
		u64 bytes, packets;
		unsigned int start;

		if (ring) {
			do {
				start = u64_stats_fetch_begin(&ring->syncp);
				packets = ring->stats.packets;
				bytes = ring->stats.bytes;
				ring_csum_err += ring->rx_stats.csum_err;
				ring_csum_good += ring->rx_stats.csum_good;
			} while (u64_stats_fetch_retry(&ring->syncp, start));
			stats->rx_packets += packets;
			stats->rx_bytes += bytes;
		}
	}

	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct rnpvf_ring *ring = adapter->tx_ring[i];
		u64 bytes, packets;
		unsigned int start;

		if (ring) {
			do {
				start = u64_stats_fetch_begin(&ring->syncp);
				packets = ring->stats.packets;
				bytes = ring->stats.bytes;
			} while (u64_stats_fetch_retry(&ring->syncp, start));
			stats->tx_packets += packets;
			stats->tx_bytes += bytes;
		}
	}
	rcu_read_unlock();
	/* following stats updated by rnp_watchdog_task() */
	stats->multicast = netdev->stats.multicast;
	stats->rx_errors = netdev->stats.rx_errors;
	stats->rx_length_errors = netdev->stats.rx_length_errors;
	stats->rx_crc_errors = netdev->stats.rx_crc_errors;
	stats->rx_missed_errors = netdev->stats.rx_missed_errors;

#ifndef HAVE_VOID_NDO_GET_STATS64
	return stats;
#endif
}
#else
/**
 * rnpvf_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/
static struct net_device_stats *rnpvf_get_stats(struct net_device *netdev)
{
        struct rnpvf_adapter *adapter = netdev_priv(netdev);

        /* update the stats data */
        rnpvf_update_stats(adapter);

#ifdef HAVE_NETDEV_STATS_IN_NETDEV
        /* only return the current stats */
        return &netdev->stats;
#else
        /* only return the current stats */
        return &adapter->net_stats;
#endif /* HAVE_NETDEV_STATS_IN_NETDEV */
}

#endif

#ifdef HAVE_NDO_FEATURES_CHECK
#define RNP_MAX_TUNNEL_HDR_LEN 80
#ifdef NETIF_F_GSO_PARTIAL
#define RNP_MAX_MAC_HDR_LEN 127
#define RNP_MAX_NETWORK_HDR_LEN 511

static netdev_features_t rnpvf_features_check(struct sk_buff *skb,
                                            struct net_device *dev,
                                            netdev_features_t features)
{
        unsigned int network_hdr_len, mac_hdr_len;

        /* Make certain the headers can be described by a context descriptor */
        mac_hdr_len = skb_network_header(skb) - skb->data;
        if (unlikely(mac_hdr_len > RNP_MAX_MAC_HDR_LEN))
                return features &
                       ~(NETIF_F_HW_CSUM | NETIF_F_SCTP_CRC |
                         NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_TSO | NETIF_F_TSO6);

        network_hdr_len = skb_checksum_start(skb) - skb_network_header(skb);
        if (unlikely(network_hdr_len > RNP_MAX_NETWORK_HDR_LEN))
                return features & ~(NETIF_F_HW_CSUM | NETIF_F_SCTP_CRC |
                                    NETIF_F_TSO | NETIF_F_TSO6);

        /* We can only support IPV4 TSO in tunnels if we can mangle the
         * inner IP ID field, so strip TSO if MANGLEID is not supported.
         */
        if (skb->encapsulation && !(features & NETIF_F_TSO_MANGLEID))
                features &= ~NETIF_F_TSO;

        return features;
}
#else
static netdev_features_t rnpvf_features_check(struct sk_buff *skb,
                                            struct net_device *dev,
                                            netdev_features_t features)
{
        if (!skb->encapsulation)
                return features;

        if (unlikely(skb_inner_mac_header(skb) - skb_transport_header(skb) >
                     RNP_MAX_TUNNEL_HDR_LEN))
                return features & ~NETIF_F_CSUM_MASK;

        return features;
}

#endif /* NETIF_F_GSO_PARTIAL */
#endif /* HAVE_NDO_FEATURES_CHECK */

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops rnpvf_netdev_ops = {
	.ndo_open = rnpvf_open,
	.ndo_stop = rnpvf_close,
	.ndo_start_xmit = rnpvf_xmit_frame,
	.ndo_validate_addr = eth_validate_addr,
#ifdef HAVE_NDO_GET_STATS64
	.ndo_get_stats64 = rnpvf_get_stats64,
#else
	.ndo_get_stats	= rnpvf_get_stats,
#endif
	.ndo_set_rx_mode = rnpvf_set_rx_mode,
#if 0
    .ndo_validate_addr	= eth_validate_addr,
    .ndo_tx_timeout		= rnpvf_tx_timeout,
#endif
	.ndo_set_mac_address = rnpvf_set_mac,

#ifdef HAVE_RHEL7_NET_DEVICE_OPS_EXT
        /* RHEL7 requires this to be defined to enable extended ops.
         * RHEL7 uses the function get_ndo_ext to retrieve offsets for
         * extended fields from with the net_device_ops struct and
         * ndo_size is checked to determine whether or not
         * the offset is valid.
         */
        .ndo_size = sizeof(const struct net_device_ops),
#endif

#ifdef HAVE_RHEL7_EXTENDED_MIN_MAX_MTU
        .extended.ndo_change_mtu = rnpvf_change_mtu,
#else
        .ndo_change_mtu = rnpvf_change_mtu,
#endif
	/*
#ifdef HAVE_RHEL7_NETDEV_OPS_EXT_NDO_TX_MAXRATE
	.extended.ndo_set_tx_maxrate = rnpvf_tx_maxrate,
#else
	.ndo_set_tx_maxrate = rnpvf_tx_maxrate,
#endif
*/
	//.ndo_set_features = rnpvf_set_features,
	//.ndo_fix_features = rnpvf_fix_features,
#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
	.ndo_vlan_rx_add_vid = rnpvf_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = rnpvf_vlan_rx_kill_vid,
#endif

#ifdef HAVE_NDO_FEATURES_CHECK
        .ndo_features_check = rnpvf_features_check,
#endif /* HAVE_NDO_FEATURES_CHECK */

#ifdef HAVE_RHEL6_NET_DEVICE_OPS_EXT
};
/* RHEL6 keeps these operations in a separate structure */
static const struct net_device_ops_ext rnpvf_netdev_ops_ext = {
        .size = sizeof(struct net_device_ops_ext),
#endif /* HAVE_RHEL6_NET_DEVICE_OPS_EXT */
#ifdef HAVE_NDO_SET_FEATURES
        .ndo_set_features = rnpvf_set_features,
        .ndo_fix_features = rnpvf_fix_features,
#endif /* HAVE_NDO_SET_FEATURES */
};
#endif /* HAVE_NET_DEVICE_OPS */

void rnpvf_assign_netdev_ops(struct net_device *dev)
{

        /* different hw can assign difference fun */
#ifdef HAVE_NET_DEVICE_OPS
        dev->netdev_ops = &rnpvf_netdev_ops;
#ifdef HAVE_RHEL6_NET_DEVICE_OPS_EXT
        set_netdev_ops_ext(dev, &rnpvf_netdev_ops_ext);
#endif /* HAVE_RHEL6_NET_DEVICE_OPS_EXT */
#else /* HAVE_NET_DEVICE_OPS */
        dev->open = &rnpvf_open;
        dev->stop = &rnpvf_close;
        dev->hard_start_xmit = &rnpvf_xmit_frame;
        //dev->get_stats = &rnp_get_stats;
#ifdef HAVE_SET_RX_MODE
        dev->set_rx_mode = &rnpvf_set_rx_mode;
#endif
        dev->set_multicast_list = &rnpvf_set_rx_mode;
        dev->set_mac_address = &rnpvf_set_mac;
        dev->change_mtu = &rnpvf_change_mtu;
        //dev->do_ioctl = &rnpvf_ioctl;
#ifdef HAVE_TX_TIMEOUT
        dev->tx_timeout = &rnpvf_tx_timeout;
#endif
#if defined(NETIF_F_HW_VLAN_TX) || defined(NETIF_F_HW_VLAN_CTAG_TX)
        //dev->vlan_rx_register = &rnp_vlan_mode; //todo
        dev->vlan_rx_add_vid = &rnpvf_vlan_rx_add_vid;
        dev->vlan_rx_kill_vid = &rnpvf_vlan_rx_kill_vid;
#endif
#ifdef HAVE_NETDEV_SELECT_QUEUE
        dev->select_queue = &__netdev_pick_tx;
#endif /* HAVE_NETDEV_SELECT_QUEUE */
#endif /* HAVE_NET_DEVICE_OPS */

#ifdef HAVE_RHEL6_NET_DEVICE_EXTENDED
#ifdef HAVE_NDO_BUSY_POLL
        //netdev_extended(dev)->ndo_busy_poll             = rnp_busy_poll_recv; // todo
#endif /* HAVE_NDO_BUSY_POLL */
#endif /* HAVE_RHEL6_NET_DEVICE_EXTENDED */

        rnpvf_set_ethtool_ops(dev);
        dev->watchdog_timeo = 5 * HZ;
}

static u8 rnpvf_vfnum_n500(struct rnpvf_hw *hw)
{
	u16 vf_num;

	vf_num = readl(hw->hw_addr + VF_NUM_REG_N500);
#define VF_NUM_MASK_N500 (0xff)

	return (vf_num & VF_NUM_MASK_N500);

}
static u8 rnpvf_vfnum(struct rnpvf_hw *hw)
{
	u16 vf_num = -1;
	u32 pfvfnum_reg;

#if CONFIG_BAR4_PFVFNUM
	int ring, v;
	u16 func = 0;

	func = ((hw->pdev->devfn & 0x1) ? 1 : 0);
	for (ring = 0; ring < 128; ring += 2) {
		v = rd32(hw, RNP_DMA_RX_START(ring));
		if ((v & 0xFFFF) == hw->pdev->vendor) {
			continue;
		} else {
			vf_num = (1 << 7) /*vf-active*/ | (func << 6) /*pf*/ |
					 (ring / 2) /*vfnum*/;
			break;
		}
	}
	//printk("%s: devfn:0x%x func:%d\n", __func__, hw->pdev->devfn, nr_pf);
	return vf_num;
#else
	pfvfnum_reg = (VF_NUM_REG_N10 & (pci_resource_len(hw->pdev, 0) - 1));
	vf_num = readl(hw->hw_addr_bar0 + pfvfnum_reg);
	//printk("vf_num is %x, reg:0x%x\n", vf_num, pfvfnum_reg);
#define VF_NUM_MASK_TEMP (0xff0)
#define VF_NUM_OFF		 (4)
	return ((vf_num & VF_NUM_MASK_TEMP) >> VF_NUM_OFF);
#endif
}

static inline unsigned long rnpvf_tso_features(struct rnpvf_hw *hw)
{

        unsigned long features = 0;

#ifdef NETIF_F_TSO
        if (hw->feature_flags & RNPVF_NET_FEATURE_TSO)
                features |= NETIF_F_TSO;
#endif /* NETIF_F_TSO */
#ifdef NETIF_F_TSO6
        if (hw->feature_flags & RNPVF_NET_FEATURE_TSO)
                features |= NETIF_F_TSO6;
#endif /* NETIF_F_TSO6 */
#ifdef NETIF_F_GSO_PARTIAL
        features |= NETIF_F_GSO_PARTIAL;
        if (hw->feature_flags & RNPVF_NET_FEATURE_TX_UDP_TUNNEL)
		features |= RNPVF_GSO_PARTIAL_FEATURES;
#endif

        return features;
}


static int rnpvf_add_adpater(struct pci_dev *pdev,
		const struct rnpvf_info *ii,
		struct rnpvf_adapter **padapter)
{
	int err = 0;
	struct rnpvf_adapter *adapter = NULL;
	struct net_device *netdev;
	struct rnpvf_hw *hw;
	unsigned int queues = MAX_TX_QUEUES;
	static int pf0_cards_found;
	static int pf1_cards_found;
	static int pf2_cards_found;
	static int pf3_cards_found;
	//netdev_features_t hw_enc_features = 0;

#ifndef NETIF_F_GSO_PARTIAL
#ifdef HAVE_NDO_SET_FEATURES
#ifndef HAVE_RHEL6_NET_DEVICE_OPS_EXT
	netdev_features_t hw_features;
#else
	u32 hw_features;
#endif
#endif
#endif /* NETIF_F_GSO_PARTIAL */
	// call rx/rx queue cnt
	pr_info("====  add adapter queues:%d ====", queues);

	netdev = alloc_etherdev_mq(sizeof(struct rnpvf_adapter), queues);
	if (!netdev) {
		return -ENOMEM;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	adapter = netdev_priv(netdev);
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	/* setup some status */
#ifdef FIX_VF_BUG
	adapter->status |= GET_VFNUM_FROM_BAR0;
#endif

	if (padapter) {
		*padapter = adapter;
	}
	pci_set_drvdata(pdev, adapter);

	hw = &adapter->hw;
	hw->back = adapter;
	hw->pdev = pdev;
	hw->board_type = ii->board_type;
	adapter->msg_enable = netif_msg_init(debug,
			NETIF_MSG_DRV
#ifdef MSG_PROBE_ENABLE
			| NETIF_MSG_PROBE
#endif
#ifdef MSG_IFUP_ENABLE
			| NETIF_MSG_IFUP
#endif
#ifdef MSG_IFDOWN_ENABLE
			| NETIF_MSG_IFDOWN
#endif
			);
			//| NETIF_MSG_PROBE |
			//NETIF_MSG_IFUP | NETIF_MSG_IFDOWN);

	switch (ii->mac) {
	case rnp_mac_2port_10G:
		hw->mode = MODE_NIC_MODE_2PORT_10G;
		break;
	case rnp_mac_2port_40G:
		hw->mode = MODE_NIC_MODE_2PORT_40G;
		break;
	case rnp_mac_4port_10G:
		hw->mode = MODE_NIC_MODE_4PORT_10G;
		break;
	case rnp_mac_8port_10G:
		hw->mode = MODE_NIC_MODE_8PORT_10G;
		break;
	default:
		break;
	}

	switch (hw->board_type) {
	case rnp_board_n10:
#define RNP_N10_BAR 4
		hw->hw_addr = pcim_iomap(pdev, RNP_N10_BAR, 0);
		if (!hw->hw_addr) {
			err = -EIO;
			goto err_ioremap;
		}
		dev_info(&pdev->dev,
				"[bar%d]:%p %llx len=%d MB\n",
				RNP_N10_BAR,
				hw->hw_addr,
				(unsigned long long)pci_resource_start(pdev, RNP_N10_BAR),
				(int)pci_resource_len(pdev, RNP_N10_BAR) / 1024 / 1024);
#if CONFIG_BAR4_PFVFNUM
#else
		hw->hw_addr_bar0 = pcim_iomap(pdev, 0, 0);
		if (!hw->hw_addr_bar0) {
			err = -EIO;
			goto err_ioremap;
		}
#endif

		// get version
		hw->vfnum = rnpvf_vfnum(hw);
		dev_info(&adapter->pdev->dev, "hw->vfnum is %x\n", hw->vfnum);
		hw->ring_msix_base = hw->hw_addr + 0xa0000;

		if (hw->vfnum & 0x40) {
#ifdef FIX_VF_BUG
			/* in this mode offset hw_addr */
			hw->ring_msix_base += 0x200;
			hw->hw_addr += 0x100000;
#endif
			adapter->port = adapter->bd_number = pf1_cards_found++;
			//printk("this card is pf1\n");
			if (pf1_cards_found == 1000)
				pf1_cards_found = 0;
		} else {
			adapter->port = adapter->bd_number = pf0_cards_found++;
			//printk("this card is pf0\n");
			if (pf0_cards_found == 1000)
				pf0_cards_found = 0;
		}
		snprintf(adapter->name,
				sizeof(netdev->name),
				"%s%d%d",
				rnpvf_driver_name,
				(hw->vfnum & 0x40) >> 6,
				adapter->bd_number);
		// n10 only support msix
		adapter->irq_mode = irq_mode_msix;
		break;

	case rnp_board_n500:
#define RNP_N500_BAR 2
		hw->hw_addr = pcim_iomap(pdev, RNP_N500_BAR, 0);
		if (!hw->hw_addr) {
			err = -EIO;
			goto err_ioremap;
		}
		dev_info(&pdev->dev,
				"[bar%d]:%p %llx len=%d kB\n",
				RNP_N500_BAR,
				hw->hw_addr,
				(unsigned long long)pci_resource_start(pdev, RNP_N500_BAR),
				(int)pci_resource_len(pdev, RNP_N500_BAR) / 1024);
				//

		 // n500 not support version
		hw->vfnum = rnpvf_vfnum_n500(hw);
		hw->ring_msix_base = hw->hw_addr + 0x24700;

		switch ((hw->vfnum & 0x60) >> 5) {
		case 0x00:
			adapter->port = adapter->bd_number = pf0_cards_found++;
			if (pf0_cards_found == 1000)
				pf0_cards_found = 0;
		break;
		case 0x01:
			adapter->port = adapter->bd_number = pf1_cards_found++;
			if (pf1_cards_found == 1000)
				pf1_cards_found = 0;
		break;
		case 0x02:
			adapter->port = adapter->bd_number = pf2_cards_found++;
			if (pf2_cards_found == 1000)
				pf2_cards_found = 0;
		break;
		case 0x03:
			adapter->port = adapter->bd_number = pf3_cards_found++;
			if (pf3_cards_found == 1000)
				pf3_cards_found = 0;
		break;


		}
		snprintf(adapter->name,
				sizeof(netdev->name),
				"%s%d%d",
				rnpvf_driver_name,
				(hw->vfnum & 0x60) >> 5,
				adapter->bd_number);

		//adapter->irq_mode = irq_mode;
		adapter->irq_mode = irq_mode_msix;
		//adapter->flags |= RNPVF_FLAG_MSI_CAPABLE;
		break;

	}

	pr_info("%s %s: vfnum:0x%x\n", adapter->name, pci_name(pdev), hw->vfnum);

	rnpvf_assign_netdev_ops(netdev);
	//netdev->netdev_ops = &rnpvf_netdev_ops;
	//rnpvf_set_ethtool_ops(netdev);
	//netdev->watchdog_timeo = 5 * HZ;
	strncpy(netdev->name, adapter->name, sizeof(netdev->name) - 1);
	// strncpy(netdev->name, "n10vf%", 6);

	/* Setup hw api */
	memcpy(&hw->mac.ops, ii->mac_ops, sizeof(hw->mac.ops));
	hw->mac.type = ii->mac;

	ii->get_invariants(hw);

	memcpy(&hw->mbx.ops, &rnpvf_mbx_ops, sizeof(struct rnp_mbx_operations));

	/* setup the private structure */
	err = rnpvf_sw_init(adapter);
	if (err)
		goto err_sw_init;

	/* The HW MAC address was set and/or determined in sw_init */
	if (!is_valid_ether_addr(netdev->dev_addr)) {
		pr_err("invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}
#ifdef HAVE_NETDEVICE_MIN_MAX_MTU
	/* MTU range: 68 - 9710 */
#ifdef HAVE_RHEL7_EXTENDED_MIN_MAX_MTU
	netdev->extended->min_mtu = hw->min_length;
	netdev->extended->max_mtu = hw->max_length-
		(ETH_HLEN + 2 * ETH_FCS_LEN);
#else
	netdev->min_mtu = hw->min_length;
	netdev->max_mtu = hw->max_length - (ETH_HLEN + 2 * ETH_FCS_LEN);
#endif
#endif

	netdev->mtu = hw->mtu;

#ifdef NETIF_F_GSO_PARTIAL

	if (hw->feature_flags & RNPVF_NET_FEATURE_SG)
		netdev->features |= NETIF_F_SG;
	if (hw->feature_flags & RNPVF_NET_FEATURE_TSO)
		netdev->features |= NETIF_F_TSO | NETIF_F_TSO6;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_HASH)
		netdev->features |= NETIF_F_RXHASH;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_CHECKSUM)
		netdev->features |= NETIF_F_RXCSUM;
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM) {
		netdev->features |= NETIF_F_HW_CSUM | NETIF_F_SCTP_CRC;
	}
#ifdef NETIF_F_GSO_UDP_L4
        if (hw->feature_flags & RNPVF_NET_FEATURE_USO) {
                netdev->features |= NETIF_F_GSO_UDP_L4;
        }
#endif

	
	netdev->features |= NETIF_F_HIGHDMA;
		
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_UDP_TUNNEL) {
		netdev->gso_partial_features = RNPVF_GSO_PARTIAL_FEATURES;
		netdev->features |= NETIF_F_GSO_PARTIAL | RNPVF_GSO_PARTIAL_FEATURES;
	}


	netdev->hw_features |= netdev->features;

	
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_FILTER)
		netdev->hw_features |= NETIF_F_HW_VLAN_CTAG_FILTER;
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_OFFLOAD) {
		netdev->hw_features |= NETIF_F_HW_VLAN_CTAG_RX
				    | NETIF_F_HW_VLAN_CTAG_TX;
	}
#endif

#ifdef NETIF_F_HW_VLAN_STAG_RX
	if (hw->feature_flags & RNPVF_NET_FEATURE_STAG_OFFLOAD) {
		netdev->hw_features |= NETIF_F_HW_VLAN_STAG_RX
				    | NETIF_F_HW_VLAN_STAG_TX;
	}
#endif
	netdev->hw_features |= NETIF_F_RXALL;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_NTUPLE_FILTER)
		netdev->hw_features |= NETIF_F_NTUPLE;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_FCS)
		netdev->hw_features |= NETIF_F_RXFCS;

	netdev->vlan_features |= netdev->features | NETIF_F_TSO_MANGLEID;
	netdev->hw_enc_features |= netdev->vlan_features;
	netdev->mpls_features |= NETIF_F_HW_CSUM;

	// some fixed feature control by pf
	if (hw->pf_feature & PF_FEATURE_VLAN_FILTER)
		netdev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;

	/*
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_FILTER)
		netdev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;
	*/
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_OFFLOAD) {
		netdev->features |= NETIF_F_HW_VLAN_CTAG_RX
				    | NETIF_F_HW_VLAN_CTAG_TX;
	}
#endif
#ifdef NETIF_F_HW_VLAN_STAG_RX
	if (hw->feature_flags & RNPVF_NET_FEATURE_STAG_OFFLOAD) {
		netdev->features |= NETIF_F_HW_VLAN_STAG_RX
				    | NETIF_F_HW_VLAN_STAG_TX;
	}
#endif

	netdev->priv_flags |= IFF_UNICAST_FLT;
	netdev->priv_flags |= IFF_SUPP_NOFCS;

#else /* NETIF_F_GSO_PARTIAL */

	if (hw->feature_flags & RNPVF_NET_FEATURE_SG)
		netdev->features |= NETIF_F_SG;
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM)
		netdev->features |= NETIF_F_IP_CSUM;

	netdev->features |= NETIF_F_HIGHDMA;

	netdev->features |= NETIF_F_GSO_UDP_TUNNEL
			 | NETIF_F_GSO_UDP_TUNNEL_CSUM;

#ifdef NETIF_F_IPV6_CSUM
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM)
		netdev->features |= NETIF_F_IPV6_CSUM;
#endif

        if (hw->feature_flags & RNPVF_NET_FEATURE_TSO)
                netdev->features |= NETIF_F_TSO | NETIF_F_TSO6;
#ifdef NETIF_F_GSO_UDP_L4
        if (hw->feature_flags & RNPVF_NET_FEATURE_USO)
                netdev->features |= NETIF_F_GSO_UDP_L4;
#endif


#ifdef NETIF_F_HW_VLAN_CTAG_TX

	/*
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_FILTER)
		netdev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;
	*/
	if (hw->feature_flags & RNPVF_NET_FEATURE_VLAN_OFFLOAD) {
		netdev->features |= NETIF_F_HW_VLAN_CTAG_RX
				 | NETIF_F_HW_VLAN_CTAG_TX;
	}
#endif
#ifdef NETIF_F_HW_VLAN_STAG_TX
	if (hw->feature_flags & RNPVF_NET_FEATURE_STAG_OFFLOAD) {
		netdev->features |= NETIF_F_HW_VLAN_STAG_RX
				 | NETIF_F_HW_VLAN_STAG_TX;
	}

#endif
	netdev->features |= rnpvf_tso_features(hw);

#ifdef NETIF_F_RXHASH
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_HASH)
		netdev->features |= NETIF_F_RXHASH;
#endif /* NETIF_F_RXHASH */

	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_CHECKSUM)
		netdev->features |= NETIF_F_RXCSUM;

#ifdef HAVE_NDO_SET_FEATURES
	/* copy netdev features into list of user selectable features */
#ifndef HAVE_RHEL6_NET_DEVICE_OPS_EXT
	hw_features = netdev->hw_features;
#else
	hw_features = get_netdev_hw_features(netdev);
#endif
	hw_features |= netdev->features;

	// fixed feature
#ifdef NETIF_F_HW_VLAN_CTAG_FILTER
	if (hw->pf_feature & PF_FEATURE_VLAN_FILTER)
		netdev->features |= NETIF_F_HW_VLAN_CTAG_FILTER;
#endif
	/* give us the option of enabling RSC/LRO later */
	//if (adapter->flags2 & RNPVF_FLAG2_RSC_CAPABLE)
	//	hw_features |= NETIF_F_LRO;
#else
#ifdef NETIF_F_GRO
	/* this is only needed on kernels prior to 2.6.39 */
	netdev->features |= NETIF_F_GRO;
#endif /* NETIF_F_GRO */
#endif /* HAVE_NDO_SET_FEATURES */

#ifdef HAVE_NDO_SET_FEATURES
	
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM)
		hw_features |= NETIF_F_SCTP_CSUM;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_NTUPLE_FILTER)
		hw_features |= NETIF_F_NTUPLE;

	//netdev->hw_features |= NETIF_F_RXALL;
	hw_features |= NETIF_F_RXALL;
	if (hw->feature_flags & RNPVF_NET_FEATURE_RX_FCS)
		hw_features |= NETIF_F_RXFCS;	
		//netdev->hw_features |= NETIF_F_RXFCS;	
#endif
#ifdef HAVE_NDO_SET_FEATURES
#ifdef HAVE_RHEL6_NET_DEVICE_OPS_EXT
	set_netdev_hw_features(netdev, hw_features);
#else
	netdev->hw_features = hw_features;
#endif
#endif


#ifdef HAVE_NETDEV_VLAN_FEATURES

	if (hw->feature_flags & RNPVF_NET_FEATURE_SG)
		netdev->vlan_features |= NETIF_F_SG;
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM)
		netdev->vlan_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
	if (hw->feature_flags & RNPVF_NET_FEATURE_TSO)
		netdev->vlan_features |= NETIF_F_TSO | NETIF_F_TSO6;
#ifdef NETIF_F_GSO_UDP_L4
        if (hw->feature_flags & RNPVF_NET_FEATURE_USO)
                netdev->vlan_features |= NETIF_F_GSO_UDP_L4;
#endif


#endif /* HAVE_NETDEV_VLAN_FEATURES */

#ifdef HAVE_ENCAP_CSUM_OFFLOAD
	if (hw->feature_flags & RNPVF_NET_FEATURE_SG)
		netdev->hw_enc_features |= NETIF_F_SG;
#endif /* HAVE_ENCAP_CSUM_OFFLOAD */


#ifdef HAVE_VXLAN_RX_OFFLOAD
	if (hw->feature_flags & RNPVF_NET_FEATURE_TX_CHECKSUM) {
		netdev->hw_enc_features |= 
			NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
	}

#endif /* HAVE_VXLAN_RX_OFFLOAD */

#endif /* NETIF_F_GSO_PARTIAL */

#ifdef IFF_UNICAST_FLT
	netdev->priv_flags |= IFF_UNICAST_FLT;
#endif
#ifdef IFF_SUPP_NOFCS
	netdev->priv_flags |= IFF_SUPP_NOFCS;
#endif

	// timer_setup(&adapter->service_timer, &rnpvf_service_timer, (unsigned
	// long) adapter);
	timer_setup(&adapter->watchdog_timer, rnpvf_watchdog, 0);

	//INIT_WORK(&adapter->reset_task, rnpvf_reset_task);
	INIT_WORK(&adapter->watchdog_task, rnpvf_watchdog_task);

	err = rnpvf_init_interrupt_scheme(adapter);
	if (err)
		goto err_sw_init;

	err = register_mbx_irq(adapter);
	if (err)
		goto err_register;

	if (fix_eth_name){
		strncpy(netdev->name, adapter->name, sizeof(netdev->name) - 1);
	}else{
		strscpy(netdev->name, pci_name(pdev), sizeof(netdev->name));
		strscpy(netdev->name, "eth%d", sizeof(netdev->name));
	}
	err = register_netdev(netdev);
	if (err) {
		rnpvf_err("register_netdev faild!\n");
		dev_err(&pdev->dev,
				"%s %s: vfnum:0x%x. register_netdev faild!\n",
				adapter->name,
				pci_name(pdev),
				hw->vfnum);
		goto err_register;
	}

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);

	rnpvf_init_last_counter_stats(adapter);

	rnpvf_sysfs_init(netdev);

	/* print the MAC address */
	hw_dbg(hw, "%pM\n", netdev->dev_addr);

	hw_dbg(hw, "Mucse(R) n10 Virtual Function\n");

	return 0;
err_register:
	remove_mbx_irq(adapter);
	rnpvf_clear_interrupt_scheme(adapter);
err_sw_init:
	// rnpvf_reset_interrupt_capability(adapter);
err_ioremap:
	free_netdev(netdev);

	dev_err(&pdev->dev, "%s faild. err:%d\n", __func__, err);
	return err;
}

static int rnpvf_rm_adpater(struct rnpvf_adapter *adapter)
{
	struct net_device *netdev;

	if (!adapter)
		return -EINVAL;

	rnpvf_info("= remove adapter:%s =\n", adapter->name);
	netdev = adapter->netdev;

	if (netdev) {
		netif_carrier_off(netdev);
		rnpvf_sysfs_exit(netdev);
	}

	//set_bit(__RNPVF_DOWN, &adapter->state);
	set_bit(__RNPVF_REMOVE, &adapter->state);
	del_timer_sync(&adapter->watchdog_timer);

	//cancel_work_sync(&adapter->reset_task);
	cancel_work_sync(&adapter->watchdog_task);

	if (netdev) {
		if (netdev->reg_state == NETREG_REGISTERED)
			unregister_netdev(netdev);
	}

	remove_mbx_irq(adapter);
	rnpvf_clear_interrupt_scheme(adapter);
	rnpvf_reset_interrupt_capability(adapter);

	free_netdev(netdev);

	rnpvf_info("remove %s  complete\n", adapter->name);

	return 0;
}

/**
 * rnpvf_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in rnpvf_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * rnpvf_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
static int rnpvf_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct rnpvf_adapter *adapter = NULL;
	const struct rnpvf_info *ii = rnpvf_info_tbl[ent->driver_data];
	int err;
	err = pci_enable_device_mem(pdev);
	//err = pcim_enable_device(pdev);
	if (err)
		return err;

	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(56)) &&
		!dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(56))) {
		pci_using_hi_dma = 1;
	} else {
		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
			if (err) {
				dev_err(&pdev->dev,
						"No usable DMA "
						"configuration, aborting\n");
				goto err_dma;
			}
		}
		pci_using_hi_dma = 0;
	}

	err = pci_request_mem_regions(pdev, rnpvf_driver_name);
//	err = pci_request_selected_regions(
//		pdev, pci_select_bars(pdev, IORESOURCE_MEM), rnpvf_driver_name);
	if (err) {
		dev_err(&pdev->dev, "pci_request_selected_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}

	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);
	pci_save_state(pdev);

	err = rnpvf_add_adpater(pdev, ii, &adapter);
	if (err) {
		dev_err(&pdev->dev, "ERROR %s: %d\n", __func__, __LINE__);
		goto err_regions;
	}

	return 0;

err_regions:
	//pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_release_mem_regions(pdev);
err_dma:
err_pci_reg:
	return err;
}

/**
 * rnpvf_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * rnpvf_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/
static void rnpvf_remove(struct pci_dev *pdev)
{
	struct rnpvf_adapter *adapter = pci_get_drvdata(pdev);

	rnpvf_rm_adpater(adapter);

	//pci_release_regions(pdev);

	//pci_disable_device(pdev);
        pci_release_mem_regions(pdev);
        pci_disable_pcie_error_reporting(pdev);
        pci_disable_device(pdev);
}

/**
 * rnpvf_io_error_detected - called when PCI error is detected
 * @pdev: Pointer to PCI device
 * @state: The current pci connection state
 *
 * This function is called after a PCI bus error affecting
 * this device has been detected.
 */
static pci_ers_result_t rnpvf_io_error_detected(struct pci_dev *pdev,
												pci_channel_state_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

	netif_device_detach(netdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (netif_running(netdev))
		rnpvf_down(adapter);

	pci_disable_device(pdev);

	/* Request a slot slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

/**
 * rnpvf_io_slot_reset - called after the pci bus has been reset.
 * @pdev: Pointer to PCI device
 *
 * Restart the card from scratch, as if from a cold-boot. Implementation
 * resembles the first-half of the rnpvf_resume routine.
 */
static pci_ers_result_t rnpvf_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

	if (pci_enable_device_mem(pdev)) {
		dev_err(&pdev->dev, "Cannot re-enable PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);

	rnpvf_reset(adapter);

	return PCI_ERS_RESULT_RECOVERED;
}

/**
 * rnpvf_io_resume - called when traffic can start flowing again.
 * @pdev: Pointer to PCI device
 *
 * This callback is called when the error recovery driver tells us that
 * its OK to resume normal operation. Implementation resembles the
 * second-half of the rnpvf_resume routine.
 */
static void rnpvf_io_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct rnpvf_adapter *adapter = netdev_priv(netdev);

	if (netif_running(netdev))
		rnpvf_up(adapter);

	netif_device_attach(netdev);
}

/* PCI Error Recovery (ERS) */
static const struct pci_error_handlers rnpvf_err_handler = {
	.error_detected = rnpvf_io_error_detected,
	.slot_reset = rnpvf_io_slot_reset,
	.resume = rnpvf_io_resume,
};

static struct pci_driver rnpvf_driver = {
	.name = rnpvf_driver_name,
	.id_table = rnpvf_pci_tbl,
	.probe = rnpvf_probe,
	.remove = rnpvf_remove,
#ifdef CONFIG_PM
	/* Power Management Hooks */
	.suspend = rnpvf_suspend,
	.resume = rnpvf_resume,
#endif
	.shutdown = rnpvf_shutdown,
	.err_handler = &rnpvf_err_handler,
};

#ifdef DEBUG
/**
 * rnpvf_get_hw_dev_name - return device name string
 * used by hardware layer to print debugging information
 **/
char *rnpvf_get_hw_dev_name(struct rnpvf_hw *hw)
{
	struct rnpvf_adapter *adapter = hw->back;
	return adapter->netdev->name;
}

#endif

/**
 * rnpvf_init_module - Driver Registration Routine
 *
 * rnpvf_init_module is the first routine called when the driver is
 * loaded. All it does is register with the PCI subsystem.
 **/
static int __init rnpvf_init_module(void)
{
	int ret;
	pr_info("%s - version %s\n", rnpvf_driver_string, rnpvf_driver_version);

	pr_info("%s\n", rnpvf_copyright);

	ret = pci_register_driver(&rnpvf_driver);
	return ret;
}

module_init(rnpvf_init_module);

/**
 * rnpvf_exit_module - Driver Exit Cleanup Routine
 *
 * rnpvf_exit_module is called just before the driver is removed
 * from memory.
 **/
static void __exit rnpvf_exit_module(void)
{
	pci_unregister_driver(&rnpvf_driver);
}

module_exit(rnpvf_exit_module);

/* rnpvf_main.c */
