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
#include "kernel.h"

/*
 * Exit a process
 */
void
sys_exit(int status)
{
    struct ktask *t;

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
 *      Upon successful completion, the fork() function returns a value of 0 to
 *      the child process and returns the process ID of the child process to the
 *      parent process.  Otherwise, a value of -1 is returned to the parent
 *      process and no child process is created.
 */
pid_t
sys_fork(void)
{
    struct ktask *t;

    /* Allocate task data structure */
    t = kmalloc(sizeof(struct ktask));
    if ( NULL == t ) {
        /* Failed to create a new task */
        return -1;
    }
    t->state = KTASK_STATE_CREATED;

    /* Copy the process */

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
 *      The open() function attempts to open a file specified by path for
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
 *      If success, open() returns a non-negative integer, termed a file
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
 *      The wait4() function suspends execution of its calling process until
 *      stat_loc information is available for a terminated child process, or a
 *      signal is received.  On return from a successful from a successful
 *      wait4() call, the stat_loc area contains termination information about
 *      the process that exited as defined below.
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
 *      status, the wait4() function returns a process ID of 0.
 *
 * RETURN VALUES
 *      If the wait4() function returns due to a stopped or terminated child
 *      process, the process ID of the child is returned to the calling process.
 *      If there are no children not previously awaited, a value of -1 is
 *      returned.  Otherwise, if WNOHANG is specified and there are no stopped
 *      or exited children, a value of 0 is returned.  If an error is detected
 *      or a caught signal aborts the call, a value of -1 is returned.
 */
pid_t
sys_wait4(pid_t pid, int *stat_loc, int options, struct rusage *rusage)
{
    return -1;
}

/*
 * Delete a descriptor
 *
 * SYNOPSIS
 *      int
 *      close(int fildes);
 *
 * DESCRIPTION
 *      The close() function deletes a descriptor from the per-process object
 *      reference table.
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
 * execute a file
 *
 * SYNOPSIS
 *      int
 *      sys_execve(const char *path, char *const argv[], char *const envp[]);
 *
 * DESCRIPTION
 *      The function execve() transforms the calling process into a new process.
 *      The new process is constructed from an ordinary file, whose name is
 *      pointed by path, called the new process file.  In the current
 *      implementation, this file should be an executable object file, whose
 *      text section virtual address starts from 0x40000000.  The design of
 *      relocatable object support is still ongoing.
 *
 * RETURN VALUES
 *      As the function execve() overlays the current process image with a new
 *      process image, the successful call has no process to return to.  If it
 *      does return to the calling process, an error has occurred; the return
 *      value will be -1.
 */
int
sys_execve(const char *path, char *const argv[], char *const envp[])
{
    return -1;
}

/*
 * Get calling process identification
 *
 * SYNOPSIS
 *      pid_t
 *      getpid(void);
 *
 * DESCRIPTION
 *      The getpid() function returns the process ID of the calling process.
 *
 * RETURN VALUES
 *      The getpid() function is always successful, and no return value is
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
 *      getppid(void);
 *
 * DESCRIPTION
 *      The getppid() function returns the process ID of the parent of the
 *      calling process.
 *
 * RETURN VALUES
 *      The getppid() function is always successful, and no return value is
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
