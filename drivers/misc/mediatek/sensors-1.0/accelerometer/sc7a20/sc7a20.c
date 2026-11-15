#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include <cust_acc.h>
#include <accel.h>
#include "sc7a20.h"

#define GSE_TAG                  "[SC7A20] "
#define GSE_FUN()                pr_debug(GSE_TAG "%s\n", __func__)
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG "%s %d: " fmt, __func__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_info(GSE_TAG fmt, ##args)

struct sc7a20_i2c_data {
	struct i2c_client *client;
	struct acc_hw hw;
	struct hwmsen_convert cvt;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[SC7A20_AXES_NUM];
	s16 data[SC7A20_AXES_NUM];
	bool sensor_power;
	bool enable;
	int sensitivity;
};

static struct i2c_client *sc7a20_i2c_client;
static struct sc7a20_i2c_data *obj_i2c_data;
static int sc7a20_init_flag = -1;
static DEFINE_MUTEX(sc7a20_i2c_mutex);

static int sc7a20_local_init(void);
static int sc7a20_remove(void);

static struct acc_init_info sc7a20_init_info = {
	.name = "sc7a20",
	.init = sc7a20_local_init,
	.uninit = sc7a20_remove,
};

static int sc7a20_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	if (!client || !data)
		return -EINVAL;

	mutex_lock(&sc7a20_i2c_mutex);
	err = i2c_transfer(client->adapter, msgs, 2);
	mutex_unlock(&sc7a20_i2c_mutex);
	if (err != 2)
		return err < 0 ? err : -EIO;
	return 0;
}

static int sc7a20_i2c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int err;
	u8 buf[2] = { reg, val };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

	if (!client)
		return -EINVAL;

	mutex_lock(&sc7a20_i2c_mutex);
	err = i2c_transfer(client->adapter, &msg, 1);
	mutex_unlock(&sc7a20_i2c_mutex);
	if (err != 1)
		return err < 0 ? err : -EIO;
	return 0;
}

static int SC7A20_CheckDeviceID(struct i2c_client *client)
{
	u8 chip_id = 0;
	int err;

	err = sc7a20_i2c_read_block(client, SC7A20_REG_WHO_AM_I, &chip_id, 1);
	if (err < 0)
		return err;

	GSE_LOG("WHO_AM_I = 0x%02x at addr 0x%02x\n", chip_id, client->addr);
	if (chip_id != SC7A20_CHIP_ID)
		return SC7A20_ERR_IDENTIFICATION;

	return SC7A20_SUCCESS;
}

static int SC7A20_SetPowerMode(struct i2c_client *client, bool enable)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	u8 ctrl1;

	if (enable == obj->sensor_power)
		return 0;

	ctrl1 = enable ? 0x57 : 0x07;
	err = sc7a20_i2c_write_reg(client, SC7A20_REG_CTRL1, ctrl1);
	if (err)
		return err;

	obj->sensor_power = enable;
	if (enable)
		msleep(20);
	return 0;
}

static int sc7a20_init_client(struct i2c_client *client, int reset_cali)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	err = SC7A20_CheckDeviceID(client);
	if (err)
		return err;

	err = sc7a20_i2c_write_reg(client, SC7A20_REG_CTRL2, 0x00);
	if (err)
		return err;
	err = sc7a20_i2c_write_reg(client, SC7A20_REG_CTRL3, 0x00);
	if (err)
		return err;
	err = sc7a20_i2c_write_reg(client, SC7A20_REG_CTRL4, 0x88);
	if (err)
		return err;

	obj->sensitivity = 1024;
	err = SC7A20_SetPowerMode(client, false);
	if (err)
		return err;

	if (reset_cali)
		memset(obj->cali_sw, 0, sizeof(obj->cali_sw));

	return 0;
}

