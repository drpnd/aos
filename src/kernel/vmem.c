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

/* Prototype declarations of static functions */
static int _vmem_buddy_order(struct vmem_region *, size_t);
static int _vmem_buddy_spg_split(struct vmem_region *, int);
static void
_vmem_buddy_spg_merge(struct vmem_region *, struct vmem_superpage *, int);
static int _vmem_buddy_pg_split(struct vmem_region *, int);
static void _vmem_buddy_pg_merge(struct vmem_region *, struct vmem_page *, int);

static struct vmem_region * _vmem_search_region(struct vmem_space *, void *);

/*
 * Allocate virtual pages
 */
void *
vmem_alloc_pages(struct vmem_space *space, int order)
{
    void *paddr;
    void *vaddr;

    /* Check the order */
    if ( order >= SP_SHIFT ) {
        /* Allocate virtual superspace */
        vaddr = vmem_buddy_alloc_superpages(space, order - SP_SHIFT);
        if ( NULL == vaddr ) {
            return NULL;
        }
    } else {
        /* Allocate virtual pages */
        vaddr = vmem_buddy_alloc_pages(space, order);
        if ( NULL == vaddr ) {
            return NULL;
        }
    }

    /* Allocate physical pages */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
    if ( NULL == paddr ) {
        return NULL;
    }

    /* FIXME: Map page table */
    panic("FIXME: vmem_alloc_pages()");

    return vaddr;

#if 0
    pg = vmem_grab_pages(kmem->space, order);
    if ( NULL != pg ) {
        /* Found */
        vaddr = pg->superpage->region->start
            + SUPERPAGE_ADDR(pg->superpage - pg->superpage->region->superpages)
            + PAGE_ADDR(pg - pg->superpage->u.page.pages);

        /* Allocate physical memory */
        paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
        if ( NULL == paddr ) {
            /* Release the virtual memory */
            vmem_return_pages(pg);
            return NULL;
        }

        /* Map the physical and virtual memory */
        ret = arch_vmem_map(kmem->space, vaddr, paddr, pg->flags);
        if ( ret < 0 ) {
            /* Release the virtual and physical memory */
            vmem_return_pages(pg);
            pmem_free_pages(paddr);
            return NULL;
        }

        /* Return */
        return vaddr;
    }

    /* Try to grab a superpage from the kernel memory space */
    spg = vmem_grab_superpages(kmem->space, 0);
    if ( NULL != spg ) {
        /* Found */
        vaddr = spg->region->start
            + SUPERPAGE_ADDR(spg - spg->region->superpages);

        /* Allocate physical memory */
        paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
        if ( NULL == paddr ) {
            /* Release the virtual memory */
            vmem_return_superpages(spg);
            return NULL;
        }

        /* Map the physical and virtual memory */
        //ret = arch_vmem_map(kmem->space, vaddr, paddr, spg->flags);

        return vaddr;
    }
#endif
}

/*
 * Deallocate virtual memory space pointed by a
 */
void
vmem_free_pages(struct vmem_space *space, void *a)
{
    struct vmem_region *reg;
    off_t spgidx;
    off_t pgidx;
    struct vmem_superpage *spg;
    struct vmem_page *pg;

    /* Search the corresponding region for the virtual address pointed by a */
    reg = _vmem_search_region(space, a);
    if ( NULL == reg ) {
        /* Could not find any regions corresponding to the space pointed by a */
        return;
    }

    /* Found, then get the indexes of the superpage/page */
    spgidx = SUPERPAGE_INDEX(a - reg->start);
    pgidx = PAGE_INDEX(a - SUPERPAGE_ADDR(spgidx));

    /* Get the pointer to the superpage */
    spg = &reg->superpages[spgidx];
    if ( VMEM_IS_SUPERPAGE(spg) ) {
        /* The corresponding superpage is "superpage".  */
        vmem_buddy_free_superpages(space, spg);
    } else {
        /* The corresponding superpage is a set of pages. */
        pg = &spg->u.page.pages[pgidx];
        vmem_buddy_free_pages(space, pg);
    }
}

/*
 * Search the start address of available region
 */
