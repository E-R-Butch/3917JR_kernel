/*
 * yl_display_hbm.c - /sys/class/display_hbm/hbm_mode sysfs glue for 3917JR
 *
 * 原厂 drop 只留下 Kconfig 里的 CONFIG_YL_DISPLAY_HBM 与 panels 配置里的 =y，
 * 创建 display_hbm 类的源码被删除了（内核里只剩 dsi_iris3_lightup.c 里一句
 * `extern bool hbm_mode;`，且那段还在 CONFIG_PXLW_IRIS3_TCL_BACKLIGHT 下）。
 * 指纹 HAL 靠写 /sys/class/display_hbm/hbm_mode/status 点屏下亮区，
 * 节点不存在 → 灯不亮 → 取图失败。
 *
 * 本文件按原厂行为重建该节点：status/hbm 写 1/0 时，经既有
 * dsi_panel_disp_param_set() 发 DISPPARAM_HBM_FOD_ON/OFF 的屏命令
 * （dsi_panel_mi.c 里对应 DSI_CMD_SET_MI_HBM_FOD_ON / _OFF）。
 *
 * 说明（如实记录，不夸大）：
 *  - gray/alpha 两个属性按原厂节点名保留；原文件被删、语义未实测，
 *    这里只做存取与打印，不作为已验证的点灯手段。
 *  - 原厂驱动里还有 msm_notifier 的 fps 变化回调，本重建未做；
 *    是否需要在刷新率切换时重发 HBM 命令，待实测决定。
 */
#define pr_fmt(fmt) "yl_display_hbm: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/types.h>

#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_defs.h"
#include "dsi_panel_mi.h"
#include "dsi_mi_feature.h"

#define YL_HBM_CLASS_NAME "display_hbm"
#define YL_HBM_DEV_NAME   "hbm_mode"

/* 原厂另有同名全局（dsi_iris3_lightup.c 里 extern 引用它） */
bool hbm_mode;
EXPORT_SYMBOL(hbm_mode);

static bool hbm_status;
static int hbm_gray;
static int hbm_alpha;
static struct class *hbm_class;
static struct device *hbm_dev;

/* 取当前活动面板；返回 NULL 表示显示未就绪 */
static struct dsi_panel *yl_hbm_active_panel(void)
{
	void *displays[8];
	struct dsi_display *display;
	int n, i;

	n = dsi_display_get_active_displays(displays, ARRAY_SIZE(displays));
	if (n <= 0)
		return NULL;

	for (i = 0; i < n; i++) {
		display = displays[i];
		if (display && display->panel)
			return display->panel;
	}
	return NULL;
}

/*
 * 把 HBM 状态交给面板命令通路。
 *
 * 必须持 panel_lock：dsi_panel_tx_cmd_set() 自身不加锁，dsi_panel_disp_param_set()
 * 是全程持锁的。R24 之前那版直接裸发命令，指纹 HAL 连续请求时与显示线程交错，
 * 打乱 DSI 命令队列 -> DSI_CMD_SET_ON 超时 / TE 丢失 -> PANEL_DEAD。
 */
static int yl_hbm_apply(bool on)
{
	struct dsi_panel *panel = yl_hbm_active_panel();
	int rc;

	hbm_mode = on;

	if (!panel) {
		pr_warn("no active panel, hbm_mode = %d\n", on);
		return -ENODEV;
	}

	dsi_panel_acquire_panel_lock(panel);

	if (!panel->panel_initialized) {
		dsi_panel_release_panel_lock(panel);
		pr_warn("panel not initialized, skip hbm=%d\n", on);
		return -EAGAIN;
	}

	/* 设备 DT 用标准高通命令集 qcom,mdss-dsi-hbm-on/off（无 -command 后缀） */
	rc = dsi_panel_tx_cmd_set(panel,
			on ? DSI_CMD_SET_HBM : DSI_CMD_SET_HBM_OFF);

	dsi_panel_release_panel_lock(panel);

	if (rc)
		pr_err("tx hbm cmd on=%d failed rc=%d\n", on, rc);

	return rc;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", hbm_status ? 1 : 0);
}

static ssize_t status_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int val, rc;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	rc = yl_hbm_apply(val ? true : false);
	if (rc) {
		pr_err("set hbm status=%d failed rc=%d\n", val, rc);
		return rc;
	}
	hbm_status = !!val;
	return count;
}

static ssize_t hbm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", hbm_mode ? 1 : 0);
}

static ssize_t hbm_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	int val, rc;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	rc = yl_hbm_apply(val ? true : false);
	if (rc) {
		pr_err("set hbm mode=%d failed rc=%d\n", val, rc);
		return rc;
	}
	hbm_status = !!val;
	pr_info("set hbm mode = %d\n", val);
	return count;
}

static ssize_t gray_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", hbm_gray);
}

static ssize_t gray_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	hbm_gray = val;
	pr_info("gray = %d\n", val);
	return count;
}

static ssize_t alpha_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", hbm_alpha);
}

static ssize_t alpha_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	hbm_alpha = val;
	pr_info("alpha = %d\n", val);
	return count;
}

static DEVICE_ATTR_RW(status);
static DEVICE_ATTR_RW(hbm);
static DEVICE_ATTR_RW(gray);
static DEVICE_ATTR_RW(alpha);

static struct attribute *yl_hbm_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_hbm.attr,
	&dev_attr_gray.attr,
	&dev_attr_alpha.attr,
	NULL,
};

static const struct attribute_group yl_hbm_group = {
	.attrs = yl_hbm_attrs,
};

static int __init yl_display_hbm_init(void)
{
	int rc;

	hbm_class = class_create(THIS_MODULE, YL_HBM_CLASS_NAME);
	if (IS_ERR(hbm_class)) {
		rc = PTR_ERR(hbm_class);
		hbm_class = NULL;
		pr_err("Failed to create class. rc=%d\n", rc);
		return rc;
	}

	hbm_dev = device_create(hbm_class, NULL, 0, NULL, YL_HBM_DEV_NAME);
	if (IS_ERR(hbm_dev)) {
		rc = PTR_ERR(hbm_dev);
		hbm_dev = NULL;
		class_destroy(hbm_class);
		hbm_class = NULL;
		pr_err("Failed to create dev. rc=%d\n", rc);
		return rc;
	}

	rc = sysfs_create_group(&hbm_dev->kobj, &yl_hbm_group);
	if (rc) {
		pr_err("Failed to create attrs rc=%d\n", rc);
		device_destroy(hbm_class, 0);
		class_destroy(hbm_class);
		hbm_dev = NULL;
		hbm_class = NULL;
		return rc;
	}

	pr_info("ready: /sys/class/%s/%s/{status,hbm,gray,alpha}\n",
		YL_HBM_CLASS_NAME, YL_HBM_DEV_NAME);
	return 0;
}

static void __exit yl_display_hbm_exit(void)
{
	if (hbm_dev) {
		sysfs_remove_group(&hbm_dev->kobj, &yl_hbm_group);
		device_destroy(hbm_class, 0);
		hbm_dev = NULL;
	}
	if (hbm_class) {
		class_destroy(hbm_class);
		hbm_class = NULL;
	}
}

module_init(yl_display_hbm_init);
module_exit(yl_display_hbm_exit);

MODULE_DESCRIPTION("3917JR display_hbm sysfs glue (rebuilt)");
MODULE_LICENSE("GPL v2");
