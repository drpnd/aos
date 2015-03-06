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
#include "desc.h"
#include "arch.h"
#include "../../kernel.h"

#define IDT_PRESENT     0x80
#define IDT_INTGATE     0x0e
void intr_null(void);

/*
 * Setup gate descriptor
 */
static void
idt_setup_gate_desc(int nr, u64 base, u16 selector, u8 flags)
{
    struct idtr *idtr;
    struct idt_gate_desc *idt;

    idtr = (struct idtr *)(IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR);
    idt = (struct idt_gate_desc *)(idtr->base
                                   + nr * sizeof(struct idt_gate_desc));
    idt->target_lo = (u16)(base & 0xffff);
    idt->selector = (u16)selector;
    idt->reserved1 = 0;
    idt->flags = flags;
    idt->target_mid = (u16)((base & 0xffff0000UL) >> 16);
    idt->target_hi = (u16)((base & 0xffffffff00000000UL) >> 32);
    idt->reserved2 = 0;
}

/*
 * Setup interrupt gate of interrupt descriptor table
 */
void
idt_setup_intr_gate(int nr, void *target)
{
    idt_setup_gate_desc(nr, (u64)target, GDT_RING0_CODE_SEL,
                        IDT_PRESENT | IDT_INTGATE);
}

/*
 * Initialize interrupt descriptor table
 */
void
idt_init(void)
{
    struct idtr *idtr;
    int i;

    idtr = (struct idtr *)(IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR);
    idtr->base = IDT_ADDR;
    idtr->size = IDT_NR * sizeof(struct idt_gate_desc) - 1;

    for ( i = 0; i < 256; i++ ) {
        idt_setup_intr_gate(i, &intr_null);
    }
}

/*
 * Load interrupt descriptor table
 */
void
idt_load(void)
{
    struct idtr *idtr;

    idtr = (struct idtr *)(IDT_ADDR + sizeof(struct idt_gate_desc) * IDT_NR);
    lidt(idtr);
}

/*
 * Setup global descriptor table entry
 */
void
gdt_setup_desc(struct gdt_desc *e, u32 base, u32 limit, u8 type, u8 dpl,
               u8 lbit, u8 dbit, u8 gbit)
{
    limit &= 0xfffff;
    /* 16bit: l=0, d=0, 32bit: l=0, d=1, 64bit: l=1, d=0 */
    e->w0 = limit & 0xffff;
    e->w1 = base & 0xffff;
    e->w2 = ((base>>16) & 0xff) | ((u64)type<<8) | ((u64)1<<12)
        | ((u64)dpl<<13) | ((u64)1<<15);
    e->w3 = ((limit>>16) & 0xf) | ((u64)lbit<<5) | ((u64)dbit<<6)
        | ((u64)gbit<<7) | (((base>>24) & 0xff)<<8);
}

/*
 * Initialize global descriptor table
 */
void
gdt_init(void)
{
    u64 sz;
    struct gdt_desc *gdt;
    struct gdtr *gdtr;

    sz = (1 + 2 * 4) * sizeof(struct gdt_desc);
    gdtr = (struct gdtr *)(GDT_ADDR + sz);
    gdt = (struct gdt_desc *)GDT_ADDR;

    gdtr->base = (u64)gdt;
    gdtr->size = sz - 1;

    /* Null descriptor */
    gdt_setup_desc(&gdt[0], 0, 0, 0, 0, 0, 0, 0);
    /* For each ring */
    gdt_setup_desc(&gdt[1], 0, 0xfffff, 0xa, 0, 1, 0, 1);
    gdt_setup_desc(&gdt[2], 0, 0xfffff, 0x2, 0, 1, 0, 1);
    gdt_setup_desc(&gdt[3], 0, 0xfffff, 0xa, 1, 1, 0, 1);
    gdt_setup_desc(&gdt[4], 0, 0xfffff, 0x2, 1, 1, 0, 1);
    gdt_setup_desc(&gdt[5], 0, 0xfffff, 0xa, 2, 1, 0, 1);
    gdt_setup_desc(&gdt[6], 0, 0xfffff, 0x2, 2, 1, 0, 1);
    gdt_setup_desc(&gdt[7], 0, 0xfffff, 0xa, 3, 1, 0, 1);
    gdt_setup_desc(&gdt[8], 0, 0xfffff, 0x2, 3, 1, 0, 1);
}

/*
 * Load global descriptor table
 */
void
gdt_load(void)
{
    struct gdtr *gdtr;

    gdtr = (struct gdtr *)(GDT_ADDR + (1 + 2 * 4) * 8);
    lgdt(gdtr, GDT_RING0_CODE_SEL);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