void *
vmem_search_available_region(struct vmem_space *space, size_t size)
{
    struct vmem_region *reg;
    reg_t maxaddr;

    /* Search the maximum address in the space */
    reg = space->first_region;
    maxaddr = 0;
    while ( NULL != reg ) {
        if ( (reg_t)reg->start + reg->len > maxaddr ) {
            maxaddr = (reg_t)reg->start + reg->len;
        }
        reg = reg->next;
    }

    /* Check the range of the physical-memory address */
    if ( (1ULL << arch_address_width()) < size
         || maxaddr >= (1ULL << arch_address_width()) - size ) {
        /* Out of the range */
        return NULL;
    }

    return (void *)maxaddr;
}






















/*
 * Virtual memory region
 */
struct vmem_region *
vmem_region_create(void)
{
    struct vmem_region *reg;
    struct vmem_superpage *spgs;
    size_t i;
    int ret;

    /* Allocate a new virtual memory region */
    reg = kmalloc(sizeof(struct vmem_region));
    if ( NULL == reg ) {
        /* Cannot allocate a virtual memory region */
        return NULL;
    }
    kmemset(reg, 0, sizeof(struct vmem_region));
    reg->start = (void *)(1ULL << 30);
    reg->len = SUPERPAGESIZE * 512; /* 1 GiB */
    reg->next = NULL;

    /* Allocate virtual pages for this region */
    spgs = kmalloc(sizeof(struct vmem_superpage) * reg->len / SUPERPAGESIZE);
    if ( NULL == spgs ) {
        /* Cannot allocate the virtual pages */
        kfree(reg);
        return NULL;
    }

    /* Initialize the all pages */
    for ( i = 0; i < reg->len / SUPERPAGESIZE; i++ ) {
        spgs[i].u.superpage.addr = 0;
        spgs[i].order = 0;
        spgs[i].flags = VMEM_SUPERPAGE | VMEM_USABLE;
        spgs[i].region = reg;
        spgs[i].next = NULL;
        spgs[i].prev = NULL;
    }
    reg->superpages = spgs;

    ret = vmem_buddy_init(reg);
    if ( ret < 0 ) {
        kfree(spgs);
        kfree(reg);
        return NULL;
    }

    return reg;
}

/*
 * Virtual memory preparation
 */
struct vmem_space *
vmem_space_create(void)
{
    struct vmem_space *space;
    struct vmem_region *reg;

    /* Allocate a new virtual memory space */
    space = kmalloc(sizeof(struct vmem_space));
    if ( NULL == space ) {
        return NULL;
    }
    kmemset(space, 0, sizeof(struct vmem_space));

    /* Allocate a new virtual memory region */
    reg = vmem_region_create();
    if ( NULL == reg ) {
        /* Cannot allocate a virtual memory region */
        kfree(space);
        return NULL;
    }

    /* Set the region to the first region */
    space->first_region = reg;

    /* Initialize the architecture-specific data structure */
    if ( arch_vmem_init(space) < 0 ) {
        /* FIXME: Release pages in the region */
        kfree(space);
        return NULL;
    }

    return space;
}

/*
 * Delete the virtual memory process
 */
void
vmem_space_delete(struct vmem_space *vmem)
{
    /* FIXME: Implement this function */
    return;
}

/*
 * Copy a virtual memory space
 */
struct vmem_space *
vmem_space_copy(struct vmem_space *vmem)
{
    struct vmem_space *space;
    struct vmem_region *reg;

    /* Allocate a new virtual memory space */
    space = kmalloc(sizeof(struct vmem_space));
    if ( NULL == space ) {
        return NULL;
    }
    kmemset(space, 0, sizeof(struct vmem_space));

    /* Allocate a new virtual memory region */
    reg = vmem_region_create();
    if ( NULL == reg ) {
        /* Cannot allocate a virtual memory region */
        kfree(space);
        return NULL;
    }

    /* Set the region to the first region */
    space->first_region = reg;

    /* Initialize the architecture-specific data structure */
    if ( arch_vmem_init(space) < 0 ) {
        /* FIXME: Release pages in the region */
        kfree(space);
        return NULL;
    }

    /* FIXME: Copy */

    return space;
}

/*
 * Search the corresponding region from the virtual address
 */
static struct vmem_region *
_vmem_search_region(struct vmem_space *vmem, void *vaddr)
{
    struct vmem_region *reg;

    /* Search from the first region */
    reg = vmem->first_region;
    while ( NULL != reg ) {
        if ( (reg_t)vaddr >= (reg_t)reg->start
             && (reg_t)vaddr < (reg_t)reg->start + reg->len ) {
            /* Found */
            return reg;
        }
        reg = reg->next;
    }

    return NULL;
}




