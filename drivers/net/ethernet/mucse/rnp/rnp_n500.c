#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "rnp.h"
#include "rnp_phy.h"
#include "rnp_mbx.h"
#include "rnp_pcs.h"
#include "rnp_ethtool.h"
#include "rnp_sriov.h"

//#include <linux/iopoll.h>


#define RNP_N500_PKT_LEN_ERR (2)
#define RNP_N500_HDR_LEN_ERR (1)
#define RNP_N500_MAX_VF 8
#define RNP_N500_RSS_TBL_NUM 128
#define RNP_N500_RSS_TC_TBL_NUM 8
#define RNP_N500_MAX_TX_QUEUES 8
#define RNP_N500_MAX_RX_QUEUES 8
#ifdef NIC_VF_FXIED
#define RNP_N500_RAR_ENTRIES 32
#else
#define RNP_N500_RAR_ENTRIES 32
#endif
#define RNP_N500_MC_TBL_SIZE 128
#define RNP_N500_VFT_TBL_SIZE 128
#define RNP_N500_MSIX_VECTORS 32

#define RNP500_MAX_LAYER2_FILTERS 16
#define RNP500_MAX_TUPLE5_FILTERS 128

#ifdef HAVE_ETHTOOL_GET_SSET_COUNT

enum n500_priv_bits
{
	n500_mac_loopback = 0,
	n500_padding_enable = 8,
};

static const char rnp500_priv_flags_strings[][ETH_GSTRING_LEN] = {
#define RNP500_MAC_LOOPBACK	  BIT(0)
#define RNP500_FT_PADDING	  BIT(1)
#define RNP500_PADDING_DEBUG	  BIT(2)
#define RNP500_SIMULATE_DOWN	  BIT(3)
#define RNP500_ULTRA_SHORT        BIT(4)
#define RNP500_DOUBLE_VLAN        BIT(5)
#define RNP500_PAUSE_OWN 	  BIT(6)
#define RNP500_STAGS_ENABLE 	  BIT(7)
#define RNP500_JUMBO_ENABLE       BIT(8)
#define RNP500_TX_PADDING         BIT(9)
#define RNP500_REC_HDR_LEN_ERR     BIT(10)
#define RNP500_DOUBLE_VLAN_RECEIVE        BIT(11)
#define RNP500_RX_SKIP_EN        BIT(12)
#define RNP500_TCP_SYNC_PRIO     BIT(13)
#define RNP500_REMAP_PRIO     BIT(14)
#define RNP500_8023_PRIO     BIT(15)
#define RNP500_SRIOV_VLAN_MODE     BIT(16)
	"mac_loopback",
	"pcie_patch",
	"padding_debug",
	"simulate_link_down",
	"ultra_short_packet",
	"double_vlan",
	"pause_use_own_address",
	"stags_enable",
	"jumbo_enable",
	"tx_padding_off",
	"mask_len_err",
	"double_vlan_receive",
	"rx_skip_en",
	"tcp_sync_prio",
	"remap_prio",
	"8023_prio",
	"sriov_vlan_mode"};

#define RNP500_PRIV_FLAGS_STR_LEN ARRAY_SIZE(rnp500_priv_flags_strings)
#endif
__maybe_unused static bool rnp_mng_enabled(struct rnp_hw *hw)
{
	return false;
}

__maybe_unused static void rnp_init_mac_link_ops_n500(struct rnp_hw *hw)
{
}


/**
 *  rnp_init_phy_ops_n500 - PHY/SFP specific init
 *  @hw: pointer to hardware structure
 *
 *  Initialize any function pointers that were not able to be
 *  set during get_invariants because the PHY/SFP type was
 *  not known.  Perform the SFP init if necessary.
 *
 **/
static s32 rnp_init_phy_ops_n500(struct rnp_hw *hw)
{
	
	return 0;
}

__maybe_unused static s32 rnp_setup_sfp_modules_n500(struct rnp_hw *hw)
{
	return 0;
}

/**
 *  rnp_reinit_fdir_tables_n500 - Reinitialize Flow Director tables.
 *  @hw: pointer to hardware structure
 **/
s32 rnp_reinit_fdir_tables_n500(struct rnp_hw *hw)
{
	return 0;
}

/**
 *  rnp_fdir_enable_n500 - Initialize Flow Director control registers
 *  @hw: pointer to hardware structure
 *  @fdirctrl: value to write to flow director control register
 **/
__maybe_unused static void rnp_fdir_enable_n500(struct rnp_hw *hw, u32 fdirctrl)
{
}



/*
 * These defines allow us to quickly generate all of the necessary instructions
 * in the function below by simply calling out RNP_COMPUTE_SIG_HASH_ITERATION
 * for values 0 through 15
 */
#define RNP_ATR_COMMON_HASH_KEY                                                \
	(RNP_ATR_BUCKET_HASH_KEY & RNP_ATR_SIGNATURE_HASH_KEY)
#define RNP_COMPUTE_SIG_HASH_ITERATION(_n)                                     \
	do {                                                                   \
	} while (0)

/**
 *  rnp_atr_compute_sig_hash_n500 - Compute the signature hash
 *  @stream: input bitstream to compute the hash on
 *
 *  This function is almost identical to the function above but contains
 *  several optomizations such as unwinding all of the loops, letting the
 *  compiler work out all of the conditional ifs since the keys are static
 *  defines, and computing two keys at once since the hashed dword stream
 *  will be the same for both keys.
 **/
__maybe_unused static u32 rnp_atr_compute_sig_hash_n500(union rnp_atr_hash_dword input,
					  union rnp_atr_hash_dword common)
{
#if 0
	u32 hi_hash_dword, lo_hash_dword, flow_vm_vlan;
	u32 sig_hash = 0, bucket_hash = 0, common_hash = 0;

	/* record the flow_vm_vlan bits as they are a key part to the hash */
	flow_vm_vlan = ntohl(input.dword);

	/* generate common hash dword */
	hi_hash_dword = ntohl(common.dword);

	/* low dword is word swapped version of common */
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	/* apply flow ID/VM pool/VLAN ID bits to hash words */
	hi_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan >> 16);

	/* Process bits 0 and 16 */
	RNP_COMPUTE_SIG_HASH_ITERATION(0);

	/*
	 * apply flow ID/VM pool/VLAN ID bits to lo hash dword, we had to
	 * delay this because bit 0 of the stream should not be processed
	 * so we do not add the vlan until after bit 0 was processed
	 */
	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);

	/* Process remaining 30 bit of the key */
	RNP_COMPUTE_SIG_HASH_ITERATION(1);
	RNP_COMPUTE_SIG_HASH_ITERATION(2);
	RNP_COMPUTE_SIG_HASH_ITERATION(3);
	RNP_COMPUTE_SIG_HASH_ITERATION(4);
	RNP_COMPUTE_SIG_HASH_ITERATION(5);
	RNP_COMPUTE_SIG_HASH_ITERATION(6);
	RNP_COMPUTE_SIG_HASH_ITERATION(7);
	RNP_COMPUTE_SIG_HASH_ITERATION(8);
	RNP_COMPUTE_SIG_HASH_ITERATION(9);
	RNP_COMPUTE_SIG_HASH_ITERATION(10);
	RNP_COMPUTE_SIG_HASH_ITERATION(11);
	RNP_COMPUTE_SIG_HASH_ITERATION(12);
	RNP_COMPUTE_SIG_HASH_ITERATION(13);
	RNP_COMPUTE_SIG_HASH_ITERATION(14);
	RNP_COMPUTE_SIG_HASH_ITERATION(15);

	/* combine common_hash result with signature and bucket hashes */
	bucket_hash ^= common_hash;
	bucket_hash &= RNP_ATR_HASH_MASK;

	sig_hash ^= common_hash << 16;
	sig_hash &= RNP_ATR_HASH_MASK << 16;

	/* return completed signature hash */
	return sig_hash ^ bucket_hash;
#else
	return 0;
#endif
}

/**
 *  rnp_atr_add_signature_filter_n500 - Adds a signature hash filter
 *  @hw: pointer to hardware structure
 *  @input: unique input dword
 *  @common: compressed common input dword
 *  @queue: queue index to direct traffic to
 **/
s32 rnp_fdir_add_signature_filter_n500(struct rnp_hw *hw,
					union rnp_atr_hash_dword input,
					union rnp_atr_hash_dword common,
					u8 queue)
{
#if 0
	u64 fdirhashcmd;
	u32 fdircmd;
	/*
	 * Get the flow_type in order to program FDIRCMD properly
	 * lowest 2 bits are FDIRCMD.L4TYPE, third lowest bit is FDIRCMD.IPV6
	 */
	switch (input.formatted.flow_type) {
	case RNP_ATR_FLOW_TYPE_TCPV4:
	case RNP_ATR_FLOW_TYPE_UDPV4:
	case RNP_ATR_FLOW_TYPE_SCTPV4:
	case RNP_ATR_FLOW_TYPE_TCPV6:
	case RNP_ATR_FLOW_TYPE_UDPV6:
	case RNP_ATR_FLOW_TYPE_SCTPV6:
		break;
	default:
		hw_dbg(hw, " Error on flow type input\n");
		return RNP_ERR_CONFIG;
	}

	/* configure FDIRCMD register */
	fdircmd = RNP_FDIRCMD_CMD_ADD_FLOW | RNP_FDIRCMD_FILTER_UPDATE |
		RNP_FDIRCMD_LAST | RNP_FDIRCMD_QUEUE_EN;
	fdircmd |= input.formatted.flow_type << RNP_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (u32)queue << RNP_FDIRCMD_RX_QUEUE_SHIFT;

	/*
	 * The lower 32-bits of fdirhashcmd is for FDIRHASH, the upper 32-bits
	 * is for FDIRCMD.  Then do a 64-bit register write from FDIRHASH.
	 */
	fdirhashcmd = (u64)fdircmd << 32;
	fdirhashcmd |= rnp_atr_compute_sig_hash_n500(input, common);
	//RNP_WRITE_REG64(hw, RNP_FDIRHASH, fdirhashcmd);

	hw_dbg(hw, "Tx Queue=%x hash=%x\n", queue, (u32)fdirhashcmd);
#endif
	return 0;
}

#define RNP_COMPUTE_BKT_HASH_ITERATION(_n)                                     \
	do {                                                                   \
		u32 n = (_n);                                                  \
		if (RNP_ATR_BUCKET_HASH_KEY & (0x01 << n))                     \
			bucket_hash ^= lo_hash_dword >> n;                     \
		if (RNP_ATR_BUCKET_HASH_KEY & (0x01 << (n + 16)))              \
			bucket_hash ^= hi_hash_dword >> n;                     \
	} while (0)

/**
 *  rnp_atr_compute_perfect_hash_n500 - Compute the perfect filter hash
 *  @atr_input: input bitstream to compute the hash on
 *  @input_mask: mask for the input bitstream
 *
 *  This function serves two main purposes.  First it applys the input_mask
 *  to the atr_input resulting in a cleaned up atr_input data stream.
 *  Secondly it computes the hash and stores it in the bkt_hash field at
 *  the end of the input byte stream.  This way it will be available for
 *  future use without needing to recompute the hash.
 **/
void rnp_atr_compute_perfect_hash_n500(union rnp_atr_input *input,
					union rnp_atr_input *input_mask)
{
#if 0
	u32 hi_hash_dword, lo_hash_dword, flow_vm_vlan;
	u32 bucket_hash = 0;

	/* Apply masks to input data */
	input->dword_stream[0]  &= input_mask->dword_stream[0];
	input->dword_stream[1]  &= input_mask->dword_stream[1];
	input->dword_stream[2]  &= input_mask->dword_stream[2];
	input->dword_stream[3]  &= input_mask->dword_stream[3];
	input->dword_stream[4]  &= input_mask->dword_stream[4];
	input->dword_stream[5]  &= input_mask->dword_stream[5];
	input->dword_stream[6]  &= input_mask->dword_stream[6];
	input->dword_stream[7]  &= input_mask->dword_stream[7];
	input->dword_stream[8]  &= input_mask->dword_stream[8];
	input->dword_stream[9]  &= input_mask->dword_stream[9];
	input->dword_stream[10] &= input_mask->dword_stream[10];

	/* record the flow_vm_vlan bits as they are a key part to the hash */
	flow_vm_vlan = ntohl(input->dword_stream[0]);

	/* generate common hash dword */
	hi_hash_dword = ntohl(input->dword_stream[1] ^
			input->dword_stream[2] ^
			input->dword_stream[3] ^
			input->dword_stream[4] ^
			input->dword_stream[5] ^
			input->dword_stream[6] ^
			input->dword_stream[7] ^
			input->dword_stream[8] ^
			input->dword_stream[9] ^
			input->dword_stream[10]);

	/* low dword is word swapped version of common */
	lo_hash_dword = (hi_hash_dword >> 16) | (hi_hash_dword << 16);

	/* apply flow ID/VM pool/VLAN ID bits to hash words */
	hi_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan >> 16);

	/* Process bits 0 and 16 */
	RNP_COMPUTE_BKT_HASH_ITERATION(0);

	/*
	 * apply flow ID/VM pool/VLAN ID bits to lo hash dword, we had to
	 * delay this because bit 0 of the stream should not be processed
	 * so we do not add the vlan until after bit 0 was processed
	 */
	lo_hash_dword ^= flow_vm_vlan ^ (flow_vm_vlan << 16);

	/* Process remaining 30 bit of the key */
	RNP_COMPUTE_BKT_HASH_ITERATION(1);
	RNP_COMPUTE_BKT_HASH_ITERATION(2);
	RNP_COMPUTE_BKT_HASH_ITERATION(3);
	RNP_COMPUTE_BKT_HASH_ITERATION(4);
	RNP_COMPUTE_BKT_HASH_ITERATION(5);
	RNP_COMPUTE_BKT_HASH_ITERATION(6);
	RNP_COMPUTE_BKT_HASH_ITERATION(7);
	RNP_COMPUTE_BKT_HASH_ITERATION(8);
	RNP_COMPUTE_BKT_HASH_ITERATION(9);
	RNP_COMPUTE_BKT_HASH_ITERATION(10);
	RNP_COMPUTE_BKT_HASH_ITERATION(11);
	RNP_COMPUTE_BKT_HASH_ITERATION(12);
	RNP_COMPUTE_BKT_HASH_ITERATION(13);
	RNP_COMPUTE_BKT_HASH_ITERATION(14);
	RNP_COMPUTE_BKT_HASH_ITERATION(15);

	/*
	 * Limit hash to 13 bits since max bucket count is 8K.
	 * Store result at the end of the input stream.
	 */
	input->formatted.bkt_hash = bucket_hash & 0x1FFF;
#endif
}

/**
 *  rnp_get_fdirtcpm_n500 - generate a tcp port from atr_input_masks
 *  @input_mask: mask to be bit swapped
 *
 *  The source and destination port masks for flow director are bit swapped
 *  in that bit 15 effects bit 0, 14 effects 1, 13, 2 etc.  In order to
 *  generate a correctly swapped value we need to bit swap the mask and that
 *  is what is accomplished by this function.
 **/
__maybe_unused static u32 rnp_get_fdirtcpm_n500(union rnp_atr_input *input_mask)
{
#if 0
	u32 mask = ntohs(input_mask->formatted.dst_port);

	mask <<= RNP_FDIRTCPM_DPORTM_SHIFT;
	mask |= ntohs(input_mask->formatted.src_port);
	mask = ((mask & 0x55555555) << 1) | ((mask & 0xAAAAAAAA) >> 1);
	mask = ((mask & 0x33333333) << 2) | ((mask & 0xCCCCCCCC) >> 2);
	mask = ((mask & 0x0F0F0F0F) << 4) | ((mask & 0xF0F0F0F0) >> 4);
	return ((mask & 0x00FF00FF) << 8) | ((mask & 0xFF00FF00) >> 8);
#else
	return 0;
#endif
}

/*
 * These two macros are meant to address the fact that we have registers
 * that are either all or in part big-endian.  As a result on big-endian
 * systems we will end up byte swapping the value to little-endian before
 * it is byte swapped again and written to the hardware in the original
 * big-endian format.
 */
#define RNP_STORE_AS_BE32(_value)                                              \
	(((u32)(_value) >> 24) | (((u32)(_value)&0x00FF0000) >> 8) |           \
	 (((u32)(_value)&0x0000FF00) << 8) | ((u32)(_value) << 24))

#define RNP_WRITE_REG_BE32(a, reg, value)                                      \
	RNP_WRITE_REG((a), (reg), RNP_STORE_AS_BE32(ntohl(value)))

#define RNP_STORE_AS_BE16(_value)                                              \
	ntohs(((u16)(_value) >> 8) | ((u16)(_value) << 8))

s32 rnp_fdir_set_input_mask_n500(struct rnp_hw *hw,
				  union rnp_atr_input *input_mask)
{
#if 0
	/* mask IPv6 since it is currently not supported */
	u32 fdirm = RNP_FDIRM_DIPv6;
	u32 fdirtcpm;

	/*
	 * Program the relevant mask registers.  If src/dst_port or src/dst_addr
	 * are zero, then assume a full mask for that field.  Also assume that
	 * a VLAN of 0 is unspecified, so mask that out as well.  L4type
	 * cannot be masked out in this implementation.
	 *
	 * This also assumes IPv4 only.  IPv6 masking isn't supported at this
	 * point in time.
	 */

	/* verify bucket hash is cleared on hash generation */
	if (input_mask->formatted.bkt_hash)
		hw_dbg(hw, " bucket hash should always be 0 in mask\n");

	/* Program FDIRM and verify partial masks */
	switch (input_mask->formatted.vm_pool & 0x7F) {
	case 0x0:
		fdirm |= RNP_FDIRM_POOL;
	case 0x7F:
		break;
	default:
		hw_dbg(hw, " Error on vm pool mask\n");
		return RNP_ERR_CONFIG;
	}

	switch (input_mask->formatted.flow_type & RNP_ATR_L4TYPE_MASK) {
	case 0x0:
		fdirm |= RNP_FDIRM_L4P;
		if (input_mask->formatted.dst_port ||
				input_mask->formatted.src_port) {
			hw_dbg(hw, " Error on src/dst port mask\n");
			return RNP_ERR_CONFIG;
		}
	case RNP_ATR_L4TYPE_MASK:
		break;
	default:
		hw_dbg(hw, " Error on flow type mask\n");
		return RNP_ERR_CONFIG;
	}

	switch (ntohs(input_mask->formatted.vlan_id) & 0xEFFF) {
	case 0x0000:
		/* mask VLAN ID, fall through to mask VLAN priority */
		fdirm |= RNP_FDIRM_VLANID;
	case 0x0FFF:
		/* mask VLAN priority */
		fdirm |= RNP_FDIRM_VLANP;
		break;
	case 0xE000:
		/* mask VLAN ID only, fall through */
		fdirm |= RNP_FDIRM_VLANID;
	case 0xEFFF:
		/* no VLAN fields masked */
		break;
	default:
		hw_dbg(hw, " Error on VLAN mask\n");
		return RNP_ERR_CONFIG;
	}

	switch (input_mask->formatted.flex_bytes & 0xFFFF) {
	case 0x0000:
		/* Mask Flex Bytes, fall through */
		fdirm |= RNP_FDIRM_FLEX;
	case 0xFFFF:
		break;
	default:
		hw_dbg(hw, " Error on flexible byte mask\n");
		return RNP_ERR_CONFIG;
	}

	/* Now mask VM pool and destination IPv6 - bits 5 and 2 */
	RNP_WRITE_REG(hw, RNP_FDIRM, fdirm);

	/* store the TCP/UDP port masks, bit reversed from port layout */
	fdirtcpm = rnp_get_fdirtcpm_n500(input_mask);

	/* write both the same so that UDP and TCP use the same mask */
	RNP_WRITE_REG(hw, RNP_FDIRTCPM, ~fdirtcpm);
	RNP_WRITE_REG(hw, RNP_FDIRUDPM, ~fdirtcpm);

	/* store source and destination IP masks (big-enian) */
	RNP_WRITE_REG_BE32(hw, RNP_FDIRSIP4M,
			~input_mask->formatted.src_ip[0]);
	RNP_WRITE_REG_BE32(hw, RNP_FDIRDIP4M,
			~input_mask->formatted.dst_ip[0]);

#endif
	return 0;
}

s32 rnp_fdir_write_perfect_filter_n500(struct rnp_hw *hw,
					union rnp_atr_input *input, u16 soft_id,
					u8 queue)
{
#if 0
	u32 fdirport, fdirvlan, fdirhash, fdircmd;

	/* currently IPv6 is not supported, must be programmed with 0 */
	RNP_WRITE_REG_BE32(hw, RNP_FDIRSIPv6(0),
			input->formatted.src_ip[0]);
	RNP_WRITE_REG_BE32(hw, RNP_FDIRSIPv6(1),
			input->formatted.src_ip[1]);
	RNP_WRITE_REG_BE32(hw, RNP_FDIRSIPv6(2),
			input->formatted.src_ip[2]);

	/* record the source address (big-endian) */
	RNP_WRITE_REG_BE32(hw, RNP_FDIRIPSA, input->formatted.src_ip[0]);

	/* record the first 32 bits of the destination address (big-endian) */
	RNP_WRITE_REG_BE32(hw, RNP_FDIRIPDA, input->formatted.dst_ip[0]);

	/* record source and destination port (little-endian)*/
	fdirport = ntohs(input->formatted.dst_port);
	fdirport <<= RNP_FDIRPORT_DESTINATION_SHIFT;
	fdirport |= ntohs(input->formatted.src_port);
	RNP_WRITE_REG(hw, RNP_FDIRPORT, fdirport);

	/* record vlan (little-endian) and flex_bytes(big-endian) */
	fdirvlan = RNP_STORE_AS_BE16(input->formatted.flex_bytes);
	fdirvlan <<= RNP_FDIRVLAN_FLEX_SHIFT;
	fdirvlan |= ntohs(input->formatted.vlan_id);
	RNP_WRITE_REG(hw, RNP_FDIRVLAN, fdirvlan);

	/* configure FDIRHASH register */
	fdirhash = input->formatted.bkt_hash;
	fdirhash |= soft_id << RNP_FDIRHASH_SIG_SW_INDEX_SHIFT;
	RNP_WRITE_REG(hw, RNP_FDIRHASH, fdirhash);

	/*
	 * flush all previous writes to make certain registers are
	 * programmed prior to issuing the command
	 */
	RNP_WRITE_FLUSH(hw);

	/* configure FDIRCMD register */
	fdircmd = RNP_FDIRCMD_CMD_ADD_FLOW | RNP_FDIRCMD_FILTER_UPDATE |
		RNP_FDIRCMD_LAST | RNP_FDIRCMD_QUEUE_EN;
	if (queue == RNP_FDIR_DROP_QUEUE)
		fdircmd |= RNP_FDIRCMD_DROP;
	fdircmd |= input->formatted.flow_type << RNP_FDIRCMD_FLOW_TYPE_SHIFT;
	fdircmd |= (u32)queue << RNP_FDIRCMD_RX_QUEUE_SHIFT;
	fdircmd |= (u32)input->formatted.vm_pool << RNP_FDIRCMD_VT_POOL_SHIFT;

	RNP_WRITE_REG(hw, RNP_FDIRCMD, fdircmd);
#endif
	return 0;
}

s32 rnp_fdir_erase_perfect_filter_n500(struct rnp_hw *hw,
					union rnp_atr_input *input, u16 soft_id)
{
	s32 err = 0;
#if 0
	u32 fdirhash;
	u32 fdircmd = 0;
	u32 retry_count;

	/* configure FDIRHASH register */
	fdirhash = input->formatted.bkt_hash;
	fdirhash |= soft_id << RNP_FDIRHASH_SIG_SW_INDEX_SHIFT;
	RNP_WRITE_REG(hw, RNP_FDIRHASH, fdirhash);

	/* flush hash to HW */
	RNP_WRITE_FLUSH(hw);

	/* Query if filter is present */
	RNP_WRITE_REG(hw, RNP_FDIRCMD, RNP_FDIRCMD_CMD_QUERY_REM_FILT);

	for (retry_count = 10; retry_count; retry_count--) {
		/* allow 10us for query to process */
		udelay(10);
		/* verify query completed successfully */
		fdircmd = RNP_READ_REG(hw, RNP_FDIRCMD);
		if (!(fdircmd & RNP_FDIRCMD_CMD_MASK))
			break;
	}

	if (!retry_count)
		err = RNP_ERR_FDIR_REINIT_FAILED;

	/* if filter exists in hardware then remove it */
	if (fdircmd & RNP_FDIRCMD_FILTER_VALID) {
		RNP_WRITE_REG(hw, RNP_FDIRHASH, fdirhash);
		RNP_WRITE_FLUSH(hw);
		RNP_WRITE_REG(hw, RNP_FDIRCMD,
				RNP_FDIRCMD_CMD_REMOVE_FLOW);
	}
#endif
	return err;
}

/**
 *  rnp_identify_phy_n500 - Get physical layer module
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 *  If PHY already detected, maintains current PHY type in hw struct,
 *  otherwise executes the PHY detection routine.
 **/
static s32 rnp_identify_phy_n500(struct rnp_hw *hw)
{
	//s32 status = RNP_ERR_PHY_ADDR_INVALID;

	hw->phy.type = rnp_phy_sfp;

	return 0;
}

static s32 rnp_identify_sfp_module_n500(struct rnp_hw *hw)
{
	hw->phy.sfp_type = rnp_sfp_type_da_cu;

	return 0;
}


/**
 *  rnp_verify_fw_version_n500 - verify fw version for n500
 *  @hw: pointer to hardware structure
 *
 *  Verifies that installed the firmware version is 0.6 or higher
 *  for SFI devices. All n500 SFI devices should have version 0.6 or higher.
 *
 *  Returns RNP_ERR_EEPROM_VERSION if the FW is not present or
 *  if the FW version is not supported.
 **/
__maybe_unused static s32 rnp_verify_fw_version_n500(struct rnp_hw *hw)
{
	//s32 status = RNP_ERR_EEPROM_VERSION;
	//u16 fw_offset, fw_ptp_cfg_offset;
	//u16 fw_version = 0;

	return 0;
}

