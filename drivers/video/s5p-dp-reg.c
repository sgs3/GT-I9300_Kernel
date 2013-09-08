/*
 * Samsung DP (Display port) register interface driver.
 *
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <video/s5p-dp.h>

#include <plat/cpu.h>

#include "s5p-dp-core.h"
#include "s5p-dp-reg.h"

#define COMMON_INT_MASK_1 (0)
#define COMMON_INT_MASK_2 (0)
#define COMMON_INT_MASK_3 (0)
#define COMMON_INT_MASK_4 (0)
#define INT_STA_MASK (0)

void s5p_dp_enable_video_mute(struct s5p_dp_device *dp, bool enable)
{
	u32 reg;

	if (enable) {
		reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_1);
		reg |= HDCP_VIDEO_MUTE;
		writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_1);
	} else {
		reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_1);
		reg &= ~HDCP_VIDEO_MUTE;
		writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_1);
	}
}

void s5p_dp_stop_video(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_1);
	reg &= ~VIDEO_EN;
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_1);
}

void s5p_dp_lane_swap(struct s5p_dp_device *dp, bool enable)
{
	u32 reg;

	if (soc_is_exynos5250()) {
		if (enable)
			reg = LANE3_MAP_LOGIC_LANE_0 | LANE2_MAP_LOGIC_LANE_1 |
				LANE1_MAP_LOGIC_LANE_2 | LANE0_MAP_LOGIC_LANE_3;
		else
			reg = LANE3_MAP_LOGIC_LANE_3 | LANE2_MAP_LOGIC_LANE_2 |
				LANE1_MAP_LOGIC_LANE_1 | LANE0_MAP_LOGIC_LANE_0;
	} else {
		if (enable)
			reg = LANE1_MAP_LOGIC_LANE_0 | LANE0_MAP_LOGIC_LANE_1;
		else
			reg = LANE1_MAP_LOGIC_LANE_1 | LANE0_MAP_LOGIC_LANE_0;
	}

	writel(reg, dp->reg_base + S5P_DP_LANE_MAP);
}

/*
 * FIXME: Mask this function to fix compile error,
 *        LSI patch will be released for this error.
 */
#if 0
void s5p_dp_init_analog_param(struct s5p_dp_device *dp)
{
	u32 reg;

	/*
	 * Set termination
	 * Normal bandgap, Normal swing, Tx terminal registor 61 ohm
	 * 24M Phy clock, TX digital logic power is 100:1.0625V
	 */
	reg = SEL_BG_NEW_BANDGAP | TX_TERMINAL_CTRL_61_OHM |
		SWING_A_30PER_G_NORMAL;
	writel(reg, dp->reg_base + S5P_DP_ANALOG_CTL_1);
	reg = SEL_24M | TX_DVDD_BIT_1_0625V;
	writel(reg, dp->reg_base + S5P_DP_ANALOG_CTL_2);

	/*
	 * Set power source for internal clk driver to 1.0625v.
	 * Select current reference of TX driver current to 00:Ipp/2+Ic/2.
	 * Set VCO range of PLL +- 0uA
	 */
	reg = DRIVE_DVDD_BIT_1_0625V | SEL_CURRENT_DEFAULT |
		VCO_BIT_000_MICRO;
	writel(reg, dp->reg_base + S5P_DP_ANALOG_CTL_3);

	/*
	 * Set AUX TX terminal resistor to 102 ohm
	 * Set AUX channel amplitude control
	*/
	reg = PD_RING_OSC | AUX_TERMINAL_CTRL_102_OHM |
		TX_CUR1_2X | TX_CUR_4_MA;
	writel(reg, dp->reg_base + S5P_DP_PLL_FILTER_CTL_1);

	if (soc_is_exynos5250()) {
		/* Output amplitude fine setting */
		reg = CH3_AMP_0_MV | CH2_AMP_0_MV |
			CH1_AMP_0_MV | CH0_AMP_0_MV;
		writel(reg, dp->reg_base + S5P_DP_PLL_FILTER_CTL_2);

		/*
		 * PLL loop filter bandwidth
		 * For 2.7Gbps: 175KHz, For 1.62Gbps: 234KHz
		 * PLL digital power select: 1.2500V
		 */
		reg = DP_PLL_LOOP_BIT_DEFAULT | DP_PLL_REF_BIT_1_2500V;
		writel(reg, dp->reg_base + S5P_DP_PLL_CTL);
	} else {
		/* Output amplitude fine setting */
		reg = CH1_AMP_0_MV | CH0_AMP_0_MV;
		writel(reg, dp->reg_base + S5P_DP_PLL_FILTER_CTL_2);

		/*
		 * PLL loop filter bandwidth
		 * For 2.7Gbps: 175KHz, For 1.62Gbps: 234KHz
		 * PLL digital power select: 1.1250V
		 */
		reg = DP_PLL_LOOP_BIT_DEFAULT | DP_PLL_REF_BIT_1_1250V;
		writel(reg, dp->reg_base + S5P_DP_PLL_CTL);
	}
}
#endif

