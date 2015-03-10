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
#include "apic.h"
#include "arch.h"

/* ICR delivery mode */
#define ICR_FIXED               0x00000000
#define ICR_INIT                0x00000500
#define ICR_STARTUP             0x00000600
/* ICR status */
#define ICR_SEND_PENDING        0x00001000
/* ICR level */
#define ICR_LEVEL_ASSERT        0x00004000
/* ICR destination */
#define ICR_DEST_NOSHORTHAND    0x00000000
#define ICR_DEST_SELF           0x00040000
#define ICR_DEST_ALL_INC_SELF   0x00080000
#define ICR_DEST_ALL_EX_SELF    0x000c0000

/*
 * Initialize the local APIC
 */
void
lapic_init(void)
{
    u32 reg;

    /* Enable APIC at spurious interrupt vector register: default vector 0xff */
    reg = mfread32(APIC_BASE + APIC_SIVR);
    reg |= 0x100;       /* Bit 8: APIC Software Enable/Disable */
    mfwrite32(APIC_BASE + APIC_SIVR, reg);
}

/*
 * Send INIT IPI
 */
void
lapic_send_init_ipi(void)
{
    u32 icrl;
    u32 icrh;

    icrl = mfread32(APIC_BASE + APIC_ICR_LOW);
    icrh = mfread32(APIC_BASE + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_INIT | ICR_DEST_ALL_EX_SELF;
    icrh = (icrh & 0x000fffff);

    mfwrite32(APIC_BASE + APIC_ICR_HIGH, icrh);
    mfwrite32(APIC_BASE + APIC_ICR_LOW, icrl);
}

/*
 * Send Start Up IPI
 */
void
lapic_send_startup_ipi(u8 vector)
{
    u32 icrl;
    u32 icrh;

    do {
        icrl = mfread32(APIC_BASE + APIC_ICR_LOW);
        icrh = mfread32(APIC_BASE + APIC_ICR_HIGH);
        /* Wait until it's idle */
    } while ( icrl & (ICR_SEND_PENDING) );

    icrl = (icrl & ~0x000cdfff) | ICR_STARTUP | ICR_DEST_ALL_EX_SELF | vector;
    icrh = (icrh & 0x000fffff);

    mfwrite32(APIC_BASE + APIC_ICR_HIGH, icrh);
    mfwrite32(APIC_BASE + APIC_ICR_LOW, icrl);
}

/*
 * Return this local APIC ID
 */
int
lapic_id(void)
{
    u32 reg;

    reg = *(u32 *)(APIC_BASE + APIC_LAPIC_ID);

    return reg >> 24;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
