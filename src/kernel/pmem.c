/*_
 * Copyright (c) 2015-2016 Hirochika Asai <asai@jar.jp>
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

extern struct kmem *g_kmem;

/* Prototype declarations of static functions */
static int _pmem_buddy_split(struct pmem *, struct pmem_buddy *, int);
static void
_pmem_buddy_merge(struct pmem *, struct pmem_buddy *, struct pmem_page *, int);


/*
 * Allocate 2^order pages from the specified zone of a physical memory region
 *
 * SYNOPSIS
 *      void *
 *      pmem_alloc_pages(int zone, int order);
 *
 * DESCRIPTION
 *      The pmem_alloc_pages() function allocates 2^order pages of physical
 *      memory from the zone of a physical memory region specified by the zone
 *      argument.
 *
 * RETURN VALUES
 *      The pmem_alloc_pages() function returns a pointer to allocated physical
 *      memory.  If there is an error, it returns NULL.
 */
void *
pmem_alloc_pages(int zone, int order)
{
    int ret;
    u32 idx;
    struct pmem *pmem;
    size_t i;

    /* Get the pmem data structure from the global variable */
    pmem = g_kmem->pmem;

    /* Check the order */
    if ( order > PMEM_MAX_BUDDY_ORDER ) {
        return NULL;
    }

    /* Split the upper-order's buddy first if needed */
    ret = _pmem_buddy_split(pmem, &pmem->zones[zone].buddy, order);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Obtain the contiguous pages from the head */
    idx = pmem->zones[zone].buddy.heads[order];
    if ( PMEM_INVAL_INDEX == idx ) {
        return NULL;
    }
    pmem->zones[zone].buddy.heads[order] = pmem->pages[idx].next;

    /* Ensure the allocated pages are free */
    for ( i = 0; i < (1ULL << order); i++ ) {
        if ( !PMEM_IS_FREE(&pmem->pages[idx + i])
             || pmem->pages[idx + i].order != order ) {
            return NULL;
        }
    }
    /* Mark as used */
    for ( i = 0; i < (1ULL << order); i++ ) {
        pmem->pages[idx + i].flags |= PMEM_USED;
    }

    return (void *)PAGE_ADDR(idx);
}

/*
 * Allocate a page from the specified zone of a physical memory region
 *
 * SYNOPSIS
 *      void *
 *      pmem_alloc_page(int zone);
 *
 * DESCRIPTION
 *      The pmem_alloc_page() function allocates a pages of physical memory from
 *      the zone of a physical memory region specified by the zone argument.
 *
 * RETURN VALUES
 *      The pmem_alloc_page() function returns a pointer to allocated physical
 *      memory.  If there is an error, it returns NULL.
 */
void *
pmem_alloc_page(int zone)
{
    return pmem_alloc_pages(zone, 0);
}

/*
 * Allocate a superpage from the specified zone of a physical memory region
 *
 * SYNOPSIS
 *      void *
 *      pmem_alloc_superpage(int zone);
 *
 * DESCRIPTION
 *      The pmem_alloc_superpage() function allocates a superpages of physical
 *      memory from the zone of a physical memory region specified by the zone
 *      argument.
 *
 * RETURN VALUES
 *      The pmem_alloc_superpage() function returns a pointer to allocated
 *      physical memory.  If there is an error, it returns NULL.
 */
void *
pmem_alloc_superpage(int zone)
{
    return pmem_alloc_pages(zone, SP_SHIFT);
}

/*
 * Deallocate physical memory space pointed by a
 *
 * SYNOPSIS
 *      void
 *      pmem_free_pages(void *a);
 *
 * DESCRIPTION
 *      The pmem_free_pages() function deallocates the physical memory
 *      allocation pointed by a.
 *
 * RETURN VALUES
 *      The pmem_free_pages() function does not return a value.
 */
