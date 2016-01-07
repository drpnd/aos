#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

ARCH=x86_64

## Global flags
ASFLAGS=-nostdlib -I./kernel/arch/$(ARCH)/
CFLAGS=-I./include -Wall -fleading-underscore -nostdinc -nostdlib -O3 -m64

## Override the flags
diskboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/
bootmon: CFLAGS=-I./include \
	-Wall -fleading-underscore -nostdlib -nodefaultlibs -fno-builtin -O3 -m64

pxeboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/

kpack: CFLAGS=-I./include \
	-Wall -fleading-underscore -nostdlib -nodefaultlibs -fno-builtin -O3 -m64

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

## PXE boot
pxeboot: boot/arch/$(ARCH)/pxeboot.o
	$(LD) -N -e pxeboot -Ttext=0x7c00 --oformat binary -o $@ $^

## Kernel
kernel/arch/$(ARCH)/arch.o: include/aos/const.h kernel/kernel.h \
	kernel/arch/$(ARCH)/arch.h \
	include/sys/syscall.h
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
kernel/ramfs.o: include/aos/const.h kernel/kernel.h
kernel/syscall.o: include/aos/const.h kernel/kernel.h
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
	kernel/arch/$(ARCH)/vmx.o \
	kernel/arch/$(ARCH)/task.o \
	kernel/kernel.o \
	kernel/memory.o \
	kernel/pmem.o \
	kernel/kmem.o \
	kernel/vmem.o \
	kernel/strfmt.o \
	kernel/sched.o \
	kernel/rbtree.o \
	kernel/mpq.o \
	kernel/ramfs.o \
	kernel/syscall.o \
	kernel/exp.o
	$(LD) -N -e kstart64 -Ttext=0x10000 --oformat binary -o $@ $^
	$(LD) -N -e kstart64 -Ttext=0x10000 -o kpack.dbg $^

## init server
init: servers/init/init.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## process manager
pm: servers/pm/pm.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## file system
fs: servers/fs/fs.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## tty driver
tty: drivers/tty/tty.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## shell
pash: bin/pash/pash.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## PCI driver
pci: drivers/pci/pci.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## e1000 driver
e1000: drivers/e1000/e1000.o lib/arch/$(ARCH)/libc.o lib/arch/$(ARCH)/libcasm.o
	$(LD) -N -e _entry -Ttext=$(ORG) --oformat binary -o $@ $^

## Clean
clean:
	find . -name "*.o" | xargs rm -f
	rm -f init
	rm -f pm
	rm -f kpack
	rm -f bootmon
	rm -f diskboot
