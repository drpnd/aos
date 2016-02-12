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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <machine/sysarch.h>
#include "pci.h"

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

/*
 * Read PCI configuration
 */
uint16_t
pci_read_config(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset)
{
    uint32_t addr;
    struct sysarch_io io;

    addr = ((uint32_t)bus << 16) | ((uint32_t)slot << 11)
        | ((uint32_t)func << 8) | ((uint32_t)offset & 0xfc);
    /* Set enable bit */
    addr |= (uint32_t)0x80000000;

    io.port = PCI_CONFIG_ADDR;
    io.data = addr;
    sysarch(SYSARCH_OUTL, &io);
    io.port = PCI_CONFIG_DATA;
    sysarch(SYSARCH_INL, &io);

    return (io.data >> ((offset & 2) * 8)) & 0xffff;
}

/*
 * Read memory mapped I/O (MMIO) base address from BAR0
 */
uint64_t
pci_read_mmio(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint64_t addr;
    uint32_t bar0;
    uint32_t bar1;
    uint8_t type;
    uint8_t prefetchable;

    bar0 = pci_read_config(bus, slot, func, 0x10);
    bar0 |= (uint32_t)pci_read_config(bus, slot, func, 0x12) << 16;

    type = (bar0 >> 1) & 0x3;
    prefetchable = (bar0 >> 3) & 0x1;
    (void)prefetchable;
    addr = bar0 & 0xfffffff0;

    if ( 0x00 == type ) {
        /* 32bit */
    } else if ( 0x02 == type ) {
        /* 64bit */
        bar1 = pci_read_config(bus, slot, func, 0x14);
        bar1 |= (uint32_t)pci_read_config(bus, slot, func, 0x16) << 16;
        addr |= ((uint64_t)bar1) << 32;
    } else {
        return 0;
    }

    return addr;
}

/*
 * Read ROM BAR
 */
uint32_t
pci_read_rom_bar(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint8_t type;
    uint32_t bar;

    type = pci_get_header_type(bus, slot, func);
    if ( 0x00 == type ) {
        bar = pci_read_config(bus, slot, func, 0x30);
        bar |= (uint32_t)pci_read_config(bus, slot, func, 0x32) << 16;
    } else if ( 0x00 == type ) {
        bar = pci_read_config(bus, slot, func, 0x38);
        bar |= (uint32_t)pci_read_config(bus, slot, func, 0x3a) << 16;
    } else {
        bar = 0;
    }

    return bar;
}

/*
 * Get header type
 */
uint8_t
pci_get_header_type(uint16_t bus, uint16_t slot, uint16_t func)
{
    return pci_read_config(bus, slot, func, 0x0e) & 0xff;
}

/*
 * Check function
 */
void
pci_check_function(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint16_t vendor;
    uint16_t device;
    uint16_t reg;
    uint16_t class;
    uint16_t prog;
    struct pci_dev_conf *conf;
    struct pci_dev *dev;

    vendor = pci_read_config(bus, slot, func, 0);
    device = pci_read_config(bus, slot, func, 2);

    /* Allocate the memory space for a PCI device */
    dev = malloc(sizeof(struct pci_dev));
    if ( NULL == dev ) {
        /* Memory error */
        return;
    }
    conf = malloc(sizeof(struct pci_dev_conf));
    if ( NULL == conf ) {
        /* Memory error */
        free(dev);
        return;
    }

    /* Read interrupt pin and line */
    reg = pci_read_config(bus, slot, func, 0x3c);

    /* Read class and subclass */
    class = pci_read_config(bus, slot, func, 0x0a);

    /* Read program interface and revision ID */
    prog = pci_read_config(bus, slot, func, 0x08);

    conf->bus = bus;
    conf->slot = slot;
    conf->func = func;
    conf->vendor_id = vendor;
    conf->device_id = device;
    conf->intr_pin = (uint8_t)(reg >> 8);
    conf->intr_line = (uint8_t)(reg & 0xff);
    conf->class = (uint8_t)(class >> 8);
    conf->subclass = (uint8_t)(class & 0xff);
    conf->progif = (uint8_t)(prog >> 8);
    conf->revision = (uint8_t)(prog & 0xff);
    dev->device = conf;
    dev->next = NULL;
}

/*
 * Check a specified device
 */
void
pci_check_device(uint8_t bus, uint8_t device)
{
    uint16_t vendor;
    uint8_t func;
    uint8_t hdr_type;

    func = 0;
    vendor = pci_read_config(bus, device, func, 0);
    if ( 0xffff == vendor ) {
        return;
    }

    pci_check_function(bus, device, func);
    hdr_type = pci_get_header_type(bus, device, func);

    if ( hdr_type & 0x80 ) {
        /* Multiple functions */
        for ( func = 1; func < 8; func++ ) {
            vendor = pci_read_config(bus, device, func, 0);
            if ( 0xffff != vendor ) {
                pci_check_function(bus, device, func);
            }
         }
    }
}

/*
 * Check a specified bus
 */
void
pci_check_bus(uint8_t bus)
{
    uint8_t device;

    for ( device = 0; device < 32; device++ ) {
        pci_check_device(bus, device);
    }
}

/*
 * Check all PCI buses
 */
void
pci_check_all_buses(void)
{
    uint16_t bus;
    uint16_t func;
    uint16_t hdr_type;
    uint16_t vendor;

    hdr_type = pci_get_header_type(0, 0, 0);
    if ( !(hdr_type & 0x80) ) {
        /* Single PCI host controller */
        for ( bus = 0; bus < 256; bus++ ) {
            pci_check_bus(bus);
        }
    } else {
        /* Multiple PCI host controllers */
        for ( func = 0; func < 8; func++ ) {
            vendor = pci_read_config(0, 0, func, 0);
            if ( 0xffff != vendor ) {
                break;
            }
            bus = func;
            pci_check_bus(bus);
        }
    }
}

/*
 * Initialize PCI driver
 */
void
pci_init(void)
{
    /* Search all PCI devices */
    pci_check_all_buses();
}

/*
 * Entry point for the PCI driver
 */
int
main(int argc, char *argv[])
{
    while ( 1 ) {
        write(0, NULL, 0);
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
