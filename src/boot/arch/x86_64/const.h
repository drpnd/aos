/*_
 * Copyright (c) 2016 Hirochika Asai <asai@jar.jp>
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

#ifndef _CONST_H
#define _CONST_H

#define KERNEL_BASE     0x10000
#define INITRAMFS_BASE  0x30000
#define KERNEL_PGT      0x00079000      /* Page table */
#define P_DATA_SIZE     0x10000         /* Data size for each processor */
#define P_DATA_BASE     0x1000000       /* Data base for each processor */
#define P_STACK_GUARD   0x10

#define BOOTINFO_BASE   0x8000          /* Boot information base address */
#define BOOTINFO_SIZE   0x100           /* Size of boot info structure */

#define MSR_APIC_BASE   0x1b

#define APIC_BASE_MASK  0xfffffffffffff000
#define LAPIC_ID_OFF    0x020
#define LAPIC_ID_SHR    24

#define BOOTINFO_MM_NUM BOOTINFO_BASE
#define BOOTINFO_MM_PTR (BOOTINFO_BASE + 8)
#define BOOTINFO_MM_TBL (BOOTINFO_BASE + BOOTINFO_SIZE)

#define NUM_RETRIES     3               /* # of retries for disk read */
#define ERRCODE_TIMEOUT 0x80            /* Error code: Timeout */
#define SECTOR_SIZE     0x200           /* 512 bytes / sector */
#define BUFFER          0x6000          /* Buffer: 6000-61ff */

/* I/O port */

/* i8259 */
#define IO_PIC1_CMD     0x20
#define IO_PIC1_DATA    0x21
#define IO_PIC2_CMD     0xa0
#define IO_PIC2_DATA    0xa1

/* Flags */
#define EFLAGS_CPUID    21

/* CPUID features */

/* CPUID leaves > 3 < 80000000 are visible only
   when IA32_MISC_ENABLE.BOOT_NT4[bit 22] = 0 (default). */
