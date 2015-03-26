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

#ifndef _KERNEL_ARCH_H
#define _KERNEL_ARCH_H

#include <aos/const.h>

/* Boot information from the boot loader */
#define BOOTINFO_BASE           0x8000ULL

/* Lowest memory address managed by memory allocator
 * Note that ISA memory hole (0x00f00000-0x00ffffff) are detected as available
 * in the system memory map obtained from the BIOS, so be carefull if we use
 * the address below 0x01000000 for PHYS_MEM_FREE_ADDR.
 */
#define PHYS_MEM_FREE_ADDR      0x02000000ULL

/* Maximum number of processors */
#define MAX_PROCESSORS          256

/* GDT and IDT */
#define GDT_ADDR                0x74000ULL
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                0x76000ULL
#define IDT_MAX_SIZE            0x2000

/* Tick */
#define HZ                      100
#define IV_LOC_TMR              0x50
#define IV_IRQ(n)       (0x20 + (n))

/*********************************************************/
/* The folloowing values are also defined in asmconst.h */
/*********************************************************/
/* # of IDT entries */
#define IDT_NR                  256
/* Kernel page table */
#define KERNEL_PGT              0x00079000ULL
/* Per-processor information (flags, cpuinfo, stats, tss, task, stack) */
#define P_DATA_BASE             0x01000000ULL
#define P_DATA_SIZE             0x10000
#define P_STACK_GUARD           0x10
#define P_TSS_OFFSET            (0x20 + IDT_NR * 8)
/* Trampoline: 0x70 (0x70000) */
#define TRAMPOLINE_VEC          0x70
#define TRAMPOLINE_MAX_SIZE     0x1000
/* GDT selectors */
#define GDT_NR                  9
#define GDT_NULL_SEL            (0<<3)
#define GDT_RING0_CODE_SEL      (1<<3)
#define GDT_RING0_DATA_SEL      (2<<3)
#define GDT_RING1_CODE_SEL      (3<<3)
#define GDT_RING1_DATA_SEL      (4<<3)
#define GDT_RING2_CODE_SEL      (5<<3)
#define GDT_RING2_DATA_SEL      (6<<3)
#define GDT_RING3_CODE_SEL      (7<<3)
#define GDT_RING3_DATA_SEL      (8<<3)
#define GDT_TSS_SEL_BASE        (9<<3)

/*
 * Boot information from boot loader
 */
struct bootinfo {
    struct {
        u64 nr;
        struct bootinfo_sysaddrmap_entry *entries;      /* u64 */
    } __attribute__ ((packed)) sysaddrmap;
} __attribute__ ((packed));
struct bootinfo_sysaddrmap_entry {
    u64 base;
    u64 len;
    u32 type;
    u32 attr;
} __attribute__ ((packed));

/*
 * Stack frame for interrupt handlers
 */
struct stackframe64 {
    /* Segment registers */
    u16 gs;
    u16 fs;

    /* Base pointer */
    u64 bp;

    /* Index registers */
    u64 di;
    u64 si;

    /* Generic registers */
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 dx;
    u64 cx;
    u64 bx;
    u64 ax;

    /* Restored by `iretq' instruction */
    u64 ip;             /* Instruction pointer */
    u64 cs;             /* Code segment */
    u64 flags;          /* Flags */
    u64 sp;             /* Stack pointer */
    u64 ss;             /* Stack segment */
} __attribute__ ((packed));

/*
 * TSS
 */
struct tss {
    u32 reserved1;
    u32 rsp0l;
    u32 rsp0h;
    u32 rsp1l;
    u32 rsp1h;
    u32 rsp2l;
    u32 rsp2h;
    u32 reserved2;
    u32 reserved3;
    u32 ist1l;
    u32 ist1h;
    u32 ist2l;
    u32 ist2h;
    u32 ist3l;
    u32 ist3h;
    u32 ist4l;
    u32 ist4h;
    u32 ist5l;
    u32 ist5h;
    u32 ist6l;
    u32 ist6h;
    u32 ist7l;
    u32 ist7h;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u16 iomap;
} __attribute__ ((packed));


/*
 * Process (architecture specific structure)
 */
struct arch_proc {
    /* Page table (cr3) */
    u64 pgt;
} __attribute__ ((packed));

/*
 * Task (architecture specific structure)
 */
struct arch_task {
    /* Do not change the first two.  These must be on the top.  See asm.s. */
    /* Restart point */
    struct stackframe64 *rp;
    /* SP0 for tss */
    u64 sp0;
    /* Stack pointers (kernel and user) */
    void *kstack;
    void *ustack;
    /* Parent structure (architecture-independent generic task structure) */
    struct ktask *ktask;
} __attribute__ ((packed));


/*
 * Data space for each processor
 */
struct p_data {
    u32 flags;          /* bit 0: enabled (working); bit 1- reserved */
    u32 cpu_id;
    u64 freq;           /* Frequency */
    u32 reserved[4];
    u64 stats[IDT_NR];  /* Interrupt counter */
    /* P_TSS_OFFSET */
    struct tss tss;
    /* P_CUR_TASK_OFFSET */
    struct arch_task *cur_task;
    /* P_NEXT_TASK_OFFSET */
    struct arch_task *next_task;
    /* Stack and stack guard follow */
} __attribute__ ((packed));

/* in arch.c */
struct p_data * this_cpu(void);

/* in asm.s */
void lidt(void *);
void lgdt(void *, u64);
void lldt(u16);
void ltr(u16);
void cli(void);
void sti(void);
void intr_null(void);
void intr_apic_loc_tmr(void);
void task_restart(void);
void syscall_setup(void *, u64);
void pause(void);
u8 inb(u16);
u16 inw(u16);
u32 inl(u16);
void outb(u16, u8);
void outw(u16, u16);
void outl(u16, u32);
u32 mfread32(u64);
void mfwrite32(u64, u32);
u64 binorder(u64);
void spin_lock_intr(u32 *);
void spin_lock(u32 *);
void spin_unlock(u32 *);
void spin_unlock_intr(u32 *);

/* in trampoline.s */
void trampoline(void);
void trampoline_end(void);

#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
