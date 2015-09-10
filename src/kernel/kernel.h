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

#ifndef _KERNEL_H
#define _KERNEL_H

#include <aos/const.h>
#include <aos/types.h>
#include <sys/resource.h>

/* Page size */
#define PAGESIZE                4096
#define SUPERPAGESIZE           (1ULL << 21)

#define PMEM_USED               1ULL            /* Managed by buddy system */
#define PMEM_WIRED              (1ULL<<1)       /* Wired (kernel use) */
#define PMEM_ALLOC              (1ULL<<2)       /* Allocated */
#define PMEM_SLAB               (1ULL<<3)       /* For slab */
#define PMEM_UNAVAIL            (1ULL<<16)      /* Unavailable space */
#define PMEM_IS_FREE(x)         (0 == (x)->flags ? 1 : 0)

#define PMEM_NUMA_MAX_DOMAINS   256
#define PMEM_MAX_BUDDY_ORDER    9

/* 32 (2^5) byte is the minimum object size of a slab object */
#define KMEM_SLAB_BASE_ORDER    5
/* 1024 (2^(5 + 6 - 1)) byte is the maximum object size of a slab object */
#define KMEM_SLAB_ORDER         6
/* 2^16 objects in a cache */
#define KMEM_SLAB_NR_OBJ_ORDER  4

#define KMEM_MAX_BUDDY_ORDER    9
#define KMEM_REGION_SIZE        512

#define VMEM_MAX_BUDDY_ORDER    9


/* Process table size */
#define PROC_NR                 65536

/* Maximum number of file descriptors */
#define FD_MAX                  1024

/* Task policy */
#define KTASK_POLICY_KERNEL     0
#define KTASK_POLICY_DRIVER     1
#define KTASK_POLICY_SERVER     2
#define KTASK_POLICY_USER       3

/* Tick */
#define HZ                      100
#define IV_LOC_TMR              0x50
#define IV_CRASH                0xfe
#define NR_IV                   0x100
#define IV_IRQ(n)               (0x20 + (n))

/*
 * File descriptor
 */
struct fildes {
    void *data;
    off_t pos;
    ssize_t (*read)(struct fildes *, void *, size_t);
    ssize_t (*write)(struct fildes *, const void *, size_t);
    off_t (*lseek)(struct fildes *, off_t, int);
};

/*
 * Virtual page
 */
struct vmem_page {
    reg_t addr;
    int type;
    /* Buddy system */
    struct vmem_page *next;
};

/*
 * Virtual memory region
 */
struct vmem_region {
    /* Region */
    ptr_t start;
    size_t len;

    /* Pages belonging to this region */
    struct vmem_page *pages;

    /* Buddy system */
    struct vmem_page *heads[VMEM_MAX_BUDDY_ORDER + 1];

    /* Pointer to the next region */
    struct vmem_region *next;
};

/*
 * Virtual memory space
 */
struct vmem_space {
    /* Virtual memory region */
    struct vmem_region *first_region;

    /* Architecture specific data structure (e.g., page table)  */
    void *arch;
};

/*
 * Physical page structure (superpage)
 */
struct pmem_superpage {
    u32 flags;
    int prox_domain;
    int refcnt;
    /* Buddy system */
    int order;
    struct pmem_superpage *prev;
    struct pmem_superpage *next;
};

/*
 * Buddy system
 */
struct pmem_buddy {
    struct pmem_superpage *heads[PMEM_MAX_BUDDY_ORDER + 1];
};

/*
 * NUMA domain
 */
struct pmem_numa_domain {
    struct pmem_buddy buddy;
};

/*
 * Physical memory
 */
struct pmem {
    /* Lock */
    spinlock_t lock;
    /* The number of superpages */
    size_t nr;
    /* Superpages (maintaining flags, proximity domain etc.) */
    struct pmem_superpage *superpages;
    /* NUMA domains */
    struct pmem_numa_domain domains[PMEM_NUMA_MAX_DOMAINS];
};

/*
 * A slab object
 */
struct kmem_slab_obj {
    void *addr;
} __attribute__ ((packed));

/*
 * Slab objects
 *  slab_hdr
 *    object 0
 *    object 1
 *    ...
 */
struct kmem_slab {
    /* slab_hdr */
    struct kmem_slab *next;
    int nr;
    int nused;
    int free;
    void *obj_head;
    /* Free marks follows (nr byte) */
    u8 marks[1];
    /* Objects follows */
} __attribute__ ((packed));

/*
 * Free list of slab objects
 */
struct kmem_slab_free_list {
    struct kmem_slab *partial;
    struct kmem_slab *full;
    struct kmem_slab *free;
} __attribute__ ((packed));

/*
 * Root data structure of slab objects
 */
struct kmem_slab_root {
    /* Generic slabs */
    struct kmem_slab_free_list gslabs[KMEM_SLAB_ORDER];
};

/*
 * Kernel memory page
 */
struct kmem_page {
    reg_t address;
    int type;
    /* Buddy system */
    struct kmem_page *next;
};

