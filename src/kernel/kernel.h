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

#define FLOOR(val, base)        (((val) / (base)) * (base))
#define CEIL(val, base)         ((((val) - 1) / (base) + 1) * (base))
#define DIV_FLOOR(val, base)    ((val) / (base))
#define DIV_CEIL(val, base)     (((val) - 1) / (base) + 1)

/* Page size: Must be consistent with the architecture's page size */
#define PAGESIZE                4096ULL         /* 4 KiB */
#define SUPERPAGESIZE           (1ULL << 21)    /* 2 MiB */

#define PAGE_ADDR(i)            (PAGESIZE * (i))
#define SUPERPAGE_ADDR(i)       (SUPERPAGESIZE * (i))
#define PAGE_INDEX(a)           ((u64)(a) / PAGESIZE)
#define SUPERPAGE_INDEX(a)      ((u64)(a) / SUPERPAGESIZE)

#define PMEM_USABLE             (1)             /* Usable */
#define PMEM_USED               (1<<1)          /* Used */
#define PMEM_IS_FREE(x)         (PMEM_USABLE == (x)->flags ? 1 : 0)

#define PMEM_MAX_BUDDY_ORDER    18
#define PMEM_INVAL_BUDDY_ORDER  0x3f

#define PMEM_NUMA_MAX_DOMAINS   16
#define PMEM_ZONE_DMA           0
#define PMEM_ZONE_LOWMEM        1
#define PMEM_ZONE_UMA           2
#define PMEM_ZONE_NUMA(d)       (3 + (d))
#define PMEM_NUM_ZONES          (3 + PMEM_NUMA_MAX_DOMAINS)
#define PMEM_INVAL_INDEX        0xffffffffUL


/* 32 (2^5) -byte is the minimum object size of a slab object */
#define KMEM_SLAB_BASE_ORDER    5
/* 1024 (2^(5 + 6 - 1)) byte is the maximum object size of a slab object */
#define KMEM_SLAB_ORDER         6
/* 2^16 objects in a cache */
#define KMEM_SLAB_NR_OBJ_ORDER  4

#define KMEM_MAX_BUDDY_ORDER    21
//#define KMEM_REGION_SIZE        512

#define VMEM_MAX_BUDDY_ORDER    18
#define VMEM_INVAL_BUDDY_ORDER  0x3f

#define VMEM_USABLE             (1)
#define VMEM_USED               (1<<1)
#define VMEM_GLOBAL             (1<<2)
#define VMEM_SUPERPAGE          (1<<3)
#define VMEM_IS_FREE(x)         (VMEM_USABLE == ((x)->flags & 0x3))
#define VMEM_IS_SUPERPAGE(x)    (VMEM_SUPERPAGE & (x)->flags)

#define INITRAMFS_BASE          0x20000ULL
#define USTACK_INIT             0xbfe00000ULL
#define CODE_INIT               0x40000000ULL
#define KSTACK_SIZE             4096
#define USTACK_SIZE             (4096 * 16)

/* Process table size */
#define PROC_NR                 65536

/* Maximum number of file descriptors */
#define FD_MAX                  1024

/* Maximum bytes in the path name */
#define PATH_MAX                1024

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


#define ENOENT                  2
#define EINTR                   4
#define EIO                     5
#define ENOEXEC                 8
#define EBADF                   9
#define ENOMEM                  12
#define EACCES                  13
#define EFAULT                  14
#define EINVAL                  22

/*
 * String
 */
struct kstring {
    void *base;
    size_t sz;
};

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
    /* Physical address: least significant bits are plan to be used for flags */
    reg_t addr;
    /* Order */
    int order;
    /* Flags */
    int flags;
    /* Back-link to the corresponding superpage */
    struct vmem_superpage *superpage;
    /* Buddy system */
    //struct vmem_page *next;
    //struct vmem_page *prev;
} __attribute__ ((packed));

/*
 * Virtual superpage
 */
struct vmem_superpage {
    union {
        /* Superpage */
        struct {
            /* Physical address */
            reg_t addr;
        } superpage;
        /* Page */
        struct {
            /* Pages */
            struct vmem_page *pages;
        } page;
    } u;
    /* Order */
    int order;
    /* Flags */
    int flags;
    /* Back-link to the corresponding region */
    struct vmem_region *region;
    /* Buddy system */
    struct vmem_superpage *next;
    struct vmem_superpage *prev;
};

