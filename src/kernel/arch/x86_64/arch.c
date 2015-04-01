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
#include <sys/syscall.h>
#include "../../kernel.h"
#include "arch.h"
#include "desc.h"
#include "acpi.h"
#include "i8254.h"
#include "apic.h"
#include "memory.h"

/* Prototype declarations */
static int _load_trampoline(void);

/* System call table */
void *syscall_table[SYS_MAXSYSCALL];
struct proc_table *proc_table;

void intr_pf(void);

/* ACPI structure */
struct acpi arch_acpi;

#define KSTACK_SIZE     PAGESIZE
#define USTACK_SIZE     (PAGESIZE * 16)

/*
 * Relocate the trampoline code to a 4 KiB page alined space
 */
static int
_load_trampoline(void)
{
    int i;
    int tsz;

    /* Check and copy trampoline code */
    tsz = (u64)trampoline_end - (u64)trampoline;
    if ( tsz > TRAMPOLINE_MAX_SIZE ) {
        /* Error */
        return -1;
    }
    for ( i = 0; i < tsz; i++ ) {
        *(u8 *)((u64)(TRAMPOLINE_VEC << 12) + i) = *(u8 *)((u64)trampoline + i);
    }

    return 0;
}

/*
 * Panic -- damn blue screen, lovely green screen
 */
void
panic(char *s)
{
    int i;
    u16 *video;
    u16 val;

    /* Disable interrupt */
    cli();

    /* Notify other processors to stop */
    /* Send IPI and halt self */
    lapic_send_fixed_ipi(IV_CRASH);

    /* Print out the message string directly */
    video = (u16 *)0xb8000;
    for ( i = 0; *s; i++, s++  ) {
        *video = 0x2f00 | *s;
        video++;
    }
    /* Move the cursor */
    val = ((i & 0xff) << 8) | 0x0f;
    outw(0x3d4, val);   /* Low */
    val = (((i >> 8) & 0xff) << 8) | 0x0e;
    outw(0x3d4, val);   /* High */
    /* Fill out */
    for ( ; i < 80 * 25; i++ ) {
        *video = 0x2f00;
        video++;
    }

    /* Stop forever */
    while ( 1 ) {
        halt();
    }
}

