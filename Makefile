#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

KERNEL_SIZE = $(shell stat -f "%z" src/kpack)
KERNEL_CLS = $(shell expr \( ${KERNEL_SIZE} + 4095 \) / 4096)
KERNEL_CLS_HALF = $(shell expr ${KERNEL_CLS} / 2)
KERNEL_CLS_R = $(shell expr ${KERNEL_CLS} % 2)


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
#	Write kernel (FAT12)
	printf '\353\076\220AOS  1.0\000\002\010\001\000\002\000\002\300\012\370\010\000\040\000\040\000\000\010\000\000\000\000\000\000\200\000\051\000\000\000\000NO NAME    FAT12   ' | dd of=./aos.img bs=1 seek=65536 conv=notrunc
	printf '\125\252' | dd of=./aos.img bs=1 seek=66046 conv=notrunc
	printf '\370\377\377' | dd of=./aos.img bs=1 seek=66048 conv=notrunc
	@s=2; c=66051; if [ ${KERNEL_CLS_HALF} -gt 0 ]; then \
		for i in `seq ${KERNEL_CLS_HALF}`; do \
			b=`expr \( $$s + 1 \) + \( $$s + 2 \) '*' 4096`; \
			printf "0: %.6x" $$b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc; \
			echo "$$s $$c"; \
			s=`expr $$s + 2`; \
			c=`expr $$c + 3`; \
		done; \
	fi; \
	printf "0: %.6x" 4095 | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc;
	printf '\370\377\377' | dd of=./aos.img bs=1 seek=70144 conv=notrunc
	@c=70147; printf "0: %.6x" 4095 | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc;
	printf 'NO NAME    \010\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74240 conv=notrunc
	printf 'KERNEL     \001\000\000\000\000\000\000\002\000' | dd of=./aos.img bs=1 seek=74272 conv=notrunc
	printf "0: %.8x" ${KERNEL_SIZE} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=74300 conv=notrunc
	dd if=src/kpack of=./aos.img bs=1 seek=90624 conv=notrunc
#	Use truncate if your system supports: i.e., truncate aos.img 1474560
	printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc

