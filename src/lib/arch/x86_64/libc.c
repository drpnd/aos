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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))
#define alloca(size)            __builtin_alloca((size))

/* in libcasm.s */
unsigned long long syscall(int, ...);

#if !defined(TEST) || !TEST
int main(int argc, char *argv[]);

/*
 * Entry point to a process
 */
void
entry(int argc, char *argv[])
{
    int ret;

    ret = main(argc, argv);

    while ( 1 ) {}
}
#endif

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
 * execve
 */
int
execve(const char *path, char *const argv[], char *const envp[])
{
    return syscall(SYS_execve, path, argv, envp);
}

/*
 * mmap
 */
void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    return (void * )syscall(SYS_mmap, addr, len, prot, flags, fd, offset);
}

/*
 * munmap
 */
int
munmap(void *addr, size_t len)
{
    return syscall(SYS_munmap, addr, len);
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
 * lseek
 */
off_t
lseek(int fildes, off_t offset, int whence)
{
    return syscall(SYS_lseek, fildes, offset, whence);
}

/*
 * malloc
 */
void *
malloc(size_t size)
{
    void *ptr;

    ptr = NULL;
    //ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);

    return ptr;
}

/*
 * free
 */
void
free(void *ptr)
{
    //munmap(ptr, len);
}




#define PRINTF_MOD_NONE         0
#define PRINTF_MOD_LONG         1
#define PRINTF_MOD_LONGLONG     2

static const char *
_parse_printf_format(const char *fmt, int *zero, int *pad, int *prec, int *mod)
{
    /* Reset */
    *zero = 0;
    *pad = 0;
    *prec = 0;
    *mod = 0;

    /* Padding with zero? */
    if ( '0' == *fmt ) {
        *zero = 1;
        fmt++;
    }

    /* Padding length */
    if ( *fmt >= '1' && *fmt <= '9' ) {
        *pad += *fmt - '0';
        fmt++;
        while ( *fmt >= '0' && *fmt <= '9' ) {
            *pad *= 10;
            *pad += *fmt - '0';
            fmt++;
        }
    }

    /* Precision */
    if ( '.' == *fmt ) {
        fmt++;
        while ( *fmt >= '0' && *fmt <= '9' ) {
            *prec *= 10;
            *prec += *fmt - '0';
            fmt++;
        }
    }

    /* Modifier */
    if ( 'l' == *fmt ) {
        fmt++;
        if ( 'l' == *fmt ) {
            *mod = PRINTF_MOD_LONGLONG;
            fmt++;
        } else {
            *mod = PRINTF_MOD_LONG;
        }
    }

    return fmt;
}


/*
 * Put a % character with paddings to the standard output of the kernel
 */
static int
_printf_percent(char * __restrict__ str, size_t size, int pad)
{
    if ( size > 0 && pad > size - 1 ) {
        pad = size - 1;
    }
    if ( pad > 0 ) {
        memset(str, ' ', pad);
    }
    str[pad] = '%';

    return pad + 1;
}

/*
 * Print out a string
 */
int
_printf_string(char * __restrict__ str, size_t size,
               const char * __restrict__ s)
{
    if ( NULL == s ) {
        s = "(null)";
    }

    strncpy(str, s, size - 1);
    str[size - 1] = '\0';

    return strlen(str);
}

/*
 * Format
 */
static int
_printf_format(char *__restrict__ str, size_t size,
               const char *__restrict__ *format, va_list ap)
{
    const char *fmt;
    /* Leading suffix */
    int zero;
    /* Minimum length */
    int pad;
    /* Precision */
    int prec;
    /* Modifier */
    int mod;

    /* Values */
    const char *s;

    int ret;

    fmt = *format + 1;
    fmt = _parse_printf_format(fmt, &zero, &pad, &prec, &mod);

    /* Conversion */
    if ( '%' == *fmt ) {
        ret = _printf_percent(str, size, pad);
        *format = fmt + 1;
        return ret;
    } else if ( 's' == *fmt ) {
        /* String */
        s = va_arg(ap, char *);
        ret = _printf_string(str, size, s);
        *format = fmt + 1;
        return ret;
    }

    return 0;
}

/*
 * vsnprintf
 */
int
vsnprintf(char *__restrict__ str, size_t size,
          const char *__restrict__ format, va_list ap)
{
    /* Written length */
    int wr;
    int ret;

    /* Read the format */
    wr = 0;
    while ( '\0' != *format ) {
        if ( '%' == *format ) {
            /* % character */
            ret = _printf_format(&str[wr], size - wr, &format, ap);
            wr += ret;
        } else {
            /* An ordinary character */
            str[wr] = *format;
            format++;
            wr++;
        }
        if ( wr >= size - 1 ) {
            str[wr] = '\0';
            return wr;
        }
    }

    str[wr] = '\0';
    return wr;
}

/*
 * snprintf
 */
int
snprintf(char *__restrict__ str, size_t size, const char *__restrict__ format,
         ...)
{
    int ret;
    va_list ap;

    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);

    return ret;
}

