#include <linux/fs.h>
#include <linux/types.h>

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
	if (tsb->dev.block_map[0].count < cnt) {
		return NULL;
	} else {
		found->head = tsb->dev.block_map[0].head;
		found->count = cnt;
		tsb->dev.block_map[0].head += cnt;
		tsb->dev.block_map[0].count -= cnt;
	}

	return found;
}
