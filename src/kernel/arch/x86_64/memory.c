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
#include "arch.h"
#include "memory.h"
#include "../../kernel.h"

extern struct kmem *g_kmem;

#define KMEM_LOW_P2V(a)         ((u64)(a))

#define KMEM_DIR_RW(a)          ((a) | 0x007ULL)
#define KMEM_PG_RW(a)           ((a) | 0x083ULL)
#define KMEM_PG_GRW(a)          ((a) | 0x183ULL)
#define VMEM_DIR_RW(a)          ((a) | 0x007ULL)
#define VMEM_PG_RW(a)           ((a) | 0x087ULL)
#define VMEM_PG_GRW(a)          ((a) | 0x187ULL)
#define VMEM_IS_PAGE(a)         ((a) & 0x080ULL)
#define VMEM_IS_PRESENT(a)      ((a) & 0x001ULL)
#define VMEM_PT(a)              (u64 *)((a) & 0x7ffffffffffff000ULL)
#define VMEM_PDPG(a)            (void *)((a) & 0x7fffffffffe00000ULL)

/* Type of memory area */
#define BSE_USABLE              1
#define BSE_RESERVED            2
#define BSE_ACPI_RECLAIMABLE    3
#define BSE_ACPI_NVS            4
#define BSE_BAD                 5

/*
 * Prototype declarations of static functions
 */
static struct kmem * _kmem_init(struct kstring *);
static int _kmem_pgt_init(struct arch_vmem_space **, u64 *);
static void * _kmem_mm_page_alloc(struct kmem *);
static int _kmem_create_mm_region(struct kmem *, void *);
static void _kmem_mm_page_free(struct kmem *, void *);
static struct vmem_space * _kmem_vmem_space_create(void *, u64, u64 *);
static int _kmem_vmem_space_pgt_reflect(struct kmem *);
static int _kmem_vmem_map(struct kmem *, u64, u64, int);
static int _kmem_superpage_map(struct kmem *, u64, u64, int);
static int
_pmem_init_stage1(struct bootinfo *, struct acpi *, struct kstring *,
                  struct kstring *, struct kstring *);