void s5p_dp_init_analog_param(struct s5p_dp_device *dp)
{
	writel(0x10, dp->reg_base + S5P_DP_ANALOG_CTL_1);

	writel(0x0C, dp->reg_base + S5P_DP_ANALOG_CTL_2);

	writel(0x85, dp->reg_base + S5P_DP_ANALOG_CTL_3);

	writel(0x66, dp->reg_base + S5P_DP_PLL_FILTER_CTL_1);

	writel(0x0, dp->reg_base + S5P_DP_TX_AMP_TUNING_CTL);
}

void s5p_dp_init_interrupt(struct s5p_dp_device *dp)
{
	/* Set interrupt pin assertion polarity as high */
	writel(INT_POL, dp->reg_base + S5P_DP_INT_CTL);

	/* Clear pending regisers */
	writel(0xff, dp->reg_base + S5P_DP_COMMON_INT_STA_1);
	writel(0x4f, dp->reg_base + S5P_DP_COMMON_INT_STA_2);
	writel(0xe0, dp->reg_base + S5P_DP_COMMON_INT_STA_3);
	if (soc_is_exynos5250())
		writel(0xe7, dp->reg_base + S5P_DP_COMMON_INT_STA_4);
	else
		writel(0x27, dp->reg_base + S5P_DP_COMMON_INT_STA_4);
	writel(0x63, dp->reg_base + S5P_DP_INT_STA);

	/* 0:mask,1: unmask */
	writel(0x00, dp->reg_base + S5P_DP_COMMON_INT_MASK_1);
	writel(0x00, dp->reg_base + S5P_DP_COMMON_INT_MASK_2);
	writel(0x00, dp->reg_base + S5P_DP_COMMON_INT_MASK_3);
	writel(0x00, dp->reg_base + S5P_DP_COMMON_INT_MASK_4);
	writel(0x00, dp->reg_base + S5P_DP_INT_STA_MASK);
}

void s5p_dp_reset(struct s5p_dp_device *dp)
{
	u32 reg;

	writel(RESET_DP_TX, dp->reg_base + S5P_DP_TX_SW_RESET);

	s5p_dp_stop_video(dp);
	s5p_dp_enable_video_mute(dp, 0);

	reg = MASTER_VID_FUNC_EN_N | SLAVE_VID_FUNC_EN_N |
		AUD_FIFO_FUNC_EN_N | AUD_FUNC_EN_N |
		HDCP_FUNC_EN_N | SW_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_1);

	reg = SSC_FUNC_EN_N | AUX_FUNC_EN_N |
		SERDES_FIFO_FUNC_EN_N |
		LS_CLK_DOMAIN_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_2);

	udelay(20);

	s5p_dp_lane_swap(dp, 0);

	if (soc_is_exynos5250() && samsung_rev() < EXYNOS5250_REV_1_0)
		writel(0x75, dp->reg_base + S5P_DP_PLL_FILTER_CTL_1);

	writel(0x0, dp->reg_base + S5P_DP_SYS_CTL_1);
	writel(0x40, dp->reg_base + S5P_DP_SYS_CTL_2);
	writel(0x0, dp->reg_base + S5P_DP_SYS_CTL_3);
	writel(0x0, dp->reg_base + S5P_DP_SYS_CTL_4);

	writel(0x0, dp->reg_base + S5P_DP_PKT_SEND_CTL);
	writel(0x0, dp->reg_base + S5P_DP_HDCP_CTL);

	writel(0x5e, dp->reg_base + S5P_DP_HPD_DEGLITCH_L);
	writel(0x1a, dp->reg_base + S5P_DP_HPD_DEGLITCH_H);

	writel(0x10, dp->reg_base + S5P_DP_LINK_DEBUG_CTL);

	writel(0x0, dp->reg_base + S5P_DP_PHY_TEST);

	writel(0x0, dp->reg_base + S5P_DP_VIDEO_FIFO_THRD);
	writel(0x20, dp->reg_base + S5P_DP_AUDIO_MARGIN);

	writel(0x4, dp->reg_base + S5P_DP_M_VID_GEN_FILTER_TH);
	writel(0x2, dp->reg_base + S5P_DP_M_AUD_GEN_FILTER_TH);

	writel(0x00000101, dp->reg_base + S5P_DP_SOC_GENERAL_CTL);

	if (soc_is_exynos5250() && samsung_rev() >= EXYNOS5250_REV_1_0)
		s5p_dp_init_analog_param(dp);
	s5p_dp_init_interrupt(dp);
}

void s5p_dp_config_interrupt(struct s5p_dp_device *dp)
{
	u32 reg;

	/* 0: mask, 1: unmask */
	reg = COMMON_INT_MASK_1;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_MASK_1);

	reg = COMMON_INT_MASK_2;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_MASK_2);

	reg = COMMON_INT_MASK_3;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_MASK_3);

	reg = COMMON_INT_MASK_4;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_MASK_4);

	reg = INT_STA_MASK;
	writel(reg, dp->reg_base + S5P_DP_INT_STA_MASK);
}

u32 s5p_dp_get_pll_lock_status(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_DEBUG_CTL);
	if (reg & PLL_LOCK)
		return PLL_LOCKED;
	else
		return PLL_UNLOCKED;
}

