#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/pm_wakeup.h>
#include <linux/notifier.h>

#include "mtk_spi.h"
#include "sf_ctl.h"

enum sf_pinctrl_state {
	SF_PIN_POWER_HIGH = 0,
	SF_PIN_POWER_LOW,
	SF_PIN_RESET_LOW,
	SF_PIN_RESET_HIGH,
	SF_PIN_IRQ,
	SF_PIN_MAX
};

static const char * const sf_pinctrl_names[SF_PIN_MAX] = {
	"finger_power_high",
	"finger_power_low",
	"fingerprint_reset_low",
	"fingerprint_reset_high",
	"fingerprint_pin_irq",
};

struct sf_mtk_hw {
	struct pinctrl *pinctrl;
	struct pinctrl_state *states[SF_PIN_MAX];
	struct sf_ctl_device *ctl_dev;
};

static struct sf_mtk_hw sf_hw;

int sf_ctl_device_power(bool on)
{
	int err = 0;

	pr_debug("[sunwave] %s(on=%d)\n", __func__, on);
	if (!sf_hw.pinctrl)
		return -ENODEV;

	if (on) {
		err = pinctrl_select_state(sf_hw.pinctrl,
					   sf_hw.states[SF_PIN_POWER_HIGH]);
		msleep(10);
	} else {
		err = pinctrl_select_state(sf_hw.pinctrl,
					   sf_hw.states[SF_PIN_POWER_LOW]);
	}

	return err;
}

int sf_spi_clock_enable(bool on)
{
	if (on)
		mt_spi_enable_clk();
	else
		mt_spi_disable_clk();

	return 0;
}

int sf_ctl_device_reset(void)
{
	int err;

	pr_debug("[sunwave] %s\n", __func__);
	if (!sf_hw.pinctrl) {
		pr_err("[sunwave] pinctrl is NULL\n");
		return -1;
	}

	err = pinctrl_select_state(sf_hw.pinctrl,
				   sf_hw.states[SF_PIN_RESET_HIGH]);
	if (err)
		return err;
	msleep(1);

	err = pinctrl_select_state(sf_hw.pinctrl,
				   sf_hw.states[SF_PIN_RESET_LOW]);
	if (err)
		return err;
	msleep(10);

	err = pinctrl_select_state(sf_hw.pinctrl,
				   sf_hw.states[SF_PIN_RESET_HIGH]);

	return err;
}

int sf_ctl_device_init_gpio_pins(struct sf_ctl_device *ctl_dev)
{
	struct device_node *node;
	struct platform_device *pdev;
	int i, err;

	pr_info("[sunwave] %s enter\n", __func__);
	sf_hw.ctl_dev = ctl_dev;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sunwave_fp");
	if (!node) {
		pr_err("[sunwave] of_find_compatible_node failed\n");
		return -ENODEV;
	}
	ctl_dev->irq_node = node;

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		pr_err("[sunwave] of_find_device_by_node failed\n");
		return -ENODEV;
	}

	sf_hw.pinctrl = pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(sf_hw.pinctrl)) {
		pr_err("[sunwave] pinctrl_get failed\n");
		sf_hw.pinctrl = NULL;
		return -ENODEV;
	}

	ctl_dev->irq_num = irq_of_parse_and_map(node, 0);
	pr_info("[sunwave] irq_num = %d\n", ctl_dev->irq_num);

	for (i = 0; i < SF_PIN_MAX; i++) {
		sf_hw.states[i] = pinctrl_lookup_state(sf_hw.pinctrl,
						       sf_pinctrl_names[i]);
		if (IS_ERR_OR_NULL(sf_hw.states[i])) {
			pr_err("[sunwave] lookup pinctrl '%s' failed\n",
			       sf_pinctrl_names[i]);
			pinctrl_put(sf_hw.pinctrl);
			sf_hw.pinctrl = NULL;
			return -ENODEV;
		}
	}

	err = pinctrl_select_state(sf_hw.pinctrl, sf_hw.states[SF_PIN_IRQ]);
	if (err)
		pr_warn("[sunwave] select irq state failed: %d\n", err);

	sf_ctl_device_power(true);

	return 0;
}

int sf_ctl_device_free_gpio(struct sf_ctl_device *ctl_dev)
{
	pr_debug("[sunwave] %s\n", __func__);
	if (sf_hw.pinctrl) {
		pinctrl_put(sf_hw.pinctrl);
		sf_hw.pinctrl = NULL;
	}
	return 0;
}

int sf_platform_init(struct sf_ctl_device *ctl_dev)
{
	if (!ctl_dev)
		return -EINVAL;

	ctl_dev->gpio_init = sf_ctl_device_init_gpio_pins;
	ctl_dev->gpio_free = sf_ctl_device_free_gpio;
	ctl_dev->power_on = sf_ctl_device_power;
	ctl_dev->reset = sf_ctl_device_reset;
	ctl_dev->spi_clk_on = sf_spi_clock_enable;

	return 0;
}

void sf_platform_exit(struct sf_ctl_device *ctl_dev)
{
	if (!ctl_dev)
		return;

	ctl_dev->gpio_init = NULL;
	ctl_dev->gpio_free = NULL;
	ctl_dev->power_on = NULL;
	ctl_dev->reset = NULL;
	ctl_dev->spi_clk_on = NULL;
}
