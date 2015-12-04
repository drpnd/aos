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

#define FLOOR(val, base)        (((val) / (base)) * (base))
#define CEIL(val, base)         ((((val) - 1) / (base) + 1) * (base))
#define DIV_FLOOR(val, base)    ((val) / (base))
#define DIV_CEIL(val, base)     (((val) - 1) / (base) + 1)

/* Type of memory area */
#define BSE_USABLE              1
#define BSE_RESERVED            2
#define BSE_ACPI_RECLAIMABLE    3
#define BSE_ACPI_NVS            4
#define BSE_BAD                 5

/*
 * Prototype declarations
 */
static struct kmem * _kmem_init(u64, u64);
static int _pmem_alloc(struct bootinfo *, void **, u64 *);
static u64 _resolve_phys_mem_size(struct bootinfo *);
static u64 _find_pmem_region(struct bootinfo *, u64 );
static int
_init_pages(struct bootinfo *, struct pmem *, u64, u64);
static int _init_pmem_zone_buddy(struct pmem *, struct acpi *);
static int _aligned_usable_pages(struct pmem *, struct acpi *, u64, int *);
static int _count_linear_page_tables(u64);
static int _prepare_kernel_page_table(u64 *, u64);
static int _pmem_split(struct pmem_buddy *, int);
static void _pmem_merge(struct pmem_buddy *, void *, int);
static void _pmem_return_to_buddy(struct pmem *, void *, int, int);
static int _pmem_page_zone(void *);
static u64 _pmem_size(u64, int);
static struct pmem * _pmem_init(u64, u64, int);
static void _enable_page_global(void);
static void _disable_page_global(void);

/*
 * Initialize the memory management module
 *
 * SYNOPSIS
 *      int
 *      arch_memory_init(struct bootinfo *bi, struct acpi *acpi);
 *
 * DESCRIPTION
 *      The arch_memory_init() function initializes the memory management module
 *      including physical and kernel memory.
 *
 * RETURN VALUES
 *      If successful, the arch_memory_init() function returns the value of 0.
 *      Otherwise, it returns the value of -1.
 */
int
arch_memory_init(struct bootinfo *bi, struct acpi *acpi)
{
    void *pmbase;
    u64 pmsz;
    int ret;

    /* Allocate the physical memory management data structure */
    ret = _pmem_alloc(bi, &pmbase, &pmsz);
    if ( ret < 0 ) {
        return -1;
    }

    /* Initialize the kernel page table */
    int pt;
    int pt2m;
    int pt4k;

    pt = DIV_CEIL(pmsz, 4096);
    pt2m = pt / 4096;
    pt4k = pt % 4096;

    if ( pt2m >= 8 ) {
        /* Use 2 MiB page table for huge memory machines */
    }

    return 0;
}

/*
 * Allocate physical memory management data structure
 *
 * SYNOPSIS
 *      static int
 *      _pmem_alloc(struct bootinfo *bi, void **base, u64 *pmsz);
 *
 * DESCRIPTION
 *      The _pmem_alloc() function allocates a space for the physical memory
 *      manager from the memory map information bi inherited from the boot
 *      monitor.  The base address and the size of the allocated memory space is
 *      returned through the second and third arguments, base and pmsz.
 *
 * RETURN VALUES
 *      If successful, the _pmem_alloc() function returns the value of 0.  It
 *      returns the value of -1 on failure.
 */
static int
_pmem_alloc(struct bootinfo *bi, void **base, u64 *pmsz)
{
    u64 sz;
    u64 npg;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return -1;
    }

    /* Obtain memory (space) size from the system address map */
    sz = _resolve_phys_mem_size(bi);

    /* Calculate the number of pages from the upper-bound of the memory space */
    npg = DIV_CEIL(sz, PAGESIZE);

    /* Calculate the size required by the pmem and pmem_page structures */
    *pmsz = sizeof(struct pmem) + npg * sizeof(struct pmem_page);

    /* Fine the available region for the pmem data structure */
    *base = (void *)_find_pmem_region(bi, *pmsz);
    /* Could not find available pages for the management structure */
    if ( NULL == *base ) {
        return -1;
    }

    return 0;
}

/*
 * Initialize physical memory
 *
 * SYNOPSIS
 *      struct pmem *
 *      arch_pmem_init(struct bootinfo *bi, struct acpi *acpi);
 *
 * DESCRIPTION
 *      The arch_pmem_init() function initializes the physical memory manager
 *      with the memory map information bi inherited from the boot monitor.
 *      The third argument acpi is used to determine the proximity domain of the
 *      memory spaces.
 *
 * RETURN VALUES
 *      If successful, the arch_pmem_init() function returns the pointer to the
 *      physical memory manager.  It returns NULL on failure.
 */
struct pmem *
arch_pmem_init(struct bootinfo *bi, struct acpi *acpi)
{
    struct pmem *pm;
    struct kmem *km;
    u64 npg;
    u64 base;
    u64 sz;
    u64 pmsz;
    int ret;
    int nzme;

    /* Allocate physical memory management data structure */
    ret = _pmem_alloc(bi, &base, &pmsz);
    if ( ret < 0 ) {
        return NULL;
    }
    km = _kmem_init(base, pmsz);

    /* to kernel page table */
    panic("stop here for refactoring");

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return NULL;
    }

    /* Disable the global page feature */
    _disable_page_global();

    /* Obtain memory (space) size from the system address map */
    sz = _resolve_phys_mem_size(bi);

    /* Calculate the number of pages from the upper-bound of the memory space */
    npg = DIV_CEIL(sz, PAGESIZE);

    /* Count the number of entries in ACPI SRAT for memory zone mapping */
    nzme = acpi_memory_count_entries(acpi);
    if ( nzme < 0 ) {
        /* It's UMA, then we use a single zone. */
        nzme = 0;
    }

    /* Calculate the required memory size for pages */
    pmsz = _pmem_size(npg, nzme);

    /* Fine the available region for the pmem data structure */
    base = _find_pmem_region(bi, pmsz);
    /* Could not find available pages for the management structure */
    if ( 0 == base ) {
        return NULL;
    }

    /* Setup the memory page management structure */
    //pm = _pmem_init(base, npg, nzme);

    /* Update the zone map information */
    acpi_memory_zone_map(acpi, &pm->zmap);

    /* Prepare a page table for kernel */
    //ret = _prepare_kernel_page_table(pm->arch, sz);
    ret = -1;
    if ( ret < 0 ) {
        return NULL;
    }

    /* Set the page table of kernel */
    //set_cr3(pm->arch);

    /* Initialize all available pages */
    ret = _init_pages(bi, pm, base, pmsz);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Initialize all usable pages with the buddy system except for wired
       memory.  Note that the wired memory space is the range from 0 to
       PMEM_LBOUND, and the space used by pmem. */
    ret = _init_pmem_zone_buddy(pm, acpi);

    return pm;
}


/* Initialize virtual memory space for kernel */
static struct kmem *
_kmem_init(u64 pmbase, u64 pmsz)
{
    u64 i;
    u64 off;
    struct kmem *kmem;
    struct vmem_space *space;
    struct vmem_region *reg_low;
    struct vmem_region *reg_pmem;
    struct vmem_region *reg_kernel;
    struct vmem_page *pgs_low;
    struct vmem_page *pgs_pmem;
    struct vmem_page *pgs_kernel;
    struct kmem_free_page *fpg;

    /* Get the kernel memory base address */
    off = 0;

    /* Kernel memory space */
    kmem = (struct kmem *)(KMEM_BASE + off);
    off += sizeof(struct kmem);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(kmem, 0, sizeof(struct kmem));

    /* Virtual memory space */
    space = (struct vmem_space *)(KMEM_BASE + off);
    off += sizeof(struct vmem_space);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(space, 0, sizeof(struct vmem_space));

    /* Low address space (below 32 MiB): Note that this operating system has
       ``two'' kernel regions shared  unlike other UNIX-like systems, regions
       from 0 to 1 GiB and from 3 to 4 GiB.  The first region could be removed
       by relocating the kernel, but this version of our operating system does
       not do it. */
    reg_low = (struct vmem_region *)(KMEM_BASE + off);
    off += sizeof(struct vmem_region);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_low, 0, sizeof(struct vmem_region));
    reg_low->start = (void *)0;
    reg_low->len = PMEM_LBOUND;

    /* Physical pages: This region is not placed at the kernel region because
       this is not directly referred from user-land processes (e.g., through
       system calls). */
    reg_pmem = (struct vmem_region *)(KMEM_BASE + off);
    off += sizeof(struct vmem_region);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_pmem, 0, sizeof(struct vmem_region));
    reg_pmem->start = (void *)0x40000000ULL;
    reg_pmem->len = CEIL(pmsz, PAGESIZE);

    /* Kernel address space (3-4 GiB) */
    reg_kernel = (struct vmem_region *)(KMEM_BASE + off);
    off += sizeof(struct vmem_region);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_kernel, 0, sizeof(struct vmem_region));
    reg_kernel->start = (void *)0xc0000000ULL;
    reg_kernel->len = 0x40000000ULL;

    /* Page-alignment */
    off = CEIL(off, PAGESIZE);

    /* Prepare page data structures */
    pgs_low = (struct vmem_page *)(KMEM_BASE + off);
    off += sizeof(struct vmem_page) * DIV_CEIL(PMEM_LBOUND, PAGESIZE);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_low, 0, sizeof(struct vmem_page)
            * DIV_CEIL(PMEM_LBOUND, PAGESIZE));
    pgs_pmem = (struct vmem_page *)(KMEM_BASE + off);
    off += sizeof(struct vmem_page) * DIV_CEIL(pmsz, PAGESIZE);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_pmem, 0, sizeof(struct vmem_page) * DIV_CEIL(pmsz, PAGESIZE));
    pgs_kernel = (struct vmem_page *)(KMEM_BASE + off);
    off += sizeof(struct vmem_page) * DIV_CEIL(0x40000000ULL, PAGESIZE);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_kernel, 0, sizeof(struct vmem_page)
            * DIV_CEIL(0x40000000ULL, PAGESIZE));

    /* Page-alignment */
    off = CEIL(off, PAGESIZE);

    /* Add the remaining pages to the free page list */
    kmem->free_pgs = NULL;
    for ( i = DIV_CEIL(off, PAGESIZE);
          i < DIV_FLOOR(KMEM_MAX_SIZE, PAGESIZE) - 1; i++ ) {
        /* Prepend a page */
        fpg = (struct kmem_free_page *)(KMEM_BASE + PAGESIZE * i);
        fpg->next = kmem->free_pgs;
        kmem->free_pgs = fpg;
    }

    /* Create the chain of regions */
    space->first_region = reg_low;
    reg_low->next = reg_pmem;
    reg_pmem->next = reg_kernel;
    reg_kernel->next = NULL;

    /* Set the virtual memory space to the kmem data structure */
    kmem->space = space;

    /* Initialize the page mapping to the kernel space */
    for ( i = 0; i < DIV_CEIL(PMEM_LBOUND, PAGESIZE); i++ ) {
        pgs_low[i].addr = PAGE_ADDR(i);
        pgs_low[i].region = reg_low;
        pgs_low[i].order = 0;
        pgs_low[i].type = 0;
        pgs_low[i].next = NULL;
    }
    for ( i = 0; i < DIV_CEIL(pmsz, PAGESIZE); i++ ) {
        pgs_pmem[i].addr = pmbase + PAGE_ADDR(i);
        pgs_pmem[i].region = reg_pmem;
        pgs_pmem[i].order = 0;
        pgs_pmem[i].type = 0;
        pgs_pmem[i].next = NULL;
    }
    for ( i = 0; i < DIV_CEIL(0x40000000ULL, PAGESIZE); i++ ) {
        pgs_kernel[i].addr = 0;
        pgs_kernel[i].region = reg_kernel;
        pgs_kernel[i].order = 0;
        pgs_kernel[i].type = 0;
        pgs_kernel[i].next = NULL;
    }


    return kmem;
}
















