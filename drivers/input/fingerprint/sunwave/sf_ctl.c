#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/spi/spi.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>

#include "mtk_spi.h"
#include "sf_ctl.h"

int g_sunwave_matched;
EXPORT_SYMBOL(g_sunwave_matched);

static int sf_screen_state;
static char sf_tee_version[32];

static struct mt_chip_conf sf_spi_conf = {
	.setuptime = 15,
	.holdtime = 15,
	.high_time = 50,
	.low_time = 50,
	.cs_idletime = 20,
	.ulthgh_thrsh = 0,
	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,
	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,
	.tx_endian = SPI_LENDIAN,
	.rx_endian = SPI_LENDIAN,
	.com_mod = FIFO_TRANSFER,
	.pause = PAUSE_MODE_DISABLE,
	.finish_intr = FINISH_INTR_EN,
	.deassert = DEASSERT_DISABLE,
	.ulthigh = ULTRA_HIGH_DISABLE,
	.tckdly = TICK_DLY0,
};

static struct spi_board_info sf_spi_board_info[] __initdata = {
	{
		.modalias = "sunwave-fp",
		.max_speed_hz = 1 * 1000 * 1000,
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &sf_spi_conf,
	},
};

static int sf_ctl_dev_open(struct inode *inode, struct file *filp);
static int sf_ctl_dev_release(struct inode *inode, struct file *filp);
static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static const struct file_operations sf_ctl_fops = {
	.owner = THIS_MODULE,
	.open = sf_ctl_dev_open,
	.release = sf_ctl_dev_release,
	.unlocked_ioctl = sf_ctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sf_ctl_ioctl,
#endif
};

static struct sf_ctl_device sf_ctl_dev = {
	.miscdev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "sunwave_fp",
		.fops = &sf_ctl_fops,
	},
	.version = sf_tee_version,
};

static int sf_spi_xfer(struct spi_device *spi, u8 *tx, u8 *rx, int len)
{
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = len,
	};
	struct spi_message m;

	if (!spi)
		return -ENODEV;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static irqreturn_t sf_ctl_device_irq(int irq, void *dev_id)
{
	struct sf_ctl_device *ctl = dev_id;
	static char *envp[2] = { "FINGERPRINT_STATUS=0", NULL };

	__pm_wakeup_event(&ctl->wakelock, msecs_to_jiffies(5000));
	if (ctl->miscdev.this_device)
		kobject_uevent_env(&ctl->miscdev.this_device->kobj,
				   KOBJ_CHANGE, envp);

	return IRQ_HANDLED;
}

static int sf_fb_notifier_callback(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (event != FB_EVENT_BLANK || !evdata || !evdata->data)
		return 0;

	blank = evdata->data;
	sf_screen_state = (*blank == FB_BLANK_UNBLANK) ? 1 : 0;
	pr_debug("[sunwave] screen state=%d\n", sf_screen_state);
	return 0;
}

static int sf_ctl_dev_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &sf_ctl_dev;
	return 0;
}

static int sf_ctl_dev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sf_ctl_report_key_event(struct sf_ctl_device *ctl,
				   sf_key_event_t *kevent)
{
	static const unsigned int key_map[] = {
		KEY_RESERVED,
		KEY_HOME,
		KEY_MENU,
		KEY_BACK,
		KEY_F11,
		KEY_ENTER,
		KEY_UP,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_DOWN,
		KEY_WAKEUP,
		KEY_F19,
	};
	unsigned int code;

	if (!ctl->input)
		return -ENODEV;

	if ((unsigned int)kevent->key < ARRAY_SIZE(key_map))
		code = key_map[kevent->key];
	else
		code = KEY_CAMERA;

	if (code == KEY_RESERVED)
		return -EINVAL;

	input_report_key(ctl->input, code, kevent->value);
	input_sync(ctl->input);
	return 0;
}

