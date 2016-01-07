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

/* Prototype declarations */
void aos_stdc_bzero(void *, size_t);
size_t aos_stdc_strlen(const char *);
int aos_stdc_atoi(const char *);

/*
 * Test bzero()
 */
int
test_bzero(void)
{
    char zero[256];
    char buf0[256];
    char buf1[256];
    char buf2[256];

    /* Test */
    bzero(zero, 256);
    aos_stdc_bzero(buf0, 10);
    aos_stdc_bzero(buf1, 128);
    aos_stdc_bzero(buf2, 256);
    if ( 0 != memcmp(zero, buf0, 10) ) {
        return -1;
    }
    if ( 0 != memcmp(zero, buf1, 128) ) {
        return -1;
    }
    if ( 0 != memcmp(zero, buf2, 256) ) {
        return -1;
    }

    return 0;
}


/*
 * Test strlen
 */
int
test_strlen(void)
{
    const char *str[] = {"test", "", "testing"};
    size_t i;

    /* Test */
    for ( i = 0; i < sizeof(str) / sizeof(char *); i++ ) {
        if ( strlen(str[i]) != aos_stdc_strlen(str[i]) ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Test atoi
 */
int
test_atoi(void)
{
    const char *str[] = {"10", "0d", "31.a"};
    size_t i;

    /* Test */
    for ( i = 0; i < sizeof(str) / sizeof(char *); i++ ) {
        if ( atoi(str[i]) != aos_stdc_atoi(str[i]) ) {
            return -1;
        }
    }

    return 0;
}

/* Macro for testing */
#define TEST_FUNC(str, func, ret)               \
    do {                                        \
        printf("%s: ", str);                    \
        if ( 0 == func() ) {                    \
            printf("passed");                   \
        } else {                                \
            printf("failed");                   \
            ret = -1;                           \
        }                                       \
        printf("\n");                           \
    } while ( 0 )

/*
 * Main routine
 */
int
main(int argc, const char *const argv[])
{
    int ret;

    ret = 0;
    TEST_FUNC("strlen", test_strlen, ret);
    TEST_FUNC("atoi", test_atoi, ret);
    TEST_FUNC("bzero", test_bzero, ret);

    return ret;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
