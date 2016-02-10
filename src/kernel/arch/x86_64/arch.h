/*_
 * Copyright (c) 2015-2016 Hirochika Asai <asai@jar.jp>
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
#include <aos/types.h>

/* Boot information from the boot loader */
#define BOOTINFO_BASE           0x8000ULL

/* Color video RAM */
#define VIDEO_COLOR             0xb8000ULL

/* Lowest memory address managed by memory allocator
 * Note that ISA memory hole (0x00f00000-0x00ffffff) are detected as available
 * in the system memory map obtained from the BIOS, so be carefull if we use
 * the address below 0x01000000 for PMEM_LBOUND.
 */
#define PMEM_LBOUND             0x02000000ULL

#define KMEM_BASE               0x00100000ULL
#define KMEM_MAX_SIZE           0x00e00000ULL

#define KMEM_REGION_PMEM_BASE   0x100000000ULL
#define KMEM_REGION_KERNEL_BASE 0xc0000000ULL
#define KMEM_REGION_KERNEL_SIZE 0x18000000ULL

#define KMEM_REGION_SPEC_BASE   0xd8000000ULL
#define KMEM_REGION_SPEC_SIZE   0x28000000ULL

/* Maximum number of processors supported in this operating system */
#define MAX_PROCESSORS          256

/* GDT and IDT */
#define GDT_ADDR                0x74000ULL
#define GDT_MAX_SIZE            0x2000
#define IDT_ADDR                0x76000ULL
#define IDT_MAX_SIZE            0x2000

/* Kernel variable */
#define KVAR_ADDR               0x78000ULL

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
#define GDT_RING3_CODE32_SEL    (3<<3)
#define GDT_RING3_DATA_SEL      (4<<3)
#define GDT_RING3_CODE64_SEL    (5<<3)
#define GDT_TSS_SEL_BASE        (6<<3)

/* Control registers */
#define CR4_PGE                 (1ULL<<7)

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
 * Page table entry
 */
struct page_entry {
    u64 entries[512];
} __attribute__ ((packed));


/*
 * Architecture specific page entry
 */
struct arch_page_entry {
    union {
        struct arch_page_dir *child;
        void *addr;
        u64 bits;
    } u;
};
struct arch_page_dir {
    u64 *entries[512];
};

/*
 * Level-3 Complete multiway (512-ary) tree for virtual memory
 */
#define VMEM_NENT(x)        (DIV_CEIL((x), 512 * 512) + DIV_CEIL((x), 512) \
                             + (x))
#define VMEM_PML4(x)        (x[0])
#define VMEM_PDPT(x, pg)    (x[0 + DIV_CEIL((pg) + 1, 512) + FLOOR((pg), 512)])
#define VMEM_PD(x, pg)      (x[1 + DIV_CEIL((pg) + 1, 512) + pg])
struct arch_vmem_space {
    /* The root of the page table */
    void *pgt;
    /* Level-3 complete multiway (512-ary) tree */
    int nr;
    u64 **array;
    /* Leaves for virtual memory */
    u64 **vls;
};

/*
 * Task (architecture specific structure)
 */
struct arch_task {
    /* Do not change the first two.  These must be on the top.  See asm.s. */
    /* Restart point */
    struct stackframe64 *rp;
    /* SP0 for tss */
    u64 sp0;
    /* CR3: Physical address of the page table */
    void *cr3;
    /* Kernel stack pointer (kernel address) */
    void *kstack;
    /* User stack pointer (virtual address) */
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
    int prox_domain;
    u32 reserved[3];
    u64 stats[IDT_NR];  /* Interrupt counter */
    /* P_TSS_OFFSET */
    struct tss tss;
    /* P_CUR_TASK_OFFSET */
    struct arch_task *cur_task;
    /* P_NEXT_TASK_OFFSET */
    struct arch_task *next_task;
    /* Idle task */
    struct arch_task *idle_task;
    /* Stack and stack guard follow */
} __attribute__ ((packed));

/* in arch.c */
struct p_data * this_cpu(void);
int
arch_exec(struct arch_task *, void (*)(void), size_t, int, char *const [],
          char *const []);
void arch_idle(void);

/* in vmx.c */
int vmx_enable(void);
int vmx_initialize_vmcs(void);

/* in asm.s */
void lidt(void *);
void lgdt(void *, u64);
void sidt(void *);
void sgdt(void *);
void lldt(u16);
void ltr(u16);
void cli(void);
void sti(void);
void intr_null(void);

void intr_dze(void);
void intr_debug(void);
void intr_nmi(void);
void intr_breakpoint(void);
void intr_overflow(void);
void intr_bre(void);
void intr_dna(void);
void intr_df(void);
void intr_snpf(void);
void intr_ssf(void);

void intr_iof(void);
void intr_gpf(void);
void intr_pf(void);
void intr_x87_fpe(void);
void intr_simd_fpe(void);
void intr_apic_loc_tmr(void);
void intr_crash(void);
void task_restart(void);
void task_replace(void *);
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
u64 cpuid(u64, u64 *, u64 *);
u64 rdmsr(u64);
void wrmsr(u64, u64);
u64 get_cr0(void);
void set_cr0(u64);
void * get_cr3(void);
void set_cr3(void *);
u64 get_cr4(void);
void set_cr4(u64);
void invlpg(void *);
int vmxon(void *);
int vmclear(void *);
int vmptrld(void *);
int vmwrite(u64, u64);
u64 vmread(u64);
int vmlaunch(void);
int vmresume(void);
void spin_lock_intr(u32 *);
void spin_unlock_intr(u32 *);

/* in trampoline.s */
void trampoline(void);
void trampoline_end(void);

/* in task.c */
struct arch_task * task_create_idle(void);
int proc_create(const char *, const char *, pid_t);

/* In-line assembly */
#define set_cr3(cr3)    __asm__ __volatile__ ("movq %%rax,%%cr3" :: "a"((cr3)))

#endif /* _KERNEL_ARCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