void s5p_dp_set_pll_power_down(struct s5p_dp_device *dp, bool enable)
{
	u32 reg;

	if (enable) {
		reg = readl(dp->reg_base + S5P_DP_PLL_CTL);
		reg |= DP_PLL_PD;
		writel(reg, dp->reg_base + S5P_DP_PLL_CTL);
	} else {
		reg = readl(dp->reg_base + S5P_DP_PLL_CTL);
		reg &= ~DP_PLL_PD;
		writel(reg, dp->reg_base + S5P_DP_PLL_CTL);
	}
}

void s5p_dp_set_analog_power_down(struct s5p_dp_device *dp,
				enum analog_power_block block,
				bool enable)
{
	u32 reg;

	switch (block) {
	case AUX_BLOCK:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= AUX_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~AUX_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case CH0_BLOCK:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= CH0_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~CH0_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case CH1_BLOCK:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= CH1_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~CH1_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case CH2_BLOCK:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= CH2_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~CH2_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case CH3_BLOCK:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= CH3_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~CH3_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case ANALOG_TOTAL:
		if (enable) {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg |= DP_PHY_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			reg = readl(dp->reg_base + S5P_DP_PHY_PD);
			reg &= ~DP_PHY_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	case POWER_ALL:
		if (enable) {
			reg = DP_PHY_PD | AUX_PD | CH3_PD | CH2_PD |
				CH1_PD | CH0_PD;
			writel(reg, dp->reg_base + S5P_DP_PHY_PD);
		} else {
			writel(0x00, dp->reg_base + S5P_DP_PHY_PD);
		}
		break;
	default:
		break;
	}
}

void s5p_dp_init_analog_func(struct s5p_dp_device *dp)
{
	u32 reg;
	int timeout_loop = 0;

	s5p_dp_set_analog_power_down(dp, POWER_ALL, 0);

	reg = PLL_LOCK_CHG;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_STA_1);

	reg = readl(dp->reg_base + S5P_DP_DEBUG_CTL);
	reg &= ~(F_PLL_LOCK | PLL_LOCK_CTRL);
	writel(reg, dp->reg_base + S5P_DP_DEBUG_CTL);

	/* Power up PLL */
	if (s5p_dp_get_pll_lock_status(dp) == PLL_UNLOCKED) {
		s5p_dp_set_pll_power_down(dp, 0);

		while (s5p_dp_get_pll_lock_status(dp) == PLL_UNLOCKED) {
			timeout_loop++;
			if (DP_TIMEOUT_LOOP_COUNT < timeout_loop) {
				dev_err(dp->dev, "failed to get pll lock status\n");
				return;
			}
			udelay(10);
		}
	}

	/* Enable Serdes FIFO function and Link symbol clock domain module */
	reg = readl(dp->reg_base + S5P_DP_FUNC_EN_2);
	reg &= ~(SERDES_FIFO_FUNC_EN_N | LS_CLK_DOMAIN_FUNC_EN_N
		| AUX_FUNC_EN_N);
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_2);
}

void s5p_dp_init_hpd(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = HOTPLUG_CHG | HPD_LOST | PLUG;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_STA_4);

	reg = INT_HPD;
	writel(reg, dp->reg_base + S5P_DP_INT_STA);

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_3);
	reg &= ~(F_HPD | HPD_CTRL);
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_3);
}

void s5p_dp_reset_aux(struct s5p_dp_device *dp)
{
	u32 reg;

	/* Disable AUX channel module */
	reg = readl(dp->reg_base + S5P_DP_FUNC_EN_2);
	reg |= AUX_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_2);
}

void s5p_dp_init_aux(struct s5p_dp_device *dp)
{
	u32 reg;

	/* Clear inerrupts related to AUX channel */
	reg = RPLY_RECEIV | AUX_ERR;
	writel(reg, dp->reg_base + S5P_DP_INT_STA);

	s5p_dp_reset_aux(dp);

	/* Disable AUX transaction H/W retry */
	reg = AUX_BIT_PERIOD_EXPECTED_DELAY(3) | AUX_HW_RETRY_COUNT_SEL(0)|
		AUX_HW_RETRY_INTERVAL_600_MICROSECONDS;
	writel(reg, dp->reg_base + S5P_DP_AUX_HW_RETRY_CTL) ;

	/* Receive AUX Channel DEFER commands equal to DEFFER_COUNT*64 */
	reg = DEFER_CTRL_EN | DEFER_COUNT(1);
	writel(reg, dp->reg_base + S5P_DP_AUX_CH_DEFER_CTL);

	/* Enable AUX channel module */
	reg = readl(dp->reg_base + S5P_DP_FUNC_EN_2);
	reg &= ~AUX_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_2);
}

int s5p_dp_get_plug_in_status(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_3);
	if (reg & HPD_STATUS)
		return 0;

	return -EINVAL;
}

void s5p_dp_enable_sw_function(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_FUNC_EN_1);
	reg &= ~SW_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_1);
}