static int SC7A20_ReadData(struct i2c_client *client, s16 data[SC7A20_AXES_NUM])
{
	u8 buf[SC7A20_DATA_LEN] = { 0 };
	int err;

	err = sc7a20_i2c_read_block(client, SC7A20_REG_OUT_X_L | 0x80, buf, SC7A20_DATA_LEN);
	if (err)
		return err;

	data[SC7A20_AXIS_X] = ((s16)((buf[1] << 8) | buf[0])) >> 4;
	data[SC7A20_AXIS_Y] = ((s16)((buf[3] << 8) | buf[2])) >> 4;
	data[SC7A20_AXIS_Z] = ((s16)((buf[5] << 8) | buf[4])) >> 4;
	return 0;
}

static int SC7A20_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);
	s16 raw[SC7A20_AXES_NUM];
	int acc[SC7A20_AXES_NUM];
	int err;

	if (!buf || !obj)
		return -EINVAL;

	if (!obj->sensor_power) {
		err = SC7A20_SetPowerMode(client, true);
		if (err)
			return err;
	}

	err = SC7A20_ReadData(client, raw);
	if (err)
		return err;

	raw[SC7A20_AXIS_X] += obj->cali_sw[SC7A20_AXIS_X];
	raw[SC7A20_AXIS_Y] += obj->cali_sw[SC7A20_AXIS_Y];
	raw[SC7A20_AXIS_Z] += obj->cali_sw[SC7A20_AXIS_Z];

	acc[obj->cvt.map[SC7A20_AXIS_X]] = obj->cvt.sign[SC7A20_AXIS_X] * raw[SC7A20_AXIS_X];
	acc[obj->cvt.map[SC7A20_AXIS_Y]] = obj->cvt.sign[SC7A20_AXIS_Y] * raw[SC7A20_AXIS_Y];
	acc[obj->cvt.map[SC7A20_AXIS_Z]] = obj->cvt.sign[SC7A20_AXIS_Z] * raw[SC7A20_AXIS_Z];

	acc[SC7A20_AXIS_X] = acc[SC7A20_AXIS_X] * GRAVITY_EARTH_1000 / obj->sensitivity;
	acc[SC7A20_AXIS_Y] = acc[SC7A20_AXIS_Y] * GRAVITY_EARTH_1000 / obj->sensitivity;
	acc[SC7A20_AXIS_Z] = acc[SC7A20_AXIS_Z] * GRAVITY_EARTH_1000 / obj->sensitivity;

	snprintf(buf, bufsize, "%04x %04x %04x", acc[SC7A20_AXIS_X], acc[SC7A20_AXIS_Y], acc[SC7A20_AXIS_Z]);
	return 0;
}

static int sc7a20_open_report_data(int open)
{
	return 0;
}

static int sc7a20_enable_nodata(int en)
{
	if (!sc7a20_i2c_client || !obj_i2c_data)
		return -EINVAL;
	obj_i2c_data->enable = (en != 0);
	return SC7A20_SetPowerMode(sc7a20_i2c_client, en != 0);
}

static int sc7a20_set_delay(u64 ns)
{
	u8 odr = 0x57;

	if (!sc7a20_i2c_client)
		return -EINVAL;
	if (ns >= 200000000ULL)
		odr = 0x27;
	else if (ns >= 40000000ULL)
		odr = 0x37;
	else if (ns >= 20000000ULL)
		odr = 0x47;
	else if (ns >= 10000000ULL)
		odr = 0x57;
	else
		odr = 0x67;

	if (obj_i2c_data && obj_i2c_data->sensor_power)
		return sc7a20_i2c_write_reg(sc7a20_i2c_client, SC7A20_REG_CTRL1, odr);
	return 0;
}

static int sc7a20_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return sc7a20_set_delay((u64)samplingPeriodNs);
}

static int sc7a20_flush(void)
{
	return acc_flush_report();
}

static int sc7a20_get_data(int *x, int *y, int *z, int *status)
{
	char buff[SC7A20_BUFSIZE];
	int err;

	if (!sc7a20_i2c_client)
		return -EINVAL;
	err = SC7A20_ReadSensorData(sc7a20_i2c_client, buff, sizeof(buff));
	if (err)
		return err;
	if (sscanf(buff, "%x %x %x", x, y, z) != 3)
		return -EINVAL;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}

static int sc7a20_factory_enable_sensor(bool enable_disable, int64_t sample_periods_ms)
{
	return sc7a20_enable_nodata(enable_disable ? 1 : 0);
}

static int sc7a20_factory_get_data(int32_t data[3], int *status)
{
	return sc7a20_get_data(&data[0], &data[1], &data[2], status);
}

static int sc7a20_factory_get_raw_data(int32_t data[3])
{
	s16 raw[SC7A20_AXES_NUM];
	int err;

	if (!sc7a20_i2c_client)
		return -EINVAL;
	err = SC7A20_ReadData(sc7a20_i2c_client, raw);
	if (err)
		return err;
	data[0] = raw[0];
	data[1] = raw[1];
	data[2] = raw[2];
	return 0;
}

static int sc7a20_factory_enable_calibration(void)
{
	return 0;
}

static int sc7a20_factory_clear_cali(void)
{
	if (!obj_i2c_data)
		return -EINVAL;
	memset(obj_i2c_data->cali_sw, 0, sizeof(obj_i2c_data->cali_sw));
	return 0;
}

static int sc7a20_factory_set_cali(int32_t data[3])
{
	struct sc7a20_i2c_data *obj = obj_i2c_data;
	int raw_cali[3];

	if (!obj)
		return -EINVAL;
	raw_cali[0] = data[0] * obj->sensitivity / GRAVITY_EARTH_1000;
	raw_cali[1] = data[1] * obj->sensitivity / GRAVITY_EARTH_1000;
	raw_cali[2] = data[2] * obj->sensitivity / GRAVITY_EARTH_1000;

	obj->cali_sw[obj->cvt.map[0]] += obj->cvt.sign[0] * raw_cali[0];
	obj->cali_sw[obj->cvt.map[1]] += obj->cvt.sign[1] * raw_cali[1];
	obj->cali_sw[obj->cvt.map[2]] += obj->cvt.sign[2] * raw_cali[2];
	return 0;
}

static int sc7a20_factory_get_cali(int32_t data[3])
{
	struct sc7a20_i2c_data *obj = obj_i2c_data;
	int raw_cali[3];

	if (!obj)
		return -EINVAL;
	raw_cali[0] = obj->cvt.sign[0] * obj->cali_sw[obj->cvt.map[0]];
	raw_cali[1] = obj->cvt.sign[1] * obj->cali_sw[obj->cvt.map[1]];
	raw_cali[2] = obj->cvt.sign[2] * obj->cali_sw[obj->cvt.map[2]];

	data[0] = raw_cali[0] * GRAVITY_EARTH_1000 / obj->sensitivity;
	data[1] = raw_cali[1] * GRAVITY_EARTH_1000 / obj->sensitivity;
	data[2] = raw_cali[2] * GRAVITY_EARTH_1000 / obj->sensitivity;
	return 0;
}

static int sc7a20_factory_do_self_test(void)
{
	return 0;
}

static struct accel_factory_fops sc7a20_factory_fops = {
	.enable_sensor = sc7a20_factory_enable_sensor,
	.get_data = sc7a20_factory_get_data,
	.get_raw_data = sc7a20_factory_get_raw_data,
	.enable_calibration = sc7a20_factory_enable_calibration,
	.clear_cali = sc7a20_factory_clear_cali,
	.set_cali = sc7a20_factory_set_cali,
	.get_cali = sc7a20_factory_get_cali,
	.do_self_test = sc7a20_factory_do_self_test,
};

static struct accel_factory_public sc7a20_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &sc7a20_factory_fops,
};

static int __maybe_unused sc7a20_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);

	if (obj && msg.event == PM_EVENT_SUSPEND) {
		atomic_set(&obj->suspend, 1);
		SC7A20_SetPowerMode(client, false);
	}
	return 0;
}

