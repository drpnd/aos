#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

KERNEL_SIZE = $(shell stat -f "%z" src/kpack)
KERNEL_CLS = $(shell expr \( ${KERNEL_SIZE} + 4095 \) / 4096)


all:
	@echo "make all is not currently supported."

image:
	make -C src diskboot  # Compile the initial program loader in MBR
	make -C src bootmon   # Compile the boot monitor called from diskboot
	make -C src kpack     # Compile the kernel
	@cp src/diskboot aos.img
#	Write partition table (#1: start: cyl=0, hd=2, sec=3)
#	N.B., # of cyl, hd, and sec in the entry are different from drives
	@printf '\200\002\003\000\013\055\055\000\200\000\000\000\300\012' | dd of=./aos.img bs=1 seek=446 conv=notrunc > /dev/null 2>&1
#	Write magic number
	@printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc > /dev/null 2>&1
#	Write bootmon
	@dd if=src/bootmon of=./aos.img bs=1 seek=512 conv=notrunc > /dev/null 2>&1
#	Write kernel (FAT12)
	@printf '\353\076\220AOS  1.0\000\002\010\001\000\002\000\002\300\012\370\010\000\040\000\040\000\000\010\000\000\000\000\000\000\200\000\051\000\000\000\000NO NAME    FAT12   ' | dd of=./aos.img bs=1 seek=65536 conv=notrunc > /dev/null 2>&1
	@printf '\364\353\375' | dd of=./aos.img bs=1 seek=65600 conv=notrunc > /dev/null 2>&1 # 1: hlt; jmp 1b
	@printf '\125\252' | dd of=./aos.img bs=1 seek=66046 conv=notrunc > /dev/null 2>&1
	@printf '\370\377\377' | dd of=./aos.img bs=1 seek=66048 conv=notrunc > /dev/null 2>&1
	@s=2; c=66051; cls=${KERNEL_CLS}; \
	if [ $$cls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$cls ]; do \
			if [ `expr $$i + 1` -eq $$cls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$cls ]; then \
				b=`expr \( $$s + 1 \) + 4095 '*' 4096`; \
			else \
				b=`expr \( $$s + 1 \) + \( $$s + 2 \) '*' 4096`; \
			fi; \
			printf "0: %.6x" $$b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc > /dev/null 2>&1; \
			i=`expr $$i + 2`; \
			s=`expr $$s + 2`; \
			c=`expr $$c + 3`; \
		done; \
	fi
	@printf '\370\377\377' | dd of=./aos.img bs=1 seek=70144 conv=notrunc > /dev/null 2>&1
	@s=2; c=70147; cls=${KERNEL_CLS}; \
	if [ $$cls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$cls ]; do \
			if [ `expr $$i + 1` -eq $$cls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$cls ]; then \
				b=`expr \( $$s + 1 \) + 4095 '*' 4096`; \
			else \
				b=`expr \( $$s + 1 \) + \( $$s + 2 \) '*' 4096`; \
			fi; \
			printf "0: %.6x" $$b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc > /dev/null 2>&1; \
			i=`expr $$i + 2`; \
			s=`expr $$s + 2`; \
			c=`expr $$c + 3`; \
		done; \
	fi
	@printf 'NO NAME    \010\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74240 conv=notrunc > /dev/null 2>&1
	@if [ ${KERNEL_CLS} -eq 0 ]; then \
		printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74272 conv=notrunc > /dev/null 2>&1; \
	else \
		printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000' | dd of=./aos.img bs=1 seek=74272 conv=notrunc > /dev/null 2>&1; \
	fi
	@printf "0: %.8x" ${KERNEL_SIZE} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=74300 conv=notrunc > /dev/null 2>&1
	@dd if=src/kpack of=./aos.img bs=1 seek=90624 conv=notrunc > /dev/null 2>&1
#	Use truncate if your system supports: i.e., truncate aos.img 1474560
	@printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc > /dev/null 2>&1

## Clean
clean:
	make -C src clean
	rm -f initramfs
	rm -f aos.img
