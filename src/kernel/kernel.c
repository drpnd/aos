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
#include "kernel.h"

struct proc_table *proc_table;
struct ktask_root *ktask_root;

/*
 * Entry point to the kernel in C for all processors, called from asm.s.
 */
void
kmain(void)
{
    for ( ;; ) {
        halt();
    }
}

/*
 * Local APIC timer
 * Low-level scheduler (just loading run queue)
 */
void
isr_loc_tmr(void)
{
    struct ktask *ktask;

    ktask = this_ktask();
    if ( ktask ) {
        /* Decrement the credit */
        ktask->credit--;
        if ( ktask->credit <= 0 ) {
            /* Expires */
            if ( ktask->next ) {
                /* Schedule the next task */
                set_next_ktask(ktask->next);
            } else {
                /* Call high-level scheduler */
                sched_high();
            }
        }
    }
}

/*
 * Interrupt service routine
 */
void
kintr_isr(u64 vec)
{
    switch ( vec ) {
    case IV_LOC_TMR:
        isr_loc_tmr();
        break;
    default:
        ;
    }
}

#if !defined(HAS_KMEMSET) || !HAS_KMEMSET
/*
 * kmemset
 */
void *
kmemset(void *b, int c, size_t len)
{
    size_t i;

    i = 0;
    while ( len > 0 ) {
        ((u8 *)b)[i] = c;
        i++;
        len--;
    }

    return b;
}
#endif

#if !defined(HAS_KMEMCMP) || !HAS_KMEMCMP
/*
 * kmemcmp
 */
int
kmemcmp(const void *s1, const void *s2, size_t n)
{
    size_t i;
    int diff;

    i = 0;
    while ( n > 0 ) {
        diff = (u8)((u8 *)s1)[i] - ((u8 *)s2)[i];
        if ( diff ) {
            return diff;
        }
        i++;
        n--;
    }

    return 0;
}
#endif

#if !defined(HAS_KMEMCPY) || !HAS_KMEMCPY
/*
 * kstrcpy
 */
void *
kmemcpy(void *__restrict dst, const void *__restrict src, size_t n)
{
    size_t i;

    for ( i = 0; i < n; i++ ) {
        *((u8 *)dst + i) = *((u8 *)src + i);
    }

    return dst;
}
#endif

/*
 * kstrlen
 */
size_t
kstrlen(const char *s)
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
 * kstrcmp
 */
int
kstrcmp(const char *s1, const char *s2)
{
    size_t i;
    int diff;

    i = 0;
    while ( s1[i] != '\0' || s2[i] != '\0' ) {
        diff = s1[i] - s2[i];
        if ( diff ) {
            return diff;
        }
        i++;
    }

    return 0;
}

/*
 * kstrcpy
 */
char *
kstrcpy(char *dst, const char *src)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = src[i];

    return dst;
}

/*
 * kstrncpy
 */
char *
kstrncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' || i < n ) {
        dst[i] = src[i];
        i++;
    }
    for ( ; i < n; i++ ) {
        dst[i] = '\0';
    }

    return dst;
}

/*
 * kstrlcpy
 */
size_t
kstrlcpy(char *dst, const char *src, size_t n)
{
    size_t i;

    i = 0;
    while ( src[i] != '\0' || i < n - 1 ) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';

    return i;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
