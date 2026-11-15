#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>

#include "ff_ctl.h"
#include "ff_log.h"
#include "ff_err.h"

#ifdef CONFIG_SUNWAVE_FP
extern int g_sunwave_matched;
#endif

#define FF_CTL_DRV_NAME "focaltech_fp"
#define FF_CTL_DRV_VER  "2.2.0"

typedef struct {
	struct miscdevice miscdev;
	int irq_num;
	struct work_struct work_queue;
	struct fasync_struct *async_queue;
	struct input_dev *input;
	struct notifier_block fb_notifier;
	struct wakeup_source wake_lock;
	bool b_driver_inited;
	bool b_config_dirtied;
	bool b_irq_enabled;
	ff_driver_config_t config;
	char ic_info[128];
} ff_ctl_context_t;

ff_log_level_t g_log_level = FF_LOG_LEVEL_INF;
static ff_ctl_context_t *g_context;

int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int ret;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	switch (level) {
	case FF_LOG_LEVEL_ERR:
		ret = printk(KERN_ERR "[%s] %pV\n", tag, &vaf);
		break;
	case FF_LOG_LEVEL_WRN:
		ret = printk(KERN_WARNING "[%s] %pV\n", tag, &vaf);
		break;
	case FF_LOG_LEVEL_INF:
		ret = printk(KERN_INFO "[%s] %pV\n", tag, &vaf);
		break;
	default:
		ret = printk(KERN_DEBUG "[%s] %pV\n", tag, &vaf);
		break;
	}

	va_end(args);
	return ret;
}

const char *ff_err_strerror(int err)
{
	switch (err) {
	case FF_SUCCESS:
		return "Success";
	case FF_ERR_INTERNAL:
		return "Internal error";
	case FF_ERR_BAD_PARAMS:
		return "Bad parameters";
	case FF_ERR_NULL_PTR:
		return "Null pointer";
	case FF_ERR_NOENT:
		return "No such entry";
	default:
		return "Error";
	}
}

static int ff_ctl_enable_irq(bool on)
{
	if (!g_context || g_context->irq_num <= 0)
		return -ENODEV;

	if (on && !g_context->b_irq_enabled) {
		enable_irq(g_context->irq_num);
		g_context->b_irq_enabled = true;
	} else if (!on && g_context->b_irq_enabled) {
		disable_irq_nosync(g_context->irq_num);
		g_context->b_irq_enabled = false;
	}

	return 0;
}

static void ff_ctl_device_event(struct work_struct *ws)
{
	ff_ctl_context_t *ctx = container_of(ws, ff_ctl_context_t, work_queue);
	char *uevent_env[2] = { "FF_INTERRUPT=1", NULL };

	__pm_wakeup_event(&ctx->wake_lock, msecs_to_jiffies(5000));
	if (ctx->miscdev.this_device)
		kobject_uevent_env(&ctx->miscdev.this_device->kobj,
				   KOBJ_CHANGE, uevent_env);
}

static irqreturn_t ff_ctl_device_irq(int irq, void *dev_id)
{
	ff_ctl_context_t *ctx = (ff_ctl_context_t *)dev_id;

	disable_irq_nosync(irq);
	ctx->b_irq_enabled = false;
	schedule_work(&ctx->work_queue);

	return IRQ_HANDLED;
}

static int ff_ctl_fb_notifier_callback(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	ff_ctl_context_t *ctx = container_of(nb, ff_ctl_context_t, fb_notifier);
	struct fb_event *event = (struct fb_event *)data;
	int *blank;
	char *uevent_env[2];

	if (action != FB_EVENT_BLANK || !event || !event->data)
		return 0;

	blank = (int *)event->data;
	if (*blank == FB_BLANK_UNBLANK)
		uevent_env[0] = "FF_SCREEN_ON=1";
	else if (*blank == FB_BLANK_POWERDOWN)
		uevent_env[0] = "FF_SCREEN_OFF=1";
	else
		return 0;

	uevent_env[1] = NULL;
	if (ctx->miscdev.this_device)
		kobject_uevent_env(&ctx->miscdev.this_device->kobj,
				   KOBJ_CHANGE, uevent_env);

	return 0;
}

