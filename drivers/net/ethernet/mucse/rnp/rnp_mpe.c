#include <linux/types.h>
#include <linux/module.h>
#include <linux/firmware.h>

#include "rnp_common.h"
#include "rnp_mbx.h"
#include "rnp_mpe.h"

extern unsigned int mpe_src_port;
extern unsigned int mpe_pkt_version;

#define CFG_RPU_OFFSET 0x100000 // 4010_0000 broadcast addr

// RV_CORE_STATUS: 4000_6000
#define RV_CORE0_WORING_REG 0x6000
#define RPU_ID				0x6060 // read-only rpu id

// RPU_REG
#define RV_BROADCASE_START_REG (0x106000) // broadcast to 0x400X_6000
#define RPU_DMA_START_REG	   (0x110000)
#define RPU_ENDIAN_REG (0x110010)

// MPE0_ICCM:	4020_0000H
#define CFG_MPE_ICCM(nr) (0x200000 + (nr)*0x80000)
#define CFG_MPE_DCCM(nr) (0x220000 + (nr)*0x80000)

#define RPU_CM3_BASE 0x40000000 // 0x4000_0000

#define iowrite32_arrary(rpubase, offset, array, size)                      \
	do {                                                                    \
		int i;                                                              \
		for (i = 0; i < size; i++) {                                        \
			rnp_wr_reg(((char *)(rpubase)) + (offset) + i * 4, (array)[i]); \
		}                                                                   \
	} while (0)

#define cm3_reg_write32(hw, cm3_rpu_reg, v) \
	rnp_mbx_reg_write((hw), (cm3_rpu_reg), (v))

#define cm3_reg_read32(hw, cm3_rpu_reg) rnp_mbx_fw_reg_read((hw), (cm3_rpu_reg))

void reset_rpu(struct rnp_hw *hw)
{
#define SYSCTL_CRG_CTRL12 0x30007030
#define RPU_RESET_BIT	  9

	// reset rpu/mpe/pub
	cm3_reg_write32(hw, SYSCTL_CRG_CTRL12, BIT(RPU_RESET_BIT + 16) | 0);
	smp_mb();
	mdelay(100);

	cm3_reg_write32(
		hw, SYSCTL_CRG_CTRL12, BIT(RPU_RESET_BIT + 16) | BIT(RPU_RESET_BIT));

	mdelay(10);
}

/*
	@rpu_base: mapped(0x4000_0000)
*/
int download_and_start_rpu(struct rnp_hw *hw,
						   char *rpu_base,
						   const unsigned int *mpe_bin,
						   const int mpe_bin_sz,
						   const unsigned int *mpe_data,
						   const int mpe_data_sz,
						   const unsigned int *rpu_bin,
						   const int rpu_sz)
{
	int nr = 0;

	reset_rpu(hw);

	printk("download rpu fw:%d mpe:%d mpe-data:%d\n", rpu_sz, mpe_bin_sz, mpe_data_sz);

	// download rpu firmeware
	iowrite32_arrary(rpu_base, CFG_RPU_OFFSET + 0x4000, rpu_bin, rpu_sz / 4);

	// download firmware to 4 mpe-core: mpe0,mpe1,mpe2,mpe3
	for (nr = 0; nr < 4; nr++) {
		iowrite32_arrary(rpu_base, CFG_MPE_ICCM(nr), mpe_bin, mpe_bin_sz / 4);
		iowrite32_arrary(rpu_base, CFG_MPE_DCCM(nr), mpe_data, mpe_data_sz / 4);
	}

	if (mpe_src_port != 0) {
		printk("%s %d\n", __func__, __LINE__);
		rnp_wr_reg(rpu_base + 0x100000, mpe_pkt_version);
		rnp_wr_reg(rpu_base + 0x100004, mpe_src_port);
	}

	rnp_wr_reg(rpu_base + 0x198700, 0xf);
	rnp_wr_reg(rpu_base + RPU_ENDIAN_REG, 0xf);
	smp_mb();
	// start all rv-core
	rnp_wr_reg(rpu_base + RV_BROADCASE_START_REG, 0x1);
	// start rpu
	rnp_wr_reg(rpu_base + RPU_DMA_START_REG, 0x1);

	return 0;
}

/*
	load fw bin from: /lib/firmware/ directory
*/
const struct firmware *load_fw(struct device *dev, const char *fw_name)
{
	const struct firmware *fw;
	int rc;

	rc = request_firmware(&fw, fw_name, dev);
	if (rc != 0) {
		dev_err(dev, "cannot %d requesting firmware file: %s\n", rc, fw_name);
		return NULL;
	}

	return fw;
}

#define MPE_FW_BIN	"n10/n10-mpe.bin"
#define MPE_FW_DATA "n10/n10-mpe-data.bin"
#define MPE_RPU_BIN "n10/n10-rpu.bin"

int rpu_mpe_start(struct rnp_adapter *adapter)
{
	const struct firmware *mpe_bin = NULL, *mpe_data = NULL, *rpu_bin = NULL;
	struct rnp_hw *hw = &adapter->hw;
	int rpu_version, err = 0;

	rpu_version = cm3_reg_read32(hw, RPU_CM3_BASE + RPU_ID);
	dev_info(&adapter->pdev->dev, "rpu_version:0x%x\n", rpu_version);

	if (rpu_version != 0x20201125) {
		dev_info(&adapter->pdev->dev, "rpu not enabled! quit\n");
		return -1;
	}

	dev_info(&adapter->pdev->dev, "rpu_addr=%p\n", hw->rpu_addr);
	if (hw->rpu_addr == NULL) {
		return -EINVAL;
	}

	mpe_bin = load_fw(&adapter->pdev->dev, MPE_FW_BIN);
	if (!mpe_bin) {
		goto quit;
	}
	mpe_data = load_fw(&adapter->pdev->dev, MPE_FW_DATA);
	if (!mpe_data) {
		goto quit;
	}
	rpu_bin = load_fw(&adapter->pdev->dev, MPE_RPU_BIN);
	if (!mpe_data) {
		goto quit;
	}

	err = download_and_start_rpu(hw,
								 hw->rpu_addr,
								 (unsigned int *)mpe_bin->data,
								 mpe_bin->size,
								 (unsigned int *)mpe_data->data,
								 mpe_data->size,
								 (unsigned int *)rpu_bin->data,
								 rpu_bin->size);
	adapter->rpu_inited = 1;

quit:
	if (rpu_bin) {
		release_firmware(rpu_bin);
	}
	if (mpe_data)
		release_firmware(mpe_data);
	if (mpe_bin)
		release_firmware(mpe_bin);
	return 0;
}

void rpu_mpe_stop(struct rnp_adapter *adapter)
{
	adapter->rpu_inited = 0;
	// reset_rpu(&adapter->hw);
}
