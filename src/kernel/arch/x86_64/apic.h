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

#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

#include <aos/const.h>

#define APIC_MSR                0x1b
#define APIC_LAPIC_ID           0x020
#define APIC_SIVR               0x0f0
#define APIC_ICR_LOW            0x300
#define APIC_ICR_HIGH           0x310
#define APIC_LVT_TMR            0x320
#define APIC_TMRDIV             0x3e0
#define APIC_INITTMR            0x380
#define APIC_CURTMR             0x390

#define IOAPIC_VBASE            0xfec00000ULL
#define IOAPIC_VSIZE            0x00001000ULL

#define APIC_LVT_DISABLE        0x10000
#define APIC_LVT_ONESHOT        0x00000000
#define APIC_LVT_PERIODIC       0x00020000
#define APIC_LVT_TSC_DEADLINE   0x00040000

#define APIC_TMRDIV_X1          0xb
#define APIC_TMRDIV_X2          0x0
#define APIC_TMRDIV_X4          0x1
#define APIC_TMRDIV_X8          0x2
#define APIC_TMRDIV_X16         0x3
#define APIC_TMRDIV_X32         0x8
#define APIC_TMRDIV_X64         0x9
#define APIC_TMRDIV_X128        0xa
#define APIC_FREQ_PROBE         100000

void lapic_init(void);
u64 lapic_base_addr(void);
void lapic_send_init_ipi(void);
void lapic_send_startup_ipi(u8);
void lapic_send_fixed_ipi(u8);
int lapic_id(void);
u64 lapic_estimate_freq(void);
void lapic_start_timer(u64, u8);
void lapic_stop_timer(void);
void ioapic_init(void);
void ioapic_map_intr(u64, u64, u64);

#endif /* _KERNEL_APIC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
