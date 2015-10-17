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
static u64 _resolve_phys_mem_size(struct bootinfo *);
static u64 _find_pmem_region(struct bootinfo *, u64 );
static int
_init_pages(struct bootinfo *, struct pmem *, struct acpi *, u64, u64);
static int
_init_pages_range(u64, u64, struct pmem *, struct acpi *, u64, u64);
static int _count_linear_page_tables(u64);
static int _prepare_linear_page_tables(u64 *, u64);
static void _pmem_merge(struct pmem_buddy *, void *, int);
static void _enable_page_global(void);
static void _disable_page_global(void);

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
    u64 nr;
    u64 base;
    u64 sz;
    u64 pmsz;
    int nt;
    int ret;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return NULL;
    }

    /* Disable the global page feature */
    _disable_page_global();

    /* Obtain memory size from the system address map */
    sz = _resolve_phys_mem_size(bi);

    /* Calculate the number of page tables to map the physical memory space into
       the linear address */
    nt = _count_linear_page_tables(sz);
    if ( nt < 0 ) {
        return NULL;
    }

    /* Calculate the number of pages */
    nr  = DIV_CEIL(sz, PAGESIZE);

    /* Calculate the required memory size for pages */
    pmsz = nt * PMEM_PTESIZE + sizeof(struct pmem);

    /* Fine the available region for the pmem data structure */
    base = _find_pmem_region(bi, pmsz);
    /* Could not find available pages for the management structure */
    if ( 0 == base ) {
        return NULL;
    }

    /* Setup the memory page management structure */
    pm = (struct pmem *)(base + nt * PMEM_PTESIZE);
    kmemset(pm, 0, sizeof(struct pmem));
    pm->nr = nr;
    pm->arch = (void *)base;

    /* Prepare a page table for linear addressing */
    ret = _prepare_linear_page_tables(pm->arch, sz);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Linear addressing */
    set_cr3(pm->arch);

    /* Initialize all available pages with the buddy system except for wired
       memory.  Note that wired memory space are the range from 0 to
       PMEM_LBOUND, and the space used by pmem. */
    ret = _init_pages(bi, pm, acpi, base, pmsz);
    if ( ret < 0 ) {
        return NULL;
    }

    /* Set the page allocation function */
    pm->proto.alloc_pages = arch_pmem_alloc_pages;
    pm->proto.alloc_page = arch_pmem_alloc_page;
    pm->proto.free_pages = arch_pmem_free_pages;

    return pm;
}

/*
 * Allocate 2^order physical pages
 *
 * SYNOPSIS
 *      void *
 *      pmem_alloc_pages(int domain, int order);
 *
 * DESCRIPTION
 *      The pmem_alloc_pages() function allocates 2^order pages.
 *
 * RETURN VALUES
 *      The pmem_alloc_pages() function returns a pointer to allocated page.  If
 *      there is an error, it returns a NULL pointer.
 */
void *
arch_pmem_alloc_pages(int domain, int order)
{
    struct pmem_page_althdr *a;

    /* Check the argument */
    if ( order < 0 ) {
        /* Invalid argument */
        return NULL;
    }

    /* Lock */
    spin_lock(&pmem->lock);

    /* FIXME */
    a = NULL;

    /* Unlock */
    spin_unlock(&pmem->lock);

    return a;
}

/*
 * Allocate a page
 */
void *
arch_pmem_alloc_page(int domain)
{
    return arch_pmem_alloc_pages(domain, 0);
}


/*
 * Free allocated 2^order superpages
 *
 * SYNOPSIS
 *      void
 *      arch_pmem_free_pages(void *page, int order);
 *
 * DESCRIPTION
 *      The arch_pmem_free_pages() function deallocates superpages pointed by
 *      page.
 *
 * RETURN VALUES
 *      The arch_pmem_free_pages() function does not return a value.
 */
void
arch_pmem_free_pages(void *page, int order)
{
    struct pmem_page *list;
    int domain;

    /* If the order exceeds its maximum, that's something wrong. */
    if ( order > PMEM_MAX_BUDDY_ORDER || order < 0 ) {
        /* Something is wrong... */
        return;
    }

    /* Lock */
    spin_lock(&pmem->lock);

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
            a = CEIL(bse->base, SUPERPAGESIZE);
            b = FLOOR(bse->base + bse->len, SUPERPAGESIZE);

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
_init_pages(struct bootinfo *bi, struct pmem *pm, struct acpi *acpi, u64 pmbase,
            u64 pmsz)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 i;
    u64 a;
    u64 b;
    int ret;

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( BSE_USABLE == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE) / PAGESIZE;
            b = FLOOR(bse->base + bse->len, PAGESIZE) / PAGESIZE;

            ret = _init_pages_range(a, b, pm, acpi, pmbase, pmsz);
        }
    }

    return 0;
}

/*
 * Initialize pages in the range from page a through page b
 */
static int
_init_pages_range(u64 a, u64 b, struct pmem *pm, struct acpi *acpi,
                  u64 pmbase, u64 pmsz)
{
    u64 i;
    int prox;
    u64 pxbase;
    u64 pxlen;

    /* Let the proximity domain be resolve in the first loop. */
    pxbase = 0;
    pxlen = 0;
    prox = -1;

    for ( i = a; i < b; i++ ) {
        if ( i >= pm->nr ) {
            /* Overflowed page */
            return -1;
        }
        /* Check this page is not wired */
        if ( PAGE_ADDR(i + 1) < PMEM_LBOUND ) {
            /* This page is wired. */
            continue;
        } else if ( (PAGE_ADDR(i) >= pmbase
                     && PAGE_ADDR(i + 1) <= pmbase + pmsz)
                    || (PAGE_ADDR(i) <= pmbase && PAGE_ADDR(i + 1) > pmbase )
                    || (PAGE_ADDR(i) < pmbase + pmsz
                        && PAGE_ADDR(i + 1) >= pmbase + pmsz ) ) {
            /* This page is used by the pmem data structure. */
            continue;
        }

        /* Check the proximity domain of the page */
        if ( PAGE_ADDR(i) >= pxbase && PAGE_ADDR(i + 1) <= pxbase + pxlen ) {
            /* This page is within the previously resolved memory space. */
            _pmem_merge(&pm->domains[prox].buddy, (void *)PAGE_ADDR(i), 0);
        } else {
            /* This page is out of the range of the previously resolved
               proximity domain, then resolve it now. */
            prox = acpi_memory_prox_domain(acpi, PAGE_ADDR(i), &pxbase, &pxlen);
            if ( prox >= 0 ) {
                /* Set the proximity domain */
                _pmem_merge(&pm->domains[prox].buddy, (void *)PAGE_ADDR(i), 0);
            } else {
                /* No proximity domain; then clear the stored base and length
                   to force resolve the proximity domain in the next loop. */
                pxbase = 0;
                pxlen = 0;
            }
        }
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
 * Prepare a page table of the linear address space
 */
static int
_prepare_linear_page_tables(u64 *pt, u64 sz)
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
 * Enable the global page feature
 */
static void
_enable_page_global(void)
{
    /* Enable the global page feature */
    set_cr4(get_cr4() | 0x80);
}

/*
 * Disable the global page feature
 */
static void
_disable_page_global(void)
{
    /* Disable the global page feature */
    set_cr4(get_cr4() & ~0x80ULL);
}





/*
 * Initialize the kernel memory
 */
struct kmem *
arch_kmem_init(void)
{
    struct kmem *kmem;
    struct kmem_region *region[2];
    int i;

    /* Check the size of the kmem data structure first */
    if ( sizeof(struct kmem) + sizeof(struct kmem_region) * 2
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
    region[0] = (struct kmem_region *)(KMEM_BASE + sizeof(struct kmem));
    region[1] = (struct kmem_region *)(KMEM_BASE + sizeof(struct kmem)
                                       + sizeof(struct kmem_region));
    region[0]->start = (ptr_t)0;
    region[0]->len = (1ULL << 30);
    region[0]->pages = (struct kmem_page *)(KMEM_BASE + sizeof(struct kmem)
                                            + sizeof(struct kmem_region) * 2);
    region[0]->next = region[1];
    region[1]->start = (ptr_t)(3ULL << 30);
    region[1]->len = (1ULL << 30);
    region[1]->pages = (struct kmem_page *)(KMEM_BASE + sizeof(struct kmem)
                                            + sizeof(struct kmem_region) * 2
                                            + sizeof(struct kmem_page) * 512);
    region[1]->next = NULL;

    /* 512 pages in a region */
    for ( i = 0; i < 512; i++ ) {
        region[0]->pages[i].address = SUPERPAGE_ADDR(i);
        region[0]->pages[i].type = 0;
        region[1]->pages[i].address = SUPERPAGE_ADDR(i) + (3ULL << 30);
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
