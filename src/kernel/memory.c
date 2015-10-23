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

struct pmem *pmem;
struct kmem *kmem;

#define PMEM_SUPERPAGE_ADDRESS(p)                               \
    (void *)(SUPERPAGESIZE * (size_t)((p) - pmem->superpages));

#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)
u64 binorder(u64);
int kmem_remap(u64, u64, int);
u64 kmem_paddr(u64);
int vmem_arch_init(struct vmem_space *);
int vmem_remap(struct vmem_space *, u64, u64, int);
u64 vmem_paddr(struct vmem_space *, u64);


/*
 * Build buddy system
 */
int
pmem_init(struct pmem *pm)
{
    return 0;
}


/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_kpage_split(int o)
{
    int ret;
    struct kmem_page *next;

    /* Check the head of the current order */
    if ( NULL != kmem->heads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= KMEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == kmem->heads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _kpage_split(o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Save next at the upper order */
    next = kmem->heads[o + 1]->next;
    /* Split into two */
    kmem->heads[o] = kmem->heads[o + 1];
    kmem->heads[o]->next = kmem->heads[o] + (1 << o);
    kmem->heads[o]->next->next = NULL;
    /* Remove the split one from the upper order */
    kmem->heads[o + 1] = next;

    return 0;
}

/*
 * Merge buddies onto the upper order on if possible
 */
static void
_kpage_merge(struct kmem_page *off, int o)
{
    int found;
    struct kmem_page *p0;
    struct kmem_page *p0p;
    struct kmem_page *p1p;
    struct kmem_page *p1;
    struct kmem_page *prev;
    struct kmem_page *list;
    struct kmem_page *region;

    if ( o + 1 >= KMEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order */
        return;
    }

    /* Check the region */
    if ( off - kmem->region1 >= 0
         && off - kmem->region1 < KMEM_REGION_SIZE ) {
        /* Region 1 */
        region = kmem->region1;
    } else if ( off - kmem->region2 >= 0
                && off - kmem->region2 < KMEM_REGION_SIZE ) {
        /* Region 2 */
        region = kmem->region2;
    } else {
        /* Fatal error: Not to be reached here */
        return;
    }

    /* Get the first page of the upper order */
    p0 = region + ((off - region) / (1 << (o + 1)) * (1 << (o + 1)));
    /* Get the neighboring buddy */
    p1 = p0 + (1 << o);

    /* Check the current level and remove the pairs */
    list = kmem->heads[o];
    found = 0;
    prev = NULL;
    while ( NULL != list ) {
        if ( p0 == list || p1 == list ) {
            /* Preserve the pointer to the previous entry */
            if ( p0 == list ) {
                p0p = prev;
            } else {
                p1p = prev;
            }
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
    if ( p0p == NULL ) {
        /* Head */
        kmem->heads[o] = p0->next;
    } else {
        /* Otherwise */
        list = p0p;
        list->next = p0->next;
    }
    if ( p1p == NULL ) {
        /* Head */
        kmem->heads[o] = p1->next;
    } else {
        /* Otherwise */
        list = p1p;
        list->next = p1->next;
    }

    /* Prepend it to the upper order */
    p0->next = kmem->heads[o + 1];
    kmem->heads[o + 1] = p0;

    /* Try to merge the upper order of buddies */
    _kpage_merge(p0, o + 1);
}

/*
 * Search available kernel pages
 */
static struct kmem_page *
_kpage_alloc(int n)
{
    struct kmem_page *page;
    struct kmem_page *region;
    ssize_t i;
    int ret;

    if ( n < 0 || n > KMEM_MAX_BUDDY_ORDER ) {
        /* Invalid order */
        return NULL;
    }

    /* Lock */
    spin_lock(&kmem->lock);

    /* Split first if needed */
    ret = _kpage_split(n);
    if ( ret < 0 ) {
        /* No memory available */
        spin_unlock(&kmem->lock);
        return NULL;
    }

    /* Return this block */
    page = kmem->heads[n];

    /* Get the region */
    if ( page - kmem->region1 >= 0
         && page - kmem->region1 < KMEM_REGION_SIZE ) {
        /* Region 1 */
        region = kmem->region1;
    } else if ( page - kmem->region2 >= 0
                && page - kmem->region2 < KMEM_REGION_SIZE ) {
        /* Region 2 */
        region = kmem->region2;
    } else {
        /* Fatal error: Not to be reached here */
        spin_unlock(&kmem->lock);
        return NULL;
    }
    /* Remove the found pages from the list */
    kmem->heads[n] = page->next;

    /* Manage the contiguous pages in a list */
    for ( i = 0; i < (1LL << n) - 1; i++ ) {
        region[page - region + i].next = &region[page - region + i + 1];
    }
    region[page - region + i].next = NULL;

    /* Unlock */
    spin_unlock(&kmem->lock);

    return page;
}

static void
_kpage_free(struct kmem_page *page)
{
    struct kmem_page *list;
    int order;
    ssize_t n;
    int i;

    /* Count the number of pages */
    list = page;
    n = 0;
    while ( NULL != list ) {
        n++;
        list = list->next;
    }

    /* Resolve the order from the number of the pages */
    order = -1;
    for ( i = 0; i <= KMEM_MAX_BUDDY_ORDER; i++ ) {
        if ( n == (1LL << i) ) {
            order = i;
            break;
        }
    }

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order > KMEM_MAX_BUDDY_ORDER || order < 0 ) {
        /* Something is wrong... */
        return;
    }

    /* Lock */
    spin_lock(&kmem->lock);

    /* Return it to the buddy system */
    list = kmem->heads[order];
    /* Prepend the returned pages */
    kmem->heads[order] = page;
    kmem->heads[order]->next = list;

    /* Merge buddies if possible */
    _kpage_merge(page, order);

    /* Unlock */
    spin_unlock(&kmem->lock);
}





/*
 * Initialize the kernel memory
 */
int
kmem_init(void)
{
    ssize_t i;
    int ret;

    /* Initialize kmem */
    if ( sizeof(struct kmem) > SUPERPAGESIZE ) {
        return -1;
    }
    kmem = (struct kmem *)0x00100000ULL;
    kmemset(kmem, 0, sizeof(struct kmem));

#if 0
    /* First region */
    for ( i = 0; i < KMEM_REGION_SIZE; i++ ) {
        if ( i < pmem->nr
             && (pmem->superpages[i].flags & (PMEM_WIRED | PMEM_UNAVAIL)) ) {
            kmem->region1[i].address = (size_t)i * SUPERPAGESIZE;
            kmem->region1[i].type = 1;
        } else {
            kmem->region1[i].address = 0;
            kmem->region1[i].type = 0;
        }
    }

    /* Second region */
    for ( i = 0; i < KMEM_REGION_SIZE; i++ ) {
        if ( i + 1536 < pmem->nr
             && (pmem->superpages[1536 + i].flags
                 & (PMEM_WIRED | PMEM_UNAVAIL)) ) {
            kmem->region2[i].address = (size_t)(i + 1536) * SUPERPAGESIZE;
            kmem->region2[i].type = 1;
        } else {
            kmem->region2[i].address = 0;
            kmem->region2[i].type = 0;
        }
    }
#endif
    /* Build the buddy system */
    for ( i = KMEM_REGION_SIZE - 1; i >= 0; i-- ) {
        if ( 0 == kmem->region2[i].type ) {
            _kpage_free(&kmem->region2[i]);
        }
    }
    for ( i = KMEM_REGION_SIZE - 1; i >= 0; i-- ) {
        if ( 0 == kmem->region1[i].type ) {
            _kpage_free(&kmem->region1[i]);
        }
    }

    /* Mapping (modify the kernel page table) */
    for ( i = 0; i < KMEM_REGION_SIZE; i++ ) {
        if ( kmem->region1[i].type ) {
            ret = kmem_remap((u64)i * SUPERPAGESIZE, kmem->region1[i].address,
                             1);
            if ( ret < 0 ) {
                return -1;
            }
        } else {
            ret = kmem_remap((u64)i * SUPERPAGESIZE, 0, 0);
            if ( ret < 0 ) {
                return -1;
            }
        }
        if ( kmem->region2[i].type ) {
            ret = kmem_remap((u64)(i + 1536) * SUPERPAGESIZE,
                             kmem->region2[i].address, 1);
            if ( ret < 0 ) {
                return -1;
            }
        } else {
            ret = kmem_remap((u64)(i + 1536) * SUPERPAGESIZE, 0, 0);
            if ( ret < 0 ) {
                return -1;
            }
        }
    }

    /* Slab */
    for ( i = 0; i < KMEM_SLAB_ORDER; i++ ) {
        kmem->slab.gslabs[i].partial = NULL;
        kmem->slab.gslabs[i].full = NULL;
        kmem->slab.gslabs[i].free = NULL;
    }

    return 0;
}

/*
 * Allocate kernel pages
 */
void *
kmem_alloc_pages(int order)
{
    struct kmem_page *kpage;
    void *ppage;
    void *vaddr;
    ssize_t i;
    struct kmem_page *region;
    off_t off;
    int ret;

    /* Allocate a physical page */
    ppage = pmem->proto.alloc_pages(0, order);
    if ( NULL == ppage ) {
        return NULL;
    }

    /* Kernel page */
    kpage = _kpage_alloc(order);
    if ( NULL == kpage ) {
        pmem->proto.free_pages(ppage, 0, order);
        return NULL;
    }

    if ( kpage - kmem->region1 >= 0
         && kpage - kmem->region1 < KMEM_REGION_SIZE ) {
        /* Region 1 */
        region = kmem->region1;
        off = kpage - kmem->region1;
        vaddr = (void *)(off * SUPERPAGESIZE);
    } else if ( kpage - kmem->region2 >= 0
                && kpage - kmem->region2 < KMEM_REGION_SIZE ) {
        /* Region 2 */
        region = kmem->region2;
        off = kpage - kmem->region2;
        vaddr = (void *)((off + 1536) * SUPERPAGESIZE);
    } else {
        /* Error */
        pmem->proto.free_pages(ppage, 0, order);
        _kpage_free(kpage);
        return NULL;
    }
    for ( i = 0; i < (1LL << order); i++ ) {
        region[off + i].address = (size_t)ppage + SUPERPAGE_ADDR(i);
        region[off + i].type = 1;
        ret = kmem_remap((u64)vaddr + SUPERPAGE_ADDR(i),
                         (u64)ppage + SUPERPAGE_ADDR(i), 1);
        if ( ret < 0 ) {
            /* Rollback */
            for ( ; i >= 0; i-- ) {
                kmem_remap((u64)vaddr + (u64)i * SUPERPAGESIZE, 0, 0);
            }
            pmem->proto.free_pages(ppage, 0, order);
            _kpage_free(kpage);
            return NULL;
        }
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

    _kpage_free(kpage);
    paddr = kmem_paddr((u64)vaddr);
    //pmem_free_pages(&pmem->superpages[(u64)paddr / SUPERPAGESIZE]);
}


/*
 * Allocate memory space
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
    spin_lock(&kmem->slab_lock);

    if ( o < KMEM_SLAB_ORDER ) {
        /* Small object: Slab allocator */
        if ( NULL != kmem->slab.gslabs[o].partial ) {
            /* Partial list is available. */
            hdr = kmem->slab.gslabs[o].partial;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + KMEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem->slab.gslabs[o].full;
                kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Search free space for the next allocation */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else if ( NULL != kmem->slab.gslabs[o].free ) {
            /* Partial list is empty, but free list is available. */
            hdr = kmem->slab.gslabs[o].free;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + KMEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem->slab.gslabs[o].full;
                kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = kmem->slab.gslabs[o].partial;
                kmem->slab.gslabs[o].partial = hdr;
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
                spin_unlock(&kmem->slab_lock);
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
                kmem->slab.gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = kmem->slab.gslabs[o].full;
                kmem->slab.gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = kmem->slab.gslabs[o].partial;
                kmem->slab.gslabs[o].partial = hdr;
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
    spin_unlock(&kmem->slab_lock);

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
    spin_lock(&kmem->slab_lock);

    if ( 0 == (u64)ptr % SUPERPAGESIZE ) {
        /* Free pages */
        kmem_free_pages(ptr);
    } else {
        /* Search for each order */
        for ( i = 0; i < KMEM_SLAB_BASE_ORDER; i++ ) {
            asz = (1 << (i + KMEM_SLAB_BASE_ORDER));

            /* Search from partial */
            hdrp = &kmem->slab.gslabs[i].partial;

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
                        hdr->next = kmem->slab.gslabs[i].free;
                        kmem->slab.gslabs[i].free = hdr;
                    }
                    spin_unlock(&kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }

            /* Search from full */
            hdrp = &kmem->slab.gslabs[i].full;

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
                        hdr->next = kmem->slab.gslabs[i].free;
                        kmem->slab.gslabs[i].free = hdr;
                    } else {
                        /* To partial list */
                        *hdrp = hdr->next;
                        hdr->next = kmem->slab.gslabs[i].partial;
                        kmem->slab.gslabs[i].partial = hdr;
                    }
                    spin_unlock(&kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }
        }
    }

    /* Unlock */
    spin_unlock(&kmem->slab_lock);
}

/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_vmem_page_split(struct vmem_region *region, int o)
{
    int ret;
    struct vmem_page *next;

    /* Check the head of the current order */
    if ( NULL != region->heads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= VMEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == region->heads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _vmem_page_split(region, o + 1);
        if ( ret < 0 ) {
            /* Cannot get any */
            return ret;
        }
    }

    /* Save next at the upper order */
    next = region->heads[o + 1]->next;
    /* Split into two */
    region->heads[o] = region->heads[o + 1];
    region->heads[o]->next = region->heads[o] + (1 << o);
    region->heads[o]->next->next = NULL;
    /* Remove the split one from the upper order */
    region->heads[o + 1] = next;

    return 0;
}

/*
 * Merge buddies onto the upper order on if possible
 */
static void
_vmem_page_merge(struct vmem_region *region, struct vmem_page *off, int o)
{
    int found;
    struct vmem_page *p0;
    struct vmem_page *p0p;
    struct vmem_page *p1p;
    struct vmem_page *p1;
    struct vmem_page *prev;
    struct vmem_page *list;

    if ( o + 1 >= VMEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order */
        return;
    }

    /* Check the region */

    /* Get the first page of the upper order */
    p0 = region->pages + ((off - region->pages)
                          / (1 << (o + 1)) * (1 << (o + 1)));
    /* Get the neighboring buddy */
    p1 = p0 + (1 << o);

    /* Check the current level and remove the pairs */
    list = region->heads[o];
    found = 0;
    prev = NULL;
    while ( NULL != list ) {
        if ( p0 == list || p1 == list ) {
            /* Preserve the pointer to the previous entry */
            if ( p0 == list ) {
                p0p = prev;
            } else {
                p1p = prev;
            }
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
    if ( p0p == NULL ) {
        /* Head */
        region->heads[o] = p0->next;
    } else {
        /* Otherwise */
        list = p0p;
        list->next = p0->next;
    }
    if ( p1p == NULL ) {
        /* Head */
        region->heads[o] = p1->next;
    } else {
        /* Otherwise */
        list = p1p;
        list->next = p1->next;
    }

    /* Prepend it to the upper order */
    p0->next = region->heads[o + 1];
    region->heads[o + 1] = p0;

    /* Try to merge the upper order of buddies */
    _vmem_page_merge(region, p0, o + 1);
}

/*
 * Search available virtual pages
 */
static struct vmem_page *
_vmem_page_alloc(struct vmem_space *vm, int n)
{
    struct vmem_page *page;
    struct vmem_region *region;
    ssize_t i;
    int ret;

    if ( n < 0 || n > VMEM_MAX_BUDDY_ORDER ) {
        /* Invalid order */
        return NULL;
    }

    /* Check the first region */
    region = vm->first_region;

    /* Split first if needed */
    ret = _vmem_page_split(region, n);
    if ( ret < 0 ) {
        /* No memory available */
        return NULL;
    }

    /* Return this block */
    page = region->heads[n];

    /* Remove the found pages from the list */
    region->heads[n] = page->next;

    /* Manage the contiguous pages in a list */
    for ( i = 0; i < (1LL << n) - 1; i++ ) {
        region->pages[page - region->pages + i].next
            = &region->pages[page - region->pages + i + 1];
    }
    region->pages[page - region->pages + i].next = NULL;

    return page;
}

/*
 * Release a virtual page
 */
static void
_vmem_page_free(struct vmem_space *vm, void *addr)
{
    struct vmem_region *region;
    struct vmem_page *page;
    struct vmem_page *list;
    int order;
    ssize_t n;
    int i;

    /* Find the region and page */
    region = vm->first_region;
    while ( NULL != region ) {
        if ( (reg_t)addr >= (reg_t)region->start
             && (reg_t)addr < (reg_t)region->start + region->len ) {
            /* Found */
            page = &region->pages[((reg_t)addr - (reg_t)region->start)
                                  / SUPERPAGESIZE];
            break;
        }
        region = region->next;
    }
    if ( NULL == region ) {
        /* Region not found */
        return;
    }

    /* Count the number of pages */
    list = page;
    n = 0;
    while ( NULL != list ) {
        n++;
        list = list->next;
    }

    /* Resolve the order from the number of the pages */
    order = -1;
    for ( i = 0; i <= VMEM_MAX_BUDDY_ORDER; i++ ) {
        if ( n == (1LL << i) ) {
            order = i;
            break;
        }
    }

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order > VMEM_MAX_BUDDY_ORDER || order < 0 ) {
        /* Something is wrong... */
        return;
    }

    /* Return it to the buddy system */
    list = region->heads[order];
    /* Prepend the returned pages */
    region->heads[order] = page;
    region->heads[order]->next = list;

    /* Merge buddies if possible */
    _vmem_page_merge(region, page, order);
}

/*
 * Virtual memory region
 */
struct vmem_region *
vmem_region_create(void)
{
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
        pages[i].type = 0;
        pages[i].next = NULL;
        pages[i].region = region;
    }
    region->pages = pages;

    /* Prepare the buddy system */
    if ( VMEM_MAX_BUDDY_ORDER < 9 ) {
        region->heads[VMEM_MAX_BUDDY_ORDER] = &pages[0];
        tmp = &pages[0];
        idx = 1ULL << (VMEM_MAX_BUDDY_ORDER);
        for ( i = 0; i < 1 << (9 - VMEM_MAX_BUDDY_ORDER); i++ ) {
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
 * Allocate virtual pages
 */
struct vmem_page *
vmem_alloc_pages(struct vmem_space *vmem, int order)
{
    struct vmem_page *vpage;
    struct vmem_page *tmp;
    void *paddr;
    void *vaddr;
    ssize_t i;
    int ret;

    /* Allocate 2^order contiguous virtual pages */
    vpage = _vmem_page_alloc(vmem, order);
    if ( NULL == vpage ) {
        return NULL;
    }

    /* Allocate physical page */
    paddr = pmem->proto.alloc_pages(0, order);
    if ( NULL == paddr ) {
        _vmem_page_free(vmem, vpage);
        return NULL;
    }

    /* Resolve the virtual address */
    vaddr = vpage->region->start
        + (vpage - vpage->region->pages) * SUPERPAGESIZE;

    /* Update the page table */
    tmp = vpage;
    for ( i = 0; i < (1LL << order); i++ ) {
        tmp->addr = (size_t)paddr + (size_t)i * SUPERPAGESIZE;
        tmp->type = VMEM_USED;
        ret = vmem_remap(vmem, (u64)vaddr + (u64)i * SUPERPAGESIZE,
                         (u64)paddr + (u64)i * SUPERPAGESIZE, 1);
        if ( ret < 0 ) {
            /* Rollback */
            for ( ; i >= 0; i-- ) {
                vmem_remap(vmem, (u64)vaddr + (u64)i * SUPERPAGESIZE, 0, 0);
            }
            pmem->proto.free_pages(paddr, 0, order);
            _vmem_page_free(vmem, vpage);
            return NULL;
        }
        tmp = vpage->next;
    }


    return vpage;
}

/*
 * Release virtual pages
 */
void
vmem_free_pages(void *addr)
{
    /* FIXME: Implement this */
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

/*
 * Search the corresponding page from the virtual address
 */
static struct vmem_page *
_vmem_search_page(struct vmem_space *vmem, void *vaddr)
{
    struct vmem_region *region;
    struct vmem_region *page;

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

/*
 * Remap virtual memory space in the page table
 */
int
vmem_remap_(struct vmem_space *vmem, void *vaddr, reg_t paddr, int flag)
{
    /* Research the corresponding region */
    struct vmem_page *page;

    /* Search the corresponding page */
    page = _vmem_search_page(vmem, vaddr);

    if ( VMEM_USED == page->type ) {
        /* The specified page is already mapped */
        return -ENOMEM;
    }

    /* Align and map the page to a physical address */
    page->addr = (paddr / SUPERPAGESIZE) * SUPERPAGESIZE;
    page->type = VMEM_USED;

    /* FIXME: Virtual address of the page table entries must be set up and
       stored here.  Then, translate to the physical addresss. */

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
