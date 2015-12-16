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

extern struct kmem *g_kmem;

#define KMEM_LOW_P2V(a)     (a)

#define KMEM_DIR_RW(a)          ((a) | 0x007ULL)
#define KMEM_PG_RW(a)           ((a) | 0x083ULL)
#define KMEM_PG_GRW(a)          ((a) | 0x183ULL)
#define KMEM_IS_PAGE(a)         ((a) & 0x080ULL)
#define KMEM_IS_PRESENT(a)      ((a) & 0x001ULL)
#define KMEM_PT(a)              (u64 *)((a) & 0x7ffffffffffffe00ULL)

/* Type of memory area */
#define BSE_USABLE              1
#define BSE_RESERVED            2
#define BSE_ACPI_RECLAIMABLE    3
#define BSE_ACPI_NVS            4
#define BSE_BAD                 5

/*
 * Prototype declarations
 */
static struct kmem * _kmem_init(struct kstring *);
static int _kmem_pgt_init(struct arch_kmem **, u64 *);
static void * _kmem_page_alloc(struct kmem *);
static void _kmem_page_free(struct kmem *, void *);
static struct vmem_space * _kmem_vmem_space_create(void *, u64, u64 *);
static int _kmem_vmem_space_pgt_reflect(struct kmem *);
static int _kmem_vmem_map(struct kmem *, u64, u64, int);
static int
_pmem_init_stage1(struct bootinfo *, struct acpi *, struct kstring *,
                  struct kstring *, struct kstring *);
static int
_pmem_init_stage2(struct kmem *, struct bootinfo *, struct kstring *,
                  struct kstring *, struct kstring *);
static int _pmem_buddy_init(struct pmem *);
static int _pmem_buddy_order(struct pmem *, size_t);
static u64 _resolve_phys_mem_size(struct bootinfo *);
static void * _find_pmem_region(struct bootinfo *, u64 );
static __inline__ int _pmem_page_zone(void *, int);
static void _enable_page_global(void);
static void _disable_page_global(void);


/*
 * Initialize physical memory
 *
 * SYNOPSIS
 *      int
 *      arch_memory_init(struct bootinfo *bi, struct acpi *acpi);
 *
 * DESCRIPTION
 *      The arch_memory_init() function initializes the physical memory manager
 *      with the memory map information bi inherited from the boot monitor.
 *      The third argument acpi is used to determine the proximity domain of the
 *      memory spaces.
 *
 * RETURN VALUES
 *      If successful, the arch_memory_init() function returns the value of 0.
 *      It returns the value of -1 on failure.
 */
