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
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "i8254.h"
#include "apic.h"

static int _load_trampoline(void);

struct acpi arch_acpi;

/*
 * Initialize the bootstrap processor
 */
void
bsp_init(void)
{
    struct p_data *pdata;
    u16 *video;
    long long i;

    /* Ensure the i8254 timer is stopped */
    i8254_stop_timer();

    /* Reset all processors */
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        /* Fill the processor data space with zero excluding stack area */
        kmemset((u8 *)((u64)P_DATA_BASE + i * P_DATA_SIZE), 0,
                sizeof(struct p_data));
    }

    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Load ACPI */
    acpi_load(&arch_acpi);

    /* Set up interrupt vector */
    idt_setup_intr_gate(IV_LOC_TMR, intr_apic_loc_tmr);

    /* Initialize I/O APIC */
    ioapic_init();

    /* Setup interrupt service routine then initialize I/O APIC */
    for ( i = 0; i < 32; i++ ) {
        ioapic_map_intr(IV_IRQ(i), i, arch_acpi.acpi_ioapic_base); /* IRQn */
    }

    /* Load LDT */
    lldt(0);

    /* Initialize TSS */
    tss_init();
    tr_load(lapic_id());

    /* Initialize the local APIC */
    lapic_init();

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Display a mark to notify me that this code is properly executed */
    video = (u16 *)0xb8000;
    *(video + lapic_id()) = 0x0700 | '*';

    /* Load trampoline code */
    _load_trampoline();

    /* Send INIT IPI */
    lapic_send_init_ipi();

    /* Wait 10 ms */
    acpi_busy_usleep(&arch_acpi, 10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&arch_acpi, 200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&arch_acpi, 200);

    /* Initialize local APIC counter */
    sti();
    lapic_start_timer(HZ, IV_LOC_TMR);
}

/*
 * Initialize the application processor
 */
void
ap_init(void)
{
    struct p_data *pdata;
    u16 *video;

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Load LDT */
    lldt(0);

    /* Load TSS */
    tr_load(lapic_id());

    /* Initialize the local APIC */
    lapic_init();

    /* Display a mark to notify me that this code is properly executed */
    video = (u16 *)0xb8000;
    *(video + lapic_id()) = 0x0700 | '*';
}

/*
 * Relocate the trampoline code to a 4 KiB page alined space
 */
static int
_load_trampoline(void)
{
    int i;
    int tsz;

    /* Check and copy trampoline code */
    tsz = (u64)trampoline_end - (u64)trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        return -1;
    }
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)(TRAMPOLINE_VEC << 12) + i) = *(u8 *)((u64)trampoline + i);
    }

    return 0;
}

/*
 * Get the CPU data structure
 */
struct p_data *
this_cpu(void)
{
    struct p_data *pdata;

    pdata = (struct p_data *)(P_DATA_BASE + lapic_id() * P_DATA_SIZE);

    return pdata;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