/*
 * Create buddy system for the specified virtual memory region
 */
int
vmem_buddy_init(struct vmem_region *reg)
{
    size_t i;
    size_t j;
    int o;

    /* For superpages */
    for ( i = 0; i <= VMEM_MAX_BUDDY_ORDER; i++ ) {
        reg->spgheads[i] = NULL;
    }
    /* For pages */
    for ( i = 0; i <= SP_SHIFT; i++ ) {
        reg->pgheads[i] = NULL;
    }

    /* Look through all the pages */
    for ( i = 0; i < reg->len / SUPERPAGESIZE; i += (1ULL << o) ) {
        o = _vmem_buddy_order(reg, i);
        if ( o < 0 ) {
            /* This page is not usable. */
            o = 0;
        } else {
            /* This page is usable. */
            for ( j = 0; j < (1ULL << o); j++ ) {
                reg->superpages[i + j].order = o;
            }
            /* Add this to the buddy system at the order of o */
            reg->superpages[i].prev = NULL;
            reg->superpages[i].next = reg->spgheads[o];
            if ( NULL != reg->spgheads[o] ) {
                reg->spgheads[o]->prev = &reg->superpages[i];
            }
            reg->spgheads[o] = &reg->superpages[i];
        }
    }

    return 0;
}

/*
 * Count the virtual memory order for buddy system
 */
static int
_vmem_buddy_order(struct vmem_region *reg, size_t pg)
{
    int o;
    size_t i;
    size_t npg;

    /* Calculate the number of pages */
    npg = reg->len / SUPERPAGESIZE;

    /* Check the order for contiguous usable pages */
    for ( o = 0; o <= VMEM_MAX_BUDDY_ORDER; o++ ) {
        for ( i = pg; i < pg + (1ULL << o); i++ ) {
            if ( !VMEM_IS_FREE(&reg->superpages[pg]) ) {
                /* It contains an unusable page, then return the current order
                   minus 1, immediately. */
                return o - 1;
            }
        }
        /* Test whether the next order is feasible; feasible if it is properly
           aligned and the pages are within the range of this region. */
        if ( 0 != (pg & (1ULL << o)) || pg + (1ULL << (o + 1)) > npg ) {
            /* Infeasible, then return the current order immediately */
            return o;
        }
    }

    /* Reaches the maximum order */
    return VMEM_MAX_BUDDY_ORDER;
}

/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_vmem_buddy_spg_split(struct vmem_region *reg, int o)
{
    int ret;
    struct vmem_superpage *p0;
    struct vmem_superpage *p1;
    struct vmem_superpage *next;
    size_t i;

    /* Check the head of the current order */
    if ( NULL != reg->spgheads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= VMEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == reg->spgheads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _vmem_buddy_spg_split(reg, o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Save next at the upper order */
    next = reg->spgheads[o + 1]->next;
    /* Split it into two */
    p0 = reg->spgheads[o + 1];
    p1 = p0 + (1ULL << o);

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o;
    }

    /* Insert it to the list */
    p0->prev = NULL;
    p0->next = p1;
    p1->prev = p0;
    p1->next = reg->spgheads[o];
    reg->spgheads[o]->prev = p1;
    reg->spgheads[o] = p0;
    /* Remove the split one from the upper order */
    reg->spgheads[o + 1] = next;
    next->prev = NULL;

    return 0;
}

/*
 * Merge buddies onto the upper order on if possible
 */
static void
_vmem_buddy_spg_merge(struct vmem_region *reg, struct vmem_superpage *off,
                      int o)
{
    struct vmem_superpage *p0;
    struct vmem_superpage *p1;
    size_t pi;
    size_t i;

    if ( o + 1 >= VMEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order */
        return;
    }

    /* Check the region */
    pi = off - reg->superpages;
    if ( pi >= reg->len / SUPERPAGESIZE ) {
        /* Out of this region */
        return;
    }

    /* Get the first page of the upper order */
    p0 = &reg->superpages[FLOOR(pi, 1ULL << (o + 1))];

    /* Get the neighboring buddy */
    p1 = p0 + (1ULL << o);

    /* Ensure that p0 and p1 are free */
    if ( !VMEM_IS_FREE(p0) || !VMEM_IS_FREE(p1) ) {
        return;
    }

    /* Check the order of p1 */
    if ( p0->order != o || p1->order != o ) {
        /* Cannot merge because of the order mismatch */
        return;
    }

    /* Remove both of the pair from the list of current order */
    if ( p0->prev == NULL ) {
        /* Head */
        reg->spgheads[o] = p0->next;
        reg->spgheads[o]->prev = NULL;
    } else {
        /* Otherwise */
        p0->prev->next = p0->next;
        p0->next->prev = p0->prev;
    }
    if ( p1->prev == NULL ) {
        /* Head; note that this occurs if the p0 was the head and p1 was next to
           p0. */
        reg->spgheads[o] = p1->next;
        reg->spgheads[o]->prev = NULL;
    } else {
        p1->prev->next = p1->next;
        p1->next->prev = p1->prev;
    }

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o + 1;
    }

    /* Prepend it to the upper order */
    p0->prev = NULL;
    p0->next = reg->spgheads[o + 1];
    reg->spgheads[o + 1]->prev = p0;
    reg->spgheads[o + 1] = p0;

    /* Try to merge the upper order of buddies */
    _vmem_buddy_spg_merge(reg, p0, o + 1);
}

