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
int _create_process(struct arch_task *, void (*)(void), size_t, int);

/* System call table */
void *syscall_table[SYS_MAXSYSCALL];

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
    t->kstack = kmalloc(PAGESIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }
    t->ustack = kmalloc(PAGESIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask->arch = t;
    //t->ktask->state = KTASK_STATE_CREATED;
    t->ktask->state = KTASK_STATE_READY;
    t->ktask->proc = NULL;
    t->ktask->next = NULL;

    t->rp = t->kstack + PAGESIZE - 16 - sizeof(struct stackframe64);

    /* Idle task runs at ring 0. */
    t->rp->cs = GDT_RING0_CODE_SEL;
    t->rp->ss = GDT_RING0_DATA_SEL;
    t->rp->ip = (u64)arch_idle;
    t->rp->sp = (u64)t->ustack + PAGESIZE - 16;
    t->rp->flags = 0x0200;
    t->sp0 = (u64)t->kstack + PAGESIZE - 16;

    /* Page table */
    t->cr3 = KERNEL_PGT;

    return t;
}

/*
 * Create the init server
 */
static int
_create_init_server(void)
{
    u64 *initramfs = (u64 *)INITRAMFS_BASE;
    u64 offset = 0;
    u64 size;
    struct arch_task *t;
    struct ktask_list *l;
    int ret;

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
        return -1;
    }

    /* Create a process */
    t = kmalloc(sizeof(struct arch_task));
    t->ktask = kmalloc(sizeof(struct ktask));
    t->ktask->arch = t;
    t->ktask->proc = kmalloc(sizeof(struct proc));
    t->ktask->proc->policy = KTASK_POLICY_DRIVER;
    t->ktask->proc->arch = kmalloc(sizeof(struct arch_proc));
    t->ktask->next = NULL;
    ((struct arch_proc *)t->ktask->proc->arch)->proc = t->ktask->proc;
    t->kstack = kmalloc(KSTACK_SIZE);
    t->ustack = kmalloc(USTACK_SIZE);
    t->rp = t->kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);
    //t->ktask->state = KTASK_STATE_CREATED;
    t->ktask->state = KTASK_STATE_READY;

    /* Process table */
    proc_table->procs[1] = t->ktask->proc;
    proc_table->lastpid = 1;

    /* Kernel task */
    l = kmalloc(sizeof(struct ktask_list));
    if ( NULL == l ) {
        return -1;
    }
    l->ktask = t->ktask;
    l->next = ktask_root->r;
    ktask_root->r = l;

    /* Create a process */
    ret = _create_process(t, (void *)(INITRAMFS_BASE + offset), size,
                          KTASK_POLICY_DRIVER);

    return ret;
}

/*
 * Create the pm server
 */
static int
_create_pm_server(void)
{
    u64 *initramfs = (u64 *)INITRAMFS_BASE;
    u64 offset = 0;
    u64 size;
    struct arch_task *t;
    struct ktask_list *l;
    int ret;

    /* Find the file pointed by path from the initramfs */
    while ( 0 != *initramfs ) {
        if ( 0 == kstrcmp((char *)initramfs, "/servers/pm") ) {
            offset = *(initramfs + 2);
            size = *(initramfs + 3);
            break;
        }
        initramfs += 4;
    }
    if ( 0 == offset ) {
        /* Could not find pm */
        return -1;
    }

    /* Create a process */
    t = kmalloc(sizeof(struct arch_task));
    t->ktask = kmalloc(sizeof(struct ktask));
    t->ktask->arch = t;
    t->ktask->proc = kmalloc(sizeof(struct proc));
    t->ktask->proc->policy = KTASK_POLICY_DRIVER;
    t->ktask->proc->arch = kmalloc(sizeof(struct arch_proc));
    t->ktask->next = NULL;
    ((struct arch_proc *)t->ktask->proc->arch)->proc = t->ktask->proc;
    t->kstack = kmalloc(KSTACK_SIZE);
    t->ustack = kmalloc(USTACK_SIZE);
    t->rp = t->kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);
    //t->ktask->state = KTASK_STATE_CREATED;
    t->ktask->state = KTASK_STATE_READY;

    /* Process table */
    proc_table->procs[0] = t->ktask->proc;
    proc_table->lastpid = 0;

    /* Kernel task */
    l = kmalloc(sizeof(struct ktask_list));
    if ( NULL == l ) {
        return -1;
    }
    l->ktask = t->ktask;
    l->next = ktask_root->r;
    ktask_root->r = l;

    /* Create a process */
    ret = _create_process(t, (void *)(INITRAMFS_BASE + offset), size,
                          KTASK_POLICY_DRIVER);

    return ret;
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
    idt_setup_intr_gate(6, intr_iof);
    idt_setup_intr_gate(13, intr_gpf);
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
    syscall_table[SYS_getpid] = sys_getpid;
    syscall_table[SYS_getppid] = sys_getppid;
    syscall_table[SYS_execve] = sys_execve;
    syscall_table[SYS_mmap] = sys_mmap;
    syscall_table[SYS_munmap] = sys_munmap;
    syscall_table[SYS_lseek] = sys_lseek;
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

    /* Initialize the task lists */
    ktask_root = kmalloc(sizeof(struct ktask_root));
    if ( NULL == ktask_root ) {
        panic("Fatal: Could not initialize the task lists.");
        return;
    }
    ktask_root->r = NULL;
    ktask_root->b = NULL;

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

    /* Launch the `init' server */
    cli();

    if ( _create_pm_server() < 0 ) {
        panic("Fatal: Cannot create the `pm' server.");
        return;
    }
    if ( _create_init_server() < 0 ) {
        panic("Fatal: Cannot create the `init' server.");
        return;
    }

    /* Schedule the idle task */
    this_cpu()->cur_task = NULL;
    this_cpu()->next_task = this_cpu()->idle_task;

    /* Start the idle task */
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
 * Create a new process
 */
