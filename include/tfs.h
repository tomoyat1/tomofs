#ifndef _TFS_H_
#define _TFS_H_

#include <linux/types.h>

/* Super block number, in terms of sector_t's */
#define TOMOFS_SB_BLK_NO 0
#define TOMOFS_SB_MAGIC 0xdeadbeef
#define TOMOFS_ROOTDIR_INODE_NO 1
#define TOMOFS_BLK_SIZE (1 << 12) /* (== 4KiB; PAGE_SIZE on x86_64) */
#define TOMOFS_MAX_FILENAME_LEN 64

/* TODO: Limit this if we only allow direct blocks */
static const loff_t TOMOFS_MAXBYTES = 1 << 63;

enum tomofs_obj_type {
	TOMOFS_INODE,
	TOMOFS_SPACEMAP,
};

struct block_extent {
	uintptr_t head;
	loff_t count;
};

struct block_dev {
	uintptr_t block_map;
	uint64_t block_cnt;
};

struct tomofs_inode {
	int flags;
	mode_t mode;
	unsigned long i_ino;
	uintptr_t inode_block_ptr;
	struct timespec i_atime;
	struct timespec i_mtime;
	struct timespec i_ctime;
};

/*
 * Max number of inodes supported.
 * Limited to the number of struct tomofs_inodes that fit in a 8KiB block.
 * since we only use 1 block to store inodes.
 * TODO: Dynamic inode allocation
 */
#define TOMOFS_MAXINODES (TOMOFS_BLK_SIZE / sizeof(struct tomofs_inode))

struct tomofs_directory_record {
	char filename[64];
	unsigned long i_ino;
};

#define TOMOFS_DIR_MAXINODES \
    (TOMOFS_BLK_SIZE / sizeof(struct tomofs_directory_record)

struct tomofs_super_block {
	int magic;
	struct block_dev dev;
	uint64_t inode_count;
	uintptr_t inodes;
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
