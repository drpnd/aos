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
	make -C src diskboot
	make -C src bootmon
	cp src/diskboot aos.img
	printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc
	dd if=src/bootmon of=./aos.img bs=1 seek=512 conv=notrunc
	# Use truncate if your system supports
	printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc

