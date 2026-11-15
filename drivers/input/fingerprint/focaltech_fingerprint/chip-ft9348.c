#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

#include "ff_ctl.h"
#include "ff_log.h"
#include "ff_err.h"

#undef LOG_TAG
#define LOG_TAG "focaltech:ft9348"

int g_focaltech_chip_id;
EXPORT_SYMBOL(g_focaltech_chip_id);

static struct spi_device *g_ff_spidev;

static const u8 g_wm_cmds[12][3] = {
	{ 0xc0, 0x3f, 0x00 },
	{ 0xc1, 0x3e, 0x00 },
	{ 0xc2, 0x3d, 0x00 },
	{ 0xc4, 0x3b, 0x00 },
	{ 0xc8, 0x37, 0x00 },
	{ 0xd8, 0x27, 0x00 },
	{ 0xd1, 0x2e, 0x00 },
	{ 0xd2, 0x2d, 0x00 },
	{ 0xd4, 0x2b, 0x00 },
	{ 0x5a, 0xa5, 0x00 },
	{ 0xa5, 0x5a, 0x00 },
	{ 0x70, 0x00, 0x00 },
};

int ff_spi_write_buf(const u8 *tx_buf, int len)
{
	struct spi_transfer t = {
		.tx_buf = tx_buf,
		.len = len,
	};
	struct spi_message m;

	if (!g_ff_spidev)
		return -ENODEV;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(g_ff_spidev, &m);
}

int ff_spi_write_then_read_buf(const u8 *tx_buf, int tx_len,
			       u8 *rx_buf, int rx_len)
{
	if (!g_ff_spidev)
		return -ENODEV;

	return spi_write_then_read(g_ff_spidev, tx_buf, tx_len,
				   rx_buf, rx_len);
}

static int fw8064_wm_switch(u8 mode)
{
	u8 cmd[3];
	int len;

	if (mode > 11)
		return FF_ERR_BAD_PARAMS;

	cmd[0] = g_wm_cmds[mode][0];
	cmd[1] = g_wm_cmds[mode][1];
	cmd[2] = g_wm_cmds[mode][2];
	len = (mode == 11) ? 1 : 3;

	return ff_spi_write_buf(cmd, len);
}

static u16 fw8064_read_sram(u16 addr)
{
	u8 tx[6];
	u8 rx[2] = { 0, 0 };
	u16 inv = (~addr) & 0x7fff;

	tx[0] = 0x04;
	tx[1] = 0xfb;
	tx[2] = (inv >> 8) & 0xff;
	tx[3] = inv & 0xff;
	tx[4] = 0x00;
	tx[5] = 0x00;

	ff_spi_write_then_read_buf(tx, 6, rx, 2);
	return ((u16)rx[0] << 8) | rx[1];
}

static u16 fw8064_chipid_get(void)
{
	return fw8064_read_sram(0x1a8b);
}

int focaltech_fp_probe_chip_id(struct spi_device *spi)
{
	u16 chip_id;
	int err, i;

	g_ff_spidev = spi;

	err = fw8064_wm_switch(9);
	FF_LOGI("e_WorkMode_OscOn= : %d", err);

	err = fw8064_wm_switch(0);
	FF_LOGI("e_WorkMode_Idle= : %d", err);

	for (i = 0; i < 5; i++)
		udelay(600);

	chip_id = fw8064_chipid_get();
	FF_LOGI("chip id is = : 0x%04x", chip_id);

	if (chip_id == 0xffff)
		chip_id = 0;

	g_focaltech_chip_id = chip_id;
	return g_focaltech_chip_id;
}
EXPORT_SYMBOL(focaltech_fp_probe_chip_id);