/**
 *  rnp_verify_lesm_fw_enabled_n500 - Checks LESM FW module state.
 *  @hw: pointer to hardware structure
 *
 *  Returns true if the LESM FW module is present and enabled. Otherwise
 *  returns false. Smart Speed must be disabled if LESM FW module is enabled.
 **/
bool rnp_verify_lesm_fw_enabled_n500(struct rnp_hw *hw)
{
	bool lesm_enabled = false;

	return lesm_enabled;
}

/**
 *  rnp_read_eeprom_buffer_n500 - Read EEPROM word(s) using
 *  fastest available method
 *
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in EEPROM to read
 *  @words: number of words
 *  @data: word(s) read from the EEPROM
 *
 *  Retrieves 16 bit word(s) read from EEPROM
 **/
__maybe_unused static s32 rnp_read_eeprom_buffer_n500(struct rnp_hw *hw, u16 offset,
					u16 words, u16 *data)
{
	s32 ret_val = RNP_ERR_CONFIG;
#if 0
	struct rnp_eeprom_info *eeprom = &hw->eeprom;

	/*
	 * If EEPROM is detected and can be addressed using 14 bits,
	 * use EERD otherwise use bit bang
	 */
	if ((eeprom->type == rnp_eeprom_spi) &&
			(offset + (words - 1) <= RNP_EERD_MAX_ADDR))
		ret_val = rnp_read_eerd_buffer_generic(hw, offset, words,
				data);
	else
		ret_val = rnp_read_eeprom_buffer_bit_bang_generic(hw, offset,
				words,
				data);

#endif
	return ret_val;
}

/**
 *  rnp_read_eeprom_n500 - Read EEPROM word using
 *  fastest available method
 *
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM
 **/
__maybe_unused static s32 rnp_read_eeprom_n500(struct rnp_hw *hw, u16 offset, u16 *data)
{
	s32 ret_val = RNP_ERR_CONFIG;

#if 0
	struct rnp_eeprom_info *eeprom = &hw->eeprom;
	/*
	 * If EEPROM is detected and can be addressed using 14 bits,
	 * use EERD otherwise use bit bang
	 */
	if ((eeprom->type == rnp_eeprom_spi) &&
			(offset <= RNP_EERD_MAX_ADDR))
		ret_val = rnp_read_eerd_generic(hw, offset, data);
	else
		ret_val = rnp_read_eeprom_bit_bang_generic(hw, offset, data);
#endif
	return ret_val;
}

/**
 * rnp_reset_pipeline_n500 - perform pipeline reset
 *
 * @hw: pointer to hardware structure
 *
 * Reset pipeline by asserting Restart_AN together with LMS change to ensure
 * full pipeline reset.  Note - We must hold the SW/FW semaphore before writing
 * to AUTOC, so this function assumes the semaphore is held.
 **/
s32 rnp_reset_pipeline_n500(struct rnp_hw *hw)
{
	s32 ret_val;
	u32 i;

	/* Enable link if disabled in NVM */

	/* Write AUTOC register with toggled LMS[2] bit and Restart_AN */

	/* Wait for AN to leave state 0 */
	for (i = 0; i < 10; i++) {
		usleep_range(4000, 8000);
		break;
	}

	ret_val = 0;

	/* Write AUTOC register with original LMS field and Restart_AN */

	return ret_val;
}

/**
 *  rnp_reset_hw_n500 - Perform hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks
 *  and clears all interrupts, perform a PHY reset, and perform a link (MAC)
 *  reset.
 **/
//static s32 rnp_reset_hw_n500(struct rnp_hw *hw)
//{
//	int i;
//	s32 status;
//	u32 reg = 0;
//	int timeout = 0;
//	struct rnp_dma_info *dma = &hw->dma;
//
//	/* Call adapter stop to disable tx/rx and clear interrupts */
//	wr32(hw, RNP_DMA_AXI_EN, 0);
//
//	//nic_reset
//
//	wr32(hw, RNP_TOP_NIC_REST_N, NIC_RESET);
//	/*
//	 * we need this
//	 */
//	wmb();
//	wr32(hw, RNP_TOP_NIC_REST_N, ~NIC_RESET);
//
//#define TSRN500_REG_DEBUG_VALUE          (0x1a2b3c4d)
//
//#ifdef NO_MBX_VERSION
//	wr32(hw, RNP_DMA_DUMY, TSRN500_REG_DEBUG_VALUE);
//
//	while(!((reg = rd32(hw, RNP_DMA_DUMY)) == TSRN500_REG_DEBUG_VALUE + 1)) {
//		usleep_range(100, 200);
//		timeout++;
//
//		if (timeout > 10000) {
//			printk("wait reset timeout\n");
//			break;
//		}
//		//printk("wait reset");
//	}
//
//#else
//	rnp_mbx_fw_reset_phy(hw);
//#endif
//	printk("mac-rx-cfg: %x\n", rd32(hw, RNP_MAC_RX_CFG));
//	/* should set all tx-start to 1 */
//	for (i = 0; i < RNP_N500_MAX_TX_QUEUES; i++)
//		dma_ring_wr32(dma, RING_OFFSET(i) + RNP_DMA_TX_START, 1);
//
//#if 0
//	for (i = 0; i < 30; i++)
//		msleep(100);
//#endif
//	// default open this patch
//	wr32(hw, RNP_TOP_ETH_BUG_40G_PATCH, 1);
//	//wr32(hw, RNP_ETH_EXCEPT_DROP_PROC, 0xf);
//
//
//	/* Identify PHY and related function pointers */
//	status = hw->phy.ops.init(hw);
//
//	/* Setup SFP module if there is one present. */
//	if (hw->phy.sfp_setup_needed) {
//		status = hw->mac.ops.setup_sfp(hw);
//		hw->phy.sfp_setup_needed = false;
//	}
//
//	/* Reset PHY */
//	if (hw->phy.reset_disable == false && hw->phy.ops.reset != NULL)
//		hw->phy.ops.reset(hw);
//
//	/*todo earase tcm */
//	wr32(hw, RNP_ETH_TCAM_EN, 1);
//	wr32(hw, RNP_TOP_ETH_TCAM_CONFIG_ENABLE, 1);
//	wr32(hw, RNP_TCAM_MODE, 2);
//	/* dont't open tcam cache */
//	wr32(hw, RNP_TCAM_CACHE_ENABLE, 0);
//
//	for (i = 0; i < 4096; i++) {
//		wr32(hw, RNP_TCAM_SDPQF(i), 0);
//		wr32(hw, RNP_TCAM_DAQF(i), 0);
//		wr32(hw, RNP_TCAM_SAQF(i), 0);
//		wr32(hw, RNP_TCAM_APQF(i), 0);
//
//		wr32(hw, RNP_TCAM_SDPQF_MASK(i), 0);
//		wr32(hw, RNP_TCAM_DAQF_MASK(i), 0);
//		wr32(hw, RNP_TCAM_SAQF_MASK(i), 0);
//		wr32(hw, RNP_TCAM_APQF_MASK(i), 0);
//	}
//	wr32(hw, RNP_TCAM_MODE, 1);
//
//	/* Store the permanent mac address */
//	if (!(hw->mac.mac_flags & RNP_FLAGS_INIT_MAC_ADDRESS)) {
//		rnp_get_permtion_mac_addr(hw, hw->mac.perm_addr);
//		memcpy(hw->mac.addr, hw->mac.perm_addr, ETH_ALEN);
//	}
//
//	hw->mac.num_rar_entries = RNP_N500_RAR_ENTRIES;
//	hw->mac.ops.init_rx_addrs(hw);
//
//	/* open vxlan default */
//#define VXLAN_HW_ENABLE (1)
//	wr32(hw, RNP_ETH_TUNNEL_MOD, VXLAN_HW_ENABLE);
//
//	/* set pkt_len_err and hdr_len_err default to 1 */
//#define PKT_LEN_ERR (2)
//#define HDR_LEN_ERR (1)
//	wr32(hw, RNP_ETH_ERR_MASK_VECTOR, PKT_LEN_ERR | HDR_LEN_ERR);
//
//	/*=====  mac steup ===*/
//	/* open mac at last */
//	// dwc_xlgmac_databook.pdf
////	for (i = 0; i < 1; i++) {
////		//wr32(hw, RNP_MAC_TX_CFG, 0x40010001);
////		wr32(hw, RNP_MAC_RX_CFG, rd32(hw, RNP_MAC_RX_CFG) | 0x01); 
////		//wr32(hw, RNP_MAC_RX_CFG, 0x07d001c7);
////		wr32(hw, RNP_MAC_PKT_FLT, 0x80000001);
////		wr32(hw, RNP_MAC_LPI_CTRL, 0x00060000);
////	}
//reset_hw_out:
//	rnp_reset_msix_table_generic(hw);
//
//	return 0;
//}

/**
 *  rnp_start_hw_n500 - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware using the generic start_hw function
 *  and the generation start_hw function.
 *  Then performs revision-specific operations, if any.
 **/
//static s32 rnp_start_hw_n500(struct rnp_hw *hw)
//{
//	s32 ret_val = 0;
//	int i;
//
//	ret_val = rnp_start_hw_generic(hw);
//	if (ret_val != 0)
//		goto out;
//
//	ret_val = rnp_start_hw_gen2(hw);
//	if (ret_val != 0)
//		goto out;
//
//	// ETH Registers
//	//wr32(hw, RNP_ETH_ERR_MASK_VECTOR,~ETH_IGNORE_ALL_ERR);
//	wr32(hw, RNP_ETH_ERR_MASK_VECTOR, 0);
//	wr32(hw, RNP_ETH_BYPASS, 0);
//	wr32(hw, RNP_ETH_DEFAULT_RX_RING, 0);
//
//	wr32(hw, RNP_TOP_NIC_CONFIG,
//	     hw->mode
//#ifdef CONFIG_RNP_FPGA
//		     | hw->default_rx_queue << 24
//#endif
//	);
//
//	// DMA common Registers
//	wr32(hw, RNP_DMA_CONFIG, DMA_VEB_BYPASS);
//
//	// enable-dma-axi
//	wr32(hw, RNP_DMA_AXI_EN, (RX_AXI_RW_EN | TX_AXI_RW_EN));
//
//	if (ret_val == 0)
//		ret_val = rnp_verify_fw_version_n500(hw);
//out:
//	return ret_val;
//}

/**
 *  rnp_get_media_type_n500 - Get media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 **/
__maybe_unused static enum rnp_media_type rnp_get_media_type_n500(struct rnp_hw *hw)
{
	enum rnp_media_type media_type = rnp_media_type_fiber;
	return media_type;
}

/**
 *  rnp_get_supported_physical_layer_n500 - Returns physical layer type
 *  @hw: pointer to hardware structure
 *
 *  Determines physical layer capabilities of the current configuration.
 **/
__maybe_unused static u32 rnp_get_supported_physical_layer_n500(struct rnp_hw *hw)
{
	u32 physical_layer = 0;
	return physical_layer;
}


// not used 
/**
 *  rnp_get_mac_addr_n500 - Generic get MAC address
 *  @hw: pointer to hardware structure
 *  @mac_addr: Adapter MAC address
 *
 *  Reads the adapter's MAC address from first Receive Address Register (RAR0)
 *  A reset of the adapter must be performed prior to calling this function
 *  in order for the MAC address to have been loaded from the EEPROM into RAR0
 **/
s32 rnp_get_mac_addr_n500(struct rnp_eth_info *eth, u8 *mac_addr)
{
	u32 rar_high, rar_low, i;

	rar_high = eth_rd32(eth, RNP10_ETH_RAR_RH(0));
	rar_low = eth_rd32(eth, RNP10_ETH_RAR_RL(0));

	for (i = 0; i < 4; i++)
		mac_addr[i] = (u8)(rar_low >> (i * 8));

	for (i = 0; i < 2; i++)
		mac_addr[i + 4] = (u8)(rar_high >> (i * 8));

	return 0;
}



static struct rnp_phy_operations phy_ops_n500 = {
	.identify = &rnp_identify_phy_n500,
	.identify_sfp = &rnp_identify_sfp_module_n500,
	.init = &rnp_init_phy_ops_n500,
	.reset = &rnp_reset_phy_generic,
	.read_reg = &rnp_read_phy_reg_generic,
	.write_reg = &rnp_write_phy_reg_generic,
	.setup_link = &rnp_setup_phy_link_generic,
	.setup_link_speed = &rnp_setup_phy_link_speed_generic,
	.read_i2c_byte = &rnp_read_i2c_byte_generic,
	.write_i2c_byte = &rnp_write_i2c_byte_generic,
	.read_i2c_sff8472 = &rnp_read_i2c_sff8472_generic,
	.read_i2c_eeprom = &rnp_read_i2c_eeprom_generic,
	.write_i2c_eeprom = &rnp_write_i2c_eeprom_generic,
	.check_overtemp = &rnp_tn_check_overtemp,
};

// dma ops
/* setup queue speed limit to max_rate */
static void rnp_dma_set_tx_maxrate_n500(struct rnp_dma_info *dma, u16 queue, u32 max_rate)
{
	// struct rnp_hw *hw = (struct rnp_hw *)dma->back;
	// no use here ,use ring_wr32 instead


}

/* setup mac with vf_num to veb table */
static void rnp_dma_set_veb_mac_n500(struct rnp_dma_info *dma, u8 *mac, u32 vfnum, u32 ring)
{
	// n500 only has 1 port veb table
	u32 maclow, machi, ring_vfnum;
	int port;

	maclow = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) |
		mac[5];
	machi = (mac[0] << 8) | mac[1];
	ring_vfnum = ring | ((0x80 | vfnum) << 8);
	for (port = 0; port < 1; port++) {
		dma_wr32(dma, RNP500_DMA_PORT_VBE_MAC_LO_TBL(port, vfnum),
				maclow);
		dma_wr32(dma, RNP500_DMA_PORT_VBE_MAC_HI_TBL(port, vfnum),
				machi);
		dma_wr32(dma, RNP500_DMA_PORT_VEB_VF_RING_TBL(port, vfnum),
				ring_vfnum);
	}
}

/* setup vlan with vf_num to veb table */
static void rnp_dma_set_veb_vlan_n500(struct rnp_dma_info *dma, u16 vlan, u32 vfnum)
{
	int port;

	/* each vf can support only one vlan */
	for (port = 0; port < 1; port++)
		dma_wr32(dma, RNP500_DMA_PORT_VEB_VID_TBL(port, vfnum), vlan);

}
static void rnp_dma_set_veb_vlan_mask_n500(struct rnp_dma_info *dma, u16 vlan, u16 mask, int entry)
{
	// bit 19:12 is mask bit 11:0 is vid
	dma_wr32(dma, RNP500_DMA_PORT_VEB_VID_TBL(0, entry), (mask << 12) | vlan);
}

static void rnp_dma_clr_veb_all_n500(struct rnp_dma_info *dma)
{
	int port, i;

	for (port = 0; port < 1; port++) {
		for (i = 0; i < RNP500_VEB_TBL_CNTS; i++) {
			dma_wr32(dma, RNP500_DMA_PORT_VBE_MAC_LO_TBL(port, i), 0);
			dma_wr32(dma, RNP500_DMA_PORT_VBE_MAC_HI_TBL(port, i), 0);
			dma_wr32(dma, RNP500_DMA_PORT_VEB_VID_TBL(port, i), 0);
			dma_wr32(dma, RNP500_DMA_PORT_VEB_VF_RING_TBL(port, i), 0);
		}
	}

}

static struct rnp_dma_operations dma_ops_n500 = {
	.set_tx_maxrate = &rnp_dma_set_tx_maxrate_n500,
	.set_veb_mac = &rnp_dma_set_veb_mac_n500,
	.set_veb_vlan = &rnp_dma_set_veb_vlan_n500,
	.set_veb_vlan_mask = &rnp_dma_set_veb_vlan_mask_n500,
	.clr_veb_all = &rnp_dma_clr_veb_all_n500,


};

// eth ops
/**
 *  rnp_eth_set_rar_n500 - Set Rx address register
 *  @eth: pointer to eth structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 *  @sriov_flag 
 *
 *  Puts an ethernet address into a receive address register.
 **/
s32 rnp_eth_set_rar_n500(
	struct rnp_eth_info *eth, u32 index, u8 *addr,
	bool enable_addr)
{
	u32 mcstctrl;
	u32 rar_low, rar_high = 0;
	u32 rar_entries = eth->num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		rnp_err("RAR index %d is out of range.\n", index);
		return RNP_ERR_INVALID_ARGUMENT;
	}

	eth_dbg(eth,
		   "    RAR[%d] <= %pM.  vmdq:%d enable:0x%x\n",
		   index,
		   addr);

	/* setup VMDq pool selection before this RAR gets enabled */
	/* only sriov mode use this */
	/*
	if (sriov_on)
		eth->ops.set_vmdq(eth, index, vmdq);
		*/

	/*
	 * HW expects these in big endian so we reverse the byte
	 * order from network order (big endian) to little endian
	 */
	rar_low = ((u32)addr[5] | ((u32)addr[4] << 8) | ((u32)addr[3] << 16) |
			   ((u32)addr[2] << 24));
	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	rar_high = eth_rd32(eth, RNP500_ETH_RAR_RH(index));
	rar_high &= ~(0x0000FFFF | RNP500_RAH_AV);
	rar_high |= ((u32)addr[1] | ((u32)addr[0] << 8));

	if (enable_addr)
		rar_high |= RNP500_RAH_AV;

	eth_wr32(eth, RNP500_ETH_RAR_RL(index), rar_low);
	eth_wr32(eth, RNP500_ETH_RAR_RH(index), rar_high);

	/* open unicast filter */
	/* we now not use unicast */
	/* but we must open this since dest-mac filter | unicast table */
	/* all packets up if close unicast table */
	mcstctrl = eth_rd32(eth, RNP500_ETH_DMAC_MCSTCTRL);
	mcstctrl |= RNP500_MCSTCTRL_UNICASE_TBL_EN;
	eth_wr32(eth, RNP500_ETH_DMAC_MCSTCTRL, mcstctrl);

	return 0;
}

/**
 *  rnp_eth_clear_rar_n500 - Remove Rx address register
 *  @eth: pointer to eth structure
 *  @index: Receive address register to write
 *
 *  Clears an ethernet address from a receive address register.
 **/
s32 rnp_eth_clear_rar_n500(struct rnp_eth_info *eth, u32 index)
{
	u32 rar_high;
	u32 rar_entries = eth->num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		eth_dbg(eth, "RAR index %d is out of range.\n", index);
		return RNP_ERR_INVALID_ARGUMENT;
	}

	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	rar_high = eth_rd32(eth, RNP500_ETH_RAR_RH(index));
	rar_high &= ~(0x0000FFFF | RNP500_RAH_AV);

	// hw_dbg(hw, "Clearing RAR[%d]\n", index);
	eth_wr32(eth, RNP500_ETH_RAR_RL(index), 0);
	eth_wr32(eth, RNP500_ETH_RAR_RH(index), rar_high);

	/* clear VMDq pool/queue selection for this RAR */
	eth->ops.clear_vmdq(eth, index, RNP_CLEAR_VMDQ_ALL);

	return 0;
}

/**
 *  rnp_eth_set_vmdq_n500 - Associate a VMDq pool index with a rx address
 *  @eth: pointer to eth struct
 *  @rar: receive address register index to associate with a VMDq index
 *  @vmdq: VMDq pool index
 *  only mac->vf
 **/
s32 rnp_eth_set_vmdq_n500(struct rnp_eth_info *eth, u32 rar, u32 vmdq)
{
	u32 rar_entries = eth->num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		eth_dbg(eth, "RAR index %d is out of range.\n", rar);
		return RNP_ERR_INVALID_ARGUMENT;
	}

	eth_wr32(eth, RNP500_VM_DMAC_MPSAR_RING(rar), vmdq);

	return 0;
}

/**
 *  rnp_eth_clear_vmdq_n500 - Disassociate a VMDq pool index from a rx address
 *  @eth: pointer to eth struct
 *  @rar: receive address register index to disassociate
 *  @vmdq: VMDq pool index to remove from the rar
 **/
s32 rnp_eth_clear_vmdq_n500(struct rnp_eth_info *eth, u32 rar, u32 vmdq)
{
	u32 rar_entries = eth->num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		eth_dbg(eth, "RAR index %d is out of range.\n", rar);
		return RNP_ERR_INVALID_ARGUMENT;
	}

	eth_wr32(eth, RNP500_VM_DMAC_MPSAR_RING(rar), 0);

	return 0;
}



