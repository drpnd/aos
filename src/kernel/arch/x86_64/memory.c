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
    struct bootinfo_sysaddrmap_entry *bse;
    u64 nr;
    u64 addr;
    u64 sz;
    u64 a;
    u64 b;
    u64 i;
    u64 j;
    int prox;
    u64 pxbase;
    u64 pxlen;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.nr <= 0 ) {
        return NULL;
    }

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
    nr  = CEIL(addr, SUPERPAGESIZE) / SUPERPAGESIZE;
    sz = nr * sizeof(struct pmem_superpage) + sizeof(struct pmem);

    /* Search free space system address map obitaned from BIOS for the memory
       allocator (calculated above) */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
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

    /* Could not find available pages for the management structure */
    if ( 0 == addr ) {
        return NULL;
    }

    /* Setup the memory page management structure */
    pm = (struct pmem *)(addr + nr * sizeof(struct pmem_superpage));
    kmemset(pm, 0, sizeof(struct pmem));
    pm->nr = nr;
    pm->superpages = (struct pmem_superpage *)addr;

    /* Reset flags */
    pxbase = 0;
    pxlen = 0;
    prox = -1;
    for ( i = 0; i < pm->nr; i++ ) {
        /* Mark as unavailable */
        pm->superpages[i].flags = PMEM_UNAVAIL;
        pm->superpages[i].order = -1;
        pm->superpages[i].prox_domain = -1;
        pm->superpages[i].refcnt = 0;

        /* Check the proximity domain */
        if ( i * SUPERPAGESIZE >= pxbase
             && (i + 1) * SUPERPAGESIZE <= pxbase + pxlen ) {
            pm->superpages[i].prox_domain = prox;
        } else {
            prox = acpi_memory_prox_domain(acpi, i * SUPERPAGESIZE, &pxbase,
                                           &pxlen);
            if ( prox >= 0 ) {
                pm->superpages[i].prox_domain = prox;
            } else {
                pxbase = 0;
                pxlen = 0;
            }
        }
    }

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.nr; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, SUPERPAGESIZE) / SUPERPAGESIZE;
            b = FLOOR(bse->base + bse->len, SUPERPAGESIZE) / SUPERPAGESIZE;

            /* Mark as unallocated */
            for ( j = a; j < b; j++ ) {
                if ( j >= pm->nr ) {
                    /* Error */
                    return NULL;
                }
                /* Unmark unavailable */
                pm->superpages[j].flags &= ~PMEM_UNAVAIL;
                if ( j * SUPERPAGESIZE <= PMEM_LBOUND ) {
                    /* Wired by kernel */
                    pm->superpages[j].flags |= PMEM_WIRED;
                }
            }
        }
    }

    /* Mark self (used by phys_mem and phys_mem->pages) */
    for ( i = addr / SUPERPAGESIZE;
          i <= CEIL(addr + sz, SUPERPAGESIZE) / SUPERPAGESIZE; i++ ) {
        pm->superpages[i].flags |= PMEM_WIRED;
    }

    return pm;
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

    return ent[pd];
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
