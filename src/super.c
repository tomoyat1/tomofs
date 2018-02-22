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

static void tomofs_kill_superblock(struct super_block *sb);

static struct file_system_type tomofs_fs_type = {
	.owner = THIS_MODULE,
	.name = "tomofs",
	.mount = tomofs_mount,
	.kill_sb = tomofs_kill_superblock,
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

struct tomofs_inode *tomofs_get_inode(struct super_block *sb, uint64_t ino)
{
	struct tomofs_inode *t_inode;
	struct tomofs_super_block *tsb = (struct tomofs_super_block *)sb->s_fs_info;
	/* TODO: Change to BUG_ON() */
	WARN_ON(!tsb);

	t_inode = &(tsb->inodes[ino]);
	/* Checking for used flag */
	if ((t_inode->flags & 0x1) != 0x1) {
		return NULL;
	}
	return t_inode;
}

static int tomofs_create_fs_obj(struct inode *parent, struct dentry *dentry,
    umode_t mode)
{
	return 0;
}

static int tomofs_create(struct inode *parent, struct dentry *dentry,
    umode_t mode, bool excl)
{
	return tomofs_create_fs_obj(parent, dentry, mode);
}

static int tomofs_mkdir(struct inode *parent, struct dentry *dentry,
    umode_t mode)
{
	return tomofs_create_fs_obj(parent, dentry, S_IFDIR | mode);
}

ssize_t tomofs_read(struct file *f, char __user *buf, size_t len,
    loff_t *offset)
{
	ssize_t read;
	return read;
}

ssize_t tomofs_write(struct file *f, const char __user *buf, size_t len,
    loff_t *offset)
{
	ssize_t wrote;
	return wrote;
}

/* TODO: Fill out structs */
static const struct inode_operations tomofs_i_op = {
	.create = tomofs_create,
	.lookup = NULL,
	.mkdir = tomofs_mkdir,
};

static const struct file_operations tomofs_i_fop = {
	.read = tomofs_read,
	.write = tomofs_write,
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
	struct tomofs_inode *t_root;
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
	t_root = tomofs_get_inode(sb, TOMOFS_ROOTDIR_INODE_NO);

	/* rootdir inode should have been created at mkfs time */
	/* TODO: Change to BUG_ON() */
	WARN_ON(!t_root);
	root_inode = new_inode(sb);
	/* Found rootdir inode on disk */
	root_inode->i_ino = t_root->i_ino;
	root_inode->i_op = &tomofs_i_op;
	root_inode->i_fop = &tomofs_i_fop;
	if (t_root->i_atime.tv_sec == 0) {
		root_inode->i_atime = t_root->i_atime;
	} else {
		root_inode->i_atime = current_time(root_inode);
		t_root->i_atime = root_inode->i_atime;
	}

	if (t_root->i_ctime.tv_sec == 0) {
		root_inode->i_ctime = t_root->i_ctime;
	} else {
		root_inode->i_ctime = current_time(root_inode);
		t_root->i_ctime = root_inode->i_ctime;
	}

	if (t_root->i_mtime.tv_sec == 0) {
		root_inode->i_mtime = t_root->i_mtime;
	} else {
		root_inode->i_mtime = current_time(root_inode);
		t_root->i_mtime = root_inode->i_mtime;
	}
	root_inode->i_private = t_root;
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

static void tomofs_kill_superblock(struct super_block *sb)
{
	printk(KERN_INFO "Unmounting tomofs");
	kill_block_super(sb);
}

module_init(init_tomofs_fs)
module_exit(exit_tomofs_fs)

MODULE_LICENSE("GPL");
