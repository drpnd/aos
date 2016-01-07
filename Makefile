#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

DISKBOOT_SIZE = $(shell stat -f "%z" src/diskboot)
BOOTMON_SIZE = $(shell stat -f "%z" src/bootmon)

KERNEL_SIZE = $(shell stat -f "%z" src/kpack)
KERNEL_CLS = $(shell expr \( ${KERNEL_SIZE} + 4095 \) / 4096)

INITRAMFS_SIZE = $(shell stat -f "%z" initramfs)
INITRAMFS_CLS = $(shell expr \( ${INITRAMFS_SIZE} + 4095 \) / 4096)

all:
	@echo "make all is not currently supported."

## Compile initramfs (including kernel as well)
initrd:
#	Reserve for file information
	@rm -f initramfs
	@dd if=/dev/zero of=initramfs seek=0 bs=1 count=4096 conv=notrunc > /dev/null 2>&1
#	Compile and write the init server
	@target='init'; fname="/servers/$$target"; entry=0; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='pm'; fname="/servers/$$target"; entry=1; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='fs'; fname="/servers/$$target"; entry=2; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='tty'; fname="/drivers/$$target"; entry=3; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='pash'; fname="/bin/$$target"; entry=4; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='pci'; fname="/drivers/$$target"; entry=5; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1
	@target='e1000'; fname="/drivers/$$target"; entry=5; \
	ORG=0x40000000 make -C src $$target; \
	off=`stat -f "%z" initramfs`; \
	printf "$$fname\000" | dd of=initramfs seek=`expr $$entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" $$off | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 16` conv=notrunc > /dev/null 2>&1; \
	printf "0: %.16x" `stat -f "%z" src/$$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=initramfs bs=1 seek=`expr $$entry \* 32 + 24` conv=notrunc > /dev/null 2>&1; \
	dd if=src/$$target of=initramfs seek=$$off bs=1 conv=notrunc > /dev/null 2>&1

## Compile boot loader
bootloader:
#	Compile the initial program loader in MBR
	make -C src diskboot
#	Compile the PXE boot loader
	make -C src pxeboot
#	Compile the boot monitor called from diskboot
	make -C src bootmon

## Compile kernel
kernel:
	make -C src kpack

## Create FAT12/16 image
image: bootloader kernel initrd
# Check the file size first
	@if [ ${DISKBOOT_SIZE} -ge 446 ]; then echo "Error: src/diskboot is too large"; exit 1; fi
	@if [ ${BOOTMON_SIZE} -ge 28672 ]; then echo "Error: src/bootmon is too large"; exit 1; fi
	@if [ ${KERNEL_SIZE} -ge 131072 ]; then echo "Error: src/kpack is too large"; exit 1; fi
	@if [ ${INITRAMFS_SIZE} -ge 262144 ]; then echo "Error: src/kpack is too large"; exit 1; fi
	@cp src/diskboot aos.img
#	Write partition table (#1: start: cyl=0, hd=2, sec=3)
#	N.B., # of cyl, hd, and sec in the entry are different from drives
	@printf '\200\002\003\000\013\055\055\000\200\000\000\000\300\012' | dd of=./aos.img bs=1 seek=446 conv=notrunc > /dev/null 2>&1
#	Write magic number
	@printf '\125\252' | dd of=./aos.img bs=1 seek=510 conv=notrunc > /dev/null 2>&1
#	Write bootmon
	@dd if=src/bootmon of=./aos.img bs=1 seek=512 conv=notrunc > /dev/null 2>&1
#	Write initramfs (FAT12)
	@printf '\353\076\220AOS  1.0\000\002\010\001\000\002\000\002\300\012\370\010\000\040\000\040\000\000\010\000\000\000\000\000\000\200\000\051\000\000\000\000NO NAME    FAT12   ' | dd of=./aos.img bs=1 seek=65536 conv=notrunc > /dev/null 2>&1
	@printf '\364\353\375' | dd of=./aos.img bs=1 seek=65600 conv=notrunc > /dev/null 2>&1 # 1: hlt; jmp 1b
	@printf '\125\252' | dd of=./aos.img bs=1 seek=66046 conv=notrunc > /dev/null 2>&1
	@printf '\370\377\377' | dd of=./aos.img bs=1 seek=66048 conv=notrunc > /dev/null 2>&1
	@s=2; c=66051; kcls=${KERNEL_CLS}; icls=${INITRAMFS_CLS}; \
	if [ $$kcls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$kcls ]; do \
			if [ `expr $$i + 1` -eq $$kcls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$kcls ]; then \
				b=`expr \( $$s + 1 \) + 4095 '*' 4096`; \
			else \
				b=`expr \( $$s + 1 \) + \( $$s + 2 \) '*' 4096`; \
			fi; \
			printf "0: %.6x" $$b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc > /dev/null 2>&1; \
			i=`expr $$i + 2`; \
			s=`expr $$s + 2`; \
			c=`expr $$c + 3`; \
		done; \
	fi; \
	if [ $$icls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$icls ]; do \
			if [ `expr $$i + 1` -eq $$icls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$icls ]; then \
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
	@s=2; c=70147; kcls=${KERNEL_CLS}; icls=${INITRAMFS_CLS}; \
	if [ $$kcls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$kcls ]; do \
			if [ `expr $$i + 1` -eq $$kcls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$kcls ]; then \
				b=`expr \( $$s + 1 \) + 4095 '*' 4096`; \
			else \
				b=`expr \( $$s + 1 \) + \( $$s + 2 \) '*' 4096`; \
			fi; \
			printf "0: %.6x" $$b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=$$c conv=notrunc > /dev/null 2>&1; \
			i=`expr $$i + 2`; \
			s=`expr $$s + 2`; \
			c=`expr $$c + 3`; \
		done; \
	fi; \
	if [ $$icls -gt 0 ]; then \
		i=0; \
		while [ $$i -lt $$icls ]; do \
			if [ `expr $$i + 1` -eq $$icls ]; then \
				b=4095; \
			elif [ `expr $$i + 2` -eq $$icls ]; then \
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
#	Write root directory
	@printf 'NO NAME    \010\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74240 conv=notrunc > /dev/null 2>&1
#	Write kernel
	@if [ ${KERNEL_CLS} -eq 0 ]; then \
		printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74272 conv=notrunc > /dev/null 2>&1; \
	else \
		printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000' | dd of=./aos.img bs=1 seek=74272 conv=notrunc > /dev/null 2>&1; \
	fi
	@printf "0: %.8x" ${KERNEL_SIZE} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=74300 conv=notrunc > /dev/null 2>&1
#	Write initramfs
	@if [ ${INITRAMFS_CLS} -eq 0 ]; then \
		printf 'INITRD     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74304 conv=notrunc > /dev/null 2>&1; \
	else \
		printf 'INITRD     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=./aos.img bs=1 seek=74304 conv=notrunc > /dev/null 2>&1; \
		printf "0: %.4x" `expr 2 + \( ${KERNEL_CLS} + 1 \) / 2 \* 2` | sed -E 's/0: (..)(..)/0: \2\1/'| xxd -r | dd of=./aos.img bs=1 seek=74330 conv=notrunc > /dev/null 2>&1; \
	fi
	@printf "0: %.8x" ${INITRAMFS_SIZE} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/'| xxd -r | dd of=./aos.img bs=1 seek=74332 conv=notrunc > /dev/null 2>&1
#	Write kernel (contents)
	@dd if=src/kpack of=./aos.img bs=1 seek=90624 conv=notrunc > /dev/null 2>&1
#	Write initramfs (contents)
	@pos=`expr 90624 + \( ${KERNEL_CLS} + 1 \) / 2 \* 2 \* 4096`; \
	dd if=initramfs of=./aos.img bs=1 seek=$$pos conv=notrunc > /dev/null 2>&1
#	Use truncate if your system supports: i.e., truncate aos.img 1474560
	@printf '\000' | dd of=./aos.img bs=1 seek=1474559 conv=notrunc > /dev/null 2>&1


## VMDK
vmdk: image
	cp aos.img aos.raw.img
	@printf '\000' | dd of=aos.raw.img bs=1 seek=268435455 conv=notrunc > /dev/null 2>&1
	qemu-img convert -f raw -O vmdk aos.img aos.vmdk
	rm -f aos.raw.img

## Test
test:
	make -C src/tests test-all

## Clean
clean:
	make -C src clean
	rm -f initramfs
	rm -f aos.img
