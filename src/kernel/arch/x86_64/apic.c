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
#include "acpi.h"
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

u64 apic_base;

/*
 * Initialize the local APIC
 */
void
lapic_init(void)
{
    u32 reg;
    u64 msr;

    /* Get the IA32_APIC_BASE */
    msr = rdmsr(APIC_MSR);
    apic_base = msr & 0xfffffffffffff000;

    /* Enable APIC at spurious interrupt vector register: default vector 0xff */
    reg = mfread32(apic_base + APIC_SIVR);
    reg |= 0x100;       /* Bit 8: APIC Software Enable/Disable */
    mfwrite32(apic_base + APIC_SIVR, reg);
}

/*
 * Return the APIC_BASE address
 */
u64
lapic_base_addr(void)
{
    return apic_base;
}

/*
 * Send INIT IPI
 */
void
lapic_send_init_ipi(void)
{
    u32 icrl;
    u32 icrh;

    icrl = mfread32(apic_base + APIC_ICR_LOW);
    icrh = mfread32(apic_base + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_INIT | ICR_DEST_ALL_EX_SELF;
    icrh = (icrh & 0x000fffff);

    mfwrite32(apic_base + APIC_ICR_HIGH, icrh);
    mfwrite32(apic_base + APIC_ICR_LOW, icrl);
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
        icrl = mfread32(apic_base + APIC_ICR_LOW);
        icrh = mfread32(apic_base + APIC_ICR_HIGH);
        /* Wait until it's idle */
    } while ( icrl & (ICR_SEND_PENDING) );

    icrl = (icrl & ~0x000cdfff) | ICR_STARTUP | ICR_DEST_ALL_EX_SELF | vector;
    icrh = (icrh & 0x000fffff);

    mfwrite32(apic_base + APIC_ICR_HIGH, icrh);
    mfwrite32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * Broadcast fixed IPI
 */
void
lapic_send_fixed_ipi(u8 vector)
{
    u32 icrl;
    u32 icrh;

    icrl = mfread32(apic_base + APIC_ICR_LOW);
    icrh = mfread32(apic_base + APIC_ICR_HIGH);

    icrl = (icrl & ~0x000cdfff) | ICR_FIXED | ICR_DEST_ALL_EX_SELF | vector;
    icrh = (icrh & 0x000fffff);

    mfwrite32(apic_base + APIC_ICR_HIGH, icrh);
    mfwrite32(apic_base + APIC_ICR_LOW, icrl);
}

/*
 * Return this local APIC ID
 */
int
lapic_id(void)
{
    u32 reg;

    reg = *(u32 *)(apic_base + APIC_LAPIC_ID);

    return reg >> 24;
}

/*
 * Estimate bus frequency using busy usleep
 */
u64
lapic_estimate_freq(void)
{
    u32 t0;
    u32 t1;
    u32 probe;
    u64 ret;

    /* Set probe timer */
    probe = APIC_FREQ_PROBE;

    /* Disable timer */
    mfwrite32(apic_base + APIC_LVT_TMR, APIC_LVT_DISABLE);

    /* Set divide configuration */
    mfwrite32(apic_base + APIC_TMRDIV, APIC_TMRDIV_X16);

    /* Vector: lvt[18:17] = 00 : oneshot */
    mfwrite32(apic_base + APIC_LVT_TMR, 0x0);

    /* Set initial counter */
    t0 = 0xffffffff;
    mfwrite32(apic_base + APIC_INITTMR, t0);

    /* Sleep probing time */
    acpi_busy_usleep(&arch_acpi, probe);

    /* Disable current timer */
    mfwrite32(apic_base + APIC_LVT_TMR, APIC_LVT_DISABLE);

    /* Read current timer */
    t1 = mfread32(apic_base + APIC_CURTMR);

    /* Calculate the APIC bus frequency */
    ret = (u64)(t0 - t1) << 4;
    ret = ret * 1000000 / probe;

    return ret;
}

/*
 * Start local APIC timer
 */
void
lapic_start_timer(u64 freq, u8 vec)
{
    /* Estimate frequency first */
    u64 busfreq;
    struct p_data *pdata;

    /* Get CPU frequency to this CPU data area */
    pdata = this_cpu();
    busfreq = pdata->freq;

    /* Set counter */
    mfwrite32(apic_base + APIC_LVT_TMR, APIC_LVT_PERIODIC | (u32)vec);
    mfwrite32(apic_base + APIC_TMRDIV, APIC_TMRDIV_X16);
    mfwrite32(apic_base + APIC_INITTMR, (busfreq >> 4) / freq);
}

/*
 * Stop APIC timer
 */
void
lapic_stop_timer(void)
{
    /* Disable timer */
    mfwrite32(apic_base + APIC_LVT_TMR, APIC_LVT_DISABLE);
}

/*
 * Initialize I/O APIC
 */
void
ioapic_init(void)
{
    /* Ensure to disable i8259 PIC */
    outb(0xa1, 0xff);
    outb(0x21, 0xff);
}

/*
 * Set a map entry of interrupt vector
 */
void asm_ioapic_map_intr(u64 val, u64 tbldst, u64 ioapic_base);
void
ioapic_map_intr(u64 intvec, u64 tbldst, u64 ioapic_base)
{
    u64 val;

    /*
     * 63:56    destination field
     * 16       interrupt mask (1: masked for edge sensitive)
     * 15       trigger mode (1=level sensitive, 0=edge sensitive)
     * 14       remote IRR (R/O) (1 if local APICs accept the level interrupts)
     * 13       interrupt input pin polarity (0=high active, 1=low active)
     * 12       delivery status (R/O)
     * 11       destination mode (0=physical, 1=logical)
     * 10:8     delivery mode
     *          000 fixed, 001 lowest priority, 010 SMI, 011 reserved
     *          100 NMI, 101 INIT, 110 reserved, 111 ExtINT
     * 7:0      interrupt vector
     */
    val = intvec;

    /* To avoid compiler optimization, call assembler function */
    asm_ioapic_map_intr(val, tbldst, ioapic_base);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
