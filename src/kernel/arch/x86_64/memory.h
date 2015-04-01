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

#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_H

#include <aos/const.h>
#include "arch.h"

/* Maximum order of buddy system */
#define PHYS_MEM_MAX_BUDDY_ORDER        18
/* 32 (2^5) byte is the minimum object size of a slab object */
#define PHYS_MEM_SLAB_BASE_ORDER        5
/* 1024 (2^(5 + 6 - 1)) byte is the maximum object size of a slab object */
#define PHYS_MEM_SLAB_ORDER             6
/* 2^16 objects in a cache */
#define PHYS_MEM_SLAB_NR_OBJ_ORDER      4

/*
 * Buddy system
 */
struct phys_mem_buddy {
    struct phys_mem_buddy_list *heads[PHYS_MEM_MAX_BUDDY_ORDER];
};

/*
 * Physical memory page
 */
struct phys_mem_page {
    u64 flags;
    int order;
};

/*
 * List structure in unused page for the buddy system
 */
struct phys_mem_buddy_list {
    struct phys_mem_buddy_list *prev;
    struct phys_mem_buddy_list *next;
} __attribute__ ((packed));

/*
 * Physical memory
 */
struct phys_mem {
    /* The number of pages */
    u64 nr;
    /* Pages (flags etc.) */
    struct phys_mem_page *pages;
    /* Buddy structure */
    struct phys_mem_buddy buddy;
};

/*
 * A slab object
 */
struct phys_mem_slab_obj {
    void *addr;
} __attribute__ ((packed));

/*
 * Slab objects
 *  slab_hdr
 *    object 0
 *    object 1
 *    ...
 */
struct phys_mem_slab {
    /* slab_hdr */
    struct phys_mem_slab *next;
    int nr;
    int nused;
    int free;
    void *obj_head;
    /* Free marks follows (nr byte) */
    u8 marks[1];
    /* Objects follows */
} __attribute__ ((packed));

/*
 * Free list of slab objects
 */
struct phys_mem_slab_free_list {
    struct phys_mem_slab *partial;
    struct phys_mem_slab *full;
    struct phys_mem_slab *free;
} __attribute__ ((packed));

/*
 * Root data structure of slab objects
 */
struct phys_mem_slab_root {
    /* Generic slabs */
    struct phys_mem_slab_free_list gslabs[PHYS_MEM_SLAB_ORDER];
} __attribute__ ((packed));


/* in memory.c */
int phys_mem_init(struct bootinfo *);
void * phys_mem_alloc_pages(int);
void phys_mem_free_pages(void *);

#endif /* _KERNEL_MEMORY_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