/*
 * Allocate 2^order physical pages from the zone
 *
 * SYNOPSIS
 *      void *
 *      pmem_alloc_pages(int zone, int order);
 *
 * DESCRIPTION
 *      The pmem_alloc_pages() function allocates 2^order pages from the zone.
 *
 * RETURN VALUES
 *      The pmem_alloc_pages() function returns a pointer to allocated page(s).
 *      IF there is an error, it returns a NULL pointer.
 */
void *
arch_pmem_alloc_pages(int zone, int order)
{
    void *a;
    void *cr3;
    int ret;
    u64 i;

    /* Check the argument of the allocation order */
    if ( order < 0 ) {
        /* Invalid argument */
        return NULL;
    }

    /* Check the size */
    if ( order > PMEM_MAX_BUDDY_ORDER ) {
        /* Oversized request */
        return NULL;
    }

    /* Check the zone */
    if ( zone < 0 || zone >= PMEM_MAX_ZONES ) {
        /* Invalid zone */
        return NULL;
    }

    /* Take the lock */
    spin_lock(&pmem->lock);

#if 0
    /* Save the cr3 */
    cr3 = get_cr3();

    /* Linear addressing */
    _disable_page_global();
    set_cr3(pmem->arch);

    /* Split the upper-order's buddy first if needed */
    ret = _pmem_split(&pmem->zones[zone].buddy, order);
    if ( ret < 0 ) {
        /* Restore cr3 then unlock */
        set_cr3(cr3);
        spin_unlock(&pmem->lock);
        return NULL;
    }

    /* Obtain the contiguous pages from the head */
    a = pmem->zones[zone].buddy.heads[order];
    pmem->zones[zone].buddy.heads[order] = a->next;
    if ( NULL != pmem->zones[zone].buddy.heads[order] ) {
        pmem->zones[zone].buddy.heads[order]->prev = NULL;
    }

    /* Restore cr3 */
    set_cr3(cr3);
    _enable_page_global();

    /* Mark the allocated memory as used, and set the order */
    for ( i = 0; i < (1ULL << order); i++ ) {
        /* Assert that the page is not used */
        if ( pmem->pages[PAGE_INDEX(a) + i].used ) {
            /* Raise kernel panic */
            panic("Fatal: arch_pmem_alloc_pages()");
        }
        pmem->pages[PAGE_INDEX(a) + i].used = 1;
    }
    pmem->pages[PAGE_INDEX(a)].order = order;
#endif

    /* Release the lock */
    spin_unlock(&pmem->lock);

    return a;
}

