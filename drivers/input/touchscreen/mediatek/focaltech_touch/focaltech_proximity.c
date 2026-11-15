#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "focaltech_core.h"

#if FTS_PSENSOR_EN
#include "alsps.h"

struct fts_proximity_st {
	u8 mode: 1;
	u8 detect: 1;
	u8 unused: 6;
};

static struct fts_proximity_st fts_proximity_data;
static struct task_struct *fts_ps_thread;
static wait_queue_head_t fts_ps_wait;
static int fts_ps_enable;
static int fts_ps_delay;

static int fts_enter_proximity_mode(int mode);

static int ps_open_report_data(int open)
{
	return 0;
}

static int ps_set_delay(u64 delay)
{
	return 0;
}

static int ps_batch(int flag, int64_t samplingPeriodNs,
		    int64_t maxBatchReportLatencyNs)
{
	return 0;
}

static int ps_flush(void)
{
	return 0;
}

static int ps_get_data(int *ps_value, int *status)
{
	*ps_value = (int)fts_proximity_data.detect;
	pr_info("[FTS_TS] ps_get_data: %d\n", *ps_value);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}

static int ps_enable_nodata(int en)
{
	int ret;

	pr_info("[FTS_TS] ps_enable_nodata: %d\n", en);
	mutex_lock(&fts_data->input_dev->mutex);
	ret = fts_enter_proximity_mode(en);
	if (en <= 1) {
		fts_ps_enable = en;
		if (en == 1)
			wake_up(&fts_ps_wait);
	}
	mutex_unlock(&fts_data->input_dev->mutex);
	return ret;
}

static int sagereal_ps_report_thread(void *arg)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(fts_ps_wait,
			fts_proximity_data.mode && fts_ps_enable && !fts_data->suspended);
		msleep(500);
		ps_report_interrupt_data((int)fts_proximity_data.detect);
		pr_info("[FTS_TS] report ps data: %d\n", (int)fts_proximity_data.detect);
	}
	return 0;
}

int ps_local_init(void)
{
	struct ps_control_path ps_ctl;
	struct ps_data_path ps_data;
	int err = 0;

	memset(&ps_ctl, 0, sizeof(ps_ctl));
	memset(&ps_data, 0, sizeof(ps_data));

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	err = ps_register_control_path(&ps_ctl);
	if (err)
		pr_err("[FTS_TS] ps_register_control_path failed\n");

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err)
		pr_err("[FTS_TS] ps_register_data_path failed: %d\n", err);

	init_waitqueue_head(&fts_ps_wait);
	fts_ps_thread = kthread_run(sagereal_ps_report_thread, NULL, "sagereal-ps-report");
	if (IS_ERR(fts_ps_thread))
		fts_ps_thread = NULL;

	return err;
}

int ps_local_uninit(void)
{
	if (fts_ps_thread) {
		kthread_stop(fts_ps_thread);
		fts_ps_thread = NULL;
	}
	return 0;
}

static int fts_enter_proximity_mode(int mode)
{
	int ret = 0;
	int retry = 0;

	pr_info("[FTS_TS] %s: mode=%d\n", __func__, mode);
	if (mode && fts_data->suspended) {
		pr_info("[FTS_TS] suspended, delay enter proximity\n");
		fts_ps_delay = 1;
		return 0;
	}

	do {
		ret = fts_i2c_write_reg(fts_data->client, 0xb0, mode ? 1 : 0);
		retry++;
		pr_info("[FTS_TS] write reg 0xb0 ret=%d, retry=%d\n", ret, retry);
		if (ret < 0)
			fts_reset_proc(200);
	} while (ret < 0 && retry <= 4);

	if (ret < 0) {
		pr_err("[FTS_TS] write reg 0xb0 failed: %d\n", ret);
		return ret;
	}

	fts_proximity_data.mode = mode ? 1 : 0;
	pr_info("[FTS_TS] proximity mode set to %d\n", fts_proximity_data.mode);
	return 0;
}

int fts_proximity_recovery(struct i2c_client *client)
{
	if (fts_proximity_data.mode)
		fts_enter_proximity_mode(1);
	return 0;
}

int fts_proximity_recovery_force(struct i2c_client *client)
{
	int ret;

	pr_info("[FTS_TS] %s\n", __func__);
	ret = fts_enter_proximity_mode(1);
	if (ret < 0) {
		pr_err("[FTS_TS] recovery force failed, reset and retry\n");
		fts_reset_proc(100);
		fts_enter_proximity_mode(1);
	}
	return 0;
}

int fts_proximity_readdata(struct i2c_client *client)
{
	u8 regval = 0;
	int status;
	int ret;

	if (!fts_proximity_data.mode)
		return -1;

	ret = fts_i2c_read_reg(fts_data->client, 0x01, &regval);
	status = (regval == 0xc0) ? 0 : 1;
	pr_info("[FTS_TS] p-sensor read: reg=0x%02x, status=%d, prev=%d\n",
		regval, status, (int)fts_proximity_data.detect);
	if (status != (int)fts_proximity_data.detect) {
		pr_info("[FTS_TS] p-sensor state changed to %s\n", status ? "FAR" : "NEAR");
		fts_proximity_data.detect = status ? 1 : 0;
		ps_report_interrupt_data(status);
		return 0;
	}
	return -1;
}

int fts_proximity_suspend(void)
{
	return fts_proximity_data.mode ? 0 : -1;
}

int fts_proximity_resume(void)
{
	return fts_proximity_data.mode ? 0 : -1;
}

static struct alsps_init_info fts_alsps_info = {
	.name = "fts_ts",
	.init = ps_local_init,
	.uninit = ps_local_uninit,
};

int fts_proximity_init(struct i2c_client *client)
{
	pr_info("[FTS_TS] fts_proximity_init: Enter\n");
	memset(&fts_proximity_data, 0, sizeof(fts_proximity_data));
	fts_proximity_data.detect = 1;
	alsps_driver_add(&fts_alsps_info);
	pr_info("[FTS_TS] fts_proximity_init: Exit(0)\n");
	return 0;
}

int fts_proximity_exit(struct i2c_client *client)
{
	return 0;
}

#endif /* FTS_PSENSOR_EN */
