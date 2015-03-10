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
#include "desc.h"
#include "acpi.h"
#include "i8254.h"
#include "apic.h"

static int _load_trampoline(void);

/*
 * Initialize the bootstrap processor
 */
void
bsp_init(void)
{
    u16 *video;
    struct acpi acpi;

    /* Ensure the i8254 timer is stopped */
    i8254_stop_timer();

    /* Load ACPI: In this function, we need to get the following values.
       - I/O APIC base address
       - Power management timer variables
       - CMOS century
    */
    acpi_load(&acpi);

    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Display a mark to notify me that this code is properly executed */
    video = (u16 *)0xb8000;
    *(video + lapic_id()) = 0x0700 | '*';

    /* Load trampoline code */
    _load_trampoline();

    /* Send INIT IPI */
    lapic_send_init_ipi();

    /* Wait 10 ms */
    acpi_busy_usleep(&acpi, 10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&acpi, 200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&acpi, 200);
}

/*
 * Initialize the application processor
 */
void
ap_init(void)
{
    u16 *video;

    video = (u16 *)0xb8000;
    *(video + lapic_id()) = 0x0700 | '*';
}

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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
