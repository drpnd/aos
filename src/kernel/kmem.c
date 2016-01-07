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

/* ToDo: Implement segregated fit */


/* Prototype declarations */
static void * _kmem_alloc_superpages(struct kmem *, int);
static void * _kmem_alloc_pages(struct kmem *, int);
static void * _kmem_alloc_superpages_from_new_region(struct kmem *, int);
static void * _kmem_alloc_pages_from_new_region(struct kmem *, int);
static void * _kmem_alloc_pages_from_new_superpage(struct kmem *, int);

/*
 * Allocate pages
 */
void *
kmem_alloc_pages(struct kmem *kmem, size_t npg)
{
    int order;
    void *vaddr;

    /* Calculate the order in the buddy system from the number of pages */
    order = bitwidth(npg);

    /* Check the order */
    if ( order >= SP_SHIFT ) {
        /* Allocate virtual superpages */
        vaddr = _kmem_alloc_superpages(kmem, order - SP_SHIFT);
    } else {
        /* Allocate virtual pages */
        vaddr = _kmem_alloc_pages(kmem, order);
    }

    return vaddr;
}

void
kmem_free_pages(struct kmem *kmem, void *ptr)
{
    panic("FIXME: kmem_free_pages()");
}


/*
 * Allocate superpages
 */
static void *
_kmem_alloc_superpages(struct kmem *kmem, int order)
{
    struct vmem_superpage *spg;
    void *vaddr;
    void *paddr;
    ssize_t i;
    int ret;

    /* Allocate virtual superpages */
    spg = vmem_grab_superpages(kmem->space, order);
    if ( NULL == spg ) {
        /* No matching superpage found, then try to create a new region */
        return _kmem_alloc_superpages_from_new_region(kmem, order);
    }
    /* Superpage(s) are properly allocated, then try to allocate physical pages
       and set page table */
    vaddr = spg->region->start + SUPERPAGE_ADDR(spg - spg->region->superpages);

    /* Allocate physical memory */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order + SP_SHIFT);
    if ( NULL == paddr ) {
        /* Release the virtual memory */
        vmem_return_superpages(spg);
        return NULL;
    }

    /* Map the physical and virtual memory */
    for ( i = 0; i < (1LL << order); i++ ) {
        ret = arch_kmem_map(kmem->space, vaddr + SUPERPAGE_ADDR(i),
                            paddr + SUPERPAGE_ADDR(i), spg->flags);
        if ( ret < 0 ) {
            /* Release the virtual and physical memory */
            vmem_return_superpages(spg);
            pmem_free_pages(paddr);
            return NULL;
        }
    }

    return vaddr;
}

/*
 * Allocate pages
 */
static void *
_kmem_alloc_pages(struct kmem *kmem, int order)
{
    struct vmem_page *pg;
    void *vaddr;
    void *paddr;
    ssize_t i;
    int ret;

    /* Try to grab pages from the kernel memory space */
    pg = vmem_grab_pages(kmem->space, order);
    if ( NULL == pg ) {
        /* No available pages, then try to grab from superpage  */
        return _kmem_alloc_pages_from_new_superpage(kmem, order);
    }

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
    for ( i = 0; i < (1LL << order); i++ ) {
        ret = arch_kmem_map(kmem->space, vaddr + PAGE_ADDR(i),
                            paddr + PAGE_ADDR(i), pg->flags);
        if ( ret < 0 ) {
            /* Release the virtual and physical memory */
            vmem_return_pages(pg);
            pmem_free_pages(paddr);
            return NULL;
        }
    }

    return vaddr;
}

/*
 * Allocate superpages from new region
 */
static void *
_kmem_alloc_superpages_from_new_region(struct kmem *kmem, int order)
{
    void *vaddr;
    size_t sz;

    /* Check the size */
    sz = SUPERPAGESIZE * (1ULL << order) + sizeof(struct vmem_region);
    sz += sizeof(struct vmem_superpage) * DIV_CEIL(sz, SUPERPAGESIZE);
    while ( sz < SUPERPAGESIZE * (1ULL << order) + sizeof(struct vmem_region)
            + sizeof(struct vmem_superpage) * DIV_CEIL(sz, SUPERPAGESIZE) ) {
        sz++;
    }

    vaddr = vmem_search_available_region(kmem->space,
                                         SUPERPAGESIZE * (1ULL << order));
    (void)vaddr;

    panic("FIXME: alloc_superpages_from_new_region");
    return NULL;
}

/*
 * Allocate pages from new region
 */
static void *
_kmem_alloc_pages_from_new_region(struct kmem *kmem, int order)
{
    panic("FIXME: alloc_pages_from_new_region");
    return NULL;
}

/*
 * Allocate pages from a superpage
 */
