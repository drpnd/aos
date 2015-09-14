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
#include "../../kernel.h"
#include "arch.h"
#include "memory.h"

#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)

int vmem_remap(struct vmem_space *, u64, u64, int);

/*
 * Create a new task
 */
struct ktask *
task_new(void)
{
    struct arch_task *t;

    /* Allocate the architecture-specific task structure of a new task */
    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    /* Allocate the kernel task structure of a new task */
    t->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }
    /* Allocate the user stack of a new task */
    t->ustack = kmalloc(USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    /* Allocate the kernel stack of a new task */
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask->arch = t;

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    return t->ktask;
}

/*
 * Clone the task
 */
struct ktask *
task_clone(struct ktask *ot)
{
    struct arch_task *t;
    struct page_entry *pgt;
    u64 i;
    u64 j;

    /* Allocate the architecture-specific task structure of a new task */
    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    /* Allocate the kernel task structure of a new task */
    t->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }
    /* Allocate the user stack of a new task */
    t->ustack = kmalloc(USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    /* Allocate the kernel stack of a new task */
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }
    t->ktask->arch = t;

    /* Copy the kernel and user stacks */
    kmemcpy(t->kstack, ((struct arch_task *)ot->arch)->kstack,
            KSTACK_SIZE);
    kmemcpy(t->ustack, ((struct arch_task *)ot->arch)->ustack,
            USTACK_SIZE);

    /* Setup the restart point */
    t->rp = (struct stackframe64 *)
        ((u64)((struct arch_task *)ot->arch)->rp + (u64)t->kstack
         - (u64)((struct arch_task *)ot->arch)->kstack);

    /* Copy the user memory space */
    /* Setup page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return NULL;
    }
    kmemset(pgt, 0, sizeof(struct page_entry) * (6 + 512));
    /* Kernel */
    pgt[0].entries[0] = kmem_paddr((u64)&pgt[1]) | 0x007;
    /* PDPT */
    for ( i = 0; i < 1; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }
    /* PT (1GB-- +2MiB) */
    for ( i = 1; i < 2; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    pgt[2 + 1].entries[0] = kmem_paddr((u64)&pgt[6]) | 0x007;
    pgt[2 + 1].entries[510] = kmem_paddr((u64)&pgt[516]) | 0x007;
    pgt[2 + 1].entries[511] = kmem_paddr((u64)&pgt[517]) | 0x007;
    for ( i = 2; i < 3; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            /* Not present */
            pgt[2 + i].entries[j] = 0x000;
        }
    }
#if 0
    /* Kernel */
    for ( i = 3; i < 4; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }
    /* Executable */
    for ( i = 0; i < 512; i++ ) {
        /* Mapping */
        pgt[6].entries[i] = ((struct page_entry *)((struct arch_proc *)ot->proc
                                                   ->arch)->pgt)[6].entries[i];
    }
    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = (kmem_paddr((u64)t->ustack) + i * PAGESIZE) | 0x087;
    }
    /* Arguments */
    pgt[516].entries[0] = ((struct page_entry *)((struct arch_proc *)ot->proc
                                                 ->arch)->pgt)[516].entries[0];
#endif
    t->cr3 = kmem_paddr((u64)pgt);

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    /* Set the page table for the process */
    //((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

    return t->ktask;
}




/*
 * Create a new process
 */
