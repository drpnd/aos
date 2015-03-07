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
#include "acpi.h"

u64 acpi_ioapic_base;
u64 acpi_pm_tmr_port;
u8 acpi_pm_tmr_ext;
u32 acpi_pm1a_ctrl_block;
u32 acpi_pm1b_ctrl_block;
u16 acpi_slp_typa;
u16 acpi_slp_typb;
u32 acpi_smi_cmd_port;
u8 acpi_enable_val;
u8 acpi_cmos_century;

/*
 * Compute ACPI checksum
 */
int
acpi_checksum(u8 *ptr, unsigned int len)
{
    u8 sum = 0;
    unsigned int i;

    for ( i = 0; i < len; i++ ) {
        sum += ptr[i];
    }

    return sum;
}

/*
 * Memcmp
 */
static int
_memcmp(const u8 *a, const u8 *b, int len)
{
    int i;

    for ( i = 0; i < len; i++ ) {
        if ( a[i] != b[i] ) {
            return a[i] - b[i];
        }
    }

    return 0;
}

/*
 * APIC
 *   0: Processor Local APIC
 *   1: I/O APIC
 *   2: Interrupt Source Override
 */
int
acpi_parse_apic(struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_apic *apic;
    struct acpi_sdt_apic_hdr *hdr;
    struct acpi_sdt_apic_lapic *lapic;
    struct acpi_sdt_apic_ioapic *ioapic;
    u32 len;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr);
    apic = (struct acpi_sdt_apic *)(addr + len);
    len += sizeof(struct acpi_sdt_apic);
    (void)apic;

    while ( len < sdt->length ) {
        hdr = (struct acpi_sdt_apic_hdr *)(addr + len);
        if ( len + hdr->length > sdt->length ) {
            /* Invalid */
            return -1;
        }
        switch  ( hdr->type ) {
        case 0:
            /* Local APIC */
            lapic = (struct acpi_sdt_apic_lapic *)hdr;
            (void)lapic;
            break;
        case 1:
            /* I/O APIC */
            ioapic = (struct acpi_sdt_apic_ioapic *)hdr;
            acpi_ioapic_base = ioapic->addr;
            break;
        case 2:
            /* Interrupt Source Override */
            break;
        default:
            /* Other */
            ;
        }
        len += hdr->length;
    }

    return 0;
}

int
acpi_parse_fadt(struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_fadt *fadt;
    u32 len;
    u64 dsdt;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr);
    fadt = (struct acpi_sdt_fadt *)(addr + len);

    if ( sdt->revision >= 3 ) {
        /* FADT revision 2.0 or higher */
        if ( fadt->x_pm_timer_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm_tmr_port = fadt->x_pm_timer_block.addr;
            if ( !acpi_pm_tmr_port ) {
                acpi_pm_tmr_port = fadt->pm_timer_block;
            }
        }

        /* PM1a control block */
        if ( fadt->x_pm1a_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm1a_ctrl_block = fadt->x_pm1a_ctrl_block.addr;
            if ( !acpi_pm1a_ctrl_block ) {
                acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;
            }
        }

        /* PM1b control block */
        if ( fadt->x_pm1b_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi_pm1b_ctrl_block = fadt->x_pm1b_ctrl_block.addr;
            if ( !acpi_pm1b_ctrl_block ) {
                acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;
            }
        }

        /* DSDT */
        dsdt = fadt->x_dsdt;
        if ( !dsdt ) {
            dsdt = fadt->dsdt;
        }
    } else {
        /* Revision  */
        acpi_pm_tmr_port = fadt->pm_timer_block;

        /* PM1a control block  */
        acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;

        /* PM1b control block  */
        acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;

        /* DSDT */
        dsdt = fadt->dsdt;
    }

    /* Check flags */
    acpi_pm_tmr_ext = (fadt->flags >> 8) & 0x1;

    /* SMI command */
    acpi_smi_cmd_port = fadt->smi_cmd_port;

    /* ACPI enable */
    acpi_enable_val = fadt->acpi_enable;

    /* Century */
    acpi_cmos_century = fadt->century;

    /* Ignore DSDT */

    return 0;
}


/*
 * Parse Root System Description Table (RSDT/XSDT) in RSDP
 */
int
acpi_parse_rsdt(struct acpi_rsdp *rsdp)
{
    struct acpi_sdt_hdr *rsdt;
    int i;
    int nr;
    int sz;

    if ( rsdp->revision >= 1 ) {
        /* ACPI 2.0 or later */
        sz = 8;
        rsdt = (struct acpi_sdt_hdr *)rsdp->xsdt_addr;
        if ( 0 != _memcmp((u8 *)rsdt->signature, (u8 *)"XSDT ", 4) ) {
            return -1;
        }
    } else {
        /* Parse RSDT (ACPI 1.x) */
        sz = 4;
        rsdt = (struct acpi_sdt_hdr *)(u64)rsdp->rsdt_addr;
        if ( 0 != _memcmp((u8 *)rsdt->signature, (u8 *)"RSDT ", 4) ) {
            return -1;
        }
    }
    nr = (rsdt->length - sizeof(struct acpi_sdt_hdr)) / sz;
    for ( i = 0; i < nr; i++ ) {
        u64 xx;
        if ( 4 == sz ) {
            xx = *(u32 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        } else {
            xx = *(u64 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        }
        struct acpi_sdt_hdr *tmp = (struct acpi_sdt_hdr *)xx;
        if ( 0 == _memcmp((u8 *)tmp->signature, (u8 *)"APIC", 4) ) {
            /* APIC */
            acpi_parse_apic(tmp);
        } else if ( 0 == _memcmp((u8 *)tmp->signature, (u8 *)"FACP", 4) ) {
            /* FADT */
            acpi_parse_fadt(tmp);
        }
    }

    return 0;
}

/*
 * Search Root System Description Pointer (RSDP) in ACPI data structure
 */
int
acpi_rsdp_search_range(u64 start, u64 end)
{
    u64 addr;
    struct acpi_rsdp *rsdp;

    for ( addr = start; addr < end; addr += 0x10 ) {
        /* Check the checksum of the RSDP */
        if ( 0 == acpi_checksum((u8 *)addr, 20) ) {
            /* Checksum is correct, then check the signature. */
            rsdp = (struct acpi_rsdp *)addr;
            if ( 0 == _memcmp((u8 *)rsdp->signature, (u8 *)"RSD PTR ", 8) ) {
                /* This seems to be a valid RSDP, then parse RSDT. */
                acpi_parse_rsdt(rsdp);
                return 1;
            }
        }
    }

    return 0;
}

/*
 * Parse Root System Descriptor Pointer (RSDP) in ACPI data structure
 */
int
acpi_load(void)
{
    u16 ebda;
    u64 ebda_addr;

    /* Check 1KB of EBDA, first */
    ebda = *(u16 *)0x040e;
    if ( ebda ) {
        ebda_addr = (u64)ebda << 4;
        if ( acpi_rsdp_search_range(ebda_addr, ebda_addr + 0x0400) ) {
            return 1;
        }
    }

    /* If not found in the EDBA, check main BIOS area */
    return acpi_rsdp_search_range(0xe0000, 0x100000);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
