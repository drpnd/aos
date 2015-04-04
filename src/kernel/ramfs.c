/*_
 * Copyright (c) 2015 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <aos/const.h>
#include "kernel.h"

/*
 * Data structure for a file descriptor
 */
struct ramfs_fildes {
    void *content;
    size_t size;
};

/*
 * ramfs
 */
struct ramfs {
    u64 *root;
};

struct ramfs *ramfs;

/* Prototype declarations */
ssize_t ramfs_read(struct fildes *, void *, size_t);
ssize_t ramfs_write(struct fildes *, const void *, size_t);
off_t ramfs_lseek(struct fildes *, off_t, int);

/*
 * Initialize ramfs
 */
int
ramfs_init(u64 *buffer)
{
    /* Allocate the ramfs space */
    ramfs = kmalloc(sizeof(struct ramfs));
    if ( NULL == ramfs ) {
        return -1;
    }
    ramfs->root = buffer;

    return 0;
}

/*
 * Open the file
 */
int
ramfs_open(const char *path)
{
    u64 *buffer;
    u64 offset;
    u64 size;
    struct fildes *fildes;
    struct ramfs_fildes *data;

    /* Search the specified file */
    buffer = ramfs->root;
    offset = 0;
    while ( 0 != *buffer ) {
        if ( 0 == kstrcmp((char *)buffer, path) ) {
            offset = *(buffer + 2);
            size = *(buffer + 3);
            break;
        }
        buffer += 4;
    }
    if ( 0 == offset ) {
        return -1;
    }

    /* Create a fild descriptor */
    fildes = kmalloc(sizeof(struct fildes));
    kmemset(fildes, 0, sizeof(struct fildes));
    data = kmalloc(sizeof(struct ramfs_fildes));
    if ( NULL == data ) {
        kfree(fildes);
        return -1;
    }
    data->content = (void *)((u64)ramfs->root + offset);
    data->size = size;

    /* Set functions and file descriptor specific data */
    fildes->data = data;
    fildes->read = ramfs_read;
    fildes->write = ramfs_write;
    fildes->lseek = ramfs_lseek;

    return 0;
}

/*
 * Read
 */
ssize_t
ramfs_read(struct fildes *fildes, void *buf, size_t nbyte)
{
    return -1;
}

/*
 * Write
 */
ssize_t
ramfs_write(struct fildes *fildes, const void *buf, size_t nbyte)
{
    return -1;
}

/*
 * Seek
 */
off_t
ramfs_lseek(struct fildes *fildes, off_t offset, int whence)
{
    return -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
