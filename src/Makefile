#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

ARCH=x86_64

diskboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: CFLAGS=-I./include \
	-Wall -mno-sse3 -fleading-underscore -nostdlib -m64
kpack: ASFLAGS=-nostdlib -I./kernel/arch/$(ARCH)/
kpack: CFLAGS=-I./include \
        -Wall -mno-sse3 -fleading-underscore -nostdlib -O3 -m64

## IPL
diskboot: boot/arch/$(ARCH)/diskboot.o
	$(LD) -N -e start -Ttext=0x7c00 --oformat binary -o $@ $^

## Boot monitor
bootmon: boot/arch/$(ARCH)/bootmon.o \
	boot/arch/$(ARCH)/kernload.o \
	boot/arch/$(ARCH)/entry16.o \
	boot/arch/$(ARCH)/entry32.o \
	boot/arch/$(ARCH)/entry64.o \
	boot/arch/$(ARCH)/boot.o \
	boot/arch/$(ARCH)/floppy.o
	$(LD) -N -e bootmon -Ttext=0x9000 --oformat binary -o $@ $^

## Kernel
kpack: kernel/arch/$(ARCH)/asm.o
	$(LD) -N -e kstart64 -Ttext=0x10000 --oformat binary -o $@ $^
