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
	printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc  # Write magic number
	dd if=src/bootmon of=./aos.img bs=1 seek=512 conv=notrunc  # Write bootmon
#	Use truncate if your system supports: i.e., truncate aos.img 1474560
	printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc

