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
#include <unistd.h>
#include <sys/syscall.h>
#include "kernel.h"

/*
 * Exit a process
 */
void
sys_exit(int status)
{
    struct ktask *t;
    u16 *video;
    int i;
    char *s;
    s = "exit";

    video = (u16 *)0xb8000;
    for ( i = 0; i < 80 * 25; i++ ) {
        //*(video + i) = 0xe000;
        *(video + i) = 0x2000;
    }
    while ( *s ) {
        //*video = 0xe000 | (u16)*s;
        *video = 0x2f00 | (u16)*s;
        s++;
        video++;
    }

    /* Get the task */
    t = this_ktask();
    if ( NULL == t ) {
        return;
    }

    /* Call atexit */
}

/*
 * Create a new process
 *
 * SYNOPSIS
 *      pid_t
 *      sys_fork(void);
 *
 * DESCRIPTION
 *
 * RETURN VALUES
 *      Upon successful completion, the sys_fork() function returns a value of 0
 *      to the child process and returns the process ID of the child process to
 *      the parent process.  Otherwise, a value of -1 is returned to the parent
 *      process and no child process is created.
 */
pid_t
sys_fork(void)
{
    int i;
    struct proc *np;
    struct ktask *t;
    struct ktask *nt;
    pid_t pid;

    /* Get the current process */
    t = this_ktask();
    if ( NULL == t || NULL == t->proc ) {
        return -1;
    }

    /* Search an available process ID */
    pid = -1;
    for ( i = 0; i < PROC_NR; i++ ) {
        if ( NULL == proc_table->procs[(proc_table->lastpid + i) % PROC_NR] ) {
            pid = (proc_table->lastpid + i) % PROC_NR;
            break;
        }
    }
    if ( pid < 0 ) {
        /* Could not find any available process ID */
        return -1;
    }

    /* Create a new process */
    np = kmalloc(sizeof(struct proc));
    if ( NULL == np ) {
        return -1;
    }
    kmemset(np, 0, sizeof(struct proc));
    nt = task_clone(t);
    if ( NULL == nt ) {
        kfree(np);
        return -1;
    }
    nt->proc = np;
    nt->state = KTASK_STATE_CREATED;
    nt->next = NULL;
    nt->credit = 100;

    proc_table->procs[pid] = np;
    proc_table->lastpid = pid;

    t->next = nt;

    sys_fork_restart(nt->arch, 0, pid);

    /* To prevent compiler error */
    return -1;
}

/*
 * Read input
 *
 * SYNOPSIS
 *      ssize_t
 *      sys_read(int fildes, void *buf, size_t nbyte);
 *
 * DESCRIPTION
 *      The sys_read() function attempts to read nbyte bytes of data from the
 *      object referenced by the descriptor fildes into the buffer pointed by
 *      buf.
 * RETURN VALUES
 *      If success, the number of bytes actually read is returned.  Upon reading
 *      end-of-file, zero is returned.  Otherwise, a -1 is returned.
 */
ssize_t
sys_read(int fildes, void *buf, size_t nbyte)
{
    return -1;
}

/*
 * Write output
 *
 * SYNOPSIS
 *      ssize_t
 *      sys_write(int fildes, const void *buf, size_t nbyte);
 *
 * DESCRIPTION
 *      The sys_write() function attempts to write nbyte bytes of data to the
 *      object referenced by the descriptor fildes from the buffer pointed by
 *      buf.
 * RETURN VALUES
 *      Upon successful completion, the number of bytes which were written is
 *      returned.  Otherwise, a -1 is returned.
 */
ssize_t
sys_write(int fildes, const void *buf, size_t nbyte)
{
    return -1;
}

/*
 * Open or create a file for reading or writing
 *
 * SYNOPSIS
 *      int
 *      sys_open(const char *path, int oflags);
 *
 * DESCRIPTION
 *      The sys_open() function attempts to open a file specified by path for
 *      reading and/or writing, as specified by the argument oflag.
 *
 *      The flags specified for the oflag argument are formed by or'ing the
 *      following values (to be implemented):
 *
 *              O_RDONLY        open for reading only
 *              O_WRONLY        open for writing only
 *              O_RDWR          open for reading and writing
 *              O_NONBLOCK      do not block on open or for data to become
 *                              available
 *              O_APPEND        append on each write
 *              O_CREAT         create a file if it does not exist
 *              O_TRUNC         truncate size to 0
 *              O_EXCL          error if O_CREAT and the file exists
 *              O_SHLOCK        atomically obtain a shared lock
 *              O_EXLOCK        atomically obtain an exclusive lock
 *              O_NOFOLLOW      do not follow symlinks
 *              O_SYMLINK       allow open of symlinks
 *              O_EVTONLY       descriptor requested for event notifications
 *                              only
 *              O_CLOEXEC       mark as close-on-exec
 *
 * RETURN VALUES
 *      If success, sys_open() returns a non-negative integer, termed a file
 *      descriptor.  It returns -1 on failure.
 */
