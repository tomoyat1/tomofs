#include <linux/fs.h>
#include <linux/types.h>
#include <linux/buffer_head.h>

#include "tfs.h"
/*
 * block.c: TFS block layer
 * TODO: Support recovering freed blocks.
 */

struct block_extent *get_empty_block(struct super_block *sb,
    uint64_t cnt, struct block_extent *found)
{
	struct tomofs_super_block *tsb =
	    (struct tomofs_super_block *)sb->s_fs_info;
	struct buffer_head *bh;
	struct block_extent *block_map;
	bh = __bread(sb->s_bdev, tsb->dev.block_map >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	block_map = (struct block_extent *)bh->b_data;
	if (block_map[0].count < cnt) {
		return NULL;
	} else {
		found->head = block_map[0].head;
		printk(KERN_DEBUG "tomofs block allocator: next block: 0x%x\n", found->head);
		found->count = cnt;
		block_map[0].head += TOMOFS_BLK_SIZE * cnt;
		block_map[0].count -= cnt;
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
	}

	brelse(bh);
	return found;
}

int zero_block(struct super_block *sb, struct block_extent *e)
{
	struct tomofs_super_block *tsb =
	    (struct tomofs_super_block *)sb->s_fs_info;
	struct buffer_head *bh;
	char *buf;
	bh = __bread(sb->s_bdev, tsb->dev.block_map >> sb->s_blocksize_bits,
	    TOMOFS_BLK_SIZE);
	buf = (char *)bh->b_data;
	memset(buf, 0, TOMOFS_BLK_SIZE);
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
	return 0;
}
