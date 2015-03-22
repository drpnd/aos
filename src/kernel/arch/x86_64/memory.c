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
#include "arch.h"
#include "memory.h"
#include "../../kernel.h"

/* Flags */
#define PHYS_MEM_USED           (u64)1
#define PHYS_MEM_WIRED          (u64)(1<<1)
#define PHYS_MEM_HEAD           (u64)(1<<2)
#define PHYS_MEM_UNAVAIL        (u64)(1<<16)

#define PHYS_MEM_IS_FREE(x)     (0 == (x)->flags ? 1 : 0)

#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)

static struct phys_mem *phys_mem;

/*
 * Split the buddies so that we get at least one buddy at the order o
 */
static int
_split(struct phys_mem_buddy *buddy, int o)
{
    int ret;

    /* Check the head of the current order */
    if ( NULL != buddy->heads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= PHYS_MEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == buddy->heads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _split(buddy, o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Split into two */
    buddy->heads[o] = buddy->heads[o + 1];
    buddy->heads[o]->prev = NULL;
    buddy->heads[o]->next = (struct phys_mem_buddy_list *)((u64)buddy->heads[o]
                                                           + (1 << o));
    buddy->heads[o]->next->prev = buddy->heads[o];
    buddy->heads[o]->next->next = NULL;
    /* Remove the split one from the upper order */
    buddy->heads[o + 1] = buddy->heads[o + 1]->next;
    if ( NULL != buddy->heads[o + 1] ) {
        buddy->heads[o + 1]->prev = NULL;
    }

    return 0;
}

/*
 * Merge buddies onto the upper order on if possible
 */
static void
_merge(struct phys_mem_buddy *buddy, struct phys_mem_buddy_list *off, int o)
{
    int found;
    struct phys_mem_buddy_list *p0;
    struct phys_mem_buddy_list *p1;
    struct phys_mem_buddy_list *list;

    if ( o + 1 >= PHYS_MEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order */
        return;
    }

    /* Get the first page of the upper order */
    p0 = (struct phys_mem_buddy_list *)((u64)off / (1 << (o + 1))
                                        * (1 << (o + 1)));
    /* Get the neighboring buddy */
    p1 = (struct phys_mem_buddy_list *)((u64)p0 + (1 << o));

    /* Check the current level and remove the pairs */
    list = buddy->heads[o];
    found = 0;
    while ( NULL != list ) {
        if ( p0 == list || p1 == list ) {
            /* Found */
            found++;
            if ( 2 == found ) {
                /* Found both */
                break;
            }
        }
        /* Go to the next one */
        list = list->next;
    }
    if ( 2 != found ) {
        /* Either of the buddy is not free */
        return;
    }

    /* Remove both from the list */
    if ( p0->prev == NULL ) {
        /* Head */
        buddy->heads[o] = p0->next;
        if ( NULL != p0->next ) {
            p0->next->prev = buddy->heads[o];
        }
    } else {
        /* Otherwise */
        list = p0->prev;
        list->next = p0->next;
        if ( NULL != p0->next ) {
            p0->next->prev = list;
        }
    }
    if ( p1->prev == NULL ) {
        /* Head */
        buddy->heads[o] = p1->next;
        if ( NULL != p1->next ) {
            p1->next->prev = buddy->heads[o];
        }
    } else {
        /* Otherwise */
        list = p1->prev;
        list->next = p1->next;
        if ( NULL != p1->next ) {
            p1->next->prev = list;
        }
    }

    /* Prepend it to the upper order */
    p0->prev = NULL;
    p0->next = buddy->heads[o + 1];
    buddy->heads[o + 1] = p0;

    /* Try to merge the upper order of buddies */
    _merge(buddy, p0, o + 1);
}

/*
 * Initialize physical memory
 *    This is not thread safe.  Call this from BSP.
 */
int
phys_mem_init(struct bootinfo *bi)
{
    struct bootinfo_sysaddrmap_entry *bse;
    struct phys_mem_buddy_list *list;
    u64 nr;
    u64 addr;
    u64 sz;
    u64 a;
    u64 b;
    u64 i;
    u64 j;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return -1;
    }

    /* Obtain usable memory size */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Usable */
            if ( bse->base + bse->len > addr ) {
                /* Get the highest address */
                addr = bse->base + bse->len;
            }
        }
    }

    /* Calculate required memory size for pages */
    nr  = CEIL(addr, PAGESIZE) / PAGESIZE;
    sz = nr * sizeof(struct phys_mem_page) + sizeof(struct phys_mem);

    /* Search free space system address map obitaned from BIOS for the memory
       allocator (calculated above) */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE);
            b = FLOOR(bse->base + bse->len, PAGESIZE);

            if ( b < PHYS_MEM_FREE_ADDR ) {
                /* Skip */
                continue;
            } else if ( a < PHYS_MEM_FREE_ADDR ) {
                if ( b - PHYS_MEM_FREE_ADDR >= sz ) {
                    /* Found */
                    addr = PHYS_MEM_FREE_ADDR;
                    break;
                } else {
                    continue;
                }
            } else {
                if ( b - a >= sz ) {
                    addr = a;
                    break;
                } else {
                    continue;
                }
            }
        }
    }

    /* Could not find available pages for the management structure */
    if ( 0 == addr ) {
        return -1;
    }

    /* Setup the memory page management structure */
    phys_mem = (struct phys_mem *)(addr + nr * sizeof(struct phys_mem_page));
    phys_mem->nr = nr;
    phys_mem->pages = (struct phys_mem_page *)addr;

    /* Reset flags */
    for ( i = 0; i < phys_mem->nr; i++ ) {
        /* Mark as unavailable */
        phys_mem->pages[i].flags = PHYS_MEM_UNAVAIL;
    }

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE) / PAGESIZE;
            b = FLOOR(bse->base + bse->len, PAGESIZE) / PAGESIZE;

            /* Mark as unallocated */
            for ( j = a; j < b; j++ ) {
                if ( j >= phys_mem->nr ) {
                    /* Error */
                    return -1;
                }
                /* Unmark unavailable */
                phys_mem->pages[j].flags &= ~PHYS_MEM_UNAVAIL;
                if ( j * PAGESIZE <= PHYS_MEM_FREE_ADDR ) {
                    /* Wired by kernel */
                    phys_mem->pages[j].flags |= PHYS_MEM_WIRED;
                }
            }
        }
    }

    /* Mark self (used by phys_mem and phys_mem->pages) */
    for ( i = addr / PAGESIZE; i <= CEIL(addr + sz, PAGESIZE) / PAGESIZE; i++ ) {
        phys_mem->pages[i].flags |= PHYS_MEM_WIRED;
    }

    /* Initialize buddy system */
    for ( i = 0; i < PHYS_MEM_MAX_BUDDY_ORDER; i++ ) {
        phys_mem->buddy.heads[i] = NULL;
    }

    /* Add all pages to the buddy system */
    for ( i = 0; i < phys_mem->nr; i++ ) {
        /* Do it from the highest pages so that the initial buddies in the list
           becomes ascending order because the following code tries to `prepend'
           a page to the list */
        j = phys_mem->nr - i - 1;
        /* Check whether this page is free */
        if ( !PHYS_MEM_IS_FREE(&phys_mem->pages[j]) ) {
            continue;
        }
        /* Prepend this page to the order 0 in the buddy system */
        list = (struct phys_mem_buddy_list *)(j * PAGESIZE);
        list->prev = NULL;
        list->next = phys_mem->buddy.heads[0];
        if ( NULL != phys_mem->buddy.heads[0] ) {
            phys_mem->buddy.heads[0]->prev = list;
        }
        phys_mem->buddy.heads[0] = list;
        /* Try to merge contiguous pages in the buddy system */
        _merge(&phys_mem->buddy, list, 0);

        __asm__ __volatile__ (" movq %%rax,%%dr0 " :: "a"(i) );
    }

    return 0;
}