/*
 * Allocate a page from the zone
 */
void *
arch_pmem_alloc_page(int zone)
{
    return arch_pmem_alloc_pages(zone, 0);
}


/*
 * Free allocated 2^order superpages
 *
 * SYNOPSIS
 *      void
 *      arch_pmem_free_pages(void *page);
 *
 * DESCRIPTION
 *      The arch_pmem_free_pages() function deallocates superpages pointed by
 *      page.
 *
 * RETURN VALUES
 *      The arch_pmem_free_pages() function does not return a value.
 */
void
arch_pmem_free_pages(void *page)
{
    int zone;
    int order;
    void *cr3;
    u64 i;

    /* Obtain the order of the page */
    order = pmem->pages[PAGE_INDEX(page)].order;

    /* Resolve the zone of the pages to be released */
    zone = _pmem_page_zone(page);

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order > PMEM_MAX_BUDDY_ORDER || order < 0 ) {
        /* Something is wrong... */
        return;
    }

    /* Lock */
    spin_lock(&pmem->lock);
#if 0
    /* Save the cr3 */
    cr3 = get_cr3();

    /* Linear addressing */
    _disable_page_global();
    set_cr3(pmem->arch);

    /* Return it to the buddy system */
    _pmem_return_to_buddy(pmem, page, zone, order);

    /* Merge buddies if possible */
    _pmem_merge(&pmem->zones[zone].buddy, page, order);

    /* Clear the used flag */
    for ( i = 0; i < (1ULL << order); i++ ) {
        pmem->pages[PAGE_INDEX(page) + i].used = 0;
    }

    /* Restore cr3 */
    set_cr3(cr3);
    _enable_page_global();
#endif
    /* Unlock */
    spin_unlock(&pmem->lock);
}

/*
 * Find the upper bound (highest address) of the memory region
 */
static u64
_resolve_phys_mem_size(struct bootinfo *bi)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 addr;
    u64 i;

    /* Obtain memory size */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( bse->base + bse->len > addr ) {
            /* Get the highest address */
            addr = bse->base + bse->len;
        }
    }

    return addr;
}

/*
 * Find the memory region for the pmem data structure
 */
static u64
_find_pmem_region(struct bootinfo *bi, u64 sz)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 addr;
    u64 i;
    u64 a;
    u64 b;

    /* Search free space system address map obitaned from BIOS for the memory
       allocator (calculated above) */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( BSE_USABLE == bse->type ) {
            /* Available space from a to b */
            a = CEIL(bse->base, PAGESIZE);
            b = FLOOR(bse->base + bse->len, PAGESIZE);

            if ( b < PMEM_LBOUND ) {
                /* Skip below the lower bound */
                continue;
            } else if ( a < PMEM_LBOUND ) {
                /* Check the space from the lower bound to b */
                if ( b - PMEM_LBOUND >= sz ) {
                    /* Found */
                    addr = PMEM_LBOUND;
                    break;
                } else {
                    /* Not found, then search another space */
                    continue;
                }
            } else {
                if ( b - a >= sz ) {
                    /* Found */
                    addr = a;
                    break;
                } else {
                    /* Not found, then search another space */
                    continue;
                }
            }
        }
    }

    return addr;
}

/*
 * Initialize all pages
 */
static int
_init_pages(struct bootinfo *bi, struct pmem *pm, u64 pmbase, u64 pmsz)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 i;
    u64 j;
    u64 a;
    u64 b;

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( BSE_USABLE == bse->type ) {
            /* Available */
            a = DIV_CEIL(bse->base, PAGESIZE);
            b = DIV_FLOOR(bse->base + bse->len, PAGESIZE);

            /* Mark usable pages */
            for ( j = a; j < b; j++ ) {
                if ( j >= pm->nr ) {
                    /* Overflowed page.  This must not be reached because the
                       number of pages should properly counted, but doublecheck
                       it here. */
                    return -1;
                }
                //pm->pages[j].usable = 1;
                pm->pages[j].order = PMEM_INVAL_BUDDY_ORDER;

                /* Mark the pages below the lower bound as used */
                if ( PAGE_ADDR(j + 1) < PMEM_LBOUND ) {
                    //pm->pages[j].used = 1;
                }
            }
        }
    }

    /* Mark the pages used by the pmem data structure */
    for ( i = PAGE_INDEX(pmbase); i <= PAGE_INDEX(pmbase + pmsz - 1); i++ ) {
        //pm->pages[i].used = 1;
    }

    return 0;
}

/*
 * Return the number of usable pages in the power of 2
 */
