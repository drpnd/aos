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

#define KMEM_LOW_P2V(a)     (a)
#define FLOOR(val, base)        (((val) / (base)) * (base))
#define CEIL(val, base)         ((((val) - 1) / (base) + 1) * (base))
#define DIV_FLOOR(val, base)    ((val) / (base))
#define DIV_CEIL(val, base)     (((val) - 1) / (base) + 1)

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
static struct kmem * _kmem_init(void *, u64);
static int _kmem_pgt_init(struct arch_kmem **, u64 *);
static void * _kmem_page_alloc(struct kmem *);
static void _kmem_page_free(struct kmem *, void *);
static struct vmem_space * _kmem_vmem_space_create(void *, u64, u64 *);
static int _kmem_vmem_space_pgt_reflect(struct kmem *);
static int _kmem_vmem_map(struct kmem *, u64, u64, int);
static int _pmem_alloc(struct bootinfo *, void **, u64 *);
static int
_pmem_init(struct kmem *, struct bootinfo *, struct acpi *, void *, u64);

static u64 _resolve_phys_mem_size(struct bootinfo *);
static void * _find_pmem_region(struct bootinfo *, u64 );
static int _init_pmem_zone_buddy(struct pmem *, struct acpi *);
static int _aligned_usable_pages(struct pmem *, struct acpi *, u64, int *);
static int _pmem_split(struct pmem_buddy *, int);
static void _pmem_merge(struct pmem_buddy *, void *, int);
static void _pmem_return_to_buddy(struct pmem *, void *, int, int);
static __inline__ int _pmem_page_zone(void *, int);
static void _enable_page_global(void);
static void _disable_page_global(void);


#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc
u16
pci_read_config(u16 bus, u16 slot, u16 func, u16 offset)
{
    u32 addr;

    addr = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8)
        | ((u32)offset & 0xfc);
    /* Set enable bit */
    addr |= (u32)0x80000000;

    outl(PCI_CONFIG_ADDR, addr);
    return (inl(0xcfc) >> ((offset & 2) * 8)) & 0xffff;
}
u64
pci_read_mmio(u8 bus, u8 slot, u8 func)
{
    u64 addr;
    u32 bar0;
    u32 bar1;
    u8 type;
#if 0
    u8 prefetchable;
#endif

    bar0 = pci_read_config(bus, slot, func, 0x10);
    bar0 |= (u32)pci_read_config(bus, slot, func, 0x12) << 16;

    type = (bar0 >> 1) & 0x3;
#if 0
    prefetchable = (bar0 >> 3) & 0x1;
#endif
    addr = bar0 & 0xfffffff0;

    if ( 0x00 == type ) {
        /* 32bit */
    } else if ( 0x02 == type ) {
        /* 64bit */
        bar1 = pci_read_config(bus, slot, func, 0x14);
        bar1 |= (u32)pci_read_config(bus, slot, func, 0x16) << 16;
        addr |= ((u64)bar1) << 32;
    } else {
        return 0;
    }

    return addr;
}
u8
pci_get_header_type(u16 bus, u16 slot, u16 func)
{
    return pci_read_config(bus, slot, func, 0x0e) & 0xff;
}

