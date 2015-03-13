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

#ifndef _KERNEL_DESC_H
#define _KERNEL_DESC_H

#include <aos/const.h>

/* GDT type */
#define GDT_TYPE_EX     8
#define GDT_TYPE_DC     4
#define GDT_TYPE_RW     2
#define GDT_TYPE_AC     1
/* IDT flags */
#define IDT_PRESENT     0x80
#define IDT_INTGATE     0x0e

#define TSS_INACTIVE    0x9
#define TSS_BUSY        0xb

/*
 * Global Descriptor
 *  base: 32-bit base address of the memory space
 *  limit: 20-bit size minus 1
 *  type: (Executable, Direction/Conforming, Readable/Writable, Accessed)
 *  DPL: Privilege (0: highest, 3: lowest)
 *  P: Present bit
 *  G: Granularity; 0 for 1 byte block, 1 for 4 KiB block granularity
 *  DB: Size; 0 for 16 bit mode, 1 for 32 bit mode
 *  L: 1 for 64 bit mode
 *  A: 0
 *  S: 1
 */
struct gdt_desc {
    u16 w0;     /* limit[0:15] */
    u16 w1;     /* base[0:15] */
    u16 w2;     /* base[16:23] type[0:3] S DPL[0:1] P  */
    u16 w3;     /* limit[16:19] A L DB G base[24:31] */
} __attribute__ ((packed));

/*
 * TSS
 */
struct gdt_desc_tss {
    u16 w0;
    u16 w1;
    u16 w2;
    u16 w3;
    u16 w4;
    u16 w5;
    u16 w6;
    u16 w7;
} __attribute__ ((packed));

/*
 * Global Descriptor Table Register
 */
struct gdtr {
    u16 size;
    u64 base;   /* (struct gdt_desc *) */
} __attribute__ ((packed));

/*
 * Interrupt Descriptor
 */
struct idt_gate_desc {
    u16 target_lo;
    u16 selector;
    u8 reserved1;
    u8 flags;
    u16 target_mid;
    u32 target_hi;
    u32 reserved2;
} __attribute__ ((packed));

/*
 * Interrupt Descriptor Table Register
 */
struct idtr {
    u16 size;
    u64 base;   /* (struct idt_gate_desc *) */
} __attribute__ ((packed));

/* Prototype declarations */
void gdt_init(void);
void gdt_load(void);
void idt_init(void);
void idt_load(void);
void idt_setup_intr_gate(int, void *);
void tss_init(void);
void tr_load(int);

#endif /* _KERNEL_DESC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