int
proc_create_(struct arch_task *t, void (*entry)(void), size_t size,
            int policy, void *argpg)
{
    u64 cs;
    u64 ss;
    u64 flags;
    struct page_entry *pgt;
    u64 i;
    u64 j;
    u64 pg;
    void *exec;
    void *kstack;
    void *ustack;
    struct vmem_space *vm;

    /* Allocate a new virtual memory space */
    vm = vmem_space_create();
    if ( NULL == vm ) {
        return -1;
    }



    /* Setup a page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return -1;
    }
    kmemset(pgt, 0, sizeof(struct page_entry) * (6 + 512));
    /* PML4 */
    pgt[0].entries[0] = kmem_paddr((u64)&pgt[1]) | 0x007;
    /* Pages for kernel space (0--1 GiB) */
    for ( i = 0; i < 1; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }
    /* Pages for user space (1--3 GiB) */
    for ( i = 1; i < 2; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    pgt[2 + 1].entries[0] = kmem_paddr((u64)&pgt[6]) | 0x007;
    pgt[2 + 1].entries[510] = kmem_paddr((u64)&pgt[516]) | 0x007;
    pgt[2 + 1].entries[511] = kmem_paddr((u64)&pgt[517]) | 0x007;
    for ( i = 2; i < 3; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            /* Not present */
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    /* Pages for kernel space (3--4 GiB) */
    for ( i = 3; i < 4; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }

    /* Program */
    pg = CEIL(size, PAGESIZE) / PAGESIZE;
    for ( i = 0; i < pg; i++ ) {
        exec = kmalloc(PAGESIZE);
        if ( NULL == exec ) {
            /* FIXME: free */
            kfree(pgt);
            return -1;
        }
        /* Copy the executable memory */
        (void)kmemcpy(exec, entry + i * PAGESIZE, PAGESIZE);
        pgt[6].entries[i] = kmem_paddr((u64)exec) | 0x087;
    }
    /* Stack */
    kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == kstack ) {
        kfree(pgt);
        return -1;
    }
    ustack = kmalloc(USTACK_SIZE);
    if ( NULL == ustack ) {
        kfree(pgt);
        kfree(kstack);
        return -1;
    }

    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = (kmem_paddr((u64)ustack) + i * PAGESIZE) | 0x087;
    }
    /* Arguments */
    pgt[516].entries[0] = kmem_paddr((u64)argpg) | 0x087;

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
    t->rp = kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);
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
    t->cr3 = kmem_paddr((u64)pgt);

    /* Set the page table for the client */
    //((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

    return 0;
}

/*
 * Clone a process of the context specified by the ot argument
 */
struct proc *
proc_clone(struct ktask *ot)
{
    struct proc *np;
    struct arch_task *nt;

    /* Allocate a new process */
    np = kmalloc(sizeof(struct proc));
    if ( NULL == np ) {
        return NULL;
    }
    kmemset(np, 0, sizeof(struct proc));
    kmemcpy(np->name, ot->proc->name, 1024); /* FIXME */

    /* Allocate the architecture-specific task structure of a new task */
    nt = kmalloc(sizeof(struct arch_task));
    if ( NULL == nt ) {
        kfree(np);
        return NULL;
    }
    /* Allocate the kernel stack of the new task */
    nt->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == nt->kstack ) {
        kfree(nt);
        kfree(np);
        return NULL;
    }
    /* Allocate the user stack of the new task */
    nt->ustack = kmalloc(USTACK_SIZE);
    if ( NULL == nt->ustack ) {
        kfree(nt->kstack);
        kfree(nt);
        kfree(np);
        return NULL;
    }
    /* Allocate the kernel task of the new task */
    nt->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == nt->ktask ) {
        kfree(nt->ustack);
        kfree(nt->kstack);
        kfree(nt);
        kfree(np);
        return NULL;
    }
    /* Create the bidirectional link */
    nt->ktask->arch = nt;

    /* Copy the kernel and user stacks */
    kmemcpy(nt->kstack, ((struct arch_task *)ot->arch)->kstack,
            KSTACK_SIZE);

    /* Setup the restart point */
    nt->rp = (struct stackframe64 *)
        ((u64)((struct arch_task *)ot->arch)->rp + (u64)nt->kstack
         - (u64)((struct arch_task *)ot->arch)->kstack);



    return NULL;
