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

#ifndef _BOOT_CONST_H
#define _BOOT_CONST_H

#define KERNEL_BASE     0x00010000      /* Address to load the kernel */
#define INITRAMFS_BASE  0x00030000      /* Address to load the initramfs */
#define KERNEL_PGT      0x00079000      /* Page table */
#define P_DATA_SIZE     0x10000         /* Data size for each processor */
#define P_DATA_BASE     0x1000000       /* Data base for each processor */
#define P_STACK_GUARD   0x10

#define BOOT_TIMEOUT    3               /* Timeout in seconds */

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

#define MME_SIZE        24              /* Memory map entry size */
#define MME_SIGN        0x534d4150      /* MME signature (ascii "SMAP")  */


/* Descriptors */
#define GDT_NULL        0x00
#define GDT_CODE64      0x08
#define GDT_DATA64      0x10
#define GDT_CODE32      0x18
#define GDT_DATA32      0x20
#define GDT_CODE16      0x28

/* I/O port */

/* i8259 */
#define IO_PIC1_CMD     0x20
#define IO_PIC1_DATA    0x21
#define IO_PIC2_CMD     0xa0
#define IO_PIC2_DATA    0xa1

/* Flags */
#define EFLAGS_CPUID    21

/* Keyboard */
#define KBD_LCTRL       0x1d    /* Left ctrl */
#define KBD_LSHIFT      0x2a    /* Left shift */
#define KBD_RSHIFT      0x36    /* Right shift */
#define KBD_CAPS        0x3a    /* Caps lock */
#define KBD_RCTRL       0x5a    /* Right ctrl */
#define KBD_UP          0x48    /* Up */
#define KBD_LEFT        0x4b    /* Left */
#define KBD_RIGHT       0x4d    /* Right */
#define KBD_DOWN        0x50    /* Down */


/* CPUID features */
#define CPUID1H_ECX_SSE3        0       /* SSE3 Extensions */
#define CPUID1H_ECX_PCLMULQDQ   1       /* Carryless Multiplication */
#define CPUID1H_ECX_DTES64      2       /* 64-bit DS Area */
#define CPUID1H_ECX_MONITOR     3       /* MONITOR/MWAIT */
#define CPUID1H_ECX_DSCPL       4       /* CPL Qualified Debug Store */
#define CPUID1H_ECX_VMX         5       /* Virtual Machine Extensions */
#define CPUID1H_ECX_SMX         6       /* Safer Mode Extensions */
#define CPUID1H_ECX_EST         7       /* Enhanced SpeedStep Technology */
#define CPUID1H_ECX_TM2         8       /* Thermal Monitor 2 */
#define CPUID1H_ECX_SSSE3       9       /* SSSE3 Extensions */
#define CPUID1H_ECX_CNXTID      10      /* L1 Context ID */
#define CPUID1H_ECX_FMA         12      /* Fused Multiply Add */
#define CPUID1H_ECX_CMPXCHG16B  13      /* CMPXCHG16B */
#define CPUID1H_ECX_XTPR        14      /* xTPR Update Control */
#define CPUID1H_ECX_PDCM        15      /* Perf/Debug Capability MSR */
#define CPUID1H_ECX_PCID        17      /* Process-context Identifiers */
#define CPUID1H_ECX_DCA         18      /* Direct Cache Access */
#define CPUID1H_ECX_SSE41       19      /* SSE4.1 */
#define CPUID1H_ECX_SSE42       20      /* SSE4.2 */
#define CPUID1H_ECX_X2APIC      21      /* x2APIC */
#define CPUID1H_ECX_MOVBE       22      /* MOVBE */
#define CPUID1H_ECX_POPCNT      23      /* POPCNT */
#define CPUID1H_ECX_TSCDL       24      /* TSC-Deadline */
#define CPUID1H_ECX_AES         25      /* AES */
#define CPUID1H_ECX_XSAVE       26      /* XSAVE */
#define CPUID1H_ECX_OSXSAVE     27      /* OSXSAVE */
#define CPUID1H_ECX_AVX         28      /* AVX */
#define CPUID1H_ECX_F16C        29      /* F16C */
#define CPUID1H_ECX_RDRAND      30      /* RDRAND */