static int ff_ctl_register_input(void)
{
	struct input_dev *input;
	int err;

	input = input_allocate_device();
	if (!input)
		return -ENOMEM;

	input->name = "ff_key";
	__set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_HOMEPAGE, input->keybit);
	__set_bit(KEY_BACK, input->keybit);
	__set_bit(KEY_MENU, input->keybit);
	__set_bit(KEY_POWER, input->keybit);
	__set_bit(KEY_UP, input->keybit);
	__set_bit(KEY_DOWN, input->keybit);
	__set_bit(KEY_LEFT, input->keybit);
	__set_bit(KEY_RIGHT, input->keybit);
	__set_bit(KEY_ENTER, input->keybit);
	__set_bit(KEY_F11, input->keybit);

	err = input_register_device(input);
	if (err) {
		input_free_device(input);
		return err;
	}

	g_context->input = input;
	return 0;
}

static int ff_ctl_free_driver(void)
{
	int err;

	if (!g_context)
		return -ENOSYS;

	fb_unregister_client(&g_context->fb_notifier);

	if (g_context->input) {
		input_unregister_device(g_context->input);
		g_context->input = NULL;
	}

	if (g_context->irq_num > 0) {
		disable_irq_wake(g_context->irq_num);
		free_irq(g_context->irq_num, (void *)g_context);
		g_context->irq_num = -1;
	}

	err = ff_ctl_free_pins();
	g_context->b_driver_inited = false;
	return err;
}

static int ff_ctl_init_driver(void)
{
	int err;

	if (!g_context)
		return -ENOSYS;

	err = ff_ctl_init_pins(&g_context->irq_num);
	if (err) {
		FF_LOGE("ff_ctl_init_pins(..) = %d.", err);
		ff_ctl_free_driver();
		return err;
	}

	err = request_irq(g_context->irq_num, ff_ctl_device_irq,
			  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			  "ff_irq", (void *)g_context);
	if (err) {
		FF_LOGE("request_irq(..) = %d.", err);
		ff_ctl_free_driver();
		return err;
	}

	g_context->b_irq_enabled = true;
	err = enable_irq_wake(g_context->irq_num);
	if (err)
		FF_LOGW("enable_irq_wake(%d) = %d.", g_context->irq_num, err);

	err = ff_ctl_register_input();
	if (err)
		FF_LOGE("ff_ctl_register_input() = %d.", err);

	ff_ctl_enable_power(true);
	udelay(1000);
	ff_ctl_enable_spiclk(true);

	g_context->fb_notifier.notifier_call = ff_ctl_fb_notifier_callback;
	fb_register_client(&g_context->fb_notifier);

	g_context->b_driver_inited = true;
	return 0;
}

static int ff_ctl_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ff_ctl_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ff_ctl_fasync(int fd, struct file *filp, int mode)
{
	return 0;
}

