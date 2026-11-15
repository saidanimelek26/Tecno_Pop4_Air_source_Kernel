#ifndef __FF_CTL_H__
#define __FF_CTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spi.h>

#define FF_IOC_MAGIC 'f'

#define FF_IOC_INIT_DRIVER      _IO(FF_IOC_MAGIC, 0x00)
#define FF_IOC_FREE_DRIVER      _IO(FF_IOC_MAGIC, 0x01)
#define FF_IOC_RESET_DEVICE     _IO(FF_IOC_MAGIC, 0x02)
#define FF_IOC_ENABLE_IRQ       _IO(FF_IOC_MAGIC, 0x03)
#define FF_IOC_DISABLE_IRQ      _IO(FF_IOC_MAGIC, 0x04)
#define FF_IOC_ENABLE_SPI_CLK   _IO(FF_IOC_MAGIC, 0x05)
#define FF_IOC_DISABLE_SPI_CLK  _IO(FF_IOC_MAGIC, 0x06)
#define FF_IOC_ENABLE_POWER     _IO(FF_IOC_MAGIC, 0x07)
#define FF_IOC_DISABLE_POWER    _IO(FF_IOC_MAGIC, 0x08)

typedef struct {
	unsigned int code;
	int value;
} ff_key_event_t;

#define FF_IOC_REPORT_KEY_EVENT _IOW(FF_IOC_MAGIC, 0x09, ff_key_event_t)

typedef struct {
	unsigned int key_code[16];
} ff_driver_config_t;

#define FF_IOC_SYNC_CONFIG      _IOWR(FF_IOC_MAGIC, 0x0a, ff_driver_config_t)
#define FF_IOC_GET_VERSION      _IOR(FF_IOC_MAGIC, 0x0b, char[128])
#define FF_IOC_SET_VENDOR_INFO  _IOW(FF_IOC_MAGIC, 0x0c, char[128])
#define FF_IOC_SET_IC_INFORMATION _IOW(FF_IOC_MAGIC, 0x0d, char[128])
#define FF_IOC_GET_IC_INFORMATION _IOR(FF_IOC_MAGIC, 0x0e, char[128])

typedef enum {
	FF_POLARITY_falling = 0,
	FF_POLARITY_rising  = 1,
} ff_irq_polarity_t;

typedef enum {
	FF_EVENT_POLL = 0,
	FF_EVENT_NETLINK = 1,
} ff_event_type_t;

typedef struct {
	int (*init_pins)(int *irq_num);
	int (*free_pins)(void);
	int (*enable_spiclk)(bool on);
	int (*enable_power)(bool on);
	int (*reset_device)(void);
	const char *(*arch_str)(void);
} ff_plat_context_t;

int ff_ctl_init_pins(int *irq_num);
int ff_ctl_free_pins(void);
int ff_ctl_enable_spiclk(bool on);
int ff_ctl_enable_power(bool on);
int ff_ctl_reset_device(void);
const char *ff_ctl_arch_str(void);

int ff_spi_write_buf(const u8 *tx_buf, int len);
int ff_spi_write_then_read_buf(const u8 *tx_buf, int tx_len,
			       u8 *rx_buf, int rx_len);
int focaltech_fp_probe_chip_id(struct spi_device *spi);

extern int g_focaltech_chip_id;

#endif
