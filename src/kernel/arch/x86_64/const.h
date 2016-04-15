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

#ifndef _KERNEL_CONST_H
#define _KERNEL_CONST_H

/* GDT selectors */
#define GDT_RING0_CODE_SEL      0x08
#define GDT_RING0_DATA_SEL      0x10
#define GDT_RING3_CODE32_SEL    0x18
#define GDT_RING3_DATA_SEL      0x20
#define GDT_RING3_CODE64_SEL    0x28

/* Temporary GDT for application processors */
#define AP_GDT_CODE64_SEL       0x08    /* Code64 selector */
#define AP_GDT_CODE32_SEL       0x10    /* Code32 selector */
#define AP_GDT_CODE16_SEL       0x18    /* Code16 selector */
#define AP_GDT_DATA64_SEL       0x20    /* Data selector */
#define AP_GDT_DATA32_SEL       0x28    /* Data selector */

#define APIC_BASE               0xfee00000

/* # of interrupts */
#define IDT_NR                  256

/* Kernel page table */
#define KERNEL_PGT              0x00079000
/* Per-processor information (struct p_data) */
#define P_DATA_BASE             0x01000000
#define P_DATA_SIZE             0x10000
#define P_STACK_GUARD           0x10
#define P_TSS_SIZE              104     /* sizeof(struct tss) */
#define P_TSS_OFFSET            (0x20 + IDT_NR * 8)     /* struct tss */
#define P_CUR_TASK_OFFSET       (P_TSS_OFFSET + P_TSS_SIZE)     /* cur_task */
#define P_NEXT_TASK_OFFSET      (P_CUR_TASK_OFFSET + 8) /* next_task */
/* Task information (struct arch_task) */
#define TASK_RP                 0
#define TASK_SP0                8
#define TASK_CR3                16
#define TASK_KTASK              40
/* TSS */
#define TSS_SP0                 4
/* Trampoline */
#define TRAMPOLINE_VEC          0x70

/* Syscall */
#define SYSCALL_MAX_NR          0x10

/* MSR */
#define MSR_IA32_EFER           0xc0000080
#define IA32_EFER_SCE           0               /* SysCall enable */
#define IA32_EFER_LME           8               /* IA-32e mode enable */
#define IA32_EFER_LMA           10              /* IA-32e mode active */
#define IA32_EFER_NXE           11              /* Execute-disable bit enable */

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

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
