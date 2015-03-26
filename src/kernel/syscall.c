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
}

/*
 * Create a new process
 */
pid_t
sys_fork(void)
{
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
