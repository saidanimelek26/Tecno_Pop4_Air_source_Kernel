#ifndef __SUNWAVE_SF_CTL_H__
#define __SUNWAVE_SF_CTL_H__

#include <linux/types.h>
#include <linux/ioctl.h>

#define SF_DRV_VERSION "v2.2.38-2018-12-25"

typedef enum {
	SF_KEY_NONE = 0,
	SF_KEY_HOME,
	SF_KEY_MENU,
	SF_KEY_BACK,
	SF_KEY_F11,
	SF_KEY_ENTER,
	SF_KEY_UP,
	SF_KEY_LEFT,
	SF_KEY_RIGHT,
	SF_KEY_DOWN,
	SF_KEY_WAKEUP,
	SF_KEY_F19,
} sf_key_type_t;

typedef struct {
	sf_key_type_t key;
	int value;
} sf_key_event_t;

typedef struct {
	char tee_solution[32];
	char ta_name[32];
	char tee_version[32];
	char drv_version[32];
} sf_version_info_t;

#define SF_IOC_MAGIC 's'

#define SF_IOC_INIT_DRIVER      _IO(SF_IOC_MAGIC, 0x00)
#define SF_IOC_DEINIT_DRIVER    _IO(SF_IOC_MAGIC, 0x01)
#define SF_IOC_RESET_DEVICE     _IO(SF_IOC_MAGIC, 0x01)
#define SF_IOC_ENABLE_IRQ       _IO(SF_IOC_MAGIC, 0x02)
#define SF_IOC_DISABLE_IRQ      _IO(SF_IOC_MAGIC, 0x03)
#define SF_IOC_REQUEST_IRQ      _IO(SF_IOC_MAGIC, 0x21)
#define SF_IOC_ENABLE_SPI_CLK   _IO(SF_IOC_MAGIC, 0x04)
#define SF_IOC_DISABLE_SPI_CLK  _IO(SF_IOC_MAGIC, 0x05)
#define SF_IOC_ENABLE_POWER     _IO(SF_IOC_MAGIC, 0x06)
#define SF_IOC_DISABLE_POWER    _IO(SF_IOC_MAGIC, 0x07)
#define SF_IOC_REPORT_KEY_EVENT _IOW(SF_IOC_MAGIC, 0x08, sf_key_event_t)
#define SF_IOC_SYNC_CONFIG      _IOWR(SF_IOC_MAGIC, 0x09, void *)
#define SF_IOC_SPI_SPEED        _IO(SF_IOC_MAGIC, 0x0b)
#define SF_IOC_ATTRIBUTE        _IO(SF_IOC_MAGIC, 0x0d)
#define SF_IOC_GET_VERSION      _IOR(SF_IOC_MAGIC, 0x20, sf_version_info_t)
#define SF_IOC_SET_LIB_VERSION  _IO(SF_IOC_MAGIC, 0x30)
#define SF_IOC_GET_LIB_VERSION  _IO(SF_IOC_MAGIC, 0x31)

#define SF_IOC_MTK_COOLBOOT_POWER _IO('k', 0x04)
#define SF_IOC_MTK_SET_SPI_SPEED  _IOW('k', 0x04, u32)
#define SF_IOC_MTK_GET_IRQ_STAT   _IOR('k', 0x04, u32)
#define SF_IOC_MTK_WAIT_IRQ       _IO('k', 0x05)

struct sf_ctl_device {
	struct miscdevice miscdev;
	int irq_num;
	struct work_struct work_queue;
	struct input_dev *input;
	struct regulator *vdd_reg;
	int (*gpio_init)(struct sf_ctl_device *ctl_dev);
	int (*gpio_free)(struct sf_ctl_device *ctl_dev);
	int (*power_on)(bool on);
	int (*spi_clk_on)(bool on);
	int (*reset)(void);
	struct wakeup_source wakelock;
	struct notifier_block notifier;
	char *version;
	struct spi_device *pdev;
	int attribute;
	int irq_is_on;
	int reset_num;
	int irq_pin_num;
	int pwr_num;
	struct device_node *irq_node;
};

int sf_platform_init(struct sf_ctl_device *ctl_dev);
void sf_platform_exit(struct sf_ctl_device *ctl_dev);
int sf_ctl_device_power(bool on);
int sf_ctl_device_reset(void);
int sf_ctl_device_init_gpio_pins(struct sf_ctl_device *ctl_dev);
int sf_ctl_device_free_gpio(struct sf_ctl_device *ctl_dev);
int sf_spi_clock_enable(bool on);

extern int g_sunwave_matched;
extern int g_focaltech_chip_id;

#ifdef CONFIG_FOCALTECH_FINGERPRINT_EDISON
int focaltech_fp_probe_chip_id(struct spi_device *spi);
#endif

#endif
