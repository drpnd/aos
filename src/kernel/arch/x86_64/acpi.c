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

/* BIOS data area (BDA): 0x0400--0x04ff; if extended BDA (EDBA) presents, its
   address >> 4 (16 bit) is stored in 0x040e. */
#define BDA_EDBA        0x040e

/* Prototype declarations */
static int _checksum(u8 *, int);
static int _memcmp(const u8 *, const u8 *, int);
static int _parse_apic(struct acpi *, struct acpi_sdt_hdr *);
static int _parse_fadt(struct acpi *, struct acpi_sdt_hdr *);
static int _parse_rsdt(struct acpi *, struct acpi_rsdp *);
static int _rsdp_search_range(struct acpi *, u64, u64);


/*
 * Compute ACPI checksum
 */
static int
_checksum(u8 *ptr, int len)
{
    u8 sum = 0;
    int i;

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
 * Get the current ACPI timer
 */
u32
acpi_get_timer(struct acpi *acpi)
{
    return inl(acpi->acpi_pm_tmr_port);
}

/*
 * Get the timer period (wrapping)
 */
u64
acpi_get_timer_period(struct acpi *acpi)
{
    if ( acpi->acpi_pm_tmr_ext ) {
        return ((u64)1<<32);
    } else {
        return (1<<24);
    }
}
/*
 * Get the frequency of the ACPI timer
 */
u64
acpi_get_timer_hz(void)
{
    return ACPI_TMR_HZ;
}

/*
 * Wait
 */
void
acpi_busy_usleep(struct acpi *acpi, u64 usec)
{
    u64 clk;
    volatile u64 acc;
    volatile u64 cur;
    volatile u64 prev;

    /* usec to count */
    clk = (ACPI_TMR_HZ * usec) / 1000000;

    prev = acpi_get_timer(acpi);
    acc = 0;
    while ( acc < clk ) {
        cur = acpi_get_timer(acpi);
        if ( cur < prev ) {
            /* Overflow */
            acc += acpi_get_timer_period(acpi) + cur - prev;
        } else {
            acc += cur - prev;
        }
        prev = cur;
        pause();
    }
}

/*
 * APIC
 *   0: Processor Local APIC
 *   1: I/O APIC
 *   2: Interrupt Source Override
 */
static int
_parse_apic(struct acpi *acpi, struct acpi_sdt_hdr *sdt)
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
            return 0;
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
            acpi->acpi_ioapic_base = ioapic->addr;
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

    return 1;
}

/*
 * Parse Fixed ACPI Description Table (FADT)
 */
static int
_parse_fadt(struct acpi *acpi, struct acpi_sdt_hdr *sdt)
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
            acpi->acpi_pm_tmr_port = fadt->x_pm_timer_block.addr;
            if ( !acpi->acpi_pm_tmr_port ) {
                acpi->acpi_pm_tmr_port = fadt->pm_timer_block;
            }
        }

        /* PM1a control block */
        if ( fadt->x_pm1a_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi->acpi_pm1a_ctrl_block = fadt->x_pm1a_ctrl_block.addr;
            if ( !acpi->acpi_pm1a_ctrl_block ) {
                acpi->acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;
            }
        }

        /* PM1b control block */
        if ( fadt->x_pm1b_ctrl_block.addr_space == 1 ) {
            /* Must be 1 (System I/O) */
            acpi->acpi_pm1b_ctrl_block = fadt->x_pm1b_ctrl_block.addr;
            if ( !acpi->acpi_pm1b_ctrl_block ) {
                acpi->acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;
            }
        }

        /* DSDT */
        dsdt = fadt->x_dsdt;
        if ( !dsdt ) {
            dsdt = fadt->dsdt;
        }
    } else {
        /* Revision  */
        acpi->acpi_pm_tmr_port = fadt->pm_timer_block;

        /* PM1a control block  */
        acpi->acpi_pm1a_ctrl_block = fadt->pm1a_ctrl_block;

        /* PM1b control block  */
        acpi->acpi_pm1b_ctrl_block = fadt->pm1b_ctrl_block;

        /* DSDT */
        dsdt = fadt->dsdt;
    }

    /* Check flags */
    acpi->acpi_pm_tmr_ext = (fadt->flags >> 8) & 0x1;

    /* SMI command */
    acpi->acpi_smi_cmd_port = fadt->smi_cmd_port;

    /* ACPI enable */
    acpi->acpi_enable_val = fadt->acpi_enable;

    /* Century */
    acpi->acpi_cmos_century = fadt->century;

    /* Ignore DSDT */

    return 1;
}