static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sf_ctl_device *ctl = filp->private_data;
	int err = 0;

	if (!ctl)
		return -ENODEV;

	switch (cmd) {
	case SF_IOC_RESET_DEVICE:
		if (ctl->reset)
			err = ctl->reset();
		break;

	case SF_IOC_ENABLE_IRQ:
		if (ctl->irq_num > 0 && !ctl->irq_is_on) {
			enable_irq(ctl->irq_num);
			ctl->irq_is_on = 1;
		}
		break;

	case SF_IOC_DISABLE_IRQ:
		if (ctl->irq_num > 0 && ctl->irq_is_on) {
			disable_irq_nosync(ctl->irq_num);
			ctl->irq_is_on = 0;
		}
		break;

	case SF_IOC_REQUEST_IRQ:
		if (ctl->irq_num > 0) {
			err = request_threaded_irq(ctl->irq_num, NULL,
						   sf_ctl_device_irq,
						   IRQF_TRIGGER_RISING |
						   IRQF_ONESHOT,
						   "sunwave-fp", ctl);
			if (!err) {
				enable_irq_wake(ctl->irq_num);
				ctl->irq_is_on = 1;
			}
		}
		break;

	case SF_IOC_ENABLE_SPI_CLK:
	case SF_IOC_MTK_COOLBOOT_POWER:
		if (ctl->spi_clk_on)
			ctl->spi_clk_on(true);
		break;

	case SF_IOC_DISABLE_SPI_CLK:
		if (ctl->spi_clk_on)
			ctl->spi_clk_on(false);
		break;

	case SF_IOC_ENABLE_POWER:
		if (ctl->power_on)
			err = ctl->power_on(true);
		break;

	case SF_IOC_DISABLE_POWER:
		if (ctl->power_on)
			err = ctl->power_on(false);
		break;

	case SF_IOC_REPORT_KEY_EVENT: {
		sf_key_event_t kevent;

		if (copy_from_user(&kevent, (void __user *)arg,
				   sizeof(kevent))) {
			err = -EFAULT;
			break;
		}
		err = sf_ctl_report_key_event(ctl, &kevent);
		break;
	}

	case SF_IOC_SPI_SPEED:
	case SF_IOC_MTK_SET_SPI_SPEED: {
		u32 speed = (u32)arg;

		if (cmd == SF_IOC_MTK_SET_SPI_SPEED) {
			if (copy_from_user(&speed, (void __user *)arg,
					   sizeof(speed))) {
				err = -EFAULT;
				break;
			}
		}
		if (ctl->pdev && speed > 0) {
			u32 clk_div = 104000000 / speed;

			if (clk_div < 2)
				clk_div = 2;
			sf_spi_conf.high_time = clk_div / 2;
			sf_spi_conf.low_time = clk_div / 2;
			ctl->pdev->max_speed_hz = speed;
			spi_setup(ctl->pdev);
		}
		break;
	}

	case SF_IOC_ATTRIBUTE:
		if (copy_to_user((void __user *)arg, &ctl->attribute,
				 sizeof(ctl->attribute)))
			err = -EFAULT;
		break;

	case SF_IOC_GET_VERSION: {
		sf_version_info_t ver;

		memset(&ver, 0, sizeof(ver));
		strlcpy(ver.drv_version, SF_DRV_VERSION,
			sizeof(ver.drv_version));
		if (copy_to_user((void __user *)arg, &ver, sizeof(ver)))
			err = -EFAULT;
		break;
	}

	case SF_IOC_SET_LIB_VERSION:
		if (copy_from_user(sf_tee_version, (void __user *)arg,
				   sizeof(sf_tee_version) - 1))
			err = -EFAULT;
		sf_tee_version[sizeof(sf_tee_version) - 1] = '\0';
		break;

	case SF_IOC_GET_LIB_VERSION:
		if (copy_to_user((void __user *)arg, sf_tee_version,
				 sizeof(sf_tee_version)))
			err = -EFAULT;
		break;

	case SF_IOC_MTK_GET_IRQ_STAT:
		err = put_user(ctl->irq_is_on, (u32 __user *)arg);
		break;

	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int sf_read_sensor_id(struct spi_device *spi)
{
	u8 tx[16];
	u8 rx[16];
	int i, err;

	pr_info("[sunwave] %s enter\n", __func__);

	for (i = 0; i < 4; i++) {
		msleep(10);

		memset(tx, 0, sizeof(tx));
		memset(rx, 0, sizeof(rx));
		tx[0] = 0xc0;
		tx[1] = 0x5f;
		err = sf_spi_xfer(spi, tx, rx, 6);
		if (!err && rx[2] == 'S' && rx[3] == 'u' &&
		    rx[4] == 'n' && rx[5] == 'W') {
			pr_info("[sunwave] detected SunW sensor\n");
			return 0;
		}

		memset(tx, 0, sizeof(tx));
		memset(rx, 0, sizeof(rx));
		tx[0] = 0x60;
		tx[1] = 0x9f;
		tx[2] = 0x28;
		tx[3] = 0x02;
		err = sf_spi_xfer(spi, tx, rx, 7);
		if (!err && rx[5] == 0x82) {
			pr_info("[sunwave] detected SW82xx sensor (7-byte)\n");
			return 0;
		}

		memset(tx, 0, sizeof(tx));
		memset(rx, 0, sizeof(rx));
		tx[0] = 0x60;
		tx[1] = 0x28;
		tx[2] = 0x02;
		err = sf_spi_xfer(spi, tx, rx, 6);
		if (!err && rx[4] == 0x82) {
			pr_info("[sunwave] detected SW82xx sensor (6-byte)\n");
			return 0;
		}
	}

	return -ENODEV;
}

static int sf_ctl_init_input(struct sf_ctl_device *ctl)
{
	struct input_dev *input;
	int err;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	input->name = "sf-keys";
	__set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_HOME, input->keybit);
	__set_bit(KEY_MENU, input->keybit);
	__set_bit(KEY_BACK, input->keybit);
	__set_bit(KEY_UP, input->keybit);
	__set_bit(KEY_DOWN, input->keybit);
	__set_bit(KEY_LEFT, input->keybit);
	__set_bit(KEY_RIGHT, input->keybit);
	__set_bit(KEY_ENTER, input->keybit);
	__set_bit(KEY_CAMERA, input->keybit);
	__set_bit(KEY_F11, input->keybit);
	__set_bit(KEY_F19, input->keybit);

	err = input_register_device(input);
	if (err) {
		input_free_device(input);
		return err;
	}

	ctl->input = input;
	return 0;
}