int
sys_open(const char *path, int oflag, ...)
{
    return -1;
}

/*
 * Wait for process termination
 *
 * SYNOPSIS
 *      pid_t
 *      sys_wait4(pid_t pid, int *stat_loc, int options, struct rusage *rusage);
 *
 * DESCRIPTION
 *      The sys_wait4() function suspends execution of its calling process until
 *      stat_loc information is available for a terminated child process, or a
 *      signal is received.  On return from a successful from a successful
 *      sys_wait4() call, the stat_loc area contains termination information
 *      about the process that exited as defined below.
 *
 *      The pid parameter specified the set of child processes for which to
 *      wait.  If pid is -1, the call waits for any child process.  If pid is 0,
 *      the call wailts for any child process in the process group of the
 *      caller.  If pid is greater than zero, the call waits for the process
 *      with process ID pid.  If pid is less than -1, the call waits for any
 *      process whose process group ID equals the absolute value of pid.
 *
 *      The stat_loc parameter is defined below.  The options parameter contains
 *      the bitwise OR of any of the following options.  The WNOHANG options is
 *      used to indicate that the call should not block if there are no
 *      processes that wish to report status.  If the WUNTRACED option is set,
 *      children of the current process that are stopped due to a SIGTTIN,
 *      SIGTTOU, SIGTSTPP, or SIGTOP signal also have their status reported.
 *
 *      If rusage is non-zero, a summary of the resources used by the terminated
 *      process and all its children is returned.
 *
 *      When the WNOHANG option is specified and no processes with to report
 *      status, the sys_wait4() function returns a process ID of 0.
 *
 * RETURN VALUES
 *      If the sys_wait4() function returns due to a stopped or terminated child
 *      process, the process ID of the child is returned to the calling process.
 *      If there are no children not previously awaited, a value of -1 is
 *      returned.  Otherwise, if WNOHANG is specified and there are no stopped
 *      or exited children, a value of 0 is returned.  If an error is detected
 *      or a caught signal aborts the call, a value of -1 is returned.
 */
pid_t
sys_wait4(pid_t pid, int *stat_loc, int options, struct rusage *rusage)
{
    /* Change the state to BLOCKED */
    u16 *video;
    int i;
    char *s = "sys_wait4";

    video = (u16 *)0xb8000;
    for ( i = 0; i < 80 * 25; i++ ) {
        *(video + i) = 0xe000;
    }
    while ( *s ) {
        *video = 0xe000 | (u16)*s;
        s++;
        video++;
    }

    return -1;
}

/*
 * Delete a descriptor
 *
 * SYNOPSIS
 *      int
 *      sys_close(int fildes);
 *
 * DESCRIPTION
 *      The sys_close() function deletes a descriptor from the per-process
 *      object reference table.
 *
 * RETURN VALUES
 *      Upon successful completion, a value of 0 is returned.  Otherwise, a
 *      value of -1 is returned.
 */
int
sys_close(int fildes)
{
    return -1;
}

/*
 * Get calling process identification
 *
 * SYNOPSIS
 *      pid_t
 *      sys_getpid(void);
 *
 * DESCRIPTION
 *      The sys_getpid() function returns the process ID of the calling process.
 *
 * RETURN VALUES
 *      The sys_getpid() function is always successful, and no return value is
 *      reserved to indicate an error.
 */
pid_t
sys_getpid(void)
{
    struct ktask *t;

    /* Get the current task information */
    t = this_ktask();
    if ( NULL == t ) {
        /* This error must not occur. */
        return -1;
    }
    if ( NULL == t->proc ) {
        /* This error must not occur. */
        return -1;
    }

    /* Return the process ID */
    return t->proc->id;
}

/*
 * Get parent process identification
 *
 * SYNOPSIS
 *      pid_t
 *      sys_getppid(void);
 *
 * DESCRIPTION
 *      The sys_getppid() function returns the process ID of the parent of the
 *      calling process.
 *
 * RETURN VALUES
 *      The sys_getppid() function is always successful, and no return value is
 *      reserved to indicate an error.
 */
pid_t
sys_getppid(void)
{
    struct ktask *t;

    /* Get the current task information */
    t = this_ktask();
    if ( NULL == t ) {
        /* This error must not occur. */
        return -1;
    }
    if ( NULL == t->proc ) {
        /* This error must not occur. */
        return -1;
    }
    if ( NULL == t->proc->parent ) {
        /* No parent process found */
        return -1;
    }

    return t->proc->parent->id;
}

