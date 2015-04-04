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
static struct arch_task * _create_idle_task(void);

/* System call table */
void *syscall_table[SYS_MAXSYSCALL];
struct proc_table *proc_table;

void intr_pf(void);

/* ACPI structure */
struct acpi arch_acpi;

#define KSTACK_SIZE     PAGESIZE
#define USTACK_SIZE     (PAGESIZE * 16)


#define INITRAMFS_BASE  0x20000ULL
#define USTACK_INIT     0x80000000ULL - 16
#define CODE_INIT       0x40000000ULL

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
 * Create an idle task
 */
static struct arch_task *
_create_idle_task(void)
{
    struct arch_task *t;

    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    t->rp = kmalloc(sizeof(struct stackframe64));
    if ( NULL == t->rp ) {
        kfree(t);
        return NULL;
    }
    t->kstack = kmalloc(PAGESIZE);
    if ( NULL == t->kstack ) {
        kfree(t->rp);
        kfree(t);
        return NULL;
    }
    t->ustack = kmalloc(PAGESIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t->rp);
        kfree(t);
        return NULL;
    }
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t->rp);
        kfree(t);
        return NULL;
    }
    t->ktask->arch = t;
    t->ktask->state = KTASK_STATE_CREATED;
    t->ktask->proc = NULL;
    t->ktask->next = NULL;

    /* Idle task runs at ring 0. */
    t->rp->cs = GDT_RING0_CODE_SEL;
    t->rp->ss = GDT_RING0_DATA_SEL;
    t->rp->ip = (u64)arch_idle;
    t->rp->sp = (u64)t->ustack + PAGESIZE - 16;
    t->rp->flags = 0x0200;
    t->sp0 = (u64)t->kstack + PAGESIZE - 16;

    return t;
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
    idt_setup_intr_gate(14, intr_pf);
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

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Set idle task */
    pdata->idle_task = _create_idle_task();
    if ( NULL == pdata->idle_task ) {
        panic("Fatal: Could not create the idle task for BSP.");
        return;
    }

    /* Initialize initramfs */
    if ( ramfs_init((u64 *)INITRAMFS_BASE) < 0 ) {
        panic("Fatal: Could not initialize the ramfs.");
        return;
    }

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

    /* Initialize local APIC counter */
    lapic_start_timer(HZ, IV_LOC_TMR);

    cli();
    u64 *initramfs = (u64 *)INITRAMFS_BASE;
    u64 offset = 0;
    u64 size;
    struct arch_task *t;

    /* Find the file pointed by path from the initramfs */
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

    /* Create a process */
    t = kmalloc(sizeof(struct arch_task));
    t->rp = kmalloc(sizeof(struct stackframe64));
    t->ktask = kmalloc(sizeof(struct ktask));
    t->ktask->arch = t;
    t->ktask->proc = kmalloc(sizeof(struct proc));
    t->ktask->proc->policy = KTASK_POLICY_DRIVER;
    t->ktask->proc->arch = kmalloc(sizeof(struct arch_proc));
    t->ktask->next = NULL;
    ((struct arch_proc *)t->ktask->proc->arch)->proc = t->ktask->proc;
    t->kstack = kmalloc(KSTACK_SIZE);
    t->ustack = kmalloc(USTACK_SIZE);

    t->ktask->next = pdata->idle_task->ktask;
    t->ktask->credit = 100;

    arch_exec(t, (void *)(INITRAMFS_BASE + offset), size, KTASK_POLICY_DRIVER);

    panic("Fatal: Could not start the `init' server.");
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

    /* Set idle task */
    pdata->idle_task = _create_idle_task();
    if ( NULL == pdata->idle_task ) {
        panic("Fatal: Could not create the idle task for BSP.");
        return;
    }

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
 * Execute a process
 */
int
arch_exec(struct arch_task *t, void (*entry)(void), size_t size, int policy)
{
    u64 cs;
    u64 ss;
    u64 flags;
    struct page_entry *pgt;
    u64 i;
    u64 j;
    void *exec;
    void *kstack;
    void *ustack;

    /* Setup page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return -1;
    }
    kmemset(pgt, 0, sizeof(struct page_entry) * (6 + 512));
    /* PML4 */
    pgt[0].entries[0] = (u64)&pgt[1] | 0x007;
    /* PDPT */
    for ( i = 0; i < 1; i++ ) {
        pgt[1].entries[i] = (u64)&pgt[2 + i] | 0x007;
        /* PD */
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = (i << 30) | (j << 21) | 0x183;
        }
    }
    /* PT (1GB-- +2MiB) */
    for ( i = 1; i < 2; i++ ) {
        pgt[1].entries[i] = (u64)&pgt[2 + i] | 0x007;
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    pgt[2 + 1].entries[0] = (u64)&pgt[6] | 0x007;
    pgt[2 + 1].entries[511] = (u64)&pgt[517] | 0x007;
    for ( i = 2; i < 3; i++ ) {
        pgt[1].entries[i] = (u64)&pgt[2 + i] | 0x007;
        for ( j = 0; j < 512; j++ ) {
            /* Not present */
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    /* Kernel */
    for ( i = 3; i < 4; i++ ) {
        pgt[1].entries[i] = (u64)&pgt[2 + i] | 0x007;
        /* PD */
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = (i << 30) | (j << 21) | 0x183;
        }
    }

    /* Program */
    if ( size < PAGESIZE ) {
        /* Alignment */
        exec = kmalloc(PAGESIZE);
    } else {
        exec = kmalloc(size);
    }
    if ( NULL == exec ) {
        kfree(pgt);
        return -1;
    }
    /* Stack */
    kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == kstack ) {
        kfree(pgt);
        kfree(exec);
        return -1;
    }
    ustack = kmalloc(USTACK_SIZE);
    if ( NULL == ustack ) {
        kfree(pgt);
        kfree(exec);
        kfree(kstack);
        return -1;
    }

    /* Copy the executable memory */
    (void)kmemcpy(exec, entry, size);
    for ( i = 0; i < (size - 1) / 4096 + 1; i++ ) {
        /* Mapping */
        pgt[6].entries[i] = ((u64)exec + i * PAGESIZE) | 0x087;
    }
    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = ((u64)ustack + i * PAGESIZE) | 0x087;
    }

    /* Configure the ring protection by the policy */
    switch ( policy ) {
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

    /* Clean up memory space of the current process */
    kfree(t->kstack);
    kfree(t->ustack);
    kmemset(t->rp, 0, sizeof(struct stackframe64));

    /* Replace the current process with the new process */
    t->kstack = kstack;
    t->ustack = ustack;
    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;
    t->rp->gs = ss;
    t->rp->fs = ss;
    t->rp->sp = USTACK_INIT;
    t->rp->ss = ss;
    t->rp->cs = cs;
    t->rp->ip = CODE_INIT;
    t->rp->flags = flags;

    /* Set the page table for the client */
    set_cr3(pgt);

    /* Free the old page table */
    kfree(((struct arch_proc *)t->ktask->proc->arch)->pgt);

    /* Set the page table for the client */
    ((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

    /* Schedule */
    this_cpu()->next_task = t;

    /* Restart the task */
    task_restart();

    /* Never reach here but do this to prevent a compiler error */
    return -1;
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
 * Schedule the next task
 */
void
set_next_ktask(struct ktask *ktask)
{
    this_cpu()->next_task = ktask->arch;
}

/*
 * Idle task
 */
void
arch_idle(void)
{
    while ( 1 ) {
        halt();
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