static s32 rnp500_mta_vector(struct rnp_eth_info *eth, u8 *mc_addr)
{
	u32 vector = 0;

	// maybe update this 
	switch (eth->mc_filter_type) {
		case 0: /* use bits [36:47] of the address */
			vector = ((mc_addr[4] << 8) | (((u16)mc_addr[5])));
			break;
		case 1: /* use bits [35:46] of the address */
			vector = ((mc_addr[4] << 7) | (((u16)mc_addr[5]) >> 1));
			break;
		case 2: /* use bits [34:45] of the address */
			vector = ((mc_addr[4] << 6) | (((u16)mc_addr[5]) >> 2));
			break;
		case 3: /* use bits [32:43] of the address */
			vector = ((mc_addr[4] << 5) | (((u16)mc_addr[5]) >> 3));
			break;
		case 4: /* use bits [32:43] of the address */
			vector = ((mc_addr[0] << 8) | (((u16)mc_addr[1])));
			vector = (vector >> 4);
			break;
		case 5: /* use bits [32:43] of the address */
			vector = ((mc_addr[0] << 8) | (((u16)mc_addr[1])));
			vector = (vector >> 3);
			break;
		case 6: /* use bits [32:43] of the address */
			vector = ((mc_addr[0] << 8) | (((u16)mc_addr[1])));
			vector = (vector >> 2);
			break;
		case 7: /* use bits [32:43] of the address */
			vector = ((mc_addr[0] << 8) | (((u16)mc_addr[1])));
			break;
		default: /* Invalid mc_filter_type */
			hw_dbg(hw, "MC filter type param set incorrectly\n");
			break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

static void rnp500_set_mta(struct rnp_hw *hw, u8 *mc_addr)
{
	u32 vector;
	u32 vector_bit;
	u32 vector_reg;
	struct rnp_eth_info *eth = &hw->eth;

	hw->addr_ctrl.mta_in_use++;

	// n500 use eth mta 
	vector = rnp500_mta_vector(eth, mc_addr);

	/*
	 * The MTA is a register array of 128 32-bit registers. It is treated
	 * like an array of 4096 bits.  We want to set bit
	 * BitArray[vector_value]. So we figure out what register the bit is
	 * in, read it, OR in the new bit, then write back the new value.  The
	 * register is determined by the upper 7 bits of the vector value and
	 * the bit within that register are determined by the lower 5 bits of
	 * the value.
	 */
	vector_reg = (vector >> 5) & 0x7F;
	vector_bit = vector & 0x1F;
	hw_dbg(hw,
	"\t\t%pM: MTA-BIT:%4d, MTA_REG[%d][%d] <= 1\n",
		   mc_addr,
		   vector,
		   vector_reg,
		   vector_bit);
	eth->mta_shadow[vector_reg] |= (1 << vector_bit);
}

static void rnp500_set_vf_mta(struct rnp_hw *hw, u16 vector)
{
	/* vf/pf use the same multicast table */
	u32 vector_bit;
	u32 vector_reg;
	struct rnp_eth_info *eth = &hw->eth;

	hw->addr_ctrl.mta_in_use++;

	vector_reg = (vector >> 5) & 0x7F;
	vector_bit = vector & 0x1F;
	hw_dbg(hw,
		   "\t\t vf M: MTA-BIT:%4d, MTA_REG[%d][%d] <= 1\n",
		   vector,
		   vector_reg,
		   vector_bit);
	eth->mta_shadow[vector_reg] |= (1 << vector_bit);
}

/**
 *  rnp_update_mc_addr_list_n500 - Updates MAC list of multicast addresses
 *  @hw: pointer to hardware structure
 *  @netdev: pointer to net device structure
 *
 *  The given list replaces any existing list. Clears the MC addrs from receive
 *  address registers and the multicast table. Uses unused receive address
 *  registers for the first multicast addresses, and hashes the rest into the
 *  multicast table.
 **/
s32 rnp_eth_update_mc_addr_list_n500(struct rnp_eth_info *eth,
		struct net_device *netdev, bool sriov_on)
{
	struct rnp_hw *hw = (struct rnp_hw *)eth->back;
#ifdef NETDEV_HW_ADDR_T_MULTICAST
	struct netdev_hw_addr *ha;
#endif
	u32 i;
	u32 v;
	int addr_count = 0;
	u8 *addr_list = NULL;

	/*
	 * Set the new number of MC addresses that we are being requested to
	 * use.
	 */
	hw->addr_ctrl.num_mc_addrs = netdev_mc_count(netdev);
	hw->addr_ctrl.mta_in_use = 0;

	/* Clear mta_shadow */
	eth_dbg(eth, " Clearing MTA(multicast table)\n");

	memset(&eth->mta_shadow, 0, sizeof(eth->mta_shadow));

	/* Update mta shadow */
	eth_dbg(eth, " Updating MTA..\n");

	addr_count = netdev_mc_count(netdev);

#ifdef NETDEV_HW_ADDR_T_MULTICAST     
	ha = list_first_entry(&netdev->mc.list,
			struct netdev_hw_addr, list);
	addr_list = ha->addr;
#else   
	addr_list = netdev->mc_list->dmi_addr;
#endif
	for (i = 0; i < addr_count; i++) {
		eth_dbg(eth, " Adding the multicast addresses:\n");
		rnp500_set_mta(hw, rnp_addr_list_itr(hw, &addr_list));
	}

	

	// sriov mode should set for vf multicast
	// not so good
	if (sriov_on) {
		struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;

		for (i = 0; i < adapter->num_vfs; i++) {
			if (adapter->vfinfo) {
				struct vf_data_storage *vfinfo = &adapter->vfinfo[i];
				int j;

				for (j = 0; j < vfinfo->num_vf_mc_hashes; j++)
					rnp500_set_vf_mta(hw, vfinfo->vf_mc_hashes[j]);
			} else {
				//printk("error sriov on but vfinfo is null\n");
			}
		}
	}

	/* Enable mta */
	for (i = 0; i < hw->eth.mcft_size; i++) {
		if (hw->addr_ctrl.mta_in_use) {
			eth_wr32(eth, RNP500_ETH_MUTICAST_HASH_TABLE(i), eth->mta_shadow[i]);
		}
	}

	if (hw->addr_ctrl.mta_in_use > 0) {
		v = eth_rd32(eth, RNP500_ETH_DMAC_MCSTCTRL);
		eth_wr32(eth,
			 RNP500_ETH_DMAC_MCSTCTRL,
			 v | RNP500_MCSTCTRL_MULTICASE_TBL_EN | eth->mc_filter_type);
	}

	eth_dbg(eth, " update MTA Done. mta_in_use:%d\n", hw->addr_ctrl.mta_in_use);
	return hw->addr_ctrl.mta_in_use;
}

/* clean all mc addr */
void rnp_eth_clr_mc_addr_n500(struct rnp_eth_info *eth)
{
	int i;

	for (i = 0; i < eth->mcft_size; i++)
		eth_wr32(eth, RNP500_ETH_MUTICAST_HASH_TABLE(i), 0);

}

/**
 *  rnp_eth_set_rss_hfunc_n500 - Remove Rx address register
 *  @eth: pointer to eth structure
 *  @hfunc hfunc type
 *
 *  update rss key to eth regs
 **/
int rnp_eth_set_rss_hfunc_n500(struct rnp_eth_info *eth, int hfunc)
{
	u32 data;

	data = eth_rd32(eth, RNP500_ETH_RSS_CONTROL);
	// clean mode only bit[14:15]
	data &= ~(BIT(14) | BIT(15));

	if (hfunc == rss_func_top) {

	} else if (hfunc == rss_func_xor) {
		data |= BIT(14);
	} else if (hfunc == rss_func_order) {
		data |= BIT(15);	
	} else{
		return -EINVAL;
	}

	/* update to hardware */
	eth_wr32(eth, RNP500_ETH_RSS_CONTROL, data);

	return 0;
}

/**
 *  rnp_eth_update_rss_key_n500 - Remove Rx address register
 *  @eth: pointer to eth structure
 *  @sriov_flag sriov status
 *
 *  update rss key to eth regs
 **/

void rnp_eth_update_rss_key_n500(struct rnp_eth_info *eth, bool sriov_flag)
{
	struct rnp_hw *hw = (struct rnp_hw *)eth->back;
	int i;
	u8 *key_temp;
	int key_len = RNP_RSS_KEY_SIZE;
	u8 *key = hw->rss_key;
	u32 data;
	u32 iov_en = (sriov_flag) ? RNP500_IOV_ENABLED : 0;

	data = eth_rd32(eth, RNP500_ETH_RSS_CONTROL);


	key_temp = kmalloc(key_len, GFP_KERNEL);
	//reoder the key
	for (i = 0; i < key_len; i++)
		*(key_temp + key_len - i - 1) = *(key + i);

	memcpy((u8 *)(eth->eth_base_addr + RNP500_ETH_RSS_KEY), key_temp, key_len);
	kfree(key_temp);

	// open rss now
	// maybe check rss control flags ?
	data |= (RNP500_ETH_ENABLE_RSS_ONLY | iov_en);
	eth_wr32(eth, RNP500_ETH_RSS_CONTROL, data);
}

/**
 *  rnp_eth_update_rss_table_n500 - Remove Rx address register
 *  @eth: pointer to eth structure
 *
 *  update rss table to eth regs
 **/
void rnp_eth_update_rss_table_n500(struct rnp_eth_info *eth)
{
	struct rnp_hw *hw = (struct rnp_hw *)eth->back;
	// setup rss info to hw regs
	u32 reta_entries = hw->rss_indir_tbl_num;
	u32 tc_entries = hw->rss_tc_tbl_num;
	int i;

	for (i = 0; i < tc_entries; i++)
		eth_wr32(eth, RNP500_ETH_TC_IPH_OFFSET_TABLE(i), hw->rss_tc_tbl[i]);

	for (i = 0; i < reta_entries; i++) 
		eth_wr32(eth, RNP500_ETH_RSS_INDIR_TBL(i), hw->rss_indir_tbl[i]);
		
}
/**
 *  rnp_eth_set_vfta_n500 - Set VLAN filter table
 *  @eth: pointer to eth structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vlan_on: boolean flag to turn on/off VLAN in VFVF
 *
 *  Turn on/off specified VLAN in the VLAN filter table.
 **/
s32 rnp_eth_set_vfta_n500(struct rnp_eth_info *eth, u32 vlan, bool vlan_on)
{
	s32 regindex;
	u32 bitindex;
	u32 vfta;
	u32 targetbit;
	bool vfta_changed = false;

	/* todo in vf mode vlvf regester can be set according to vind*/
	if (vlan > 4095)
		return RNP_ERR_PARAM;

	/*
	 * this is a 2 part operation - first the VFTA, then the
	 * VLVF and VLVFB if VT Mode is set
	 * We don't write the VFTA until we know the VLVF part succeeded.
	 */

	/* Part 1
	 * The VFTA is a bitstring made up of 128 32-bit registers
	 * that enable the particular VLAN id, much like the MTA:
	 *    bits[11-5]: which register
	 *    bits[4-0]:  which bit in the register
	 */
	regindex = (vlan >> 5) & 0x7F;
	bitindex = vlan & 0x1F;
	targetbit = (1 << bitindex);
	vfta = eth_rd32(eth, RNP500_VFTA(regindex));

	if (vlan_on) {
		if (!(vfta & targetbit)) {
			vfta |= targetbit;
			vfta_changed = true;
		}
	} else {
		if ((vfta & targetbit)) {
			vfta &= ~targetbit;
			vfta_changed = true;
		}
	}

	// todo 
	/*
	 * to enable two vf have same vlan feature, disable vlvf function.
	 * as vlan has high-priority than mac-address filter, which means
	 * two vf can't have same vlan.
	 */

	if (vfta_changed)
		eth_wr32(eth, RNP500_VFTA(regindex), vfta);
	return 0;
}

void rnp_eth_clr_vfta_n500(struct rnp_eth_info *eth)
{
	u32 offset;

	for (offset = 0; offset < eth->vft_size; offset++)
		eth_wr32(eth, RNP500_VFTA(offset), 0);
		/*
#define RNP500_VLVF_ENTRIES 64
	for (offset = 0; offset < RNP500_VLVF_ENTRIES; offset++)
		eth_wr32(eth, RNP500_VLVF(offset), 0);
		*/

}

static void rnp_eth_set_doulbe_vlan_n500(struct rnp_eth_info *eth, bool on)
{
	if (on)
		eth_wr32(eth, RNP500_ETH_VLAN_RM_TYPE, 1);
	else
		eth_wr32(eth, RNP500_ETH_VLAN_RM_TYPE, 0);
}

static void rnp_eth_set_outer_vlan_type_n500(struct rnp_eth_info *eth, int type)
{
	u32 data = 0x88a8;

	switch(type) {
	case outer_vlan_type_88a8:
		data = 0x88a8;
	break;
#ifdef ETH_P_QINQ1
	case outer_vlan_type_9100:
		data = 0x9100;
	break;
#endif
#ifdef ETH_P_QINQ2
	case outer_vlan_type_9200:
		data = 0x9200;
	break;
#endif
	}
	eth_wr32(eth, RNP500_ETH_WRAP_FIELD_TYPE, data);
	eth_wr32(eth, RNP500_ETH_TX_VLAN_TYPE, data);

}
/**
 *  rnp_eth_set_vlan_filter_n500 - Set VLAN filter table
 *  @eth: pointer to eth structure
 *  @status: on |off
 *  Turn on/off VLAN filter table.
 **/
static void rnp_eth_set_vlan_filter_n500(struct rnp_eth_info *eth, bool status)
{
#define ETH_VLAN_FILTER_BIT (30)
	u32 value = eth_rd32(eth, RNP500_ETH_VLAN_FILTER_ENABLE);

	// clear bit first 
	value &= (~(0x01 << ETH_VLAN_FILTER_BIT));
	if (status)
		value |= (0x01 << ETH_VLAN_FILTER_BIT);
	eth_wr32(eth, RNP500_ETH_VLAN_FILTER_ENABLE, value);
}

u16 rnp_layer2_pritologic_n500(u16 hw_id)
{
	return hw_id;
}

void rnp_eth_set_layer2_n500(struct rnp_eth_info *eth, union rnp_atr_input *input,
				u16 pri_id, u8 queue, bool prio_flag)
{
	u16 hw_id;

	hw_id = rnp_layer2_pritologic_n500(pri_id);
	/* enable layer2 */
	eth_wr32(eth, RNP500_ETH_LAYER2_ETQF(hw_id),
	     (0x1 << 31) | (ntohs(input->layer2_formate.proto)));

	/* setup action */
	if (queue == RNP_FDIR_DROP_QUEUE) {
		eth_wr32(eth, RNP500_ETH_LAYER2_ETQS(hw_id), (0x1 << 31));

	} else {
		/* setup ring_number */
		if (prio_flag)
			eth_wr32(eth, RNP500_ETH_LAYER2_ETQS(hw_id),
					(0x1 << 30) | (queue << 20) | (0x1 << 28));
		else
			eth_wr32(eth, RNP500_ETH_LAYER2_ETQS(hw_id),
					(0x1 << 30) | (queue << 20));

	}
}

void rnp_eth_clr_layer2_n500(struct rnp_eth_info *eth, u16 pri_id)
{
	u16 hw_id;

	hw_id = rnp_layer2_pritologic_n500(pri_id);
	eth_wr32(eth, RNP500_ETH_LAYER2_ETQF(hw_id), 0);

}

void rnp_eth_clr_all_layer2_n500(struct rnp_eth_info *eth)
{
	int i;
#define RNP500_MAX_LAYER2_FILTERS 16
	for (i = 0; i < RNP500_MAX_LAYER2_FILTERS; i++) 
		eth_wr32(eth, RNP500_ETH_LAYER2_ETQF(i), 0);

}

u16 rnp_tuple5_pritologic_n500(u16 hw_id)
{
	return hw_id;
}


void rnp_eth_set_tuple5_n500(struct rnp_eth_info *eth, union rnp_atr_input *input,
				u16 pri_id, u8 queue, bool prio_flag)
{
#define RNP500_SRC_IP_MASK BIT(0)
#define RNP500_DST_IP_MASK BIT(1)
#define RNP500_SRC_PORT_MASK BIT(2)
#define RNP500_DST_PORT_MASK BIT(3)
#define RNP500_L4_PROTO_MASK BIT(4)

	u32 port = 0;
	u8 mask_temp = 0;
	u8 l4_proto_type = 0;
	u16 hw_id;

	hw_id = rnp_tuple5_pritologic_n500(pri_id);	
	dbg("try to eable tuple 5 %x\n", hw_id);
	if (input->formatted.src_ip[0] != 0) {
		eth_wr32(eth, RNP500_ETH_TUPLE5_SAQF(hw_id),
				htonl(input->formatted.src_ip[0]));
	} else {
		mask_temp |= RNP500_SRC_IP_MASK;
	}
	if (input->formatted.dst_ip[0] != 0) {
		eth_wr32(eth, RNP500_ETH_TUPLE5_DAQF(hw_id),
				htonl(input->formatted.dst_ip[0]));
	} else
		mask_temp |= RNP500_DST_IP_MASK;
	if (input->formatted.src_port != 0)
		port |= (htons(input->formatted.src_port));
	else
		mask_temp |= RNP500_SRC_PORT_MASK;
	if (input->formatted.dst_port != 0)
		port |= (htons(input->formatted.dst_port) << 16);
	else
		mask_temp |= RNP500_DST_PORT_MASK;

	if (port != 0)
		eth_wr32(eth, RNP500_ETH_TUPLE5_SDPQF(hw_id), port);

	switch (input->formatted.flow_type) {
		case RNP_ATR_FLOW_TYPE_TCPV4:
			l4_proto_type = IPPROTO_TCP;
			break;
		case RNP_ATR_FLOW_TYPE_UDPV4:
			l4_proto_type = IPPROTO_UDP;
			break;
		case RNP_ATR_FLOW_TYPE_SCTPV4:
			l4_proto_type = IPPROTO_SCTP;
			break;
		case RNP_ATR_FLOW_TYPE_IPV4:
			l4_proto_type = input->formatted.inner_mac[0];
			break;
		default:
			l4_proto_type = 0;
	}

	if (l4_proto_type == 0)
		mask_temp |= RNP500_L4_PROTO_MASK;

	/* setup ftqf*/
	/* always set 0x3 */
	eth_wr32(eth, RNP500_ETH_TUPLE5_FTQF(hw_id),
			(1 << 31) | (mask_temp << 25) | (l4_proto_type << 16) | 0x3);

	/* setup action */
	if (queue == RNP_FDIR_DROP_QUEUE) {
		eth_wr32(eth, RNP500_ETH_TUPLE5_POLICY(hw_id), (0x1 << 31));
	} else {
		/* setup ring_number */
		if (prio_flag)
			eth_wr32(eth, RNP500_ETH_TUPLE5_POLICY(hw_id),
					((0x1 << 30) | (queue << 20) | (0x1 << 28)));
		else
			eth_wr32(eth, RNP500_ETH_TUPLE5_POLICY(hw_id),
					((0x1 << 30) | (queue << 20)));
	}


}

void rnp_eth_clr_tuple5_n500(struct rnp_eth_info *eth, u16 pri_id)
{
	u16 hw_id;

	hw_id = rnp_tuple5_pritologic_n500(pri_id);	
	eth_wr32(eth, RNP500_ETH_TUPLE5_FTQF(hw_id), 0);
}

void rnp_eth_clr_all_tuple5_n500(struct rnp_eth_info *eth)
{
	int i;

	for (i = 0; i < RNP500_MAX_TUPLE5_FILTERS; i++)
		eth_wr32(eth, RNP500_ETH_TUPLE5_FTQF(i), 0);
}

void rnp_eth_set_tcp_sync_n500(struct rnp_eth_info *eth, int queue, bool flag, bool prio)
{
	if (flag) {
		eth_wr32(eth, RNP500_ETH_SYNQF, (0x1 << 30) | (queue << 20));
		if (prio)
			eth_wr32(eth, RNP500_ETH_SYNQF_PRIORITY, (0x1 << 31) | 0x1);
		else
			eth_wr32(eth, RNP500_ETH_SYNQF_PRIORITY, (0x1 << 31));
		// todo add tcp-sync setup

	} else {
		eth_wr32(eth, RNP500_ETH_SYNQF, 0);
		eth_wr32(eth, RNP500_ETH_SYNQF_PRIORITY, 0);
	}
}

void rnp_eth_set_rx_skip_n500(struct rnp_eth_info *eth, int count, bool flag)
{
	if (flag) {
		eth_wr32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG, PRIV_DATA_EN | count);
	} else {
		eth_wr32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG, 0);
	}
}

static void rnp_eth_set_min_max_packets_n500(struct rnp_eth_info *eth, int min, int max)
{
	
	eth_wr32(eth, RNP500_ETH_DEFAULT_RX_MIN_LEN, min); 	
	eth_wr32(eth, RNP500_ETH_DEFAULT_RX_MAX_LEN, max); 	
}

static void rnp_eth_set_vlan_strip_n500(struct rnp_eth_info *eth, u16 queue, bool enable)
{
	u32 reg = RNP500_ETH_VLAN_VME_REG(queue / 32);
	u32 offset = queue % 32;
	u32 data = eth_rd32(eth, reg);

	if (enable == true)
		data |= (1 << offset);
	else
		data &= ~(1 << offset);

	eth_wr32(eth, reg, data);
}

static void rnp_eth_set_vxlan_port_n500(struct rnp_eth_info *eth, u32 port)
{
	// n500 not support vxlan
	//eth_wr32(eth, RNP10_ETH_VXLAN_PORT, port);
}

static void rnp_eth_set_vxlan_mode_n500(struct rnp_eth_info *eth, bool inner)
{
	// n500 not support vxlan 
	/*
	if (inner)
		eth_wr32(eth, RNP10_ETH_WRAP_FIELD_TYPE, 1);
	else
		eth_wr32(eth, RNP10_ETH_WRAP_FIELD_TYPE, 0);
	*/

}

static void rnp_eth_set_rx_hash_n500(struct rnp_eth_info *eth, bool status, bool sriov_flag)
{
	u32 iov_en = (sriov_flag) ? RNP500_IOV_ENABLED : 0;
	u32 data;
	data = eth_rd32(eth, RNP500_ETH_RSS_CONTROL);

	//clean 
	data &= ~RNP500_ETH_RSS_MASK;

	if (status) {
		data |= RNP500_ETH_ENABLE_RSS_ONLY;
		eth_wr32(eth, RNP500_ETH_RSS_CONTROL,
				data | iov_en);
	} else {
		eth_wr32(eth, RNP500_ETH_RSS_CONTROL,
				data | iov_en);
	}


}

static void rnp_eth_set_rx_n500(struct rnp_eth_info *eth, bool status)
{
	if (status) {
		eth_wr32(eth, RNP500_ETH_EXCEPT_DROP_PROC, 0);
		eth_wr32(eth, RNP500_ETH_TX_MUX_DROP, 0);
	} else { 
		eth_wr32(eth, RNP500_ETH_EXCEPT_DROP_PROC, 1);
		eth_wr32(eth, RNP500_ETH_TX_MUX_DROP, 1);
	}


} 

static void rnp_eth_fcs_n500(struct rnp_eth_info *eth, bool status)
{
	if (status)
		eth_wr32(eth, RNP500_ETH_FCS_EN, 1);
	else
		eth_wr32(eth, RNP500_ETH_FCS_EN, 0);

}

static void rnp_eth_set_vf_vlan_mode_n500(struct rnp_eth_info *eth, u16 vlan, int vf, bool enable)
{
	u32 value = vlan;
	if (enable)
		value |= BIT(31);

	eth_wr32(eth, RNP500_VLVF(vf), value);
	// n500 1 vf only can setup 1 vlan
	eth_wr32(eth, RNP500_VLVF_TABLE(vf), vf);

}

static s32 rnp_eth_set_fc_mode_n500(struct rnp_eth_info *eth)
{
	struct rnp_hw *hw = (struct rnp_hw *)eth->back;
	s32 ret_val = 0;
	int i;
	// n500 has only 1 traffic class

	for (i = 0; i < 1; i++) {
		if ((hw->fc.current_mode & rnp_fc_tx_pause) && hw->fc.high_water[i]) {
			if (!hw->fc.low_water[i] ||
				hw->fc.low_water[i] >= hw->fc.high_water[i]) {
				hw_dbg(hw, "Invalid water mark configuration\n");
				ret_val = RNP_ERR_INVALID_LINK_SETTINGS;
				goto out;
			}
		}
	}

	// should consider mtu 
	for (i = 0; i < 1; i++) {
		if ((hw->fc.current_mode & rnp_fc_tx_pause)) {
			if (hw->fc.high_water[i]) {
				eth_wr32(eth, RNP500_ETH_HIGH_WATER(i), hw->fc.high_water[i]);
			}
			if (hw->fc.low_water[i]) {
				eth_wr32(eth, RNP500_ETH_LOW_WATER(i), hw->fc.low_water[i]);
			}
		}
	} 
out:	
	return ret_val;

}

static struct rnp_eth_operations eth_ops_n500 = {
	.set_rar = &rnp_eth_set_rar_n500,
	.clear_rar = &rnp_eth_clear_rar_n500,
	.set_vmdq = &rnp_eth_set_vmdq_n500,
	.clear_vmdq = &rnp_eth_clear_vmdq_n500,
	//.get_mac_addr = &rnp_eth_get_mac_addr_n500,

	.update_mc_addr_list = &rnp_eth_update_mc_addr_list_n500,
	.clr_mc_addr = &rnp_eth_clr_mc_addr_n500,

	/* store rss info to eth */
	.set_rss_hfunc = &rnp_eth_set_rss_hfunc_n500,
	.set_rss_key = &rnp_eth_update_rss_key_n500,
	.set_rss_table = &rnp_eth_update_rss_table_n500,
	.set_vfta = &rnp_eth_set_vfta_n500,
	.clr_vfta = &rnp_eth_clr_vfta_n500,
	.set_vlan_filter = &rnp_eth_set_vlan_filter_n500,
	.set_outer_vlan_type = &rnp_eth_set_outer_vlan_type_n500,
	.set_double_vlan = &rnp_eth_set_doulbe_vlan_n500,

	.set_layer2_remapping = &rnp_eth_set_layer2_n500,
	.clr_layer2_remapping = &rnp_eth_clr_layer2_n500,
	.clr_all_layer2_remapping = &rnp_eth_clr_all_layer2_n500,
	.set_tuple5_remapping = &rnp_eth_set_tuple5_n500,
	.clr_tuple5_remapping = &rnp_eth_clr_tuple5_n500,
	.clr_all_tuple5_remapping = &rnp_eth_clr_all_tuple5_n500,
	.set_tcp_sync_remapping = &rnp_eth_set_tcp_sync_n500,
	.set_rx_skip = &rnp_eth_set_rx_skip_n500,

	.set_min_max_packet = &rnp_eth_set_min_max_packets_n500,
	.set_vlan_strip = &rnp_eth_set_vlan_strip_n500,
	.set_vxlan_port = &rnp_eth_set_vxlan_port_n500,
	.set_vxlan_mode = &rnp_eth_set_vxlan_mode_n500,
	.set_rx_hash = &rnp_eth_set_rx_hash_n500,
	.set_fc_mode = &rnp_eth_set_fc_mode_n500,
	// setup init sriov
	.set_rx = &rnp_eth_set_rx_n500,

	.set_fcs = &rnp_eth_fcs_n500,

	.set_vf_vlan_mode = &rnp_eth_set_vf_vlan_mode_n500,
	// mac 
	// setup fcs mode 
	// setup rx status

	// init_filter_default

};

/**
 *  rnp_init_hw_n500 - Generic hardware initialization
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting the hardware, filling the bus info
 *  structure and media type, clears all on chip counters, initializes receive
 *  address registers, multicast table, VLAN filter table, calls routine to set
 *  up link and flow control settings, and leaves transmit and receive units
 *  disabled and uninitialized
 **/
s32 rnp_init_hw_ops_n500(struct rnp_hw *hw)
{
	s32 status = 0;

	/* Reset the hardware */
	status = hw->ops.reset_hw(hw);

	if (status == 0) {
		/* Start the HW */
		status = hw->ops.start_hw(hw);
	}

	return status;
}

s32 rnp_get_permtion_mac_addr_n500(struct rnp_hw *hw, u8 *mac_addr)
{

#ifdef NO_CM3_MBX
	TRACE();
	v = nic_rd32(nic, RNP500_TOP_MAC_OUI);
	mac_addr[0] = (u8)(v >> 16);
	mac_addr[1] = (u8)(v >> 8);
	mac_addr[2] = (u8)(v >> 0);

	v = nic_rd32(nic, RNP500_TOP_MAC_SN);
	mac_addr[3] = (u8)(v >> 16);
	mac_addr[4] = (u8)(v >> 8);
	mac_addr[5] = (u8)(v >> 0);
#else
	if (rnp_fw_get_macaddr(hw, hw->pfvfnum, mac_addr, hw->nr_lane)) {
		printk("generate ramdom macaddress...\n");
		eth_random_addr(mac_addr);
	} else {
		printk("get mac addr %x:%x:%x:%x:%x:%x\n", mac_addr[0],
		mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], 
		mac_addr[5]);
	}
#endif
	// try to fixme
	hw->mac.mac_flags |= RNP_FLAGS_INIT_MAC_ADDRESS;
	dbg("%s mac:%pM\n", __func__, mac_addr);

	return 0;
}

s32 rnp_reset_hw_ops_n500(struct rnp_hw *hw)
{
	int i;
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_mac_info *mac = &hw->mac;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	dma_wr32(dma, RNP_DMA_AXI_EN, 0);

	//nic_reset
	//printk("we call reset \n");
#ifndef NO_CM3_MBX
	rnp_mbx_fw_reset_phy(hw);
#else
#define N500_NIC_RESET 0
	//wr32(hw, RNP10_TOP_NIC_REST_N, N500_NIC_RESET);
	nic_wr32(nic, RNP500_TOP_NIC_REST_N, N500_NIC_RESET);
	/*
	 * we need this
	 */
	wmb();
	nic_wr32(nic, RNP500_TOP_NIC_REST_N, ~N500_NIC_RESET);
#endif

	/* tcam not reset */
	eth->ops.clr_all_tuple5_remapping(eth);

	// fixme 
	/* Store the permanent mac address */
	if (!(hw->mac.mac_flags & RNP_FLAGS_INIT_MAC_ADDRESS)) {
		rnp_get_permtion_mac_addr_n500(hw, hw->mac.perm_addr);
		memcpy(hw->mac.addr, hw->mac.perm_addr, ETH_ALEN);
	}

	hw->ops.init_rx_addrs(hw);


	/* n500 should do this ? */
	eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, RNP_N500_PKT_LEN_ERR | RNP_N500_HDR_LEN_ERR);

	wr32(hw, RNP_DMA_RX_DATA_PROG_FULL_THRESH, 0xa);

	// reset all ring msix table to 0
	// each ring has one vector	
	//for (i = 0; i < dma->max_tx_queues; i++)
	for (i = 0; i < 12; i++)
		rnp_wr_reg(hw->ring_msix_base + RING_VECTOR(i), 0);
	// earase 

	{
		u32 value = 0;

		value |= RNP_MODE_NO_SA_INSER << RNP_SARC_OFFSET;
		value &= (~RNP_TWOKPE_MASK);
		value &= (~RNP_SFTERR_MASK);
		value |= (RNP_CST_MASK);
		value |= RNP_TC_MASK;
		value &= (~RNP_WD_MASK);
		value &= (~RNP_JD_MASK);
		value &= (~RNP_BE_MASK);
		value |= (RNP_JE_MASK);
		value |= (RNP_IFG_96 << RNP_IFG_OFFSET);
		value &= (~RNP_DCRS_MASK);
		value &= (~RNP_PS_MASK);
		value &= (~RNP_FES_MASK);
		value &= (~RNP_DO_MASK);
		value &= (~RNP_LM_MASK);
		value |= RNP_DM_MASK;
		value |= RNP_IPC_MASK; /* open rx checksum */
		value &= (~RNP_DR_MASK);
		value &= (~RNP_LUD_MASK);
		//value |= RNP_ACS_MASK;
		value |= (RNP_BL_MODE << RNP_BL_OFFSET);
		value &= (~RNP_DC_MASK);
		value |= RNP_TE_MASK;
		value |= RNP_RE_MASK;
		value |= (RNP_PRELEN_MODE);
		mac_wr32(mac, GMAC_CONTROL, value);
	}

	rnp_mbx_phy_pause_set(hw, hw->fc.requested_mode);
	return 0;
}