#if 0
    struct page_entry *pgt;
    u64 i;
    u64 j;


    /* Copy the user memory space */
    /* Setup page table */
    pgt = kmalloc(sizeof(struct page_entry) * (6 + 512));
    if ( NULL == pgt ) {
        return NULL;
    }
    kmemset(pgt, 0, sizeof(struct page_entry) * (6 + 512));
    /* Kernel */
    pgt[0].entries[0] = kmem_paddr((u64)&pgt[1]) | 0x007;
    /* PDPT */
    for ( i = 0; i < 1; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }
    /* PT (1GB-- +2MiB) */
    for ( i = 1; i < 2; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    pgt[2 + 1].entries[0] = kmem_paddr((u64)&pgt[6]) | 0x007;
    pgt[2 + 1].entries[510] = kmem_paddr((u64)&pgt[516]) | 0x007;
    pgt[2 + 1].entries[511] = kmem_paddr((u64)&pgt[517]) | 0x007;
    for ( i = 2; i < 3; i++ ) {
        pgt[1].entries[i] = kmem_paddr((u64)&pgt[2 + i]) | 0x007;
        for ( j = 0; j < 512; j++ ) {
            /* Not present */
            pgt[2 + i].entries[j] = 0x000;
        }
    }
    /* Kernel */
    for ( i = 3; i < 4; i++ ) {
        pgt[1].entries[i] = ((u64)KERNEL_PGT + 4096 * (2 + i)) | 0x007;
    }
    /* Executable */
    for ( i = 0; i < 512; i++ ) {
        /* Mapping */
        pgt[6].entries[i] = ((struct page_entry *)((struct arch_proc *)ot->proc
                                                   ->arch)->pgt)[6].entries[i];
    }
    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = (kmem_paddr((u64)t->ustack) + i * PAGESIZE) | 0x087;
    }
    /* Arguments */
    pgt[516].entries[0] = ((struct page_entry *)((struct arch_proc *)ot->proc
                                                 ->arch)->pgt)[516].entries[0];

    t->cr3 = kmem_paddr((u64)pgt);

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    /* Set the page table for the process */
    //((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

    return t->ktask;
#endif
}


/*
 * Create an idle task
 */
struct arch_task *
task_create_idle(void)
{
    struct arch_task *t;

    /* Allocate and initialize the architecture-specific kernel task */
    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        return NULL;
    }
    kmemset(t, 0, sizeof(struct arch_task));

    /* Page table for the kernel */
    t->cr3 = KERNEL_PGT;

    /* Kernel stack */
    t->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        kfree(t);
        return NULL;
    }

    /* User stack (in the kernel space) */
    t->ustack = kmalloc(USTACK_SIZE);
    if ( NULL == t->ustack ) {
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }

    /* Kernel task */
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        kfree(t->ustack);
        kfree(t->kstack);
        kfree(t);
        return NULL;
    }

    /* Create a bidirectional link */
    t->ktask->arch = t;

    /* No process associated with the idle task */
    t->ktask->proc = NULL;

    /* Set the task state to ready */
    t->ktask->state = KTASK_STATE_READY;

    /* Setup the restart point */
    t->rp = t->kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);

    /* The idle task runs at ring 0. */
    t->rp->cs = GDT_RING0_CODE_SEL;
    t->rp->ss = GDT_RING0_DATA_SEL;

    /* Entry point, user/kernel stack, and flags of the idle task */
    t->rp->ip = (u64)arch_idle;
    t->rp->sp = (u64)t->ustack + USTACK_SIZE - 16;
    t->rp->flags = 0x0200;
    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    return t;
}

/*
 * Create an init server process
 */
