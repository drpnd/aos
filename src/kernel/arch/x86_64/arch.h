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

#ifndef _KERNEL_ARCH_H
#define _KERNEL_ARCH_H

#include <aos/const.h>

/* Boot information from the boot loader */
#define BOOTINFO_BASE           (u64)0x8000

/* # of IDT entries */
#define IDT_NR                  256

/* GDT and IDT */
#define GDT_ADDR                (u64)0x74000
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                (u64)0x76000
#define IDT_MAX_SIZE            0x2000

/* GDT selectors */
#define GDT_NR                  9
#define GDT_NULL_SEL            (0<<3)
#define GDT_RING0_CODE_SEL      (1<<3)
#define GDT_RING0_DATA_SEL      (2<<3)
#define GDT_RING1_CODE_SEL      (3<<3)
#define GDT_RING1_DATA_SEL      (4<<3)
#define GDT_RING2_CODE_SEL      (5<<3)
#define GDT_RING2_DATA_SEL      (6<<3)
#define GDT_RING3_CODE_SEL      (7<<3)
#define GDT_RING3_DATA_SEL      (8<<3)

/*********************************************************/
/* The folloowing values are also defined in asmconst.h */
/*********************************************************/
/* Kernel page table */
#define KERNEL_PGT              (u64)0x00079000
/* Per-processor information (flags, cpuinfo, stats, tss, task, stack) */
#define P_DATA_BASE             (u64)0x01000000
#define P_DATA_SIZE             0x10000
#define P_STACK_GUARD           0x10

/*
 * Boot information from boot loader
 */
struct bootinfo {
    struct {
        u64 nr;
        u64 entries;    /* (struct bootinfo_sysaddrmap_entry *) */
    } __attribute__ ((packed)) sysaddrmap ;
} __attribute__ ((packed));
struct bootinfo_sysaddrmap_entry {
    u64 base;
    u64 len;
    u32 type;
    u32 attr;
} __attribute__ ((packed));

/* in asm.s */
void lidt(void *);
void lgdt(void *, u64);
void intr_null(void);

#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
