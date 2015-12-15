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

#ifndef _KERNEL_ACPI_H
#define _KERNEL_ACPI_H

#include <aos/const.h>
#include "../../kernel.h"

#define ACPI_TMR_HZ 3579545
#define ACPI_SCI_EN 0x1
#define ACPI_SLP_EN (1<<13)

/*
 * Root System Descriptor Pointer (RSDP)
 */
struct acpi_rsdp {
    char signature[8];
    u8 checksum;
    char oemid[6];
    u8 revision;
    u32 rsdt_addr;
    /* the following values are introduced since 2.0 */
    u32 length;
    u64 xsdt_addr;
    u8 extended_checksum;
    char reserved[3];
} __attribute__ ((packed));

/*
 * System Description Table (SDT) header
 */
struct acpi_sdt_hdr {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oemid[6];
    char oemtableid[8];
    u32 oemrevision;
    u32 creatorid;
    u32 creatorrevision;
} __attribute__ ((packed));

/*
 * APIC
 */
struct acpi_sdt_apic {
    u32 local_controller_addr;
    u32 flags;
} __attribute__ ((packed));

/*
 * APIC header
 */
struct acpi_sdt_apic_hdr {
    u8 type; /* 0 = local APIC, 1 = I/O APIC */
    u8 length;
} __attribute__ ((packed));

/*
 * Local APIC
 */
struct acpi_sdt_apic_lapic {
    struct acpi_sdt_apic_hdr hdr;
    u8 cpu_id;
    u8 apic_id;
    u32 flags;
} __attribute__ ((packed));

/*
 * I/O APIC
 */
struct acpi_sdt_apic_ioapic {
    struct acpi_sdt_apic_hdr hdr;
    u8 id;
    u8 reserved;
    u32 addr;
    u32 global_int_base;
} __attribute__ ((packed));

/*
 * Interrupt Source Override
 */
struct acpi_sdt_apic_int_src {
    struct acpi_sdt_apic_hdr hdr;
    u8 bus;
    u8 bus_int;
    u32 global_int;
    u16 mps_flags;
} __attribute__ ((packed));

/*
 * Generic address structure of ACPI
 */
struct acpi_generic_addr_struct {
    u8 addr_space;
    u8 bit_width;
    u8 bit_offset;
    u8 access_size;
    u64 addr;
} __attribute__ ((packed));

/*
 * FADT
 */
struct acpi_sdt_fadt {
    /* acpi_sdt_hdr */
    u32 firmware_ctrl;
    u32 dsdt;

    u8 reserved;

    u8 preferred_pm_profile;
    u16 sci_interrupt;
    u32 smi_cmd_port;
    u8 acpi_enable;
    u8 acpi_disable;
    u8 s4bios_req;
    u8 pstate_ctrl;
    u32 pm1a_event_block;
    u32 pm1b_event_block;
    u32 pm1a_ctrl_block;
    u32 pm1b_ctrl_block;
    u32 pm2_ctrl_block;
    u32 pm_timer_block;
    u32 gpe0_block;
    u32 gpe1_block;
    u8 pm1_event_length;
    u8 pm1_ctrl_length;
    u8 pm2_ctrl_length;
    u8 pm_timer_length;
    u8 gpe0_length;
    u8 gpe1_length;
    u8 gpe1_base;
    u8 cstate_ctrl;
    u16 worst_c2_latency;
    u16 worst_c3_latency;
    u16 flush_size;
    u16 flush_stride;
    u8 duty_offset;
    u8 duty_width;
    u8 day_alarm;
    u8 month_alarm;
    u8 century;

    u16 boot_arch_flags;

    u8 reserved2;
    u32 flags;

    struct acpi_generic_addr_struct reset_reg;
    u8 reset_value;

    u8 reserved3[3];

    u64 x_firmware_ctrl;
    u64 x_dsdt;

    struct acpi_generic_addr_struct x_pm1a_event_block;
    struct acpi_generic_addr_struct x_pm1b_event_block;
    struct acpi_generic_addr_struct x_pm1a_ctrl_block;
    struct acpi_generic_addr_struct x_pm1b_ctrl_block;
    struct acpi_generic_addr_struct x_pm2_ctrl_block;
    struct acpi_generic_addr_struct x_pm_timer_block;
    struct acpi_generic_addr_struct x_gpe0_block;
    struct acpi_generic_addr_struct x_gpe1_block;

} __attribute__ ((packed));

/*
 * SRAT
 * - acpi_sdt_hdr
 * - reserved[4+8]
 * - Static Resource Allocation Structure[n]
 */
struct acpi_sdt_srat_common {
    u8 type;
    u8 length;
} __attribute__ ((packed));
struct acpi_sdt_srat_lapic {
    u8 type;                    /* 0: Local APIC */
    u8 length;                  /* 16 */
    u8 proximity_domain;
    u8 apic_id;
    u32 flags;
    u8 local_sapic_eid;
    u8 proximity_domain2[3];
    u32 clock_domain;
} __attribute__ ((packed));
struct acpi_sdt_srat_memory {
    u8 type;                    /* 1: Memory */
    u8 length;                  /* 40 */
    u32 proximity_domain;
    u8 reserved1[2];
    u32 base_addr_low;
    u32 base_addr_high;
    u32 length_low;
    u32 length_high;
    u32 reserved2;
    u32 flags;
    u8 reserved3[8];
} __attribute__ ((packed));
struct acpi_sdt_srat_lapicx2 {
    u8 type;                    /* 2: Local x2APIC */
    u8 length;                  /* 24 */
    u16 reserved1;
    u32 proximity_domain;
    u32 x2apic_id;
    u32 flags;
    u32 clock_domain;
    u32 reserved2;
} __attribute__ ((packed));
struct acpi_sdt_srat_hdr {
    /* acpi_sdt_hdr */
    u8 reserved1[4];
    u8 reserved2[8];
    /* acpi_sdt_srat_*[n] */
} __attribute__ ((packed));

/*
 * ACPI configuration
 */
struct acpi {
    u64 acpi_ioapic_base;
    u64 acpi_pm_tmr_port;
    u8 acpi_pm_tmr_ext;
    u32 acpi_pm1a_ctrl_block;
    u32 acpi_pm1b_ctrl_block;
    u32 acpi_smi_cmd_port;
    u8 acpi_enable_val;
    u8 acpi_cmos_century;
    /* SRAT */
    struct acpi_sdt_hdr *srat;
};

int acpi_load(struct acpi *);
void acpi_busy_usleep(struct acpi *, u64);
int acpi_timer_available(struct acpi *);
u32 acpi_get_timer(struct acpi *);
u64 acpi_get_timer_period(struct acpi *);
u64 acpi_get_timer_hz(void);

int acpi_is_numa(struct acpi *);
int acpi_lapic_prox_domain(struct acpi *, int);
int acpi_memory_prox_domain(struct acpi *, u64, u64 *, u64 *);
int acpi_memory_count_entries(struct acpi *);

extern struct acpi arch_acpi;

#endif /* _KERNEL_ACPI_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