int
proc_create_init(void)
{
    u64 *initramfs = (u64 *)INITRAMFS_BASE;
    u64 offset = 0;
    u64 size;
    struct arch_task *t;
    struct ktask_list *l;
    int ret;
    const char *path;
    struct proc *proc;
    struct pmem_superpage *ppage;
    void *paddr;

    /* Find the file pointed by path from the initramfs */
    path = "/servers/init";
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

    /* New process */
    proc = kmalloc(sizeof(struct proc));
    if ( NULL == proc ) {
        goto error_proc;
    }
    kmemset(proc, 0, sizeof(struct proc));

    /* Set the process name */
    kstrlcpy(proc->name, "init", PATH_MAX);

    /* Set the policy */
    proc->policy = KTASK_POLICY_USER;

    /* Create a virtual memory space */
    proc->vmem = vmem_space_create();
    if ( NULL == proc->vmem ) {
        goto error_vmem;
    }

    /* Set the page table */
    set_cr3((void *)kmem_paddr(
                (u64)((struct arch_vmem_space *)proc->vmem->arch)->pgt));

    /* Create an architecture-specific task data structure */
    t = kmalloc(sizeof(struct arch_task));
    if ( NULL == t ) {
        goto error_arch_task;
    }
    kmemset(t, 0, sizeof(struct arch_task));

    /* Create a task */
    t->ktask = kmalloc(sizeof(struct ktask));
    if ( NULL == t->ktask ) {
        goto error_task;
    }
    kmemset(t->ktask, 0, sizeof(struct ktask));
    t->ktask->arch = t;

    /* Associate the task with a process */
    t->ktask->proc = proc;

    /* Prepare the kernel stack */
    t->kstack = kmalloc(KSTACK_SIZE);
    if ( NULL == t->kstack ) {
        goto error_kstack;
    }

    /* Prepare the user stack */
    ppage = pmem_alloc_superpage(0);
    if ( NULL == ppage ) {
        goto error_ustack;
    }
    paddr = pmem_superpage_address(ppage);
    t->ustack = (void *)USTACK_INIT;
    vmem_remap(proc->vmem, (u64)t->ustack, (u64)paddr, 1);

    /* Prepare exec */
    ppage = pmem_alloc_superpage(0);
    if ( NULL == ppage ) {
        //goto error_exec;
        return -1;
    }
    paddr = pmem_superpage_address(ppage);
    void *exec;
    exec = (void *)CODE_INIT;
    vmem_remap(proc->vmem, (u64)exec, (u64)paddr, 1);
    (void)kmemcpy(exec, (void *)(INITRAMFS_BASE + offset), size);

    t->rp = t->kstack + KSTACK_SIZE - 16 - sizeof(struct stackframe64);
    kmemset(t->rp, 0, sizeof(struct stackframe64));
    t->ktask->state = KTASK_STATE_READY;

    /* Process table */
    proc_table->procs[1] = t->ktask->proc;
    proc_table->lastpid = 1;

    /* Kernel task */
    l = kmalloc(sizeof(struct ktask_list));
    if ( NULL == l ) {
        goto error_tl;
    }
    l->ktask = t->ktask;
    l->next = NULL;
    /* Push */
    if ( NULL == ktask_root->r.head ) {
        ktask_root->r.head = l;
        ktask_root->r.tail = l;
    } else {
        ktask_root->r.tail->next = l;
        ktask_root->r.tail = l;
    }


    u64 cs;
    u64 ss;
    u64 flags;
    int policy = KTASK_POLICY_USER;

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

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;
    t->rp->gs = ss;
    t->rp->fs = ss;
    t->rp->sp = USTACK_INIT + USTACK_SIZE - 16;
    t->rp->ss = ss;
    t->rp->cs = cs;
    t->rp->ip = CODE_INIT;
    t->rp->flags = flags;
    t->cr3 = kmem_paddr((u64)((struct arch_vmem_space *)proc->vmem->arch)->pgt);

    return 0;

error_tl:
    pmem_free_superpages(ppage);
error_ustack:
    kfree(t->kstack);
error_kstack:
    kfree(t->ktask);
error_task:
    kfree(t);
error_arch_task:
    vmem_space_delete(proc->vmem);
error_vmem:
    kfree(proc);
error_proc:
    return -1;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
