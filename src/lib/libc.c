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

#if __LP64__

#define NULL    ((void *)0)
typedef signed long long ssize_t;
typedef unsigned long long size_t;
typedef signed int pid_t;

#else
#error "Must be LP64"
#endif

#include <sys/syscall.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))

unsigned long long syscall(int, ...);

void exit(int) __attribute__ ((__noreturn__));
pid_t fork(void);
pid_t waitpid(pid_t, int *, int);
pid_t getpid(void);
pid_t getppid(void);

/*
 * exit
 */
void
exit(int status)
{
    syscall(SYS_exit, status);

    /* Infinite loop to prevent the warning: 'noreturn' function does return */
    while ( 1 ) {}
}

/*
 * fork
 */
pid_t
fork(void)
{
    return syscall(SYS_fork);
}

/*
 * read
 */
ssize_t
read(int fildes, void *buf, size_t nbyte)
{
    return syscall(SYS_read, fildes, buf, nbyte);
}

/*
 * write
 */
ssize_t
write(int fildes, const void *buf, size_t nbyte)
{
    return syscall(SYS_write, fildes, buf, nbyte);
}

/*
 * open
 */
int
open(const char *path, int oflag, ...)
{
    va_list ap;

    va_start(ap, oflag);
    syscall(SYS_open, path, oflag, ap);
    va_end(ap);

    return -1;
}

/*
 * close
 */
int
close(int fildes)
{
    return syscall(SYS_close, fildes);
}

/*
 * waitpid
 */
pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
    return syscall(SYS_wait4, pid, stat_loc, options, NULL);
}

/*
 * getpid
 */
pid_t
getpid(void)
{
    return syscall(SYS_getpid);
}

/*
 * getppid
 */
pid_t
getppid(void)
{
    return syscall(SYS_getppid);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
