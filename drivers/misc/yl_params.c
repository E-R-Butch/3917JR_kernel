/*
 * yl_params —— Coolpad (Yulong) 参数区读取接口。
 *
 * 原厂 qca_cld3_wlan.ko 的 get_mac_address() 调用形态（逆向自 .ko）：
 *     sub sp, sp, #528
 *     x0 = buf (sp+8)      x1 = 0x200 (512)
 *     bl  yl_params_kernel_read
 *     ldrb w1, [sp, #392]   ← 读 buf+384 起的 6 字节作为 MAC
 *
 * 即签名 int yl_params_kernel_read(void *buf, size_t len)，
 * 语义是把 params 分区起始 len 字节读入 buf，MAC 位于偏移 384。
 * 读取失败返回负 errno，调用方走默认值，不会误用半截数据。
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/err.h>

#define YL_PARAMS_PATH "/dev/block/by-name/params"

int yl_params_kernel_read(void *buf, size_t len)
{
	struct file *f;
	loff_t pos = 0;
	ssize_t n;

	if (!buf || !len)
		return -EINVAL;

	f = filp_open(YL_PARAMS_PATH, O_RDONLY, 0);
	if (IS_ERR(f))
		return PTR_ERR(f);

	n = kernel_read(f, buf, len, &pos);
	filp_close(f, NULL);

	if (n < 0)
		return n;
	if (n < (ssize_t)len)
		return -EIO;

	return 0;
}
EXPORT_SYMBOL(yl_params_kernel_read);
