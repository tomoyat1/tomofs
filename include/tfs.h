#ifndef _TFS_H_
#define _TFS_H_

#include <linux/types.h>

/* Super block number, in terms of sector_t's */
#define TOMOFS_SB_BLK_NO 0
#define TOMOFS_SB_MAGIC 0xdeadbeef
#define TOMOFS_ROOTDIR_INODE_NO 0

static const loff_t TOMOFS_MAXBYTES = 2 << 63;

struct block_extent {
	uintptr_t head;
	loff_t count;
};

struct block_dev {
	struct block_extent *block_map;
	uint64_t blk_cnt;
};

struct tomofs_super_block {
	int magic;
	struct block_dev dev;
};

struct tomofs_inode {
	int type; /* 0: file, 1: directory */
	unsigned long i_ino;
	struct timespec i_atime;
	struct timespec i_mtime;
	struct timespec i_ctime;
};

/*
  * Get empty block
  * @sb: super block
  * @block_cnt: number of contiguous blocks
  * @found: block_extent to return found block into
  *
  * Does not support recovering freed blocks.
  */
struct block_extent *get_empty_block(struct super_block *sb,
    uint64_t cnt, struct block_extent *found);

#endif /* #define _TFS_H_ */