static ssize_t sf_show_version(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", sf_tee_version);
}

static ssize_t sunwave_chip_info_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "sunwave\n");
}

static ssize_t sunwave_version_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", SF_DRV_VERSION);
}

static ssize_t sf_store_set_fun(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(tee_version, 0444, sf_show_version, NULL);
static DEVICE_ATTR(chip_info, 0444, sunwave_chip_info_show, NULL);
static DEVICE_ATTR(version, 0444, sunwave_version_show, NULL);
static DEVICE_ATTR(set_fun, 0200, NULL, sf_store_set_fun);

static struct attribute *sf_attrs[] = {
	&dev_attr_tee_version.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_version.attr,
	&dev_attr_set_fun.attr,
	NULL,
};

static const struct attribute_group sf_attr_group = {
	.attrs = sf_attrs,
};

static int sf_probe(struct spi_device *spi)
{
	int err;

	pr_info("[sunwave] %s enter\n", __func__);

	sf_ctl_dev.pdev = spi;
	spi_set_drvdata(spi, &sf_ctl_dev);

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 1 * 1000 * 1000;
	spi->controller_data = &sf_spi_conf;
	spi_setup(spi);

	err = sf_platform_init(&sf_ctl_dev);
	if (err) {
		sf_ctl_dev.pdev = NULL;
		return err;
	}

	wakeup_source_init(&sf_ctl_dev.wakelock, "sf_wakelock");

	err = sf_ctl_dev.gpio_init(&sf_ctl_dev);
	if (err) {
		wakeup_source_trash(&sf_ctl_dev.wakelock);
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		return err;
	}

	sf_ctl_dev.spi_clk_on(true);
	sf_ctl_dev.reset();

	err = sf_read_sensor_id(spi);
	if (err) {
#ifdef CONFIG_FOCALTECH_FINGERPRINT_EDISON
		int chip_id;

		sf_ctl_dev.power_on(true);
		sf_ctl_dev.reset();
		chip_id = focaltech_fp_probe_chip_id(spi);
		pr_info("[sunwave] focaltech_chipid=%x\n", chip_id);
		if (chip_id != 0x9371) {
			sf_ctl_dev.reset();
			chip_id = focaltech_fp_probe_chip_id(spi);
			pr_info("[sunwave] focaltech_chipid2=%x\n", chip_id);
		}
#endif
		sf_ctl_dev.spi_clk_on(false);
		sf_ctl_dev.power_on(false);
		sf_ctl_dev.gpio_free(&sf_ctl_dev);
		wakeup_source_trash(&sf_ctl_dev.wakelock);
		sf_platform_exit(&sf_ctl_dev);
		sf_ctl_dev.pdev = NULL;
		return -ENODEV;
	}

	g_sunwave_matched = 1;

	err = sf_ctl_init_input(&sf_ctl_dev);
	if (err)
		goto err_cleanup;

	err = misc_register(&sf_ctl_dev.miscdev);
	if (err) {
		input_unregister_device(sf_ctl_dev.input);
		sf_ctl_dev.input = NULL;
		goto err_cleanup;
	}

	if (sf_ctl_dev.miscdev.this_device) {
		if (sysfs_create_group(&sf_ctl_dev.miscdev.this_device->kobj,
				       &sf_attr_group))
			pr_warn("[sunwave] sysfs_create_group failed\n");
	}

	sf_ctl_dev.notifier.notifier_call = sf_fb_notifier_callback;
	fb_register_client(&sf_ctl_dev.notifier);

	pr_info("[sunwave] probe done\n");
	return 0;

err_cleanup:
	sf_ctl_dev.spi_clk_on(false);
	sf_ctl_dev.gpio_free(&sf_ctl_dev);
	wakeup_source_trash(&sf_ctl_dev.wakelock);
	sf_platform_exit(&sf_ctl_dev);
	sf_ctl_dev.pdev = NULL;
	return err;
}

static int sf_remove(struct spi_device *spi)
{
	if (sf_ctl_dev.miscdev.this_device)
		sysfs_remove_group(&sf_ctl_dev.miscdev.this_device->kobj,
				   &sf_attr_group);

	fb_unregister_client(&sf_ctl_dev.notifier);

	if (sf_ctl_dev.irq_num > 0 && sf_ctl_dev.irq_is_on) {
		disable_irq_wake(sf_ctl_dev.irq_num);
		free_irq(sf_ctl_dev.irq_num, &sf_ctl_dev);
		sf_ctl_dev.irq_is_on = 0;
	}

	misc_deregister(&sf_ctl_dev.miscdev);
	if (sf_ctl_dev.input) {
		input_unregister_device(sf_ctl_dev.input);
		sf_ctl_dev.input = NULL;
	}

	if (sf_ctl_dev.gpio_free)
		sf_ctl_dev.gpio_free(&sf_ctl_dev);

	wakeup_source_trash(&sf_ctl_dev.wakelock);
	sf_platform_exit(&sf_ctl_dev);
	sf_ctl_dev.pdev = NULL;

	return 0;
}

static struct spi_driver sf_spi_driver = {
	.driver = {
		.name = "sunwave-fp",
		.owner = THIS_MODULE,
	},
	.probe = sf_probe,
	.remove = sf_remove,
};

static int __init sf_ctl_driver_init(void)
{
	int err;

	strlcpy(sf_tee_version, SF_DRV_VERSION, sizeof(sf_tee_version));
	spi_register_board_info(sf_spi_board_info,
				ARRAY_SIZE(sf_spi_board_info));

	err = spi_register_driver(&sf_spi_driver);
	if (err < 0)
		pr_err("[sunwave] spi_register_driver failed (%d)\n", err);

	return err;
}

static void __exit sf_ctl_driver_exit(void)
{
	spi_unregister_driver(&sf_spi_driver);
}

module_init(sf_ctl_driver_init);
module_exit(sf_ctl_driver_exit);

MODULE_DESCRIPTION("Sunwave Fingerprint Control Driver");
MODULE_LICENSE("GPL");