/*
 * Kernel memory
 */
struct kmem {
    /* Lock */
    spinlock_t lock;
    spinlock_t slab_lock;
    /* Regions */
    struct kmem_page region1[KMEM_REGION_SIZE];
    struct kmem_page region2[KMEM_REGION_SIZE];
    /* Buddy system */
    struct kmem_page *heads[KMEM_MAX_BUDDY_ORDER + 1];

    /* Slab allocator */
    struct kmem_slab_root slab;
};









/*
 * Pager
 */
typedef void * (*page_alloc_f)(void);
typedef void (*page_free_f)(void *);
struct pager {
    page_alloc_f *alloc_page;
    page_free_f *free_page;
};

/*
 * Process
 */
struct proc {
    /* Process ID */
    pid_t id;

    /* Name */
    char *name;

    /* Parent process */
    struct proc *parent;

    /* User information */
    uid_t uid;
    gid_t gid;

    /* Architecture specific structure; i.e., (struct arch_proc) */
    void *arch;

    /* Memory */
    struct vmem_space *vmem;

    /* Policy */
    int policy;

    /* File descriptors */
    struct fildes *fds[FD_MAX];
};

/*
 * Process table
 */
struct proc_table {
    /* Process table */
    struct proc *procs[PROC_NR];
    /* pid last assigned (to find the next pid by sequential search) */
    pid_t lastpid;
};

/*
 * Kernel task state
 */
enum ktask_state {
    KTASK_STATE_CREATED,
    KTASK_STATE_READY,
    KTASK_STATE_BLOCKED,
    KTASK_STATE_TERMINATED,
};

/*
 * Kernel task data structure
 */
struct ktask {
    /* Architecture specific structure; i.e., (struct arch_task) */
    void *arch;
    /* State */
    enum ktask_state state;

    /* Process */
    struct proc *proc;

    /* Task type: Tick-full or tickless */
    int type;
    /* Pointers for scheduler (runqueue) */
    struct ktask *next;
    int credit;                 /* quantum */
};

/*
 * Kernel task list
 */
struct ktask_list {
    struct ktask *ktask;
    struct ktask_list *next;
};
struct ktask_root {
    /* Running */
    struct {
        struct ktask_list *head;
        struct ktask_list *tail;
    } r;
    /* Blocked (or others) */
    struct {
        struct ktask_list *head;
        struct ktask_list *tail;
    } b;
};

/* Kernel event handler */
typedef void (*kevent_handler_f)(void);
struct kevent_handlers {
    /* Interrupt vector table */
    kevent_handler_f ivt[NR_IV];
};

/* Global variable */
extern struct pmem *pmem;
extern struct proc_table *proc_table;
extern struct ktask_root *ktask_root;

/* in kernel.c */
void kernel(void);
int kstrcmp(const char *, const char *);
size_t kstrlen(const char *);

/* in asm.s */
#define HAS_KMEMSET     1       /* kmemset is implemented in asm.s. */
#define HAS_KMEMCMP     1       /* kmemcmp is implemented in asm.s. */
#define HAS_KMEMCPY     1       /* kmemcpy is implemented in asm.s. */
void * kmemset(void *, int, size_t);
int kmemcmp(const void *, const void *, size_t);
void * kmemcpy(void *__restrict, const void *__restrict, size_t);

/* in sched.c */
void sched_high(void);

/* in memory.c */
int pmem_init(struct pmem *);
struct pmem_superpage * pmem_alloc_superpages(int, int);
struct pmem_superpage * pmem_alloc_superpage(int);
void * pmem_superpage_address(struct pmem_superpage *);
void pmem_free_superpages(struct pmem_superpage *);
int kmem_init(void);
void * kmalloc(size_t);
void kfree(void *);
struct vmem_region * vmem_region_create(void);
struct vmem_space * vmem_space_create(void);

/* in ramfs.c */
int ramfs_init(u64 *);

/* in syscall.c */
void sys_exit(int);
ssize_t sys_read(int, void *, size_t);
ssize_t sys_write(int, const void *, size_t);
int sys_open(const char *, int, ...);
int sys_close(int);
pid_t sys_wait4(pid_t, int *, int, struct rusage *);
pid_t sys_getpid(void);
uid_t sys_getuid(void);
int sys_kill(pid_t, int);
pid_t sys_getppid(void);
gid_t sys_getgid(void);
int sys_execve(const char *, char *const [], char *const []);
void * sys_mmap(void *, size_t, int, int, int, off_t);
int sys_munmap(void *, size_t);
off_t sys_lseek(int, off_t, int);

/* The followings are mandatory functions for the kernel and should be
   implemented somewhere in arch/<arch_name>/ */
struct ktask * this_ktask(void);
void set_next_ktask(struct ktask *);
void set_next_idle(void);
void panic(char *);
void halt(void);
struct ktask * task_clone(struct ktask *);
void task_set_return(struct ktask *, unsigned long long);
pid_t sys_fork(void);
void spin_lock(u32 *);
void spin_unlock(u32 *);

#endif /* _KERNEL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