s32 rnp_start_hw_ops_n500(struct rnp_hw *hw)
{
	s32 ret_val = 0;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_dma_info *dma = &hw->dma;

	ret_val = rnp_start_hw_generic(hw);
	if (ret_val != 0)
		goto out;

	//ret_val = rnp_start_hw_gen2(hw);
	//if (ret_val != 0)
	//goto out;

	// ETH Registers
	//wr32(hw, RNP_ETH_ERR_MASK_VECTOR,~ETH_IGNORE_ALL_ERR);
	eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, RNP_N500_PKT_LEN_ERR | RNP_N500_HDR_LEN_ERR);

	eth_wr32(eth, RNP500_ETH_BYPASS, 0);
	eth_wr32(eth, RNP500_ETH_DEFAULT_RX_RING, 0);

	// n500 driver not set this 
	/*
	   wr32(hw, RNP500_TOP_NIC_CONFIG,
	   hw->mode
#ifdef CONFIG_RNP_FPGA
| hw->default_rx_queue << 24
#endif
); */

	// DMA common Registers
	dma_wr32(dma, RNP_DMA_CONFIG, DMA_VEB_BYPASS);

	// enable-dma-axi
	dma_wr32(dma, RNP_DMA_AXI_EN, (RX_AXI_RW_EN | TX_AXI_RW_EN));

	{
		int value = dma_rd32(dma, RNP_DMA_DUMY);

		value |= RC_CONTROL_HW;
		dma_wr32(dma, RNP_DMA_DUMY, value);
	}
	/*if (ret_val == 0)
	  ret_val = rnp_verify_fw_version_n500(hw);
	  */
out:
	return ret_val;
}

/* set n500 min/max packet according to new_mtu 
 * we support mtu + 14 + 4 * 3 as max packet len*/
static void rnp_set_mtu_hw_ops_n500(struct rnp_hw *hw, int new_mtu)
{
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;

	int min;
	int max = new_mtu + ETH_HLEN + ETH_FCS_LEN * 3;
#define ULTRA_SHORT 33
#define DEFAULT_SHORT 60
	if ((adapter->priv_flags & RNP_PRIV_FLAG_ULTRA_SHORT) ||
		(adapter->priv_flags & RNP_PRIV_FLAG_RX_ALL))
		min = ULTRA_SHORT; 
	else
		min = DEFAULT_SHORT; 

	// we receive jumbo fram only in jumbo enable or rx all mode
	if ((adapter->priv_flags & RNP_PRIV_FLAG_JUMBO) ||
			(adapter->priv_flags & RNP_PRIV_FLAG_RX_ALL))
		max = hw->max_length; 
	
	hw->min_length_current = min;
	hw->max_length_current = max;
	eth->ops.set_min_max_packet(eth, min, max);
}

/* setup n500 vlan filter status */
static void rnp_set_vlan_filter_en_hw_ops_n500(struct rnp_hw *hw, bool status)
{
	struct rnp_eth_info *eth = &hw->eth;
	eth->ops.set_vlan_filter(eth, status);
}

/* set vlan to n500 vlan filter table & veb */
/* pf setup call */
static void rnp_set_vlan_filter_hw_ops_n500(struct rnp_hw *hw, u16 vid, bool enable, bool sriov_flag)
{
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_dma_info *dma = &hw->dma;

	//todo set up own veb , use the last vfnum
	u32 vfnum = hw->max_vfs - 1;

	// vid 0 do nothing
	//if (!vid)
	//	return;
	/* setup n500 eth vlan table */
	eth->ops.set_vfta(eth, vid, enable);

	/* setup veb */
	if (sriov_flag) {
		if (hw->feature_flags & RNP_VEB_VLAN_MASK_EN) {
			// we update veb int other location
		} else {
			if (enable) {
				dma->ops.set_veb_vlan(dma, vid, vfnum);
			} else {
				dma->ops.set_veb_vlan(dma, 0, vfnum);
			}
		}
	}
}

// setup veb vlan mask mode 
static int rnp_set_veb_vlan_mask_hw_ops_n500(struct rnp_hw *hw, u16 vid, int vf, bool enable) 
{

	struct list_head *pos;
	struct vf_vebvlans *entry;
	struct rnp_dma_info *dma = &hw->dma;
	bool find = false;
	int err = 0;
	// 1 try to find is this vid is in vlan mask table 
	list_for_each(pos, &hw->vf_vas.l) {
		entry = list_entry(pos, struct vf_vebvlans, l);
		if (entry->vid == vid) {
			find = true;
			break;
		}

	}
	if (find) {
		/* this vid is used before */
		if (enable) {
			entry->mask |= (1 << vf);
		} else {
			entry->mask &= (~(1 << vf));
			// if mask is zero free this
			if (!entry) {
				entry->vid = -1;
				entry->free = true;
			}
		}
	} else {
		// it it a new vid 
		// 2 try to get new entries
		list_for_each(pos, &hw->vf_vas.l) {
			entry = list_entry(pos, struct vf_vebvlans, l);
			if (entry->free == true) {
				find = true;
				break;
			}
		}
		if (find) {
			// use this entry
			entry->free = false;
			entry->vid = vid;
			entry->mask |= (1 << vf);
		} else {
			err = -1;
			goto err_out;
		}

	}
	//
	//3 update new vlan mask to hw 
	dma->ops.set_veb_vlan_mask(dma, entry->vid, entry->mask, entry->veb_entry);
err_out:	
	return err;
}


static void rnp_set_vf_vlan_filter_hw_ops_n500(struct rnp_hw *hw, u16 vid, int vf, bool enable, bool veb_only)
{
	struct rnp_dma_info *dma = &hw->dma;

	if (!veb_only) {
		// call set vfta without veb setup
		hw->ops.set_vlan_filter(hw, vid, enable, false);

	} else {
		if (enable) {
			dma->ops.set_veb_vlan(dma, vid, vf);
		} else {
			dma->ops.set_veb_vlan(dma, 0, vf);
		}
	}

}

static void rnp_clr_vlan_veb_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_dma_info *dma = &hw->dma;
	u32 vfnum = hw->vfnum;

	dma->ops.set_veb_vlan(dma, 0, vfnum);

}

/* setup n500 vlan strip status */
static void rnp_set_vlan_strip_hw_ops_n500(struct rnp_hw *hw, u16 queue, bool strip)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	eth->ops.set_vlan_strip(eth, queue, strip);
}

/* update new n500 mac */
static void rnp_set_mac_hw_ops_n500(struct rnp_hw *hw, u8* mac, bool sriov_flag)
{
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_mac_info *mac_info = &hw->mac;
	/* use this queue index to setup veb */
	/* now pf use queu 0 /1 
	 * vfnum is the last vfnum */
	int queue = hw->veb_ring;
	int vfnum = hw->vfnum;
	// update new mac in index 0
	eth->ops.set_rar(eth, 0, mac, true);
	// if in sriov mode ,should update veb
	if (sriov_flag) {
		eth->ops.set_vmdq(eth, 0, queue / hw->sriov_ring_limit); 
		dma->ops.set_veb_mac(dma, mac, vfnum, queue);
	}
	// update pasue mac
	mac_info->ops.set_mac(mac_info, mac, 0);
}

/**
 * rnp_write_uc_addr_list - write unicast addresses to RAR table
 * @netdev: network interface device structure
 *
 * Writes unicast address list to the RAR table.
 * Returns: -ENOMEM on failure/insufficient address space
 *                0 on no addresses written
 *                X on writing X addresses to the RAR table
 **/
static int rnp_write_uc_addr_list_n500(struct rnp_hw *hw, struct net_device *netdev, bool sriov_flag)
{
	unsigned int rar_entries = hw->num_rar_entries - 1;
	u32 vfnum = hw->vfnum;
	struct rnp_eth_info *eth = &hw->eth;
	int count = 0;

	/* In SR-IOV mode significantly less RAR entries are available */
	// fixme in n500
	if (sriov_flag)
		rar_entries = hw->max_pf_macvlans - 1;

	/* return ENOMEM indicating insufficient memory for addresses */
	if (netdev_uc_count(netdev) > rar_entries)
		return -ENOMEM;

	if (!netdev_uc_empty(netdev)) {
		struct netdev_hw_addr *ha;

		hw_dbg(hw, "%s: rar_entries:%d, uc_count:%d\n", __func__,
		       hw->num_rar_entries, netdev_uc_count(netdev));

		/* return error if we do not support writing to RAR table */
		if (!eth->ops.set_rar)
			return -ENOMEM;

		netdev_for_each_uc_addr(ha, netdev) {
			if (!rar_entries)
				break;
			/* VMDQ_P(0) is num_vfs pf use the last
			 * vf in sriov mode
			 */
			/* that's ok */
			eth->ops.set_rar(eth, rar_entries, ha->addr, RNP500_RAH_AV);	
			//hw->mac.ops.set_rar(hw, rar_entries--, ha->addr,
			//		    VMDQ_P(0), RNP_RAH_AV);
			if (sriov_flag)
				eth->ops.set_vmdq(eth, rar_entries, vfnum); 	

			rar_entries--;

			count++;
		}
	}
	/* write the addresses in reverse order to avoid write combining */

	hw_dbg(hw, "%s: Clearing RAR[1 - %d]\n", __func__, rar_entries);
	for (; rar_entries > 0; rar_entries--)
		eth->ops.clear_rar(eth, rar_entries);

	return count;
}
static void rnp_set_rx_mode_hw_ops_n500(struct rnp_hw *hw, struct net_device *netdev, bool sriov_flag)
{

	struct rnp_adapter *adapter = netdev_priv(netdev);
	u32 fctrl;
#if defined(NETIF_F_HW_VLAN_CTAG_FILTER) || \
	defined(NETIF_F_HW_VLAN_CTAG_RX) 
	netdev_features_t features = netdev->features;
#endif
	int count;
	struct rnp_eth_info *eth = &hw->eth;

	hw_dbg(hw, "%s\n", __func__);

	/* broadcast always bypass */
	fctrl = eth_rd32(eth, RNP500_ETH_DMAC_FCTRL) | RNP500_FCTRL_BPE;

	/* clear the bits we are changing the status of */
	fctrl &= ~(RNP500_FCTRL_UPE | RNP500_FCTRL_MPE);
	/* promisc mode */
	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = true;
		fctrl |= (RNP500_FCTRL_UPE | RNP500_FCTRL_MPE);
		/* disable hardware filter vlans in promisc mode */
#ifdef NETIF_F_HW_VLAN_CTAG_FILTER
		features &= ~NETIF_F_HW_VLAN_CTAG_FILTER;
#endif
#ifdef NETIF_F_HW_VLAN_CTAG_RX
		features &= ~NETIF_F_HW_VLAN_CTAG_RX;
#endif
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			fctrl |= RNP500_FCTRL_MPE;
		} else {
			/* Write addresses to the MTA, if the attempt fails
			 * then we should just turn on promiscuous mode so
			 * that we can at least receive multicast traffic
			 */
			// we always update vf multicast info
			count = eth->ops.update_mc_addr_list(eth, netdev, true); 
			//count = hw->mac.ops.update_mc_addr_list(hw, netdev);
			if (count < 0) {
				fctrl |= RNP500_FCTRL_MPE;
				// mcstctrl &= ~RNP_MCSTCTRL_MULTICASE_TBL_EN;
			} else if (count) {
				// mcstctrl |= RNP_MCSTCTRL_MULTICASE_TBL_EN;
			}
		}
		hw->addr_ctrl.user_set_promisc = false;
	}

	/*
	 * Write addresses to available RAR registers, if there is not
	 * sufficient space to store all the addresses then enable
	 * unicast promiscuous mode
	 */
	if (rnp_write_uc_addr_list_n500(hw, netdev, sriov_flag) < 0) {
		fctrl |= RNP500_FCTRL_UPE;
	}

	/* not so good */
	// update this outside hw_ops
	// todo 
	/*if (adapter->num_vfs)
		rnp_restore_vf_multicasts(adapter);
	*/
	eth_wr32(eth, RNP500_ETH_DMAC_FCTRL, fctrl);
#ifdef NETIF_F_HW_VLAN_CTAG_FILTER
	if (features & NETIF_F_HW_VLAN_CTAG_FILTER)
		eth->ops.set_vlan_filter(eth, true);
	else
		eth->ops.set_vlan_filter(eth, false);
#endif

	/* not so good */
	// maybe set this outside ?
	/*
#ifdef NETIF_F_HW_VLAN_CTAG_RX
	{
		int i;
		for (i = 0; i < adapter->num_rx_queues; i++) {
			struct rnp_ring *tx_ring;

			tx_ring = adapter->rx_ring[i];
			if (features & NETIF_F_HW_VLAN_CTAG_RX)
				eth->ops.set_vlan_strip(eth, tx_ring->rnp_queue_idx, true);
			else
				eth->ops.set_vlan_strip(eth, tx_ring->rnp_queue_idx, false);
		}
	}
#endif
*/
	if ((hw->addr_ctrl.user_set_promisc == true) ||
			(adapter->priv_flags & RNP_PRIV_FLAG_REC_HDR_LEN_ERR)) {

		/* set pkt_len_err and hdr_len_err default to 1 */
		eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, PKT_LEN_ERR | HDR_LEN_ERR);
	} else {

		eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, 0);
	}

	// update mtu 
	hw->ops.set_mtu(hw, netdev->mtu);

}

/* setup an rar with vfnum */
static void rnp_set_rar_with_vf_hw_ops_n500(struct rnp_hw *hw, u8 *mac,
		int idx, u32 vfnum, bool enable)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.set_rar(eth, idx, mac, enable);
	/* should check error or not ?*/
	eth->ops.set_vmdq(eth, idx, vfnum); 

}

static void rnp_clr_rar_hw_ops_n500(struct rnp_hw *hw, int idx)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clear_rar(eth, idx);

}

static void rnp_clr_rar_all_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;
	unsigned int rar_entries = hw->num_rar_entries - 1;
	int i;

	for (i = 0; i < rar_entries; i++) 
		eth->ops.clear_rar(eth, rar_entries);

}

static void rnp_set_fcs_mode_hw_ops_n500(struct rnp_hw *hw, bool status)
{
	struct rnp_mac_info *mac = &hw->mac;
	struct rnp_eth_info *eth = &hw->eth;
	// call mac to up fcs
	mac->ops.set_mac_fcs(mac, status);
	eth->ops.set_fcs(eth, status);

}

static void rnp_set_vxlan_port_hw_ops_n500(struct rnp_hw *hw, u32 port)
{
	//n500 not support
	//struct rnp_eth_info *eth = &hw->eth;

	//eth->ops.set_vxlan_port(eth, port);
	//set vxlan port to eth mode

}

static void rnp_set_vxlan_mode_hw_ops_n500(struct rnp_hw *hw, bool inner)
{
	//n500 not support
	//struct rnp_eth_info *eth = &hw->eth;

	//eth->ops.set_vxlan_mode(eth, inner);
}

static void rnp_set_mac_speed_hw_ops_n500(struct rnp_hw *hw, bool link, u32 speed, bool duplex)
{
//	struct rnp_mac_info *mac = &hw->mac;
//	u32 value;
//
//	mac->ops.set_mac_speed(mac, link, speed, duplex);
//#define CLOCK_OFFSET (0x30200)
//	
//	value = rd32(hw, CLOCK_OFFSET);
//
//	value &= (~0x30);
//	value |= 0x300000;
//	switch (speed) {
//		case RNP_LINK_SPEED_100_FULL:
//			value |= 0x10;
//		break;
//		case RNP_LINK_SPEED_10_FULL:
//			value |= 0x00;
//		break;
//		case RNP_LINK_SPEED_1GB_FULL:
//			value |= 0x20;
//		break;
//	}
//	//printk("set clock offset to %x\n", value);
//
//	wr32(hw, CLOCK_OFFSET, value);

}


static void rnp_set_mac_rx_hw_ops_n500(struct rnp_hw *hw, bool status)
{
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_mac_info *mac = &hw->mac;

	if (status) {
		mac->ops.set_mac_rx(mac, status);
		eth->ops.set_rx(eth, status);
	} else {
		// 
		eth->ops.set_rx(eth, status);
		mac->ops.set_mac_rx(mac, status);
	}
	//set mac rx status
}

static void rnp_set_sriov_status_hw_ops_n500(struct rnp_hw *hw, bool status)
{
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	u32 v, fctrl;
	//open sriov or not
	//
	fctrl = eth_rd32(eth, RNP500_ETH_DMAC_FCTRL);
#define RNP500_DMAC_MASK (0x7f)
	fctrl &= ~RNP500_DMAC_MASK;

	if (status) {
		fctrl |=  hw->veb_ring;
		eth_wr32(eth, RNP500_ETH_DMAC_FCTRL, fctrl);
		/* setup default ring */
		dma_wr32(dma, RNP_DMA_CONFIG, dma_rd32(dma, RNP_DMA_CONFIG) & (~DMA_VEB_BYPASS));
		v = eth_rd32(eth, RNP500_MRQC_IOV_EN);
		v |= RNP500_IOV_ENABLED;
		eth_wr32(eth, RNP500_MRQC_IOV_EN, v);
		// 1 setup veb vlan type
	
	} else {

		eth_wr32(eth, RNP500_ETH_DMAC_FCTRL, fctrl);
		v = eth_rd32(eth, RNP500_MRQC_IOV_EN);
		v &= ~(RNP500_IOV_ENABLED);
		eth_wr32(eth, RNP500_MRQC_IOV_EN, v);

		dma->ops.clr_veb_all(dma);
		// clean veb ?

	}

}


/* setup a vf mac a new */
//static void rnp_set_sriov_vf_mac_hw_ops_n500(struct rnp_hw *hw, u8 *mac, int vf, bool enable)
//{
//
//	struct rnp_eth_info *eth = &hw->eth;
//	int rar_entry = hw->num_rar_entries - (vf + 1);
//#ifdef FIX_VF_BUG
//	/* we use the next vf ring in condition*/
//	int vf_ring = vf * 2 + 2; 
//#else
//	int vf_ring = vf * 2; 
//#endif
//	// setup eth rar
//	eth->ops.set_rar(eth, rar_entry, mac, enable);	
//	// setup vmdq 
//	eth->ops.set_vmdq(eth, rar_entry, vf_ring / 2);
//	// now only setup own veb
//
//}
static void rnp_set_sriov_vf_mc_hw_ops_n500(struct rnp_hw *hw, u16 mc_addr)
{
	struct rnp_eth_info *eth = &hw->eth;
	u32 vector_bit;
	u32 vector_reg;
	u32 mta_reg;
	/* pf/ vf share one mc table */

	vector_reg = (mc_addr >> 5) & 0x7F;
	vector_bit = mc_addr & 0x1F;
	mta_reg = eth_rd32(eth, RNP500_ETH_MUTICAST_HASH_TABLE(vector_reg));
	mta_reg |= (1 << vector_bit);
	eth_wr32(eth, RNP500_ETH_MUTICAST_HASH_TABLE(vector_reg), mta_reg);
}

/*static void rnp_set_sriov_vf_vlan_hw_ops_n500(struct rnp_hw *hw, int vf, u16 vlan ,u8 qos)
  {

	// setup veb only ?
}*/

static void rnp_update_sriov_info_hw_ops_n500(struct rnp_hw *hw)
{

	// update sriov info to hw 
}

static void rnp_set_pause_mode_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_mac_info *mac = &hw->mac;
	struct rnp_eth_info *eth = &hw->eth;

	mac->ops.set_fc_mode(mac);
	eth->ops.set_fc_mode(eth);
	//set up pasus info to he
	// call 

}

static void rnp_get_pause_mode_hw_ops_n500(struct rnp_hw *hw)
{
	// n500 can get pause mode in link event


}

static void rnp_update_hw_info_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_mac_info *mac = &hw->mac;
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	u32 data;
	//1 enable eth filter
	eth_wr32(eth, RNP500_HOST_FILTER_EN, 1);
	//2 open redir en
	eth_wr32(eth, RNP500_REDIR_EN, 1);
	
	//3 open sctp checksum and other checksum?
	if (hw->feature_flags & RNP_NET_FEATURE_TX_CHECKSUM)
		eth_wr32(eth, RNP500_ETH_SCTP_CHECKSUM_EN, 1);
	
	//4 mark muticaset as broadcast
	dma_wr32(dma, RNP_VEB_MAC_MASK_LO, 0xffffffff);
	dma_wr32(dma, RNP_VEB_MAC_MASK_HI, 0xfeff);
	// test only
	dma_wr32(dma, 0x00b0, 615);

	// 5 setup ft padding and veb vlan mode
	data = dma_rd32(dma, RNP_DMA_CONFIG);
#ifdef FT_PADDING
#define N500_PADDING_BIT 8
	if (adapter->priv_flags & RNP_PRIV_FLAG_FT_PADDING)
		SET_BIT(N500_PADDING_BIT, data);
#endif
#define N500_VLAN_POLL_EN BIT(3)
	if (hw->feature_flags & RNP_VEB_VLAN_MASK_EN)
		data |= N500_VLAN_POLL_EN;

	dma_wr32(dma, RNP_DMA_CONFIG, data);
	//6 setup vlan mode
	if (adapter->priv_flags & RNP_PRIV_FLAG_DOUBLE_VLAN)	
		eth->ops.set_double_vlan(eth, true);
	else 
		eth->ops.set_double_vlan(eth, false);

	//7 setup rss-hash mode
	eth->ops.set_rss_hfunc(eth, adapter->rss_func_mode);
	/*
	data = eth_rd32(eth, RNP500_ETH_RSS_CONTROL);
	// clean mode only bit[14:15]
	data &= ~(BIT(14) | BIT(15));
	switch (adapter->rss_func_mode) {
	case rss_func_top:
	// do-nothing 
	break;
	case rss_func_xor:
		data |= BIT(14);
	break;
	case rss_func_order:
		data |= BIT(15);
	break;

	}
	eth_wr32(eth, RNP500_ETH_RSS_CONTROL, data);
	*/
	// 8 setup outer-vlan type 
	eth->ops.set_outer_vlan_type(eth, adapter->outer_vlan_type);
	/*
	switch(adapter->outer_vlan_type) {
	case outer_vlan_type_88a8:
		data = 0x88a8;
	break;
	case outer_vlan_type_9100:
		data = 0x9100;
	break;

	case outer_vlan_type_9200:
		data = 0x9200;
	break;

	}
	eth_wr32(eth, RNP500_ETH_WRAP_FIELD_TYPE, data);
	*/
	// 9 setup tcp sync remapping
	if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC) {
		if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC_PRIO)
			hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue, true, true);
		else
			hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue, true, false);
	} else {
		hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue, false, false);

	}
	// 10 setup pause status
	data = mac_rd32(mac, GMAC_FLOW_CTRL);
	if (adapter->priv_flags & RNP_PRIV_FLAG_PAUSE_OWN) {
		data |= GMAC_FLOW_CTRL_UP;
	} else {
		data &= (~GMAC_FLOW_CTRL_UP);
	}
	mac_wr32(mac, GMAC_FLOW_CTRL, data);

	// 11 open tx double vlan according to stags
	//if (adapter->priv_flags & RNP_FLAG2_VLAN_STAGS_ENABLED)
	eth_wr32(eth, RNP500_ETH_TX_VLAN_CONTROL_EANBLE, 1);
	//else
	//	eth_wr32(eth, RNP500_ETH_TX_VLAN_CONTROL_EANBLE, 0);

	// 12 test
	//eth_wr32(eth, RNP500_ETH_RX_MAC_LEN_REG, 1);
	eth_wr32(eth, RNP500_ETH_WHOLE_PKT_LEN_ERR_DROP, 1);

	// 13 setup double vlan drop
	if (adapter->priv_flags & RNP_PRIV_FLAG_DOUBLE_VLAN_RECEIVE)
		eth_wr32(eth, RNP500_ETH_DOUBLE_VLAN_DROP, 0);
	else
		eth_wr32(eth, RNP500_ETH_DOUBLE_VLAN_DROP, 1);

	// 14 open error mask if in rx all mode 
	if (adapter->priv_flags & RNP_PRIV_FLAG_RX_ALL) {
		eth_wr32(eth, RNP500_MAC_ERR_MASK,
				RUN_FRAME_ERROR | GAINT_FRAME_ERROR | CRC_ERROR | LENGTH_ERROR);
		// we open this in rx all mode	
		eth_wr32(eth, RNP500_ETH_DOUBLE_VLAN_DROP, 0);
		//eth_wr32(eth, RNP500_BAD_PACKETS_RECEIVE_EN, 1);

	} else {
		eth_wr32(eth, RNP500_MAC_ERR_MASK, RUN_FRAME_ERROR | GAINT_FRAME_ERROR);
		//eth_wr32(eth, RNP500_BAD_PACKETS_RECEIVE_EN, 0);
	}	

	// 15 update water acoording to max length
	{
#define FIFO_ALL (1024)
		int water_high = FIFO_ALL - ((hw->max_length_current + 15) >> 4);
		//int wather_low = water_high - 100;

		// n500 only use one
		hw->fc.high_water[0] = water_high;
		hw->fc.low_water[0] = water_high;
		
		dma_wr32(dma, RNP500_DMA_RBUF_FIFO, ((hw->max_length_current + 15) >> 4) + 5);
		eth_wr32(eth, RNP500_ETH_EMAC_PARSE_PROGFULL_THRESH, ((hw->max_length_current + 15) >> 4) + 2);
		//eth_wr32(eth, RNP500_ETH_EMAC_PARSE_PROGFULL_THRESH, 100);
		
	}
	// 16 setup fcs mode //todo
	if (adapter->priv_flags & RNP_PRIV_FLAG_RX_FCS) {
		hw->ops.set_fcs_mode(hw, true);

	} else {
		hw->ops.set_fcs_mode(hw, false);
	}

	// 17 setup tso fifo
	dma_wr32(dma, RNP_DMA_PKT_FIFO_DATA_PROG_FULL_THRESH, 36);

	// data = eth_rd32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG);
	// 18 setup priv skip 
	if (adapter->priv_flags & RNP_PRIV_FLAG_RX_SKIP_EN) {
		data = PRIV_DATA_EN | adapter->priv_skip_count; 
	} else {
		data = 0;
	}
	eth_wr32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG, data);

	// 19 setup mac count read self clear
	data = mac_rd32(mac, RNP500_MAC_COUNT_CONTROL);	