#define CPUID1H_EDX_FPU         0       /* x87 FPU on Chip */
#define CPUID1H_EDX_VME         1       /* Virtual-8086 Mode Enhancement */
#define CPUID1H_EDX_DE          2       /* Debugging Extensions */
#define CPUID1H_EDX_PSE         3       /* Page Size Extensions */
#define CPUID1H_EDX_TSC         4       /* Time Stamp Counter */
#define CPUID1H_EDX_MSR         5       /* RDMSR and WRMSR Support */
#define CPUID1H_EDX_PAE         6       /* Page Address Extensions */
#define CPUID1H_EDX_MCE         7       /* Machine Check Exception */
#define CPUID1H_EDX_CX8         8       /* CMPXCHG8B Inst. */
#define CPUID1H_EDX_APIC        9       /* APIC on Chip */
#define CPUID1H_EDX_SEP         11      /* SYSENTER and SYSEXIT */
#define CPUID1H_EDX_MTRR        12      /* Memory Type Range Registers */
#define CPUID1H_EDX_PGE         13      /* PTE Global Bit */
#define CPUID1H_EDX_MCA         14      /* Machine Check Architecture */
#define CPUID1H_EDX_CMOV        15      /* Conditional Move/Compare Inst. */
#define CPUID1H_EDX_PAT         16      /* Page Attribute Table */
#define CPUID1H_EDX_PSE36       17      /* Page Size Extension */
#define CPUID1H_EDX_PSN         18      /* Processor Serial Number */
#define CPUID1H_EDX_CLFSH       19      /* CFLUSH instruction */
#define CPUID1H_EDX_DS          21      /* Debug Store */
#define CPUID1H_EDX_ACPI        22      /* Thermal Monitor and Clock Ctrl */
#define CPUID1H_EDX_MMX         23      /* MMX Technology */
#define CPUID1H_EDX_FXSR        24      /* FXSAVE/FXRSTOR */
#define CPUID1H_EDX_SSE         25      /* SSE Extensions */
#define CPUID1H_EDX_SSE2        26      /* SSE2 Extensions */
#define CPUID1H_EDX_SS          27      /* Self Snoop */
#define CPUID1H_EDX_HTT         28      /* Multi-threading */
#define CPUID1H_EDX_TM          29      /* Therm. Monitor */
#define CPUID1H_EDX_PBE         31      /* Pend. Brk. EN. */


/* Control Registers */
#define CR0_PE                  0
#define CR0_MP                  1
#define CR0_EM                  2
#define CR0_TS                  3
#define CR0_ET                  4
#define CR0_NE                  5
#define CR0_WP                  16
#define CR0_AM                  18
#define CR0_NW                  29
#define CR0_CD                  30
#define CR0_PG                  31

#define CR3_PWT                 3
#define CR3_PCD                 4

#define CR4_VME                 0
#define CR4_PVI                 1
#define CR4_TSD                 2
#define CR4_DE                  3
#define CR4_PSE                 4
#define CR4_PAE                 5
#define CR4_MCE                 6
#define CR4_PGE                 7
#define CR4_PCE                 8
#define CR4_OSFXSR              9
#define CR4_OSXMMEXCPT          10
#define CR4_VMXE                13
#define CR4_SMXE                14
#define CR4_FSGSBASE            16
#define CR4_PCIDE               17
#define CR4_OSXSAVE             18
#define CR4_SMEP                20


/* MSR */
#define MSR_IA32_EFER           0xc0000080
#define IA32_EFER_SCE           0               /* SysCall enable */
#define IA32_EFER_LME           8               /* IA-32e mode enable */
#define IA32_EFER_LMA           10              /* IA-32e mode active */
#define IA32_EFER_NXE           11              /* Execute-disable bit enable */


/* PXE boot */
#define PXENV_SIGNATURE0        0x4e455850      /* PXEN */
#define PXENV_SIGNATURE1        0x2b56          /* V+ */
#define PXE_SIGNATURE           0x45585021      /* !PXE */


/* IRQ */
#define IVT_IRQ0                0x08            /* IRQ0 = 0x08 (BIOS default) */
#define IVT_IRQ8                0x70            /* IRQ0 = 0x70 (BIOS default) */
#define VGA_TEXT_COLOR_80x25    0x03
#define NUM_RETRIES             3               /* # of retries for disk read */
#define ERRCODE_TIMEOUT         0x80            /* Error code: Timeout */
#define SECTOR_SIZE             0x200           /* 512 bytes / sector */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
