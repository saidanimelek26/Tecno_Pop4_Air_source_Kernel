#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>

#define CAM_CAL_DRVNAME "CAM_CAL_DRV"
#define CAM_CAL_CLASS_NAME "CAM_CALdrv1"
#define CAM_CALAGIC 'i'

struct legacy_cam_cal_info {
	u32 u4Offset;
	u32 u4Length;
	u8 *pu1Params;
};

struct ext_cam_cal_info {
	u32 u4Offset;
	u32 u4Length;
	u32 sensorID;
	u32 deviceID;
	u8 *pu1Params;
};

#define LEGACY_CAM_CALIOC_S_WRITE _IOW(CAM_CALAGIC, 0, struct legacy_cam_cal_info)
#define LEGACY_CAM_CALIOC_G_READ  _IOWR(CAM_CALAGIC, 5, struct legacy_cam_cal_info)
#define EXT_CAM_CALIOC_G_READ     _IOWR(CAM_CALAGIC, 5, struct ext_cam_cal_info)

u16 gc5035_cam_cal_rg;
u16 gc5035_cam_cal_bg;
EXPORT_SYMBOL(gc5035_cam_cal_rg);
EXPORT_SYMBOL(gc5035_cam_cal_bg);

static dev_t cam_cal_devno;
static struct cdev *cam_cal_cdev;
static struct class *cam_cal_class;

static int cam_cal_drv_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int cam_cal_drv_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long cam_cal_drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 offset = 0;
	u32 length = 0;
	u8 __user *user_buf = NULL;
	u8 *kbuf = NULL;
	int ret = 0;

	if (_IOC_TYPE(cmd) != CAM_CALAGIC)
		return -ENOTTY;

	if (cmd == LEGACY_CAM_CALIOC_S_WRITE) {
		struct legacy_cam_cal_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if (cmd == LEGACY_CAM_CALIOC_G_READ) {
		struct legacy_cam_cal_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		offset = info.u4Offset;
		length = info.u4Length;
		user_buf = (u8 __user *)info.pu1Params;
	} else if (cmd == EXT_CAM_CALIOC_G_READ) {
		struct ext_cam_cal_info info;

		if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
			return -EFAULT;
		offset = info.u4Offset;
		length = info.u4Length;
		user_buf = (u8 __user *)info.pu1Params;
	} else {
		return -EINVAL;
	}

	if (!length || length > 65536 || !user_buf)
		return -EINVAL;

	kbuf = kzalloc(length, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (offset == 0 && length == 4) {
		kbuf[0] = 0x00;
		kbuf[1] = 0x00;
		kbuf[2] = 0x00;
		kbuf[3] = 0x00;
	} else if (offset == 1 && length == 4) {
		kbuf[0] = 0xff;
		kbuf[1] = 0x00;
		kbuf[2] = 0x0b;
		kbuf[3] = 0x01;
	} else if (offset == 8 && length == 1) {
		kbuf[0] = 0x03;
	} else if (offset == 17 && length == 2) {
		kbuf[0] = (u8)(gc5035_cam_cal_rg & 0xff);
		kbuf[1] = (u8)((gc5035_cam_cal_rg >> 8) & 0xff);
	} else if (offset == 19 && length == 2) {
		kbuf[0] = (u8)(gc5035_cam_cal_bg & 0xff);
		kbuf[1] = (u8)((gc5035_cam_cal_bg >> 8) & 0xff);
	}

	if (copy_to_user(user_buf, kbuf, length))
		ret = -EFAULT;
	else
		ret = (int)length;

	kfree(kbuf);
	return ret;
}

static const struct file_operations cam_cal_fops = {
	.owner = THIS_MODULE,
	.open = cam_cal_drv_open,
	.release = cam_cal_drv_release,
	.unlocked_ioctl = cam_cal_drv_ioctl,
};

static int __init cam_cal_drv_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&cam_cal_devno, 0, 1, CAM_CAL_DRVNAME);
	if (ret)
		return ret;

	cam_cal_cdev = cdev_alloc();
	if (!cam_cal_cdev) {
		unregister_chrdev_region(cam_cal_devno, 1);
		return -ENOMEM;
	}

	cdev_init(cam_cal_cdev, &cam_cal_fops);
	cam_cal_cdev->owner = THIS_MODULE;
	ret = cdev_add(cam_cal_cdev, cam_cal_devno, 1);
	if (ret) {
		unregister_chrdev_region(cam_cal_devno, 1);
		return ret;
	}

	cam_cal_class = class_create(THIS_MODULE, CAM_CAL_CLASS_NAME);
	if (IS_ERR(cam_cal_class)) {
		cdev_del(cam_cal_cdev);
		unregister_chrdev_region(cam_cal_devno, 1);
		return PTR_ERR(cam_cal_class);
	}

	device_create(cam_cal_class, NULL, cam_cal_devno, NULL, CAM_CAL_DRVNAME);
	return 0;
}

static void __exit cam_cal_drv_exit(void)
{
	device_destroy(cam_cal_class, cam_cal_devno);
	class_destroy(cam_cal_class);
	cdev_del(cam_cal_cdev);
	unregister_chrdev_region(cam_cal_devno, 1);
}

module_init(cam_cal_drv_init);
module_exit(cam_cal_drv_exit);
MODULE_LICENSE("GPL");
