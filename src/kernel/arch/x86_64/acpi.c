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
#include "acpi.h"

/* BIOS data area (BDA): 0x0400--0x04ff; if extended BDA (EDBA) presents, its
   address >> 4 (16 bit) is stored in 0x040e. */
#define BDA_EDBA        0x040e

/* Prototype declarations */
static int _validate_checksum(const u8 *, int);
static int _parse_apic(struct acpi *, struct acpi_sdt_hdr *);
static int _parse_fadt(struct acpi *, struct acpi_sdt_hdr *);
static int _parse_rsdt(struct acpi *, struct acpi_rsdp *);
static int _rsdp_search_range(struct acpi *, u64, u64);


/*
 * Validate ACPI checksum: Since the ACPI checksum is a one-byte modular sum,
 * this function calculates the sum of len bytes from the memory space pointed
 * by ptr.  If the checksum is valid, this function returns zero.  Otherwise,
 * this returns a non-zero value.
 */
static int
_validate_checksum(const u8 *ptr, int len)
{
    u8 sum = 0;
    int i;

    for ( i = 0; i < len; i++ ) {
        sum += ptr[i];
    }

    return sum;
}


/*
 * Check if the ACPI timer is available
 */
int
acpi_timer_available(struct acpi *acpi)
{
    if ( 0 == acpi->acpi_pm_tmr_port ) {
        return -1;
    }

    return 0;
}