/*
 * Parse Root System Description Table (RSDT/XSDT) in RSDP
 */
static int
_parse_rsdt(struct acpi *acpi, struct acpi_rsdp *rsdp)
{
    struct acpi_sdt_hdr *rsdt;
    int i;
    int nr;
    int sz;
    u64 addr;
    struct acpi_sdt_hdr *tmp;

    if ( rsdp->revision >= 1 ) {
        /* ACPI 2.0 or later */
        sz = 8;
        rsdt = (struct acpi_sdt_hdr *)rsdp->xsdt_addr;
        if ( 0 != _memcmp((u8 *)rsdt->signature, (u8 *)"XSDT", 4) ) {
            return 0;
        }
    } else {
        /* Parse RSDT (ACPI 1.x) */
        sz = 4;
        rsdt = (struct acpi_sdt_hdr *)(u64)rsdp->rsdt_addr;
        if ( 0 != _memcmp((u8 *)rsdt->signature, (u8 *)"RSDT", 4) ) {
            return 0;
        }
    }
    nr = (rsdt->length - sizeof(struct acpi_sdt_hdr)) / sz;
    for ( i = 0; i < nr; i++ ) {
        if ( 4 == sz ) {
            addr = *(u32 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        } else {
            addr = *(u64 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        }
        tmp = (struct acpi_sdt_hdr *)addr;
        if ( 0 == _memcmp((u8 *)tmp->signature, (u8 *)"APIC", 4) ) {
            /* APIC */
            _parse_apic(acpi, tmp);
        } else if ( 0 == _memcmp((u8 *)tmp->signature, (u8 *)"FACP", 4) ) {
            /* FADT */
            _parse_fadt(acpi, tmp);
        }
    }

    return 1;
}

/*
 * Search Root System Description Pointer (RSDP) in ACPI data structure
 */
static int
_rsdp_search_range(struct acpi *acpi, u64 start, u64 end)
{
    u64 addr;
    struct acpi_rsdp *rsdp;

    for ( addr = start; addr < end; addr += 0x10 ) {
        /* Check the checksum of the RSDP */
        if ( 0 == _checksum((u8 *)addr, 20) ) {
            /* Checksum is correct, then check the signature. */
            rsdp = (struct acpi_rsdp *)addr;
            if ( 0 == _memcmp((u8 *)rsdp->signature, (u8 *)"RSD PTR ", 8) ) {
                /* This seems to be a valid RSDP, then parse RSDT. */
                return _parse_rsdt(acpi, rsdp);
            }
        }
    }

    return 0;
}

/*
 * Parse Root System Descriptor Pointer (RSDP) in ACPI data structure
 */
int
acpi_load(struct acpi *acpi)
{
    u16 ebda;
    u64 ebda_addr;

    /* Check 1KB of EBDA, first */
    ebda = *(u16 *)BDA_EDBA;
    if ( ebda ) {
        ebda_addr = (u64)ebda << 4;
        if ( _rsdp_search_range(acpi, ebda_addr, ebda_addr + 0x0400) ) {
            return 1;
        }
    }

    /* If not found in the EDBA, check main BIOS area */
    return _rsdp_search_range(acpi, 0xe0000, 0x100000);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
