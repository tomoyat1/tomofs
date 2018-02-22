#include <linux/types.h>
/*
 * block.c: TFS block layer
 * TODO: Support recovering freed blocks.
 */

struct block_extent {
	uintptr_t head;
	uint64_t count;
};

struct block_dev {
	struct block_extent *block_map;
	uint64_t blk_cnt;
};

/*
  * Get empty block
  * *dev: block device
  * block_cnt: number of contiguous blocks
  * *found: block_extent_t to return found block into
  *
  * Does not support recovering freed blocks.
  */
struct block_extent *get_empty_block(struct block_dev *dev,
    uint64_t cnt, struct block_extent *found)
{
	if (dev->block_map[0].count < cnt) {
		return NULL;
	} else {
		found->head = dev->block_map[0].head;
		found->count = cnt;
		dev->block_map[0].head += cnt;
		dev->block_map[0].count -= cnt;
	}

	return found;
}