int
_create_process(struct arch_task *t, void (*entry)(void), size_t size,
                int policy)
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

    /* Setup a page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return -1;
    }
    kmemset(pgt, 0, sizeof(struct page_entry) * (6 + 512));
    /* PML4 */
    pgt[0].entries[0] = (u64)&pgt[1] | 0x007;
    /* Pages for kernel space (0--1 GiB) */
    for ( i = 0; i < 1; i++ ) {
        pgt[1].entries[i] = (u64)&pgt[2 + i] | 0x007;
        /* PD */
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = (i << 30) | (j << 21) | 0x183;
        }
    }
    /* Pages for user space (1--3 GiB) */
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
    /* Pages for kernel space (3--4 GiB) */
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
    case KTASK_POLICY_SERVER:
    case KTASK_POLICY_USER:
    default:
        cs = GDT_RING3_CODE64_SEL + 3;
        ss = GDT_RING3_DATA_SEL + 3;
        flags = 0x3200;
        break;
    }

    /* Clean up memory space of the current process */
    kfree(t->kstack);
    kfree(t->ustack);
    t->rp = kstack = t->kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);
    //kmemset(t->rp, 0, sizeof(struct stackframe64));

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
    t->cr3 = (u64)pgt;

    /* Set the page table for the client */
    ((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

    return 0;
}

/*
 * Execute a process
 */
int
arch_exec(struct arch_task *t, void (*entry)(void), size_t size, int policy)
{
    int ret;

    /* Create a process */
    ret = _create_process(t, entry, size, policy);
    if ( ret < 0 ) {
        return -1;
    }

    /* Restart the task */
    task_replace(t);

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
 * Schedule the idle task as the next
 */
void set_next_idle(void)
{
    this_cpu()->next_task = this_cpu()->idle_task;
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
 * Clone the task
 */
struct ktask *
task_clone(struct ktask *ot)
{
    struct arch_task *t;

    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    t->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }
    t->ustack = kmalloc(USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask->arch = t;
    //t->rp = t->kstack + PAGESIZE - 16 - sizeof(struct stackframe64);

    struct page_entry *pgt;
    u64 i;
    u64 j;
    /* Setup page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return NULL;
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
    for ( i = 0; i < 512; i++ ) {
        /* Mapping */
        pgt[6].entries[i] = ((struct page_entry *)((struct arch_task *)ot->arch)
                             ->cr3)[6].entries[i];
    }
    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = ((u64)t->ustack + i * PAGESIZE) | 0x087;
    }

    t->cr3 = (u64)pgt;

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    kmemcpy(t->kstack, ((struct arch_task *)ot->arch)->kstack,
            KSTACK_SIZE);
    kmemcpy(t->ustack, ((struct arch_task *)ot->arch)->ustack,
            USTACK_SIZE);

    return t->ktask;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