int
arch_memory_init(struct bootinfo *bi, struct acpi *acpi)
{
    struct kmem *kmem;
    struct kstring region;
    struct kstring pmem;
    struct kstring pmem_pages;
    int ret;

    /* Stage 1: Initialize the physical memory with the page table of linear
       addressing.  This allocates the data structure for the physical memory
       manager, and also stores the physical page zone in each page data
       structure. */

    /* Allocate physical memory management data structure */
    ret = _pmem_init_stage1(bi, acpi, &region, &pmem, &pmem_pages);
    if ( ret < 0 ) {
        return -1;
    }

    /* Stage 2: Setup the kernel page table, and initialize the kernel memory
       and physical memory with this page table. */

    /* Initialize the kernel memory management data structure */
    kmem = _kmem_init(&region);
    if ( NULL == kmem ) {
        return -1;
    }
    g_kmem = kmem;

    /* Initialize the physical pages */
    ret = _pmem_init_stage2(kmem, bi, &region, &pmem, &pmem_pages);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Allocate physical memory management data structure
 *
 * SYNOPSIS
 *      static int
 *      _pmem_init_stage1(struct bootinfo *bi, void **base, u64 *pmsz);
 *
 * DESCRIPTION
 *      The _pmem_init_stage1() function allocates a space for the physical
 *      memory manager from the memory map information bi inherited from the
 *      boot monitor.  The base address and the size of the allocated memory
 *      space are returned through the second and third arguments, base and
 *      pmsz.
 *
 * RETURN VALUES
 *      If successful, the _pmem_init_stage1() function returns the value of 0.
 *      It returns the value of -1 on failure.
 */
static int
_pmem_init_stage1(struct bootinfo *bi, struct acpi *acpi,
                  struct kstring *region, struct kstring *pmem,
                  struct kstring *pmem_pages)
{
    u64 sz;
    u64 npg;
    void *base;
    u64 pmsz;
    u64 i;
    u64 a;
    u64 b;
    u64 pg;
    struct pmem *pm;
    struct pmem_page *pgs;
    struct bootinfo_sysaddrmap_entry *bse;
    u64 pxbase;
    u64 pxlen;
    int prox;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return -1;
    }

    /* Obtain memory (space) size from the system address map */
    sz = _resolve_phys_mem_size(bi);

    /* Calculate the number of pages from the upper-bound of the memory space */
    npg = DIV_CEIL(sz, PAGESIZE);

    /* Calculate the size required by the pmem and pmem_page structures */
    pmsz = sizeof(struct pmem) + npg * sizeof(struct pmem_page);

    /* Fine the available region for the pmem data structure */
    base = _find_pmem_region(bi, pmsz);
    /* Could not find available pages for the management structure */
    if ( NULL == base ) {
        return -1;
    }

    /* Return the memory space */
    region->base = base;
    region->sz = pmsz;
    pmem_pages->base = base;
    pmem_pages->sz = npg * sizeof(struct pmem_page);
    pmem->base = base + pmem_pages->sz;
    pmem->sz = sizeof(struct pmem);

    /* Initialize the pmem data structure */
    pm = (struct pmem *)pmem->base;
    kmemset(pm, 0, sizeof(struct pmem));
    pm->nr = npg;
    pm->pages = NULL;

    /* Initialize the pages with linear addressing page table */
    pgs = (struct pmem_page *)pmem_pages->base;
    kmemset(pgs, 0, sizeof(struct pmem_page) * npg);
    for ( i = 0; i < npg; i++ ) {
        /* Initialize this as a page in the UMA zone */
        pgs[i].zone = PMEM_ZONE_UMA;
        pgs[i].flags = 0;
        pgs[i].order = PMEM_INVAL_BUDDY_ORDER;
        pgs[i].next = PMEM_INVAL_INDEX;
    }

    /* Mark as used for the low memory */
    for ( i = 0; i < DIV_CEIL(PMEM_LBOUND, PAGESIZE); i++ ) {
        pgs[i].flags |= PMEM_USED;
    }

    /* Mark as used for the pmem pages */
    for ( i = 0; i < DIV_CEIL(region->sz, PAGESIZE); i++ ) {
        pg = DIV_FLOOR((u64)region->base, PAGESIZE) + i;
        pgs[pg].flags |= PMEM_USED;
    }

    /* Mark the usable region */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( BSE_USABLE == bse->type ) {
            a = DIV_CEIL(bse->base, PAGESIZE);
            b = DIV_FLOOR(bse->base + bse->len, PAGESIZE);
            for ( pg = a; pg < b; pg++ ) {
                pgs[pg].flags |= PMEM_USABLE;
            }
        }
    }

    /* Resolve the zone of each page */
    if ( acpi_is_numa(acpi) ) {
        /* NUMA */
        prox = -1;
        for ( i = 0; i < npg; i++ ) {
            if ( prox < 0 || !(PAGE_ADDR(i) >= pxbase
                               && PAGE_ADDR(i + 1) <= pxbase + pxlen) ) {
                /* Resolve the proximity domain of the page */
                prox = acpi_memory_prox_domain(acpi, PAGE_ADDR(i), &pxbase,
                                               &pxlen);
                if ( prox < 0 || !(PAGE_ADDR(i) >= pxbase
                                   && PAGE_ADDR(i + 1) <= pxbase + pxlen) ) {
                    /* No proximity domain; meaning unusable page. */
                    continue;
                }
            }
            pgs[i].zone = _pmem_page_zone((void *)PAGE_ADDR(i), prox);
        }
    } else {
        /* UMA */
        for ( i = 0; i < npg; i++ ) {
            pgs[i].zone = _pmem_page_zone((void *)PAGE_ADDR(i), -1);
        }
    }

    return 0;
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
 * Find the memory region (page-aligned) for the pmem data structure
 */
