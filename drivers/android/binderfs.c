// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal "binder" filesystem for Android 11+ systems.
 *
 * Upstream binderfs (Linux 5.0+) lets userspace create/destroy binder devices
 * through a control node.  Android 11+ init only needs the far smaller subset:
 * mount -t binder binder /dev/binderfs, then use binder/hwbinder/vndbinder.
 * Those devices already exist on this kernel (they are registered from
 * CONFIG_ANDROID_BINDER_DEVICES), so this filesystem just republishes them as
 * character nodes at mount time.  No control interface, no dynamic devices.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/miscdevice.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/string.h>

#define BINDERFS_SUPER_MAGIC 0x6c6f6f70

extern dev_t binder_get_device_dev(const char *name);

static const char * const binderfs_default_devices[] = {
	"binder",
	"hwbinder",
	"vndbinder",
};

static struct inode *binderfs_make_inode(struct super_block *sb, umode_t mode)
{
	struct inode *inode = new_inode(sb);

	if (!inode)
		return NULL;

	inode->i_ino = get_next_ino();
	inode->i_mode = mode;
	inode->i_uid = GLOBAL_ROOT_UID;
	inode->i_gid = GLOBAL_ROOT_GID;

	return inode;
}

static int binderfs_add_device(struct dentry *root, const char *name)
{
	struct super_block *sb = root->d_sb;
	struct dentry *dentry;
	struct inode *inode;
	dev_t dev;

	dev = binder_get_device_dev(name);
	if (!dev) {
		pr_warn("binderfs: no such binder device: %s\n", name);
		return -ENODEV;
	}

	inode = binderfs_make_inode(sb, S_IFCHR | 0666);
	if (!inode)
		return -ENOMEM;

	init_special_inode(inode, inode->i_mode, dev);

	dentry = d_alloc_name(root, name);
	if (!dentry) {
		iput(inode);
		return -ENOMEM;
	}
	d_add(dentry, inode);

	return 0;
}

static int binderfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root_inode;
	struct dentry *root;
	int i;

	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = BINDERFS_SUPER_MAGIC;
	sb->s_time_gran = 1;

	root_inode = binderfs_make_inode(sb, S_IFDIR | 0755);
	if (!root_inode)
		return -ENOMEM;

	root_inode->i_op = &simple_dir_inode_operations;
	root_inode->i_fop = &simple_dir_operations;
	inc_nlink(root_inode);

	root = d_make_root(root_inode);
	if (!root)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(binderfs_default_devices); i++)
		binderfs_add_device(root, binderfs_default_devices[i]);

	sb->s_root = root;

	return 0;
}

static struct dentry *binderfs_mount(struct file_system_type *fs_type,
				     int flags, const char *dev_name,
				     void *data)
{
	return mount_nodev(fs_type, flags, data, binderfs_fill_super);
}

static void binderfs_kill_super(struct super_block *sb)
{
	kill_litter_super(sb);
}

static struct file_system_type binder_fs_type = {
	.name		= "binder",
	.mount		= binderfs_mount,
	.kill_sb	= binderfs_kill_super,
};

static int __init init_binderfs(void)
{
	return register_filesystem(&binder_fs_type);
}
fs_initcall(init_binderfs);

