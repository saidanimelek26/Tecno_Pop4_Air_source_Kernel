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

/* ------------------------------------------------------------------ */
/* Local constants                                                    */
/* ------------------------------------------------------------------ */

#define FRAME_WIDTH   540
#define FRAME_HEIGHT  1200

#define LCM_ID_1      0x211F        /* Observed hardware response on Tecno Pop 4 Air BC1 */
#define LCM_ID_2      0x9504        /* Vendor/alternate panel ID */
#define LCM_ID        LCM_ID_1

/* Push-table markers */
#define REGFLAG_DELAY        0xFFE
#define REGFLAG_END_OF_TABLE 0xFFF

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ------------------------------------------------------------------ */
/* Local variables                                                    */
/* ------------------------------------------------------------------ */

static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)   (lcm_util.set_reset_pin((v)))
#define UDELAY(n)          (lcm_util.udelay(n))
#define MDELAY(n)          (lcm_util.mdelay(n))

#define dsi_set_cmdq_V3(para_tbl, size, force_update) \
		lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/* ------------------------------------------------------------------ */
/* Command tables                                                    */
/* ------------------------------------------------------------------ */

struct LCM_setting_table {
	unsigned int  cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_init_setting[] = {
	{0xB9, 3, {0xF1, 0x12, 0x83}},
	{0xBA, 27, {
		0x32, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x44, 0x25, 0x00, 0x91, 0x0A, 0x00, 0x00, 0x02, 0x4F, 0x01,
		0x00, 0x00, 0x37,
	}},
	{0xB8, 1, {0x26}},
	{0xB3, 10, {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}},
	{0xC0, 9, {0x70, 0x73, 0x50, 0x50, 0x00, 0x00, 0x08, 0x70, 0x00}},
	{0xBC, 1, {0x4E}},
	{0xCC, 1, {0x0B}},
	{0xB4, 1, {0x80}},
	{0xB2, 3, {0xB4, 0x15, 0xF0}},
	{0xE3, 14, {
		0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
		0xC0, 0x10,
	}},
	{0xC1, 12, {0x54, 0x00, 0x32, 0x32, 0x99, 0xF4, 0xFF, 0xFF, 0xEE, 0xEE, 0xEE, 0xEE}},
	{0xC6, 6, {0x01, 0x00, 0xFF, 0xFF, 0x00, 0x20}},
	{0xB5, 2, {0x0A, 0x0A}},
	{0xB6, 2, {0x14, 0x14}},
	{0xBF, 3, {0x02, 0x11, 0x00}},
	{0xE9, 63, {
		0xC2, 0x10, 0x06, 0x04, 0xAE, 0x80, 0x81, 0x12, 0x31, 0x23, 0x47, 0x83,
		0x80, 0x81, 0x37, 0x08, 0x0C, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0C, 0x00,
		0x18, 0x00, 0x00, 0x00, 0x88, 0x87, 0x57, 0x53, 0x18, 0x88, 0x88, 0x88,
		0x88, 0x13, 0x88, 0x88, 0x86, 0x46, 0x42, 0x08, 0x88, 0x88, 0x88, 0x88,
		0x02, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
	}},
	{0xEA, 61, {
		0x02, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x88, 0x80, 0x24, 0x60, 0x28, 0x88, 0x88, 0x88, 0x88, 0x64, 0x88, 0x88,
		0x81, 0x35, 0x71, 0x38, 0x88, 0x88, 0x88, 0x88, 0x75, 0x88, 0x23, 0x20,
		0x00, 0x00, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00,
		0x00,
	}},
	{0xE0, 34, {
		0x00, 0x0D, 0x15, 0x2A, 0x2D, 0x3F, 0x48, 0x3A, 0x07, 0x0E, 0x0E, 0x12,
		0x13, 0x11, 0x13, 0x12, 0x19, 0x00, 0x0D, 0x15, 0x2A, 0x2D, 0x3F, 0x48,
		0x3A, 0x07, 0x0E, 0x0E, 0x12, 0x13, 0x11, 0x13, 0x12, 0x19,
	}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 10, {}},
	{REGFLAG_END_OF_TABLE, 0, {}},
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	{0x10, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0, {}},
};

/* ------------------------------------------------------------------ */
/* Local functions                                                    */
/* ------------------------------------------------------------------ */

/* helper to send init/suspend commands */
static void push_table(struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, 1);
			break;
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

/* ------------------------------------------------------------------ */
/* lcm_get_params                                                    */
/* ------------------------------------------------------------------ */

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;   /* 540 */
	params->height = FRAME_HEIGHT;  /* 1200 */

	params->dbi.te_mode          = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

	params->dsi.mode                    = SYNC_PULSE_VDO_MODE;
	params->dsi.LANE_NUM                = LCM_THREE_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	params->dsi.intermediat_buffer_num  = 0;
	params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count              = 1440;
	params->dsi.packet_size             = 256;

	params->dsi.vertical_sync_active    = 4;
	params->dsi.vertical_backporch      = 15;
	params->dsi.vertical_frontporch     = 18;
	params->dsi.vertical_active_line    = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active  = 35;
	params->dsi.horizontal_backporch    = 35;
	params->dsi.horizontal_frontporch   = 35;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK               = 200;
	params->dsi.ssc_disable             = 1;
	params->dsi.cont_clock              = 1;
	params->dsi.noncont_clock           = 0;

	params->dsi.customization_esd_check_enable = 1;
	params->dsi.esd_check_enable               = 1;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x68;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0xC0;
	params->dsi.lcm_esd_check_table[1].cmd          = 0xAF;
	params->dsi.lcm_esd_check_table[1].count        = 1;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0xFD;
	params->dsi.lcm_esd_check_table[2].cmd          = 0x09;
	params->dsi.lcm_esd_check_table[2].count        = 1;
	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x80;
}

/* ------------------------------------------------------------------ */
/* lcm_init                                                          */
/* ------------------------------------------------------------------ */

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(20);          /* reset low delay */
	SET_RESET_PIN(1);
	MDELAY(120);         /* reset high delay */

	push_table(lcm_init_setting,
		   sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table));
}

static void lcm_suspend(void)
{

	push_table(lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));

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

/* ------------------------------------------------------------------ */
/* lcm_compare_id - reads RDDID and compares against 0x9504           */
/* ------------------------------------------------------------------ */

static unsigned int lcm_compare_id(void)
{
	unsigned char  buffer[4];
	unsigned int   array[1];
	unsigned int   id;

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	/* DCS SET_MAX_RETURN_PKT_SIZE (0x37) with param 0x03 */
	array[0] = 0x00033700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer, 4);           /* RDDID */

	id = (buffer[1] << 8) | buffer[2];      

	pr_info("st7703_hdminus_dsi_vdo_dezhixin_vp650 %s:0x%2x,0x%2x,0x%2x,0x%2x id=0x%x\n",
	       __func__, buffer[0], buffer[1], buffer[2], buffer[3], id);

	return (id == LCM_ID_1 || id == LCM_ID_2) ? TRUE : FALSE;
}


struct LCM_DRIVER st7703_hdminus_dsi_vdo_dezhixin_vp650 = {
	.name           = "st7703_hdminus_dsi_vdo_dezhixin_vp650_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
};
