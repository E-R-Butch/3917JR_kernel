/* SPDX-License-Identifier: GPL-2.0-only */
/* Goodix GW9558 gaea adapter; ABI based on the stock driver and
 * the GPL Goodix/Oplus reference (Copyright 2018-2020 Oplus).
 */
#ifndef GAEA_GF_SPI_H
#define GAEA_GF_SPI_H
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>

#define GF_IOC_MAGIC 'g'
struct gf_key { __u32 key; __u32 value; };
struct gf_ioc_chip_info { __u8 vendor_id, mode, operation, reserved[5]; };
#define GF_IOC_INIT _IOR(GF_IOC_MAGIC, 0, __u8)
#define GF_IOC_EXIT _IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET _IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ _IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ _IO(GF_IOC_MAGIC, 4)
#define GF_IOC_ENABLE_SPI_CLK _IOW(GF_IOC_MAGIC, 5, __u32)
#define GF_IOC_DISABLE_SPI_CLK _IO(GF_IOC_MAGIC, 6)
#define GF_IOC_ENABLE_POWER _IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER _IO(GF_IOC_MAGIC, 8)
#define GF_IOC_INPUT_KEY_EVENT _IOW(GF_IOC_MAGIC, 9, struct gf_key)
#define GF_IOC_ENTER_SLEEP_MODE _IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO _IOR(GF_IOC_MAGIC, 11, __u8)
#define GF_IOC_REMOVE _IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO _IOW(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)
#define NETLINK_TEST 25
#define GF_NET_EVENT_IRQ 1
#define GF_NET_EVENT_FB_BLACK 2
#define GF_NET_EVENT_FB_UNBLACK 3

struct gf_dev {
    struct platform_device *pdev;
    struct input_dev *input;
    struct pinctrl *pins;
    struct pinctrl_state *reset_low, *reset_high, *irq_active;
    struct notifier_block notifier;
    struct wakeup_source wake;
    struct mutex lock;
    struct fasync_struct *async;
    int pwr_gpio, reset_gpio, irq_gpio, irq;
    unsigned int users;
    bool present, irq_requested, irq_enabled, irq_wake;
    bool device_available;
};
int gf_parse_dts(struct gf_dev *gf);
int gf_power_on(struct gf_dev *gf);
int gf_power_off(struct gf_dev *gf);
int gf_hw_reset(struct gf_dev *gf, unsigned int final_ms);
int gf_irq_num(struct gf_dev *gf);
int select_pin_ctl(struct gf_dev *gf, const char *name);
int sendnlmsg(char *msg);
int netlink_init(void);
void netlink_exit(void);
#endif
