#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

ARCH=x86_64

## Global flags
CFLAGS=-Wall -O3

libc.o: ../lib/arch/$(ARCH)/libc.c ../lib/arch/$(ARCH)/libcasm.s
	$(CC) -nostdlib -c -o $@ $?
	objcopy --prefix-symbols=aos_stdc_

test-libc: test-libc.o libc.o
	$(CC) -o $@ $?

test-all: test-libc
	./test-libc