/*
 * Get the current ACPI timer.  Note that the caller must check the
 * availability of the ACPI timer through the acpi_timer_available() function
 * before calling this function.
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
        /* 32-bit counter */
        return ((u64)1ULL << 32);
    } else {
        /* 24-bit counter */
        return (1 << 24);
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
 * Wait usec microseconds using ACPI timer.  Note that the caller must check
 * the availability of the ACPI timer through the acpi_timer_available()
 * function before calling this function.
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

    /* Check flags: The eighth bit of fadt->flags presents the TMR_VAL_EXT flag.
       If this flag is clear, the counter of the timer is implemented as a
       24-bit value.  Otherwise, it is implemented as a 32-bit value. */
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
 * Parse ACPI Static Resource Affinity Table (SRAT)
 */
static int
_parse_srat(struct acpi *acpi, struct acpi_sdt_hdr *sdt)
{
    u64 addr;
    struct acpi_sdt_srat_common *srat;
    struct acpi_sdt_srat_lapic *srat_lapic;
    struct acpi_sdt_srat_memory *srat_memory;
    struct acpi_sdt_srat_lapicx2 *srat_lapicx2;
    u32 len;

    len = 0;
    addr = (u64)sdt;
    len += sizeof(struct acpi_sdt_hdr) + sizeof(struct acpi_sdt_srat_hdr);

    while ( len < sdt->length ) {
        srat = (struct acpi_sdt_srat_common *)(addr + len);
        if ( len + srat->length > sdt->length ) {
            /* Oversized */
            break;
        }
        switch ( srat->type ) {
        case 0:
            /* Local APIC */
            srat_lapic = (struct acpi_sdt_srat_lapic *)srat;
            (void)srat_lapic;
            break;
        case 1:
            /* Memory */
            srat_memory = (struct acpi_sdt_srat_memory *)srat;
            (void)srat_memory;
            break;
        case 2:
            /* Local x2APIC */
            srat_lapicx2 = (struct acpi_sdt_srat_lapicx2 *)srat;
            (void)srat_lapicx2;
            break;
        default:
            /* Unknown */
            ;
        }

        /* Next entry */
        len += srat->length;
    }

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
        if ( 0 != kmemcmp((u8 *)rsdt->signature, (u8 *)"XSDT", 4) ) {
            return 0;
        }
    } else {
        /* Parse RSDT (ACPI 1.x) */
        sz = 4;
        rsdt = (struct acpi_sdt_hdr *)(u64)rsdp->rsdt_addr;
        if ( 0 != kmemcmp((u8 *)rsdt->signature, (u8 *)"RSDT", 4) ) {
            return 0;
        }
    }
    /* Compute the number of SDTs */
    nr = (rsdt->length - sizeof(struct acpi_sdt_hdr)) / sz;
    /* Check all SDTs */
    for ( i = 0; i < nr; i++ ) {
        if ( 4 == sz ) {
            addr = *(u32 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        } else {
            addr = *(u64 *)((u64)(rsdt) + sizeof(struct acpi_sdt_hdr) + i * sz);
        }
        tmp = (struct acpi_sdt_hdr *)addr;
        if ( 0 == kmemcmp((u8 *)tmp->signature, (u8 *)"APIC", 4) ) {
            /* APIC */
            if ( !_parse_apic(acpi, tmp) ) {
                return 0;
            }
        } else if ( 0 == kmemcmp((u8 *)tmp->signature, (u8 *)"FACP", 4) ) {
            /* FADT */
            if ( !_parse_fadt(acpi, tmp) ) {
                return 0;
            }
        } else if ( 0 == kmemcmp((u8 *)tmp->signature, (u8 *)"SRAT ", 4) ) {
            /* SRAT */
            acpi->srat = tmp;
            if ( !_parse_srat(acpi, tmp) ) {
                return 0;
            }
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
        if ( 0 == _validate_checksum((u8 *)addr, 20) ) {
            /* Checksum is correct, then check the signature. */
            rsdp = (struct acpi_rsdp *)addr;
            if ( 0 == kmemcmp((u8 *)rsdp->signature, (u8 *)"RSD PTR ", 8) ) {
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

    /* Reset the data structure */
    kmemset(acpi, 0, sizeof(struct acpi));

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
 * Test if the platform is running on NUMA architecture
 */
int
acpi_is_numa(struct acpi *acpi)
{
    /* Check the pointer to the SRAT */
    if ( NULL == acpi->srat ) {
        /* NUMA is enabled. */
        return 0;
    }

    /* NUMA is disabled or unsupported. */
    return 1;
}

/*
 * Resolve the proximity domain of a local APIC
 */
int
acpi_lapic_prox_domain(struct acpi *acpi, int apicid)
{
    u64 addr;
    struct acpi_sdt_srat_common *srat;
    struct acpi_sdt_srat_lapic *srat_lapic;
    u32 len;

    /* Check the pointer to the SRAT */
    if ( NULL == acpi->srat ) {
        return 0;
    }

    len = 0;
    addr = (u64)acpi->srat;
    len += sizeof(struct acpi_sdt_hdr) + sizeof(struct acpi_sdt_srat_hdr);

    while ( len < acpi->srat->length ) {
        srat = (struct acpi_sdt_srat_common *)(addr + len);
        if ( len + srat->length > acpi->srat->length ) {
            /* Oversized */
            break;
        }
        switch ( srat->type ) {
        case 0:
            /* Local APIC */
            srat_lapic = (struct acpi_sdt_srat_lapic *)srat;
            if ( srat_lapic->apic_id == apicid ) {
                return (u32)srat_lapic->proximity_domain
                    | ((u32)srat_lapic->proximity_domain2[0] << 8)
                    | ((u32)srat_lapic->proximity_domain2[1] << 8)
                    | ((u32)srat_lapic->proximity_domain2[2] << 8);
            }
            break;
        default:
            /* Other or unknown */
            ;
        }

        /* Next entry */
        len += srat->length;
    }

    return -1;
}

/*
 * Resolve the proximity domain of the physical memory address
 */
int
acpi_memory_prox_domain(struct acpi *acpi, u64 m, u64 *rbase, u64 *rlen)
{
    u64 addr;
    struct acpi_sdt_srat_common *srat;
    struct acpi_sdt_srat_memory *srat_memory;
    u32 len;
    u64 tbase;
    u64 tlen;

    /* Check the pointer to the SRAT */
    if ( NULL == acpi->srat ) {
        return 0;
    }

    len = 0;
    addr = (u64)acpi->srat;
    len += sizeof(struct acpi_sdt_hdr) + sizeof(struct acpi_sdt_srat_hdr);

    while ( len < acpi->srat->length ) {
        srat = (struct acpi_sdt_srat_common *)(addr + len);
        if ( len + srat->length > acpi->srat->length ) {
            /* Oversized */
            break;
        }
        switch ( srat->type ) {
        case 1:
            /* Memory */
            srat_memory = (struct acpi_sdt_srat_memory *)srat;
            tbase = (u64)srat_memory->base_addr_low |
                ((u64)srat_memory->base_addr_high << 32);
            tlen = (u64)srat_memory->length_low |
                ((u64)srat_memory->length_high << 32);
            if ( m >= tbase && m < tbase + tlen ) {
                *rbase = tbase;
                *rlen = tlen;
                return srat_memory->proximity_domain;
            }
            break;
        default:
            /* Other or unknown */
            ;
        }

        /* Next entry */
        len += srat->length;
    }

    return -1;
}

/*
 * Count the number of entries of memory domains from ACPI SRAT
 */
int
acpi_memory_count_entries(struct acpi *acpi)
{
    u64 addr;
    struct acpi_sdt_srat_common *srat;
    u32 len;
    int n;

    /* Check the pointer to the SRAT */
    if ( NULL == acpi->srat ) {
        return -1;
    }

    /* Clear */
    n = 0;
    len = 0;

    /* Entry point for the SRAT */
    addr = (u64)acpi->srat;
    len += sizeof(struct acpi_sdt_hdr) + sizeof(struct acpi_sdt_srat_hdr);

    /* Look through the SRAT for memory proximity domain entries */
    while ( len < acpi->srat->length ) {
        srat = (struct acpi_sdt_srat_common *)(addr + len);
        if ( len + srat->length > acpi->srat->length ) {
            /* Oversized */
            break;
        }
        switch ( srat->type ) {
        case 1:
            /* Memory */
            n++;
            break;
        default:
            /* Other or unknown */
            ;
        }
        /* Next entry */
        len += srat->length;
    }

    return n;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