/* This function is trial one, so need to be modified. */
int
kexecve(const char *path, char *const argv[], char *const envp[])
{
    u64 *initramfs = (u64 *)0x20000ULL;
    u64 offset = 0;
    u64 size;
    struct arch_task *t;
    u64 cs;
    u64 ss;
    u64 flags;

    /* Find the file pointed by path from the initramfs */
    while ( 0 != *initramfs ) {
        if ( 0 == kstrcmp((char *)initramfs, path) ) {
            offset = *(initramfs + 2);
            size = *(initramfs + 3);
            break;
        }
        initramfs += 4;
    }
    if ( 0 == offset ) {
        /* Could not find init */
        return -1;
    }

    /* Get the currently running task information */
    t = this_cpu()->cur_task;
    if ( NULL != t ) {
        kfree(t->kstack);
        kfree(t->ustack);
        kmemset(t->rp, 0, sizeof(struct stackframe64));
    } else {
        t = kmalloc(sizeof(struct arch_task));
        t->rp = kmalloc(sizeof(struct stackframe64));
        t->ktask = kmalloc(sizeof(struct ktask));
        t->ktask->arch = t;
        t->ktask->proc = kmalloc(sizeof(struct proc));
        t->ktask->proc->policy = KTASK_POLICY_DRIVER;
    }

    /* Clean up memory space of the current process */

    /* Inherit the policy from the current task */
    switch ( t->ktask->proc->policy ) {
    case KTASK_POLICY_KERNEL:
        cs = GDT_RING0_CODE_SEL;
        ss = GDT_RING0_DATA_SEL;
        flags = 0x0200;
        break;
    case KTASK_POLICY_DRIVER:
        cs = GDT_RING1_CODE_SEL + 1;
        ss = GDT_RING1_DATA_SEL + 1;
        flags = 0x1200;
        break;
    case KTASK_POLICY_USER:
        cs = GDT_RING3_CODE_SEL + 3;
        ss = GDT_RING3_DATA_SEL + 3;
        flags = 0x3200;
        break;
    }

    /* Setup page table */
    /* 0: Present
       1: R/W
       2: U/S (user/superuser)
       3: PWT (page-level write through)
       4: PCD (page-level cache disable)
     */
    u64 *pgt = kmalloc(4096 * (6 + 512));
    if ( NULL == pgt ) {
        return -1;
    }
    kmemset(pgt, 0, 4096 * (6 + 512));
    /* PML4 */
    pgt[0] = (u64)pgt + 0x1007;
    u64 i;
    u64 j;
    /* PDPT */
    for ( i = 0; i < 1; i++ ) {
        pgt[512 + i] = (u64)pgt + ((i + 2) << 12) + 0x007;
        /* PD */
        for ( j = 0; j < 512; j++ ) {
            pgt[1024 + 512 * i + j] = (i << 30) | (j << 21) | 0x183;
        }
    }
    /* PT (1GB-- +2MiB) */
    for ( i = 1; i < 2; i++ ) {
        pgt[512 + i] = (u64)pgt + ((i + 2) << 12) + 0x007;
        for ( j = 0; j < 512; j++ ) {
            pgt[1024 + 512 * i + j] = 0x000;
        }
    }
    pgt[1024 + 512 * 1 + 0] = (u64)pgt + ((6 + 0) << 12) + 0x007;
    for ( i = 2; i < 3; i++ ) {
        pgt[512 + i] = (u64)pgt + ((i + 2) << 12) + 0x007;
        for ( j = 0; j < 512; j++ ) {
            /* Not present */
            pgt[1024 + 512 * i + j] = 0x000;
        }
    }
    for ( i = 3; i < 4; i++ ) {
        pgt[512 + i] = ((u64)pgt + ((i + 2) << 12)) | 0x007;
        /* PD */
        for ( j = 0; j < 512; j++ ) {
            pgt[1024 + 512 * i + j] = (i << 30) | (j << 21) | 0x183;
        }
    }

    /* Relocate the file */
    void *exec;
    void *ustack;
    if ( size < PAGESIZE ) {
        /* Alignment */
        exec = kmalloc(PAGESIZE);
    } else {
        exec = kmalloc(size);
    }
    if ( NULL == exec ) {
        return -1;
    }
    (void)kmemcpy(exec, (void *)(0x20000ULL + offset), size);
    for ( i = 0; i < (size - 1) / 4096 + 1; i++ ) {
        /* Mapping */
        pgt[1024 + 512 + i] = (u64)exec | 0x087;
    }
    __asm__ __volatile__ (" movq %%rax,%%dr3 " :: "a"(i));
    //__asm__ __volatile__ (" movq %%rax,%%dr3 " :: "a"(pgt[3072 + 0]));
    //__asm__ __volatile__ (" movq %%rax,%%dr2 " :: "a"(exec));
    /* Stack */
    ustack = kmalloc(PAGESIZE);
    if ( NULL == ustack ) {
        kfree(exec);
        return -1;
    }
    for ( i = 0; i < (PAGESIZE - 1) / 4096 + 1; i++ ) {
        pgt[265216 - (PAGESIZE - 1) / 4096 + 1 + i] = (u64)ustack | 0x087;
    }

    /* Replace the current process with the new process */
    t->kstack = kmalloc(KSTACK_SIZE);
    t->ustack = ustack;
    t->sp0 = (u64)t->kstack;
    t->rp->gs = ss;
    t->rp->fs = ss;
    t->rp->sp = (u64)0x40200000ULL - 16; //t->ustack;
    t->rp->ss = ss;
    t->rp->cs = cs;
    t->rp->ip = 0x40000000ULL;
    t->rp->flags = flags;

    /* Schedule */
    this_cpu()->next_task = t;
    //__asm__ __volatile__ (" movq %%rax,%%dr2 " :: "a"(*(u64 *)0x40000000ULL));
    __asm__ __volatile__ (" movq %%rax,%%cr3 " :: "a"(pgt));
    __asm__ __volatile__ (" movq %%rax,%%dr2 " :: "a"(*(u64 *)0x40000000ULL));

    /* Restart the task */
    task_restart();

    /* Never reach here but do this to prevent a compiler error */
    return -1;
}

/*
 * Initialize the bootstrap processor
 */