#define READ_CLEAR BIT(2)
	data |= READ_CLEAR;
	mac_wr32(mac, RNP500_MAC_COUNT_CONTROL, data);

	// 20 setup prio
	if (adapter->priv_flags & (RNP_PRIV_FLAG_8023_PRIO | RNP_PRIV_FLAG_REMAP_PRIO)) {
		eth_wr32(eth, RNP500_PRIORITY_1_MARK, RNP500_PRIORITY_1);
		eth_wr32(eth, RNP500_PRIORITY_0_MARK, RNP500_PRIORITY_0);
		eth_wr32(eth, RNP500_PRIORITY_EN, 1);
		if (adapter->priv_flags & RNP_PRIV_FLAG_8023_PRIO)
			eth_wr32(eth, RNP500_PRIORITY_EN_8023, 1);
		else
			eth_wr32(eth, RNP500_PRIORITY_EN_8023, 0);
	} else {
		eth_wr32(eth, RNP500_PRIORITY_EN, 0);
	}

}

static void rnp_update_hw_rx_drop_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	int i;
	struct rnp_ring *ring;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		ring = adapter->rx_ring[i];
		if (adapter->rx_drop_status & BIT(i)) {
			ring_wr32(ring, PCI_DMA_REG_RX_DESC_TIMEOUT_TH, adapter->drop_time);
		} else {
			ring_wr32(ring, PCI_DMA_REG_RX_DESC_TIMEOUT_TH, 0);
		}
	}
}

static void rnp_set_rx_hash_hw_ops_n500(struct rnp_hw *hw, bool status, bool sriov_flag)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.set_rx_hash(eth, status, sriov_flag);

}

/* setup mac to rar 0 
 * clean vmdq 
 * clean mc addr */
static s32 rnp_init_rx_addrs_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	u32 i;
	u32 rar_entries = eth->num_rar_entries;
	u32 v;

	hw_dbg(hw,
		   "init_rx_addrs:rar_entries:%d, mac.addr:%pM\n",
		   rar_entries,
		   hw->mac.addr);
	/*
	 * If the current mac address is valid, assume it is a software override
	 * to the permanent address.
	 * Otherwise, use the permanent address from the eeprom.
	 */
	if (!is_valid_ether_addr(hw->mac.addr)) {
		/* Get the MAC address from the RAR0 for later reference */
		// fixme 
		// use perm mac 
		//hw->mac.ops.get_mac_addr(hw, hw->mac.addr);
		memcpy(hw->mac.addr, hw->mac.perm_addr, ETH_ALEN);
		hw_dbg(hw, " Keeping Current RAR0 Addr =%pM\n", hw->mac.addr);
	} else {
		/* Setup the receive address. */
		hw_dbg(hw, "Overriding MAC Address in RAR[0]\n");
		hw_dbg(hw, " New MAC Addr =%pM\n", hw->mac.addr);

		// set rar 0 
		eth->ops.set_rar(eth, 0, hw->mac.addr, true);
		//hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, RNP_RAH_AV);

		/*  clear VMDq pool/queue selection for RAR 0 */
		eth->ops.clear_vmdq(eth, 0, RNP_CLEAR_VMDQ_ALL); 
		//hw->mac.ops.clear_vmdq(hw, 0, RNP_CLEAR_VMDQ_ALL);
	}
	hw->addr_ctrl.overflow_promisc = 0;
	hw->addr_ctrl.rar_used_count = 1;

	/* Zero out the other receive addresses. */
	hw_dbg(hw, "Clearing RAR[1-%d]\n", rar_entries - 1);
	for (i = 1; i < rar_entries; i++) {
		eth->ops.clear_rar(eth, i);
	}

	/* Clear the MTA */
	hw->addr_ctrl.mta_in_use = 0;
	v = eth_rd32(eth, RNP500_ETH_DMAC_MCSTCTRL);
	v &= (~0x3);
	v |= eth->mc_filter_type;
	eth_wr32(eth, RNP500_ETH_DMAC_MCSTCTRL, v);

	hw_dbg(hw, " Clearing MTA\n");
	eth->ops.clr_mc_addr(eth);

	/* we not use uta filter now */
	//if (hw->mac.ops.init_uta_tables)
	//	hw->mac.ops.init_uta_tables(hw);

	return 0;
}

// clean vlan filter tables
static void rnp_clr_vfta_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clr_vfta(eth);
}

static void rnp_set_txvlan_mode_hw_ops_n500(struct rnp_hw *hw, bool cvlan)
{
	//struct rnp_mac_info *mac = &hw->mac;

	// update me 
	/*if (cvlan) {
		mac_wr32(mac, RNP10_MAC_TX_VLAN_TAG, 0x4000000);
		mac_wr32(mac, RNP10_MAC_TX_VLAN_MODE, 0x100000);
		mac_wr32(mac, RNP10_MAC_INNER_VLAN_INCL, 0x100000);
	} else {
		mac_wr32(mac, RNP10_MAC_TX_VLAN_TAG, 0xc600000);
		mac_wr32(mac, RNP10_MAC_TX_VLAN_MODE, 0x180000);
		mac_wr32(mac, RNP10_MAC_INNER_VLAN_INCL, 0x100000);
	}*/

}

static int rnp_set_rss_hfunc_hw_ops_n500(struct rnp_hw *hw, u8 hfunc)
{
#ifdef HAVE_RXFH_HASHFUNC
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	switch (hfunc) {
	case ETH_RSS_HASH_TOP: 
		adapter->rss_func_mode = rss_func_top;
	break;

	case ETH_RSS_HASH_XOR:
		adapter->rss_func_mode = rss_func_xor;
	break;
	default:
		return -EINVAL;
	}
	
	eth->ops.set_rss_hfunc(eth, adapter->rss_func_mode);
#endif
	return 0;
}

static void rnp_set_rss_key_hw_ops_n500(struct rnp_hw *hw, bool sriov_flag)
{
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	int key_len = RNP_RSS_KEY_SIZE;

	memcpy(hw->rss_key, adapter->rss_key, key_len);

	eth->ops.set_rss_key(eth, sriov_flag);


}

static void rnp_set_rss_table_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.set_rss_table(eth);

}

static void rnp_set_mbx_link_event_hw_ops_n500(struct rnp_hw *hw, int enable)
{
	rnp_mbx_link_event_enable(hw, enable);
}
//#define NO_UP_DOWN
static void rnp_set_mbx_ifup_hw_ops_n500(struct rnp_hw *hw, int enable)
{
#ifdef NO_UP_DOWN
	static int flags[4];
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back; 
	// only once 
	if ((enable) && (flags[adapter->bd_number] == 0)) {
		flags[adapter->bd_number] = 1;
#endif
		rnp_mbx_ifup_down(hw, enable);
	
#ifdef NO_UP_DOWN
	}
#endif
}



/**
 *  rnp_check_mac_link_n500 - Determine link and speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: true when link is up
 *  @link_up_wait_to_complete: bool used to wait for link up or not
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 rnp_check_mac_link_hw_ops_n500(struct rnp_hw *hw,
		rnp_link_speed *speed,
		bool *link_up,
		bool *duplex,
		bool link_up_wait_to_complete)
{
	struct rnp_adapter *adapter = (struct rnp_adapter *) hw->back;


	if (hw->speed == 10) {
		*speed = RNP_LINK_SPEED_10_FULL;
	} else if (hw->speed == 100) {
		*speed = RNP_LINK_SPEED_100_FULL;
	} else if (hw->speed == 1000) {
		*speed = RNP_LINK_SPEED_1GB_FULL;
	} else if (hw->speed == 10000) {
		*speed = RNP_LINK_SPEED_10GB_FULL;
	} else if (hw->speed == 25000) {
		*speed = RNP_LINK_SPEED_25GB_FULL;
	} else if (hw->speed == 40000) {
		*speed = RNP_LINK_SPEED_40GB_FULL;
	} else {
		*speed = RNP_LINK_SPEED_UNKNOWN;
	}

	*link_up = hw->link;

	if (adapter->priv_flags & RNP_PRIV_FLGA_TEST_TX_HANG)
		*link_up = 0;

	*duplex = hw->duplex;

	return 0;
}

s32 rnp_setup_mac_link_hw_ops_n500(struct rnp_hw *hw, u32 adv, u32 autoneg, u32 speed, u32 duplex)
{

	rnp_mbx_phy_link_set(hw, adv, autoneg, speed, duplex, hw->tp_mdix_ctrl);

	return 0;
}

void rnp_clean_link_hw_ops_n500(struct rnp_hw *hw)
{
        hw->link = 0;
        //wr32(hw, RNP_ETH_EXCEPT_DROP_PROC, 0xf);
}

static s32 rnp_get_link_capabilities_hw_ops_n500(struct rnp_hw *hw,
					   rnp_link_speed *speed, bool *autoneg)
{
	/* fix setup */
	/* reletive with firmware */
	*speed = RNP_LINK_SPEED_10GB_FULL;
	*autoneg = false;

	return 0;
}

static void rnp_set_layer2_hw_ops_n500(struct rnp_hw *hw, union rnp_atr_input *input,
	u16 pri_id, u8 queue, bool prio_flag)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.set_layer2_remapping(eth, input, pri_id, queue, prio_flag);

}

static void rnp_clr_layer2_hw_ops_n500(struct rnp_hw *hw, u16 pri_id)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clr_layer2_remapping(eth, pri_id);
}

static void rnp_clr_all_layer2_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clr_all_layer2_remapping(eth);

}

static void rnp_clr_all_tuple5_hw_ops_n500(struct rnp_hw *hw)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clr_all_tuple5_remapping(eth);

}

static void rnp_set_tcp_sync_hw_ops_n500(struct rnp_hw *hw, int queue, bool flag, bool prio)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	eth->ops.set_tcp_sync_remapping(eth, queue, flag, prio);

	//eth->ops.clr_all_tuple5_remapping(eth);

}

static void rnp_set_rx_skip_hw_ops_n500(struct rnp_hw *hw, int count, bool flag)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	eth->ops.set_rx_skip(eth, count, flag);

	//eth->ops.clr_all_tuple5_remapping(eth);

}

static void rnp_set_outer_vlan_type_hw_ops_n500(struct rnp_hw *hw, int type)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	
	eth->ops.set_outer_vlan_type(eth, type);

}

s32 rnp_phy_read_reg_hw_ops_n500(struct rnp_hw *hw, u32 reg_addr, u32 device_type, u16 *phy_data)
{
	struct rnp_mac_info *mac = &hw->mac;
	// n500 use mac 
	s32 status = 0;
	u32 data = 0;

	status = mac->ops.mdio_read(mac, reg_addr, &data);
	*phy_data = data & 0xffff;

	return status;
}

s32 rnp_phy_write_reg_hw_ops_n500(struct rnp_hw *hw, u32 reg_addr, u32 device_type, u16 phy_data)
{
	struct rnp_mac_info *mac = &hw->mac;
	s32 status = 0;

	status = mac->ops.mdio_write(mac, reg_addr, (u32)phy_data);

	return status;

} 

void rnp_setup_wol_hw_ops_n500(struct rnp_hw *hw, u32 mode)
{
	// only setup mac ? 
	struct rnp_mac_info *mac = &hw->mac;

	mac->ops.pmt(mac, mode);

}

void rnp_set_vf_vlan_mode_hw_ops_n500(struct rnp_hw *hw, u16 vlan, int vf, bool enable)
{
	struct rnp_eth_info *eth = &hw->eth;
	
	eth->ops.set_vf_vlan_mode(eth, vlan, vf, enable);


}

void rnp_driver_status_hw_ops_n500(struct rnp_hw *hw, bool enable, int mode)
{
	switch (mode) {
	case rnp_driver_insmod:
		rnp_mbx_ifinsmod(hw, enable);
	break;
	case rnp_driver_suspuse:
		rnp_mbx_ifsuspuse(hw, enable);
	break;

	}


}


static void rnp_set_tuple5_hw_ops_n500(struct rnp_hw *hw, union rnp_atr_input *input,
	u16 pri_id, u8 queue, bool prio_flag)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.set_tuple5_remapping(eth, input, pri_id, queue, prio_flag);

}

static void rnp_clr_tuple5_hw_ops_n500(struct rnp_hw *hw, u16 pri_id)
{
	struct rnp_eth_info *eth = &hw->eth;

	eth->ops.clr_tuple5_remapping(eth, pri_id);
}

static void rnp_update_hw_status_hw_ops_n500(struct rnp_hw *hw,
		struct rnp_hw_stats *hw_stats,
		struct net_device_stats *net_stats)
{
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_mac_info *mac = &hw->mac;
	int i;

	//todo 
	net_stats->rx_errors += eth_rd32(eth, RNP500_RX_MAC_GFCS_ERR_NUM) + 
		eth_rd32(eth, RNP500_RX_MAC_LEN_ERR_NUM) +
		eth_rd32(eth, RNP500_RX_MAC_SFCS_ERR_NUM) +
		eth_rd32(eth, RNP500_RX_MAC_GLEN_ERR_NUM) +
		eth_rd32(eth, RNP500_RX_MAC_SLEN_ERR_NUM);


	net_stats->collisions = eth_rd32(eth, RNP500_RX_MAC_LCS_ERR_NUM);
	net_stats->rx_over_errors = eth_rd32(eth, RNP500_RX_MAC_CUT_NUM);
	net_stats->rx_crc_errors = eth_rd32(eth, RNP500_RX_MAC_GFCS_ERR_NUM);
	

	// update hw_stats
	// 8230
	hw_stats->invalid_droped_packets = eth_rd32(eth, RNP500_RX_DROP_PKT_NUM);

	hw_stats->rx_capabity_lost = eth_rd32(eth, RNP500_RXTRANS_DROP) +
		eth_rd32(eth, RNP500_RXTRANS_CUT_ERR_PKTS);
	// 82ec
	hw_stats->filter_dropped_packets = eth_rd32(eth, RNP500_DECAP_PKT_DROP1_NUM);
	// 8410
	hw_stats->host_l2_match_drop = eth_rd32(eth, RNP500_ETH_HOST_L2_DROP_PKTS);
	// 8414
	hw_stats->redir_input_match_drop =
		eth_rd32(eth, RNP500_ETH_REDIR_INPUT_MATCH_DROP_PKTS);
	// 8418
	hw_stats->redir_etype_match_drop = eth_rd32(eth, RNP500_ETH_ETYPE_DROP_PKTS);
	// 841c
	hw_stats->redir_tcp_syn_match_drop =
		eth_rd32(eth, RNP500_ETH_TCP_SYN_DROP_PKTS);
	// 8420
	hw_stats->redir_tuple5_match_drop =
		eth_rd32(eth, RNP500_ETH_REDIR_TUPLE5_DROP_PKTS);

	//hw_stats->bmc_dropped_packets = eth_rd32(eth, RNP10_ETH_DECAP_BMC_DROP_NUM);
	//hw_stats->switch_dropped_packets =
		//eth_rd32(eth, RNP10_ETH_DECAP_SWITCH_DROP_NUM);

	//RNP500_VEB_VFMPRC
	//RNP500_VEB_VFBPRC

	hw_stats->tx_multicast = eth_rd32(eth, RNP500_TX_MULTI_NUM); 
	hw_stats->tx_broadcast = eth_rd32(eth, RNP500_TX_BROADCAST_NUM); 

	hw_stats->mac_rx_broadcast = 0;
	hw_stats->mac_rx_multicast = 0;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		hw_stats->mac_rx_multicast += dma_rd32(dma, RNP500_VEB_VFMPRC(i));
		hw_stats->mac_rx_broadcast += dma_rd32(dma, RNP500_VEB_VFBPRC(i));

	}
	hw_stats->dma_rx_drop_cnt_0 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(0)); 
	hw_stats->dma_rx_drop_cnt_1 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(1)); 
	hw_stats->dma_rx_drop_cnt_2 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(2)); 
	hw_stats->dma_rx_drop_cnt_3 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(3)); 
	hw_stats->dma_rx_drop_cnt_4 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(4)); 
	hw_stats->dma_rx_drop_cnt_5 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(5)); 
	hw_stats->dma_rx_drop_cnt_6 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(6)); 
	hw_stats->dma_rx_drop_cnt_7 = dma_rd32(dma, RNP500_RX_TIMEOUT_DROP(7)); 

	net_stats->multicast = hw_stats->mac_rx_multicast;

	hw_stats->ultra_short_cnt += mac_rd32(mac, GMAC_MANAGEMENT_RX_UNDERSIZE);
	hw_stats->jumbo_cnt += mac_rd32(mac, RNP500_MAC_GLEN_ERR_NUM);	
}


const struct rnp_stats rnp500_gstrings_net_stats[] = {
	RNP_NETDEV_STAT(rx_packets),
	RNP_NETDEV_STAT(tx_packets),
	RNP_NETDEV_STAT(rx_bytes),
	RNP_NETDEV_STAT(tx_bytes),
	RNP_NETDEV_STAT(rx_errors),
	RNP_NETDEV_STAT(tx_errors),
	RNP_NETDEV_STAT(rx_dropped),
	RNP_NETDEV_STAT(tx_dropped),
	RNP_NETDEV_STAT(multicast),
	RNP_NETDEV_STAT(collisions),
	RNP_NETDEV_STAT(rx_over_errors),
	RNP_NETDEV_STAT(rx_crc_errors),
	RNP_NETDEV_STAT(rx_frame_errors),
	RNP_NETDEV_STAT(rx_fifo_errors),
	RNP_NETDEV_STAT(rx_missed_errors),
	RNP_NETDEV_STAT(tx_aborted_errors),
	RNP_NETDEV_STAT(tx_carrier_errors),
	RNP_NETDEV_STAT(tx_fifo_errors),
	RNP_NETDEV_STAT(tx_heartbeat_errors),
};

#define RNP500_GLOBAL_STATS_LEN ARRAY_SIZE(rnp500_gstrings_net_stats)
// update me
static struct rnp_stats rnp500_hwstrings_stats[] = {
	//RNP_HW_STAT("dma_to_mac", hw_stats.dma_to_dma),
	//RNP_HW_STAT("dma_to_switch", hw_stats.dma_to_switch),
	// RNP_HW_STAT("mac_to_mac", hw_stats.mac_to_mac),
	// RNP_HW_STAT("switch_to_switch", hw_stats.switch_to_switch),
	//RNP_HW_STAT("eth_to_dma", hw_stats.mac_to_dma),
	// RNP_HW_STAT("switch_to_dma", hw_stats.switch_to_dma),
	RNP_HW_STAT("vlan_add_cnt", hw_stats.vlan_add_cnt),
	RNP_HW_STAT("vlan_strip_cnt", hw_stats.vlan_strip_cnt),
	//=== drop==
	RNP_HW_STAT("invalid_droped_packets", hw_stats.invalid_droped_packets),
	RNP_HW_STAT("rx_capabity_drop", hw_stats.rx_capabity_lost),
	RNP_HW_STAT("filter_dropped_packets", hw_stats.filter_dropped_packets),
	RNP_HW_STAT("host_l2_match_drop", hw_stats.host_l2_match_drop),
	RNP_HW_STAT("redir_input_match_drop", hw_stats.redir_input_match_drop),
	RNP_HW_STAT("redir_etype_match_drop", hw_stats.redir_etype_match_drop),
	RNP_HW_STAT("redir_tcp_syn_match_drop", hw_stats.redir_tcp_syn_match_drop),
	RNP_HW_STAT("redir_tuple5_match_drop", hw_stats.redir_tuple5_match_drop),

	// RNP_HW_STAT("driver_dropped_packets", hw_stats.driver_dropped_packets),
	//RNP_HW_STAT("bmc_dropped_packets", hw_stats.bmc_dropped_packets),
	//RNP_HW_STAT("switch_dropped_packets", hw_stats.switch_dropped_packets),
	RNP_HW_STAT("tx_multicast", hw_stats.tx_multicast),
	RNP_HW_STAT("tx_broadcast", hw_stats.tx_broadcast),
	RNP_HW_STAT("rx_csum_offload_errors", hw_csum_rx_error),
	RNP_HW_STAT("rx_csum_offload_good", hw_csum_rx_good),
	RNP_HW_STAT("rx_broadcast_count", hw_stats.mac_rx_broadcast),
	RNP_HW_STAT("rx_multicast_count", hw_stats.mac_rx_multicast),
	RNP_HW_STAT("ultra_short_packets", hw_stats.ultra_short_cnt),
	RNP_HW_STAT("jumbo_packets", hw_stats.jumbo_cnt),

	RNP_HW_STAT("dma_queue0_drop_cnt", hw_stats.dma_rx_drop_cnt_0),
	RNP_HW_STAT("dma_queue1_drop_cnt", hw_stats.dma_rx_drop_cnt_1),
	RNP_HW_STAT("dma_queue2_drop_cnt", hw_stats.dma_rx_drop_cnt_2),
	RNP_HW_STAT("dma_queue3_drop_cnt", hw_stats.dma_rx_drop_cnt_3),
	RNP_HW_STAT("dma_queue4_drop_cnt", hw_stats.dma_rx_drop_cnt_4),
	RNP_HW_STAT("dma_queue5_drop_cnt", hw_stats.dma_rx_drop_cnt_5),
	RNP_HW_STAT("dma_queue6_drop_cnt", hw_stats.dma_rx_drop_cnt_6),
	RNP_HW_STAT("dma_queue7_drop_cnt", hw_stats.dma_rx_drop_cnt_7),

};

#define RNP500_HWSTRINGS_STATS_LEN ARRAY_SIZE(rnp500_hwstrings_stats)

#define RNP500_STATS_LEN \
	(RNP500_GLOBAL_STATS_LEN + RNP500_HWSTRINGS_STATS_LEN + RNP_QUEUE_STATS_LEN)


#ifndef CLOST_SELF_TEST
#ifdef ETHTOOL_TEST
static const char rnp500_gstrings_test[][ETH_GSTRING_LEN] = {
	"Register test  (offline)",
	"Eeprom test    (offline)",
	"Interrupt test (offline)",
	"Loopback test  (offline)",
	"Link test   (on/offline)"};

#define RNP500_TEST_LEN (sizeof(rnp500_gstrings_test) / ETH_GSTRING_LEN)
#else
#define RNP500_TEST_LEN 0
#endif
#else
#define RNP500_TEST_LEN 0
#endif

#ifdef ETHTOOL_GLINKSETTINGS

int rnp500_get_link_ksettings(struct net_device *netdev,
		struct ethtool_link_ksettings *cmd)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	rnp_link_speed supported_link;
	rnp_link_speed advertised_link;
	bool autoneg = hw->autoneg;
	u32 supported = 0, advertising = 0;

	ethtool_convert_link_mode_to_legacy_u32(&supported,
			cmd->link_modes.supported);
//#ifdef NO_CM3_MBX
#if 0
	adapter->speed = hw->speed;
#else
	rnp_mbx_get_lane_stat(hw);
#endif

	supported_link = hw->supported_link;
	advertised_link = hw->advertised_link;


	if (hw->is_sgmii) {
		// sgmii is tp 10/100/1000 
		if (supported_link & RNP_LINK_SPEED_10GB_FULL)
			supported |= SUPPORTED_10000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_FULL)
			supported |= SUPPORTED_1000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_100_FULL)
			supported |= SUPPORTED_100baseT_Full;
		if (supported_link & RNP_LINK_SPEED_10_FULL)
			supported |= SUPPORTED_10baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_HALF)
			supported |= SUPPORTED_1000baseT_Half;
		if (supported_link & RNP_LINK_SPEED_100_HALF)
			supported |= SUPPORTED_100baseT_Half;
		if (supported_link & RNP_LINK_SPEED_10_HALF)
			supported |= SUPPORTED_10baseT_Half;

		if (autoneg) {
			if (advertised_link & RNP_LINK_SPEED_10GB_FULL)
				advertising |= SUPPORTED_10000baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_1GB_FULL)
				advertising |= SUPPORTED_1000baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_100_FULL)
				advertising |= SUPPORTED_100baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_10_FULL)
				advertising |= SUPPORTED_10baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_1GB_HALF)
				advertising |= SUPPORTED_1000baseT_Half;
			if (advertised_link & RNP_LINK_SPEED_100_HALF)
				advertising |= SUPPORTED_100baseT_Half;
			if (advertised_link & RNP_LINK_SPEED_10_HALF)
				advertising |= SUPPORTED_10baseT_Half;
			advertising |= ADVERTISED_TP;
		}

		supported |= SUPPORTED_TP;
		//advertising = supported;

		cmd->base.port = PORT_TP;
		// ecmd->transceiver = XCVR_EXTERNAL;
		cmd->base.phy_address = adapter->phy_addr;
		cmd->base.duplex = adapter->duplex;
		if (adapter->hw.link) {
			cmd->base.eth_tp_mdix = hw->tp_mdx;
		} else {
			cmd->base.eth_tp_mdix = ETH_TP_MDI_INVALID;
		}
		//printk("hw->tp_mdx is %x\n", hw->tp_mdx);
#ifdef ETH_TP_MDI_AUTO
		cmd->base.eth_tp_mdix_ctrl = hw->tp_mdix_ctrl;
