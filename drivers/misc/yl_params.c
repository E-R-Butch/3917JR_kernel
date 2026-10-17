/*
 * yl_params —— Coolpad (Yulong) 参数区读取接口的最小实现。
 *
 * 原厂 qca_cld3_wlan.ko 引用了 yl_params_kernel_read()，本树（小米基线）
 * 没有该驱动，导致 insmod 报 Unknown symbol、WiFi 完全起不来。
 *
 * 真实语义未知（可能读 MAC/国家码），因此这里不写入调用者的缓冲区，
 * 只返回 0（= 没读到数据），让调用方走默认值，避免误写内存。
 */
#include <linux/module.h>
#include <linux/kernel.h>

int yl_params_kernel_read(void *a, void *b, void *c)
{
	return 0;
}
EXPORT_SYMBOL(yl_params_kernel_read);
