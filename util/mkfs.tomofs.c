#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tfs.h"

#define TOMOFS_BLOCK_MAP TOMOFS_BLK_SIZE
#define TOMOFS_INODES (TOMOFS_BLK_SIZE * 2)

int main(int argc, char **argv)
{
	int dev_fd;
	struct block_dev dev;
	struct block_extent init_extent;
	struct tomofs_inode t_root;

	if (argc != 2) {
		printf("You must specify a block device\n");
		exit(1);
	}
	struct tomofs_super_block tsb = {
		.magic = 0xdeadbeef,
		.inode_count = 1,
		.inodes = TOMOFS_INODES,
	};

	tsb.dev.block_map = TOMOFS_BLOCK_MAP;
	/* TODO: Calculate this */
	tsb.dev.block_cnt = 0xfffffff;

	dev_fd = open(argv[1], O_RDWR);
	write(dev_fd, &tsb, sizeof(struct tomofs_super_block));

	/* Write block map */
	lseek(dev_fd, TOMOFS_BLOCK_MAP, SEEK_SET);
	init_extent.head = TOMOFS_INODES + TOMOFS_BLK_SIZE;
	/* TODO: Calculate this */
	init_extent.count = 0xffffffff;
	write(dev_fd, &init_extent, sizeof(struct block_extent));

	
	/* Write inode table and rootdir */
	lseek(dev_fd, TOMOFS_INODES + sizeof(struct tomofs_inode),
	    SEEK_SET);
	t_root.mode = S_IFDIR;
	t_root.flags = 0 | 0x1;
	t_root.i_ino = 1;
	write(dev_fd, &t_root, sizeof(struct tomofs_inode));

	return 0;
}
