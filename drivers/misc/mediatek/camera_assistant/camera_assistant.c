/*
 * Camera Assistant subcamera brightness sensor driver (GC6133/GC6153)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <mt-plat/mt_gpio.h>

#define CAM_ASSIST_GPIO_RST (3 | 0x80000000)

struct cam_assist_reg {
	u8 reg;
	u8 val;
};

struct cam_assist_state {
	struct i2c_client *client;
	struct proc_dir_entry *proc_entry;
	struct mutex lock;
	int chip_id;
	int power_on;
	int read_id_ok;
	int low_cnt;
	int brightness;
	u8 last_reg_err;
};

static struct cam_assist_state g_cam_assist = {
	.brightness = 2,
};

static const struct cam_assist_reg gc6133_init_regs[] = {
	{0xfe, 0xa0}, {0xfe, 0xa0}, {0xfe, 0xa0}, {0xf6, 0x00},
	{0xfa, 0x11}, {0xfc, 0x12}, {0xfe, 0x00}, {0x49, 0x70},
	{0x4a, 0x40}, {0x4b, 0x5d}, {0x03, 0x00}, {0x04, 0xfa},
	{0x01, 0x41}, {0x02, 0x12}, {0x0f, 0x01}, {0x0d, 0x30},
	{0x12, 0xc8}, {0x14, 0x54}, {0x15, 0x32}, {0x16, 0x04},
	{0x17, 0x19}, {0x1d, 0xb9}, {0x1f, 0x15}, {0x7a, 0x00},
	{0x7b, 0x14}, {0x7d, 0x36}, {0xfe, 0x10}, {0x20, 0x7e},
	{0x22, 0xb8}, {0x24, 0x54}, {0x26, 0x87}, {0x39, 0x00},
	{0x3a, 0x80}, {0x3b, 0x01}, {0x3c, 0x40}, {0x3e, 0xf0},
	{0x2a, 0x2f}, {0x37, 0x46}, {0x3f, 0x18}, {0x50, 0x3c},
	{0x52, 0x4f}, {0x53, 0x81}, {0x54, 0x43}, {0x56, 0x78},
	{0x57, 0xaa}, {0x58, 0xff}, {0x5b, 0x60}, {0x5c, 0x80},
	{0xab, 0x28}, {0xac, 0xb5}, {0x60, 0x45}, {0x62, 0x68},
	{0x63, 0x13}, {0x64, 0x43}, {0x65, 0x13}, {0x66, 0x26},
	{0x67, 0x07}, {0x68, 0xf5}, {0x69, 0xea}, {0x6a, 0x21},
	{0x6b, 0x21}, {0x6c, 0xe4}, {0x6d, 0xfb}, {0x81, 0x30},
	{0x82, 0x30}, {0x83, 0x4a}, {0x85, 0x06}, {0x8d, 0x78},
	{0x8e, 0x25}, {0x90, 0x38}, {0x92, 0x50}, {0x9d, 0x32},
	{0x9e, 0x61}, {0x9f, 0xf4}, {0xa3, 0x28}, {0xa4, 0x01},
	{0xb1, 0x1e}, {0xb3, 0x20}, {0xbd, 0x70}, {0xbe, 0x58},
	{0xbf, 0xa0}, {0xfe, 0x00}, {0x43, 0xa8}, {0xb0, 0xf2},
	{0xb5, 0x40}, {0xb8, 0x05}, {0xba, 0x60}, {0xfe, 0x02},
	{0x01, 0x01}, {0x02, 0x02}, {0x03, 0x20}, {0x04, 0x20},
	{0x0a, 0x00}, {0x13, 0x10}, {0x24, 0x00}, {0x28, 0x03},
	{0xfe, 0x00}, {0x22, 0xf8},
};

static const struct cam_assist_reg gc6153_init_regs[] = {
	{0xfe, 0xa0}, {0xfe, 0xa0}, {0xfe, 0xa0}, {0xfa, 0x11},
	{0xfc, 0x00}, {0xf6, 0x00}, {0xfc, 0x12}, {0xfe, 0x00},
	{0x01, 0x40}, {0x02, 0x12}, {0x0d, 0x40}, {0x14, 0x7e},
	{0x16, 0x05}, {0x17, 0x18}, {0x1c, 0x31}, {0x1d, 0xb9},
	{0x1f, 0x1a}, {0x73, 0x20}, {0x74, 0x71}, {0x77, 0x22},
	{0x7a, 0x08}, {0x11, 0x18}, {0x13, 0x48}, {0x12, 0xc8},
	{0x70, 0xc8}, {0x7b, 0x18}, {0x7d, 0x30}, {0x7e, 0x02},
	{0xfe, 0x10}, {0xfe, 0x00}, {0xfe, 0x00}, {0xfe, 0x00},
	{0xfe, 0x00}, {0xfe, 0x00}, {0xfe, 0x10}, {0xfe, 0x00},
	{0x49, 0x61}, {0x4a, 0x40}, {0x4b, 0x58}, {0xfe, 0x00},
	{0x39, 0x02}, {0x3a, 0x80}, {0x20, 0xfe}, {0x47, 0x0a},
	{0x4c, 0x18}, {0x4d, 0x2e}, {0x4e, 0x40}, {0x4f, 0x50},
	{0x26, 0x87}, {0x33, 0x10}, {0x37, 0x06}, {0x2a, 0x21},
	{0x3f, 0x16}, {0x52, 0xa6}, {0x53, 0x84}, {0x54, 0x43},
	{0x56, 0x78}, {0x57, 0xaa}, {0x58, 0xff}, {0x5b, 0x90},
	{0x5c, 0x80}, {0xab, 0x2a}, {0xac, 0xb5}, {0x5e, 0x06},
	{0x5f, 0x06}, {0x23, 0x80}, {0x60, 0x42}, {0x61, 0xff},
	{0x62, 0x84}, {0x63, 0x15}, {0x64, 0x53}, {0x65, 0x15},
	{0x66, 0x28}, {0x67, 0x03}, {0x68, 0xf4}, {0x69, 0xe6},
	{0x6a, 0x26}, {0x6b, 0x22}, {0x6c, 0xe7}, {0x6d, 0xf7},
	{0x81, 0x42}, {0x82, 0x42}, {0x83, 0x46}, {0x84, 0x90},
	{0x86, 0xf0}, {0x87, 0x1d}, {0x88, 0x16}, {0x8d, 0x00},
	{0x8e, 0x25}, {0x90, 0x36}, {0x92, 0x45}, {0x9d, 0x32},
	{0x9e, 0x81}, {0x9f, 0xf4}, {0xa0, 0xa0}, {0xa1, 0x01},
	{0xa3, 0x2d}, {0xa4, 0x01}, {0xb0, 0xc2}, {0xb1, 0x1e},
	{0xb2, 0x10}, {0xb3, 0x1b}, {0xb4, 0x12}, {0xb5, 0x1b},
	{0xb6, 0x2e}, {0xb7, 0x20}, {0xba, 0x40}, {0xbb, 0x61},
	{0xbd, 0x70}, {0xbe, 0x42}, {0xbf, 0x68}, {0xc4, 0xe4},
	{0xc5, 0x16}, {0xc6, 0x16}, {0xc7, 0xea}, {0xc8, 0xe4},
	{0xc9, 0x16}, {0xca, 0x16}, {0xcb, 0xea}, {0x46, 0x84},
	{0x48, 0x83}, {0x22, 0xf8}, {0xfe, 0x02}, {0x01, 0x01},
	{0x02, 0x02}, {0x03, 0x20}, {0x04, 0x20}, {0x0a, 0x00},
	{0x13, 0x10}, {0x24, 0x00}, {0x28, 0x03}, {0xfe, 0x00},
	{0xf2, 0x03}, {0xfe, 0x00},
};

static int camera_assistant_I2C_read_reg(u8 reg)
{
	struct i2c_client *client = g_cam_assist.client;
	u8 rxbuf[32] = {0};
	struct i2c_msg msgs[2];
	int ret;

	if (!client)
		return -ENODEV;

	memset(msgs, 0, sizeof(msgs));
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = rxbuf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	g_cam_assist.last_reg_err = 0;
	if (ret < 0)
		return ret;

	return rxbuf[0];
}

static int CamWriteCmosSensor(u8 reg, u8 val)
{
	struct i2c_client *client = g_cam_assist.client;
	u8 txbuf[2];
	struct i2c_msg msg;

	if (!client)
		return -ENODEV;

	txbuf[0] = reg;
	txbuf[1] = val;
	memset(&msg, 0, sizeof(msg));
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = txbuf;

	return i2c_transfer(client->adapter, &msg, 1);
}

int camera_assistant_get_brightness(void)
{
	int val, id;

	if (!g_cam_assist.power_on) {
		pr_debug("camera_assistant_get_brightness powerstatusA=0\n");
		return 255;
	}

	if (g_cam_assist.chip_id != 0x6133 && g_cam_assist.chip_id != 0x6153)
		return 0;

	val = camera_assistant_I2C_read_reg(0x93);
	if (val == 0) {
		id = camera_assistant_I2C_read_reg(0xf0);
		pr_debug("camera_assistant_get_brightness readIDcheck=%d\n", id);
		if (id == 0xba || id == 0x61) {
			g_cam_assist.brightness = 1;
			if (g_cam_assist.read_id_ok == 0)
				return 0;
		} else {
			g_cam_assist.read_id_ok = val;
			g_cam_assist.brightness = 2;
		}
	} else if (val > 9) {
		g_cam_assist.low_cnt = 0;
		g_cam_assist.brightness = 0;
	} else {
		if (g_cam_assist.low_cnt <= 3) {
			g_cam_assist.low_cnt++;
			g_cam_assist.brightness = 0;
		} else {
			g_cam_assist.brightness = 1;
		}
	}

	if ((val | g_cam_assist.last_reg_err) == 0)
		g_cam_assist.brightness = 2;

	pr_debug("camera_assistant_get_brightness reg93=%d\n", val);
	return val;
}

int camera_assistant_power_on(void)
{
	int retry = 10;
	int id = 0;
	size_t i;

	pr_debug("camera_assistant_power_on\n");
	mt_set_gpio_mode(CAM_ASSIST_GPIO_RST, GPIO_MODE_00);
	mt_set_gpio_dir(CAM_ASSIST_GPIO_RST, GPIO_DIR_OUT);
	mt_set_gpio_out(CAM_ASSIST_GPIO_RST, GPIO_OUT_ONE);
	usleep_range(5000, 6000);
	mt_set_gpio_out(CAM_ASSIST_GPIO_RST, GPIO_OUT_ZERO);
	usleep_range(5000, 6000);
	mt_set_gpio_out(CAM_ASSIST_GPIO_RST, GPIO_OUT_ONE);
	usleep_range(5000, 6000);
	mt_set_gpio_out(CAM_ASSIST_GPIO_RST, GPIO_OUT_ZERO);
	msleep(10);

	while (retry-- > 0) {
		id = camera_assistant_I2C_read_reg(0xf0);
		pr_debug("camera_assistant_power_on regID=%x\n", id);
		if (id == 0xba) {
			g_cam_assist.chip_id = 0x6133;
			break;
		} else if (id == 0x61) {
			g_cam_assist.chip_id = 0x6153;
			break;
		}
		usleep_range(5000, 6000);
	}

	if (g_cam_assist.chip_id == 0x6133) {
		pr_debug("GC6133_SERIALInitialSetting\n");
		for (i = 0; i < ARRAY_SIZE(gc6133_init_regs); i++)
			CamWriteCmosSensor(gc6133_init_regs[i].reg, gc6133_init_regs[i].val);
	} else if (g_cam_assist.chip_id == 0x6153) {
		for (i = 0; i < ARRAY_SIZE(gc6153_init_regs); i++)
			CamWriteCmosSensor(gc6153_init_regs[i].reg, gc6153_init_regs[i].val);
	} else {
		return 0;
	}

	g_cam_assist.power_on = 1;
	g_cam_assist.read_id_ok = 1;
	camera_assistant_get_brightness();
	return 1;
}
EXPORT_SYMBOL(camera_assistant_power_on);

void camera_assistant_power_off(void)
{
	pr_debug("camera_assistant_power_off\n");
	g_cam_assist.power_on = 0;
	g_cam_assist.read_id_ok = 0;
	g_cam_assist.brightness = 2;
}
EXPORT_SYMBOL(camera_assistant_power_off);

static int camera_assistant_proc_show(struct seq_file *m, void *v)
{
	camera_assistant_get_brightness();
	seq_printf(m, "%d\n", g_cam_assist.brightness);
	return 0;
}

static int camera_assistant_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, camera_assistant_proc_show, NULL);
}

static const struct file_operations camera_assistant_proc_fops = {
	.owner = THIS_MODULE,
	.open = camera_assistant_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int camera_assistant_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	pr_info("camera_assistant_probe start1--->\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("camera_assistant i2c functionality check fail.\n");
		return -ENODEV;
	}

	g_cam_assist.client = client;
	pr_info("camera_assistant_i2c_client->addr = %x !!\n", client->addr);

	g_cam_assist.proc_entry = proc_create_data("subcamera_brightness", 0666,
					NULL, &camera_assistant_proc_fops, NULL);
	if (!g_cam_assist.proc_entry)
		pr_err("proc_create_data subcamera_brightness failed\n");

	pr_info("camera_assistant Initializing is done\n");
	return 0;
}

static int camera_assistant_remove(struct i2c_client *client)
{
	if (g_cam_assist.proc_entry)
		proc_remove(g_cam_assist.proc_entry);
	g_cam_assist.client = NULL;
	return 0;
}

static const struct i2c_device_id camera_assistant_id[] = {
	{ "camera_assistant", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, camera_assistant_id);

static const struct of_device_id camera_assistant_of_match[] = {
	{ .compatible = "mediatek,camera_assistant" },
	{ }
};
MODULE_DEVICE_TABLE(of, camera_assistant_of_match);

static struct i2c_driver camera_assistant_driver = {
	.driver = {
		.name = "camera_assistant",
		.of_match_table = camera_assistant_of_match,
	},
	.probe = camera_assistant_probe,
	.remove = camera_assistant_remove,
	.id_table = camera_assistant_id,
};

static int __init camera_assistant_init(void)
{
	return i2c_add_driver(&camera_assistant_driver);
}

static void __exit camera_assistant_exit(void)
{
	i2c_del_driver(&camera_assistant_driver);
}

module_init(camera_assistant_init);
module_exit(camera_assistant_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Camera Assistant Subcamera Driver");