/*
 * execute a file
 *
 * SYNOPSIS
 *      int
 *      sys_execve(const char *path, char *const argv[], char *const envp[]);
 *
 * DESCRIPTION
 *      The function sys_execve() transforms the calling process into a new
 *      process. The new process is constructed from an ordinary file, whose
 *      name is pointed by path, called the new process file.  In the current
 *      implementation, this file should be an executable object file, whose
 *      text section virtual address starts from 0x40000000.  The design of
 *      relocatable object support is still ongoing.
 *
 * RETURN VALUES
 *      As the function sys_execve() overlays the current process image with a
 *      new process image, the successful call has no process to return to.  If
 *      it does return to the calling process, an error has occurred; the return
 *      value will be -1.
 */
int arch_exec2(void *, void (*)(void), size_t, int);
int
sys_execve(const char *path, char *const argv[], char *const envp[])
{
    u64 *initramfs = (u64 *)0x20000ULL;
    u64 offset = 0;
    u64 size;
    struct ktask *t;

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
        /* Could not find the file */
        return -1;
    }

    t = this_ktask();
    arch_exec2(t->arch, (void *)(0x20000ULL + offset), size,
               KTASK_POLICY_SERVER);

    /* On failure */
    return -1;
}

/*
 * Allocate memory, or map files or devices into memory
 *
 * SYNOPSIS
 *      The sys_mmap() system call causes the pages starting at addr and
 *      continuing for at most len bytes to be mapped from the object described
 *      by fd, starting at byte offset offset.  If offset or len is not a
 *      multiple of the pagesize, the mapped region may extend past the
 *      specified range.  Any extension beyond the end of the mapped object will
 *      be zero-filled.
 *
 *      The addr argument is used by the system to determine the starting
 *      address of the mapping, and its interpretation is dependent on the
 *      setting of the MAP_FIXED flag.  If MAP_FIXED is specified in flags, the
 *      system will try to place the mapping at the specified address, possibly
 *      removing a mapping that already exists at that location.  If MAP_FIXED
 *      is not specified, then the system will attempt to use the range of
 *      addresses starting at addr if they do not overlap any existing mappings,
 *      including memory allocated by malloc(3) and other such allocators.
 *      Otherwise, the system will choose an alternate address for the mapping
 *      (using an implementation dependent algorithm) that does not overlap any
 *      existing mappings.  In other words, without MAP_FIXED the system will
 *      attempt to find an empty location in the address space if the specified
 *      address range has already been mapped by something else.  If addr is
 *      zero and MAP_FIXED is not specified, then an address will be selected by
 *      the system so as not to overlap any existing mappings in the address
 *      space.  In all cases, the actual starting address of the region is
 *      returned.  If MAP_FIXED is specified, a successful mmap deletes any
 *      previous mapping in the allocated address range.  Previous mappings are
 *      never  deleted if MAP_FIXED is not specified.
 *
 * RETURN VALUES
 *      Upon successful completion, mmap() returns a pointer to the mapped
 *      region.  Otherwise, a value of MAP_FAILED is returned.
 */
void *
sys_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    return NULL;
}

/*
 * Remove a mapping
 *
 * SYNOPSIS
 *      The sys_munmap() system call deletes the mappings for the specified
 *      address range, causing further references to addresses within the range
 *      to generate invalid memory references.
 *
 * RETURN VALUES
 *      Upon successful completion, munmap returns zero.  Otherwise, a value of
 *      -1 is returned.
 */
int
sys_munmap(void *addr, size_t len)
{
    return -1;
}


/*
 * Reposition read/write file offset
 *
 * SYNOPSIS
 *      off_t
 *      sys_lseek(int fildes, off_t offset, int whence);
 *
 * DESCRIPTION
 *      The sys_lseek() function repositions the offset of the file descriptor
 *      fildes to the argument offset, according to the directive whence.  The
 *      argument fildes must be an open file descriptor.  The function
 *      sys_lseek() repositions the file pointer fildes as follows:
 *
 *              If whence is SEEK_SET, the offset is set to offset bytes.
 *
 *              If whence is SEEK_CUR, the offset is set to its current location
 *              plus offset bytes.
 *
 *              If whnce is SEEK_END, the offset is set to the size of the file
 *              plust offset bytes.
 *
 *      The sys_lseek() function allows the file offset to be set beyond the end
 *      of the existing end-of-file of the file.  If the data is later written
 *      at this point, subsequence reads of the data in the gap return bytes of
 *      zeros (until data is actually written into the gap).
 *
 *      Some devices are incapable of seeking.  The value of the pointer
 *      associated with such device is undefined.
 *
 * RETURN VALUES
 *      Upon successful completion, the sys_lseek() function returns the
 *      resulting offset location as measured in bytes from the beginning of the
 *      file.  Otherwise, a value of -1 is returned.
 */
off_t
sys_lseek(int fildes, off_t offset, int whence)
{
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
