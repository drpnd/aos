#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	@echo "make all is not currently supported."

aos.img:
	make -C src diskboot
	cp src/diskboot aos.img
	printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc

image: aos.img
