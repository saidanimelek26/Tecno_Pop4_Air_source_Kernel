/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(BUILD_LK)
#include <string.h>
#else
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#endif

#include "lcm_drv.h"

#define FRAME_WIDTH   540
#define FRAME_HEIGHT  1200

#define LCM_ID_NV3051D 0x3052
#define LCM_ID         LCM_ID_NV3051D

#define REGFLAG_DELAY        0xFFE
#define REGFLAG_END_OF_TABLE 0xFFF

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)   (lcm_util.set_reset_pin((v)))
#define UDELAY(n)          (lcm_util.udelay(n))
#define MDELAY(n)          (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)     (lcm_util.dsi_write_cmd(cmd))
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg           (lcm_util.dsi_read_reg())
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x01}},
    {0xe3, 1, {0x00}},
    {0x03, 1, {0x40}},
    {0x04, 1, {0x00}},
    {0x05, 1, {0x03}},
    {0x08, 1, {0x00}},
    {0x09, 1, {0x07}},
    {0x0a, 1, {0x01}},
    {0x0b, 1, {0x32}},
    {0x0c, 1, {0x32}},
    {0x0d, 1, {0x0b}},
    {0x0e, 1, {0x00}},
    {0x20, 1, {0xa0}},
    {0x25, 1, {0x0a}},
    {0x28, 1, {0x74}},
    {0x2a, 1, {0xaf}},
    {0x38, 1, {0x9c}},
    {0x39, 1, {0xa7}},
    {0x3a, 1, {0x13}},
    {0x40, 1, {0x12}},
    {0x49, 1, {0x3c}},
    {0x6d, 1, {0x00}},
    {0x6e, 1, {0x00}},
    {0x91, 1, {0x77}},
    {0x92, 1, {0x77}},
    {0x99, 1, {0x59}},
    {0x9b, 1, {0x56}},
    {0xa0, 1, {0x55}},
    {0xa1, 1, {0x50}},
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x01}},
    {0xa3, 1, {0x58}},
    {0xa4, 1, {0x9c}},
    {0xa7, 1, {0x02}},
    {0xa8, 1, {0x01}},
    {0xa9, 1, {0x01}},
    {0xaa, 1, {0xfc}},
    {0xab, 1, {0x28}},
    {0xac, 1, {0x06}},
    {0xad, 1, {0x06}},
    {0xae, 1, {0x06}},
    {0xaf, 1, {0x03}},
    {0xb0, 1, {0x08}},
    {0xb1, 1, {0x26}},
    {0xb2, 1, {0x28}},
    {0xb3, 1, {0x28}},
    {0xb4, 1, {0x33}},
    {0xb5, 1, {0x08}},
    {0xb6, 1, {0x26}},
    {0xb7, 1, {0x08}},
    {0xb8, 1, {0x26}},
    {0xf0, 1, {0x00}},
    {0xf6, 1, {0xcc}},
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x02}},
    {0xb1, 1, {0x15}},
    {0xd1, 1, {0x0b}},
    {0xb4, 1, {0x32}},
    {0xd4, 1, {0x30}},
    {0xb2, 1, {0x15}},
    {0xd2, 1, {0x0d}},
    {0xb3, 1, {0x31}},
    {0xd3, 1, {0x2f}},
    {0xb6, 1, {0x24}},
    {0xd6, 1, {0x20}},
    {0xb7, 1, {0x3d}},
    {0xd7, 1, {0x3b}},
    {0xc1, 1, {0x08}},
    {0xe1, 1, {0x0a}},
    {0xb8, 1, {0x0d}},
    {0xd8, 1, {0x0f}},
    {0xb9, 1, {0x05}},
    {0xd9, 1, {0x07}},
    {0xbd, 1, {0x13}},
    {0xdd, 1, {0x13}},
    {0xbc, 1, {0x11}},
    {0xdc, 1, {0x11}},
    {0xbb, 1, {0x0f}},
    {0xdb, 1, {0x0f}},
    {0xba, 1, {0x10}},
    {0xda, 1, {0x10}},
    {0xbe, 1, {0x19}},
    {0xde, 1, {0x19}},
    {0xbf, 1, {0x10}},
    {0xdf, 1, {0x12}},
    {0xc0, 1, {0x17}},
    {0xe0, 1, {0x19}},
    {0xb5, 1, {0x37}},
    {0xd5, 1, {0x3a}},
    {0xb0, 1, {0x02}},
    {0xd0, 1, {0x05}},
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x03}},
    {0x04, 1, {0x51}},
    {0x05, 1, {0xc0}},
    {0x06, 1, {0xc0}},
    {0x08, 1, {0x87}},
    {0x09, 1, {0x86}},
    {0x0a, 1, {0x85}},
    {0x0b, 1, {0x84}},
    {0x24, 1, {0x51}},
    {0x25, 1, {0xc0}},
    {0x26, 1, {0xc0}},
    {0x28, 1, {0x44}},
    {0x29, 1, {0x44}},
    {0x2a, 1, {0xad}},
    {0x2b, 1, {0xae}},
    {0x2c, 1, {0xaf}},
    {0x2d, 1, {0xb0}},
    {0x34, 1, {0xa1}},
    {0x35, 1, {0x53}},
    {0x36, 1, {0x53}},
    {0x37, 1, {0x13}},
    {0x40, 1, {0x85}},
    {0x41, 1, {0x84}},
    {0x42, 1, {0x83}},
    {0x43, 1, {0x82}},
    {0x44, 1, {0x44}},
    {0x45, 1, {0xb0}},
    {0x46, 1, {0xb1}},
    {0x47, 1, {0x44}},
    {0x48, 1, {0xb2}},
    {0x49, 1, {0xb3}},
    {0x50, 1, {0x81}},
    {0x51, 1, {0x00}},
    {0x52, 1, {0x01}},
    {0x53, 1, {0x02}},
    {0x54, 1, {0x44}},
    {0x55, 1, {0xb4}},
    {0x56, 1, {0xb5}},
    {0x57, 1, {0x44}},
    {0x58, 1, {0xb6}},
    {0x59, 1, {0xb7}},
    {0x83, 1, {0x03}},
    {0x84, 1, {0x02}},
    {0x85, 1, {0x07}},
    {0x86, 1, {0x06}},
    {0x87, 1, {0x05}},
    {0x88, 1, {0x04}},
    {0x92, 1, {0x00}},
    {0x93, 1, {0x01}},
    {0x99, 1, {0x03}},
    {0x9a, 1, {0x02}},
    {0x9b, 1, {0x07}},
    {0x9c, 1, {0x06}},
    {0x9d, 1, {0x05}},
    {0x9e, 1, {0x04}},
    {0xa8, 1, {0x00}},
    {0xa9, 1, {0x01}},
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x01}},
    {0xa3, 1, {0x58}},
    {0xc0, 1, {0x87}},
    {0xff, 1, {0x30}},
    {0xff, 1, {0x52}},
    {0xff, 1, {0x00}},
    {0x36, 1, {0x02}},
    {0x11, 0, {}},
    {REGFLAG_DELAY, 50, {}},
    {0x29, 0, {}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {}},
	{REGFLAG_DELAY, 10, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode   = SYNC_PULSE_VDO_MODE;
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.intermediat_buffer_num = 0;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active				= 4;
	params->dsi.vertical_backporch					= 18;
	params->dsi.vertical_frontporch					= 35;
	params->dsi.vertical_active_line				= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 18;
	params->dsi.horizontal_frontporch				= 15;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 200;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(220);
	SET_RESET_PIN(1);
	MDELAY(50);

	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(120);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[4] = {0};
	unsigned int id = 0;

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	read_reg_v2(0x04, buffer, 3);
	id = (buffer[1] << 8) | buffer[2];

#if defined(BUILD_LK)
	printf("nv3051d_hdminus_dsi_vdo_dezhixin_vp650 %s:0x%2x,0x%2x,0x%2x,0x%2x id=0x%x\n",
	       __func__, buffer[0], buffer[1], buffer[2], buffer[3], id);
#else
	pr_debug("nv3051d_hdminus_dsi_vdo_dezhixin_vp650 %s:0x%2x,0x%2x,0x%2x,0x%2x id=0x%x\n",
	       __func__, buffer[0], buffer[1], buffer[2], buffer[3], id);
#endif

	return (id == LCM_ID_NV3051D || id == 0x3051) ? 1 : 0;
}

struct LCM_DRIVER nv3051d_hdminus_dsi_vdo_dezhixin_vp650_lcm_drv = {
	.name           = "nv3051d_hdminus_dsi_vdo_dezhixin_vp650",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
};
