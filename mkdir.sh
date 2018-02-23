#!/bin/bash
echo "sudo mkdir /mnt/tomofs/foo"
sudo mkdir /mnt/tomofs/foo
echo "sudo mkdir /mnt/tomofs/bar"
sudo mkdir /mnt/tomofs/bar
echo "sudo touch /mnt/tomofs/baz"
sudo touch /mnt/tomofs/baz
echo "sudo touch /mnt/tomofs/foo/foo"
sudo touch /mnt/tomofs/foo/foo
echo "sudo mkdir /mnt/tomofs/bar/foo" # shit goes wrong here
sudo mkdir /mnt/tomofs/bar/foo
echo "sudo touch /mnt/tomofs/bar/foo/bar"
sudo touch /mnt/tomofs/bar/foo/bar