/*
 * For Intel processors
 * EAX=00H
 *      EAX: Maximum Input Value for Basic CPUID Information
 *      EBX: "Genu"
 *      ECX: "ntel"
 *      EDX: "ineI"
 *
 * EAX=01H
 *      EAX: Version Information: Type, Family, Model, and Stepping ID
 *      EBX: Bits 07-00: Brand Index
 *           Bits 15-08: CLFLUSH line size (* 8 = cache line size in bytes)
 *           Bits 23-16: Maximum number of addressable IDs for logical
 *                       processors in this physical package.
 *           Bits 31-24: Initial APIC ID
 *      ECX: Feature Information
 *      EDX: Feature Information
 * EAX=02H
 *      EAX: Cache and TLB Information
 *      EBX: Cache and TLB Information
 *      ECX: Cache and TLB Information
 *      EDX: Cache and TLB Information
 * EAX=03H
 *      EAX: Reserved
 *      EBX: Reserved
 *      ECX: Bits 00-31 of 96 bit processor serial number (Pentium III only)
 *      EDX: Bits 32-63 of 96 bit processor serial number (Pentium III only)
 * EAX=04H
 *      EAX: Bits 04-00: Cache Type Field
 *             0 = Null -- No more caches
 *             1 = Data Cache
 *             2 = Instruction cache
 *             3 = Unified Cache
 *             4-31 = Reserved
 *           Bits 07-05: Cache Level (starts at 1)
 *           Bit 08: Initializing cache level (does not need SW initialization)
 *           Bit 09: Fully Associative cache
 *           Bits 13-10: Reserved
 *           Bits 25-14: Maximum number of addresssable IDs for logical
 *                       processors sharing this cache
 *           Bits 31-16: Maximum number of addresssable IDs for processor cores
 *                       in the phyisical package
 *      EBX: Bits 11-00: L = System Coherency Line Size
 *           Bits 21-12: P = Physical Line partitions
 *           Bits 31-22: W = Ways of associativity
 *      ECX: Bits 00-31: S = Number of Sets
 *      EDX: Bit 0: Write-Back Invalidate/Invalidate
 *             0 = WBINVD/INVD from threads sharing this cache acts upon lower
 *                 level caches for threads sharing this cache.
 *             1 = WBINVD/INVD is not guaranteed to act upon lower level caches
 *                 of non-originating threads sharing this cache.
 *           Bit 1: Cache Inclusiveness
 *             0 = Cache is not inclusive of lower cache levels.
 *             1 = Cache is inclusive of lower cache levels.
 *           Bit 2: Complex Cache Indexing
 *             0 = Direct mapped cache.
 *             1 = A complex function is used to index the cache, potentially
 *                 using all address bits.
 *           Bits 31-3: Reserved = 0
 * EAX=05H
 *      EAX: Bits 15-00: Smallest monitor-line size in bytes (default is
 *                       processor's monitor granularity)
 *           Bits 31-16: Reserved = 0
 *      EBX: Bits 15-00: Largest monitor-line size in bytes (default is
 *                       processor's monitor granularity)
 *           Bits 31-16: Reserved = 0
 *      ECX: Bit 00: Enumeration of Monitor-Mwait extensions (beyond EAX and
 *                   EBX registers) supported
 *           Bit 01: Supports treating interrupts as break-event for MWAIT, even
 *                   when interrupts disabled
 *           Bits 31-02: Reserved
 *      EDX: Bits 03-00: Number of C0 sub C-states supported using MWAIT
 *           Bits 07-04: Number of C1 sub C-states supported using MWAIT
 *           Bits 11-08: Number of C2 sub C-states supported using MWAIT
 *           Bits 15-12: Number of C3 sub C-states supported using MWAIT
 *           Bits 19-16: Number of C4 sub C-states supported using MWAIT
 *           Bits 31-20: Reserved = 0
 * EAX=06H
 *      EAX: Bit 00: Digital temperature sensor is supported if set
 *           Bit 01: Intel Turbo Boost Technology Available
 *           Bit 02: ARAT. APIC-Timer-always-running feature is supported if set
 *           Bit 03: Reserved
 *           Bit 04: PLN. Power limit notification controls are supported if set
 *           Bit 05: ECMD. Clock modulation duty cycle extension is supported if
 *                   set
 *           Bit 06: PTM. Package thermal management is supported if set
 *           Bit 31-07: Reserved
 *      EBX: Bits 03-00: Number of Interrupt Thresholds in Digital Thermal
 *                       Sensor
 *           Bits 31-04: Reserved
 *      ECX: Bit 00: Hardware Coordination Feedback Capability
 *           Bits 02-01: Reserved
 *           Bit 03: The processor supports performance-energy bias preference
 *                   if CPUID.06H:ECX.SETBH[bit 3] is set and it also implies
 *                   the presence of a new architectural MSR called
 *                   IA32_ENERGY_PERF_BIAS (1B0H)
 *           Bits 31-04: Reserved
 *      EDX: Reserved = 0
 * EAX=07H / ECX=00H
 *      EAX: Bits 31-00: Reports the maximum input value for supported leaf 7
 *                       sub-leaves
 *      EBX: Bit 00: FSGSBASE. Supports RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE if 1
 *           Bit 01: IA32_TSC_ADJUST MSR is supported if 1
 *           Bit 06: Reserved
 *           Bit 07: SMEP. Supports Supervisor Mode Execution Protection if 1
 *           Bit 08: Reserved
 *           Bit 09: Supports Enhanced REP MOVSB/STOSB if 1
 *           Bit 10: INVPCID. If 1, supports INVPCID instruction for system
 *                   software that manages process-context identifiers
 *           Bit 11: Reserved
 *           Bit 12: Supports Quality of Service Monitoring (QM) capability if 1
 *           Bit 13: Deprecates FPU CS and FPU DS values if 1
 *           Bits 31-14: Reserved
 *      ECX: Reserved
 *      EDX: Reserved
 * EAX=09H
 *      EAX: Value of bits [31:0] of IA32_PLATFORM_DCA_CAP MSR (address 1F8H)
 *      EBX: Reserved
 *      ECX: Reserved
 *      EDX: Reserved
 * EAX=0AH
 *      EAX: Bits 07-00: Version ID of architectural performance monitoring
 *           Bits 15-08: Number of general-purpose performance monitoring
 *                       counter per logical processor
 *           Bits 23-16: Bit width of general-purpose, performance monitoring
 *                       counter
 *           Bits 31-24: Length of EBX bit vector to enumerate architectural
 *                       performance monitoring events
 *      EBX: Bit 00: Core cycle event not available if 1
 *           Bit 01: Instruction retired event not available if 1
 *           Bit 02: Reference cycles event not available if 1
 *           Bit 03: Last-level cache reference event not available if 1
 *           Bit 04: Last-level cache misses event not available if 1
 *           Bit 05: Branch instruction retired event not available if 1
 *           Bit 06: Branch mispredict retired event not available if 1
 *           Bits 31-07: Reserved = 0
 *      ECX: Reserved = 0
 *      EDX: Bits 04-00: Number of fixed-function performance counters
 *                       (if Version ID > 1)
 *          Bits 12-05: Bit width of fixed-function performance counters
 *                       (if Version ID > 1)
 *
 */


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