void
pmem_free_pages(void *a)
{
    struct pmem *pmem;
    int order;
    int zone;
    size_t i;
    off_t idx;

    /* Get the pmem data structure from the global kmem variable */
    pmem = g_kmem->pmem;

    /* Get the index of the first page of the memory space to be released */
    idx = PAGE_INDEX(a);

    /* Check if the page index is within the physical memory space */
    if ( (size_t)idx >= pmem->nr ) {
        /* Invalid argument */
        return;
    }

    /* Check the order and zone */
    order = pmem->pages[idx].order;
    zone = pmem->pages[idx].zone;
    for ( i = 0; i < (1ULL << order); i++ ) {
        if ( order != pmem->pages[idx + i].order
             || zone != pmem->pages[idx + i].zone ) {
            /* Invalid order or zone */
            return;
        }
    }

    /* Unmark the used flag */
    for ( i = 0; i < (1ULL << order); i++ ) {
        pmem->pages[idx + i].flags &= ~PMEM_USED;
    }

    /* Return the released pages to the buddy */
    pmem->pages[idx].next = pmem->zones[zone].buddy.heads[order];
    pmem->zones[zone].buddy.heads[order] = idx;

    /* Merge buddies if possible */
    _pmem_buddy_merge(pmem, &pmem->zones[zone].buddy, &pmem->pages[idx], order);
}

/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_pmem_buddy_split(struct pmem *pmem, struct pmem_buddy *buddy, int o)
{
    int ret;
    struct pmem_page *p0;
    struct pmem_page *p1;
    u32 next;
    size_t i;

    /* Check the head ofthe current order */
    if ( PMEM_INVAL_INDEX != buddy->heads[o] ) {
        /* At least one memory block is avaiable in this order, then nothing to
           do here. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= PMEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( PMEM_INVAL_INDEX == buddy->heads[o + 1] ) {
        /* The upper order is also empty, then try to split buddy at the one
           more upper buddy. */
        ret = _pmem_buddy_split(pmem, buddy, o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Save next at the upper order */
    next = pmem->pages[buddy->heads[o + 1]].next;
    /* Split it into two */
    p0 = &pmem->pages[buddy->heads[o + 1]];
    p1 = p0 + (1ULL << o);

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o;
    }

    /* Insert it to the list */
    p0->next = p1 - pmem->pages;
    p1->next = buddy->heads[o];
    buddy->heads[o] = p0 - pmem->pages;
    /* Remove the split one from the upper order */
    buddy->heads[o + 1] = next;

    return 0;
}

/*
 * Merge buddies onto the upper order if possible
 */
static void
_pmem_buddy_merge(struct pmem *pmem, struct pmem_buddy *buddy,
                  struct pmem_page *off, int o)
{
    struct pmem_page *p0;
    struct pmem_page *p1;
    struct pmem_page *prev;
    struct pmem_page *list;
    size_t pi;
    size_t i;

    /* Check the order first */
    if ( o + 1 >= PMEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order, then do not merge anymore */
        return;
    }

    /* Check the page whether it's within the physical memory space */
    pi = off - pmem->pages;
    if ( pi >= pmem->nr ) {
        /* Out of the physical memory region */
        return;
    }

    /* Get the first page of the buddy at the upper order */
    p0 = &pmem->pages[FLOOR(pi, PAGESIZE)];

    /* Get the neighboring buddy */
    p1 = p0 + (1ULL << o);

    /* Ensure that p0 and p1 are free */
    if ( !PMEM_IS_FREE(p0) || !PMEM_IS_FREE(p1) ) {
        return;
    }

    /* Check the order of p1 */
    if ( p0->order != o || p1->order != o ) {
        /* Cannot merge because of the order mismatch */
        return;
    }

    /* Remove both of the pair from the list of current order */
    /* Try to remove p0 */
    list = &pmem->pages[buddy->heads[o]];
    prev = NULL;
    while ( NULL != list ) {
        if ( p0 == list ) {
            if ( NULL == prev ) {
                buddy->heads[o] = p0->next;
            } else {
                prev->next = p0->next;
            }
            break;
        }
        /* Go to the next one */
        prev = list;
        if ( PMEM_INVAL_INDEX != list->next ) {
            list = &pmem->pages[list->next];
        } else {
            list = NULL;
        }
    }
    /* Try to remove p1 */
    list = &pmem->pages[buddy->heads[o]];
    prev = NULL;
    while ( NULL != list ) {
        if ( p1 == list ) {
            if ( NULL == prev ) {
                buddy->heads[o] = p1->next;
            } else {
                prev->next = p1->next;
            }
            break;
        }
        /* Go to the next one */
        prev = list;
        if ( PMEM_INVAL_INDEX != list->next ) {
            list = &pmem->pages[list->next];
        } else {
            list = NULL;
        }
    }

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o + 1;
    }

    /* Try to merge the upper order of buddies */
    _pmem_buddy_merge(pmem, buddy, p0, o + 1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
