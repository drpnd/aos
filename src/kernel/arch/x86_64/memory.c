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
#define PHYS_MEM_USED           1ULL            /* Managed by buddy system */
#define PHYS_MEM_WIRED          (1ULL<<1)       /* Wired (kernel use) */
#define PHYS_MEM_ALLOC          (1ULL<<2)       /* Allocated */
#define PHYS_MEM_SLAB           (1ULL<<3)       /* For slab */
#define PHYS_MEM_UNAVAIL        (1ULL<<16)      /* Unavailable space */

#define PHYS_MEM_IS_FREE(x)     (0 == (x)->flags ? 1 : 0)

#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)

static u32 phys_mem_lock;
static u32 phys_mem_slab_lock;
static struct phys_mem *phys_mem;
struct phys_mem_slab_root *phys_mem_slab_head;

/*
 * Split the buddies so that we get at least one buddy at the order o
 */
static int
_split(struct phys_mem_buddy *buddy, int o)
{
    int ret;
    struct phys_mem_buddy_list *next;

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

    /* Save next at the upper order */
    next = buddy->heads[o + 1]->next;
    /* Split into two */
    buddy->heads[o] = buddy->heads[o + 1];
    buddy->heads[o]->prev = NULL;
    buddy->heads[o]->next
        = (struct phys_mem_buddy_list *)((u64)buddy->heads[o]
                                         + (1 << o) * PAGESIZE);
    buddy->heads[o]->next->prev = buddy->heads[o];
    buddy->heads[o]->next->next = NULL;
    /* Remove the split one from the upper order */
    buddy->heads[o + 1] = next;
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
    p0 = (struct phys_mem_buddy_list *)((u64)off / (1 << (o + 1)) / PAGESIZE
                                        * (1 << (o + 1)) * PAGESIZE);
    /* Get the neighboring buddy */
    p1 = (struct phys_mem_buddy_list *)((u64)p0 + (1 << o) * PAGESIZE);

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
 * Check the zone of the 2^k pages starting at i-th page
 */
static int
_check_zone(u64 i, int k)
{
    int zone;

    /* Check the zone */
    if ( i * PAGESIZE < 0x1000000ULL ) {
        if ( (i + (1 << k)) * PAGESIZE > 0x1000000ULL ) {
            /* Span different zones */
            return -1;
        }
        zone = PHYS_MEM_ZONE_DMA;
    } else if ( i * PAGESIZE < 0x40000000ULL ) {
        if ( (i + (1 << k)) * PAGESIZE > 0x40000000ULL ) {
            /* Span different zones */
            return -1;
        }
        /* Below 1 GiB */
        zone = PHYS_MEM_ZONE_NORMAL;
    } else {
        zone = PHYS_MEM_ZONE_HIGHMEM;
    }

    return zone;
}


/*
 * Initialize physical memory
 *
 * SYNOPSIS
 *      int
 *      phys_mem_init(struct bootinfo *bi);
 *
 * DESCRIPTION
 *      The phys_mem_init() function initializes the page allocator with the
 *      memory map information inherited from the boot monitor.
 *
 * RETURN VALUES
 *      If successful, the phys_mem_init() function returns 0.  It returns -1 on
 *      failure.
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
    int k;
    int flag;
    int zone;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return -1;
    }

    /* Initialize the lock variable */
    phys_mem_lock = 0;
    phys_mem_slab_lock = 0;

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
                    /* Found */
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
        phys_mem->pages[i].order = -1;
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
                /* FIXME */
                if ( j * PAGESIZE >= 1024ULL * 1024 * 1024 ) {
                    continue;
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
    for ( i = addr / PAGESIZE; i <= CEIL(addr + sz, PAGESIZE) / PAGESIZE;
          i++ ) {
        phys_mem->pages[i].flags |= PHYS_MEM_WIRED;
    }

    /* Initialize buddy system */
    for ( i = 0; i < PHYS_MEM_NR_ZONES; i++ ) {
        for ( j = 0; j < PHYS_MEM_MAX_BUDDY_ORDER; j++ ) {
            phys_mem->zones[i].buddy.heads[j] = NULL;
        }
    }

    /* Add all pages to the buddy system */
    for ( k = PHYS_MEM_MAX_BUDDY_ORDER - 1; k >= 0; k-- ) {
        for ( i = 0; i < phys_mem->nr; i += (1 << k) ) {
            /* Check whether all pages are free */
            flag = 0;
            for ( j = 0; j < (1 << k); j++ ) {
                if ( i + j >= phys_mem->nr ) {
                    /* Exceeds the upper limit */
                    flag = 1;
                    break;
                }
                if ( !PHYS_MEM_IS_FREE(&phys_mem->pages[i + j]) ) {
                    /* Used page */
                    flag = 1;
                    break;
                }
            }
            if ( !flag ) {
                /* Check the zone */
                zone = _check_zone(i, k);
                if ( zone < 0 ) {
                    continue;
                }

                /* Append this page to the order k in the buddy system */
                if ( NULL == phys_mem->zones[zone].buddy.heads[k] ) {
                    /* Empty list */
                    phys_mem->zones[zone].buddy.heads[k]
                        = (struct phys_mem_buddy_list *)(i * PAGESIZE);
                    phys_mem->zones[zone].buddy.heads[k]->prev = NULL;
                    phys_mem->zones[zone].buddy.heads[k]->next = NULL;
                } else {
                    /* Search the tail */
                    list = phys_mem->zones[zone].buddy.heads[k];
                    while ( NULL != list->next ) {
                        list = list->next;
                    }
                    list->next = (struct phys_mem_buddy_list *)(i * PAGESIZE);
                    list->next->prev = list;
                    list->next->next = NULL;
                }
                /* Mark these pages as used (by the buddy system) */
                for ( j = 0; j < (1 << k); j++ ) {
                    phys_mem->pages[i + j].flags |= PHYS_MEM_USED;
                }
            }
        }
    }

    /* Initialize slab */
    nr = (sizeof(struct phys_mem_slab_root) - 1) / PAGESIZE + 1;
    phys_mem_slab_head = phys_mem_alloc_pages(PHYS_MEM_ZONE_NORMAL,
                                              binorder(nr));
    if ( NULL == phys_mem_slab_head ) {
        /* Cannot allocate pages for the slab allocator */
        return -1;
    }
    for ( i = 0; i < PHYS_MEM_SLAB_ORDER; i++ ) {
        phys_mem_slab_head->gslabs[i].partial = NULL;
        phys_mem_slab_head->gslabs[i].full = NULL;
        phys_mem_slab_head->gslabs[i].free = NULL;
    }

    return 0;
}

/*
 * Allocate 2^order physical pages
 *
 * SYNOPSIS
 *      void *
 *      phys_mem_alloc_pages(int zone, int order);
 *
 * DESCRIPTION
 *      The phys_mem_alloc_pages() function allocates 2^order pages.
 *
 * RETURN VALUES
 *      The phys_mem_alloc_pages() function returns a pointer to allocated page.
 *      If there is an error, it returns a NULL pointer.
 */
void *
phys_mem_alloc_pages(int zone, int order)
{
    size_t i;
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

    /* Check the zone */
    if ( zone < 0 || zone >= PHYS_MEM_NR_ZONES ) {
        /* Invalid zone */
        return NULL;
    }

    /* Lock */
    spin_lock(&phys_mem_lock);

    /* Split first if needed */
    ret = _split(&phys_mem->zones[zone].buddy, order);
    if ( ret < 0 ) {
        /* No memory available */
        spin_unlock(&phys_mem_lock);
        return NULL;
    }

    /* Obtain from the head */
    a = phys_mem->zones[zone].buddy.heads[order];
    phys_mem->zones[zone].buddy.heads[order] = a->next;
    if ( NULL != phys_mem->zones[zone].buddy.heads[order] ) {
        phys_mem->zones[zone].buddy.heads[order]->prev = NULL;
    }

    /* Mark pages ``allocated'' */
    for ( i = (u64)a / PAGESIZE; i < (u64)a / PAGESIZE + (1 << order); i++ ) {
        if ( phys_mem->pages[i].flags & PHYS_MEM_ALLOC ) {
            panic("Fatal: Invalid operation in phys_mem_alloc_pages().");
        }
        phys_mem->pages[i].flags |= PHYS_MEM_ALLOC;
    }

    /* Save the order */
    phys_mem->pages[(u64)a / PAGESIZE].order = order;

    /* Clear the memory for security */
    kmemset(a, 0, (1 << order) * PAGESIZE);

    /* Unlock */
    spin_unlock(&phys_mem_lock);

    return a;
}

/*
 * Allocate a physical page
 *
 * SYNOPSIS
 *      void *
 *      phys_mem_alloc_page(int zone);
 *
 * DESCRIPTION
 *      The phys_mem_alloc_page() function allocates one page.
 *
 * RETURN VALUES
 *      The phys_mem_alloc_page() function returns a pointer to the allocated
 *      page.  If there is an error, it returns a NULL pointer.
 */
void *
phys_mem_alloc_page(int zone)
{
    return phys_mem_alloc_pages(zone, 0);
}

/*
 * Free allocated 2^order pages
 *
 * SYNOPSIS
 *      void
 *      phys_mem_free_pages(void *a);
 *
 * DESCRIPTION
 *      The phys_mem_free_pages() function deallocates pages pointed by a.
 *
 * RETURN VALUES
 *      The phys_mem_free_pages() function does not return a value.
 */
void
phys_mem_free_pages(void *a)
{
    size_t i;
    u64 p;
    struct phys_mem_buddy_list *list;
    int order;
    int zone;

    /* Get the index of the page */
    p = (u64)a / PAGESIZE;
    if ( p >= phys_mem->nr ) {
        /* Invalid page number */
        return;
    }
    /* Ensure to be aligned */
    a = (void *)(p * PAGESIZE);

    /* Get the order */
    order = phys_mem->pages[p].order;

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order >= PHYS_MEM_MAX_BUDDY_ORDER || order < 0 ) {
        /* Something is wrong... */
        return;
    }

    /* Check the zone */
    zone = _check_zone(p, order);
    if ( zone < 0 ) {
        /* Something went wrong... */
        return;
    }

    /* Lock */
    spin_lock(&phys_mem_lock);

    /* Reset the order */
    phys_mem->pages[p].order = -1;

    /* Unmark pages ``allocated'' */
    for ( i = p; i < p + (1 << order); i++ ) {
        phys_mem->pages[i].flags &= ~PHYS_MEM_ALLOC;
    }

    /* Return it to the buddy system */
    list = phys_mem->zones[zone].buddy.heads[order];
    /* Prepend the returned pages */
    phys_mem->zones[zone].buddy.heads[order] = a;
    phys_mem->zones[zone].buddy.heads[order]->prev = NULL;
    phys_mem->zones[zone].buddy.heads[order]->next = list;
    if ( NULL != list ) {
        list->prev = a;
    }

    /* Merge buddies if possible */
    _merge(&phys_mem->zones[zone].buddy, a, order);

    /* Unlock */
    spin_unlock(&phys_mem_lock);
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
    struct phys_mem_slab *hdr;

    /* Get the binary order */
    o = binorder(size);

    /* Align the order */
    if ( o < PHYS_MEM_SLAB_BASE_ORDER ) {
        o = 0;
    } else {
        o -= PHYS_MEM_SLAB_BASE_ORDER;
    }

    /* Lock */
    spin_lock(&phys_mem_slab_lock);

    if ( o < PHYS_MEM_SLAB_ORDER ) {
        /* Small object: Slab allocator */
        if ( NULL != phys_mem_slab_head->gslabs[o].partial ) {
            /* Partial list is available. */
            hdr = phys_mem_slab_head->gslabs[o].partial;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + PHYS_MEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                phys_mem_slab_head->gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = phys_mem_slab_head->gslabs[o].full;
                phys_mem_slab_head->gslabs[o].full = hdr;
            } else {
                /* Search free space for the next allocation */
                for ( i = 0; i < hdr->nr; i++ ) {
                    if ( 0 == hdr->marks[i] ) {
                        hdr->free = i;
                        break;
                    }
                }
            }
        } else if ( NULL != phys_mem_slab_head->gslabs[o].free ) {
            /* Partial list is empty, but free list is available. */
            hdr = phys_mem_slab_head->gslabs[o].free;
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + PHYS_MEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;
            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                phys_mem_slab_head->gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = phys_mem_slab_head->gslabs[o].full;
                phys_mem_slab_head->gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = phys_mem_slab_head->gslabs[o].partial;
                phys_mem_slab_head->gslabs[o].partial = hdr;
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
            s = (1ULL << (o + PHYS_MEM_SLAB_BASE_ORDER
                       + PHYS_MEM_SLAB_NR_OBJ_ORDER))
                + sizeof(struct phys_mem_slab);
            /* Align the page to fit to the buddy system, and get the order */
            nr = binorder(CEIL(s, PAGESIZE) / PAGESIZE);
            /* Allocate pages */
            hdr = phys_mem_alloc_pages(PHYS_MEM_ZONE_NORMAL, nr);
            if ( NULL == hdr ) {
                /* Unlock before return */
                spin_unlock(&phys_mem_slab_lock);
                return NULL;
            }
            /* Calculate the number of slab objects in this block; N.B., + 1 in
               the denominator is the `marks' for each objects. */
            hdr->nr = ((1 << nr) * PAGESIZE - sizeof(struct phys_mem_slab))
                / ((1 << (o + PHYS_MEM_SLAB_BASE_ORDER)) + 1);
            /* Reset counters */
            hdr->nused = 0;
            hdr->free = 0;
            /* Set the address of the first slab object */
            hdr->obj_head = (void *)((u64)hdr + ((1 << nr) * PAGESIZE)
                                     - ((1 << (o + PHYS_MEM_SLAB_BASE_ORDER))
                                        * hdr->nr));
            /* Reset marks and next cache */
            kmemset(hdr->marks, 0, hdr->nr);
            hdr->next = NULL;

            /* Retrieve a slab */
            ptr = (void *)((u64)hdr->obj_head + hdr->free
                           * (1 << (o + PHYS_MEM_SLAB_BASE_ORDER)));
            hdr->marks[hdr->free] = 1;
            hdr->nused++;

            if ( hdr->nr <= hdr->nused ) {
                /* Becomes full */
                hdr->free = -1;
                phys_mem_slab_head->gslabs[o].partial = hdr->next;
                /* Prepend to the full list */
                hdr->next = phys_mem_slab_head->gslabs[o].full;
                phys_mem_slab_head->gslabs[o].full = hdr;
            } else {
                /* Prepend to the partial list */
                hdr->next = phys_mem_slab_head->gslabs[o].partial;
                phys_mem_slab_head->gslabs[o].partial = hdr;
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
        ptr = phys_mem_alloc_pages(PHYS_MEM_ZONE_NORMAL,
                                   binorder(CEIL(size, PAGESIZE) / PAGESIZE));
    }

    /* Unlock */
    spin_unlock(&phys_mem_slab_lock);

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
    struct phys_mem_slab *hdr;
    struct phys_mem_slab **hdrp;

    /* Lock */
    spin_lock(&phys_mem_slab_lock);

    if ( 0 == (u64)ptr % PAGESIZE ) {
        /* Free pages */
        phys_mem_free_pages(ptr);
    } else {

        /* Search for each order */
        for ( i = 0; i < PHYS_MEM_SLAB_BASE_ORDER; i++ ) {
            asz = (1 << (i + PHYS_MEM_SLAB_BASE_ORDER));

            /* Search from partial */
            hdrp = &phys_mem_slab_head->gslabs[i].partial;

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
                        hdr->next = phys_mem_slab_head->gslabs[i].free;
                        phys_mem_slab_head->gslabs[i].free = hdr;
                    }
                    spin_unlock(&phys_mem_slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }

            /* Search from full */
            hdrp = &phys_mem_slab_head->gslabs[i].full;

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
                        hdr->next = phys_mem_slab_head->gslabs[i].free;
                        phys_mem_slab_head->gslabs[i].free = hdr;
                    } else {
                        /* To partial list */
                        *hdrp = hdr->next;
                        hdr->next = phys_mem_slab_head->gslabs[i].partial;
                        phys_mem_slab_head->gslabs[i].partial = hdr;
                    }
                    spin_unlock(&phys_mem_slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }
        }
    }

    /* Unlock */
    spin_unlock(&phys_mem_slab_lock);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