/*
 * Virtual memory region
 */
struct vmem_region {
    /* Region information */
    ptr_t start;
    size_t len;                 /* Constant multiplication of SUPERPAGESIZE */

    /* Capacity and the number of used pages */
    //size_t total_pgs;
    //size_t used_pgs;

    /* Superpages belonging to this region */
    //struct vmem_page *pages;
    struct vmem_superpage *superpages;

    /* Buddy system */
    struct vmem_superpage *heads[VMEM_MAX_BUDDY_ORDER + 1];

    /* Pointer to the next region */
    struct vmem_region *next;
};

/*
 * Virtual memory space
 */
struct vmem_space {
    /* Virtual memory region */
    struct vmem_region *first_region;

    /* Virtual page table */
    void *vmap;

    /* Architecture specific data structure (e.g., page table)  */
    void *arch;
};

/*
 * Physical page
 */
struct pmem_page {
    u16 zone;
    u8 flags;
    /* Buddy system */
    u8 order;
    u32 next;
} __attribute__((packed));

/*
 * Buddy system
 */
struct pmem_buddy {
    u32 heads[PMEM_MAX_BUDDY_ORDER + 1];
    //struct pmem_page *heads[PMEM_MAX_BUDDY_ORDER + 1];
};

/*
 * Memory zone
 */
struct pmem_zone {
    /* Buddy system */
    struct pmem_buddy buddy;
    /* Statistics */
    size_t total;
    size_t used;
};

/*
 * Protocol to operate the physical memory
 */
struct pmem_proto {
    /* Allocate 2^order pages from a particular domain */
    void * (*alloc_pages)(int domain, int order);
    /* Allocate a page from a particular domain */
    void * (*alloc_page)(int domain);
    /* Free pages */
    void (*free_pages)(void *page);
};

/*
 * Physical memory
 */
struct pmem {
    /* Lock variable for physical memory operations */
    spinlock_t lock;

    /* The number of pages */
    size_t nr;

    /* Physical pages */
    struct pmem_page *pages;

    /* Zones (NUMA domains) */
    struct pmem_zone zones[PMEM_NUM_ZONES];
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
 * Free pages in kmem region
 */
struct kmem_free_page {
    void *paddr;
    struct kmem_free_page *next;
};

/*
 * Kernel memory
 */
struct kmem {
    /* Lock */
    spinlock_t lock;
    spinlock_t slab_lock;

    /* Slab allocator */
    struct kmem_slab_root slab;

    /* Architecture-specific data structure of kernel memory */
    void *arch;

    /* Virtual memory */
    struct vmem_space *space;

    /* Free pages */
    struct kmem_free_page *free_pgs;

    /* Physical memory */
    struct pmem *pmem;
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
    char name[PATH_MAX];

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

/* Global variables */
struct kernel_variables {
    struct pmem *pmem;
    struct proc_table *proc_table;
    struct ktask_root *ktask_root;
};

/* Global variable */
extern struct pmem *pmem;
extern struct proc_table *proc_table;
extern struct ktask_root *ktask_root;

/* in kernel.c */
void kernel(void);
int kstrcmp(const char *, const char *);
size_t kstrlen(const char *);
char * kstrcpy(char *, const char *);
char * kstrncpy(char *, const char *, size_t);
size_t kstrlcpy(char *, const char *, size_t);

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
int kmem_init(void);
void * kmalloc(size_t);
void kfree(void *);
struct vmem_region * vmem_region_create(void);
struct vmem_space * vmem_space_create(void);
void vmem_space_delete(struct vmem_space *);

int vmem_buddy_init(struct vmem_region *);
void * vmem_alloc_pages(struct vmem_space *, int);
void vmem_free_pages(struct vmem_space *, void *);
void * vmem_buddy_alloc(struct vmem_space *, int);
void vmem_buddy_free(struct vmem_space *, void *);

void * pmem_alloc_pages(int, int);
void pmem_free_pages(void *);

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
int sys_sysarch(int, void *);

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
