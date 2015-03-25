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
kmemcmp(void *s1, void *s2, size_t n)
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
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
