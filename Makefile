#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

all:
	@echo "make all is not currently supported."

## Compile initramfs (including kernel as well)
initrd:
#	Compile
	ORG=0x40000000 make -C src init
	ORG=0x40000000 make -C src pm
	ORG=0x40000000 make -C src fs
	ORG=0x40000000 make -C src tty
	ORG=0x40000000 make -C src pash
	ORG=0x40000000 make -C src pci
	ORG=0x40000000 make -C src e1000
#	Create an image
	@./create_initrd.sh init:/servers/init pm:/servers/pm fs:/servers/fs \
		tty:/drivers/tty pash:/bin/pash pci:/drivers/pci \
		e1000:/drivers/e1000

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
#	Create the boot image
	@./create_image.sh src/diskboot src/bootmon src/kpack initramfs

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
