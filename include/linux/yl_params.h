/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Coolpad (Yulong) 参数区头文件的兼容实现。
 * 本树（小米基线）缺少原厂 include/linux/yl_params.h，
 * 但 qpnp-power-on.h 与 msm-poweroff.c 会引用 is_device_door_locked()。
 * 原厂该函数读 params 分区判断“整机门锁”状态（产线/防拆用），
 * 本实现恒返回 false = 未上锁，等价于正常零售机行为。
 */
#ifndef _LINUX_YL_PARAMS_H
#define _LINUX_YL_PARAMS_H

#include <linux/types.h>

static inline bool is_device_door_locked(void)
{
	return false;
}

static inline void yl_pstore_ss_failure_reason(const char *name, const char *reason)
{
	(void)name;
	(void)reason;
}

#endif /* _LINUX_YL_PARAMS_H */