int s5p_dp_start_aux_transaction(struct s5p_dp_device *dp)
{
	int reg;
	int retval = 0;

	/* Enable AUX CH operation */
	reg = readl(dp->reg_base + S5P_DP_AUX_CH_CTL_2);
	reg |= AUX_EN;
	writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_2);

	/* Is AUX CH command reply received? */
	reg = readl(dp->reg_base + S5P_DP_INT_STA);
	while (!(reg & RPLY_RECEIV))
		reg = readl(dp->reg_base + S5P_DP_INT_STA);

	/* Clear interrupt source for AUX CH command reply */
	writel(RPLY_RECEIV, dp->reg_base + S5P_DP_INT_STA);

	/* Clear interrupt source for AUX CH access error */
	reg = readl(dp->reg_base + S5P_DP_INT_STA);
	if (reg & AUX_ERR) {
		dev_err(dp->dev, "AUX CH error happens reg : %x\n", reg);
		writel(AUX_ERR, dp->reg_base + S5P_DP_INT_STA);
		return -EREMOTEIO;
	}

	reg = readl(dp->reg_base + S5P_DP_INT_STA);
	dev_err(dp->dev, "INT_STA AUX Err Status Reg : %x\n", reg);

	/* Check AUX CH error access status */
	reg = readl(dp->reg_base + S5P_DP_AUX_CH_STA);
	if ((reg & AUX_STATUS_MASK) != 0) {
		dev_err(dp->dev, "AUX CH error happens: %d\n\n",
			reg & AUX_STATUS_MASK);
		return -EREMOTEIO;
	}

	return retval;
}

int s5p_dp_write_byte_to_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned char data)
{
	u32 reg;
	int i;
	int retval;

	for (i = 0; i < 3; i++) {
		/* Clear AUX CH data buffer */
		reg = BUF_CLR;
		writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

		/* Select DPCD device address */
		reg = AUX_ADDR_7_0(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_7_0);
		reg = AUX_ADDR_15_8(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_15_8);
		reg = AUX_ADDR_19_16(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_19_16);

		/* Write data buffer */
		reg = (unsigned int)data;
		writel(reg, dp->reg_base + S5P_DP_BUF_DATA_0);

		/*
		 * Set DisplayPort transaction and write 1 byte
		 * If bit 3 is 1, DisplayPort transaction.
		 * If Bit 3 is 0, I2C transaction.
		 */
		reg = AUX_TX_COMM_DP_TRANSACTION | AUX_TX_COMM_WRITE;
		writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

		/* Start AUX transaction */
		retval = s5p_dp_start_aux_transaction(dp);
		if (retval == 0)
			break;
		else
			dev_err(dp->dev, "Aux Transaction fail!\n");
	}

	return retval;
}

int s5p_dp_read_byte_from_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned char *data)
{
	u32 reg;
	int i;
	int retval;

	for (i = 0; i < 10; i++) {
		/* Clear AUX CH data buffer */
		reg = BUF_CLR;
		writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

		/* Select DPCD device address */
		reg = AUX_ADDR_7_0(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_7_0);
		reg = AUX_ADDR_15_8(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_15_8);
		reg = AUX_ADDR_19_16(reg_addr);
		writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_19_16);

		/*
		 * Set DisplayPort transaction and read 1 byte
		 * If bit 3 is 1, DisplayPort transaction.
		 * If Bit 3 is 0, I2C transaction.
		 */
		reg = AUX_TX_COMM_DP_TRANSACTION | AUX_TX_COMM_READ;
		writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

		/* Start AUX transaction */
		retval = s5p_dp_start_aux_transaction(dp);
		if (retval == 0)
			break;
		else
			dev_err(dp->dev, "Aux Transaction fail!\n");
	}

	/* Read data buffer */
	reg = readl(dp->reg_base + S5P_DP_BUF_DATA_0);
	*data = (unsigned char)(reg & 0xff);

	return retval;
}

int s5p_dp_write_bytes_to_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned int count,
				unsigned char data[])
{
	u32 reg;
	unsigned int start_offset;
	unsigned int cur_data_count;
	unsigned int cur_data_idx;
	int i;
	int retval = 0;

	/* Clear AUX CH data buffer */
	reg = BUF_CLR;
	writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

	start_offset = 0;
	while (start_offset < count) {
		/* Buffer size of AUX CH is 16 * 4bytes */
		if ((count - start_offset) > 16)
			cur_data_count = 16;
		else
			cur_data_count = count - start_offset;

		for (i = 0; i < 10; i++) {
			/* Select DPCD device address */
			reg = AUX_ADDR_7_0(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_7_0);
			reg = AUX_ADDR_15_8(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_15_8);
			reg = AUX_ADDR_19_16(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_19_16);

			for (cur_data_idx = 0; cur_data_idx < cur_data_count;
			     cur_data_idx++) {
				reg = data[start_offset + cur_data_idx];
				writel(reg, dp->reg_base + S5P_DP_BUF_DATA_0
							  + 4 * cur_data_idx);
			}

			/*
			 * Set DisplayPort transaction and write
			 * If bit 3 is 1, DisplayPort transaction.
			 * If Bit 3 is 0, I2C transaction.
			 */
			reg = AUX_LENGTH(cur_data_count) |
				AUX_TX_COMM_DP_TRANSACTION | AUX_TX_COMM_WRITE;
			writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

			/* Start AUX transaction */
			retval = s5p_dp_start_aux_transaction(dp);
			if (retval == 0)
				break;
			else
				dev_err(dp->dev, "Aux Transaction fail!\n");
		}

		start_offset += cur_data_count;
	}

	return retval;
}

int s5p_dp_read_bytes_from_dpcd(struct s5p_dp_device *dp,
				unsigned int reg_addr,
				unsigned int count,
				unsigned char data[])
{
	u32 reg;
	unsigned int start_offset;
	unsigned int cur_data_count;
	unsigned int cur_data_idx;
	int i;
	int retval = 0;

	/* Clear AUX CH data buffer */
	reg = BUF_CLR;
	writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

	start_offset = 0;
	while (start_offset < count) {
		/* Buffer size of AUX CH is 16 * 4bytes */
		if ((count - start_offset) > 16)
			cur_data_count = 16;
		else
			cur_data_count = count - start_offset;

		/* AUX CH Request Transaction process */
		for (i = 0; i < 10; i++) {
			/* Select DPCD device address */
			reg = AUX_ADDR_7_0(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_7_0);
			reg = AUX_ADDR_15_8(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_15_8);
			reg = AUX_ADDR_19_16(reg_addr + start_offset);
			writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_19_16);

			/*
			 * Set DisplayPort transaction and read
			 * If bit 3 is 1, DisplayPort transaction.
			 * If Bit 3 is 0, I2C transaction.
			 */
			reg = AUX_LENGTH(cur_data_count) |
				AUX_TX_COMM_DP_TRANSACTION | AUX_TX_COMM_READ;
			writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

			/* Start AUX transaction */
			retval = s5p_dp_start_aux_transaction(dp);
			if (retval == 0)
				break;
			else {
				dev_err(dp->dev, "Aux Transaction fail!\n");
				msleep(20);
			}
		}

		for (cur_data_idx = 0; cur_data_idx < cur_data_count;
		    cur_data_idx++) {
			reg = readl(dp->reg_base + S5P_DP_BUF_DATA_0
						 + 4 * cur_data_idx);
			data[start_offset + cur_data_idx] =
				(unsigned char)reg;
		}

		start_offset += cur_data_count;
	}

	return retval;
}

int s5p_dp_select_i2c_device(struct s5p_dp_device *dp,
				unsigned int device_addr,
				unsigned int reg_addr)
{
	u32 reg;
	int retval;

	/* Set EDID device address */
	reg = device_addr;
	writel(reg, dp->reg_base + S5P_DP_AUX_ADDR_7_0);
	writel(0x0, dp->reg_base + S5P_DP_AUX_ADDR_15_8);
	writel(0x0, dp->reg_base + S5P_DP_AUX_ADDR_19_16);

	/* Set offset from base address of EDID device */
	writel(reg_addr, dp->reg_base + S5P_DP_BUF_DATA_0);

	/*
	 * Set I2C transaction and write address
	 * If bit 3 is 1, DisplayPort transaction.
	 * If Bit 3 is 0, I2C transaction.
	 */
	reg = AUX_TX_COMM_I2C_TRANSACTION | AUX_TX_COMM_MOT |
		AUX_TX_COMM_WRITE;
	writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

	/* Start AUX transaction */
	retval = s5p_dp_start_aux_transaction(dp);
	if (retval != 0)
		dev_err(dp->dev, "Aux Transaction fail!\n");

	return retval;
}

int s5p_dp_read_byte_from_i2c(struct s5p_dp_device *dp,
				unsigned int device_addr,
				unsigned int reg_addr,
				unsigned int *data)
{
	u32 reg;
	int i;
	int retval;

	for (i = 0; i < 10; i++) {
		/* Clear AUX CH data buffer */
		reg = BUF_CLR;
		writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

		/* Select EDID device */
		retval = s5p_dp_select_i2c_device(dp, device_addr, reg_addr);
		if (retval != 0) {
			dev_err(dp->dev, "Select EDID device fail!\n");
			continue;
		}

		/*
		 * Set I2C transaction and read data
		 * If bit 3 is 1, DisplayPort transaction.
		 * If Bit 3 is 0, I2C transaction.
		 */
		reg = AUX_TX_COMM_I2C_TRANSACTION |
			AUX_TX_COMM_READ;
		writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

		/* Start AUX transaction */
		retval = s5p_dp_start_aux_transaction(dp);
		if (retval == 0)
			break;
		else
			dev_err(dp->dev, "Aux Transaction fail!\n");
	}

	/* Read data */
	if (retval == 0)
		*data = readl(dp->reg_base + S5P_DP_BUF_DATA_0);

	return retval;
}

int s5p_dp_read_bytes_from_i2c(struct s5p_dp_device *dp,
				unsigned int device_addr,
				unsigned int reg_addr,
				unsigned int count,
				unsigned char edid[])
{
	u32 reg;
	unsigned int i, j;
	unsigned int cur_data_idx;
	unsigned int defer = 0;
	int retval = 0;

	for (i = 0; i < count; i += 16) {
		for (j = 0; j < 100; j++) {
			/* Clear AUX CH data buffer */
			reg = BUF_CLR;
			writel(reg, dp->reg_base + S5P_DP_BUFFER_DATA_CTL);

			/* Set normal AUX CH command */
			reg = readl(dp->reg_base + S5P_DP_AUX_CH_CTL_2);
			reg &= ~ADDR_ONLY;
			writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_2);

			/*
			 * If Rx sends defer, Tx sends only reads
			 * request without sending addres
			 */
			if (!defer)
				retval = s5p_dp_select_i2c_device(dp,
						device_addr, reg_addr + i);
			else
				defer = 0;

			if (retval == 0) {
				/*
				 * Set I2C transaction and write data
				 * If bit 3 is 1, DisplayPort transaction.
				 * If Bit 3 is 0, I2C transaction.
				 */
				reg = AUX_LENGTH(16) |
					AUX_TX_COMM_I2C_TRANSACTION |
					AUX_TX_COMM_READ;
				writel(reg, dp->reg_base + S5P_DP_AUX_CH_CTL_1);

				/* Start AUX transaction */
				retval = s5p_dp_start_aux_transaction(dp);
				if (retval == 0)
					break;
				else
					dev_err(dp->dev, "Aux Transaction fail!\n");
			}
			/* Check if Rx sends defer */
			reg = readl(dp->reg_base + S5P_DP_AUX_RX_COMM);
			if (reg == AUX_RX_COMM_AUX_DEFER ||
				reg == AUX_RX_COMM_I2C_DEFER) {
				dev_err(dp->dev, "Defer: %d\n\n", reg);
				defer = 1;
			}
		}

		for (cur_data_idx = 0; cur_data_idx < 16; cur_data_idx++) {
			reg = readl(dp->reg_base + S5P_DP_BUF_DATA_0
						 + 4 * cur_data_idx);
			edid[i + cur_data_idx] = (unsigned char)reg;
		}
	}

	return retval;
}

void s5p_dp_set_link_bandwidth(struct s5p_dp_device *dp, u32 bwtype)
{
	u32 reg;

	reg = bwtype;
	if ((bwtype == LINK_RATE_2_70GBPS) || (bwtype == LINK_RATE_1_62GBPS))
		writel(reg, dp->reg_base + S5P_DP_LINK_BW_SET);
}

void s5p_dp_get_link_bandwidth(struct s5p_dp_device *dp, u32 *bwtype)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LINK_BW_SET);
	*bwtype = reg;
}

void s5p_dp_set_lane_count(struct s5p_dp_device *dp, u32 count)
{
	u32 reg;

	reg = count;
	writel(reg, dp->reg_base + S5P_DP_LANE_COUNT_SET);
}

void s5p_dp_get_lane_count(struct s5p_dp_device *dp, u32 *count)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LANE_COUNT_SET);
	*count = reg;
}

void s5p_dp_enable_enhanced_mode(struct s5p_dp_device *dp, bool enable)
{
	u32 reg;

	if (enable) {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_4);
		reg |= ENHANCED;
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_4);
	} else {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_4);
		reg &= ~ENHANCED;
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_4);
	}
}

void s5p_dp_set_training_pattern(struct s5p_dp_device *dp,
				 enum pattern_set pattern)
{
	u32 reg;

	switch (pattern) {
	case PRBS7:
		reg = SCRAMBLING_ENABLE | LINK_QUAL_PATTERN_SET_PRBS7;
		writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
		break;
	case D10_2:
		reg = SCRAMBLING_ENABLE | LINK_QUAL_PATTERN_SET_D10_2;
		writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
		break;
	case TRAINING_PTN1:
		reg = SCRAMBLING_DISABLE | SW_TRAINING_PATTERN_SET_PTN1;
		writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
		break;
	case TRAINING_PTN2:
		reg = SCRAMBLING_DISABLE | SW_TRAINING_PATTERN_SET_PTN2;
		writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
		break;
	case DP_NONE:
		reg = SCRAMBLING_ENABLE |
			LINK_QUAL_PATTERN_SET_DISABLE |
			SW_TRAINING_PATTERN_SET_NORMAL;
		writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
		break;
	default:
		break;
	}
}