static void *
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

    return (void *)addr;
}

/* Initialize virtual memory space for kernel */
static struct kmem *
_kmem_init(struct kstring *region)
{
    u64 i;
    u64 off;
    struct arch_kmem *akmem;
    struct kmem *kmem;
    struct vmem_space *space;
    struct vmem_region *reg;
    struct kmem_free_page *fpg;
    int ret;

    /* Reset the offset to KMEM_BASE for the memory arrangement */
    off = 0;

    /* Prepare the minimum page table */
    ret = _kmem_pgt_init(&akmem, &off);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Kernel memory space */
    kmem = (struct kmem *)KMEM_LOW_P2V(KMEM_BASE + off);
    off += sizeof(struct kmem);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(kmem, 0, sizeof(struct kmem));

    /* Set the architecture-specific kernel memory management data structure */
    kmem->arch = akmem;

    /* Create virtual memory space for kernel memory */
    space = _kmem_vmem_space_create(region->base, region->sz, &off);
    if ( NULL == space ) {
        return NULL;
    }

    /* Set the virtual memory space to the kmem data structure */
    kmem->space = space;

    /* Add the remaining pages to the free page list */
    kmem->free_pgs = NULL;
    for ( i = DIV_CEIL(off, PAGESIZE);
          i < DIV_FLOOR(KMEM_MAX_SIZE, PAGESIZE); i++ ) {
        /* Prepend a page */
        fpg = (struct kmem_free_page *)KMEM_LOW_P2V(KMEM_BASE + PAGE_ADDR(i));
        fpg->paddr = (void *)(KMEM_BASE + PAGE_ADDR(i));
        fpg->next = kmem->free_pgs;
        kmem->free_pgs = fpg;
    }

    /* Initialize slab */
    for ( i = 0; i < KMEM_SLAB_ORDER; i++ ) {
        kmem->slab.gslabs[i].partial = NULL;
        kmem->slab.gslabs[i].full = NULL;
        kmem->slab.gslabs[i].free = NULL;
    }

    /* Reflect the regions to the page table */
    ret = _kmem_vmem_space_pgt_reflect(kmem);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Create buddy system for kernel memory */
    reg = space->first_region;
    while ( NULL != reg ) {
        vmem_buddy_init(reg);
        reg = reg->next;
    }

    return kmem;
}

/*
 * Initialize virtual memory space for kernel (minimal initialization)
 *
 * SYNOPSIS
 *      static int
 *      _kmem_pgt_init(struct arch_kmem **akmem, u64 *off);
 *
 * DESCRIPTION
 *      The _kmem_pgt_init() function initializes the page table for the kernel
 *      memory.  It creates the mapping entries from 0 to 4 GiB memory space of
 *      virtual memory with 2 MiB paging, and enables the low address space
 *      (0-32 MiB).
 *
 * RETURN VALUES
 *      If successful, the _kmem_pgt_init() function returns the value of 0.  It
 *      returns the value of -1 on failure.
 */
