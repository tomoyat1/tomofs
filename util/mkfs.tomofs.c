#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tfs.h"

int main(int argc, char **argv)
{
	int dev;
	if (argc != 2) {
		printf("You must specify a block device\n");
		exit(1);
	}
	struct tomofs_super_block foo = {
		.magic = 0xdeadbeef,
	};

	dev = open(argv[1], O_RDWR);
	write(dev, &foo, sizeof(struct tomofs_super_block));

	return 0;
}