void
pci_check_function(u8 bus, u8 slot, u8 func)
{
    u16 vendor;
    u16 device;
    u16 reg;
    u16 class;
    u16 prog;
    vendor = pci_read_config(bus, slot, func, 0);
    device = pci_read_config(bus, slot, func, 2);

    /* Read interrupt pin and line */
    reg = pci_read_config(bus, slot, func, 0x3c);

    /* Read class and subclass */
    class = pci_read_config(bus, slot, func, 0x0a);

    /* Read program interface and revision ID */
    prog = pci_read_config(bus, slot, func, 0x08);

    if ( vendor == 0x8086 && device != 0x1237 && device != 0x7000
         && device != 0x7111 ) {
        u64 mmio = pci_read_mmio(bus, slot, func);
        //__asm__ ("movq %%rax,%%dr1" :: "a"(device));
        //__asm__ ("movq %%rax,%%dr2" :: "a"(mmio));
        //panic("xxx");
    }
}
void
pci_check_device(u8 bus, u8 device)
{
    u16 vendor;
    u8 func;
    u8 hdr_type;

    func = 0;
    vendor = pci_read_config(bus, device, func, 0);
    if ( 0xffff == vendor ) {
        return;
    }

    pci_check_function(bus, device, func);
    hdr_type = pci_get_header_type(bus, device, func);

    if ( hdr_type & 0x80 ) {
        /* Multiple functions */
        for ( func = 1; func < 8; func++ ) {
            vendor = pci_read_config(bus, device, func, 0);
            if ( 0xffff != vendor ) {
                pci_check_function(bus, device, func);
            }
         }
    }
}
void
pci_check_bus(u8 bus)
{
    u8 device;

    for ( device = 0; device < 32; device++ ) {
        pci_check_device(bus, device);
    }
}
void
pci_check_all_buses(void)
{
    u16 bus;
    u8 func;
    u8 hdr_type;
    u16 vendor;

    hdr_type = pci_get_header_type(0, 0, 0);
    if ( !(hdr_type & 0x80) ) {
        /* Single PCI host controller */
        for ( bus = 0; bus < 256; bus++ ) {
            pci_check_bus(bus);
        }
    } else {
        /* Multiple PCI host controllers */
        for ( func = 0; func < 8; func++ ) {
            vendor = pci_read_config(0, 0, func, 0);
            if ( 0xffff != vendor ) {
                break;
            }
            bus = func;
            pci_check_bus(bus);
        }
    }
}

/*
 * Initialize physical memory
 *
 * SYNOPSIS
 *      int
 *      arch_pmem_init(struct bootinfo *bi, struct acpi *acpi);
 *
 * DESCRIPTION
 *      The arch_pmem_init() function initializes the physical memory manager
 *      with the memory map information bi inherited from the boot monitor.
 *      The third argument acpi is used to determine the proximity domain of the
 *      memory spaces.
 *
 * RETURN VALUES
 *      If successful, the arch_pmem_init() function returns the value of 0.
 *      It returns the value of -1 on failure.
 */
int
arch_memory_init(struct bootinfo *bi, struct acpi *acpi)
{
    struct pmem *pm;
    struct kmem *kmem;
    u64 npg;
    void *base;
    u64 sz;
    u64 pmsz;
    int ret;
    int nzme;

    /* Allocate physical memory management data structure */
    ret = _pmem_alloc(bi, &base, &pmsz);
    if ( ret < 0 ) {
        return -1;
    }

    /* Initialize the kernel memory management data structure */
    kmem = _kmem_init((void *)base, pmsz);
    if ( NULL == kmem ) {
        return -1;
    }

    /* Initialize the physical pages */
    ret = _pmem_init(kmem, bi, acpi, base, pmsz);
    if ( ret < 0 ) {
        return -1;
    }

    panic("stop here for refactoring");


    /* Search all PCI devices */
    pci_check_all_buses();


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


    /* Initialize all usable pages with the buddy system except for wired
       memory.  Note that the wired memory space is the range from 0 to
       PMEM_LBOUND, and the space used by pmem. */
    ret = _init_pmem_zone_buddy(pm, acpi);

    return pm;
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
    *base = _find_pmem_region(bi, *pmsz);
    /* Could not find available pages for the management structure */
    if ( NULL == *base ) {
        return -1;
    }

    return 0;
}