static int
_kmem_pgt_init(struct arch_kmem **akmem, u64 *off)
{
    struct arch_kmem stkmem;
    int i;
    u64 *ptr;
    int nspg;
    int pgtsz;

    /* Architecture-specific kernel memory management  */
    *akmem = (struct arch_kmem *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct arch_kmem);
    if ( *off > KMEM_MAX_SIZE ) {
        return -1;
    }

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Page table: Allocate 10 blocks (6 for keeping physical address, and 4 for
       keeping virtual address) */
    pgtsz = PAGESIZE * 10;
    ptr = (u64 *)(KMEM_BASE + *off);
    *off += pgtsz;
    if ( *off > KMEM_MAX_SIZE ) {
        return -1;
    }
    kmemset(ptr, 0, pgtsz);

    /* Set physical addresses to page directories */
    stkmem.pml4 = ptr;
    stkmem.pdpt = ptr + 512;
    for ( i = 0; i < 4; i++ ) {
        stkmem.pd[i] = ptr + 1024 + 512 * i;
    }
    /* Page directories with virtual address */
    for ( i = 0; i < 4; i++ ) {
        stkmem.vpd[i] = (u64 *)KMEM_LOW_P2V(ptr + 3072 + 512 * i);
    }

    /* Setup physical page table */
    stkmem.pml4[0] = KMEM_DIR_RW((u64)stkmem.pdpt);
    for ( i = 0; i < 4; i++ ) {
        stkmem.pdpt[i] = KMEM_DIR_RW((u64)stkmem.pd[i]);
    }
    /* Superpage for the region from 0-32 MiB */
    nspg = DIV_CEIL(PMEM_LBOUND, SUPERPAGESIZE);
    if ( nspg > 512 ) {
        /* The low memory address space for kernel memory is too large. */
        return -1;
    }
    /* Page directories for 0-32 MiB; must be consistent with KMEM_LOW_P2V */
    for ( i = 0; i < nspg; i++ ) {
        stkmem.pd[0][i] = KMEM_PG_GRW(SUPERPAGE_ADDR(i));
    }
    /* Page directories from 32 MiB to 1 GiB */
    for ( ; i < 512; i++ ) {
        stkmem.pd[0][i] = 0;
    }

    /* Disable the global page feature */
    _disable_page_global();

    /* Set the constructured page table */
    set_cr3(stkmem.pml4);

    /* Enable the global page feature */
    _enable_page_global();

    /* Setup virtual page table */
    for ( i = 0; i < 4; i++ ) {
        kmemset(stkmem.vpd[i], 0, PAGESIZE);
    }
    for ( i = 0; i < nspg; i++ ) {
        stkmem.vpd[0][i] = KMEM_PG_GRW(KMEM_LOW_P2V(SUPERPAGE_ADDR(i)));
    }

    /* Copy */
    kmemcpy(*akmem, &stkmem, sizeof(struct arch_kmem));

    return 0;
}

/*
 * Create virtual memory space for the kernel memory
 */
