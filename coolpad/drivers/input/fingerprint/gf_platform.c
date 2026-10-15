// SPDX-License-Identifier: GPL-2.0-only
/* GaEa stock three-GPIO / pinctrl adapter. No SPI data path here. */
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include "gf_spi.h"

int select_pin_ctl(struct gf_dev *gf, const char *name)
{
    struct pinctrl_state *state;
    if (!strcmp(name, "fpsensor_reset_reset"))
        state = gf->reset_low;
    else if (!strcmp(name, "fpsensor_reset_active"))
        state = gf->reset_high;
    else if (!strcmp(name, "fpsensor_irq_active"))
        state = gf->irq_active;
    else
        return -EINVAL;
    return pinctrl_select_state(gf->pins, state);
}

int gf_parse_dts(struct gf_dev *gf)
{
    struct device *dev = &gf->pdev->dev;
    struct device_node *np = dev->of_node;
    int ret;

    gf->pins = devm_pinctrl_get(dev);
    if (IS_ERR(gf->pins))
        return PTR_ERR(gf->pins);
    gf->reset_low = pinctrl_lookup_state(gf->pins, "fpsensor_reset_reset");
    if (IS_ERR(gf->reset_low))
        return PTR_ERR(gf->reset_low);
    gf->reset_high = pinctrl_lookup_state(gf->pins, "fpsensor_reset_active");
    if (IS_ERR(gf->reset_high))
        return PTR_ERR(gf->reset_high);
    gf->irq_active = pinctrl_lookup_state(gf->pins, "fpsensor_irq_active");
    if (IS_ERR(gf->irq_active))
        return PTR_ERR(gf->irq_active);
    ret = select_pin_ctl(gf, "fpsensor_reset_reset");
    if (ret)
        return ret;
    ret = select_pin_ctl(gf, "fpsensor_irq_active");
    if (ret)
        return ret;

    gf->pwr_gpio = of_get_named_gpio(np, "fpc,gpio_vdd", 0);
    if (gf->pwr_gpio < 0)
        return gf->pwr_gpio;
    gf->reset_gpio = of_get_named_gpio(np, "fpc,gpio_rst", 0);
    if (gf->reset_gpio < 0)
        return gf->reset_gpio;
    gf->irq_gpio = of_get_named_gpio(np, "fpc,gpio_irq", 0);
    if (gf->irq_gpio < 0)
        return gf->irq_gpio;
    ret = devm_gpio_request(dev, gf->pwr_gpio, "goodix_vdd");
    if (ret)
        return ret;
    /* Reset is owned by the pinctrl states, as in the stock driver.
     * Do not retain a second GPIO request for the reset line.
     */
    return devm_gpio_request(dev, gf->irq_gpio, "goodix_irq");
}

int gf_power_on(struct gf_dev *gf)
{
    return gpio_direction_output(gf->pwr_gpio, 1);
}
int gf_power_off(struct gf_dev *gf)
{
    return gpio_direction_output(gf->pwr_gpio, 0);
}
int gf_hw_reset(struct gf_dev *gf, unsigned int final_ms)
{
    int ret;
    ret = select_pin_ctl(gf, "fpsensor_reset_active");
    if (ret)
        return ret;
    mdelay(5);
    ret = select_pin_ctl(gf, "fpsensor_reset_reset");
    if (ret)
        return ret;
    mdelay(5);
    ret = select_pin_ctl(gf, "fpsensor_reset_active");
    if (ret)
        return ret;
    mdelay(final_ms);
    return 0;
}
int gf_irq_num(struct gf_dev *gf)
{
    return gpio_to_irq(gf->irq_gpio);
}
