// SPDX-License-Identifier: GPL-2.0-only
/* Goodix GW9558 platform transport for 3917JR/gaea.
 * Based on the GPL Goodix/Oplus driver interface (Copyright 2018-2020 Oplus)
 * and stock driver ABI/pinctrl disassembly. SPI and biometric decisions stay
 * in the original trusted application. No calibration or SPMT bypass.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/slab.h>
#include "gf_spi.h"

#define GF_DEV_NAME "goodix_fp"
#define CHRD_DRIVER_NAME "goodix_fp_spi"
static int gf_major;
static struct class *gf_class;
static struct gf_dev gf = { .lock = __MUTEX_INITIALIZER(gf.lock) };

static void gf_enable_irq(struct gf_dev *d)
{
    if (d->irq_requested && !d->irq_enabled) {
        enable_irq(d->irq);
        d->irq_enabled = true;
    }
}
static void gf_disable_irq(struct gf_dev *d)
{
    if (d->irq_requested && d->irq_enabled) {
        disable_irq(d->irq);
        d->irq_enabled = false;
    }
}
static irqreturn_t gf_irq(int irq, void *arg)
{
    pr_info("gf_irq: fired irq=%d\n", irq);
    struct gf_dev *d = arg;
    char msg = GF_NET_EVENT_IRQ;
    __pm_wakeup_event(&d->wake, 500);
    sendnlmsg(&msg);
    kill_fasync(&d->async, SIGIO, POLL_IN);
    return IRQ_HANDLED;
}
static int irq_setup(struct gf_dev *d)
{
    int ret;
    if (d->irq_requested)
        return 0;
    d->irq = gf_irq_num(d);
    if (d->irq < 0)
        return d->irq;
    ret = request_threaded_irq(d->irq, NULL, gf_irq,
                              IRQF_TRIGGER_RISING | IRQF_ONESHOT, "gf", d);
    if (ret)
        return ret;
    d->irq_requested = true;
    d->irq_enabled = true;
    ret = enable_irq_wake(d->irq);
    d->irq_wake = !ret;
    if (ret)
        dev_warn(&d->pdev->dev, "IRQ wake unavailable: %d\n", ret);
    return 0;
}
static void irq_cleanup(struct gf_dev *d)
{
    if (!d->irq_requested)
        return;
    gf_disable_irq(d);
    if (d->irq_wake)
        disable_irq_wake(d->irq);
    free_irq(d->irq, d);
    d->irq_requested = false;
    d->irq_wake = false;
    d->irq = 0;
}
static void gf_kernel_key_input(struct gf_dev *d, const struct gf_key *k)
{
    unsigned int code;
    switch (k->key) {
    case 1: code = KEY_HOME; break;
    case 2: code = KEY_POWER; break;
    case 5: code = KEY_CAMERA; break;
    default: return;
    }
    if ((k->key == 2 || k->key == 5) && k->value == 1) {
        input_report_key(d->input, code, 1);
        input_sync(d->input);
        input_report_key(d->input, code, 0);
        input_sync(d->input);
    } else if (k->key == 1) {
        input_report_key(d->input, code, !!k->value);
        input_sync(d->input);
    }
}
static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct gf_dev *d = filp->private_data;
    void __user *ptr = (void __user *)arg;
    struct gf_key key;
    struct gf_ioc_chip_info info;
    u8 route = NETLINK_TEST;
    int ret = 0;
    if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
        return -ENODEV;
    mutex_lock(&d->lock);
    if (!d->present) {
        ret = -ENODEV;
        goto out;
    }
    pr_info("gf_ioctl: cmd=0x%x nr=%u\n", cmd, _IOC_NR(cmd));
    switch (cmd) {
    case GF_IOC_INIT:
        ret = copy_to_user(ptr, &route, sizeof(route)) ? -EFAULT : 0;
        break;
    case GF_IOC_RESET:
        ret = gf_hw_reset(d, 5);
        break;
    case GF_IOC_ENABLE_IRQ:
        ret = irq_setup(d);
        if (!ret)
            gf_enable_irq(d);
        break;
    case GF_IOC_DISABLE_IRQ:
        gf_disable_irq(d);
        break;
    case GF_IOC_ENABLE_POWER:
        ret = gf_power_on(d);
        if (!ret)
            WRITE_ONCE(d->device_available, true);
        break;
    case GF_IOC_DISABLE_POWER:
        ret = gf_power_off(d);
        if (!ret) {
            WRITE_ONCE(d->device_available, false);
            ret = select_pin_ctl(d, "fpsensor_reset_reset");
        }
        break;
    case GF_IOC_INPUT_KEY_EVENT:
        if (copy_from_user(&key, ptr, sizeof(key)))
            ret = -EFAULT;
        else
            gf_kernel_key_input(d, &key);
        break;
    case GF_IOC_CHIP_INFO:
        ret = copy_from_user(&info, ptr, sizeof(info)) ? -EFAULT : 0;
        break;
    case GF_IOC_REMOVE:
        irq_cleanup(d);
        break;
    case GF_IOC_EXIT:
    case GF_IOC_ENABLE_SPI_CLK:
    case GF_IOC_DISABLE_SPI_CLK:
    case GF_IOC_ENTER_SLEEP_MODE:
    case GF_IOC_GET_FW_INFO:
        /* Stock no-ops. The TA owns the SPI controller. */
        break;
    default:
        /* Stock returns success for unimplemented Goodix commands. */
        dev_dbg(&d->pdev->dev, "unsupported ioctl %#x\n", cmd);
        break;
    }
out:
    mutex_unlock(&d->lock);
    return ret;
}
#ifdef CONFIG_COMPAT
static long gf_compat_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    return gf_ioctl(f, cmd, (unsigned long)compat_ptr(arg));
}
#endif
static int gf_open(struct inode *inode, struct file *filp)
{
    struct gf_dev *d = &gf;
    int ret = 0;
    mutex_lock(&d->lock);
    if (!d->present) {
        ret = -ENODEV;
        goto out;
    }
    if (!d->users) {
        ret = irq_setup(d);
        if (ret)
            goto out;
    }
    ret = gf_power_on(d);
    if (ret)
        goto power_fail;
    ret = gf_hw_reset(d, 3);
    if (ret) {
        gf_power_off(d);
        goto power_fail;
    }
    /* Last close disables the retained IRQ. Re-open must restore it after
     * power/reset succeed, even before the HAL's ENABLE_IRQ ioctl.
     */
    if (!d->users)
        gf_enable_irq(d);
    WRITE_ONCE(d->device_available, true);
    d->users++;
    filp->private_data = d;
    ret = nonseekable_open(inode, filp);
    goto out;
power_fail:
    if (!d->users)
        irq_cleanup(d);
out:
    mutex_unlock(&d->lock);
    return ret;
}
static int gf_fasync(int fd, struct file *f, int mode)
{
    struct gf_dev *d = f->private_data;
    return fasync_helper(fd, f, mode, &d->async);
}
static int gf_release(struct inode *inode, struct file *filp)
{
    struct gf_dev *d = filp->private_data;
    gf_fasync(-1, filp, 0);
    mutex_lock(&d->lock);
    if (d->users && !--d->users && d->present) {
        gf_disable_irq(d);
        gf_power_off(d);
        WRITE_ONCE(d->device_available, false);
    }
    mutex_unlock(&d->lock);
    return 0;
}
static const struct file_operations gf_fops = {
    .owner = THIS_MODULE,
    .open = gf_open,
    .release = gf_release,
    .unlocked_ioctl = gf_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = gf_compat_ioctl,
#endif
    .fasync = gf_fasync,
    .llseek = no_llseek,
};
static int goodix_fb_state_chg_callback(struct notifier_block *nb,
                                       unsigned long event, void *data)
{
    struct gf_dev *d = container_of(nb, struct gf_dev, notifier);
    struct fb_event *ev = data;
    char msg;
    int blank;
    if (event != FB_EARLY_EVENT_BLANK || !ev || !ev->data ||
        !READ_ONCE(d->device_available))
        return NOTIFY_DONE;
    blank = *(int *)ev->data;
    pr_info("gf_fb: event=%lu blank=%d\n", event, blank);
    if (blank == FB_BLANK_POWERDOWN)
        msg = GF_NET_EVENT_FB_BLACK;
    else if (blank == FB_BLANK_UNBLANK)
        msg = GF_NET_EVENT_FB_UNBLACK;
    else
        return NOTIFY_DONE;
    sendnlmsg(&msg);
    kill_fasync(&d->async, SIGIO, POLL_IN);
    return NOTIFY_OK;
}
static int gf_probe(struct platform_device *pdev)
{
    struct gf_dev *d = &gf;
    struct device *node;
    int ret;
    mutex_lock(&d->lock);
    if (d->present || d->users) {
        ret = -EBUSY;
        goto unlock;
    }
    d->pdev = pdev;
    ret = gf_parse_dts(d);
    if (ret)
        goto unlock;
    d->input = input_allocate_device();
    if (!d->input) {
        ret = -ENOMEM;
        goto unlock;
    }
    d->input->name = "qwerty";
    input_set_capability(d->input, EV_KEY, KEY_HOME);
    input_set_capability(d->input, EV_KEY, KEY_MENU);
    input_set_capability(d->input, EV_KEY, KEY_BACK);
    input_set_capability(d->input, EV_KEY, KEY_POWER);
    input_set_capability(d->input, EV_KEY, KEY_CAMERA);
    ret = input_register_device(d->input);
    if (ret) {
        input_free_device(d->input);
        d->input = NULL;
        goto unlock;
    }
    wakeup_source_init(&d->wake, "fp_wakelock");
    d->notifier.notifier_call = goodix_fb_state_chg_callback;
    ret = fb_register_client(&d->notifier);
    if (ret)
        goto input_cleanup;
    node = device_create(gf_class, &pdev->dev, MKDEV(gf_major, 0), d,
                         GF_DEV_NAME);
    if (IS_ERR(node)) {
        ret = PTR_ERR(node);
        fb_unregister_client(&d->notifier);
        goto input_cleanup;
    }
    d->present = true;
    platform_set_drvdata(pdev, d);
    dev_info(&pdev->dev, "GW9558 gaea transport ready: netlink=25, stock pinctrl\n");
    goto unlock;
input_cleanup:
    wakeup_source_trash(&d->wake);
    input_unregister_device(d->input);
    d->input = NULL;
unlock:
    mutex_unlock(&d->lock);
    return ret;
}
static int gf_remove(struct platform_device *pdev)
{
    struct gf_dev *d = platform_get_drvdata(pdev);
    mutex_lock(&d->lock);
    d->present = false;
    WRITE_ONCE(d->device_available, false);
    device_destroy(gf_class, MKDEV(gf_major, 0));
    fb_unregister_client(&d->notifier);
    irq_cleanup(d);
    gf_power_off(d);
    wakeup_source_trash(&d->wake);
    input_unregister_device(d->input);
    d->input = NULL;
    mutex_unlock(&d->lock);
    return 0;
}
static const struct of_device_id gx_match_table[] = {
    { .compatible = "goodix,gw9558" }, { }
};
MODULE_DEVICE_TABLE(of, gx_match_table);
static struct platform_driver gf_driver = {
    .probe = gf_probe,
    .remove = gf_remove,
    .driver = { .name = GF_DEV_NAME, .of_match_table = gx_match_table },
};
static int __init gf_init(void)
{
    int ret;
    gf_major = register_chrdev(0, CHRD_DRIVER_NAME, &gf_fops);
    if (gf_major < 0)
        return gf_major;
    gf_class = class_create(THIS_MODULE, GF_DEV_NAME);
    if (IS_ERR(gf_class)) {
        ret = PTR_ERR(gf_class);
        goto chr_cleanup;
    }
    ret = netlink_init();
    if (ret)
        goto class_cleanup;
    ret = platform_driver_register(&gf_driver);
    if (!ret)
        return 0;
    netlink_exit();
class_cleanup:
    class_destroy(gf_class);
chr_cleanup:
    unregister_chrdev(gf_major, CHRD_DRIVER_NAME);
    return ret;
}
static void __exit gf_exit(void)
{
    platform_driver_unregister(&gf_driver);
    netlink_exit();
    class_destroy(gf_class);
    unregister_chrdev(gf_major, CHRD_DRIVER_NAME);
}
module_init(gf_init);
module_exit(gf_exit);
MODULE_DESCRIPTION("Goodix GW9558 gaea platform transport");
MODULE_LICENSE("GPL");