static struct vmem_space *
_kmem_vmem_space_create(void *pmbase, u64 pmsz, u64 *off)
{
    u64 i;
    struct vmem_space *space;
    struct vmem_region *reg_low;
    struct vmem_region *reg_pmem;
    struct vmem_region *reg_kernel;
    struct vmem_region *reg_spec;
    struct vmem_page *pgs_low;
    struct vmem_page *pgs_pmem;
    struct vmem_page *pgs_kernel;
    struct vmem_page *pgs_spec;

    /* Virtual memory space */
    space = (struct vmem_space *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_space);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(space, 0, sizeof(struct vmem_space));

    /* Low address space (below 32 MiB): Note that this operating system has
       ``two'' kernel regions shared unlike other UNIX-like systems, regions
       from 0 to 1 GiB and from 3 to 4 GiB.  The first region could be removed
       by relocating the kernel, but this version of our operating system does
       not do it. */
    reg_low = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_low, 0, sizeof(struct vmem_region));
    reg_low->start = (void *)0;
    reg_low->len = CEIL(PMEM_LBOUND, SUPERPAGESIZE);
    reg_low->total_pgs = DIV_CEIL(reg_low->len, PAGESIZE);
    reg_low->used_pgs = DIV_CEIL(reg_low->len, PAGESIZE);

    /* Physical pages: This region is not placed at the kernel region because
       this is not directly referred from user-land processes (e.g., through
       system calls). */
    reg_pmem = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_pmem, 0, sizeof(struct vmem_region));
    reg_pmem->start = (void *)KMEM_REGION_PMEM_BASE;
    reg_pmem->len = CEIL(pmsz, PAGESIZE);
    reg_pmem->total_pgs = DIV_CEIL(pmsz, PAGESIZE);
    reg_pmem->used_pgs = DIV_CEIL(pmsz, PAGESIZE);

    /* Kernel address space (3-3.64 GiB) */
    reg_kernel = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_kernel, 0, sizeof(struct vmem_region));
    reg_kernel->start = (void *)KMEM_REGION_KERNEL_BASE;
    reg_kernel->len = KMEM_REGION_KERNEL_SIZE;
    reg_kernel->total_pgs = KMEM_REGION_KERNEL_SIZE / PAGESIZE;
    reg_kernel->used_pgs = 0;

    /* Kernel address space for special use (3.64-4 GiB) */
    reg_spec = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_spec, 0, sizeof(struct vmem_region));
    reg_spec->start = (void *)KMEM_REGION_SPEC_BASE;
    reg_spec->len = KMEM_REGION_SPEC_SIZE;
    reg_spec->total_pgs = KMEM_REGION_SPEC_SIZE / PAGESIZE;
    reg_spec->used_pgs = KMEM_REGION_SPEC_SIZE / PAGESIZE;

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Initialize the pages in this region.  Note that the (super)pages in this
       region have already been configured in the kernel's page table. */
    pgs_low = (struct vmem_page *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_page) * DIV_CEIL(reg_low->len, PAGESIZE);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_low, 0, sizeof(struct vmem_page)
            * DIV_CEIL(reg_low->len, PAGESIZE));
    for ( i = 0; i < DIV_CEIL(reg_low->len, PAGESIZE); i++ ) {
        pgs_low[i].addr = KMEM_LOW_P2V(PAGE_ADDR(i) + (reg_t)reg_low->start);
        pgs_low[i].order = VMEM_INVAL_BUDDY_ORDER;
        pgs_low[i].flags = VMEM_USED | VMEM_GLOBAL | VMEM_SUPERPAGE;
        pgs_low[i].region = reg_low;
        pgs_low[i].next = NULL;
    }

    /* Prepare page data structures for physical memory management region */
    pgs_pmem = (struct vmem_page *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_page) * DIV_CEIL(reg_pmem->len, PAGESIZE);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_pmem, 0, sizeof(struct vmem_page)
            * DIV_CEIL(reg_pmem->len, PAGESIZE));
    for ( i = 0; i < DIV_CEIL(reg_pmem->len, PAGESIZE); i++ ) {
        pgs_pmem[i].addr = (reg_t)pmbase + PAGE_ADDR(i);
        pgs_pmem[i].order = VMEM_INVAL_BUDDY_ORDER;
        pgs_pmem[i].flags = VMEM_USABLE | VMEM_USED;
        pgs_pmem[i].region = reg_pmem;
        pgs_pmem[i].next = NULL;
    }

    /* Prepare page data structures for kernel memory region */
    pgs_kernel = (struct vmem_page *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_page) * DIV_CEIL(reg_kernel->len, PAGESIZE);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_kernel, 0, sizeof(struct vmem_page)
            * DIV_CEIL(reg_kernel->len, PAGESIZE));
    for ( i = 0; i < DIV_CEIL(reg_kernel->len, PAGESIZE); i++ ) {
        pgs_kernel[i].addr = (reg_t)reg_kernel->start + PAGE_ADDR(i);
        pgs_kernel[i].order = VMEM_INVAL_BUDDY_ORDER;
        pgs_kernel[i].flags = VMEM_USABLE | VMEM_GLOBAL;
        pgs_kernel[i].region = reg_kernel;
        pgs_kernel[i].next = NULL;
    }

    /* Prepare page data structures for kernel memory region for special use */
    pgs_spec = (struct vmem_page *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_page) * DIV_CEIL(reg_spec->len, PAGESIZE);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(pgs_spec, 0, sizeof(struct vmem_page)
            * DIV_CEIL(reg_spec->len, PAGESIZE));
    for ( i = 0; i < DIV_CEIL(reg_spec->len, PAGESIZE); i++ ) {
        /* Linear addressing */
        pgs_spec[i].addr = (reg_t)reg_spec->start + PAGE_ADDR(i);
        pgs_spec[i].order = VMEM_INVAL_BUDDY_ORDER;
        pgs_spec[i].flags = VMEM_USABLE | VMEM_USED | VMEM_GLOBAL;
        pgs_spec[i].region = reg_spec;
        pgs_spec[i].next = NULL;
    }

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Set the allocated pages to each region */
    reg_low->pages = pgs_low;
    reg_pmem->pages = pgs_pmem;
    reg_kernel->pages = pgs_kernel;
    reg_spec->pages = pgs_spec;

    /* Create the chain of regions */
    space->first_region = reg_low;
    reg_low->next = reg_pmem;
    reg_pmem->next = reg_kernel;
    reg_kernel->next = reg_spec;
    reg_spec->next = NULL;

    return space;
}

