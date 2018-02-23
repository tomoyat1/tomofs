#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vfs.h>
#include <linux/time.h>
#include <linux/atomic.h>

#include <tfs.h>

static DEFINE_MUTEX(tomofs_sb_lock);
/* TODO: Make lock per-inode */
static DEFINE_MUTEX(tomofs_inode_tbl_lock);
/* TODO: Make lock per-inode */
static DEFINE_MUTEX(tomofs_directory_record_lock);

static struct dentry *tomofs_mount(struct file_system_type *fs_type,	
	int flags, const char *dev_name, void *data);

static void tomofs_kill_superblock(struct super_block *sb);

static int tomofs_create(struct inode *parent, struct dentry *dentry,
    umode_t mode, bool excl);

static struct dentry *tomofs_lookup(struct inode *parent,
    struct dentry *child_dentry, unsigned int flags);

static int tomofs_mkdir(struct inode *parent, struct dentry *dentry,
    umode_t mode);

ssize_t tomofs_read(struct file *f, char __user *buf, size_t len,
    loff_t *offset);

ssize_t tomofs_write(struct file *f, const char __user *buf, size_t len,
    loff_t *offset);

static int tomofs_iterate(struct file *fp, struct dir_context *ctx);

static struct kmem_cache *tomofs_inode_cachep;

static struct file_system_type tomofs_fs_type = {
	.owner = THIS_MODULE,
	.name = "tomofs",
	.mount = tomofs_mount,
	.kill_sb = tomofs_kill_superblock,
	.fs_flags = 0
};

/* TODO: Fill out structs */
static const struct inode_operations tomofs_i_op = {
	.create = tomofs_create,
	.lookup = tomofs_lookup,
	.mkdir = tomofs_mkdir,
};

static const struct file_operations tomofs_i_file_op = {
	.read = NULL,
	.write = NULL,
};

static const struct file_operations tomofs_i_dir_op = {
	.owner = THIS_MODULE,
	.iterate = tomofs_iterate,
};

static const struct super_operations tomofs_sops = {
	.destroy_inode = NULL,
	.put_super = NULL,
};


static int __init init_tomofs_fs(void)
{
	tomofs_inode_cachep = KMEM_CACHE(tomofs_inode, \
	    (SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD));
	if (!tomofs_inode_cachep) {
		return -ENOMEM;
	}
	printk(KERN_INFO "loading tomofs.ko\n");
	return register_filesystem(&tomofs_fs_type);
}

static void __exit exit_tomofs_fs(void)
{
	printk(KERN_INFO "unloading tomofs.ko\n");
	kmem_cache_destroy(tomofs_inode_cachep);
	return unregister_filesystem(&tomofs_fs_type);
}

/* Allocates struct tomofs_inode. Caller must clean up */
struct tomofs_inode *tomofs_get_inode(struct super_block *sb, uint64_t ino)
{
	struct tomofs_inode *t_inode = NULL;
	struct tomofs_super_block *tsb = (struct tomofs_super_block *)sb->s_fs_info;
	struct buffer_head *bh;
	struct tomofs_inode *inodes;
	BUG_ON(!tsb);

	if (mutex_lock_interruptible(&tomofs_inode_tbl_lock)) {
		printk(KERN_DEBUG "fail to aquire lock tomofs_get_inode()\n");

		return NULL;
	}