void
bsp_init(void)
{
    struct bootinfo *bi;
    struct p_data *pdata;
    long long i;

    /* Ensure the i8254 timer is stopped */
    i8254_stop_timer();

    /* Boot information from the boot monitor */
    bi = (struct bootinfo *)BOOTINFO_BASE;

    /* Reset all processors */
    for ( i = 0; i < MAX_PROCESSORS; i++ ) {
        /* Fill the processor data space with zero excluding stack area */
        kmemset((u8 *)((u64)P_DATA_BASE + i * P_DATA_SIZE), 0,
                sizeof(struct p_data));
    }

    /* Initialize global descriptor table */
    gdt_init();
    gdt_load();

    /* Initialize interrupt descriptor table */
    idt_init();
    idt_load();

    /* Load ACPI */
    acpi_load(&arch_acpi);

    /* Set up interrupt vector */
    for ( i = 0; i < 7; i++ ) {
        idt_setup_intr_gate(i, intr_pf);
    }
    idt_setup_intr_gate(IV_LOC_TMR, intr_apic_loc_tmr);
    idt_setup_intr_gate(IV_CRASH, intr_crash);

    /* Initialize memory manager */
    (void)phys_mem_init(bi);

    /* Initialize I/O APIC */
    ioapic_init();

    /* Setup interrupt service routine then initialize I/O APIC */
    for ( i = 0; i < 32; i++ ) {
        ioapic_map_intr(IV_IRQ(i), i, arch_acpi.acpi_ioapic_base); /* IRQn */
    }

    /* Load LDT */
    lldt(0);

    /* Initialize TSS */
    tss_init();
    tr_load(lapic_id());

    /* Initialize the local APIC */
    lapic_init();

    /* Setup system call */
    for ( i = 0; i < SYS_MAXSYSCALL; i++ ) {
        syscall_table[i] = NULL;
    }
    syscall_table[SYS_exit] = sys_exit;
    syscall_table[SYS_fork] = sys_fork;
    syscall_table[SYS_read] = sys_read;
    syscall_table[SYS_write] = sys_write;
    syscall_table[SYS_open] = sys_open;
    syscall_table[SYS_close] = sys_close;
    syscall_table[SYS_wait4] = sys_wait4;
    syscall_table[SYS_execve] = sys_execve;
    syscall_table[SYS_getpid] = sys_getpid;
    syscall_table[SYS_getppid] = sys_getppid;
    syscall_setup(syscall_table, SYS_MAXSYSCALL);

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Load trampoline code */
    _load_trampoline();

    /* Send INIT IPI */
    lapic_send_init_ipi();

    /* Wait 10 ms */
    acpi_busy_usleep(&arch_acpi, 10000);

    /* Send a Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&arch_acpi, 200);

    /* Send another Start Up IPI */
    lapic_send_startup_ipi(TRAMPOLINE_VEC & 0xff);

    /* Wait 200 us */
    acpi_busy_usleep(&arch_acpi, 200);

    /* Initialize the process table */
    proc_table = kmalloc(sizeof(struct proc_table));
    if ( NULL == proc_table ) {
        panic("Fatal: Could not initialize the process table.");
        return;
    }
    for ( i = 0; i < PROC_NR; i++ ) {
        proc_table->procs[i] = NULL;
    }
    proc_table->lastpid = -1;


    kexecve("/servers/init", NULL, NULL);

    /* Find init server from the initramfs */
    u64 *initramfs = (u64 *)0x20000ULL;
    u64 offset = 0;
    u64 size;
    while ( 0 != *initramfs ) {
        if ( 0 == kstrcmp((char *)initramfs, "/servers/init") ) {
            offset = *(initramfs + 2);
            size = *(initramfs + 3);
            break;
        }
        initramfs += 4;
    }
    if ( 0 == offset ) {
        /* Could not find init */
        panic("Fatal: Could not find the `init' server.");
        return;
    }

    /* Allocate a process for init server */
    struct proc *proc;
    struct arch_proc *arch_proc;
    proc = kmalloc(sizeof(struct proc));
    if ( NULL == proc ) {
        /* Cannot allocate proc */
        panic("Fatal: Could not initialize the `init' server process.");
        return;
    }
    arch_proc = kmalloc(sizeof(struct arch_proc));
    if ( NULL == arch_proc ) {
        /* Cannot allocate arch_proc */
        panic("Fatal: Could not initialize the `init' server process.");
        return;
    }
    arch_proc->proc = proc;
    proc->arch = arch_proc;
    proc_table->procs[0] = proc;
    proc_table->lastpid = 0;

    struct ktask *task;
    struct arch_task *arch_task;
    struct stackframe64 *s;
    task = kmalloc(sizeof(struct ktask));
    if ( NULL == task ) {
        panic("Fatal: Could not initialize the kernel task.");
        return;
    }
    arch_task = kmalloc(sizeof(struct arch_task));
    if ( NULL == arch_task ) {
        panic("Fatal: Could not initialize the kernel task.");
        return;
    }
    s = kmalloc(sizeof(struct stackframe64));
    if ( NULL == s ) {
        panic("Fatal: Could not initialize the kernel task.");
        return;
    }
    kmemset(s, 0, sizeof(struct stackframe64));
    s->ss = GDT_RING1_DATA_SEL + 1;
    s->cs = GDT_RING1_CODE_SEL + 1;
    s->ip = 0x20000ULL + offset;
    s->sp = kmalloc(4096);
    s->flags = 0x1200;
    arch_task->rp = s;
    arch_task->sp0 = kmalloc(4096);

    task->arch = arch_task;
    arch_task->ktask = task;

    /* Schedule */
    this_cpu()->next_task = arch_task;

    /* Initialize local APIC counter */
    sti();
    lapic_start_timer(HZ, IV_LOC_TMR);

    /* Start the init process */
    task_restart();
}

/*
 * Initialize the application processor
 */
void
ap_init(void)
{
    struct p_data *pdata;

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Load LDT */
    lldt(0);

    /* Load TSS */
    tr_load(lapic_id());

    /* Initialize the local APIC */
    lapic_init();
}

/*
 * Get the CPU data structure
 */
struct p_data *
this_cpu(void)
{
    struct p_data *pdata;

    pdata = (struct p_data *)(P_DATA_BASE + lapic_id() * P_DATA_SIZE);

    return pdata;
}

/*
 * Create a new task
 */
struct arch_task *
arch_task_new(void *entry, int policy)
{
    struct arch_task *t;
    struct stackframe64 *s;
    void *kstack;
    void *ustack;
    u64 cs;
    u64 ss;
    u64 flags;

    switch ( policy ) {
    case KTASK_POLICY_KERNEL:
        cs = GDT_RING0_CODE_SEL;
        ss = GDT_RING0_DATA_SEL;
        flags = 0x0200;
        break;
    case KTASK_POLICY_DRIVER:
        cs = GDT_RING1_CODE_SEL;
        ss = GDT_RING1_DATA_SEL;
        flags = 0x1200;
        break;
    case KTASK_POLICY_USER:
        cs = GDT_RING3_CODE_SEL;
        ss = GDT_RING3_DATA_SEL;
        flags = 0x3200;
        break;
    }

    /* Allocate task */
    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    /* Allocate for stack frame */
    s = kmalloc(sizeof(struct stackframe64));
    if ( NULL == s ) {
        kfree(t);
        return NULL;
    }
    kmemset(s, 0, sizeof(struct stackframe64));
    /* Allocate kernel stack */
    kstack = kmalloc(4096);
    if ( NULL == kstack ) {
        kfree(t);
        kfree(s);
        return NULL;
    }
    /* Allocate user stack */
    ustack = kmalloc(4096);
    if ( NULL == ustack ) {
        kfree(t);
        kfree(s);
        kfree(kstack);
        return NULL;
    }
    s->cs = cs;
    s->ss = ss;
    s->ip = (u64)entry;
    s->sp = (u64)ustack;
    s->flags = flags;
    t->rp = s;
    t->sp0 = (u64)kstack;

    return t;
}

/*
 * Clone a task
 */
struct arch_task *
arch_task_clone(struct arch_task *task)
{
    struct arch_task *ntask;

    /* Allocate */
    ntask = kmalloc(sizeof(struct arch_task));
    if ( NULL == ntask ) {
        return NULL;
    }
    /* Copy */
    kmemcpy(ntask, task, sizeof(struct arch_task));

    return ntask;
}

/*
 * A routine called when task is switched
 * Note that this is in the interrupt handler and DO NOT change the interrupt
 * flag (i.e., DO NOT use sti/cli).  Also use caution in use of lock.
 */
void
arch_task_swiched(struct arch_task *prev, struct arch_task *next)
{
    if ( NULL != prev ) {
        /* Switched from a task */
        prev->ktask->state = KTASK_STATE_READY;
        next->ktask->state = KTASK_STATE_RUNNING;
    }
}

/*
 * Get the kernel task currently running on this processor
 */
struct ktask *
this_ktask(void)
{
    struct p_data *pdata;

    /* Get the information on this processor */
    pdata = this_cpu();
    if ( NULL == pdata->cur_task ) {
        /* No task running on this processor */
        return NULL;
    }

    /* Return the kernel task data structure */
    return pdata->cur_task->ktask;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