static void *
_kmem_alloc_pages_from_new_superpage(struct kmem *kmem, int order)
{
    struct vmem_superpage *spg0;
    struct vmem_superpage *spg1;
    struct vmem_page *pg;
    void *vaddr0;
    void *vaddr1;
    void *paddr0;
    void *paddr1;
    ssize_t i;
    ssize_t j;
    int ret;
    int flags;
    size_t psz;
    int tmpo;
    int po;
    int so;

    /* Calculate the size of (struct vmem_page) * (SUPERPAGESIZE / PAGESIZE) */
    psz = sizeof(struct vmem_page) * (SUPERPAGESIZE / PAGESIZE) * 2;
    if ( psz > SUPERPAGESIZE ) {
        panic("FATAL: sizeof(struct vmem_page) is too large.");
        return NULL;
    }
    po = bitwidth(DIV_CEIL(psz, PAGESIZE));
    /* Allocate two superpages (one for struct vmem_page *) and the other for
       a set of pages in a superpage to be allocated */
    so = 1;

    /* Allocate a virtual superpage to be split */
    spg0 = vmem_grab_superpages(kmem->space, so);
    if ( NULL == spg0 ) {
        /* No matching superpage found, then try to create a new region */
        return _kmem_alloc_pages_from_new_region(kmem, so);
    }

    /* Allocate physical pages for (struct vmem_page *) */
    paddr1 = pmem_alloc_pages(PMEM_ZONE_LOWMEM, po);
    if ( NULL == paddr1 ) {
        /* Release the virtual memory */
        vmem_return_superpages(spg0);
        return NULL;
    }

    /* Superpage(s) are properly allocated, then try to convert superpage to
       pages; vaddr0 and vaddr1 point to the allocated pages and
       (struct vmem_page *), respectively. */
    vaddr0 = spg0->region->start
        + SUPERPAGE_ADDR(spg0 - spg0->region->superpages);
    vaddr1 = vaddr0 + (1ULL << order) * PAGESIZE;

    /* Remove the superpage flag */
    flags = spg0->flags & ~VMEM_SUPERPAGE;

    /* Superpage to pages; map pages first */
    for ( i = 0; i < (1LL << po); i++ ) {
        ret = arch_kmem_map(kmem->space, vaddr1 + PAGE_ADDR(i),
                            paddr1 + PAGE_ADDR(i), flags);
        if ( ret < 0 ) {
            vmem_return_superpages(spg0);
            pmem_free_pages(paddr1);
            return NULL;
        }
    }

    /* Change the superpage to a collection of pages */
    pg = (struct vmem_page *)vaddr1;
    spg0->u.page.pages = pg;
    spg0->flags = flags;
    spg0->order = 0;
    for ( i = 0; i < (1LL << po); i++ ) {
        /* Setup pages */
        pg[i].addr = (reg_t)vaddr1 + PAGE_ADDR(i);
        pg[i].order = po;
        pg[i].flags = flags;
        pg[i].superpage = spg0;
        pg[i].next = NULL;
        pg[i].prev = NULL;
    }
    /* Add the rest to the buddy system of usable pages */
    for ( tmpo = po; tmpo < SP_SHIFT; tmpo++ ) {
        spg0->region->pgheads[tmpo]->prev = &pg[i];
        pg[i].next = spg0->region->pgheads[tmpo];
        pg[i].prev = NULL;
        for ( j = 0; j < (1LL << tmpo); j++ ) {
            pg[i].addr = 0;
            pg[i].order = tmpo;
            pg[i].flags = flags & ~VMEM_USED;
            pg[i].superpage = spg0;
            i++;
        }
    }

    /* Second superpage for pages */
    spg1 = spg0 + 1;
    spg1->order = 0;

    /* Allocate physical pages */
    paddr0 = pmem_alloc_pages(PMEM_ZONE_LOWMEM, order);
    if ( NULL == paddr0 ) {
        /* Release the virtual memory */
        vmem_return_superpages(spg0);
        vmem_return_superpages(spg1);
        pmem_free_pages(paddr1);
        return NULL;
    }

    /* Remove the superpage flag */
    flags = spg1->flags & ~VMEM_SUPERPAGE;

    /* Superpage to pages; map pages first */
    for ( i = 0; i < (1LL << order); i++ ) {
        ret = arch_kmem_map(kmem->space, vaddr0 + PAGE_ADDR(i),
                            paddr0 + PAGE_ADDR(i), flags);
        if ( ret < 0 ) {
            vmem_return_superpages(spg0);
            vmem_return_superpages(spg1);
            pmem_free_pages(paddr0);
            pmem_free_pages(paddr1);
            return NULL;
        }
    }

    /* Change the superpage to a collection of pages */
    pg = (struct vmem_page *)vaddr0;
    spg1->u.page.pages = pg;
    spg1->flags = flags;
    spg1->order = 0;
    for ( i = 0; i < (1LL << order); i++ ) {
        /* Setup pages */
        pg[i].addr = (reg_t)vaddr0 + PAGE_ADDR(i);
        pg[i].order = order;
        pg[i].flags = flags;
        pg[i].superpage = spg1;
        pg[i].next = NULL;
        pg[i].prev = NULL;
    }
    /* Add the rest to the buddy system of usable pages */
    for ( tmpo = order; tmpo < SP_SHIFT; tmpo++ ) {
        spg1->region->pgheads[tmpo]->prev = &pg[i];
        pg[i].next = spg1->region->pgheads[tmpo];
        pg[i].prev = NULL;
        for ( j = 0; j < (1LL << tmpo); j++ ) {
            pg[i].addr = 0;
            pg[i].order = tmpo;
            pg[i].flags = flags & ~VMEM_USED;
            pg[i].superpage = spg1;
            i++;
        }
    }

    return vaddr0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