void s5p_dp_set_lane0_pre_emphasis(struct s5p_dp_device *dp, u32 level)
{
	u32 reg;

	reg = level << PRE_EMPHASIS_SET_SHIFT;
	writel(reg, dp->reg_base + S5P_DP_LN0_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane1_pre_emphasis(struct s5p_dp_device *dp, u32 level)
{
	u32 reg;

	reg = level << PRE_EMPHASIS_SET_SHIFT;
	writel(reg, dp->reg_base + S5P_DP_LN1_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane2_pre_emphasis(struct s5p_dp_device *dp, u32 level)
{
	u32 reg;

	reg = level << PRE_EMPHASIS_SET_SHIFT;
	writel(reg, dp->reg_base + S5P_DP_LN2_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane3_pre_emphasis(struct s5p_dp_device *dp, u32 level)
{
	u32 reg;

	reg = level << PRE_EMPHASIS_SET_SHIFT;
	writel(reg, dp->reg_base + S5P_DP_LN3_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane0_link_training(struct s5p_dp_device *dp, u32 training_lane)
{
	u32 reg;

	reg = training_lane;
	writel(reg, dp->reg_base + S5P_DP_LN0_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane1_link_training(struct s5p_dp_device *dp, u32 training_lane)
{
	u32 reg;

	reg = training_lane;
	writel(reg, dp->reg_base + S5P_DP_LN1_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane2_link_training(struct s5p_dp_device *dp, u32 training_lane)
{
	u32 reg;

	reg = training_lane;
	writel(reg, dp->reg_base + S5P_DP_LN2_LINK_TRAINING_CTL);
}

void s5p_dp_set_lane3_link_training(struct s5p_dp_device *dp, u32 training_lane)
{
	u32 reg;

	reg = training_lane;
	writel(reg, dp->reg_base + S5P_DP_LN3_LINK_TRAINING_CTL);
}

u32 s5p_dp_get_lane0_link_training(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LN0_LINK_TRAINING_CTL);
	return reg;
}

u32 s5p_dp_get_lane1_link_training(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LN1_LINK_TRAINING_CTL);
	return reg;
}

u32 s5p_dp_get_lane2_link_training(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LN2_LINK_TRAINING_CTL);
	return reg;
}

u32 s5p_dp_get_lane3_link_training(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_LN3_LINK_TRAINING_CTL);
	return reg;
}

void s5p_dp_reset_macro(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_PHY_TEST);
	reg |= MACRO_RST;
	writel(reg, dp->reg_base + S5P_DP_PHY_TEST);

	/* 10 us is the minimum reset time. */
	udelay(10);

	reg &= ~MACRO_RST;
	writel(reg, dp->reg_base + S5P_DP_PHY_TEST);
}

int s5p_dp_init_video(struct s5p_dp_device *dp)
{
	u32 reg;
#if defined(CONFIG_MACH_P10_DP_01)

	/* Clear VID_CLK_CHG[1] and VID_FORMAT_CHG[3] and VSYNC_DET[7] */
	reg = VSYNC_DET | VID_FORMAT_CHG | VID_CLK_CHG;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_STA_1);

	/* I_STRM__CLK detect : DE_CTL : Auto detect */
	reg = 0x0;
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_1);

	/* I_STRM__CLK Freq Change Detect : clock Freq force change enable
		=> Force clock not change , for protecting Display flicker */
	reg = (0x4 << 4)|(0 << 1)|(1 << 0);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_2);

	/* FIMD Video stream valid : Auto detect */
	reg = 0x0;
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_3);

	/* Video VID_HRES_TH[7:4], VID_VRES_TH[3:0] */
	reg = (0x2 << 4) | (0x0 << 0);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_8);

#elif defined(CONFIG_MACH_P10_DP_00)

	reg = VSYNC_DET | VID_FORMAT_CHG | VID_CLK_CHG;
	writel(reg, dp->reg_base + S5P_DP_COMMON_INT_STA_1);

	reg = 0x0;
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_1);

	reg = CHA_CRI(4) | CHA_CTRL;
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_2);

	reg = 0x0;
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_3);

#endif

	reg = VID_HRES_TH(2) | VID_VRES_TH(0);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_8);

	return 0;
}

void s5p_dp_set_video_color_format(struct s5p_dp_device *dp,
			u32 color_depth,
			u32 color_space,
			u32 dynamic_range,
			u32 coeff)
{
	u32 reg;

	/* Configure the input color depth, color space, dynamic range */
	reg = (dynamic_range << IN_D_RANGE_SHIFT) |
		(color_depth << IN_BPC_SHIFT) |
		(color_space << IN_COLOR_F_SHIFT);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_2);

	/* Set Input Color YCbCr Coefficients to ITU601 or ITU709 */
	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_3);
	reg &= ~IN_YC_COEFFI_MASK;
	if (coeff)
		reg |= IN_YC_COEFFI_ITU709;
	else
		reg |= IN_YC_COEFFI_ITU601;
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_3);
}

int s5p_dp_is_slave_video_stream_clock_on(struct s5p_dp_device *dp)
{
	u32 reg;
#if defined(CONFIG_MACH_P10_DP_01)


	/* Update Video stream clk detect status */
	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_1);
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_1);

	dev_dbg(dp->dev, "wait SYS_CTL_1.\n");


	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_1);

	if (!(reg & DET_STA)) {
		dev_dbg(dp->dev, "Input stream clock not detected.\n");
		return -EINVAL;
	}

	/* To check whether input stream clock is stable. */
	/* To do that clear it first. */
	/* Update Video stream clk change status */
	if (soc_is_exynos5250()) {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_2);
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_2);
	} else {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_2);
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_2);

		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_2);
		dev_dbg(dp->dev, "wait SYS_CTL_2.\n");

		if (reg & CHA_STA) {
			dev_dbg(dp->dev, "Input stream clk is changing\n");
			return -EINVAL;
		}
	}
#elif defined(CONFIG_MACH_P10_DP_00)

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_1);
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_1);

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_1);

	if (!(reg & DET_STA)) {
		dev_dbg(dp->dev, "Input stream clock not detected.\n");
		return -EINVAL;
	}

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_2);
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_2);

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_2);
	dev_dbg(dp->dev, "wait SYS_CTL_2.\n");

	if (reg & CHA_STA) {
		dev_dbg(dp->dev, "Input stream clk is changing\n");
		return -EINVAL;
	}
#endif
	return 0;
}