	bh = __bread(sb->s_bdev, tsb->inodes >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	BUG_ON(!bh);
	inodes = (struct tomofs_inode *)bh->b_data;
	printk(KERN_DEBUG "inode 0 magic: 0x%x\n", inodes->i_ino);
	WARN_ON(inodes->i_ino != 0xdeadbeef);

	if (ino == 0) {
		printk(KERN_ERR "inode no. 0 is unused");
		goto release;
	}
	/* Checking for used flag */
	if ((inodes[ino].flags & 0x1) != 0x1) {
		printk(KERN_ERR "Unused inode was requested.\n");
		goto release;
	}

	t_inode = kmem_cache_alloc(tomofs_inode_cachep, GFP_KERNEL);
	if (!t_inode) {
		printk(KERN_ERR "ENOMEM in tomofs_get_inode()\n");
	}
	memcpy(t_inode, inodes + ino, sizeof(struct tomofs_inode));

release:
	brelse(bh);
	mutex_unlock(&tomofs_inode_tbl_lock);
	return t_inode;
}

static int tomofs_sync_sb(struct super_block *sb)
{
	struct buffer_head *bh;
	int ret;

	bh = sb_bread(sb, TOMOFS_SB_BLK_NO);
	/* PANIC on failure to read super block */
	BUG_ON(!bh);

	bh->b_data = (char *)sb->s_fs_info;
	printk(KERN_DEBUG "sync_sb: inode table: %x\n", ((struct tomofs_super_block *)bh->b_data)->inodes);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}

/* called with inode_tbl lock held */
static uint64_t tomofs_allocate_next_inode(struct super_block *sb)
{
	struct tomofs_super_block *tsb;
	struct buffer_head *bh;
	struct tomofs_inode *inode;
	uint64_t next_ino = 0;
	uint64_t i;

	tsb = (struct tomofs_super_block *)sb->s_fs_info;
	printk(KERN_DEBUG "allocate: tsb->inodes %x\n", tsb->inodes);
	bh = __bread(sb->s_bdev, tsb->inodes >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	inode = (struct tomofs_inode *)bh->b_data;
	/* start from 1 */
	inode++;
	if (tsb->inode_count >= TOMOFS_MAXINODES) {
		next_ino = -ENOSPC;
		goto release;
	}
	for (i = 1; i < TOMOFS_MAXINODES; i++, inode++) {
		if ((inode->flags & 0x1) == 0) {
			tsb->inode_count++;
			inode->flags |= 0x1;
			next_ino = i;
			break;
		}
	}

	//WARN_ON(next_ino == 1);
	printk(KERN_DEBUG "Allocating inode %d\n", i);
	tomofs_sync_sb(sb);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

release:
	mutex_unlock(&tomofs_inode_tbl_lock);
	mutex_unlock(&tomofs_sb_lock);
	brelse(bh);
	return next_ino;
}

/* called with inode_tbl lock held */
/* Nobody assures that the inode being written to is not in use */
static int tomofs_save_inode(struct super_block *sb,
    struct tomofs_inode *t_inode)
{
	struct tomofs_super_block *tsb;
	struct buffer_head *bh;
	struct tomofs_inode *inodes;
	int ret = -EIO;

	tsb = (struct tomofs_super_block *)sb->s_fs_info;
	printk("tsb->inodes: %x\n", tsb->inodes);
	bh = __bread(sb->s_bdev, tsb->inodes >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	BUG_ON(!bh);

	printk(KERN_DEBUG "Saving inode %d\n", t_inode->i_ino);
	inodes = (struct tomofs_inode *)bh->b_data;
	printk(KERN_DEBUG "inode 0 magic before: 0x%x\n", inodes->i_ino);
	//WARN_ON(inodes->i_ino != 0xdeadbeef);
	printk(KERN_DEBUG "Overwriting %d\n", inodes[t_inode->i_ino].i_ino);
	memcpy(&(inodes[t_inode->i_ino]), t_inode,
	    sizeof(struct tomofs_inode));
	printk(KERN_DEBUG "inode 0 magic after: 0x%x\n", inodes->i_ino);

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	ret = 0;

release:
	brelse(bh);
	return ret;
}

/* called with sb, inode_tbl, and directory_registry locks held */
static int tomofs_register_inode(struct inode *parent,
    uint64_t ino, char *filename)
{
	/* TODO: Error handling */
	struct super_block *sb;
	struct tomofs_inode *t_parent;
	struct buffer_head *bh;
	struct tomofs_directory_record *record;

#if 1
	int i;
	struct tomofs_directory_record *foo;
#endif

	sb = parent->i_sb;
	t_parent = (struct tomofs_inode *)parent->i_private;

	bh = __bread(sb->s_bdev,
	    t_parent->inode_block_ptr >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	BUG_ON(!bh);

	record =
	    &((struct tomofs_directory_record *)bh->b_data)[t_parent->child_count];
#if 0
	foo = (struct tomofs_directory_record *)bh->b_data;
#endif

	record->i_ino = ino;
	strncpy(record->filename, filename, TOMOFS_MAX_FILENAME_LEN);

	t_parent->child_count += 1;

	printk("register_inode(): adding ino: %d, filename: %s\n", record->i_ino, record->filename);

	printk(KERN_DEBUG "register_inode(): Saving parent inode\n");
	tomofs_save_inode(sb, t_parent);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

#if 0
	for (i = 0; i < t_parent->child_count; i++) {
		printk("register_inode(): ino: %d, filename: %s\n", foo[i].i_ino, foo[i].filename);
	}
#endif

	brelse(bh);
	return 0;
}

static int tomofs_create_inode(struct inode *parent, struct dentry *dentry,
    umode_t mode)
{
	struct inode *inode;
	struct tomofs_inode *t_inode = NULL;
	struct tomofs_inode *t_parent;
	struct super_block *sb;
	uint64_t next_ino = 0;
	struct block_extent inode_block;

	if (mutex_lock_interruptible(&tomofs_sb_lock)) {
		printk(KERN_DEBUG "fail to aquire lock tomofs_create_inode()\n");
		return -EINTR;
	}

	if (mutex_lock_interruptible(&tomofs_inode_tbl_lock)) {
		printk(KERN_DEBUG "fail to aquire lock tomofs_create_inode()\n");
		mutex_unlock(&tomofs_sb_lock);
		return -EINTR;
	}

	if (mutex_lock_interruptible(&tomofs_directory_record_lock)) {
		printk(KERN_DEBUG "fail to aquire lock tomofs_create_inode()\n");
		mutex_unlock(&tomofs_directory_record_lock);
		mutex_unlock(&tomofs_sb_lock);
		return -EINTR;
	}

	sb = parent->i_sb;
	next_ino = tomofs_allocate_next_inode(sb);
	switch (next_ino) {
	case -EINTR:
		return -EINTR;
		break;
	case -ENOSPC:
		return -ENOSPC;
		break;
	default:
		break;
	}

	t_inode = kmem_cache_alloc(tomofs_inode_cachep, GFP_KERNEL);

	if (!t_inode) {
		return -ENOMEM;
	}

	inode = new_inode(sb);

	if (!inode) {
		return -ENOMEM;
	}

	t_inode->i_ino = next_ino;
	/* This is morally wrong */
	t_inode->flags |= 1;

	t_inode->i_atime = current_time(inode);
	t_inode->i_ctime = current_time(inode);
	t_inode->i_mtime = current_time(inode);

	inode->i_sb = sb;
	printk(KERN_ALERT "sb inode table of created inode: %x\n", ((struct tomofs_super_block *)sb->s_fs_info)->inodes);
	inode->i_op = parent->i_op;

	inode->i_private = t_inode;
	inode->i_ino = t_inode->i_ino;
	inode->i_atime = t_inode->i_atime;
	inode->i_ctime = t_inode->i_ctime;
	inode->i_mtime = t_inode->i_mtime;
	t_inode->mode = mode;

	if (S_ISDIR(t_inode->mode)) {
		inode->i_fop = &tomofs_i_dir_op;
		t_inode->child_count = 0;
	} else if (S_ISREG(t_inode->mode)){
		inode->i_fop = &tomofs_i_file_op;
		t_inode->file_size = 0;
	} else {
		printk(KERN_ERR "Unknown inode type\n");
		return -EINVAL;
	}

	get_empty_block(sb, 1, &inode_block);
	zero_block(sb, &inode_block);

	t_inode->inode_block_ptr = inode_block.head;

	printk(KERN_DEBUG "create_inode(): Saving new inode\n");
	tomofs_save_inode(sb, t_inode);

	t_parent = (struct tomofs_inode *)parent->i_private;
	printk(KERN_DEBUG "t_parent->i_ino: %d\n", t_parent->i_ino);
	tomofs_register_inode(parent, t_inode->i_ino, dentry->d_name.name);

	inode_init_owner(inode, parent, mode);
	d_add(dentry, inode);

	mutex_unlock(&tomofs_directory_record_lock);
	mutex_unlock(&tomofs_inode_tbl_lock);
	mutex_unlock(&tomofs_sb_lock);
	return 0;
}

static int tomofs_create(struct inode *parent, struct dentry *dentry,
    umode_t mode, bool excl)
{
	printk("create\n");
	return tomofs_create_inode(parent, dentry, mode);
}

static int tomofs_mkdir(struct inode *parent, struct dentry *dentry,
    umode_t mode)
{
	printk("mkdir\n");
	return tomofs_create_inode(parent, dentry, S_IFDIR | mode);
}

static struct dentry *tomofs_lookup(struct inode *parent,
    struct dentry *child_dentry, unsigned int flags)
{
	struct tomofs_inode *t_parent =
	    (struct tomofs_inode *)parent->i_private;
	struct tomofs_inode *t_child;
	struct super_block *sb = parent->i_sb;
	struct buffer_head *bh;
	struct tomofs_directory_record *record;
	struct inode *inode;
	int i;

	if (mutex_lock_interruptible(&tomofs_directory_record_lock)) {
		printk(KERN_DEBUG "fail to aquire lock tomofs_lookup()\n");
		return NULL;
	}

	bh = __bread(sb->s_bdev,
	    t_parent->inode_block_ptr >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	BUG_ON(!bh);

	record = (struct tomofs_directory_record *)bh->b_data;
	for (i = 0; i < t_parent->child_count; i++, record++) {
		if (!strcmp(record->filename,
		    child_dentry->d_name.name)) {
			inode = new_inode(sb);
			t_child = tomofs_get_inode(sb, record->i_ino);
			inode->i_ino = t_child->i_ino;
			inode->i_sb = sb;
			inode->i_op = &tomofs_i_op;
			if (S_ISDIR(t_child->mode)) {
				inode->i_fop = &tomofs_i_dir_op;
			} else if (S_ISREG(t_child->mode)) {
				inode->i_fop = &tomofs_i_file_op;
				t_child->file_size = 0;
			} else {
				printk(KERN_ERR "Unknown inode type\n");
			}
			/* TODO: Update atime */
			inode->i_atime = t_child->i_atime;
			inode->i_ctime = t_child->i_ctime;
			inode->i_mtime = t_child->i_mtime;
			inode->i_private = t_child;
			atomic_inc(&(inode->i_count));
			inode_init_owner(inode, parent, t_child->mode);
			d_add(child_dentry, inode);
			mutex_unlock(&tomofs_directory_record_lock);
			return NULL;
		}
	}

	mutex_unlock(&tomofs_directory_record_lock);
	return NULL;
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

static int tomofs_iterate(struct file *fp, struct dir_context *ctx)
{
	loff_t pos;
	struct inode *inode;
	struct super_block *sb;
	struct buffer_head *bh;
	struct tomofs_inode *t_inode;
	struct tomofs_directory_record *records;
	struct tomofs_directory_record *rd;

	printk("iterate\n");
	pos = ctx->pos;
#if 0
	if (pos > 0) {
		printk(KERN_DEBUG "iterate(): exiting ... 0x%x\n", pos);
		return 0;
	}
#endif
	printk(KERN_DEBUG "0x%x\n", pos);
	inode = file_inode(fp);
	sb = inode->i_sb;
	t_inode = (struct tomofs_inode *)inode->i_private;

	if (unlikely(!S_ISDIR(t_inode->mode))) {
		return -ENOTDIR;
	}

	bh = __bread(sb->s_bdev,
	    t_inode->inode_block_ptr >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	BUG_ON(!bh);

	records = (struct tomofs_directory_record *)bh->b_data;
	rd = (struct tomofs_directory_record *)(bh->b_data + pos);
	for (; rd < (records + t_inode->child_count); rd++) {
		if (!dir_emit(ctx, rd->filename, TOMOFS_MAX_FILENAME_LEN,
		    rd->i_ino, DT_UNKNOWN)) {
			goto release;
		}
		printk(KERN_DEBUG "before increment: 0x%x", ctx->pos);
		ctx->pos += sizeof(struct tomofs_directory_record);
		printk(KERN_DEBUG "after increment: 0x%x", ctx->pos);
	}
release:
	brelse(bh);

	return 0;
}

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

	tsb = kmalloc(sizeof(struct tomofs_super_block), GFP_KERNEL);
	memcpy((char *)tsb, bh->b_data, sizeof(struct tomofs_super_block));
	if (unlikely(tsb->magic != TOMOFS_SB_MAGIC)) {
		printk(KERN_ERR "tomofs: NOT TOMOFS!\n");
		goto release;
	}

	sb->s_magic = TOMOFS_SB_MAGIC;
	sb->s_fs_info = tsb;
	/* max file size */
	sb->s_maxbytes = TOMOFS_MAXBYTES;
	sb->s_op = &tomofs_sops;
	t_root = tomofs_get_inode(sb, TOMOFS_ROOTDIR_INODE_NO);

	/* rootdir inode should have been created at mkfs time */
	BUG_ON(!t_root);
	root_inode = new_inode(sb);
	root_inode->i_ino = t_root->i_ino;
	inode_init_owner(root_inode, NULL, S_IFDIR);
	/* Nasty hack */
	t_root->mode = root_inode->i_mode;
	root_inode->i_sb = sb;
	root_inode->i_op = &tomofs_i_op;
	root_inode->i_fop = &tomofs_i_dir_op;
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
		printk("Error mounting tomofs\n");
	} else {
		printk("mounted tomofs\n");
	}

	return root_dentry;
}

static void tomofs_kill_superblock(struct super_block *sb)
{
	printk(KERN_INFO "Unmounting tomofs\n");
	kill_block_super(sb);
}

module_init(init_tomofs_fs)
module_exit(exit_tomofs_fs)

MODULE_LICENSE("GPL");