/*
 * Reflect the virtual memory regions to the page table
 */
static int
_kmem_vmem_space_pgt_reflect(struct kmem *kmem)
{
    struct vmem_region *reg;
    u64 i;
    int ret;

    reg = kmem->space->first_region;
    while ( NULL != reg ) {
        for ( i = 0; i < reg->len / PAGESIZE; i++ ) {
            /* Register a page */
            if ( (VMEM_USABLE & reg->pages[i].flags)
                 && (VMEM_USED & reg->pages[i].flags) ) {
                /* Usable and used, then map the page */
                ret = _kmem_vmem_map(kmem, PAGE_INDEX(reg->start) + i,
                                     reg->pages[i].addr, reg->pages[i].flags);
                if ( ret < 0 ) {
                    return -1;
                }
            }

        }
        /* Next region */
        reg = reg->next;
    }

    return 0;
}

/*
 * Get a free page
 */
static void *
_kmem_page_alloc(struct kmem *kmem)
{
    struct kmem_free_page *fpg;

    /* Get the head of the free page list */
    fpg = kmem->free_pgs;
    if ( NULL == fpg ) {
        /* No free page found */
        return NULL;
    }
    kmem->free_pgs = fpg->next;

    return fpg->paddr;
}

/*
 * Release a page to the free list
 */
static void
_kmem_page_free(struct kmem *kmem, void *paddr)
{
    struct kmem_free_page *fpg;

    /* Resolve the virtual address */
    fpg = KMEM_LOW_P2V(paddr);
    fpg->paddr = paddr;
    /* Return to the list */
    fpg->next = kmem->free_pgs;
    kmem->free_pgs = fpg;
}

/*
 * Map a virtual page to a physical page
 */
static int
_kmem_vmem_map(struct kmem *kmem, u64 vpg, u64 paddr, int flags)
{
    struct arch_kmem *akmem;
    int idxpd;
    int idxp;
    int idx;
    u64 *pt;
    u64 *vpt;

    /* Check the physical address argument */
    if ( 0 != (paddr % PAGESIZE) ) {
        /* Invalid physical address */
        return -1;
    }

    /* Get the architecture-specific kernel memory manager */
    akmem = (struct arch_kmem *)kmem->arch;

    /* Index to page directory */
    idxpd = (vpg >> 18);
    if ( idxpd >= 4 ) {
        return -1;
    }
    /* Index to page table */
    idxp = (vpg >> 9) & 0x1ff;
    /* Index to page entry */
    idx = vpg & 0x1ffULL;
    if ( !KMEM_IS_PRESENT(akmem->pd[idxpd][idxp])
         || KMEM_IS_PAGE(akmem->pd[idxpd][idxp]) ) {
        /* Not present or 2 MiB page, then create a new page table */
        pt = _kmem_page_alloc(kmem);
        /* Get the virtual address */
        vpt = KMEM_LOW_P2V(pt);
        if ( NULL == pt ) {
            return -1;
        }
        /* Update the entry */
        akmem->pd[idxpd][idxp] = KMEM_DIR_RW((u64)pt);
        akmem->vpd[idxpd][idxp] = KMEM_DIR_RW((u64)vpt);
    } else {
        /* Directory */
        pt = KMEM_PT(akmem->pd[idxpd][idxp]);
        vpt = KMEM_PT(akmem->vpd[idxpd][idxp]);
    }

    /* Remapping */
    if ( flags & VMEM_GLOBAL ) {
        pt[idx] = KMEM_PG_GRW(paddr);
    } else {
        pt[idx] = KMEM_PG_RW(paddr);
    }

    /* Invalidate the page */
    invlpg((void *)PAGE_ADDR(vpg));

    return 0;
}

