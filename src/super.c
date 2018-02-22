#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vfs.h>
#include <linux/time.h>

#include <tfs.h>

static struct dentry *tomofs_mount(struct file_system_type *fs_type,	
	int flags, const char *dev_name, void *data);

static struct file_system_type tomofs_fs_type = {
	.owner = THIS_MODULE,
	.name = "tomofs",
	.mount = tomofs_mount,
	.kill_sb = kill_block_super,
	.fs_flags = 0
};

static int __init init_tomofs_fs(void)
{
	return register_filesystem(&tomofs_fs_type);
}

static void __exit exit_tomofs_fs(void)
{
	return unregister_filesystem(&tomofs_fs_type);
}

struct inode *tomofs_get_inode(struct super_block *sb, uint64_t ino)
{
	return NULL;
}

/* TODO: Fill out structs */
static const struct inode_operations tomofs_i_op = {
	.create = NULL,
	.lookup = NULL,
	.mkdir = NULL,
};

static const struct file_operations tomofs_i_fop = {
	.read = NULL,
	.write = NULL,
};
static const struct super_operations tomofs_sops = {
	.destroy_inode = NULL,
	.put_super = NULL,
};


int tomofs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root_inode;
	struct buffer_head *bh;
	struct tomofs_super_block *tsb;
	int ret = -EPERM;

	bh = sb_bread(sb, TOMOFS_SB_BLK_NO);
	/* PANIC on failure to read super block */
	BUG_ON(!bh);

	tsb = (struct tomofs_super_block *)bh->b_data;
	if (unlikely(tsb->magic != TOMOFS_SB_MAGIC)) {
		printk(KERN_ERR "tomofs: NOT TOMOFS!");
		goto release;
	}

	sb->s_magic = TOMOFS_SB_MAGIC;
	sb->s_fs_info = tsb;
	/* max file size */
	sb->s_maxbytes = TOMOFS_MAXBYTES;
	sb->s_op = &tomofs_sops;

	/*
         * TODO: Properly read roodir inode from disk.
	 * THIS IS IMPORTANT!
	 * Implementation below creates fake rootdir inode on every
	 * mount.
	 */
	root_inode = new_inode(sb);
	root_inode->i_ino = TOMOFS_ROOTDIR_INODE_NO;
	root_inode->i_op = &tomofs_i_op;
	root_inode->i_fop = &tomofs_i_fop;
	/*
	 * hold tomofs specific rootdir inode representation for later
	 * use. This data structure should hold block pointers and
	 * other important metadata needed to retrive user data.
	 */
	root_inode->i_private = tomofs_get_inode(sb, TOMOFS_ROOTDIR_INODE_NO);
	/* access time = create time = modify time = NOW! */
	root_inode->i_atime = current_time(root_inode);
	root_inode->i_ctime = current_time(root_inode);
	root_inode->i_mtime = current_time(root_inode);
	/* make dentry for rootdir from inode */
	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		ret = -ENOMEM;
		goto release;
	}
	ret = 0;

release:
	brelse(bh);
	return ret;
}

static struct dentry *tomofs_mount(struct file_system_type *fs_type,
    int flags, const char *dev_name, void *data)
{
	struct dentry *root_dentry;
	root_dentry = mount_bdev(fs_type, flags, dev_name, data, tomofs_fill_super);
	if (unlikely(IS_ERR(root_dentry))) {
		printk("Error mounting tomofs");
	} else {
		printk("mounted tomofs");
	}

	return root_dentry;
}

module_init(init_tomofs_fs)
module_exit(exit_tomofs_fs)

MODULE_LICENSE("GPL");
