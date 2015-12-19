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

struct kmem *g_kmem;

u64 binorder(u64);
int kmem_remap(u64, u64, int);
int vmem_arch_init(struct vmem_space *);
int vmem_remap(struct vmem_space *, u64, u64, int);
u64 vmem_paddr(struct vmem_space *, u64);

/* Prototype declarations of static functions */
static int _vmem_buddy_order(struct vmem_region *, size_t);
static int _vmem_buddy_spg_split(struct vmem_region *, int);
static void
_vmem_buddy_spg_merge(struct vmem_region *, struct vmem_superpage *, int);
static int _vmem_buddy_pg_split(struct vmem_region *, int);
static void _vmem_buddy_pg_merge(struct vmem_region *, struct vmem_page *, int);


/*
 * Allocate kernel pages
 */
void *
kmem_alloc_pages(int order)
{
    struct kmem *kmem;
    void *paddr;
    void *vaddr;

    /* Get the global variable */
    kmem = g_kmem;

    /* Allocate physical pages */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
    if ( NULL == paddr ) {
        return NULL;
    }

    /* Check the order */
    if ( order >= SP_SHIFT ) {
        /* Allocate virtual space */
        vaddr = vmem_buddy_alloc_superpages(kmem->space, order - SP_SHIFT);
        if ( NULL == vaddr ) {
            pmem_free_pages(paddr);
            return NULL;
        }
    } else {
        /* FIXME */
        vaddr = NULL;
    }

    return vaddr;
}

/*
 * Release the kernel pages starting from vaddr
 */
void
kmem_free_pages(void *vaddr)
{
    int pgnr;
    u64 paddr;
    struct kmem_page *kpage;
#if 0
    /* Resolve physical page */
    pgnr = (u64)vaddr / SUPERPAGESIZE;
    if ( pgnr < 512 ) {
        /* Region1 */
        kpage = &kmem->region1[pgnr];
    } else if ( pgnr >= 1536 && pgnr < 2048 ) {
        /* Region2 */
        kpage = &kmem->region2[pgnr - 1536];
    } else {
        return;
    }
#endif
    //_kpage_free(kpage);
    //paddr = kmem_paddr((u64)vaddr);
    //pmem_free_pages(&pmem->superpages[(u64)paddr / SUPERPAGESIZE]);
}


/*
 * Allocate memory space.
 * Note that the current implementation does not protect the slab header, so
 * the allocated memory must be carefully used.
 *
 * SYNOPSIS
 *      void *
 *      kmalloc(size_t size);
 *
 * DESCRIPTION
 *      The kmalloc() function allocates size bytes of contiguous memory.
 *
 * RETURN VALUES
 *      The kmalloc() function returns a pointer to allocated memory.  If there
 *      is an error, it returns NULL.
 */