/*
 * Initialize physical memory
 */
static int
_pmem_init_stage2(struct kmem *kmem, struct bootinfo *bi,
                  struct kstring *region, struct kstring *pmem,
                  struct kstring *pmem_pages)
{
    struct pmem *pm;
    int ret;

    /* Physical memory */
    pm = (struct pmem *)(KMEM_REGION_PMEM_BASE + pmem->base - region->base);
    pm->pages = (struct pmem_page *)(KMEM_REGION_PMEM_BASE + pmem_pages->base
                                     - region->base);

    /* Set physical memory manager */
    kmem->pmem = pm;

    /* Initialize all usable pages with the buddy system */
    ret = _pmem_buddy_init(pm);
    if ( ret < 0 ) {
        return -1;
    }

    return 0;
}

/*
 * Construct buddy system for physical memory
 */
static int
_pmem_buddy_init(struct pmem *pmem)
{
    u64 i;
    u64 j;
    int o;
    int z;

    /* Reset it */
    for ( i = 0; i < PMEM_NUM_ZONES; i++ ) {
        for ( j = 0; j <= PMEM_MAX_BUDDY_ORDER; j++ ) {
            pmem->zones[i].buddy.heads[j] = PMEM_INVAL_INDEX;
        }
    }

    for ( i = 0; i < pmem->nr; i += (1ULL << o) ) {
        /* Find the maximum contiguous usable pages fitting to the alignment of
           the buddy system */
        o = _pmem_buddy_order(pmem, i);
        if ( o < 0 ) {
            /* This page is not usable, then skip it. */
            o = 0;
        } else {
            /* This page is usable, then set the order to all the pages. */
            for ( j = 0; j < (1ULL << o); j++ ) {
                pmem->pages[i + j].order = o;
            }
            /* Get the zone */
            z = pmem->pages[i].zone;

            /* Add this to the buddy system at the order of o */
            pmem->pages[i].next = pmem->zones[z].buddy.heads[o];
            pmem->zones[z].buddy.heads[o] = i;
        }
    }

    return 0;
}

/*
 * Count the physical memory order for buddy system
 */
static int
_pmem_buddy_order(struct pmem *pmem, size_t pg)
{
    int o;
    size_t i;
    int zone;

    /* Check the pg argument within the range of the physical memory space */
    if ( pg >= pmem->nr ) {
        return -1;
    }

    /* Get the zone of the first page */
    zone = pmem->pages[pg].zone;

    /* Check the order for contiguous usable pages */
    for ( o = 0; o <= PMEM_MAX_BUDDY_ORDER; o++ ) {
        for ( i = pg; i < pg + (1ULL << o); i++ ) {
            if ( !PMEM_IS_FREE(&pmem->pages[i])
                 || zone != pmem->pages[i].zone ) {
                /* It contains an unusable page or a page of different zone,
                   then return the current order minus 1, immediately. */
                return o - 1;
            }
        }
        /* Test whether the next order is feasible; feasible if it is properly
           aligned and the pages are within the range of this zone. */
        if ( 0 != (pg & (1ULL << o)) || pg + (1ULL << (o + 1)) > pmem->nr ) {
            /* Infeasible, then return the current order immediately */
            return o;
        }
    }

    /* Return the maximum order */
    return PMEM_MAX_BUDDY_ORDER;
}

/*
 * Resolve the zone of the page
 */
static __inline__ int
_pmem_page_zone(void *page, int prox)
{
    if ( (u64)page < 0x1000000ULL ) {
        /* DMA */
        return PMEM_ZONE_DMA;
    } else if ( (u64)page < 0x100000000ULL ) {
        /* Low address memory space */
        return PMEM_ZONE_LOWMEM;
    } else {
        /* High address memory space */
        if ( prox >= 0 ) {
            return PMEM_ZONE_NUMA(prox);
        } else {
            return PMEM_ZONE_UMA;
        }
    }
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
 * Initialize the architecture-specific virtual memory data structure
 */
int
vmem_arch_init(struct vmem_space *vmem)
{
    struct arch_vmem_space *arch;

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

    return -1;
}

#if 1
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
#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