/* Initialize virtual memory space for kernel */
static struct kmem *
_kmem_init(void *pmbase, u64 pmsz)
{
    u64 i;
    u64 off;
    struct arch_kmem *akmem;
    struct kmem *kmem;
    struct vmem_space *space;
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
    space = _kmem_vmem_space_create(pmbase, pmsz, &off);
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

    /* Create buddy system for kernel memory */

    /* Reflect the regions to the page table */
    ret = _kmem_vmem_space_pgt_reflect(kmem);
    if ( ret < 0 ) {
        return NULL;
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

    /* Set the constructured page table */
    set_cr3(stkmem.pml4);

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
    struct vmem_page *pgs_low;
    struct vmem_page *pgs_pmem;
    struct vmem_page *pgs_kernel;

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
    reg_low->type = VMEM_REGION_BITMAP;
    reg_low->total_pgs = DIV_CEIL(PMEM_LBOUND, PAGESIZE);
    reg_low->used_pgs = DIV_CEIL(PMEM_LBOUND, PAGESIZE);

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
    reg_pmem->type = VMEM_REGION_BITMAP;
    reg_pmem->total_pgs = DIV_CEIL(pmsz, PAGESIZE);
    reg_pmem->used_pgs = DIV_CEIL(pmsz, PAGESIZE);

    /* Kernel address space (3-4 GiB) */
    reg_kernel = (struct vmem_region *)KMEM_LOW_P2V(KMEM_BASE + *off);
    *off += sizeof(struct vmem_region);
    if ( *off > KMEM_MAX_SIZE ) {
        return NULL;
    }
    kmemset(reg_kernel, 0, sizeof(struct vmem_region));
    reg_kernel->start = (void *)KMEM_REGION_KERNEL_BASE;
    reg_kernel->len = KMEM_REGION_KERNEL_SIZE;
    reg_kernel->type = VMEM_REGION_BITMAP;
    reg_kernel->total_pgs = KMEM_REGION_KERNEL_SIZE / PAGESIZE;
    reg_kernel->used_pgs = 0;

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
        pgs_low[i].order = PMEM_INVAL_BUDDY_ORDER;
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
        pgs_pmem[i].order = PMEM_INVAL_BUDDY_ORDER;
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
        pgs_kernel[i].order = PMEM_INVAL_BUDDY_ORDER;
        pgs_kernel[i].flags = VMEM_USABLE;
        pgs_kernel[i].region = reg_kernel;
        pgs_kernel[i].next = NULL;
    }

    /* Page-alignment */
    *off = CEIL(*off, PAGESIZE);

    /* Set the allocated pages to each region */
    reg_low->pages = pgs_low;
    reg_pmem->pages = pgs_pmem;
    reg_kernel->pages = pgs_kernel;

    /* Create the chain of regions */
    space->first_region = reg_low;
    reg_low->next = reg_pmem;
    reg_pmem->next = reg_kernel;
    reg_kernel->next = NULL;

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
_pmem_init(struct kmem *kmem, struct bootinfo *bi, struct acpi *acpi,
           void *pmbase, u64 pmsz)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 i;
    u64 sz;
    u64 npg;
    struct pmem *pmem;
    u64 a;
    u64 b;
    u64 pg;
    u64 pxbase;
    u64 pxlen;
    int prox;

    /* Obtain memory (space) size from the system address map */
    sz = _resolve_phys_mem_size(bi);

    /* Calculate the number of pages from the upper-bound of the memory space */
    npg = DIV_CEIL(sz, PAGESIZE);

    /* Physical memory */
    pmem = (struct pmem *)(KMEM_REGION_PMEM_BASE
                           + npg * sizeof(struct pmem_page));
    pmem->nr = npg;
    pmem->pages = (struct pmem_page *)KMEM_REGION_PMEM_BASE;

    /* Mark as used for the pmem pages */
    for ( i = 0; i < DIV_CEIL(pmsz, PAGESIZE); i++ ) {
        pg = DIV_FLOOR((u64)pmbase, PAGESIZE) + i;
        pmem->pages[pg].flags |= PMEM_USED;
    }

    /* Mark the usable region */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( BSE_USABLE == bse->type ) {
            a = DIV_CEIL(bse->base, PAGESIZE);
            b = DIV_FLOOR(bse->base + bse->len, PAGESIZE);
            for ( pg = a; pg < b; pg++ ) {
                pmem->pages[pg].flags |= PMEM_USABLE;
            }
        }
    }

    /* FIXME */
    int ret;
    ret = _kmem_vmem_map(kmem, PAGE_INDEX(0x7e254000), 0x7e254000, 0);

    /* Set physical memory manager */
    kmem->pmem = pmem;

    /* Resolve the zone of each page */
    if ( acpi_is_numa(acpi) ) {
        /* NUMA */
        prox = -1;
        for ( i = 0; i < pmem->nr; i++ ) {
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
            pmem->pages[i].zone = _pmem_page_zone((void *)PAGE_ADDR(i), prox);
        }
    } else {
        /* UMA */
        for ( i = 0; i < pmem->nr; i++ ) {
            pmem->pages[i].zone = _pmem_page_zone((void *)PAGE_ADDR(i), -1);
        }
    }

    /* Initialize the buddy system */

#if 0
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

    return 0;
}

/*
 * Construct buddy system for physical memory
 */
static int
_pmem_buddy_init(struct pmem *pmem)
{
    return -1;
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
    //zone = _pmem_page_zone(page);

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
static __inline__ int
_pmem_page_zone(void *page, int prox)
{
    if ( (u64)page < 0x1000000 ) {
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
