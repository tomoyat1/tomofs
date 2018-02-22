#!/bin/bash

sudo umount /mnt/tomofs
sudo rmmod tomofs
sudo zfs destroy tank/test
