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
    u64 base;   /* (idt_gate_descriptor *) */
} __attribute__ ((packed));

/*
 * Global Descriptor
 */
struct gdt_desc {
    u16 w0;
    u16 w1;
    u16 w2;
    u16 w3;
} __attribute__ ((packed));

/*
 * Global Descriptor Table Register
 */
struct gdtr {
    u16 size;
    u64 base;
} __attribute__ ((packed));


void gdt_init(void);
void gdt_load(void);

#endif /* _KERNEL_DESC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