static int
_aligned_usable_pages(struct pmem *pm, struct acpi *acpi, u64 pg, int *zone)
{
    int o;
    u64 i;
    u64 pxbase;
    u64 pxlen;
    int prox;

#if 0
    /* NUMA-conscious zones */
    if ( acpi_is_numa(acpi) ) {
        /* Resolve the proximity domain of the first page */
        prox = acpi_memory_prox_domain(acpi, PAGE_ADDR(pg), &pxbase, &pxlen);
        if ( prox < 0 ) {
            /* No proximity domain; then return -1, meaning unusable page,
               here. */
            return -1;
        }
        *zone = PMEM_ZONE_NUMA(prox);
    } else {
        prox = 0;
        pxbase = 0;
        pxlen = PAGE_ADDR(pm->nr);
        *zone = PMEM_ZONE_NUMA(prox);
    }

    for ( o = 0; o <= PMEM_MAX_BUDDY_ORDER; o++ ) {
        /* Try the order of o */
        for ( i = pg; i < pg + (1ULL << o); i++ ) {
            /* Check if this page is usable */
            if ( !pm->pages[i].usable || pm->pages[i].used ) {
                /* It contains unusable page, then return the current order
                   minus 1 immediately. */
                return o - 1;
            }
            /* Check the proximity domain */
            if ( PAGE_ADDR(i) < pxbase || PAGE_ADDR(i + 1) > pxbase + pxlen ) {
                /* This page is (perhaps) based on a different proximity domain,
                   then return the current order minus 1 immedicately. */
                return o - 1;
            }
        }

        /* Test whether the next order is feasible; feasible if it is properly
           aligned and the pages are within the range of the physical memory
           space. */
        if ( 0 != (pg & (1ULL << o)) || pg + (1ULL << (o + 1)) > pm->nr ) {
            /* Infeasible, then return the current order immediately */
            return o;
        }
    }
#endif

    return o;
}

/*
 * Initialize all pages
 */
static int
_init_pmem_zone_buddy(struct pmem *pm, struct acpi *acpi)
{
    u64 i;
    int o;
    int zone;

    for ( i = 0; i < pm->nr; i += (1ULL << o) ) {
        /* Find the maximum contiguous usable pages fitting to the alignment of
           the buddy system */
        o = _aligned_usable_pages(pm, acpi, i, &zone);
        if ( o < 0 ) {
            /* Skip an unusable page */
            o = 0;
            continue;
        }

        /* Add usable pages to the buddy */
        _pmem_return_to_buddy(pm, (void *)PAGE_ADDR(i), zone, o);
        _pmem_merge(&pm->zones[zone].buddy, (void *)PAGE_ADDR(i), o);
    }

    return 0;
}

/*
 * Count the total number of tables required for linear address mapping
 */
static int
_count_linear_page_tables(u64 sz)
{
    int npd;
    int npdpt;
    int npml4;
    int n;

    /* Calculate the number of tables at each level */
    npd = DIV_CEIL(sz, 1ULL << PMEM_PD);
    npdpt = DIV_CEIL(npd, PMEM_PTNENT);
    npml4 = DIV_CEIL(npdpt, PMEM_PTNENT);
    if ( npml4 > 512 ) {
        /* Cannot have multiple blocks for PML4 */
        return -1;
    }

    /* Total number of tables */
    n = npd + npdpt + npml4;

    return n;
}

/*
 * Prepare a page table of the kernel space
 */
static int
_prepare_kernel_page_table(u64 *pt, u64 sz)
{
    int npd;
    int npdpt;
    int npml4;
    int i;
    u64 *pml4;
    u64 *pdpt;
    u64 *pd;

    /* Calculate the number of tables at each level */
    npd = DIV_CEIL(sz, 1ULL << PMEM_PD);
    npdpt = DIV_CEIL(npd, PMEM_PTNENT);
    npml4 = DIV_CEIL(npdpt, PMEM_PTNENT);
    if ( npml4 > 512 ) {
        /* Cannot have multiple blocks for PML4 */
        return -1;
    }

    /* Pointers */
    pml4 = pt;
    pdpt = pml4 + PMEM_PTNENT;
    pd = pdpt + PMEM_PTNENT * npml4;

    /* PML4 */
    for ( i = 0; i < npml4; i++ ) {
        pml4[i] = (u64)(pdpt + i * PMEM_PTNENT) | 0x7;
    }
    /* PDPT */
    for ( i = 0; i < npdpt; i++ ) {
        pdpt[i] = (u64)(pd + i * PMEM_PTNENT) | 0x7;
    }
    /* PD */
    for ( i = 0; i < npd; i++ ) {
        pd[i] = ((1ULL << PMEM_PD) * i) | 0x83;
    }

    return 0;
}

#if 0
/*
 * Split the buddies so that we get at least one buddy at the order of o
 */
