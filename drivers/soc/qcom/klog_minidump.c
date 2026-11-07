// SPDX-License-Identifier: GPL-2.0
/*
 * Expose the kernel log to the XBL minidump.
 *
 * A hang or panic at early driver init leaves nothing behind: no adbd, no
 * /sys/fs/pstore, no partition write.  XBL does collect a minidump on a real
 * panic though, and it contains every region the kernel registered through
 * msm_minidump_add_region().  So register a static buffer as one such region
 * and fill it from a kmsg_dumper callback: after the crash the log tail can be
 * pulled over EDL with `edl.py memorydump`, no partition writes involved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kmsg_dump.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/types.h>
#include <asm/page.h>
#include <soc/qcom/minidump.h>

#define KLOG_MD_SIZE	(2 * 1024 * 1024)
#define KLOG_MD_NAME	"KLOG_BUF"
#define KLOG_MD_MAGIC	0x4b4c4f47u	/* 'KLOG' */

struct klog_md_hdr {
	u32 magic;
	u32 reason;
	u64 len;
	char data[];
};

static char klog_md_buf[KLOG_MD_SIZE] __aligned(PAGE_SIZE);
static struct md_region klog_md_entry;

static void klog_md_dump(struct kmsg_dumper *dumper,
			 enum kmsg_dump_reason reason)
{
	struct klog_md_hdr *hdr = (struct klog_md_hdr *)klog_md_buf;
	size_t cap = KLOG_MD_SIZE - sizeof(*hdr);
	size_t len = 0;

	/* clear the magic first: a partial dump must not look valid */
	hdr->magic = 0;
	hdr->reason = (u32)reason;
	hdr->len = 0;

	kmsg_dump_get_buffer(dumper, true, hdr->data, cap, &len);

	hdr->len = len;
	wmb();
	hdr->magic = KLOG_MD_MAGIC;
}

static struct kmsg_dumper klog_md_dumper = {
	.dump = klog_md_dump,
	.max_reason = KMSG_DUMP_OOPS,
};

static int __init klog_md_init(void)
{
	int ret;

	strlcpy(klog_md_entry.name, KLOG_MD_NAME, sizeof(klog_md_entry.name));
	klog_md_entry.id = MINIDUMP_DEFAULT_ID;
	klog_md_entry.virt_addr = (u64)(uintptr_t)klog_md_buf;
	klog_md_entry.phys_addr = virt_to_phys(klog_md_buf);
	klog_md_entry.size = KLOG_MD_SIZE;

	ret = msm_minidump_add_region(&klog_md_entry);
	if (ret)
		pr_err("klog_md: minidump region add failed: %d\n", ret);

	ret = kmsg_dump_register(&klog_md_dumper);
	if (ret) {
		pr_err("klog_md: kmsg dumper register failed: %d\n", ret);
		return 0;
	}

	pr_info("klog_md: kernel log -> minidump '%s' (%d KiB, phys %pa)\n",
		KLOG_MD_NAME, KLOG_MD_SIZE / 1024,
		&klog_md_entry.phys_addr);
	return 0;
}
fs_initcall(klog_md_init);
