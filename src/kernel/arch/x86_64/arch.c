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

/* ACPI structure */
struct acpi arch_acpi;

/* Multiprocessor enabled */
int mp_enabled;

/* Kernel memory */
extern struct kmem *g_kmem;

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
panic(const char *s)
{
    int i;
    u16 *video;
    u16 val;

    /* Disable interrupt */
    cli();

    if ( mp_enabled ) {
        /* Notify other processors to stop */
        /* Send IPI and halt self */
        lapic_send_fixed_ipi(IV_CRASH);
    }

    /* Print out the message string directly */
    video = (u16 *)VIDEO_COLOR;
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
    int prox;

    /* Reset */
    mp_enabled = 0;

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
    kmemset(&arch_acpi, 0, sizeof(struct acpi));
    acpi_load(&arch_acpi);

    /* Set up interrupt vector */
    idt_setup_intr_gate(0, intr_dze);
    idt_setup_intr_gate(1, intr_debug);
    //idt_setup_intr_gate(2, intr_nmi);

    idt_setup_intr_gate(6, intr_iof);
    idt_setup_intr_gate(13, intr_gpf);
    idt_setup_intr_gate(14, intr_pf);
    idt_setup_intr_gate(16, intr_x87_fpe);
    idt_setup_intr_gate(19, intr_simd_fpe);
    idt_setup_intr_gate(IV_LOC_TMR, intr_apic_loc_tmr);
    idt_setup_intr_gate(IV_CRASH, intr_crash);

    /* ToDo: Prepare the virtual pages for ACPI etc. */

    /* Initialize I/O APIC */
    ioapic_init();

    /* Setup interrupt service routine then initialize I/O APIC */
    for ( i = 0; i < 32; i++ ) {
        ioapic_map_intr(IV_IRQ(i), i, arch_acpi.acpi_ioapic_base); /* IRQn */
    }

    /* Initialize the local APIC */
    lapic_init();

    /* Get the proximity domain */
    prox = acpi_lapic_prox_domain(&arch_acpi, lapic_id());

    /* Initialize the physical memory manager */
    if ( arch_memory_init(bi, &arch_acpi) < 0 ) {
        panic("Fatal: Could not initialize the memory manager.");
        return;
    }

    /* Load LDT */
    lldt(0);

    /* Initialize TSS */
    tss_init();
    tr_load(lapic_id());

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
    syscall_table[SYS_getuid] = sys_getuid;
    syscall_table[SYS_kill] = sys_kill;
    syscall_table[SYS_getppid] = sys_getppid;
    syscall_table[SYS_getgid] = sys_getgid;
    syscall_table[SYS_execve] = sys_execve;
    syscall_table[SYS_mmap] = sys_mmap;
    syscall_table[SYS_munmap] = sys_munmap;
    syscall_table[SYS_lseek] = sys_lseek;
    syscall_table[SYS_sysarch] = sys_sysarch;
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
    ktask_root->r.head = NULL;
    ktask_root->r.tail = NULL;
    ktask_root->b.head = NULL;
    ktask_root->b.tail = NULL;

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->prox_domain = prox;
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Set an idle task for this processor */
    pdata->idle_task = task_create_idle();
    if ( NULL == pdata->idle_task ) {
        panic("Fatal: Could not create the idle task for BSP.");
        return;
    }

    /* Initialize initramfs */
    if ( ramfs_init((u64 *)INITRAMFS_BASE) < 0 ) {
        panic("Fatal: Could not initialize the ramfs.");
        return;
    }

    /* Enable MP */
    mp_enabled = 1;

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

    if ( proc_create("/servers/pm", "pm", 0) < 0 ) {
        panic("Fatal: Cannot create the `pm' server.");
        return;
    }
    if ( proc_create("/servers/init", "init", 1) < 0 ) {
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
    int prox;

    /* Load global descriptor table */
    gdt_load();

    /* Load interrupt descriptor table */
    idt_load();

    /* Get the proximity domain */
    prox = acpi_lapic_prox_domain(&arch_acpi, lapic_id());

    /* Disable the global page feature */
    set_cr4(get_cr4() & ~CR4_PGE);
    /* Set the page table */
    set_cr3(((struct arch_vmem_space *)g_kmem->space->arch)->pgt);
    /* Enable the global page feature */
    set_cr4(get_cr4() | CR4_PGE);

    /* Enable this processor */
    pdata = this_cpu();
    pdata->cpu_id = lapic_id();
    pdata->prox_domain = prox;
    pdata->flags |= 1;

    /* Estimate the frequency */
    pdata->freq = lapic_estimate_freq();

    /* Set an idle task for this processor */
    pdata->idle_task = task_create_idle();
    if ( NULL == pdata->idle_task ) {
        panic("Fatal: Could not create the idle task for AP.");
        return;
    }

    /* Load LDT */
    lldt(0);

    /* Load TSS */
    tr_load(lapic_id());

    /* Initialize the local APIC */
    lapic_init();

    /* Run experiment */
    //run_experiment(lapic_id());
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
arch_exec(struct arch_task *t, void (*entry)(void), size_t size, int policy,
          char *const argv[], char *const envp[])
{
    int argc;
    char *const *tmp;
    size_t len;

    /* Count the number of arguments */
    tmp = argv;
    argc = 0;
    len = 0;
    while ( NULL != *tmp ) {
        argc++;
        len += kstrlen(*tmp);
        tmp++;
    }

    /* Prepare arguments */
    u8 *arg;
    len += argc + (argc + 1) * sizeof(void *);
    if ( len < PAGESIZE ) {
        /* FIXME: Replace kmalloc with vmalloc */
        arg = kmalloc(PAGESIZE);
    } else {
        arg = kmalloc(len);
    }
    if ( NULL == arg ) {
        return -1;
    }
    char **narg;
    u8 *saved;
    tmp = argv;
    narg = (char **)arg;
    saved = arg + sizeof(void *) * (argc + 1);
    while ( NULL != *tmp ) {
        *narg = (char *)(saved - arg + 0x7fc00000ULL);
        kmemcpy(saved, *tmp, kstrlen(*tmp));
        saved[kstrlen(*tmp)] = '\0';
        saved += kstrlen(*tmp) + 1;
        tmp++;
        narg++;
    }
    *narg = NULL;



    u64 cs;
    u64 ss;
    u64 flags;
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

    kmemcpy((void *)CODE_INIT, entry, PAGESIZE);
    kmemset(t->rp, 0, sizeof(struct stackframe64));
    /* Replace the current process with the new process */
    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;
    t->rp->gs = ss;
    t->rp->fs = ss;
    t->rp->sp = USTACK_INIT + USTACK_SIZE - 16;
    t->rp->ss = ss;
    t->rp->cs = cs;
    t->rp->ip = CODE_INIT;
    t->rp->flags = flags;

    /* FIXME */
    t->rp->di = 0;//argc;
    t->rp->si = 0x7fc00000ULL;

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
arch_task_switched(struct arch_task *prev, struct arch_task *next)
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
 * Debug fault/trap
 */
void
isr_debug(void)
{
    char *buf = this_ktask()->proc->name;
    panic(buf);
}

/*
 * Invalid opcode fault
 */
void
isr_io_fault(void *rip)
{
    char buf[128];
    u64 x = (u64)rip;

    ksnprintf(buf, sizeof(buf), "Invalid Opcode Fault: %016x", x);
    panic(buf);
}

/*
 * General protection fault
 */
void
isr_general_protection_fault(void *rip, u64 error)
{
    char buf[128];
    u64 x = (u64)rip;

    ksnprintf(buf, sizeof(buf), "FIXME: General Protection Fault (%d): %016x",
              error, x);
    panic(buf);
}

/*
 * Page fault handler
 */
void
isr_page_fault(void *rip, void *addr, u64 error)
{
    char buf[128];
    u64 x = (u64)rip;
    u64 y = (u64)addr;

    ksnprintf(buf, sizeof(buf), "Page Fault (%d): %016x @%016x", error, y, x);
    panic(buf);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
