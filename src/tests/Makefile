#
# Copyright (c) 2015 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

## Global flags
CFLAGS=-Wall -O3

test-libc: test-libc.o
	$(CC) -o $@ $^

test-all: test-libc
	./test-libc