static int
_pmem_split(struct pmem_buddy *buddy, int o)
{
    int ret;
    struct pmem_page_althdr *next;

    /* Check the head of the current order */
    if ( NULL != buddy->heads[o] ) {
        /* At least one memory block (buddy) is available in this order. */
        return 0;
    }

    /* Check the order */
    if ( o + 1 >= PMEM_MAX_BUDDY_ORDER ) {
        /* No space available */
        return -1;
    }

    /* Check the upper order */
    if ( NULL == buddy->heads[o + 1] ) {
        /* The upper order is also empty, then try to split one more upper. */
        ret = _pmem_split(buddy, o + 1);
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
    buddy->heads[o]->next = (struct pmem_page_althdr *)
        ((u64)buddy->heads[o] + PAGESIZE * (1ULL << o));
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
_pmem_merge(struct pmem_buddy *buddy, void *addr, int o)
{
    int found;
    u64 a0;
    u64 a1;
    struct pmem_page_althdr *p0;
    struct pmem_page_althdr *p1;
    struct pmem_page_althdr *list;

    if ( o + 1 >= PMEM_MAX_BUDDY_ORDER ) {
        /* Reached the maximum order, then terminate */
        return;
    }

    /* Get the first page of the upper order buddy */
    a0 = FLOOR((u64)addr, PAGESIZE * (1ULL << (o + 1)));
    /* Get the neighboring page of the buddy */
    a1 = a0 + PAGESIZE * (1ULL << o);

    /* Convert pages to the page alternative headers */
    p0 = (struct pmem_page_althdr *)a0;
    p1 = (struct pmem_page_althdr *)a1;

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
        /* Either of the buddy is not free, then terminate */
        return;
    }

    /* Remove both from the list at the current order */
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
    _pmem_merge(buddy, p0, o + 1);
}

/*
 * Return 2^order pages to the buddy system of the specified zone
 */
static void
_pmem_return_to_buddy(struct pmem *pm, void *addr, int zone, int order)
{
    struct pmem_page_althdr *list;

    /* Return it to the buddy system */
    list = pm->zones[zone].buddy.heads[order];
    /* Prepend the returned pages */
    pm->zones[zone].buddy.heads[order] = addr;
    pm->zones[zone].buddy.heads[order]->prev = NULL;
    pm->zones[zone].buddy.heads[order]->next = list;
    if ( NULL != list ) {
        list->prev = addr;
    }
}
#endif

/*
 * Resolve the zone of the page
 */
static int
_pmem_page_zone(void *page)
{
    if ( (u64)page < 0x1000000 ) {
        /* DMA */
        return PMEM_ZONE_DMA;
    } else if ( (u64)page < 0x100000000ULL ) {
        /* Low address memory space */
        return PMEM_ZONE_LOWMEM;
    } else {
        /* High address memory space */
        if ( 0 ) {
            /* FIXME: Referring to the zone map */
            return PMEM_ZONE_NUMA(0);
        } else {
            return PMEM_ZONE_UMA;
        }
    }
}

/*
 * Calculate the memory size required by pmem structure, including the spaces
 * pointed by the structure
 */
static u64
_pmem_size(u64 npg, int nzme)
{
    u64 pmsz;

    /* Calculate the required memory size for pages
     *   - Page table: (Page table size) * (# of entries)
     *   - Page info.: (Page structure size) * (# of pages)
     *   - Zone map: (Zone map entry size) * (# of entries)
     *   - Self: Physical memory structure
     */
    pmsz = npg * sizeof(struct pmem_page)
        + sizeof(struct pmem_zone_map_entry) * nzme
        + sizeof(struct pmem);

    return pmsz;
}

/*
 * Initialize the physical memory
 */
static struct pmem *
_pmem_init(u64 base, u64 npg, int nzme)
{
    struct pmem *pm;
    struct pmem pmtmp;

    /* Setup the memory page management structure */
    kmemset(&pmtmp, 0, sizeof(struct pmem));
    /* Page */
    pmtmp.nr = npg;
    pmtmp.pages = (struct pmem_page *)base;
    kmemset(pmtmp.pages, 0, sizeof(struct pmem_page) * npg);
    base += npg * sizeof(struct pmem_page);
    /* Zone map */
    pmtmp.zmap.nr = nzme;
    pmtmp.zmap.entries = (struct pmem_zone_map_entry *)base;
    kmemset(pmtmp.zmap.entries, 0, sizeof(struct pmem_zone_map_entry) * nzme);
    base += nzme * sizeof(struct pmem_zone_map_entry);
    /* Physical memory */
    pm = (struct pmem *)base;
    kmemcpy(pm, &pmtmp, sizeof(struct pmem));

    return pm;
}

/*
 * Enable the global page feature
 */
static void
_enable_page_global(void)
{
    /* Enable the global page feature */
    set_cr4(get_cr4() | CR4_PGE);
}

/*
 * Disable the global page feature
 */
static void
_disable_page_global(void)
{
    /* Disable the global page feature */
    set_cr4(get_cr4() & ~CR4_PGE);
}

/*
 * Initialize the kernel memory
 */
struct kmem *
arch_kmem_init(void)
{
    struct kmem *kmem;
    struct vmem_region *region[2];
    int i;

    /* Check the size of the kmem data structure first */
    if ( sizeof(struct kmem) + sizeof(struct vmem_region) * 2
         + sizeof(struct kmem_page) * 1024 > KMEM_MAX_SIZE ) {
        return NULL;
    }

    /* Initialize the kmem data structure */
    kmem = (struct kmem *)KMEM_BASE;
    kmemset(kmem, 0, sizeof(struct kmem));

    /* Prepare regions: Note that this operating system has two kernel regions
       unlike other UNIX-like systems in the region from 0 to 1 GiB and from 3
       to 4 GiB.  The first region could be removed by relocating the kernel,
       but this operating system does not do it. */
    region[0] = (struct vmem_region *)(KMEM_BASE + sizeof(struct kmem));
    region[1] = (struct vmem_region *)(KMEM_BASE + sizeof(struct kmem)
                                       + sizeof(struct vmem_region));
    region[0]->start = (ptr_t)0;
    region[0]->len = (1ULL << 30);
    region[0]->pages = (struct vmem_page *)(KMEM_BASE + sizeof(struct kmem)
                                            + sizeof(struct vmem_region) * 2);
    region[0]->next = region[1];
    region[1]->start = (ptr_t)(3ULL << 30);
    region[1]->len = (1ULL << 30);
    region[1]->pages = (struct vmem_page *)(KMEM_BASE + sizeof(struct kmem)
                                            + sizeof(struct vmem_region) * 2
                                            + sizeof(struct vmem_page) * 512);
    region[1]->next = NULL;

    /* 512 pages in a region */
    for ( i = 0; i < 512; i++ ) {
        if ( SUPERPAGE_ADDR(i) < PMEM_LBOUND ) {
            region[0]->pages[i].addr = SUPERPAGE_ADDR(i);
            region[0]->pages[i].type = 1;
        } else {
            region[0]->pages[i].addr = 0;
            region[0]->pages[i].type = 0;
        }
        region[1]->pages[i].addr = SUPERPAGE_ADDR(i) + (3ULL << 30);
        region[1]->pages[i].type = 0;
    }

    return kmem;
}



/*
 * Remap kernel memory space in the page table
 */
int
kmem_remap(u64 vaddr, u64 paddr, int flag)
{
    int pml4;
    int pdpt;
    int pd;
    u64 *ent;

    pml4 = (vaddr >> 39);
    pdpt = (vaddr >> 30) & 0x1ff;
    pd = (vaddr >> 21) & 0x1ff;

    /* PML4 */
    ent = (u64 *)KERNEL_PGT;
    if ( !(ent[pml4] & 0x1) ) {
        /* Not present */
        return -1;
    }
    /* PDPT */
    ent = (u64 *)(ent[pml4] & 0xfffffffffffff000ULL);
    if ( 0x1 != (ent[pdpt] & 0x81) ) {
        /* Not present, or 1-Gbyte page */
        return -1;
    }
    /* PD */
    ent = (u64 *)(ent[pdpt] & 0xfffffffffffff000ULL);
    if ( 0x01 == (ent[pd] & 0x81) ) {
        /* Present, and 4-Kbyte page */
        return -1;
    }

    /* Update the entry */
    if ( flag ) {
        ent[pd] = (paddr & 0xffffffffffe00000ULL) | 0x183;
    } else {
        ent[pd] = (paddr & 0xffffffffffe00000ULL) | 0x000;
    }

    /* Invalidate the TLB cache for this entry */
    invlpg((void *)(vaddr & 0xffffffffffe00000ULL));

    return 0;
}

/*
 * Resolve the physical address
 */
u64
kmem_paddr(u64 vaddr)
{
    int pml4;
    int pdpt;
    int pd;
    u64 *ent;

    pml4 = (vaddr >> 39);
    pdpt = (vaddr >> 30) & 0x1ff;
    pd = (vaddr >> 21) & 0x1ff;

    /* PML4 */
    ent = (u64 *)KERNEL_PGT;
    if ( !(ent[pml4] & 0x1) ) {
        /* Not present */
        return -1;
    }
    /* PDPT */
    ent = (u64 *)(ent[pml4] & 0xfffffffffffff000ULL);
    if ( 0x1 != (ent[pdpt] & 0x81) ) {
        /* Not present, or 1-Gbyte page */
        return -1;
    }
    /* PD */
    ent = (u64 *)(ent[pdpt] & 0xfffffffffffff000ULL);
    if ( 0x81 != (ent[pd] & 0x81) ) {
        /* Not present, or 4-Kbyte page */
        return -1;
    }

    return (ent[pd] & 0xffffffffffe00000ULL) | (vaddr & 0x1fffffULL);
}

/*
 * Initialize the architecture-specific virtual memory data structure
 */
int
vmem_arch_init(struct vmem_space *vmem)
{
    struct arch_vmem_space *arch;
    u64 *ent;
    int i;

    /* Allocate an architecture-specific virtual memory space */
    arch = kmalloc(sizeof(struct arch_vmem_space));
    if ( NULL == arch ) {
        return -1;
    }
    arch->pgt = kmalloc(SUPERPAGESIZE);
    if ( NULL == arch->pgt ) {
        kfree(arch);
        return -1;
    }
    vmem->arch = arch;

    /* Setup the kernel region */

    /* PML4 */
    ent = (u64 *)arch->pgt;
    ent[0] = kmem_paddr((u64)&ent[512]) | 0xf;

    /* PDPT */
    ent[512] = (KERNEL_PGT + 4096 * 2) | 0x7;
    ent[513] = kmem_paddr((u64)&ent[1024]) | 0xf;
    ent[514] = kmem_paddr((u64)&ent[1536]) | 0xf;
    ent[515] = (KERNEL_PGT + 4096 * 5) | 0x7;

    /* PD */
    for ( i = 0; i < 1024; i++ ) {
        ent[2048 + i] = 0;
    }

    return 0;
}

/*
 * Remap the virtual memory space into the page table
 */
int
arch_vmem_remap(struct vmem_space *vmem, u64 vaddr, u64 paddr)
{
    int pml4;
    int pdpt;
    int pd;
    struct arch_vmem_space *arch;
    u64 *ent;

    /* Resolve the offset in each table */
    pml4 = (vaddr >> 39);
    pdpt = (vaddr >> 30) & 0x1ff;
    pd = (vaddr >> 21) & 0x1ff;

    /* Get the architecture specific table */
    arch = vmem->arch;

    /* Page table */
    if ( NULL == arch->pgtroot ) {
        /* Allocate PML4 */
        arch->pgtroot = kmalloc(sizeof(struct arch_page_entry));
        if ( NULL == arch->pgtroot ) {
            return -1;
        }
    }

    if ( !((u64)arch->pgtroot & 0x1) ) {
        /* Not present */
        return -1;
    }

    /* PML4 */
    ent = arch->pgtroot->entries[pml4];

    /* Resolve the page directory pointer table */
    if ( !((u64)ent & 0x1) ) {
        /* Not present */
        return -1;
    }

    return 0;
}

/*
 * Remap virtual memory space in the page table
 */
int
vmem_remap(struct vmem_space *vmem, u64 vaddr, u64 paddr, int flag)
{
    int pml4;
    int pdpt;
    int pd;
    u64 *ent;

    pml4 = (vaddr >> 39);
    pdpt = (vaddr >> 30) & 0x1ff;
    pd = (vaddr >> 21) & 0x1ff;

    /* PML4 */
    ent = ((struct arch_vmem_space *)vmem->arch)->pgt;
    if ( !(ent[pml4] & 0x1) ) {
        /* Not present */
        return -1;
    }
    /* PDPT */
    /* FIXME: This is the physical address, but must be virtual address */
    ent = (u64 *)(ent[pml4] & 0xfffffffffffff000ULL);
    if ( 0x1 != (ent[pdpt] & 0x81) ) {
        /* Not present, or 1-Gbyte page */
        return -1;
    }
    /* PD */
    ent = (u64 *)(ent[pdpt] & 0xfffffffffffff000ULL);
    if ( 0x01 == (ent[pd] & 0x81) ) {
        /* Present, and 4-Kbyte page */
        return -1;
    }

    /* Update the entry */
    if ( flag ) {
        ent[pd] = (paddr & 0xffffffffffe00000ULL) | 0x087;
    } else {
        ent[pd] = (paddr & 0xffffffffffe00000ULL) | 0x000;
    }

    /* Invalidate the TLB cache for this entry */
    invlpg((void *)(vaddr & 0xffffffffffe00000ULL));

    return 0;
}

/*
 * Resolve the physical address
 */
u64
vmem_paddr(struct vmem_space *vmem, u64 vaddr)
{
    int pml4;
    int pdpt;
    int pd;
    u64 *ent;

    pml4 = (vaddr >> 39);
    pdpt = (vaddr >> 30) & 0x1ff;
    pd = (vaddr >> 21) & 0x1ff;

    /* PML4 */
    ent = ((struct arch_vmem_space *)vmem->arch)->pgt;
    if ( !(ent[pml4] & 0x1) ) {
        /* Not present */
        return -1;
    }
    /* PDPT */
    /* FIXME: This is the physical address, but must be virtual address */
    ent = (u64 *)(ent[pml4] & 0xfffffffffffff000ULL);
    if ( 0x1 != (ent[pdpt] & 0x81) ) {
        /* Not present, or 1-Gbyte page */
        return -1;
    }
    /* PD */
    ent = (u64 *)(ent[pdpt] & 0xfffffffffffff000ULL);
    if ( 0x81 != (ent[pd] & 0x81) ) {
        /* Not present, or 4-Kbyte page */
        return -1;
    }

    return (ent[pd] & 0xffffffffffe00000ULL) | (vaddr & 0x1fffffULL);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