void *
kmalloc(size_t size)
{
    size_t o;
    void *ptr;
    int nr;
    size_t s;
    int i;
    struct kmem_slab *hdr;

    /* Get the binary order */
    o = binorder(size);

    /* Align the order */
    if ( o < KMEM_SLAB_BASE_ORDER ) {
        o = 0;
    } else {
        o -= KMEM_SLAB_BASE_ORDER;
    }

    /* Lock */
    spin_lock(&g_kmem->slab_lock);

    if ( o < KMEM_SLAB_ORDER ) {
        /* Small object: Slab allocator */
        if ( NULL != g_kmem->slab.gslabs[o].partial ) {
            /* Partial list is available. */
            hdr = g_kmem->slab.gslabs[o].partial;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + KMEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                g_kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = g_kmem->slab.gslabs[o].full;
                g_kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Search free space for the next allocation */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else if ( NULL != g_kmem->slab.gslabs[o].free ) {
            /* Partial list is empty, but free list is available. */
            hdr = g_kmem->slab.gslabs[o].free;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + KMEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                g_kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = g_kmem->slab.gslabs[o].full;
                g_kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = g_kmem->slab.gslabs[o].partial;
                g_kmem->slab.gslabs[o].partial = hdr;
                /* Search free space for the next allocation */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else {
            /* No free space, then allocate new page for slab objects */
            s = (1ULL << (o + KMEM_SLAB_BASE_ORDER + KMEM_SLAB_NR_OBJ_ORDER))
                + sizeof(struct kmem_slab);
            /* Align the page to fit to the buddy system, and get the order */
            nr = binorder(CEIL(s, SUPERPAGESIZE) / SUPERPAGESIZE);
            /* Allocate pages */
            hdr = kmem_alloc_pages(nr);
            if ( NULL == hdr ) {
                /* Unlock before return */
                spin_unlock(&g_kmem->slab_lock);
                return NULL;
            }
            /* Calculate the number of slab objects in this block; N.B., + 1 in
               the denominator is the `marks' for each objects. */
            hdr->nr = ((1 << nr) * SUPERPAGESIZE - sizeof(struct kmem_slab))
                / ((1 << (o + KMEM_SLAB_BASE_ORDER)) + 1);
            /* Reset counters */
            hdr->nused = 0;
            hdr->free = 0;
            /* Set the address of the first slab object */
            hdr->obj_head = (void *)((u64)hdr + ((1 << nr) * SUPERPAGESIZE)
                                     - ((1 << (o + KMEM_SLAB_BASE_ORDER))
                                        * hdr->nr));
            /* Reset marks and next cache */
            kmemset(hdr->marks, 0, hdr->nr);
            hdr->next = NULL;

            /* Retrieve a slab */
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + KMEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;

            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                g_kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = g_kmem->slab.gslabs[o].full;
                g_kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = g_kmem->slab.gslabs[o].partial;
                g_kmem->slab.gslabs[o].partial = hdr;
                /* Search free space for the next allocation */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        }
    } else {
        /* Large object: Page allocator */
        ptr = kmem_alloc_pages(binorder(CEIL(size, SUPERPAGESIZE)
                                        / SUPERPAGESIZE));
    }

    /* Unlock */
    spin_unlock(&g_kmem->slab_lock);

    return ptr;
}

/*
 * Deallocate memory space pointed by ptr
 *
 * SYNOPSIS
 *      void
 *      kfree(void *ptr);
 *
 * DESCRIPTION
 *      The kfree() function deallocates the memory allocation pointed by ptr.
 *
 * RETURN VALUES
 *      The kfree() function does not return a value.
 */
void
kfree(void *ptr)
{
    int i;
    int j;
    int found;
    u64 asz;
    struct kmem_slab *hdr;
    struct kmem_slab **hdrp;

    /* Lock */
    spin_lock(&g_kmem->slab_lock);

    if ( 0 == (u64)ptr % SUPERPAGESIZE ) {
        /* Free pages */
        kmem_free_pages(ptr);
    } else {
        /* Search for each order */
        for ( i = 0; i < KMEM_SLAB_BASE_ORDER; i++ ) {
            asz = (1 << (i + KMEM_SLAB_BASE_ORDER));

            /* Search from partial */
            hdrp = &g_kmem->slab.gslabs[i].partial;

            /* Continue until the corresponding object found */
            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].free;
                        g_kmem->slab.gslabs[i].free = hdr;
                    }
                    spin_unlock(&g_kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }

            /* Search from full */
            hdrp = &g_kmem->slab.gslabs[i].full;

            /* Continue until the corresponding object found */
            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].free;
                        g_kmem->slab.gslabs[i].free = hdr;
                    } else {
                        /* To partial list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].partial;
                        g_kmem->slab.gslabs[i].partial = hdr;
                    }
                    spin_unlock(&g_kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }
        }
    }

    /* Unlock */
    spin_unlock(&g_kmem->slab_lock);
}

/*
 * Virtual memory region
 */
struct vmem_region *
vmem_region_create(void)
{
#if 0
    struct vmem_region *region;
    struct vmem_page *pages;
    struct vmem_page *tmp;
    size_t i;
    off_t idx;

    /* Allocate a new virtual memory region */
    region = kmalloc(sizeof(struct vmem_region));
    if ( NULL == region ) {
        /* Cannot allocate a virtual memory region */
        return NULL;
    }
    kmemset(region, 0, sizeof(struct vmem_region));
    region->start = (void *)(1ULL << 30);
    region->len = SUPERPAGESIZE * 512; /* 1 GiB */
    region->next = NULL;

    /* Allocate virtual pages for this region */
    pages = kmalloc(sizeof(struct vmem_page) * region->len / SUPERPAGESIZE);
    if ( NULL == pages ) {
        /* Cannot allocate the virtual pages */
        kfree(region);
        return NULL;
    }

    /* Initialize the all pages */
    for ( i = 0; i < region->len / SUPERPAGESIZE; i++ ) {
        pages[i].addr = 0;
        //pages[i].type = 0;
        pages[i].next = NULL;
        pages[i].region = region;
    }
    region->pages = pages;

    /* Prepare the buddy system */
    if ( VMEM_MAX_BUDDY_ORDER < 9 ) {
        region->heads[VMEM_MAX_BUDDY_ORDER] = &pages[0];
        tmp = &pages[0];
        idx = 1ULL << (VMEM_MAX_BUDDY_ORDER);
        for ( i = 0; i < (1 << (9 - VMEM_MAX_BUDDY_ORDER)); i++ ) {
            tmp->next = &pages[idx];
            tmp = &pages[idx];
            /* For the next index */
            idx += 1ULL << VMEM_MAX_BUDDY_ORDER;
        }
        tmp->next = NULL;
    } else {
        region->heads[9] = &pages[0];
        pages[0].next = NULL;
    }

    return region;
#endif
    return NULL;
}

/*
 * Virtual memory preparation
 */
struct vmem_space *
vmem_space_create(void)
{
    struct vmem_space *space;
    struct vmem_region *region;

    /* Allocate a new virtual memory space */
    space = kmalloc(sizeof(struct vmem_space));
    if ( NULL == space ) {
        return NULL;
    }
    kmemset(space, 0, sizeof(struct vmem_space));

    /* Allocate a new virtual memory region */
    region = vmem_region_create();
    if ( NULL == region ) {
        /* Cannot allocate a virtual memory region */
        kfree(space);
        return NULL;
    }

    /* Set */
    space->first_region = region;

    /* Initialize the architecture-specific data structure */
    if ( vmem_arch_init(space) < 0 ) {
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
    struct vmem_space *nvmem;

    /* Allocate a new virtual memory space */
    nvmem = kmalloc(sizeof(struct vmem_space));
    if ( NULL == nvmem ) {
        return NULL;
    }

    /* Region */
    nvmem->first_region = kmalloc(sizeof(struct vmem_region));
    if ( NULL == nvmem->first_region ) {
        kfree(nvmem);
        return NULL;
    }

    /* Copy all the pages */

    /* FIXME */

    return nvmem;
}

/*
 * Search the corresponding region from the virtual address
 */
static struct vmem_region *
_vmem_search_region(struct vmem_space *vmem, void *vaddr)
{
    struct vmem_region *region;

    /* Search from the first region */
    region = vmem->first_region;
    while ( NULL != region ) {
        if ( (reg_t)vaddr >= (reg_t)region->start
             && (reg_t)vaddr < (reg_t)region->start + region->len ) {
            /* Found */
            return region;
            break;
        }
        region = region->next;
    }

    return NULL;
}

#if 0
/*
 * Search the corresponding page from the virtual address
 */
static struct vmem_page *
_vmem_search_page(struct vmem_space *vmem, void *vaddr)
{
    struct vmem_region *region;
    struct vmem_page *page;

    /* Search the corresponding region */
    region = _vmem_search_region(vmem, vaddr);
    if ( NULL == region ) {
        /* No region found */
        return NULL;
    }

    /* Get page information */
    page = &region->pages[((reg_t)vaddr - (reg_t)region->start)
                          / SUPERPAGESIZE];

    return page;
}

static void *
_kmem_get_free_pages(struct kmem *kmem, size_t len)
{
    void *pg;
    struct vmem_region *reg;
    size_t i;
    size_t j;

    /* Reset the return value with NULL */
    pg = NULL;

    /* Search the regions where is capable to allocate vacant pages */
    reg = kmem->space->first_region;
    while ( NULL != reg ) {
        /* Search vacant pages from this region */
        for ( i = 0; i < reg->total_pgs; i++ ) {
            for ( j = 0; j < PAGE_INDEX(len); j++ ) {
                if ( !VMEM_IS_FREE(&reg->pages[i + j]) ) {
                    /* Not free */
                    break;
                }
            }
            if ( j == PAGE_INDEX(len) ) {
                /* Found */
                pg = (void *)PAGE_ADDR(i);
                return pg;
            }
            i += j + 1;
        }

        /* Next region */
        reg = reg->next;
    }

    return NULL;
}
#endif






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
vmem_alloc_pages(struct vmem_space *space, int order)
{
    struct kmem *kmem;
    void *paddr;
    void *vaddr;

    /* Allocate physical pages first */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
    if ( NULL == paddr ) {
        return NULL;
    }

    /* Check the order */
    if ( order >= SP_SHIFT ) {
        /* Allocate virtual space */
        vaddr = vmem_buddy_alloc_superpages(space, order - SP_SHIFT);
        if ( NULL == vaddr ) {
            pmem_free_pages(paddr);
            return NULL;
        }
    } else {
        //vmem_buddy_alloc_pages
        vaddr = NULL;
    }

    return vaddr;


    return NULL;
}

/*
 * Deallocate virtual memory space pointed by a
 */
void
vmem_free_pages(struct vmem_space *space, void *a)
{
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
        /* Check the type of the region */
        if ( VMEM_NORM_REGION != reg->type ) {
            /* Next region */
            reg = reg->next;
            continue;
        }

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
            for ( i = 0; i < (1LL << order) - 1; i++ ) {
                if ( !VMEM_IS_FREE(&vpage[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous pages as "used" */
            for ( i = 0; i < (1LL << order) - 1; i++ ) {
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
        /* Check the type of the region */
        if ( VMEM_NORM_REGION != reg->type ) {
            /* Next region */
            reg = reg->next;
            continue;
        }

        /* Split first if needed */
        ret = _vmem_buddy_pg_split(reg, order);
        if ( ret < 0 ) {
            /* Take from a superpage */
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

            /* FIXME: Make this superpage to pages */
        }

        if ( ret >= 0 ) {
            /* Get one from the buddy system, and remove that from the list */
            vpage = reg->pgheads[order];
            reg->pgheads[order] = vpage->next;
            if ( NULL != reg->pgheads[order] ) {
                reg->spgheads[order]->prev = NULL;
            }

            /* Validate all the pages are not used */
            for ( i = 0; i < (1LL << order) - 1; i++ ) {
                if ( !VMEM_IS_FREE(&vpage[i]) ) {
                    return NULL;
                }
            }

            /* Mark the contiguous pages as "used" */
            for ( i = 0; i < (1LL << order) - 1; i++ ) {
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


/*
 * Superpage to pages
 */
static int
_vmem_superpage_to_pages(struct vmem_region *reg, struct vmem_superpage *spg)
{
    size_t sz;
    void *x;
    int order;
    struct vmem_page *pgs;

    /* Calculate the size of the array of the page data structure */
    sz = sizeof(struct vmem_page) * SUPERPAGESIZE / PAGESIZE;
    order = binorder(DIV_CEIL(sz, PAGESIZE));

    /* Allocate physical memory */
    x = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);

    return -1;
}










#if 0
/*
 * Search available virtual pages
 */
static void *
_vmem_buddy_alloc_region(struct vmem_region *reg, int n)
{
    struct vmem_page *page;
    ssize_t i;
    int ret;

    if ( n < 0 || n > VMEM_MAX_BUDDY_ORDER ) {
        /* Invalid order */
        return NULL;
    }

    /* Split first if needed */
    ret = _vmem_buddy_spg_split(reg, n);
    if ( ret < 0 ) {
        /* No memory available */
        return NULL;
    }

    /* Return this block */
    page = reg->heads[n];

    /* Remove the found pages from the list */
    reg->heads[n] = page->next;
    reg->heads[n]->prev = NULL;

    /* Manage the contiguous pages in a list */
    for ( i = 0; i < (1LL << n) - 1; i++ ) {
        reg->pages[page - reg->pages + i].next
            = &reg->pages[page - reg->pages + i + 1];
    }
    reg->pages[page - reg->pages + i].next = NULL;

    return reg->start + PAGE_ADDR(page - reg->pages + i);
}

/*
 * Create a region for virtual pages
 */
static void *
_vmem_new_region(struct vmem_space *vmem, size_t n)
{
    struct vmem_region *reg;
    size_t sz;
    size_t npg;
    void *p;
    void *v;

    /* Calculate the size of the region */
    sz = sizeof(struct vmem_region) + sizeof(struct vmem_page) * n;

    /* Find the physical pages */
    npg = DIV_CEIL(sz, PAGESIZE);
    p = pmem_alloc_pages(PMEM_ZONE_LOWMEM, binorder(npg));
    if ( NULL == p ) {
        return NULL;
    }

    /* Allocate a virtual memory region */
    v = p;

    reg = (struct vmem_region *)(v + sizeof(struct vmem_page) * n);
    reg->pages = (struct vmem_page *)v;
    reg->start = 0;
    reg->len = npg * PAGESIZE;

    return NULL;
}
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