#endif
		//autoneg = true;
	} else {
		if (supported_link & RNP_LINK_SPEED_10GB_FULL)
			supported |= SUPPORTED_10000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_FULL) {
			supported |= SUPPORTED_1000baseKX_Full;
		}
		if (supported_link & RNP_LINK_SPEED_25GB_FULL)
			supported |= SUPPORTED_40000baseKR4_Full;
		if (supported_link & RNP_LINK_SPEED_40GB_FULL)
			supported |= SUPPORTED_40000baseCR4_Full | SUPPORTED_40000baseSR4_Full |
				SUPPORTED_40000baseLR4_Full;

		if (advertised_link & RNP_LINK_SPEED_10GB_FULL)
			advertising |= SUPPORTED_10000baseT_Full;
		if (advertised_link & RNP_LINK_SPEED_1GB_FULL) {
			advertising |= SUPPORTED_1000baseKX_Full;
		}
		if (advertised_link & RNP_LINK_SPEED_25GB_FULL)
			advertising |= SUPPORTED_40000baseKR4_Full;
		if (advertised_link & RNP_LINK_SPEED_40GB_FULL)
			advertising |= SUPPORTED_40000baseCR4_Full | SUPPORTED_40000baseSR4_Full |
				SUPPORTED_40000baseLR4_Full;
		// must fibre
		supported |= SUPPORTED_FIBRE;
		advertising |= ADVERTISED_FIBRE;
		cmd->base.port = PORT_FIBRE;
		// ecmd->transceiver = XCVR_INTERNAL;
	}

	supported |= SUPPORTED_Autoneg;
	if (autoneg) {
		advertising |= ADVERTISED_Autoneg;
		//cmd->base.autoneg = adapter->an ? AUTONEG_ENABLE : AUTONEG_DISABLE;
		//if (hw->is_sgmii) {
		cmd->base.autoneg = AUTONEG_ENABLE;
		//}
	} else
		cmd->base.autoneg = AUTONEG_DISABLE;

	// set pause support
	supported |= SUPPORTED_Pause;

	switch (hw->fc.current_mode) {
		case rnp_fc_full:
			advertising |= ADVERTISED_Pause;
			break;
		case rnp_fc_rx_pause:
			advertising |= ADVERTISED_Pause | ADVERTISED_Asym_Pause;
			break;
		case rnp_fc_tx_pause:
			advertising |= ADVERTISED_Asym_Pause;
			break;
		default:
			advertising &= ~(ADVERTISED_Pause | ADVERTISED_Asym_Pause);
	}

	if (adapter->hw.link) {
		cmd->base.speed = adapter->speed;
	} else {
		cmd->base.speed = SPEED_UNKNOWN;
	}
	if (adapter->hw.duplex) {
		cmd->base.duplex = DUPLEX_FULL;
	} else {
		cmd->base.duplex = DUPLEX_HALF;
	}

	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.supported,
			supported);
	ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.advertising,
			advertising);

	return 0;
}

int rnp500_set_link_ksettings(struct net_device *netdev,
		const struct ethtool_link_ksettings *cmd)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	u32 advertised, old;
	s32 err = 0;
	u32 supported, advertising;
	u32 speed = 0, autoneg = 0, duplex = 0;

	ethtool_convert_link_mode_to_legacy_u32(&supported,
			cmd->link_modes.supported);
	ethtool_convert_link_mode_to_legacy_u32(&advertising,
			cmd->link_modes.advertising);
	rnp_mbx_get_lane_stat(hw);

	if ((hw->phy_type == rnp_media_type_copper) || (hw->phy.multispeed_fiber)) {
		/*
		 * this function does not support duplex forcing, but can
		 * limit the advertising of the adapter to the specified speed
		 */
		if (advertising & ~supported)
			return -EINVAL;

		/* only allow one speed at a time if no  */
		if (!cmd->base.autoneg) {
			if (cmd->base.speed == SPEED_1000)
				return -EINVAL;
			autoneg = 0;
			speed = cmd->base.speed;
			duplex = cmd->base.duplex;
		} else {
			autoneg = 1;
		}

		if (cmd->base.eth_tp_mdix_ctrl) {
			if ((cmd->base.eth_tp_mdix_ctrl != ETH_TP_MDI_AUTO) &&
					(cmd->base.autoneg != AUTONEG_ENABLE)) {
				dev_err(&adapter->pdev->dev, "forcing MDI/MDI-X state is not supported when link speed and/or duplex are forced\n");
				return -EINVAL;
			}
		}

		hw->autoneg = autoneg;
		hw->tp_mdix_ctrl = cmd->base.eth_tp_mdix_ctrl;

		old = hw->phy.autoneg_advertised;
		advertised = 0;
		if (advertising & ADVERTISED_10000baseT_Full)
			advertised |= RNP_LINK_SPEED_10GB_FULL;

		if (advertising & ADVERTISED_1000baseT_Full)
			advertised |= RNP_LINK_SPEED_1GB_FULL;

		if (advertising & ADVERTISED_100baseT_Full)
			advertised |= RNP_LINK_SPEED_100_FULL;

		if (advertising & ADVERTISED_10baseT_Full)
			advertised |= RNP_LINK_SPEED_10_FULL;

		if (advertising & ADVERTISED_100baseT_Half)
			advertised |= RNP_LINK_SPEED_100_HALF;

		if (advertising & ADVERTISED_10baseT_Half)
			advertised |= RNP_LINK_SPEED_10_HALF;
		/*
		 * if (advertising & ADVERTISED_10baseT_Full)
		 * advertised |= RNP_LINK_SPEED_10_FULL;
		 */

		// maybe only mdix setup
		//if (old == advertised)
		//	return err;
		/* this sets the link speed and restarts auto-neg */
		while (test_and_set_bit(__RNP_IN_SFP_INIT, &adapter->state))
			usleep_range(1000, 2000);

		hw->mac.autotry_restart = true;
		err = hw->ops.setup_link(hw, advertised, autoneg, speed, duplex);
		if (err) {
			//e_info(probe, "setup link failed with code %d\n", err);
			printk("setup link failed with code %d\n", err);
			hw->ops.setup_link(hw, old, autoneg, speed, duplex);
		}
		clear_bit(__RNP_IN_SFP_INIT, &adapter->state);
	} else {
		/* in this case we currently only support 10Gb/FULL */
#if 0
		u32 speed = cmd->base.speed;

		if (hw->enable_change_speed) {
			if (adapter->flags & RNP_FLAG_SRIOV_ENABLED) {
				printk("%s: can't change speed when sriov is enabled!\n",
					   netdev->name);
				return -EINVAL;
			}
			if ((speed == SPEED_1000 || speed == SPEED_10000)) {
				e_info(drv, "force speed: %d\n", speed);
				rnp_set_lane_fun(hw, LANE_FUN_SPEED_CHANGE, speed, 0, 0, 0);

				rnp_do_reset(netdev);
				return 0;
			} else {
				return -EINVAL;
			}
		}
#endif

		if (cmd->base.duplex == DUPLEX_HALF) {
			return -EINVAL;
		}
	}

	return err;
}
#else /* !ETHTOOL_GLINKSETTINGS */
int rnp500_get_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	rnp_link_speed supported_link;
	rnp_link_speed advertised_link;
	//u32 link_speed = 0;
	bool autoneg = true;
	//bool link_up;

	// hw->mac.ops.get_link_capabilities(hw, &supported_link, &autoneg,
	// &media_type);
//#ifndef NO_CM3_MBX
#if 1
	rnp_mbx_get_lane_stat(hw);
#else
	adapter->speed = hw->speed;
	//hw->ops.get_link_capabilities(hw, &supported_link, &autoneg, &media_type);
#endif
	supported_link = hw->supported_link;
	advertised_link = hw->advertised_link;

	/* set the supported link speeds */
//	if (supported_link & RNP_LINK_SPEED_10GB_FULL)
//		ecmd->supported |= SUPPORTED_10000baseT_Full;
//	if (supported_link & RNP_LINK_SPEED_1GB_FULL) {
//		ecmd->supported |= SUPPORTED_1000baseKX_Full;
//	}
//	if (supported_link & RNP_LINK_SPEED_25GB_FULL)
//		ecmd->supported |= SUPPORTED_40000baseKR4_Full;
//	if (supported_link & RNP_LINK_SPEED_40GB_FULL)
//		ecmd->supported |= SUPPORTED_40000baseCR4_Full |
//						   SUPPORTED_40000baseSR4_Full |
//						   SUPPORTED_40000baseLR4_Full;

	//ecmd->advertising = ecmd->supported;
	if (hw->is_sgmii) {
		//ecmd->supported |= ADVERTISED_1000baseT_Full |
		//				   ADVERTISED_100baseT_Full | ADVERTISED_10baseT_Full;
		if (supported_link & RNP_LINK_SPEED_10GB_FULL)
			ecmd->supported |= ADVERTISED_10000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_FULL)
			ecmd->supported |= ADVERTISED_1000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_100_FULL)
			ecmd->supported |= ADVERTISED_100baseT_Full;
		if (supported_link & RNP_LINK_SPEED_10_FULL)
			ecmd->supported |= ADVERTISED_10baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_HALF)
			ecmd->supported |= ADVERTISED_1000baseT_Half;
		if (supported_link & RNP_LINK_SPEED_100_HALF)
			ecmd->supported |= ADVERTISED_100baseT_Half;
		if (supported_link & RNP_LINK_SPEED_10_HALF)
			ecmd->supported |= ADVERTISED_10baseT_Half;

		if (hw->autoneg) {
			autoneg = true;
			if (advertised_link & RNP_LINK_SPEED_10GB_FULL)
				ecmd->advertising |= ADVERTISED_10000baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_1GB_FULL)
				ecmd->advertising |= ADVERTISED_1000baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_100_FULL)
				ecmd->advertising |= ADVERTISED_100baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_10_FULL)
				ecmd->advertising |= ADVERTISED_10baseT_Full;
			if (advertised_link & RNP_LINK_SPEED_1GB_HALF)
				ecmd->advertising |= ADVERTISED_1000baseT_Half;
			if (advertised_link & RNP_LINK_SPEED_100_HALF)
				ecmd->advertising |= ADVERTISED_100baseT_Half;
			if (advertised_link & RNP_LINK_SPEED_10_HALF)
				ecmd->advertising |= ADVERTISED_10baseT_Half;
		} else 
			autoneg = false;


		ecmd->supported |= SUPPORTED_TP;
		ecmd->advertising |= ADVERTISED_TP;
		ecmd->port = PORT_TP;
		ecmd->transceiver = XCVR_EXTERNAL;
		ecmd->phy_address = hw->mac.phy_addr;
		ecmd->duplex = adapter->duplex;
		ecmd->eth_tp_mdix = ETH_TP_MDI;
#ifdef ETH_TP_MDI_AUTO
		ecmd->eth_tp_mdix_ctrl = ETH_TP_MDI_AUTO;
#endif
	} else {
		if (supported_link & RNP_LINK_SPEED_10GB_FULL)
			ecmd->supported |= SUPPORTED_10000baseT_Full;
		if (supported_link & RNP_LINK_SPEED_1GB_FULL) {
			ecmd->supported |= SUPPORTED_1000baseKX_Full;
		}
		if (supported_link & RNP_LINK_SPEED_25GB_FULL)
			ecmd->supported |= SUPPORTED_40000baseKR4_Full;
		if (supported_link & RNP_LINK_SPEED_40GB_FULL)
			ecmd->supported |= SUPPORTED_40000baseCR4_Full |
				SUPPORTED_40000baseSR4_Full |
				SUPPORTED_40000baseLR4_Full;

		if (advertised_link & RNP_LINK_SPEED_10GB_FULL)
			ecmd->advertising |= SUPPORTED_10000baseT_Full;
		if (advertised_link & RNP_LINK_SPEED_1GB_FULL) {
			ecmd->advertising |= SUPPORTED_1000baseKX_Full;
		}
		if (advertised_link & RNP_LINK_SPEED_25GB_FULL)
			ecmd->advertising |= SUPPORTED_40000baseKR4_Full;
		if (advertised_link & RNP_LINK_SPEED_40GB_FULL)
			ecmd->advertising |= SUPPORTED_40000baseCR4_Full |
				SUPPORTED_40000baseSR4_Full |
				SUPPORTED_40000baseLR4_Full;

		ecmd->supported |= SUPPORTED_FIBRE;
		ecmd->advertising |= ADVERTISED_FIBRE;
		ecmd->port = PORT_FIBRE;
		ecmd->transceiver = XCVR_INTERNAL;
	}

	ecmd->supported |= SUPPORTED_Autoneg;
	if (autoneg) {
		ecmd->advertising |= ADVERTISED_Autoneg;
		//ecmd->autoneg = adapter->an ? AUTONEG_ENABLE : AUTONEG_DISABLE;
		//if (hw->phy_type == PHY_TYPE_RGMII) {
		ecmd->autoneg = AUTONEG_ENABLE;
		//}
	} else
		ecmd->autoneg = AUTONEG_DISABLE;

	/* Indicate pause support */
	ecmd->supported |= SUPPORTED_Pause;

	/*switch (hw->fc.requested_mode) {
	  case rnp_fc_full:
	  ecmd->advertising |= ADVERTISED_Pause;
	  break;
	  case rnp_fc_rx_pause:
	  ecmd->advertising |= ADVERTISED_Pause | ADVERTISED_Asym_Pause;
	  break;
	  case rnp_fc_tx_pause:
	  ecmd->advertising |= ADVERTISED_Asym_Pause;
	  break;
	  default:
	  ecmd->advertising &= ~(ADVERTISED_Pause | ADVERTISED_Asym_Pause);
	  }*/
	if (hw->fc.requested_mode & PAUSE_AUTO) 
		ecmd->advertising |= ADVERTISED_Pause;
	else {
		if ((hw->fc.requested_mode & PAUSE_TX) && (hw->fc.requested_mode & PAUSE_RX))
			ecmd->advertising |= ADVERTISED_Pause;
		else if (hw->fc.requested_mode & PAUSE_TX)
			ecmd->advertising |= ADVERTISED_Asym_Pause;
		else if (hw->fc.requested_mode & PAUSE_RX)
			ecmd->advertising |= ADVERTISED_Pause | ADVERTISED_Asym_Pause;
		else
			ecmd->advertising &= ~(ADVERTISED_Pause | ADVERTISED_Asym_Pause);
	}



	if (adapter->hw.link) {
		ethtool_cmd_speed_set(ecmd, adapter->speed);
		ecmd->duplex = DUPLEX_FULL;
	} else {
		ethtool_cmd_speed_set(ecmd, -1);
		ecmd->duplex = DUPLEX_HALF;
	}
	if (adapter->hw.duplex) {
		ecmd->duplex = DUPLEX_FULL;
	} else {
		ecmd->duplex = DUPLEX_HALF;
	}

	return 0;
}

int rnp500_set_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	u32 advertised, old;
	s32 err = 0;
	u32 autoneg = 0, speed = 0, duplex = 0;

	rnp_mbx_get_lane_stat(hw);

	if ((hw->phy_type == rnp_media_type_copper) || (hw->phy.multispeed_fiber)) {
		/*
		 * this function does not support duplex forcing, but can
		 * limit the advertising of the adapter to the specified speed
		 */
		// we only support diable autoneg if not 1000
		if (ecmd->autoneg == AUTONEG_DISABLE &&
				(ecmd->speed == SPEED_1000)) {
			printk("not allowd diable auto in 1000\n");
			return -EINVAL;
		}

		if (ecmd->advertising & ~ecmd->supported) {
			return -EINVAL;
		}
		if (ecmd->autoneg == AUTONEG_DISABLE) {
			autoneg = 0;
			speed = ecmd->speed;
			duplex = ecmd->duplex;
			//printk("diable autoneg\n");
			printk("speed %x duplex %x\n", ecmd->speed, ecmd->duplex);

		} else {
			autoneg = 1;
			//printk("open autoneg\n");
		}
		hw->autoneg = autoneg;

		//printk("adv is %x\n", ecmd->advertising);

		old = hw->phy.autoneg_advertised;
		advertised = 0;
		if (ecmd->advertising & ADVERTISED_10000baseT_Full)
			advertised |= RNP_LINK_SPEED_10GB_FULL;

		if (ecmd->advertising & ADVERTISED_1000baseT_Full)
			advertised |= RNP_LINK_SPEED_1GB_FULL;

		if (ecmd->advertising & ADVERTISED_100baseT_Full) {
			advertised |= RNP_LINK_SPEED_100_FULL;
			printk("100 full set\n");
		}

		if (ecmd->advertising & ADVERTISED_10baseT_Full) {
			advertised |= RNP_LINK_SPEED_10_FULL;
			printk("10 full set\n");
		}

		if (ecmd->advertising & ADVERTISED_100baseT_Half) {
			advertised |= RNP_LINK_SPEED_100_HALF;
			printk("100 half set\n");
		}

		if (ecmd->advertising & ADVERTISED_10baseT_Half) {
			advertised |= RNP_LINK_SPEED_10_HALF;
			printk("10 half set\n");
		}


		if (old == advertised)
			return err;
		/* this sets the link speed and restarts auto-neg */
		hw->mac.autotry_restart = true;
		err = hw->ops.setup_link(hw, advertised, autoneg, speed, duplex);
		if (err) {
			e_info(probe, "setup link failed with code %d\n", err);
			hw->ops.setup_link(hw, old, autoneg, speed, duplex);
		}
	} else {
		/* in this case we currently only support 10Gb/FULL */
		u32 speed = ethtool_cmd_speed(ecmd);

		if (speed != SPEED_1000)
			return -EINVAL;
		/*
		if ((ecmd->autoneg == AUTONEG_ENABLE) != adapter->an) {
			rnp_set_lane_fun(
				hw, LANE_FUN_AN, ecmd->autoneg == AUTONEG_ENABLE, 0, 0, 0);
			adapter->an = (ecmd->autoneg == AUTONEG_ENABLE);
		}*/

#if 0
		if (hw->enable_change_speed) {
			if (adapter->flags & RNP_FLAG_SRIOV_ENABLED) {
				printk("%s: can't change speed when sriov is enabled!\n",
					   netdev->name);
				return -EINVAL;
			}
			if ((speed == SPEED_1000 || speed == SPEED_10000)) {
				e_info(drv, "force speed: %d\n", speed);

				rnp_set_lane_fun(hw, LANE_FUN_SPEED_CHANGE, speed, 0, 0, 0);
				rnp_do_reset(netdev);
				return 0;
			} else {
				return -EINVAL;
			}
		}
#endif

		if (ecmd->duplex == DUPLEX_HALF) {
			return -EINVAL;
		}
	}

	// not so good
	return err;
}

#endif

static int rnp500_get_regs_len(struct net_device *netdev)
{
#define RNP500_REGS_LEN 1
	return RNP500_REGS_LEN * sizeof(u32);
}

static void rnp500_get_drvinfo(struct net_device *netdev,
		struct ethtool_drvinfo *drvinfo)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;

	strlcpy(drvinfo->driver, rnp_driver_name, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, rnp_driver_version, sizeof(drvinfo->version));

	// fixme later
#define FPGA_VESION
#ifdef FPGA_VESION
	snprintf(drvinfo->fw_version,
			 sizeof(drvinfo->fw_version),
			 "nic:0x%x fw:0x%x",
			 rd32(hw, RNP500_FPGA_VERSION),
			 hw->fw_version
			 );
#else
	snprintf(drvinfo->fw_version,
			 sizeof(drvinfo->fw_version),
			 "%d.%d.%d.%d 0x%08x",
			 ((char *)&(hw->fw_version))[3],
			 ((char *)&(hw->fw_version))[2],
			 ((char *)&(hw->fw_version))[1],
			 ((char *)&(hw->fw_version))[0],
			 hw->bd_uid);

#endif
	strlcpy(
		drvinfo->bus_info, pci_name(adapter->pdev), sizeof(drvinfo->bus_info));
	drvinfo->n_stats = RNP500_STATS_LEN;
	drvinfo->testinfo_len = RNP500_TEST_LEN;
	drvinfo->regdump_len = rnp500_get_regs_len(netdev);
#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
	drvinfo->n_priv_flags = RNP500_PRIV_FLAGS_STR_LEN;
#endif
}

static int rnp500_get_eeprom_len(struct net_device *netdev)
{
        struct rnp_adapter *adapter = netdev_priv(netdev);

        return adapter->hw.eeprom.word_size * 4;
}

static int rnp500_get_eeprom(struct net_device *netdev,
                            struct ethtool_eeprom *eeprom, u8 *bytes)
{
        struct rnp_adapter *adapter = netdev_priv(netdev);
        struct rnp_hw *hw = &adapter->hw;
        u32 *eeprom_buff;
        int first_u32, last_u32, eeprom_len;
        int ret_val = 0;

        if (eeprom->len == 0)
                return -EINVAL;

        eeprom->magic = hw->vendor_id | (hw->device_id << 16);

        /* assign to u32 */
        first_u32 = eeprom->offset >> 2;
        last_u32 = (eeprom->offset + eeprom->len - 1) >> 2;
        eeprom_len = last_u32 - first_u32 + 1;

        eeprom_buff = kmalloc_array(eeprom_len, sizeof(u32), GFP_KERNEL);
        if (!eeprom_buff)
                return -ENOMEM;

        //ret_val = hw->eeprom.ops.read_buffer(hw, first_u32, eeprom_len,
         //               eeprom_buff);
	// todo maybe use mbx


        /* Device's eeprom is always little-endian, word addressable */
        /*
        for (i = 0; i < eeprom_len; i++)
                le16_to_cpus(&eeprom_buff[i]);
        */

        memcpy(bytes, (u8 *)eeprom_buff + (eeprom->offset & 0x03), eeprom->len);
        kfree(eeprom_buff);

        return ret_val;

        return 0;

}

static int rnp500_set_eeprom(struct net_device *netdev,
                struct ethtool_eeprom *eeprom, u8 *bytes)
{


        return 0;
}

void rnp500_get_pauseparam(struct net_device *netdev,
		struct ethtool_pauseparam *pause)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;

	if (hw->fc.requested_mode & PAUSE_AUTO)
		pause->autoneg = 1;
	else
		pause->autoneg = 0;

	if (hw->fc.current_mode == rnp_fc_rx_pause) {
		pause->rx_pause = 1;
	} else if (hw->fc.current_mode == rnp_fc_tx_pause) {
		pause->tx_pause = 1;
	} else if (hw->fc.current_mode == rnp_fc_full) {
		pause->rx_pause = 1;
		pause->tx_pause = 1;
	}
}

int rnp500_set_pauseparam(struct net_device *netdev,
		struct ethtool_pauseparam *pause)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	struct rnp_fc_info fc = hw->fc;

	/* we not support change in dcb mode */
	if (adapter->flags & RNP_FLAG_DCB_ENABLED)
		return -EINVAL;

	/* we not support autoneg mode */
	//if (pause->autoneg == AUTONEG_ENABLE)
	//	return -EINVAL;

	fc.disable_fc_autoneg = (pause->autoneg != AUTONEG_ENABLE);
	//fc.disable_fc_autoneg = 0;
	
	fc.requested_mode = 0;

	if (pause->autoneg) {
		fc.requested_mode |= PAUSE_AUTO;
	} else {
		if (pause->tx_pause)
			fc.requested_mode |= PAUSE_TX;
		if (pause->rx_pause)
			fc.requested_mode |= PAUSE_RX;

	}

	rnp_mbx_phy_pause_set(hw, fc.requested_mode);
	/*
	if ((pause->rx_pause && pause->tx_pause) || (pause->autoneg))
		fc.requested_mode = rnp_fc_full;
	else if (pause->rx_pause && !pause->tx_pause)
		fc.requested_mode = rnp_fc_rx_pause;
	else if (!pause->rx_pause && pause->tx_pause)
		fc.requested_mode = rnp_fc_tx_pause;
	else
		fc.requested_mode = rnp_fc_none;
	*/
	//printk("requested_mode is %x\n", fc.requested_mode);

	//hw->fc = fc;
	// send new request_mode to firmware

	/* if the thing changed then we'll update and use new autoneg */
	//if (memcmp(&fc, &hw->fc, sizeof(struct rnp_fc_info))) {
        //        /* to tell all vf new pause status */
        //        // dbg("2 requested_mode is %d\n", hw->fc.requested_mode);
        //        hw->fc = fc;
        //        rnp_msg_post_status(adapter, PF_PAUSE_STATUS);
        //        if (netif_running(netdev))
        //                rnp_reinit_locked(adapter);
        //        else
        //                rnp_reset(adapter);
        //}

	return 0;
}


static void
rnp500_get_regs(struct net_device *netdev, struct ethtool_regs *regs, void *p)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	u32 *regs_buff = p;
	int i;

	memset(p, 0, RNP500_REGS_LEN * sizeof(u32));

	for (i = 0; i < RNP500_REGS_LEN; i++)
		regs_buff[i] = rd32(hw, i * 4);
}

