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

#define KSTACK_SIZE     PAGESIZE
#define USTACK_SIZE     (PAGESIZE * 16)

/*
 * Clone the task
 */
struct ktask *
task_clone(struct ktask *ot)
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
    struct page_entry *pgt;
    u64 i;
    u64 j;
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
        pgt[6].entries[i] = ((struct page_entry *)((struct arch_task *)ot->arch)
                             ->cr3)[6].entries[i];
    }
    /* Setup the page table for user stack */
    for ( i = 0; i < (USTACK_SIZE - 1) / PAGESIZE + 1; i++ ) {
        pgt[517].entries[511 - (USTACK_SIZE - 1) / PAGESIZE + i]
            = (kmem_paddr((u64)t->ustack) + i * PAGESIZE) | 0x087;
    }
    /* Arguments */
    pgt[516].entries[0] = ((struct page_entry *)
                           ((struct arch_proc *)ot->proc->arch)
                           ->pgt)[516].entries[0];

    t->cr3 = kmem_paddr((u64)pgt);

    t->sp0 = (u64)t->kstack + KSTACK_SIZE - 16;

    /* Set the page table for the process */
    ((struct arch_proc *)t->ktask->proc->arch)->pgt = pgt;

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