static int
_pmem_init_stage2(struct kmem *, struct kstring *, struct kstring *,
                  struct kstring *);
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
    ret = _pmem_init_stage2(kmem, &region, &pmem, &pmem_pages);
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

    /* Initialize the pages with the current linear addressing page table */
    pgs = (struct pmem_page *)pmem_pages->base;
    kmemset(pgs, 0, sizeof(struct pmem_page) * npg);
    for ( i = 0; i < npg; i++ ) {
        /* Initialize this as a page in the LOWMEM zone */
        pgs[i].zone = PMEM_ZONE_LOWMEM;
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
    struct arch_vmem_space *avmem;
    struct kmem *kmem;
    struct vmem_space *space;
    struct vmem_region *reg;
    struct kmem_mm_page *mmpg;
    int ret;

    /* Reset the offset to KMEM_BASE for the memory arrangement */
    off = 0;

    /* Prepare the minimum page table */
    ret = _kmem_pgt_init(&avmem, &off);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Prepare the kernel memory space */
    kmem = (struct kmem *)KMEM_LOW_P2V(KMEM_BASE + off);
    off += sizeof(struct kmem);
    if ( off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(kmem, 0, sizeof(struct kmem));

    /* Create virtual memory space for kernel memory */
    space = _kmem_vmem_space_create(region->base, region->sz, &off);
    if ( NULL == space ) {
        return NULL;
    }

    /* Set the architecture-specific kernel memory management data structure */
    space->arch = avmem;

    /* Set the virtual memory space to the kmem data structure */
    kmem->space = space;

    /* Add the remaining pages to the free page list */
    kmem->pool.mm_pgs = NULL;
    for ( i = DIV_CEIL(off, PAGESIZE);
          i < DIV_FLOOR(KMEM_MAX_SIZE, PAGESIZE); i++ ) {
        /* Prepend a page */
        mmpg = (struct kmem_mm_page *)KMEM_LOW_P2V(KMEM_BASE + PAGE_ADDR(i));
        mmpg->next = kmem->pool.mm_pgs;
        kmem->pool.mm_pgs = mmpg;
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
 *      _kmem_pgt_init(struct arch_vmem_space **avmem, u64 *off);
 *
 * DESCRIPTION
 *      The _kmem_pgt_init() function initializes the page table for the kernel
 *      memory.  It creates the mapping entries from 0 to 4 GiB memory space of
 *      virtual memory with 2 MiB paging, and enables the low address space
 *      (0-32 MiB).  (FIXME: Temporarily use 5 GiB space to support hundreds
 *      gigabytes memory)
 *
 * RETURN VALUES
 *      If successful, the _kmem_pgt_init() function returns the value of 0.  It
 *      returns the value of -1 on failure.
 */
#define KMEM_VMEM_NPD   6
static int
_kmem_pgt_init(struct arch_vmem_space **avmem, u64 *off)
{
    void *pgt;
    u64 *parr[VMEM_NENT(KMEM_VMEM_NPD)];
    u64 *vls[KMEM_VMEM_NPD];
    int i;
    u64 *ptr;
    int nspg;
    int pgtsz;

    /* Ensure KMEM_VMEM_NPD <= 512 */
    if ( KMEM_VMEM_NPD > 512 ) {
        return -1;
    }

    /* Architecture-specific kernel memory management  */
    *avmem = (struct arch_vmem_space *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct arch_vmem_space)
        + sizeof(u64 *) * (VMEM_NENT(KMEM_VMEM_NPD) + KMEM_VMEM_NPD);
    if ( *off > KMEM_MAX_SIZE ) {
        return -1;
    }

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Page table: Allocate 14 blocks (8 for keeping physical address, and 6 for
       keeping virtual address) */
    pgtsz = PAGESIZE * (VMEM_NENT(KMEM_VMEM_NPD) + KMEM_VMEM_NPD);
    ptr = (u64 *)(KMEM_BASE + *off);
    *off += pgtsz;
    if ( *off > KMEM_MAX_SIZE ) {
        return -1;
    }
    kmemset(ptr, 0, pgtsz);

    /* Set physical addresses to page directories */
    pgt = ptr;
    VMEM_PML4(parr) = ptr;
    VMEM_PDPT(parr, 0) = ptr + 512;
    for ( i = 0; i < KMEM_VMEM_NPD; i++ ) {
        VMEM_PD(parr, i) = ptr + 1024 + 512 * i;
    }
    /* Page directories with virtual address */
    for ( i = 0; i < KMEM_VMEM_NPD; i++ ) {
        vls[i] = (u64 *)KMEM_LOW_P2V(ptr + 512 * VMEM_NENT(KMEM_VMEM_NPD)
                                     + 512 * i);
    }

    /* Setup physical page table */
    VMEM_PML4(parr)[0] = KMEM_DIR_RW((u64)VMEM_PDPT(parr, 0));
    for ( i = 0; i < KMEM_VMEM_NPD; i++ ) {
        VMEM_PDPT(parr, 0)[i] = KMEM_DIR_RW((u64)VMEM_PD(parr, i));
    }
    /* Superpage for the region from 0-32 MiB */
    nspg = DIV_CEIL(PMEM_LBOUND, SUPERPAGESIZE);
    if ( nspg > 512 ) {
        /* The low memory address space for kernel memory is too large. */
        return -1;
    }
    /* Page directories for 0-32 MiB; must be consistent with KMEM_LOW_P2V */
    for ( i = 0; i < nspg; i++ ) {
        VMEM_PD(parr, 0)[i] = KMEM_PG_GRW(SUPERPAGE_ADDR(i));
    }
    /* Page directories from 32 MiB to 1 GiB */
    for ( ; i < 512; i++ ) {
        VMEM_PD(parr, 0)[i] = 0;
    }

    /* Disable the global page feature */
    _disable_page_global();

    /* Set the constructured page table */
    set_cr3(pgt);

    /* Enable the global page feature */
    _enable_page_global();

    /* Setup virtual page table */
    for ( i = 0; i < KMEM_VMEM_NPD; i++ ) {
        kmemset(vls[i], 0, PAGESIZE);
    }

    for ( i = 0; i < nspg; i++ ) {
        vls[0][i] = KMEM_PG_GRW(KMEM_LOW_P2V(SUPERPAGE_ADDR(i)));
    }

    /* Set the address */
    (*avmem)->pgt = pgt;
    (*avmem)->nr = KMEM_VMEM_NPD;
    (*avmem)->array = (u64 **)(*avmem + sizeof(int));
    (*avmem)->vls = (u64 **)(*avmem + sizeof(int)
                             + sizeof(u64 *) * VMEM_NENT(KMEM_VMEM_NPD));

    /* Convert to virtual address */
    for ( i = 0; i < VMEM_NENT(KMEM_VMEM_NPD); i++ ) {
        (*avmem)->array[i] = (u64 *)KMEM_LOW_P2V(parr[i]);
    }
    /* Copy */
    kmemcpy((*avmem)->vls, vls, sizeof(u64 *) * KMEM_VMEM_NPD);

    return 0;
}

/*
 * Create virtual memory space for the kernel memory
 */
static struct vmem_space *
_kmem_vmem_space_create(void *pmbase, u64 pmsz, u64 *off)
{
    u64 i;
    size_t n;
    struct vmem_space *space;
    struct vmem_region *reg_low;
    struct vmem_region *reg_pmem;
    struct vmem_region *reg_kernel;
    struct vmem_region *reg_spec;
    struct vmem_superpage *spgs_low;
    struct vmem_superpage *spgs_pmem;
    struct vmem_superpage *spgs_kernel;
    struct vmem_superpage *spgs_spec;

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

    /* Kernel address space (3-3.64 GiB) */
    reg_kernel = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_kernel, 0, sizeof(struct vmem_region));
    reg_kernel->start = (void *)KMEM_REGION_KERNEL_BASE;
    reg_kernel->len = KMEM_REGION_KERNEL_SIZE;

    /* Kernel address space for special use (3.64-4 GiB) */
    reg_spec = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_spec, 0, sizeof(struct vmem_region));
    reg_spec->start = (void *)KMEM_REGION_SPEC_BASE;
    reg_spec->len = KMEM_REGION_SPEC_SIZE;

    /* Physical pages: This region is not placed at the kernel region because
       this is not directly referred from user-land processes (e.g., through
       system calls).  FIXME: This temporarily uses 4-5 GiB region. */
    reg_pmem = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_pmem, 0, sizeof(struct vmem_region));
    reg_pmem->start = (void *)KMEM_REGION_PMEM_BASE;
    reg_pmem->len = CEIL(pmsz, SUPERPAGESIZE);

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Initialize the pages in this region.  Note that the (super)pages in this
       region have already been configured in the kernel's page table. */
    spgs_low = (struct vmem_superpage *)KMEM_LOW_P2V(KMEM_BASE + *off);
    n = DIV_CEIL(reg_low->len, SUPERPAGESIZE);
    *off += sizeof(struct vmem_superpage) * n;
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(spgs_low, 0, sizeof(struct vmem_superpage) * n);
    for ( i = 0; i < n; i++ ) {
        spgs_low[i].u.superpage.addr
            = KMEM_LOW_P2V(SUPERPAGE_ADDR(i) + (reg_t)reg_low->start);
        spgs_low[i].order = VMEM_INVAL_BUDDY_ORDER;
        spgs_low[i].flags = VMEM_USED | VMEM_GLOBAL | VMEM_SUPERPAGE;
        spgs_low[i].region = reg_low;
        spgs_low[i].next = NULL;
    }

    /* Prepare page data structures for kernel memory region */
    spgs_kernel = (struct vmem_superpage *)KMEM_LOW_P2V(KMEM_BASE + *off);
    n = DIV_CEIL(reg_kernel->len, SUPERPAGESIZE);
    *off += sizeof(struct vmem_superpage) * n;
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(spgs_kernel, 0, sizeof(struct vmem_superpage) * n);
    for ( i = 0; i < n; i++ ) {
        spgs_kernel[i].u.superpage.addr
            = (reg_t)reg_kernel->start + SUPERPAGE_ADDR(i);
        spgs_kernel[i].order = VMEM_INVAL_BUDDY_ORDER;
        spgs_kernel[i].flags = VMEM_USABLE | VMEM_GLOBAL | VMEM_SUPERPAGE;
        spgs_kernel[i].region = reg_kernel;
        spgs_kernel[i].next = NULL;
    }

    /* Prepare page data structures for kernel memory region for special use */
    spgs_spec = (struct vmem_superpage *)KMEM_LOW_P2V(KMEM_BASE + *off);
    n = DIV_CEIL(reg_spec->len, SUPERPAGESIZE);
    *off += sizeof(struct vmem_superpage) * n;
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(spgs_spec, 0, sizeof(struct vmem_superpage) * n);
    for ( i = 0; i < n; i++ ) {
        /* Linear addressing */
        spgs_spec[i].u.superpage.addr
            = (reg_t)reg_spec->start + SUPERPAGE_ADDR(i);
        spgs_spec[i].order = VMEM_INVAL_BUDDY_ORDER;
        spgs_spec[i].flags = VMEM_USABLE | VMEM_USED | VMEM_GLOBAL
            | VMEM_SUPERPAGE;
        spgs_spec[i].region = reg_spec;
        spgs_spec[i].next = NULL;
    }

    /* Prepare page data structures for physical memory management region */
    spgs_pmem = (struct vmem_superpage *)KMEM_LOW_P2V(KMEM_BASE + *off);
    n = DIV_CEIL(reg_pmem->len, SUPERPAGESIZE);
    *off += sizeof(struct vmem_superpage) * n;
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(spgs_pmem, 0, sizeof(struct vmem_superpage) * n);
    for ( i = 0; i < n; i++ ) {
        spgs_pmem[i].u.superpage.addr = (reg_t)pmbase + SUPERPAGE_ADDR(i);
        spgs_pmem[i].order = VMEM_INVAL_BUDDY_ORDER;
        spgs_pmem[i].flags = VMEM_USABLE | VMEM_USED | VMEM_GLOBAL
            | VMEM_SUPERPAGE;
        spgs_pmem[i].region = reg_pmem;
        spgs_pmem[i].next = NULL;
    }

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Set the allocated pages to each region */
    reg_low->superpages = spgs_low;
    reg_kernel->superpages = spgs_kernel;
    reg_spec->superpages = spgs_spec;
    reg_pmem->superpages = spgs_pmem;

    /* Create the chain of regions */
    space->first_region = reg_low;
    reg_low->next = reg_kernel;
    reg_kernel->next = reg_spec;
    reg_spec->next = reg_pmem;
    reg_pmem->next = NULL;

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
    u64 j;
    int ret;
    u64 paddr;
    u64 vaddr;
    int flags;

    reg = kmem->space->first_region;
    while ( NULL != reg ) {
        for ( i = 0; i < reg->len / SUPERPAGESIZE; i++ ) {
            if ( !(VMEM_USABLE & reg->superpages[i].flags)
                 || !(VMEM_USED & reg->superpages[i].flags) ) {
                /* Not usable/used, then do nothing to this superpage */
                continue;
            }
            /* Register a superpage */
            if ( VMEM_IS_SUPERPAGE(&reg->superpages[i]) ) {
                /* Superpage */
                paddr = (reg_t)reg->start + SUPERPAGE_ADDR(i);
                vaddr = reg->superpages[i].u.superpage.addr;
                flags = reg->superpages[i].flags;
                ret = _kmem_vmem_map(kmem, paddr, vaddr, flags);
                if ( ret < 0 ) {
                    return -1;
                }
            } else {
                /* Pages */
                for ( j = 0; j < SUPERPAGESIZE / PAGESIZE; j++ ) {
                    paddr = (reg_t)reg->start + SUPERPAGE_ADDR(i)
                        + PAGE_ADDR(j);
                    vaddr = reg->superpages[i].u.page.pages[j].addr;
                    flags = reg->superpages[i].u.page.pages[j].flags;
                    if ( !(VMEM_USABLE & flags) || !(VMEM_USED & flags) ) {
                        /* Not usable/used, then skip this page */
                        continue;
                    }
                    ret = _kmem_vmem_map(kmem, paddr, vaddr, flags);
                    if ( ret < 0 ) {
                        return -1;
                    }
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
_kmem_mm_page_alloc(struct kmem *kmem)
{
    struct kmem_mm_page *mmpg;
    int ret;

    /* Get the head of the free page list */
    mmpg = kmem->pool.mm_pgs;
    if ( NULL == mmpg ) {
        /* No free page found */
        return NULL;
    }
    kmem->pool.mm_pgs = mmpg->next;

    if ( NULL == kmem->pool.mm_pgs ) {
        /* Pages for memory management are empty, then allocate new pages using
           the mmpg page. */
        ret = _kmem_create_mm_region(kmem, mmpg);
        if ( ret < 0 ) {
            /* Failed, then return an error */
            mmpg->next = kmem->pool.mm_pgs;
            kmem->pool.mm_pgs = mmpg;
            return NULL;
        }
    }

    return (void *)mmpg;
}

/*
 * Create a new region for memory management
 */
static int
_kmem_create_mm_region(struct kmem *kmem, void *availpg)
{
    void *paddr;
    struct kmem_mm_page *mmpg;
    struct vmem_region *reg;
    struct vmem_superpage *spg;
    void *vstart;
    size_t sz;
    size_t i;
    int ret;

    /* Allocate for the data structure of new region */
    sz = sizeof(struct vmem_region) + sizeof(struct vmem_superpage);
    if ( sz >= PAGESIZE ) {
        return -1;
    }

    /* Allocate physical page */
    paddr = pmem_alloc_pages(PMEM_ZONE_LOWMEM, bitwidth(SUPERPAGESIZE));
    if ( NULL == paddr ) {
        return -1;
    }

    /* Search available memory space for region allocation */
    vstart = vmem_search_available_region(kmem->space, SUPERPAGESIZE);
    if ( NULL == vstart ) {
        pmem_free_pages(paddr);
        return -1;
    }

    /* Update the page table */
    ret = _kmem_superpage_map(kmem, (u64)vstart, (u64)paddr,
                              VMEM_SUPERPAGE | VMEM_USED | VMEM_USABLE);
    if ( ret < 0 ) {
        pmem_free_pages(paddr);
        return -1;
    }

    /* Prepare the region */
    reg = (struct vmem_region *)availpg;
    spg = (struct vmem_superpage *)(reg + 1);
    kmemset(reg, 0, sizeof(struct vmem_region));
    kmemset(spg, 0, sizeof(struct vmem_superpage));

    /* Prepare the allocated superpage */
    spg->flags = VMEM_SUPERPAGE | VMEM_USED | VMEM_USABLE;
    spg->u.superpage.addr = (reg_t)paddr;
    spg->order = 0;
    spg->region = reg;
    spg->next = NULL;
    spg->prev = NULL;

    /* Prepare the allocated region */
    reg->start = vstart;
    reg->len = SUPERPAGESIZE;
    reg->superpages = spg;

    /* Use all pages in this superpage as memory management pages, and add them
       to the list of free pages in kmem. */
    for ( i = 0; i < SUPERPAGESIZE / PAGESIZE; i++ ) {
        mmpg = vstart + PAGE_ADDR(i);
        /* Prepend the page to the list */
        mmpg->next = kmem->pool.mm_pgs;
        kmem->pool.mm_pgs = mmpg;
    }

    return 0;
}

/*
 * Release a page to the free list
 */
static void
_kmem_mm_page_free(struct kmem *kmem, void *vaddr)
{
    struct kmem_mm_page *mmpg;

    /* Resolve the virtual address */
    mmpg = (struct kmem_mm_page *)vaddr;
    /* Return to the list */
    mmpg->next = kmem->pool.mm_pgs;
    kmem->pool.mm_pgs = mmpg;
}

/*
 * Map a virtual page to a physical page
 */
static int
_kmem_vmem_map(struct kmem *kmem, u64 vaddr, u64 paddr, int flags)
{
    struct arch_vmem_space *avmem;
    int idxpd;
    int idxp;
    int idx;
    u64 *pt;
    u64 *vpt;

    /* Check the flags */
    if ( !(VMEM_USABLE & flags) || !(VMEM_USED & flags) ) {
        /* This page is not usable nor used, then do nothing. */
        return -1;
    }

    /* Get the architecture-specific kernel memory manager */
    avmem = (struct arch_vmem_space *)kmem->space->arch;

    /* Index to page directory */
    idxpd = (vaddr >> 30);
    if ( idxpd >= avmem->nr ) {
        return -1;
    }
    /* Index to page table */
    idxp = (vaddr >> 21) & 0x1ff;
    /* Index to page entry */
    idx = (vaddr >> 12) & 0x1ffULL;

    /* Superpage or page? */
    if ( VMEM_SUPERPAGE & flags ) {
        /* Superpage */
        /* Check the physical address argument */
        if ( 0 != (paddr % SUPERPAGESIZE) || 0 != (vaddr % SUPERPAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             && !VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Present and 4 KiB paging, then remove the descendant table */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);

            /* Delete descendant table */
            _kmem_mm_page_free(kmem, vpt);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_GRW(paddr);
            avmem->vls[idxpd][idxp] = KMEM_PG_GRW(vaddr);
        } else {
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_RW(paddr);
            avmem->vls[idxpd][idxp] = KMEM_PG_RW(vaddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    } else {
        /* Page */
        /* Check the physical address argument */
        if ( 0 != (paddr % PAGESIZE) || 0 != (vaddr % PAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( !VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             || VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Not present or 2 MiB page, then create a new page table */
            vpt = _kmem_mm_page_alloc(kmem);
            if ( NULL == vpt ) {
                return -1;
            }
            /* Get the virtual address */
            pt = arch_vmem_addr_v2p(kmem->space, vpt);
            /* Update the entry */
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_DIR_RW((u64)pt);
            avmem->vls[idxpd][idxp] = KMEM_DIR_RW((u64)vpt);
        } else {
            /* Directory */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            pt[idx] = KMEM_PG_GRW(paddr);
        } else {
            pt[idx] = KMEM_PG_RW(paddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    }

    return 0;
}

/*
 * Map a virtual page to a physical super page
 */
static int
_kmem_superpage_map(struct kmem *kmem, u64 vaddr, u64 paddr, int flags)
{
    struct arch_vmem_space *avmem;
    int idxpd;
    int idxp;
    u64 *pt;
    u64 *vpt;

    /* Check the flags */
    if ( !(VMEM_USABLE & flags) || !(VMEM_USED & flags) ) {
        /* This page is not usable nor used, then do nothing. */
        return -1;
    }

    /* Get the architecture-specific kernel memory manager */
    avmem = (struct arch_vmem_space *)kmem->space->arch;

    /* Index to page directory */
    idxpd = (vaddr >> 30);
    if ( idxpd >= 4 ) {
        return -1;
    }
    /* Index to page table */
    idxp = (vaddr >> 21) & 0x1ff;

    /* Check the physical address argument */
    if ( 0 != (paddr % SUPERPAGESIZE) || 0 != (vaddr % SUPERPAGESIZE) ) {
        /* Invalid physical address */
        return -1;
    }

    /* Check whether the page presented */
    if ( VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
         && !VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
        /* Present and 4 KiB paging, then remove the descendant table */
        pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
        vpt = VMEM_PT(avmem->vls[idxpd][idxp]);

        /* Delete descendant table */
        _kmem_mm_page_free(kmem, vpt);
    }

    /* Remapping */
    if ( flags & VMEM_GLOBAL ) {
        VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_GRW(paddr);
        avmem->vls[idxpd][idxp] = KMEM_PG_GRW(vaddr);
    } else {
        VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_RW(paddr);
        avmem->vls[idxpd][idxp] = KMEM_PG_RW(vaddr);
    }

    /* Invalidate the page */
    invlpg((void *)vaddr);

    return 0;
}

/*
 * Initialize physical memory
 */
static int
_pmem_init_stage2(struct kmem *kmem, struct kstring *region,
                  struct kstring *pmem, struct kstring *pmem_pages)
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
 * Map a virtual page to a physical page
 */
int
arch_vmem_map(struct vmem_space *space, void *vaddr, void *paddr, int flags)
{
    struct arch_vmem_space *avmem;
    int idxpd;
    int idxp;
    int idx;
    u64 *pt;
    u64 *vpt;

    /* Check the flags */
    if ( !(VMEM_USABLE & flags) || !(VMEM_USED & flags) ) {
        /* This page is not usable nor used, then do nothing. */
        return -1;
    }

    /* Get the architecture-specific kernel memory manager */
    avmem = (struct arch_vmem_space *)space->arch;

    /* Index to page directory */
    idxpd = ((u64)vaddr >> 30);
    if ( idxpd >= 4 ) {
        return -1;
    }
    /* Index to page table */
    idxp = ((u64)vaddr >> 21) & 0x1ff;
    /* Index to page entry */
    idx = ((u64)vaddr >> 12) & 0x1ffULL;

    /* Superpage or page? */
    if ( VMEM_SUPERPAGE & flags ) {
        /* Superpage */
        /* Check the physical address argument */
        if ( 0 != ((u64)paddr % SUPERPAGESIZE)
             || 0 != ((u64)vaddr % SUPERPAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             && !VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Present and 4 KiB paging, then remove the descendant table */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);

            /* Delete descendant table */
            _kmem_mm_page_free(g_kmem, vpt);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            VMEM_PD(avmem->array, idxpd)[idxp] = VMEM_PG_GRW((u64)paddr);
            avmem->vls[idxpd][idxp] = VMEM_PG_GRW((u64)vaddr);
        } else {
            VMEM_PD(avmem->array, idxpd)[idxp] = VMEM_PG_RW((u64)paddr);
            avmem->vls[idxpd][idxp] = VMEM_PG_RW((u64)vaddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    } else {
        /* Page */
        /* Check the physical address argument */
        if ( 0 != ((u64)paddr % PAGESIZE) || 0 != ((u64)vaddr % PAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( !VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             || VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Not present or 2 MiB page, then create a new page table */
            vpt = _kmem_mm_page_alloc(g_kmem);
            if ( NULL == vpt ) {
                return -1;
            }
            /* Get the virtual address */
            pt = arch_vmem_addr_v2p(g_kmem->space, vpt);

            /* Update the entry */
            VMEM_PD(avmem->array, idxpd)[idxp] = VMEM_DIR_RW((u64)pt);
            avmem->vls[idxpd][idxp] = VMEM_DIR_RW((u64)vpt);
        } else {
            /* Directory */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            pt[idx] = VMEM_PG_GRW((u64)paddr);
        } else {
            pt[idx] = VMEM_PG_RW((u64)paddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    }

    return 0;
}

/*
 * Map a virtual page to a physical page
 * FIXME: This is redundant....  Will merge with arch_vmem_map?
 */
int
arch_kmem_map(struct vmem_space *space, void *vaddr, void *paddr, int flags)
{
    struct arch_vmem_space *avmem;
    int idxpd;
    int idxp;
    int idx;
    u64 *pt;
    u64 *vpt;

    /* Check the flags */
    if ( !(VMEM_USABLE & flags) || !(VMEM_USED & flags) ) {
        /* This page is not usable nor used, then do nothing. */
        return -1;
    }

    /* Get the architecture-specific kernel memory manager */
    avmem = (struct arch_vmem_space *)space->arch;

    /* Index to page directory */
    idxpd = ((u64)vaddr >> 30);
    if ( idxpd >= 4 ) {
        return -1;
    }
    /* Index to page table */
    idxp = ((u64)vaddr >> 21) & 0x1ff;
    /* Index to page entry */
    idx = ((u64)vaddr >> 12) & 0x1ffULL;

    /* Superpage or page? */
    if ( VMEM_SUPERPAGE & flags ) {
        /* Superpage */
        /* Check the physical address argument */
        if ( 0 != ((u64)paddr % SUPERPAGESIZE)
             || 0 != ((u64)vaddr % SUPERPAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             && !VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Present and 4 KiB paging, then remove the descendant table */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);

            /* Delete descendant table */
            _kmem_mm_page_free(g_kmem, vpt);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_GRW((u64)paddr);
            avmem->vls[idxpd][idxp] = KMEM_PG_GRW((u64)vaddr);
        } else {
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_PG_RW((u64)paddr);
            avmem->vls[idxpd][idxp] = KMEM_PG_RW((u64)vaddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    } else {
        /* Page */
        /* Check the physical address argument */
        if ( 0 != ((u64)paddr % PAGESIZE) || 0 != ((u64)vaddr % PAGESIZE) ) {
            /* Invalid physical address */
            return -1;
        }

        /* Check whether the page presented */
        if ( !VMEM_IS_PRESENT(VMEM_PD(avmem->array, idxpd)[idxp])
             || VMEM_IS_PAGE(VMEM_PD(avmem->array, idxpd)[idxp]) ) {
            /* Not present or 2 MiB page, then create a new page table */
            vpt = _kmem_mm_page_alloc(g_kmem);
            if ( NULL == vpt ) {
                return -1;
            }
            /* Get the virtual address */
            pt = arch_vmem_addr_v2p(g_kmem->space, vpt);

            /* Update the entry */
            VMEM_PD(avmem->array, idxpd)[idxp] = KMEM_DIR_RW((u64)pt);
            avmem->vls[idxpd][idxp] = KMEM_DIR_RW((u64)vpt);
        } else {
            /* Directory */
            pt = VMEM_PT(VMEM_PD(avmem->array, idxpd)[idxp]);
            vpt = VMEM_PT(avmem->vls[idxpd][idxp]);
        }

        /* Remapping */
        if ( flags & VMEM_GLOBAL ) {
            pt[idx] = KMEM_PG_GRW((u64)paddr);
        } else {
            pt[idx] = KMEM_PG_RW((u64)paddr);
        }

        /* Invalidate the page */
        invlpg((void *)vaddr);
    }

    return 0;
}

/*
 * Get the address width
 */
int
arch_address_width(void)
{
    u64 rax;
    u64 rcx;
    u64 rdx;

    /* Get the physical-address width */
    rax = cpuid(0x80000008, &rcx, &rdx);

    return rax & 0xff;
}

/*
 * Resolve the physical address of the specified virtual address in a virtual
 * memory space
 */
void *
arch_vmem_addr_v2p(struct vmem_space *space, void *vaddr)
{
    struct arch_vmem_space *avmem;
    int idxpd;
    int idxp;
    int idx;
    u64 *pt;
    u64 off;

    /* Get the architecture-specific data structure */
    avmem = (struct arch_vmem_space *)space->arch;

    /* Index to page directory */
    idxpd = ((reg_t)vaddr >> 30);
    if ( idxpd >= avmem->nr ) {
        /* Oversized */
        return NULL;
    }
    /* Index to page table */
    idxp = ((reg_t)vaddr >> 21) & 0x1ff;
    /* Index to page entry */
    idx = ((reg_t)vaddr >> 12) & 0x1ffULL;

    if ( VMEM_IS_PAGE(avmem->vls[idxpd][idxp]) ) {
        /* 2-MiB paging */
        /* Get the offset */
        off = ((reg_t)vaddr) & 0x1fffffULL;
        return (void *)(VMEM_PDPG(avmem->vls[idxpd][idxp]) + off);
    } else {
        /* 4-KiB paging */
        pt = VMEM_PT(avmem->vls[idxpd][idxp]);
        /* Get the offset */
        off = ((reg_t)vaddr) & 0xfffULL;
        return (void *)(VMEM_PT(pt[idx]) + off);
    }
}

/*
 * Initialize the architecture-specific virtual memory
 */
#define VMEM_NPD    6
int
arch_vmem_init(struct vmem_space *space)
{
    struct arch_vmem_space *avmem;
    struct arch_vmem_space *tmp;
    u64 *vpg;
    u64 *vls;
    u64 *paddr;
    ssize_t i;

    avmem = kmalloc(sizeof(struct arch_vmem_space));
    if ( NULL == avmem ) {
        return -1;
    }
    avmem->array = kmalloc(sizeof(u64 *) * (VMEM_NENT(VMEM_NPD) + VMEM_NPD));
    if ( NULL == avmem->array ) {
        kfree(avmem);
        return -1;
    }
    avmem->vls = kmalloc(sizeof(u64 *) * VMEM_NPD);
    if ( NULL == avmem->vls ) {
        kfree(avmem->array);
        kfree(avmem);
        return -1;
    }
    avmem->nr = VMEM_NPD;

    kmemset(avmem->array, 0, sizeof(u64 *) * (VMEM_NENT(VMEM_NPD) + VMEM_NPD));
    kmemset(avmem->vls, 0, sizeof(u64 *) * VMEM_NPD);

    vpg = kmalloc(PAGESIZE * (VMEM_NENT(VMEM_NPD) + VMEM_NPD));
    if ( NULL == vpg ) {
        kfree(avmem->vls);
        kfree(avmem->array);
        kfree(avmem);
        return -1;
    }
    vls = kmalloc(PAGESIZE * VMEM_NPD);
    if ( NULL == vls ) {
        kfree(vpg);
        kfree(avmem->vls);
        kfree(avmem->array);
        kfree(avmem);
        return -1;
    }
    kmemset(vpg, 0, PAGESIZE * (VMEM_NENT(VMEM_NPD) + VMEM_NPD));
    kmemset(vls, 0, PAGESIZE * VMEM_NPD);

    /* Set physical addresses to page directories */
    avmem->pgt = arch_vmem_addr_v2p(g_kmem->space, vpg);
    VMEM_PML4(avmem->array) = vpg;
    VMEM_PDPT(avmem->array, 0) = vpg + 512;
    for ( i = 0; i < VMEM_NPD; i++ ) {
        VMEM_PD(avmem->array, i) = vpg + 1024 + 512 * i;
    }

    /* Page directories with virtual address */
    for ( i = 0; i < VMEM_NPD; i++ ) {
        avmem->vls[i] = vls + 512 * i;
    }

    /* Setup physical page table */
    paddr = arch_vmem_addr_v2p(g_kmem->space, VMEM_PDPT(avmem->array, 0));
    vpg[0] = VMEM_DIR_RW((u64)paddr);
    for ( i = 0; i < VMEM_NPD; i++ ) {
        paddr = arch_vmem_addr_v2p(g_kmem->space, VMEM_PD(avmem->array, i));
        vpg[512 + i] = VMEM_DIR_RW((u64)paddr);
    }

    /* Set the kernel region */
    tmp = g_kmem->space->arch;
    paddr = arch_vmem_addr_v2p(g_kmem->space, VMEM_PD(tmp->array, 0));
    vpg[512] = KMEM_DIR_RW((u64)paddr);
    paddr = arch_vmem_addr_v2p(g_kmem->space, VMEM_PD(tmp->array, 3));
    vpg[512 + 3] = KMEM_DIR_RW((u64)paddr);
    paddr = arch_vmem_addr_v2p(g_kmem->space, VMEM_PD(tmp->array, 4));
    vpg[512 + 4] = KMEM_DIR_RW((u64)paddr);
    avmem->vls[0] = tmp->vls[0];
    avmem->vls[3] = tmp->vls[3];
    avmem->vls[4] = tmp->vls[4];

    /* Set the architecture-specific data structure to its parent */
    space->arch = avmem;

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