static int __maybe_unused sc7a20_resume(struct i2c_client *client)
{
	struct sc7a20_i2c_data *obj = i2c_get_clientdata(client);

	if (obj) {
		atomic_set(&obj->suspend, 0);
		if (obj->enable)
			SC7A20_SetPowerMode(client, true);
	}
	return 0;
}

static int sc7a20_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct sc7a20_i2c_data *obj;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	u16 orig_addr = client->addr;
	int err;

	GSE_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		sc7a20_init_flag = -1;
		return -ENOMEM;
	}

	err = get_accel_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		GSE_ERR("get dts info fail\n");
		goto exit_kfree;
	}

	err = hwmsen_get_convert(obj->hw.direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw.direction);
		goto exit_kfree;
	}

	obj->client = client;
	i2c_set_clientdata(client, obj);
	obj_i2c_data = obj;
	sc7a20_i2c_client = client;

	client->addr = SC7A20_I2C_SLAVE_ADDR_LOW;
	err = sc7a20_init_client(client, 1);
	if (err) {
		client->addr = SC7A20_I2C_SLAVE_ADDR_HIGH;
		err = sc7a20_init_client(client, 1);
		if (err) {
			client->addr = orig_addr;
			goto exit_kfree;
		}
	}

	err = accel_factory_device_register(&sc7a20_factory_device);
	if (err)
		goto exit_kfree;

	ctl.open_report_data = sc7a20_open_report_data;
	ctl.enable_nodata = sc7a20_enable_nodata;
	ctl.set_delay = sc7a20_set_delay;
	ctl.batch = sc7a20_batch;
	ctl.flush = sc7a20_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	err = acc_register_control_path(&ctl);
	if (err)
		goto exit_factory;

	data.get_data = sc7a20_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err)
		goto exit_factory;

	sc7a20_init_flag = 0;
	{
		extern void hw_info_set_gsensor(unsigned int idx);
		hw_info_set_gsensor(1);
	}
	GSE_LOG("sc7a20_i2c_probe success at 0x%02x\n", client->addr);
	return 0;

exit_factory:
	accel_factory_device_deregister(&sc7a20_factory_device);
exit_kfree:
	sc7a20_i2c_client = NULL;
	obj_i2c_data = NULL;
	kfree(obj);
	sc7a20_init_flag = -1;
	return err ? err : -ENODEV;
}

static int sc7a20_i2c_remove(struct i2c_client *client)
{
	sc7a20_i2c_client = NULL;
	obj_i2c_data = NULL;
	accel_factory_device_deregister(&sc7a20_factory_device);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id sc7a20_i2c_id[] = { { SC7A20_DEV_NAME, 0 }, {} };

static const struct of_device_id sc7a20_of_match[] = {
	{ .compatible = "mediatek,gsensor" },
	{},
};

static struct i2c_driver sc7a20_i2c_driver = {
	.driver = {
		.name = SC7A20_DEV_NAME,
		.of_match_table = sc7a20_of_match,
	},
	.probe = sc7a20_i2c_probe,
	.remove = sc7a20_i2c_remove,
	.id_table = sc7a20_i2c_id,
};

static int sc7a20_local_init(void)
{
	if (i2c_add_driver(&sc7a20_i2c_driver))
		return -1;
	if (sc7a20_init_flag == -1) {
		i2c_del_driver(&sc7a20_i2c_driver);
		return -1;
	}
	return 0;
}

static int sc7a20_remove(void)
{
	i2c_del_driver(&sc7a20_i2c_driver);
	return 0;
}

static int __init sc7a20_driver_init(void)
{
	acc_driver_add(&sc7a20_init_info);
	return 0;
}

static void __exit sc7a20_driver_exit(void)
{
}

module_init(sc7a20_driver_init);
module_exit(sc7a20_driver_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SC7A20 I2C accelerometer driver");