static void rnp500_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	char *p = (char *)data;
	int i;
	struct rnp_ring *ring;
	u32 dma_ch;

	switch (stringset) {
		/* maybe we don't support test? */
#ifndef CLOST_SELF_TEST
		case ETH_SS_TEST:
			for (i = 0; i < RNP500_TEST_LEN; i++) {
				memcpy(data, rnp500_gstrings_test[i], ETH_GSTRING_LEN);
				data += ETH_GSTRING_LEN;
			}
			break;
#endif
		case ETH_SS_STATS:
			for (i = 0; i < RNP500_GLOBAL_STATS_LEN; i++) {
				memcpy(
					p, rnp500_gstrings_net_stats[i].stat_string, ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < RNP500_HWSTRINGS_STATS_LEN; i++) {
				memcpy(p, rnp500_hwstrings_stats[i].stat_string, ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
			for (i = 0; i < RNP_NUM_TX_QUEUES; i++) {
				//====  tx ========
				ring = adapter->tx_ring[i];
				dma_ch = ring->rnp_queue_idx;
#define SHORT_STATS
#ifdef SHORT_STATS
				sprintf(p, "---\n     queue%u_tx_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_bytes", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_tx_restart", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_busy", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_done_old", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_clean_desc", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_poll_count", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_irq_more", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_tx_hw_head", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_hw_tail", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_sw_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_sw_next_to_use", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_bytes", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_bytes_to_hw", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_todo_update", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_send_done_bytes", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_added_vlan_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_irq_miss", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_tx_equal_count", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_clean_times", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_tx_clean_count", i);
				p += ETH_GSTRING_LEN;

				//====  rx ========
				ring = adapter->rx_ring[i];
				dma_ch = ring->rnp_queue_idx;
				sprintf(p, "queue%u_rx_packets", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_bytes", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_driver_drop_packets", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_rsc", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_rsc_flush", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_non_eop_descs", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_alloc_page_failed", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_alloc_buff_failed", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_alloc_page", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_csum_offload_errs", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_csum_offload_good", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_poll_again_count", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_rm_vlan_packets", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_hw_head", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_hw_tail", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_sw_next_to_use", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_sw_next_to_clean", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_next_to_clean", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_irq_miss", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_equal_count", i);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_rx_clean_times", i);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_rx_clean_count", i);
				p += ETH_GSTRING_LEN;
#else
				sprintf(p, "\n     queue%u_dma%u_tx_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_tx_restart", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_busy", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_done_old", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_clean_desc", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_poll_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_irq_more", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_tx_hw_head", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_hw_tail", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_sw_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_sw_next_to_use", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_bytes_to_hw", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_todo_update", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_send_done_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_added_vlan_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_irq_miss", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_equal_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_tx_clean_times", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dam%u_tx_clean_count", i, dma_ch);
				p += ETH_GSTRING_LEN;

				//====  rx ========
				ring = adapter->rx_ring[i];
				dma_ch = ring->rnp_queue_idx;
				sprintf(p, "----\n     queue%u_dma%u_rx_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_bytes", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_driver_drop_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_rx_rsc", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_rsc_flush", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_non_eop_descs", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_alloc_page_failed", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_alloc_buff_failed", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_csum_offload_errs", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_csum_offload_good", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_poll_again_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_rm_vlan_packets", i, dma_ch);
				p += ETH_GSTRING_LEN;

				sprintf(p, "queue%u_dma%u_rx_hw_head", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_hw_tail", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_sw_next_to_use", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_sw_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_next_to_clean", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_irq_miss", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_equal_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dma%u_rx_clean_times", i, dma_ch);
				p += ETH_GSTRING_LEN;
				sprintf(p, "queue%u_dam%u_rx_clean_count", i, dma_ch);
				p += ETH_GSTRING_LEN;
#endif
			}

			break;
#ifdef HAVE_ETHTOOL_GET_SSET_COUNT
		case ETH_SS_PRIV_FLAGS:
			memcpy(data,
				   rnp500_priv_flags_strings,
				   RNP500_PRIV_FLAGS_STR_LEN * ETH_GSTRING_LEN);
			break;
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */
	}
}

#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
static int rnp500_get_stats_count(struct net_device *netdev)
{

	return RNP500_STATS_LEN;
}

#else

static int rnp500_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
		/* now we don't support test */
#ifndef CLOST_SELF_TEST
		case ETH_SS_TEST:
			return RNP500_TEST_LEN;
#endif
		case ETH_SS_STATS:
			return RNP500_STATS_LEN;
		case ETH_SS_PRIV_FLAGS:
			return RNP500_PRIV_FLAGS_STR_LEN;
		default:
			return -EOPNOTSUPP;
	}
}

static u32 rnp500_get_priv_flags(struct net_device *netdev)
{
	struct rnp_adapter *adapter = (struct rnp_adapter *)netdev_priv(netdev);
	u32 priv_flags = 0;
	// dbg("adapter priv is %x\n",iface->priv_flags);

	if (adapter->priv_flags & RNP_PRIV_FLAG_MAC_LOOPBACK)
		priv_flags |= RNP500_MAC_LOOPBACK;
	if (adapter->priv_flags & RNP_PRIV_FLAG_FT_PADDING)
		priv_flags |= RNP500_FT_PADDING;
	if (adapter->priv_flags & RNP_PRIV_FLAG_PADDING_DEBUG)
		priv_flags |= RNP500_PADDING_DEBUG;
	if (adapter->priv_flags & RNP_PRIV_FLAG_SIMUATE_DOWN)
		priv_flags |= RNP500_SIMULATE_DOWN;
	if (adapter->priv_flags & RNP_PRIV_FLAG_ULTRA_SHORT)
		priv_flags |= RNP500_ULTRA_SHORT;
	if (adapter->priv_flags & RNP_PRIV_FLAG_DOUBLE_VLAN)
		priv_flags |= RNP500_DOUBLE_VLAN;
	if (adapter->priv_flags & RNP_PRIV_FLAG_PAUSE_OWN)
		priv_flags |= RNP500_PAUSE_OWN;
	if (adapter->flags2 & RNP_FLAG2_VLAN_STAGS_ENABLED)
		priv_flags |= RNP500_STAGS_ENABLE;
	if (adapter->priv_flags & RNP_PRIV_FLAG_JUMBO)
		priv_flags |= RNP500_JUMBO_ENABLE;
	if (adapter->priv_flags & RNP_PRIV_FLAG_TX_PADDING)
		priv_flags |= RNP500_TX_PADDING;
	if (adapter->priv_flags & RNP_PRIV_FLAG_REC_HDR_LEN_ERR)
		priv_flags |= RNP500_REC_HDR_LEN_ERR;
	if (adapter->priv_flags & RNP_PRIV_FLAG_DOUBLE_VLAN_RECEIVE)
		priv_flags |= RNP500_DOUBLE_VLAN_RECEIVE;
	if (adapter->priv_flags & RNP_PRIV_FLAG_RX_SKIP_EN)
		priv_flags |= RNP500_RX_SKIP_EN;
	if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC_PRIO)
		priv_flags |= RNP500_TCP_SYNC_PRIO;
	if (adapter->priv_flags & RNP_PRIV_FLAG_REMAP_PRIO)
		priv_flags |= RNP500_REMAP_PRIO;
	if (adapter->priv_flags & RNP_PRIV_FLAG_8023_PRIO)
		priv_flags |= RNP500_8023_PRIO;
	if (adapter->priv_flags & RNP_PRIV_FLAG_SRIOV_VLAN_MODE)
		priv_flags |= RNP500_SRIOV_VLAN_MODE;
	


	return priv_flags;
}

static int rnp500_set_priv_flags(struct net_device *netdev, u32 priv_flags)
{
	struct rnp_adapter *adapter = (struct rnp_adapter *)netdev_priv(netdev);
	struct rnp_hw *hw = &adapter->hw;
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_mac_info *mac = &hw->mac;
	u32 data_old;
	u32 data_new;

	data_old = dma_rd32(dma, RNP_DMA_CONFIG);
	data_new = data_old;
	dbg("data old is %x\n", data_old);

	if (priv_flags & RNP500_MAC_LOOPBACK) {
		SET_BIT(n500_mac_loopback, data_new);
		adapter->priv_flags |= RNP_PRIV_FLAG_MAC_LOOPBACK;
	} else if (adapter->priv_flags & RNP_PRIV_FLAG_MAC_LOOPBACK) {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_MAC_LOOPBACK);
		CLR_BIT(n500_mac_loopback, data_new);
	}

	if (priv_flags & RNP500_FT_PADDING) {
		SET_BIT(n500_padding_enable, data_new);
		adapter->priv_flags |= RNP_PRIV_FLAG_FT_PADDING;
	} else if (adapter->priv_flags & RNP_PRIV_FLAG_FT_PADDING) {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_FT_PADDING);
		CLR_BIT(n500_padding_enable, data_new);
	}

	if (priv_flags & RNP500_PADDING_DEBUG)
		adapter->priv_flags |= RNP_PRIV_FLAG_PADDING_DEBUG;
	else if (adapter->priv_flags & RNP_PRIV_FLAG_PADDING_DEBUG)
		adapter->priv_flags &= (~RNP_PRIV_FLAG_PADDING_DEBUG);


	if (priv_flags & RNP500_SIMULATE_DOWN) {
		adapter->priv_flags |= RNP_PRIV_FLAG_SIMUATE_DOWN;
		/* set check link again */
		adapter->flags |= RNP_FLAG_NEED_LINK_UPDATE;
	} else if (adapter->priv_flags & RNP_PRIV_FLAG_SIMUATE_DOWN) {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_SIMUATE_DOWN);
		/* set check link again */
		adapter->flags |= RNP_FLAG_NEED_LINK_UPDATE;
	}

	// if open ultra short function
	if (priv_flags & RNP500_ULTRA_SHORT) {
		int min = 33;

		adapter->priv_flags |= RNP_PRIV_FLAG_ULTRA_SHORT;
		eth_wr32(eth, RNP500_ETH_DEFAULT_RX_MIN_LEN, min); 	


	} else {
		int min = 60;

		adapter->priv_flags &= (~RNP_PRIV_FLAG_ULTRA_SHORT);
		eth_wr32(eth, RNP500_ETH_DEFAULT_RX_MIN_LEN, min); 	

	}
//#define UNICAST_PAUSE BIT(3)
	if (priv_flags & RNP500_PAUSE_OWN) {
		u32 data;

		data = mac_rd32(mac, GMAC_FLOW_CTRL);
		data |= GMAC_FLOW_CTRL_UP;
		adapter->priv_flags |= RNP_PRIV_FLAG_PAUSE_OWN;
		mac_wr32(mac, GMAC_FLOW_CTRL, data);

	} else {
		u32 data;

		data = mac_rd32(mac, GMAC_FLOW_CTRL);
		data &= (~GMAC_FLOW_CTRL_UP);
		adapter->priv_flags &= (~RNP_PRIV_FLAG_PAUSE_OWN);
		mac_wr32(mac, GMAC_FLOW_CTRL, data);
	}

	if (priv_flags & RNP500_DOUBLE_VLAN) {
		//todo
		adapter->priv_flags |= RNP_PRIV_FLAG_DOUBLE_VLAN;
		eth->ops.set_double_vlan(eth, true);
		//eth_wr32(eth, RNP500_ETH_VLAN_RM_TYPE, 1);

	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_DOUBLE_VLAN);
		eth->ops.set_double_vlan(eth, false);
		//eth_wr32(eth, RNP500_ETH_VLAN_RM_TYPE, 0);
	}

	if (priv_flags & RNP500_STAGS_ENABLE) {
		eth_wr32(eth, RNP500_ETH_TX_VLAN_CONTROL_EANBLE, 1);
		adapter->flags2 |= RNP_FLAG2_VLAN_STAGS_ENABLED;
		// should setup vlan filter
		eth->ops.set_vfta(eth, adapter->stags_vid, true);
	} else {
		int true_remove = 1;
		int vid = adapter->stags_vid;	
		
		if (vid) {
			eth_wr32(eth, RNP500_ETH_TX_VLAN_CONTROL_EANBLE, 0);
			adapter->flags2 &= (~RNP_FLAG2_VLAN_STAGS_ENABLED);
			// should check and remove vlan filter?
#ifndef HAVE_VLAN_RX_REGISTER
			if (test_bit(vid, adapter->active_vlans))
				true_remove = 0;

#ifdef NETIF_F_HW_VLAN_STAG_RX
			if (test_bit(vid, adapter->active_vlans_stags))
				true_remove = 0;
#endif
#endif
			if (true_remove)
				hw->ops.set_vlan_filter(hw, vid, false, false);
		}
	}

	if (priv_flags & RNP500_JUMBO_ENABLE) {
		adapter->priv_flags |= RNP_PRIV_FLAG_JUMBO;
		hw->ops.set_mtu(hw, netdev->mtu);
	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_JUMBO);
		hw->ops.set_mtu(hw, netdev->mtu);
	}

	if (priv_flags & RNP500_TX_PADDING) {
		adapter->priv_flags |= RNP_PRIV_FLAG_TX_PADDING;
	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_TX_PADDING);
	}

	
        if (priv_flags & RNP500_REC_HDR_LEN_ERR) {
                adapter->priv_flags |= RNP_PRIV_FLAG_REC_HDR_LEN_ERR;
                eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, PKT_LEN_ERR | HDR_LEN_ERR);

        } else if (adapter->priv_flags & RNP_PRIV_FLAG_REC_HDR_LEN_ERR) {
                adapter->priv_flags &= (~RNP_PRIV_FLAG_REC_HDR_LEN_ERR);
                // clean mask
                eth_wr32(eth, RNP500_ETH_ERR_MASK_VECTOR, 0);
        }

	if (priv_flags & RNP500_DOUBLE_VLAN_RECEIVE) {
		adapter->priv_flags |= RNP_PRIV_FLAG_DOUBLE_VLAN_RECEIVE;
		// update to hw only not in rx-all
		if (!(adapter->priv_flags & RNP_PRIV_FLAG_RX_ALL))
			eth_wr32(eth, RNP500_ETH_DOUBLE_VLAN_DROP, 0);
	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_DOUBLE_VLAN_RECEIVE);
		// update to hw only not in rx-all
		if (!(adapter->priv_flags & RNP_PRIV_FLAG_RX_ALL))
			eth_wr32(eth, RNP500_ETH_DOUBLE_VLAN_DROP, 1);

	}
	if (priv_flags & RNP500_RX_SKIP_EN) {
		u32 data;

		adapter->priv_flags |= RNP_PRIV_FLAG_RX_SKIP_EN;

		data = PRIV_DATA_EN | adapter->priv_skip_count; 
		eth_wr32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG, data);

	} else {
		u32 data = eth_rd32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG);

		adapter->priv_flags &= (~RNP_PRIV_FLAG_RX_SKIP_EN);

		data &= (~PRIV_DATA_EN); 
		eth_wr32(eth, RNP500_ETH_PRIV_DATA_CONTROL_REG, data);
	}

	// update sync prio
	if (priv_flags & RNP500_TCP_SYNC_PRIO) {
		adapter->priv_flags |= RNP_PRIV_FLAG_TCP_SYNC_PRIO;

	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_TCP_SYNC_PRIO);

	}

	if (priv_flags & RNP500_SRIOV_VLAN_MODE) {
		int i ;

		adapter->priv_flags |= RNP_PRIV_FLAG_SRIOV_VLAN_MODE;
		if (!(adapter->flags & RNP_FLAG_SRIOV_INIT_DONE))
			goto skip_setup_vf_vlan_n500;
		// should setup vlvf table 
		for (i = 0; i < adapter->num_vfs; i++) {
			if (hw->ops.set_vf_vlan_mode) {
				if (adapter->vfinfo[i].vf_vlan)
					hw->ops.set_vf_vlan_mode(hw, adapter->vfinfo[i].vf_vlan, i, true);

				if (adapter->vfinfo[i].pf_vlan)
					hw->ops.set_vf_vlan_mode(hw, adapter->vfinfo[i].pf_vlan, i, true);
			}

		}


	} else if (adapter->priv_flags & RNP_PRIV_FLAG_SRIOV_VLAN_MODE) {
		int i;
		adapter->priv_flags &= (~RNP_PRIV_FLAG_SRIOV_VLAN_MODE);
		// should clean vlvf table
		for (i = 0; i < hw->max_vfs; i++) {
			if (hw->ops.set_vf_vlan_mode)
				hw->ops.set_vf_vlan_mode(hw, 0, i, false);

		}

	}
skip_setup_vf_vlan_n500:

	if (priv_flags & RNP500_8023_PRIO) {
		adapter->priv_flags |= RNP_PRIV_FLAG_8023_PRIO;
		eth_wr32(eth, RNP500_PRIORITY_EN_8023, 1);
	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_8023_PRIO);
		eth_wr32(eth, RNP500_PRIORITY_EN_8023, 0);
	}
		// should clear all remap?
	if (priv_flags & RNP500_REMAP_PRIO) {
		adapter->priv_flags |= RNP_PRIV_FLAG_REMAP_PRIO;
	} else {
		adapter->priv_flags &= (~RNP_PRIV_FLAG_REMAP_PRIO);
	}
	// if open remap prio or 8023 prio should open prio control
	if (priv_flags & (RNP500_8023_PRIO | RNP500_REMAP_PRIO)) {
		eth_wr32(eth, RNP500_PRIORITY_1_MARK, RNP500_PRIORITY_1);
		eth_wr32(eth, RNP500_PRIORITY_0_MARK, RNP500_PRIORITY_0);
		eth_wr32(eth, RNP500_PRIORITY_EN, 1);
	} else {
		eth_wr32(eth, RNP500_PRIORITY_EN, 0);

	}

	if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC) {
		if (adapter->priv_flags & RNP_PRIV_FLAG_TCP_SYNC_PRIO)
			hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue, true, true);
		else
			hw->ops.set_tcp_sync_remapping(hw, adapter->tcp_sync_queue, true, false);
	}
	//  upate
	
	if (data_old != data_new)
		dma_wr32(dma, RNP_DMA_CONFIG, data_new);
	/* if ft_padding changed */
	if (CHK_BIT(n500_padding_enable, data_old) !=
		CHK_BIT(n500_padding_enable, data_new)) {
		rnp_msg_post_status(adapter, PF_FT_PADDING_STATUS);
	}


	return 0;
}

#endif

static void rnp500_get_ethtool_stats(struct net_device *netdev,
		struct ethtool_stats *stats,
		u64 *data)
{
	struct rnp_adapter *adapter = netdev_priv(netdev);
	struct net_device_stats *net_stats = &netdev->stats;
	struct rnp_ring *ring;
	int i, j;
	char *p = NULL;

	// test prio map
	// for (i = 0; i < 16; i++)
	//	printk("prio_map %d is %d\n", i, netdev->prio_tc_map[i & TC_BITMASK]);

	rnp_update_stats(adapter);

	for (i = 0; i < RNP500_GLOBAL_STATS_LEN; i++) {
		p = (char *)net_stats + rnp500_gstrings_net_stats[i].stat_offset;
		data[i] = (rnp500_gstrings_net_stats[i].sizeof_stat == sizeof(u64))
					  ? *(u64 *)p
					  : *(u32 *)p;
	}
	for (j = 0; j < RNP500_HWSTRINGS_STATS_LEN; j++, i++) {
		p = (char *)adapter + rnp500_hwstrings_stats[j].stat_offset;
		data[i] = (rnp500_hwstrings_stats[j].sizeof_stat == sizeof(u64))
					  ? *(u64 *)p
					  : *(u32 *)p;
	}

	BUG_ON(RNP_NUM_TX_QUEUES != RNP_NUM_RX_QUEUES);

	for (j = 0; j < RNP_NUM_TX_QUEUES; j++) {
		int idx;
		/* tx-ring */
		ring = adapter->tx_ring[j];
		if (!ring) {
			// tx
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			// rx
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			continue;
		}
		idx = ring->rnp_queue_idx;

		data[i++] = ring->stats.packets;
		data[i++] = ring->stats.bytes;

		data[i++] = ring->tx_stats.restart_queue;
		data[i++] = ring->tx_stats.tx_busy;
		data[i++] = ring->tx_stats.tx_done_old;
		data[i++] = ring->tx_stats.clean_desc;
		data[i++] = ring->tx_stats.poll_count;
		data[i++] = ring->tx_stats.irq_more_count;

		/* rnp_tx_queue_ring_stat */
		data[i++] = ring_rd32(ring, RNP_DMA_REG_TX_DESC_BUF_HEAD);
		data[i++] = ring_rd32(ring, RNP_DMA_REG_TX_DESC_BUF_TAIL);
		data[i++] = ring->next_to_clean;
		data[i++] = ring->next_to_use;
		data[i++] = ring->tx_stats.send_bytes;
		data[i++] = ring->tx_stats.send_bytes_to_hw;
		data[i++] = ring->tx_stats.todo_update;
		data[i++] = ring->tx_stats.send_done_bytes;
		data[i++] = ring->tx_stats.vlan_add;
		if (ring->tx_stats.tx_next_to_clean == -1)
			data[i++] = ring->count;
		else
			data[i++] = ring->tx_stats.tx_next_to_clean;
		data[i++] = ring->tx_stats.tx_irq_miss;
		data[i++] = ring->tx_stats.tx_equal_count;
		data[i++] = ring->tx_stats.tx_clean_times;
		data[i++] = ring->tx_stats.tx_clean_count;

		/* rx-ring */
		ring = adapter->rx_ring[j];
		if (!ring) {
			// rx
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			data[i++] = 0;
			continue;
		}
		idx = ring->rnp_queue_idx;
		data[i++] = ring->stats.packets;
		data[i++] = ring->stats.bytes;

		data[i++] = ring->rx_stats.driver_drop_packets;
		data[i++] = ring->rx_stats.rsc_count;
		data[i++] = ring->rx_stats.rsc_flush;
		data[i++] = ring->rx_stats.non_eop_descs;
		data[i++] = ring->rx_stats.alloc_rx_page_failed;
		data[i++] = ring->rx_stats.alloc_rx_buff_failed;
		data[i++] = ring->rx_stats.alloc_rx_page;
		data[i++] = ring->rx_stats.csum_err;
		data[i++] = ring->rx_stats.csum_good;
		data[i++] = ring->rx_stats.poll_again_count;
		data[i++] = ring->rx_stats.vlan_remove;

		/* rnp_rx_queue_ring_stat */
		data[i++] = ring_rd32(ring, RNP_DMA_REG_RX_DESC_BUF_HEAD);
		data[i++] = ring_rd32(ring, RNP_DMA_REG_RX_DESC_BUF_TAIL);
		data[i++] = ring->next_to_use;
		data[i++] = ring->next_to_clean;
		if (ring->rx_stats.rx_next_to_clean == -1)
			data[i++] = ring->count;
		else
			data[i++] = ring->rx_stats.rx_next_to_clean;
		data[i++] = ring->rx_stats.rx_irq_miss;
		data[i++] = ring->rx_stats.rx_equal_count;
		data[i++] = ring->rx_stats.rx_clean_times;
		data[i++] = ring->rx_stats.rx_clean_count;
	}
}


/* n10 ethtool_ops ops here */

static const struct ethtool_ops rnp500_ethtool_ops = {

#ifdef ETHTOOL_GLINKSETTINGS
	.get_link_ksettings = rnp500_get_link_ksettings,
	.set_link_ksettings = rnp500_set_link_ksettings,
#else
	.get_settings = rnp500_get_settings,
	.set_settings = rnp500_set_settings,
#endif
	.get_drvinfo = rnp500_get_drvinfo,

	.get_regs_len = rnp500_get_regs_len,
	.get_regs = rnp500_get_regs,
	.get_wol = rnp_get_wol,
	.set_wol = rnp_set_wol,
	//.nway_reset             = rnp_nway_reset,
	.get_link = ethtool_op_get_link,
	.get_eeprom_len         = rnp500_get_eeprom_len,
	.get_eeprom             = rnp500_get_eeprom,
	.set_eeprom             = rnp500_set_eeprom,
	.get_ringparam = rnp_get_ringparam,
	.set_ringparam = rnp_set_ringparam,
	.get_pauseparam = rnp500_get_pauseparam,
	.set_pauseparam = rnp500_set_pauseparam,
	.get_msglevel = rnp_get_msglevel,
	.set_msglevel = rnp_set_msglevel,
#ifdef ETHTOOL_GFECPARAM
	.get_fecparam = rnp_get_fecparam,
	.set_fecparam = rnp_set_fecparam,
#endif
#ifndef CLOST_SELF_TEST
#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
	//.self_test_count        = rnp_diag_test_count,
#endif
	// todo 
	.self_test = rnp_diag_test,
#endif
	.get_strings = rnp500_get_strings,
#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef HAVE_ETHTOOL_SET_PHYS_ID
	.set_phys_id = rnp_set_phys_id,
#else
//.phys_id  = rnp_phys_id,
#endif /* HAVE_ETHTOOL_SET_PHYS_ID */
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
#ifndef HAVE_ETHTOOL_GET_SSET_COUNT
	.get_stats_count = rnp500_get_stats_count,
#else  /* HAVE_ETHTOOL_GET_SSET_COUNT */
	.get_sset_count = rnp500_get_sset_count,
	.get_priv_flags = rnp500_get_priv_flags, // priv flags
	// todo
	.set_priv_flags = rnp500_set_priv_flags,
#endif /* HAVE_ETHTOOL_GET_SSET_COUNT */
	.get_ethtool_stats = rnp500_get_ethtool_stats,
#ifdef HAVE_ETHTOOL_GET_PERM_ADDR
	.get_perm_addr = ethtool_op_get_perm_addr,
#endif /* HAVE_ETHTOOL_GET_PERM_ADDR */
	.get_coalesce = rnp_get_coalesce,
	.set_coalesce = rnp_set_coalesce,
#ifdef ETHTOOL_COALESCE_USECS
	.supported_coalesce_params = ETHTOOL_COALESCE_USECS,
#endif /* ETHTOOL_COALESCE_USECS */

#ifndef HAVE_NDO_SET_FEATURES
	.get_rx_csum = rnp_get_rx_csum,
	.set_rx_csum = rnp_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = rnp_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = ethtool_op_get_tso,
	.set_tso = rnp_set_tso,
#endif /* NETIF_F_TSO */
#ifdef ETHTOOL_GFLAGS
	.get_flags = ethtool_op_get_flags,
//.set_flags              = rnp_set_flags,
#endif
#endif /* HAVE_NDO_SET_FEATURES */
#ifdef ETHTOOL_GRXRINGS
	.get_rxnfc = rnp_get_rxnfc,
	.set_rxnfc = rnp_set_rxnfc,
#endif

#ifdef ETHTOOL_SRXNTUPLE
	.set_rx_ntuple = rnp_set_rx_ntuple,
#endif
#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef ETHTOOL_GEEE
//.get_eee                = rnp_get_eee,
#endif /* ETHTOOL_GEEE */
#ifdef ETHTOOL_SEEE
//.set_eee                = rnp_set_eee,
#endif /* ETHTOOL_SEEE */
#ifdef ETHTOOL_SCHANNELS
	.get_channels = rnp_get_channels,
	.set_channels = rnp_set_channels,
#endif
#ifdef ETHTOOL_GMODULEINFO
	.get_module_info = rnp_get_module_info,
	.get_module_eeprom = rnp_get_module_eeprom,
#endif
#ifdef HAVE_ETHTOOL_GET_TS_INFO
	.get_ts_info = rnp_get_ts_info,
#endif
#if defined(ETHTOOL_GRSSH) && defined(ETHTOOL_SRSSH)
	.get_rxfh_indir_size = rnp_rss_indir_size,
	.get_rxfh_key_size = rnp_get_rxfh_key_size,
	.get_rxfh = rnp_get_rxfh,
	.set_rxfh = rnp_set_rxfh,
#endif /* ETHTOOL_GRSSH && ETHTOOL_SRSSH */

	.get_dump_flag = rnp_get_dump_flag,
	.get_dump_data = rnp_get_dump_data,
	.set_dump = rnp_set_dump,
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */

#ifdef HAVE_DDP_PROFILE_UPLOAD_SUPPORT
	.flash_device = rnp_flash_device,
#endif /* HAVE_DDP_PROFILE_UPLOAD_SUPPORT */
};