/*
 * Allocate 2**order physical pages
 */
void *
phys_mem_alloc_pages(int order)
{
    int ret;
    struct phys_mem_buddy_list *a;

    /* Check the argument */
    if ( order < 0 ) {
        /* Invalid argument */
        return NULL;
    }

    /* Check the size */
    if ( order >= PHYS_MEM_MAX_BUDDY_ORDER ) {
        /* Oversized request */
        return NULL;
    }

    /* Split first if needed */
    ret = _split(&phys_mem->buddy, order);
    if ( ret < 0 ) {
        /* No memory available */
        return NULL;
    }

    /* Obtain from the head */
    a = phys_mem->buddy.heads[order];
    phys_mem->buddy.heads[order] = a->next;
    if ( NULL != phys_mem->buddy.heads[order] ) {
        phys_mem->buddy.heads[order]->prev = NULL;
    }

    /* Clear the memory for security */
    kmemset(a, 0, 1 << order);

    return a;
}

/*
 * Free allocated 2**order pages
 */
void
phys_mem_free_pages(void *a, int order)
{
    u64 p;
    struct phys_mem_buddy_list *list;

    /* Get the index of the page */
    p = (u64)a / PAGESIZE;
    if ( p >= phys_mem->nr ) {
        /* Invalid page number */
        return;
    }
    /* Ensure to be aligned */
    a = (void *)(p * PAGESIZE);

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order >= PHYS_MEM_MAX_BUDDY_ORDER ) {
        /* Something is wrong... */
        return;
    }

    /* Return it to the buddy system */
    list = phys_mem->buddy.heads[order];
    /* Prepend the returned pages */
    phys_mem->buddy.heads[order] = a;
    phys_mem->buddy.heads[order]->prev = NULL;
    phys_mem->buddy.heads[order]->next = list;
    if ( NULL != list ) {
        list->prev = a;
    }

    _merge(&phys_mem->buddy, a, 0);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
