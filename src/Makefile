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
	boot/arch/$(ARCH)/boot.o
	$(LD) -N -e bootmon -Ttext=0x9000 --oformat binary -o $@ $^

## Kernel
kernel/arch/$(ARCH)/arch.o: include/aos/const.h kernel/kernel.h \
	kernel/arch/$(ARCH)/arch.h
kernel/arch/$(ARCH)/desc.o: include/aos/const.h kernel/kernel.h \
	kernel/arch/$(ARCH)/arch.h \
	kernel/arch/$(ARCH)/desc.h
kernel/arch/$(ARCH)/acpi.o: include/aos/const.h kernel/kernel.h \
	kernel/arch/$(ARCH)/arch.h \
	kernel/arch/$(ARCH)/acpi.h
kernel/arch/$(ARCH)/i8254.o: include/aos/const.h kernel/kernel.h \
	kernel/arch/$(ARCH)/arch.h \
	kernel/arch/$(ARCH)/i8254.h
kernel/kernel.o: include/aos/const.h kernel/kernel.h
kpack: kernel/arch/$(ARCH)/asm.o \
	kernel/arch/$(ARCH)/arch.o \
	kernel/arch/$(ARCH)/desc.o \
	kernel/arch/$(ARCH)/acpi.o \
	kernel/arch/$(ARCH)/apic.o \
	kernel/arch/$(ARCH)/i8254.o \
	kernel/arch/$(ARCH)/memory.o \
	kernel/arch/$(ARCH)/trampoline.o \
	kernel/arch/$(ARCH)/ap_entry32.o \
	kernel/arch/$(ARCH)/ap_entry64.o \
	kernel/kernel.o
	$(LD) -N -e kstart64 -Ttext=0x10000 --oformat binary -o $@ $^
