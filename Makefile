#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	@echo "make all is not currently supported."

image:
	make -C src diskboot  # Compile the initial program loader in MBR
	make -C src bootmon   # Compile the boot monitor called from diskboot
	cp src/diskboot aos.img
#	Write partition table (#1: start: cyl=0, hd=2, sec=3)
#	N.B., # of cyl, hd, and sec in the entry are different from drives
	printf '\200\002\003\000\013\055\055\000\200\000\000\000\300\012' | dd of=./aos.img bs=1 seek=446 conv=notrunc
#	Write magic number
	printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc
#	Write bootmon
	dd if=src/bootmon of=./aos.img bs=1 seek=512 conv=notrunc
#	Write kernel
	printf '\253' | dd of=./aos.img bs=1 seek=65536 conv=notrunc
#	Use truncate if your system supports: i.e., truncate aos.img 1474560
	printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc

