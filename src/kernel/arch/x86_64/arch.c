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

/* ACPI structure */
struct acpi arch_acpi;

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

    /* Find init server from the initramfs */
    u64 *initramfs = (u64 *)0x20000ULL;
    u64 offset = 0;
    u64 size;
    while ( 0 != *initramfs ) {
        if ( 0 == kstrcmp((char *)initramfs, "init") ) {
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
    proc->ktask = task;

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