void s5p_dp_set_video_cr_mn(struct s5p_dp_device *dp,
		enum clock_recovery_m_value_type type,
		u32 m_value,
		u32 n_value)
{
	u32 reg;

	if (type == REGISTER_M) {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_4);
		reg |= FIX_M_VID;
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_4);
		reg = m_value & 0xff;
		writel(reg, dp->reg_base + S5P_DP_M_VID_0);
		reg = (m_value >> 8) & 0xff;
		writel(reg, dp->reg_base + S5P_DP_M_VID_1);
		reg = (m_value >> 16) & 0xff;
		writel(reg, dp->reg_base + S5P_DP_M_VID_2);

		reg = n_value & 0xff;
		writel(reg, dp->reg_base + S5P_DP_N_VID_0);
		reg = (n_value >> 8) & 0xff;
		writel(reg, dp->reg_base + S5P_DP_N_VID_1);
		reg = (n_value >> 16) & 0xff;
		writel(reg, dp->reg_base + S5P_DP_N_VID_2);
	} else  {
		reg = readl(dp->reg_base + S5P_DP_SYS_CTL_4);
		reg &= ~FIX_M_VID;
		writel(reg, dp->reg_base + S5P_DP_SYS_CTL_4);

		writel(0x00, dp->reg_base + S5P_DP_N_VID_0);
		writel(0x80, dp->reg_base + S5P_DP_N_VID_1);
		writel(0x00, dp->reg_base + S5P_DP_N_VID_2);
	}
}

void s5p_dp_set_video_timing_mode(struct s5p_dp_device *dp, u32 type)
{
	u32 reg;

	if (type == VIDEO_TIMING_FROM_CAPTURE) {
		reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_10);
		reg &= ~FORMAT_SEL;
		writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_10);
	} else {
		reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_10);
		reg |= FORMAT_SEL;
		writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_10);
	}
}

void s5p_dp_enable_video_master(struct s5p_dp_device *dp, bool enable)
{
	u32 reg;

	if (enable) {
		reg = readl(dp->reg_base + S5P_DP_SOC_GENERAL_CTL);
		reg &= ~VIDEO_MODE_MASK;
		reg |= VIDEO_MASTER_MODE_EN | VIDEO_MODE_MASTER_MODE;
		writel(reg, dp->reg_base + S5P_DP_SOC_GENERAL_CTL);
	} else {
		reg = readl(dp->reg_base + S5P_DP_SOC_GENERAL_CTL);
		reg &= ~VIDEO_MODE_MASK;
		reg |= VIDEO_MODE_SLAVE_MODE;
		writel(reg, dp->reg_base + S5P_DP_SOC_GENERAL_CTL);
	}
}

void s5p_dp_start_video(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_1);
	reg |= VIDEO_EN;
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_1);
}

int s5p_dp_is_video_stream_on(struct s5p_dp_device *dp)
{
	u32 reg;
#if defined(CONFIG_MACH_P10_DP_01)

	/* Update STRM_VALID */
	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_3);
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_3);

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_3);
	if (!(reg & STRM_VALID)) {
		dev_dbg(dp->dev, "Input video stream is not detected.\n");
		return -EINVAL;
	}
#elif defined(CONFIG_MACH_P10_DP_00)

	/* Update STRM_VALID */
	reg = F_VALID | VALID_CTRL;
	writel(reg, dp->reg_base + S5P_DP_SYS_CTL_3);

	reg = readl(dp->reg_base + S5P_DP_SYS_CTL_3);
	if (!(reg & STRM_VALID)) {
		dev_dbg(dp->dev, "Input video stream is not detected.\n");
		return -EINVAL;
	}
#endif
	return 0;
}

void s5p_dp_config_video_slave_mode(struct s5p_dp_device *dp,
			struct video_info *video_info)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_FUNC_EN_1);
	reg &= ~(MASTER_VID_FUNC_EN_N|SLAVE_VID_FUNC_EN_N);
	reg |= MASTER_VID_FUNC_EN_N;
	writel(reg, dp->reg_base + S5P_DP_FUNC_EN_1);

	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_10);
	reg &= ~INTERACE_SCAN_CFG;
	reg |= (video_info->interlaced << 2);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_10);

	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_10);
	reg &= ~VSYNC_POLARITY_CFG;
	reg |= (video_info->v_sync_polarity << 1);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_10);

	reg = readl(dp->reg_base + S5P_DP_VIDEO_CTL_10);
	reg &= ~HSYNC_POLARITY_CFG;
	reg |= (video_info->h_sync_polarity << 0);
	writel(reg, dp->reg_base + S5P_DP_VIDEO_CTL_10);

	reg = AUDIO_MODE_SPDIF_MODE | VIDEO_MODE_SLAVE_MODE;
	writel(reg, dp->reg_base + S5P_DP_SOC_GENERAL_CTL);
}

void s5p_dp_enable_scrambling(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_TRAINING_PTN_SET);
	reg &= ~SCRAMBLING_DISABLE;
	writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
}

void s5p_dp_disable_scrambling(struct s5p_dp_device *dp)
{
	u32 reg;

	reg = readl(dp->reg_base + S5P_DP_TRAINING_PTN_SET);
	reg |= SCRAMBLING_DISABLE;
	writel(reg, dp->reg_base + S5P_DP_TRAINING_PTN_SET);
}
