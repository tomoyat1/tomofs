#!/bin/bash

sudo zfs create -V 16G -s tank/test
sudo insmod ./tomofs.ko
sudo ./util/mkfs.tomofs /dev/zvol/tank/test
sudo mount -t tomofs /dev/zvol/tank/test /mnt/tomofs
