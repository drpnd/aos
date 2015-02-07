#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

ARCH=x86_64

diskboot: ASFLAGS=-nostdlib -I./boot/arch/$(ARCH)/

## IPL
diskboot: boot/arch/$(ARCH)/diskboot.o
	$(LD) -N -e start -Ttext=0x7c00 --oformat binary -o $@ $^

## Boot monitor
bootmon: boot/arch/$(ARCH)/bootmon.o
	$(LD) -N -e bootmon -Ttext=0x9000 --oformat binary -o $@ $^
