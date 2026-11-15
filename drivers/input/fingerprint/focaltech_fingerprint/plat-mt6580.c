#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>

#include "mtk_spi.h"
#include "ff_ctl.h"
#include "ff_log.h"

#undef LOG_TAG
#define LOG_TAG "focal_fp"

#define FF_COMPATIBLE_NODE "mediatek,sunwave_fp"

enum ff_pinctrl_state {
	FF_PIN_POWER_HIGH = 0,
	FF_PIN_POWER_LOW,
	FF_PIN_RESET_LOW,
	FF_PIN_RESET_HIGH,
	FF_PIN_IRQ,
	FF_PIN_MAX
};

static const char * const g_pinctrl_names[FF_PIN_MAX] = {
	"finger_power_high",
	"finger_power_low",
	"fingerprint_reset_low",
	"fingerprint_reset_high",
	"fingerprint_pin_irq",
};

static struct {
	struct pinctrl *pinctrl;
	struct pinctrl_state *states[FF_PIN_MAX];
	bool b_spiclk_enabled;
} g_plat_ctx;

int ff_ctl_enable_spiclk(bool on)
{
	if (on && !g_plat_ctx.b_spiclk_enabled) {
		mt_spi_enable_clk();
		g_plat_ctx.b_spiclk_enabled = true;
	} else if (!on && g_plat_ctx.b_spiclk_enabled) {
		mt_spi_disable_clk();
		g_plat_ctx.b_spiclk_enabled = false;
	}

	return 0;
}

int ff_ctl_enable_power(bool on)
{
	int err = 0;

	if (!g_plat_ctx.pinctrl)
		return -ENODEV;

	if (on) {
		err = pinctrl_select_state(g_plat_ctx.pinctrl,
					   g_plat_ctx.states[FF_PIN_POWER_HIGH]);
		msleep(10);
	} else {
		err = pinctrl_select_state(g_plat_ctx.pinctrl,
					   g_plat_ctx.states[FF_PIN_POWER_LOW]);
	}

	return err;
}

int ff_ctl_reset_device(void)
{
	int err;

	if (!g_plat_ctx.pinctrl)
		return -ENODEV;

	err = pinctrl_select_state(g_plat_ctx.pinctrl,
				   g_plat_ctx.states[FF_PIN_RESET_HIGH]);
	if (err)
		return err;
	msleep(1);

	err = pinctrl_select_state(g_plat_ctx.pinctrl,
				   g_plat_ctx.states[FF_PIN_RESET_LOW]);
	if (err)
		return err;
	msleep(10);

	err = pinctrl_select_state(g_plat_ctx.pinctrl,
				   g_plat_ctx.states[FF_PIN_RESET_HIGH]);

	return err;
}

int ff_ctl_init_pins(int *irq_num)
{
	struct device_node *dev_node;
	struct platform_device *pdev;
	int i, err;

	FF_LOGV("'%s' enter.", __func__);

	dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE);
	if (!dev_node) {
		FF_LOGE("of_find_compatible_node(.., '%s') failed.",
			FF_COMPATIBLE_NODE);
		return -ENODEV;
	}

	*irq_num = irq_of_parse_and_map(dev_node, 0);
	FF_LOGI("irq number is %d.", *irq_num);

	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		FF_LOGE("of_find_device_by_node(..) failed.");
		return -ENODEV;
	}

	g_plat_ctx.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(g_plat_ctx.pinctrl)) {
		FF_LOGE("devm_pinctrl_get(..) failed.");
		g_plat_ctx.pinctrl = NULL;
		return -ENODEV;
	}

	for (i = 0; i < FF_PIN_MAX; i++) {
		g_plat_ctx.states[i] = pinctrl_lookup_state(g_plat_ctx.pinctrl,
							    g_pinctrl_names[i]);
		if (IS_ERR_OR_NULL(g_plat_ctx.states[i])) {
			FF_LOGE("can't find pinctrl state for '%s'.",
				g_pinctrl_names[i]);
			return -ENODEV;
		}
	}

	err = pinctrl_select_state(g_plat_ctx.pinctrl,
				   g_plat_ctx.states[FF_PIN_IRQ]);
	if (err)
		return err;

	err = pinctrl_select_state(g_plat_ctx.pinctrl,
				   g_plat_ctx.states[FF_PIN_RESET_HIGH]);
	if (err)
		return err;

	err = ff_ctl_enable_power(true);

	FF_LOGV("'%s' leave.", __func__);
	return err;
}

int ff_ctl_free_pins(void)
{
	int err = 0;

	FF_LOGV("'%s' enter.", __func__);
	if (g_plat_ctx.pinctrl) {
		ff_ctl_enable_power(false);
		devm_pinctrl_put(g_plat_ctx.pinctrl);
		g_plat_ctx.pinctrl = NULL;
	}
	FF_LOGV("'%s' leave.", __func__);
	return err;
}

const char *ff_ctl_arch_str(void)
{
	return "mt6580";
}
