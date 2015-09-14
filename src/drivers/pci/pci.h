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

#ifndef _PCI_H
#define _PCI_H

#include <stdint.h>

/*
 * PCI configuration space
 */
struct pci_dev_conf {
    uint16_t bus;
    uint16_t slot;
    uint16_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t intr_pin;   /* 0x01: INTA#, 0x02: INTB#, 0x03: INTC#: 0x04: INTD# */
    uint8_t intr_line;  /* 0xff: No connection */
    uint8_t class;
    uint8_t subclass;
    uint8_t progif;
    uint8_t revision;
};

/*
 * PCI device
 */
struct pci_dev {
    struct pci_dev_conf *device;
    struct pci_dev *next;
};

uint16_t pci_read_config(uint16_t, uint16_t, uint16_t, uint16_t);
uint64_t pci_read_mmio(uint8_t, uint8_t, uint8_t);
uint32_t pci_read_rom_bar(uint8_t, uint8_t, uint8_t);
uint8_t pci_get_header_type(uint16_t, uint16_t, uint16_t);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
