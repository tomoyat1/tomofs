all: module mkfs

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mkfs:
	gcc -o util/mkfs.tomofs -I./include util/mkfs.tomofs.c
