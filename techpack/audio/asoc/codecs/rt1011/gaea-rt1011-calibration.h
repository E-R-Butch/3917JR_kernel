/* SPDX-License-Identifier: GPL-2.0-only */
/* 3917JR OEM params: 4 KiB record slots, CALIBRATION is slot 22.
 * Stock yl_params_kernel_read and this device's record were inspected.
 * Read-only: retain factory data; never replace it with measured/default data.
 */
#include <linux/fs.h>
#include <asm/unaligned.h>

static int rt1011_gaea_read_r0(unsigned int *r0)
{
	struct file *file;
	loff_t offset = 22 * 4096;
	u8 record[512];
	ssize_t count;
	unsigned int value;

	file = filp_open("/dev/block/by-name/params", O_RDONLY, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);
	count = kernel_read(file, record, sizeof(record), &offset);
	filp_close(file, NULL);
	if (count < 0)
		return count;
	if (count != sizeof(record))
		return -EIO;
	if (memcmp(record, "CALIBRATION", sizeof("CALIBRATION")) ||
	    get_unaligned_le32(record + 16) != 1)
		return -ENODATA;
	/* This module binds the verified 0x39 gaea amplifier, left-channel slot. */
	value = get_unaligned_le32(record + 20);
	if (!value || value > 0x1ffffff)
		return -ERANGE;
	*r0 = value;
	return 0;
}
