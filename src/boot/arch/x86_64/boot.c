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

#include "boot.h"

void hlt(void);
void ljmp(u64, u64);

#define IDT_NR 256
//typedef void (*intr_handler_f)(void);
//void (*intr_handlers[IDT_NR])(void);

/*
 * Setup IDT
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
struct idtr {
    u16 size;
    u64 base;
} __attribute__ ((packed));
struct idtr idtr;
void intr_null(void);
void intr_irq6(void);
void
setup_idt(void)
{
    int i;
    struct idt_gate_desc *idt;
    u64 base;

    for ( i = 0; i < 256; i++ ) {
        base = (u64)intr_null;
        if ( i == 0x20 + 6 ) {
            base = (u64)intr_irq6;
        }
        idt = (struct idt_gate_desc *)(0x10000ULL + i * 16);
        idt->target_lo = (u16)(base & 0xffff);
        idt->selector = 0x08;
        idt->reserved1 = 0;
        idt->flags = 0x8e;
        idt->target_mid = (u16)((base & 0xffff0000UL) >> 16);
        idt->target_hi = (u16)((base & 0xffffffff00000000UL) >> 32);
        idt->reserved2 = 0;
    }
    idtr.size = 256 * 16 - 1;
    idtr.base = 0x10000ULL;

    __asm__ __volatile__ ( "lidt (_idtr)" );
    __asm__ __volatile__ ( "sti" );
}

/*
 * Entry point for C code
 */
void
centry(void)
{
    /* Print message */
    u16 *base;
    char *msg = "Congraturations!  Welcome to the 64-bit world!";
    int offset;

    //setup_idt();

    base = (u16 *)0xb8000;
    offset = 0;
    while ( msg[offset] ) {
        *(base + offset) = 0x0700 | msg[offset];
        offset++;
    }

    ljmp(0x8, 0x10000);
    /* Sleep forever */
    for ( ;; ) {
        hlt();
    }
}

void
isr(u64 vec)
{
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