/*
 * Find length of string
 *
 * SYNOPSIS
 *      size_t
 *      strlen(const char *s);
 *
 * DESCRIPTION
 *      The strlen() function computes the length of the string s.
 *
 * RETURN VALUES
 *      The strlen() function returns the number of characters that precede the
 *      terminating NULL character.
 */
size_t
strlen(const char *s)
{
    size_t len;

    len = 0;
    while ( '\0' != *s ) {
        len++;
        s++;
    }

    return len;
}

/*
 * Copy strings
 *
 * SYNOPSIS
 *      char *
 *      strcpy(char *dst, const char *src);
 *
 * DESCRIPTION
 *      The strcpy() function copies the string src to dst (including the
 *      terminating '\0' character).
 *
 * RETURN VALUES
 *      The strcpy() function returns dst.
 *
 */
char *
strcpy(char *dst, const char *src)
{
    size_t i;

    i = 0;
    while ( '\0' != src[i] ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = src[i];

    return dst;
}

/*
 * Copy strings
 *
 * SYNOPSIS
 *      char *
 *      strncpy(char *restrict dst, const char *restrict src, size_t n);
 *
 * DESCRIPTION
 *      The strncpy() function copies at most n characters from src to dst.  If
 *      src is less than n characters long, the remainder of dst is filled with
 *      `\0' characters.  Otherwise, dst is not terminated.
 *
 * RETURN VALUES
 *      The strncpy() function returns dst.
 *
 */
char *
strncpy(char *__restrict__ dst, const char *__restrict__ src, size_t n)
{
    size_t i;

    i = 0;
    while ( '\0' != src[i] && i < n ) {
        dst[i] = src[i];
        i++;
    }
    for ( ; i < n; i++ ) {
        dst[i] = '\0';
    }

    return dst;
}

/*
 * Compare strings
 *
 * SYNOPSIS
 *      int
 *      strcmp(const char *s1, const char *s2);
 *
 * DESCRIPTION
 *      The strcmp() function lexicographically compare the null-terminated
 *      strings s1 and s2.
 *
 * RETURN VALUES
 *      The strcmp() function returns an integer greater than, equal to, or less
 *      than 0, according as the string s1 is greater than, equal to, or less
 *      than the string s2.  The comparison is done using unsigned characters,
 *      so that '\200' is greater than '\0'.
 *
 */
int
strcmp(const char *s1, const char *s2)
{
    size_t i;
    int diff;

    i = 0;
    while ( s1[i] != '\0' || s2[i] != '\0' ) {
        diff = (int)s1[i] - (int)s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * Write zeros to a byte string
 *
 * SYNOPSIS
 *      void
 *      bzero(void *s, size_t n);
 *
 * DESCRIPTION
 *      The bzero() function writes n zeroed bytes to the string s.  If n is
 *      zero, bzero() does nothing.
 *
 */
void
bzero(void *s, size_t n)
{
    memset(s, 0, n);
}

/*
 * Convert ASCII string to integer
 *
 * SYNOPSIS
 *      int
 *      atoi(const char *str);
 *
 * DESCRIPTION
 *      The atoi() function converts the initial portion of the string pointed
 *      to by str to int representation.
 */
int
atoi(const char *str)
{
    int ret;

    ret = 0;
    while  ( *str >= '0' && *str <= '9' ) {
        ret *= 10;
        ret += *str - '0';
        str++;
    }

    return ret;
}

/*
 * Architecture-specific system call
 *
 * SYNOPSIS
 *      int
 *      sysarch(int number, void *args);
 *
 * DESCRIPTION
 *      The sysarch() function calls the architecture-specific system call.
 */
int
sysarch(int number, void *args)
{
    return syscall(SYS_sysarch, number, args);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
