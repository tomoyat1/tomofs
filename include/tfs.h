#ifndef _TFS_H_
#define _TFS_H_

#include <linux/types.h>

/* Super block number, in terms of sector_t's */
#define TOMOFS_SB_BLK_NO 0
#define TOMOFS_SB_MAGIC 0xdeadbeef
#define TOMOFS_ROOTDIR_INODE_NO 0

const loff_t TOMOFS_MAXBYTES = 2 << 63;

struct tomofs_super_block {
	int magic;
};

#endif /* #define _TFS_H_ */
