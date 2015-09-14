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

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_wait4       7
//#define SYS_link      9
//#define SYS_unlink    10
//#define SYS_chdir     12
//#define SYS_mknod     14
//#define SYS_chmod     15
//#define SYS_chown     16
#define SYS_getpid      20
//#define SYS_mount     21
//#define SYS_unmount   22
//#define SYS_setuid    23
#define SYS_getuid      24
//#define SYS_access    33
//#define SYS_sync      36
#define SYS_kill      37
#define SYS_getppid     39
//#define SYS_dup       41
//#define SYS_pipe      42
#define SYS_getgid      47
//#define SYS_ioctl     54
#define SYS_execve      59
//#define SYS_umask     60
//#define SYS_chroot    61
#define SYS_munmap      73
//#define SYS_pgrp      81
//#define SYS_setitimer 83
//#define SYS_fcntl     92
//#define SYS_rename    128
//#define SYS_mkdir     136
//#define SYS_rmdir     137
//#define SYS_setsid    147
//#define SYS_setgid    181
//#define SYS_stat      188
//#define SYS_fstat     189
//#define SYS_sigprocmask 340
//#define SYS_sigsuspend 341
//#define SYS_sigpending 343
//#define SYS_sigaction 416
//#define SYS_sigreturn 417
#define SYS_mmap        477
#define SYS_lseek       478
#define SYS_sysarch     1023
#define SYS_MAXSYSCALL  1024

#endif /* _SYS_SYSCALL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