static long ff_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct miscdevice *dev = (struct miscdevice *)filp->private_data;
	ff_ctl_context_t *ctx = container_of(dev, ff_ctl_context_t, miscdev);

	switch (cmd) {
	case FF_IOC_INIT_DRIVER:
		if (ctx->b_driver_inited)
			err = ff_ctl_free_driver();
		if (!err)
			err = ff_ctl_init_driver();
		break;

	case FF_IOC_FREE_DRIVER:
		if (ctx->b_driver_inited)
			err = ff_ctl_free_driver();
		break;

	case FF_IOC_RESET_DEVICE:
		err = ff_ctl_reset_device();
		break;

	case FF_IOC_ENABLE_IRQ:
		err = ff_ctl_enable_irq(true);
		break;

	case FF_IOC_DISABLE_IRQ:
		err = ff_ctl_enable_irq(false);
		break;

	case FF_IOC_ENABLE_SPI_CLK:
		err = ff_ctl_enable_spiclk(true);
		break;

	case FF_IOC_DISABLE_SPI_CLK:
		err = ff_ctl_enable_spiclk(false);
		break;

	case FF_IOC_ENABLE_POWER:
		err = ff_ctl_enable_power(true);
		break;

	case FF_IOC_DISABLE_POWER:
		err = ff_ctl_enable_power(false);
		break;

	case FF_IOC_REPORT_KEY_EVENT: {
		ff_key_event_t kevent;

		if (copy_from_user(&kevent, (void __user *)arg,
				   sizeof(kevent))) {
			err = -EFAULT;
			break;
		}
		if (ctx->input) {
			input_report_key(ctx->input, kevent.code,
					 kevent.value);
			input_sync(ctx->input);
		}
		break;
	}

	case FF_IOC_SYNC_CONFIG:
		if (copy_from_user(&ctx->config, (void __user *)arg,
				   sizeof(ctx->config)))
			err = -EFAULT;
		break;

	case FF_IOC_GET_VERSION: {
		char ver[128];

		snprintf(ver, sizeof(ver), "%s (%s)",
			 FF_CTL_DRV_VER, ff_ctl_arch_str());
		if (copy_to_user((void __user *)arg, ver, sizeof(ver)))
			err = -EFAULT;
		break;
	}

	case FF_IOC_SET_IC_INFORMATION:
		if (copy_from_user(ctx->ic_info, (void __user *)arg,
				   sizeof(ctx->ic_info)))
			err = -EFAULT;
		break;

	case FF_IOC_GET_IC_INFORMATION:
		if (copy_to_user((void __user *)arg, ctx->ic_info,
				 sizeof(ctx->ic_info)))
			err = -EFAULT;
		break;

	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct file_operations ff_ctl_fops = {
	.owner = THIS_MODULE,
	.open = ff_ctl_open,
	.release = ff_ctl_release,
	.fasync = ff_ctl_fasync,
	.unlocked_ioctl = ff_ctl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ff_ctl_ioctl,
#endif
};

static int __init ff_ctl_driver_init(void)
{
	int err;

	FF_LOGV("'%s' enter.", __func__);

#ifdef CONFIG_SUNWAVE_FP
	if (g_sunwave_matched == 1 || g_focaltech_chip_id == 0) {
		FF_LOGI("FocalTech skipped (sunwave=%d, chip_id=0x%x).",
			g_sunwave_matched, g_focaltech_chip_id);
		return 0;
	}
#endif

	g_context = kzalloc(sizeof(*g_context), GFP_KERNEL);
	if (!g_context)
		return -ENOMEM;

	g_context->miscdev.minor = MISC_DYNAMIC_MINOR;
	g_context->miscdev.name = FF_CTL_DRV_NAME;
	g_context->miscdev.fops = &ff_ctl_fops;

	err = misc_register(&g_context->miscdev);
	if (err) {
		FF_LOGE("misc_register(..) = %d.", err);
		kfree(g_context);
		g_context = NULL;
		return err;
	}

	g_context->irq_num = -1;
	INIT_WORK(&g_context->work_queue, ff_ctl_device_event);
	wakeup_source_init(&g_context->wake_lock, "ff_wake_lock");

	err = ff_ctl_init_driver();
	if (err) {
		wakeup_source_trash(&g_context->wake_lock);
		misc_deregister(&g_context->miscdev);
		kfree(g_context);
		g_context = NULL;
		return err;
	}

	FF_LOGI("FocalTech fingerprint device control driver registered.");
	FF_LOGV("'%s' leave.", __func__);
	return 0;
}

static void __exit ff_ctl_driver_exit(void)
{
	if (g_context) {
		ff_ctl_free_driver();
		wakeup_source_trash(&g_context->wake_lock);
		misc_deregister(&g_context->miscdev);
		kfree(g_context);
		g_context = NULL;
	}
}

late_initcall(ff_ctl_driver_init);
module_exit(ff_ctl_driver_exit);

MODULE_DESCRIPTION("FocalTech Fingerprint Control Driver");
MODULE_LICENSE("GPL v2");
