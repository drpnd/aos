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

libc.o: ../lib/arch/$(ARCH)/libc.c
	$(CC) -DTEST=1 -nostdinc -nostdlib -fleading-underscore -I../include -c -o $@ ../lib/arch/$(ARCH)/libc.c
	objcopy --prefix-symbols=aos_stdc $@

libcasm.o: ../lib/arch/$(ARCH)/libcasm.s
	$(CC) -DTEST=1 -nostdinc -nostdlib -I../include -c -o $@ ../lib/arch/$(ARCH)/libcasm.s
	objcopy --prefix-symbols=aos_stdc $@

test-libc: test-libc.o libc.o libcasm.o
	$(CC) -o $@ test-libc.o libc.o libcasm.o

test-all: test-libc
	./test-libc