void rnp_set_ethtool_hw_ops_n500(struct net_device *netdev)
{
#ifndef ETHTOOL_OPS_COMPAT
	netdev->ethtool_ops = &rnp500_ethtool_ops;
#else
	SET_ETHTOOL_OPS(netdev, &rnp500_ethtool_ops);
#endif


}





// only choose eth or mac func
static struct rnp_hw_operations hw_ops_n500 = {
	.init_hw = &rnp_init_hw_ops_n500,
	.reset_hw = &rnp_reset_hw_ops_n500,
	.start_hw = &rnp_start_hw_ops_n500,

	.set_mtu = &rnp_set_mtu_hw_ops_n500,
	.set_vlan_filter_en = &rnp_set_vlan_filter_en_hw_ops_n500,
	.set_vlan_filter = &rnp_set_vlan_filter_hw_ops_n500,
	.set_veb_vlan_mask = &rnp_set_veb_vlan_mask_hw_ops_n500,
	.set_vf_vlan_filter = &rnp_set_vf_vlan_filter_hw_ops_n500,
	.set_vlan_strip = &rnp_set_vlan_strip_hw_ops_n500,
	.set_mac = &rnp_set_mac_hw_ops_n500,
	//.set_virtualization = &rnp_set_virtualization_n500, 
	.set_rx_mode = &rnp_set_rx_mode_hw_ops_n500,
	// only update rar ,no veb
	.set_rar_with_vf = &rnp_set_rar_with_vf_hw_ops_n500, 
	.clr_rar = &rnp_clr_rar_hw_ops_n500,
	.clr_rar_all = &rnp_clr_rar_all_hw_ops_n500,
	.clr_vlan_veb = &rnp_clr_vlan_veb_hw_ops_n500,
	.set_txvlan_mode = &rnp_set_txvlan_mode_hw_ops_n500,
	.set_fcs_mode = &rnp_set_fcs_mode_hw_ops_n500,
	.set_vxlan_port = &rnp_set_vxlan_port_hw_ops_n500,
	.set_vxlan_mode = &rnp_set_vxlan_mode_hw_ops_n500,
	.set_mac_rx = &rnp_set_mac_rx_hw_ops_n500,
	.set_mac_speed = &rnp_set_mac_speed_hw_ops_n500,
	.set_rx_hash = &rnp_set_rx_hash_hw_ops_n500,
	.set_pause_mode = &rnp_set_pause_mode_hw_ops_n500,
	.get_pause_mode = &rnp_get_pause_mode_hw_ops_n500,
	.update_hw_info = &rnp_update_hw_info_hw_ops_n500,
	.update_rx_drop = &rnp_update_hw_rx_drop_hw_ops_n500,

	.update_sriov_info = &rnp_update_sriov_info_hw_ops_n500,
	// sriov relative
	.set_sriov_status = &rnp_set_sriov_status_hw_ops_n500,
	//.set_sriov_vf_mac = &rnp_set_sriov_vf_mac_hw_ops_n500,
	.set_sriov_vf_mc = &rnp_set_sriov_vf_mc_hw_ops_n500,
	//.set_sriov_vf_vlan = &rnp_set_sriov_vf_vlan_hw_ops_n500,

	.init_rx_addrs = &rnp_init_rx_addrs_hw_ops_n500,
	.clr_vfta = &rnp_clr_vfta_hw_ops_n500,

	.set_rss_hfunc = &rnp_set_rss_hfunc_hw_ops_n500,
	.set_rss_key = &rnp_set_rss_key_hw_ops_n500,
	.set_rss_table = &rnp_set_rss_table_hw_ops_n500,
	
	.update_hw_status = &rnp_update_hw_status_hw_ops_n500,

	// mbx add here?
	.set_mbx_link_event = &rnp_set_mbx_link_event_hw_ops_n500,
	.set_mbx_ifup = &rnp_set_mbx_ifup_hw_ops_n500,
	// mbx write or read ?
	
	.check_link = &rnp_check_mac_link_hw_ops_n500,
	.setup_link = &rnp_setup_mac_link_hw_ops_n500,
	.clean_link = &rnp_clean_link_hw_ops_n500,
	.get_link_capabilities = &rnp_get_link_capabilities_hw_ops_n500,

	.set_layer2_remapping = &rnp_set_layer2_hw_ops_n500,
	.clr_layer2_remapping = &rnp_clr_layer2_hw_ops_n500,
	.clr_all_layer2_remapping = &rnp_clr_all_layer2_hw_ops_n500,
	.set_tuple5_remapping = &rnp_set_tuple5_hw_ops_n500,
	.clr_tuple5_remapping = &rnp_clr_tuple5_hw_ops_n500,
	.clr_all_tuple5_remapping = &rnp_clr_all_tuple5_hw_ops_n500,
	.set_tcp_sync_remapping = &rnp_set_tcp_sync_hw_ops_n500,
	.set_rx_skip = &rnp_set_rx_skip_hw_ops_n500,

	.set_outer_vlan_type = &rnp_set_outer_vlan_type_hw_ops_n500,
	// ethtool callback 
	.setup_ethtool = &rnp_set_ethtool_hw_ops_n500,
	// todo 
	//.set_rx_status = &rnp_rx_status_hw_ops_n500;
	// setup mbx?
	.phy_read_reg = &rnp_phy_read_reg_hw_ops_n500,
	.phy_write_reg = &rnp_phy_write_reg_hw_ops_n500,

	.setup_wol = &rnp_setup_wol_hw_ops_n500,
	.set_vf_vlan_mode = &rnp_set_vf_vlan_mode_hw_ops_n500,

	.driver_status = &rnp_driver_status_hw_ops_n500,
};

static void rnp_mac_set_rx_n500(struct rnp_mac_info *mac, bool status)
{

	// contro tx and rx together? 
        u32 value = mac_rd32(mac, GMAC_CONTROL);


	// set mac in promisc mode temp 

        if (status) {
                value |= GMAC_CONTROL_TE | GMAC_CONTROL_RE;
	} else {
		// never control tx ?
                //value &= ~(GMAC_CONTROL_TE | GMAC_CONTROL_RE);
                value &= ~(GMAC_CONTROL_RE);
	}

	mac_wr32(mac, GMAC_CONTROL, value);
	
	// set all receive ? 
	//mac_wr32(mac, GMAC_FRAME_FILTER, 0x80000001);
	mac_wr32(mac, GMAC_FRAME_FILTER, 0x00000001);
}

static void rnp_mac_set_speed_n500(struct rnp_mac_info *mac, bool link, u32 speed, bool duplex)
{
#define SPEED_MASK (RNP_DM_MASK | RNP_FES_MASK | RNP_PS_MASK | RNP_LUD_MASK)
        u32 value = mac_rd32(mac, GMAC_CONTROL);

	value &= (~SPEED_MASK);

	if (link) 
		value |= RNP_LUD_MASK;

	if (duplex)
		value |= RNP_DM_MASK;

	/*
	switch (speed) {
		case RNP_LINK_SPEED_100_FULL:
			value |= RNP_PS_MASK;
			value |= RNP_FES_MASK;
		break;
		case RNP_LINK_SPEED_10_FULL:
			value |= RNP_PS_MASK;
		break;
	} */

	mac_wr32(mac, GMAC_CONTROL, value);
}

static void rnp_mac_fcs_n500(struct rnp_mac_info *mac, bool status)
{

#define RNP500_CST_MASK BIT(25)
	u32 value = mac_rd32(mac, GMAC_CONTROL);

	if (status)
		value &= (~RNP500_CST_MASK);
	else
		value |= (RNP500_CST_MASK);
	mac_wr32(mac, GMAC_CONTROL, value);
}
/**
 *  rnp_fc_mode_n500 - Enable flow control
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to the current settings.
 **/
s32 rnp_mac_fc_mode_n500(struct rnp_mac_info *mac)
{

	struct rnp_hw *hw = (struct rnp_hw *)mac->back;
	s32 ret_val = 0;
	unsigned int flow = GMAC_FLOW_CTRL_UP;

	flow = mac_rd32(mac, GMAC_FLOW_CTRL);

	// only use up status
	flow &= GMAC_FLOW_CTRL_UP;
	
	//hw->fc.current_mode = hw->fc.requested_mode;
	/*
	 * Validate the water mark configuration for packet buffer 0.  Zero
	 * water marks indicate that the packet buffer was not configured
	 * and the watermarks for packet buffer 0 should always be configured.
	 */
	if (!hw->fc.pause_time) {
		ret_val = RNP_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}



	switch (hw->fc.current_mode) {
	case rnp_fc_none:
		/*
		 * Flow control is disabled by software override or autoneg.
		 * The code below will actually disable it in the HW.
		 */
		break;
	case rnp_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
		flow |= GMAC_FLOW_CTRL_RFE;
		break;
	case rnp_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		flow |= GMAC_FLOW_CTRL_TFE;
		break;
	case rnp_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		flow |= GMAC_FLOW_CTRL_RFE;
		flow |= GMAC_FLOW_CTRL_TFE;
		break;
	default:
		hw_dbg(hw, "Flow control param set incorrectly\n");
		ret_val = RNP_ERR_CONFIG;
		goto out;
	}

	flow |= (hw->fc.pause_time << GMAC_FLOW_CTRL_PT_SHIFT);

	mac_wr32(mac, GMAC_FLOW_CTRL, flow);

out:
	return ret_val;
}

bool poll_free_mdio(u8 __iomem *addr, u32 mask, int count)
{
        unsigned int value;
        int con = 0;

        do {
                //value = read_reg(addr);
		value =  rnp_rd_reg(addr);
		usleep_range(10, 100);
                con++;
                //printf("%x value %x\n", addr, value);
        } while ((value & mask) && (con < count));

        return !!(con >= count);
}

int rnp_mdio_read(struct rnp_mac_info *mac, int phyreg)
{
#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002
#define MII_DATA_MASK GENMASK(15, 0)

        unsigned int mii_address = mac->mii.addr;
        unsigned int mii_data = mac->mii.data;
        u32 value = MII_BUSY;
        int data = 0;
        int phyaddr = mac->phy_addr;

        value |= (phyaddr << mac->mii.addr_shift)
                & mac->mii.addr_mask;
        value |= (phyreg << mac->mii.reg_shift) & mac->mii.reg_mask;
        value |= (mac->clk_csr << mac->mii.clk_csr_shift)
                & mac->mii.clk_csr_mask;

	if (poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100)) {

                return -EBUSY;
	}
        //if (readl_poll_timeout(mac->mac_addr + mii_address, v, !(v & MII_BUSY),
         //                      100, 10000))

	mac_wr32(mac, mii_data, data);
	mac_wr32(mac, mii_address, value);
        //writel(data, mac->base_addr + mii_data);
        //writel(value, mac->base_addr + mii_address);

	if (poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100)) {
        //if (readl_poll_timeout(mac->mac_addr + mii_address, v, !(v & MII_BUSY),
         //                      100, 10000))
                return -EBUSY;

	}
        /* Read the data from the MII data register */
        //data = (int)readl(mac->base_addr + mii_data) & MII_DATA_MASK;
        data = (int)mac_rd32(mac, mii_data) & MII_DATA_MASK;

        return data;
}

void rnp_mac_check_link_n500(struct rnp_mac_info *mac, rnp_link_speed *speed,
                               bool *link_up, bool link_up_wait_to_complete)
{
        struct rnp_hw *hw = (struct rnp_hw *)mac->back;
        /* always assume link is up, if no check link function */
        // get status from phy
        /* used to simulate link down */
        u32 data;
#ifdef CONFIG_RNP_FPGA
#define LINK_IS_UP (0x04)
#define TEST_PHY (LINK_IS_UP)
#else
#define AUTONEGOTATION_COMPLETE (0x20)
#define LINK_IS_UP (0x04)
#define TEST_PHY (AUTONEGOTATION_COMPLETE | LINK_IS_UP)
#endif

        data = rnp_mdio_read(mac, 1);
        //printk("phy 1 is %x\n", data);
        if ((data & TEST_PHY) == TEST_PHY) {
                data = rnp_mdio_read(mac, 0);
                //printk("phy 0 is %x\n", data);
#define DUPLEX_MODE (0x100)
                if (data & DUPLEX_MODE) {
                        if (data & 0x40) {
                                *speed = RNP_LINK_SPEED_1GB_FULL;
				hw->speed = SPEED_1000;
                        } else if (data & 0x2000) {
                                *speed = RNP_LINK_SPEED_100_FULL;
				hw->speed = SPEED_100;
                        } else {
                                *speed = RNP_LINK_SPEED_10_FULL;
				hw->speed = SPEED_10;
			}
                } else {
                        if (data & 0x40) {
                                *speed = RNP_LINK_SPEED_1GB_HALF;
				hw->speed = SPEED_1000;
                        } else if (data & 0x2000) {
                                *speed = RNP_LINK_SPEED_100_HALF;
				hw->speed = SPEED_100;
                        } else {
                                *speed = RNP_LINK_SPEED_10_HALF;
				hw->speed = SPEED_10;
			}
                }
                *link_up = true;
		hw->link = true;
                printk("link up done\n");

        } else {
                printk("not link up\n");
                *link_up = false;
		hw->link = false;
                *speed = RNP_LINK_SPEED_UNKNOWN;
        }
}



void rnp_mac_set_mac_n500(struct rnp_mac_info *mac, u8 *addr, int index)
{
	u32 rar_low, rar_high = 0;
	rar_low = ((u32)addr[0] |
			((u32)addr[1] << 8) |
			((u32)addr[2] << 16) |
			((u32)addr[3] << 24));

	rar_high = RNP_RAH_AV | ((u32)addr[4] | (u32)addr[5] << 8);

	mac_wr32(mac, RNP500_MAC_UNICAST_HIGH(index), rar_high);
	mac_wr32(mac, RNP500_MAC_UNICAST_LOW(index), rar_low);



}

int rnp_mac_mdio_read_n500(struct rnp_mac_info *mac, int phyreg, int *regvalue)
{

	unsigned int mii_address = mac->mii.addr;
	unsigned int mii_data = mac->mii.data;
	u32 value = MII_BUSY;
	int data = 0;
	// we use own default phy_addr
	int phyaddr = mac->phy_addr;


        value |= (phyaddr << mac->mii.addr_shift)
                & mac->mii.addr_mask;
        value |= (phyreg << mac->mii.reg_shift) & mac->mii.reg_mask;
        value |= (mac->clk_csr << mac->mii.clk_csr_shift)
                & mac->mii.clk_csr_mask;

	if (poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100)) {

                return -EBUSY;
	}
        //if (readl_poll_timeout(mac->mac_addr + mii_address, v, !(v & MII_BUSY),
         //                      100, 10000))

	mac_wr32(mac, mii_data, data);
	mac_wr32(mac, mii_address, value);
        //writel(data, mac->base_addr + mii_data);
        //writel(value, mac->base_addr + mii_address);

	if (poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100)) {
        //if (readl_poll_timeout(mac->mac_addr + mii_address, v, !(v & MII_BUSY),
         //                      100, 10000))
                return -EBUSY;

	}
        /* Read the data from the MII data register */
        //data = (int)readl(mac->base_addr + mii_data) & MII_DATA_MASK;
        data = (int)mac_rd32(mac, mii_data) & MII_DATA_MASK;

	*regvalue = data;

        return data;

}

int rnp_mac_mdio_write_n500(struct rnp_mac_info *mac, int phyreg, int phydata)
{
	unsigned int mii_address = mac->mii.addr;
	unsigned int mii_data = mac->mii.data;
	u32 value = MII_BUSY;
	int data = phydata;
	int phyaddr = mac->phy_addr;

	value |= (phyaddr << mac->mii.addr_shift)
		& mac->mii.addr_mask;
	value |= (phyreg << mac->mii.reg_shift) & mac->mii.reg_mask;

	value |= (mac->clk_csr << mac->mii.clk_csr_shift)
		& mac->mii.clk_csr_mask;
	value |= MII_WRITE;

	/* Wait until any existing MII operation is complete */
	//if (readl_poll_timeout(mac->base_addr + mii_address, v, !(v & MII_BUSY),
	//			100, 10000))
	if (poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100)) {
		return -EBUSY;
	}
	/* Set the MII address register to write */
	//printk("mdio write %x--> %x\n", data, mii_data);
	//printk("mdio write %x--> %x\n", value, mii_address);
	//writel(data, mac->base_addr + mii_data);
	//writel(value, mac->base_addr + mii_address);
	mac_wr32(mac, mii_data, data);
	mac_wr32(mac, mii_address, value);

	/* Wait until any existing MII operation is complete */
	return poll_free_mdio(mac->mac_addr + mii_address, MII_BUSY, 100);
	//return readl_poll_timeout(mac->base_addr + mii_address, v, !(v & MII_BUSY),
	//		100, 10000);


}

void rnp_mac_pmt_n500(struct rnp_mac_info *mac, u32 mode)
{
	unsigned int pmt = 0;

	if (mode & RNP_WUFC_MAG) {
		rnp_dbg("GMAC: WOL Magic frame\n");
		pmt |= power_down | magic_pkt_en;
	}
	if (mode & RNP_WUFC_EX) {
		rnp_dbg("GMAC: WOL on global unicast\n");
		pmt |= power_down | global_unicast | wake_up_frame_en;
	}

	mac_wr32(mac, GMAC_PMT, pmt);

}

static struct rnp_mac_operations mac_ops_n500 = {
	.set_mac_rx = &rnp_mac_set_rx_n500,
	.set_mac_speed = &rnp_mac_set_speed_n500,
	.set_mac_fcs = &rnp_mac_fcs_n500,
	.set_fc_mode = &rnp_mac_fc_mode_n500,
	.check_link = &rnp_mac_check_link_n500,
	.set_mac = &rnp_mac_set_mac_n500,
	.mdio_write = &rnp_mac_mdio_write_n500,
	.mdio_read = &rnp_mac_mdio_read_n500,
	.pmt = &rnp_mac_pmt_n500,

};

static s32 rnp_get_invariants_n500(struct rnp_hw *hw)
{
	struct rnp_mac_info *mac = &hw->mac;
	struct rnp_dma_info *dma = &hw->dma;
	struct rnp_eth_info *eth = &hw->eth;
	struct rnp_nic_info *nic = &hw->nic;
	struct rnp_mbx_info *mbx = &hw->mbx;
	struct rnp_adapter *adapter = (struct rnp_adapter *)hw->back;
	int i;


	nic->nic_base_addr = hw->hw_addr + RNP500_NIC_BASE;
	/* setup dma info */
	dma->dma_base_addr = hw->hw_addr;
	dma->dma_ring_addr = hw->hw_addr + RNP500_RING_BASE;
	dma->max_tx_queues = RNP_N500_MAX_TX_QUEUES;
	dma->max_rx_queues = RNP_N500_MAX_RX_QUEUES;
	dma->back = hw;
	memcpy(&hw->dma.ops, &dma_ops_n500, sizeof(hw->dma.ops));

	/* setup eth info */
	memcpy(&hw->eth.ops, &eth_ops_n500, sizeof(hw->eth.ops));

	eth->eth_base_addr = hw->hw_addr + RNP500_ETH_BASE;
	eth->back = hw;
	eth->mc_filter_type = 4;
	eth->mcft_size = RNP_N500_MC_TBL_SIZE;
	eth->vft_size = RNP_N500_VFT_TBL_SIZE;
	eth->num_rar_entries = RNP_N500_RAR_ENTRIES;
	eth->max_rx_queues = RNP_N500_MAX_RX_QUEUES;
	eth->max_tx_queues = RNP_N500_MAX_TX_QUEUES;

	/* setup mac info */
	memcpy(&hw->mac.ops, &mac_ops_n500, sizeof(hw->mac.ops));
	mac->mac_addr = hw->hw_addr + RNP500_MAC_BASE;
	mac->back = hw;
	mac->mac_type = mac_dwc_g;
	/* move this to eth todo */
	mac->mc_filter_type = 4;
	mac->mcft_size = 2;
	mac->vft_size = 1;
	mac->num_rar_entries = 32;
	mac->max_rx_queues = RNP_N500_MAX_RX_QUEUES;
	mac->max_tx_queues = RNP_N500_MAX_TX_QUEUES;
	//mac->max_msix_vectors = rnp_get_pcie_msix_count_generic(hw);
	mac->max_msix_vectors = RNP_N500_MSIX_VECTORS;
	// init mii 
        mac->mii.addr = GMAC_MII_ADDR;
        mac->mii.data = GMAC_MII_DATA;
        mac->mii.addr_shift = 11;
        mac->mii.addr_mask = 0x0000F800;
        mac->mii.reg_shift = 6;
        mac->mii.reg_mask = 0x000007C0;
        mac->mii.clk_csr_shift = 2;
        mac->mii.clk_csr_mask = GENMASK(5, 2);
        mac->clk_csr = 0x02; /* csr 25M */

	mac->phy_addr = 0x11;

	if (!hw->axi_mhz)
		hw->usecstocount = 62;
	else 
		hw->usecstocount = hw->axi_mhz;


	//set up hw feature
	hw->feature_flags |= RNP_NET_FEATURE_SG
			  | RNP_NET_FEATURE_TX_CHECKSUM
			  | RNP_NET_FEATURE_RX_CHECKSUM
			  | RNP_NET_FEATURE_TSO
			  | RNP_NET_FEATURE_VLAN_FILTER
			  | RNP_NET_FEATURE_VLAN_OFFLOAD
			  | RNP_NET_FEATURE_RX_NTUPLE_FILTER
			  | RNP_NET_FEATURE_RX_HASH
			  | RNP_NET_FEATURE_USO 
			  | RNP_NET_FEATURE_RX_FCS
			  | RNP_NET_FEATURE_STAG_FILTER 
			  | RNP_NET_FEATURE_STAG_OFFLOAD;
			  /* maybe supported future*/
			 // | RNP_NET_FEATURE_HW_TC
	
	/* setup some fdir resource */
	hw->min_length = RNP_MIN_MTU;
	hw->max_length = RNP500_MAX_JUMBO_FRAME_SIZE;
	hw->max_msix_vectors = RNP_N500_MSIX_VECTORS;

	hw->num_rar_entries = RNP_N500_RAR_ENTRIES;
	hw->fdir_mode = fdir_mode_tuple5; 
	hw->max_vfs = RNP_N500_MAX_VF;
	hw->max_vfs_noari = 1;
	hw->layer2_count = RNP500_MAX_LAYER2_FILTERS - 1;
	hw->tuple5_count = RNP500_MAX_TUPLE5_FILTERS - 1;

	// n500 support magic wol
	hw->wol_supported = WAKE_MAGIC; 

	// not so good
	//hw->feature_flags |= RNP_VEB_VLAN_MASK_EN;
	hw->num_vebvlan_entries = 8;

	hw->default_rx_queue = 0;
	// todo 
	hw->rss_indir_tbl_num = RNP_N500_RSS_TBL_NUM;
	hw->rss_tc_tbl_num = RNP_N500_RSS_TC_TBL_NUM;
	/* vf use the last vfnum */
	hw->vfnum = RNP_N500_MAX_VF - 1;

	hw->sriov_ring_limit = 1;
	hw->max_pf_macvlans = RNP_MAX_PF_MACVLANS_N500; 

	hw->veb_ring = RNP_N500_MAX_RX_QUEUES - 1;

	memcpy(&hw->ops, &hw_ops_n500, sizeof(hw->ops));
	//sriov num

	/* PHY */
	memcpy(&hw->phy.ops, &phy_ops_n500, sizeof(hw->phy.ops));
	/* setup pcs */
	memcpy(&hw->pcs.ops, &pcs_ops_generic, sizeof(hw->pcs.ops));

	//setup defulat link status
	hw->supported_link = RNP_LINK_SPEED_1GB_FULL; 
	// hw->is_sgmii = 1;
	//hw->enable_change_speed = 1;
	mbx->mbx_feature |= MBX_FEATURE_NO_ZERO;

	//mbx setup
	mbx->vf2pf_mbox_vec_base = 0x28900;
	mbx->cpu2pf_mbox_vec = 0x28b00;
	mbx->pf_vf_shm_base = 0x29000;
	mbx->mbx_mem_size = 64;
	mbx->pf2vf_mbox_ctrl_base = 0x2a100;
	mbx->pf_vf_mbox_mask_lo = 0x2a200;
	mbx->pf_vf_mbox_mask_hi = 0;

	mbx->cpu_pf_shm_base = 0x2d000;
	mbx->pf2cpu_mbox_ctrl = 0x2e000;
	mbx->pf2cpu_mbox_mask = 0x2e200;

	mbx->cpu_vf_share_ram = 0x2b000;
	mbx->share_size = 512;

	//priv
	adapter->priv_flags |= RNP_PRIV_FLAG_PAUSE_OWN;
	adapter->drop_time = 100;

	/*initialization default pause flow */
	// we start from auto
	hw->fc.requested_mode = PAUSE_AUTO;
	hw->fc.pause_time = RNP_DEFAULT_FCPAUSE;
	//hw->fc.current_mode = rnp_fc_full;
	hw->autoneg = 1;

	// we start from auto mode
#ifdef ETH_TP_MDI_AUTO
	hw->tp_mdix_ctrl = ETH_TP_MDI_AUTO;
#endif
	for (i = 0; i < RNP_MAX_TRAFFIC_CLASS; i++) {
		hw->fc.high_water[i] = RNP500_DEFAULT_HIGH_WATER;
		hw->fc.low_water[i] = RNP500_DEFAULT_LOW_WATER;
	}
	// eeprom
	hw->eeprom.word_size = 10;

	return 0;
}

struct rnp_info rnp_n500_info = {
	.one_pf_with_two_dma = false,
	.total_queue_pair_cnts = RNP_N500_MAX_TX_QUEUES,
	.adapter_cnt = 1,
	.rss_type = rnp_rss_n500,
	.hw_type = rnp_hw_n500,
	.get_invariants = &rnp_get_invariants_n500,
	.mac_ops = &mac_ops_n500,
	.eeprom_ops = NULL,
	.phy_ops = &phy_ops_n500,
	.mbx_ops = &mbx_ops_generic,
	.pcs_ops = &pcs_ops_generic, 
};