/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_vmem_buddy_pg_split(struct vmem_region *reg, int o)
{
    int ret;
    struct vmem_page *p0;
    struct vmem_page *p1;
    struct vmem_page *next;
    size_t i;

    /* Check the head of the current order */
    if ( NULL != reg->pgheads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= SP_SHIFT ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == reg->pgheads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _vmem_buddy_pg_split(reg, o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Save next at the upper order */
    next = reg->pgheads[o + 1]->next;
    /* Split it into two */
    p0 = reg->pgheads[o + 1];
    p1 = p0 + (1ULL << o);

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o;
    }

    /* Insert it to the list */
    p0->prev = NULL;
    p0->next = p1;
    p1->prev = p0;
    p1->next = reg->pgheads[o];
    reg->pgheads[o]->prev = p1;
    reg->pgheads[o] = p0;
    /* Remove the split one from the upper order */
    reg->pgheads[o + 1] = next;
    next->prev = NULL;

    return 0;
}

/*
 * Merge buddies onto the upper order on if possible
 */
static void
_vmem_buddy_pg_merge(struct vmem_region *reg, struct vmem_page *off, int o)
{
    struct vmem_page *p0;
    struct vmem_page *p1;
    size_t pi;
    size_t spi;
    size_t i;

    if ( o + 1 >= SP_SHIFT ) {
        /* Reached the maximum order */
        return;
    }

    /* Check the region for the corresponding superpage */
    spi = off->superpage - reg->superpages;
    if ( spi >= reg->len / SUPERPAGESIZE ) {
        /* Out of this region */
        return;
    }
    if ( VMEM_IS_SUPERPAGE(&reg->superpages[spi]) ) {
        return;
    }
    /* Check the page index */
    pi = off - reg->superpages[spi].u.page.pages;
    if ( pi >= SUPERPAGESIZE / PAGESIZE ) {
        return;
    }

    /* Get the first page of the upper order */
    p0 = &reg->superpages[spi].u.page.pages[FLOOR(pi, 1ULL << (o + 1))];

    /* Get the neighboring buddy */
    p1 = p0 + (1ULL << o);

    /* Ensure that p0 and p1 are free */
    if ( !VMEM_IS_FREE(p0) || !VMEM_IS_FREE(p1) ) {
        return;
    }

    /* Check the order of p1 */
    if ( p0->order != o || p1->order != o ) {
        /* Cannot merge because of the order mismatch */
        return;
    }

    /* Remove both of the pair from the list of current order */
    if ( p0->prev == NULL ) {
        /* Head */
        reg->pgheads[o] = p0->next;
        reg->pgheads[o]->prev = NULL;
    } else {
        /* Otherwise */
        p0->prev->next = p0->next;
        p0->next->prev = p0->prev;
    }
    if ( p1->prev == NULL ) {
        /* Head; note that this occurs if the p0 was the head and p1 was next to
           p0. */
        reg->pgheads[o] = p1->next;
        reg->pgheads[o]->prev = NULL;
    } else {
        p1->prev->next = p1->next;
        p1->next->prev = p1->prev;
    }

    /* Set the order for all the pages in the pair */
    for ( i = 0; i < (1ULL << (o + 1)); i++ ) {
        p0[i].order = o + 1;
    }

    /* Prepend it to the upper order */
    p0->prev = NULL;
    p0->next = reg->pgheads[o + 1];
    reg->pgheads[o + 1]->prev = p0;
    reg->pgheads[o + 1] = p0;

    /* Try to merge the upper order of buddies */
    _vmem_buddy_pg_merge(reg, p0, o + 1);
}






/*
 * Allocate virtual pages
 */
void *
vmem_buddy_alloc_superpages(struct vmem_space *space, int order)
{
    struct vmem_region *reg;
    struct vmem_superpage *vpage;
    ssize_t i;
    int ret;

    /* Check the order first */
    if ( order < 0 || order > VMEM_MAX_BUDDY_ORDER ) {
        /* Invalid order */
        return NULL;
    }

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        /* Split first if needed */
        ret = _vmem_buddy_spg_split(reg, order);
        if ( ret >= 0 ) {
            /* Get one from the buddy system, and remove that from the list */
            vpage = reg->spgheads[order];
            reg->spgheads[order] = vpage->next;
            if ( NULL != reg->spgheads[order] ) {
                reg->spgheads[order]->prev = NULL;
            }

            /* Validate all the pages are not used */
            for ( i = 0; i < (1LL << order); i++ ) {
                if ( !VMEM_IS_FREE(&vpage[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous pages as "used" */
            for ( i = 0; i < (1LL << order); i++ ) {
                vpage[i].flags |= VMEM_USED;
            }

            /* Return the first page of the allocated pages */
            return reg->start + SUPERPAGE_ADDR(vpage - reg->superpages);
        }

        /* Next region */
        reg = reg->next;
    }

    return NULL;
}

/*
 * Deallocate virtual memory space pointed by a
 */
void
vmem_buddy_free_superpages(struct vmem_space *space, void *a)
{
    struct vmem_region *reg;
    off_t idx;
    int order;
    size_t i;

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        if ( (reg_t)a >= (reg_t)reg->start
             && (reg_t)a < (reg_t)reg->start + reg->len) {
            /* Get the index of the first page of the memory space to be
               released */
            idx = SUPERPAGE_INDEX(a - reg->start);

            /* Check the order */
            order = reg->superpages[idx].order;
            for ( i = 0; i < (1ULL << order); i++ ) {
                if ( order != reg->superpages[idx + i].order ) {
                    /* Invalid order */
                    return;
                }
            }

            /* Unmark the used flag */
            for ( i = 0; i < (1ULL << order); i++ ) {
                reg->superpages[idx + i].flags &= ~VMEM_USED;
            }

            /* Return the released pages to the buddy */
            reg->superpages[idx].prev = NULL;
            reg->superpages[idx].next = reg->spgheads[order];
            if ( NULL != reg->spgheads[order] ) {
                reg->spgheads[order]->prev = &reg->superpages[idx];
            }

            /* Merge buddies if possible */
            _vmem_buddy_spg_merge(reg, &reg->superpages[idx], order);

            return;
        }

        /* Next region */
        reg = reg->next;
    }
}


/*
 * Allocate virtual pages
 */
void *
vmem_buddy_alloc_pages(struct vmem_space *space, int order)
{
    struct vmem_region *reg;
    struct vmem_page *vpage;
    struct vmem_superpage *spg;
    ssize_t i;
    int ret;

    /* Check the order first */
    if ( order < 0 || order > SP_SHIFT ) {
        /* Invalid order */
        return NULL;
    }

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        /* Split first if needed */
        ret = _vmem_buddy_pg_split(reg, order);
        if ( ret < 0 ) {
            /* Take pages from a superpage */
            ret = _vmem_buddy_spg_split(reg, 0);
            if ( ret < 0 ) {
                /* No pages available, the search the next region */
                reg = reg->next;
                continue;
            }
            spg = reg->spgheads[0];
            reg->spgheads[0] = spg->next;
            if ( NULL != reg->spgheads[0] ) {
                reg->spgheads[0]->prev = NULL;
            }

            /* Make this superpage to a set of pages */
            spg->flags &= ~VMEM_SUPERPAGE;
            spg->u.page.pages = NULL;

            /* Add the pages to the list */
            //reg->pgheads[order];
            char buf[1024];
            ksnprintf(buf, sizeof(buf), "xxx %016x", 1000);
            panic(buf);

            /* FIXME: Make this superpage pages */
        }

        if ( ret >= 0 ) {
            /* Get one from the buddy system, and remove that from the list */
            vpage = reg->pgheads[order];
            reg->pgheads[order] = vpage->next;
            if ( NULL != reg->pgheads[order] ) {
                reg->spgheads[order]->prev = NULL;
            }

            /* Validate all the pages are not used */
            for ( i = 0; i < (1LL << order); i++ ) {
                if ( !VMEM_IS_FREE(&vpage[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous pages as "used" */
            for ( i = 0; i < (1LL << order); i++ ) {
                vpage[i].flags |= VMEM_USED;
            }

            /* Return the first page of the allocated pages */
            return reg->start
                + SUPERPAGE_ADDR(vpage->superpage - reg->superpages)
                + PAGE_ADDR(vpage - vpage->superpage->u.page.pages);
        }

        /* Next region */
        reg = reg->next;
    }

    return NULL;
}

/*
 * Deallocate virtual memory space pointed by a
 */
void
vmem_buddy_free_pages(struct vmem_space *space, void *a)
{
    struct vmem_region *reg;
    off_t idx;
    off_t spi;
    int order;
    size_t i;

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        if ( (reg_t)a >= (reg_t)reg->start
             && (reg_t)a < (reg_t)reg->start + reg->len) {
            /* Get the index of the first superpage of the memory space to be
               released */
            spi = SUPERPAGE_INDEX(a - reg->start);
            if ( VMEM_IS_SUPERPAGE(&reg->superpages[spi]) ){
                return;
            }
            idx = PAGE_INDEX(a - reg->start) % (SUPERPAGESIZE / PAGESIZE);

            /* Check the order */
            order = reg->superpages[spi].u.page.pages[idx].order;
            for ( i = 0; i < (1ULL << order); i++ ) {
                if ( order
                     != reg->superpages[spi].u.page.pages[idx + i].order ) {
                    /* Invalid order */
                    return;
                }
            }

            /* Unmark the used flag */
            for ( i = 0; i < (1ULL << order); i++ ) {
                reg->superpages[spi].u.page.pages[idx + i].flags &= ~VMEM_USED;
            }

            /* Return the released pages to the buddy */
            reg->superpages[spi].u.page.pages[idx].prev = NULL;
            reg->superpages[spi].u.page.pages[idx].next = reg->pgheads[order];
            if ( NULL != reg->pgheads[order] ) {
                reg->pgheads[order]->prev
                    = &reg->superpages[spi].u.page.pages[idx];
            }

            /* Merge buddies if possible */
            _vmem_buddy_pg_merge(reg, &reg->superpages[spi].u.page.pages[idx],
                                 order);

            /* FIXME: Return to a superpage */
            return;
        }

        /* Next region */
        reg = reg->next;
    }
}

#if 0
/*
 * Superpage to pages
 */
static int
_vmem_superpage_to_pages(struct vmem_region *reg, struct vmem_superpage *spg)
{
    size_t sz;
    void *paddr;
    int order;

    /* Calculate the size of the array of the page data structure */
    sz = sizeof(struct vmem_page) * SUPERPAGESIZE / PAGESIZE;
    order = bitwidth(DIV_CEIL(sz, PAGESIZE));

    /* Allocate physical memory */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);

    /* Allocate virtual memory */


    return -1;
}
#endif

/*
 * Find superpages
 */
struct vmem_superpage *
vmem_grab_superpages(struct vmem_space *space, int order)
{
    struct vmem_region *reg;
    struct vmem_superpage *spg;
    int ret;
    ssize_t i;

    /* Check the order first */
    if ( order < 0 || order > VMEM_MAX_BUDDY_ORDER ) {
        /* Invalid order */
        return NULL;
    }

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        /* Split first if needed */
        ret = _vmem_buddy_spg_split(reg, order);
        if ( ret >= 0 ) {
            /* Get one from the buddy system, and remove that from the list */
            spg = reg->spgheads[order];
            reg->spgheads[order] = spg->next;
            if ( NULL != reg->spgheads[order] ) {
                reg->spgheads[order]->prev = NULL;
            }

            /* Validate all the pages are not used*/
            for ( i = 0; i < (1LL << order); i++ ) {
                if ( !VMEM_IS_FREE(&spg[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous superpages as "used" */
            for ( i = 0; i < (1LL << order); i++ ) {
                spg[i].flags |= VMEM_USED;
            }

            /* Return the first superpage of the allocated memory */
            return spg;
        }
        reg = reg->next;
    }

    return NULL;
}

/*
 * Release superpages
 */
void
vmem_return_superpages(struct vmem_superpage *spg)
{
    struct vmem_region *reg;
    int order;
    ssize_t i;

    /* Get the corresponding region */
    reg = spg->region;

    /* Get the order */
    order = spg->order;

    /* Check the argument of superpage is aligned */
    if ( (spg - spg->region->superpages) & ((1ULL << order) - 1) ) {
        /* Not aligned */
        return;
    }

    /* Check the flags and order of all the superpages */
    for ( i = 0; i < (1LL << order); i++ ) {
        if ( order != spg[i].order || VMEM_IS_FREE(&spg[i]) ) {
            /* Invalid order or not used */
            return;
        }
    }

    /* Unmark "used" */
    for ( i = 0; i < (1LL << order); i++ ) {
        spg[i].flags &= ~VMEM_USED;
    }

    /* Return the released pages to the buddy */
    spg->prev = NULL;
    spg->next = reg->spgheads[order];
    if ( NULL != reg->spgheads[order] ) {
        reg->spgheads[order]->prev = spg;
    }

    /* Merge buddies if possible */
    _vmem_buddy_spg_merge(reg, spg, order);
}

/*
 * Find pages
 */
struct vmem_page *
vmem_grab_pages(struct vmem_space *space, int order)
{
    struct vmem_region *reg;
    struct vmem_page *pg;
    int ret;
    ssize_t i;

    /* Check the order first */
    if ( order < 0 || order > SP_SHIFT ) {
        /* Invalid order */
        return NULL;
    }

    /* Walking through the region */
    reg = space->first_region;
    while ( NULL != reg ) {
        /* Split first if needed */
        ret = _vmem_buddy_pg_split(reg, order);
        if ( ret >= 0 ) {
            /* Get one from the buddy system, and remove that from the list */
            pg = reg->pgheads[order];
            reg->pgheads[order] = pg->next;
            if ( NULL != reg->pgheads[order] ) {
                reg->pgheads[order]->prev = NULL;
            }

            /* Validate all the pages are not used*/
            for ( i = 0; i < (1LL << order); i++ ) {
                if ( !VMEM_IS_FREE(&pg[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous superpages as "used" */
            for ( i = 0; i < (1LL << order); i++ ) {
                pg[i].flags |= VMEM_USED;
            }

            /* Return the first superpage of the allocated memory */
            return pg;
        }
        reg = reg->next;
    }

    return NULL;
}

/*
 * Release pages
 */
void
vmem_return_pages(struct vmem_page *pg)
{
    struct vmem_region *reg;
    struct vmem_superpage *spg;
    int order;
    ssize_t i;

    /* Get the parent superpage */
    spg = pg->superpage;

    /* Get the corresponding region */
    reg = spg->region;

    /* Get the order */
    order = pg->order;

    /* Check the argument of superpage is aligned */
    if ( (pg - spg->u.page.pages) & ((1ULL << order) - 1) ) {
        /* Not aligned */
        return;
    }

    /* Check the flags and order of all the pages */
    for ( i = 0; i < (1LL << order); i++ ) {
        if ( order != pg[i].order || VMEM_IS_FREE(&pg[i]) ) {
            /* Invalid order or not used */
            return;
        }
    }

    /* Unmark "used" */
    for ( i = 0; i < (1LL << order); i++ ) {
        pg[i].flags &= ~VMEM_USED;
    }

    /* Return the released pages to the buddy */
    pg->prev = NULL;
    pg->next = reg->pgheads[order];
    if ( NULL != reg->pgheads[order] ) {
        reg->pgheads[order]->prev = pg;
    }

    /* Merge buddies if possible */
    _vmem_buddy_pg_merge(reg, pg, order);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
