/*_
 * Copyright (c) 2015-2016 Hirochika Asai <asai@jar.jp>
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

struct kmem *g_kmem;

/* Prototype declarations of static functions */
static void * _kmalloc_slab(struct kmem *, size_t);
static void * _kmalloc_slab_partial(struct kmem *, size_t);
static void * _kmalloc_slab_free(struct kmem *, size_t);
static void * _kmalloc_slab_new(struct kmem *, size_t);
static void * _kmalloc_pages(struct kmem *, size_t);

/*
 * Allocate memory space.
 * Note that the current implementation does not protect the slab header, so
 * the allocated memory must be carefully used.
 *
 * SYNOPSIS
 *      void *
 *      kmalloc(size_t size);
 *
 * DESCRIPTION
 *      The kmalloc() function allocates size bytes of contiguous memory.
 *
 * RETURN VALUES
 *      The kmalloc() function returns a pointer to allocated memory.  If there
 *      is an error, it returns NULL.
 */
void *
kmalloc(size_t size)
{
    size_t o;

    /* Get the bit-width of the size argument */
    o = bitwidth(size);

    if ( o < KMEM_SLAB_BASE_ORDER + KMEM_SLAB_ORDER ) {
        /* Slab */
        if ( o < KMEM_SLAB_BASE_ORDER ) {
            o = 0;
        } else {
            o = o - KMEM_SLAB_BASE_ORDER;
        }
        return _kmalloc_slab(g_kmem, o);
    } else {
        /* Pages */
        return _kmalloc_pages(g_kmem, size);
    }
}

/*
 * Allocate memory from the slab allocator
 */
static void *
_kmalloc_slab(struct kmem *kmem, size_t o)
{
    void *ptr;

    /* Ensure that the order is less than the maximum configured order */
    if ( o >= KMEM_SLAB_ORDER ) {
        return NULL;
    }

    /* Lock */
    spin_lock(&kmem->slab_lock);

    /* Small object: Slab allocator */
    if ( NULL != kmem->slab.gslabs[o].partial ) {
        /* Partial list is available. */
        ptr = _kmalloc_slab_partial(kmem, o);
    } else if ( NULL != kmem->slab.gslabs[o].free ) {
        /* Partial list is empty, but free list is available. */
        ptr = _kmalloc_slab_free(kmem, o);
    } else {
        /* No free space, then allocate new page for slab objects */
        ptr = _kmalloc_slab_new(kmem, o);
     }

    /* Unlock */
    spin_unlock(&kmem->slab_lock);

    return ptr;
}

/*
 * Allocate memory from partial slab
 */
static void *
_kmalloc_slab_partial(struct kmem *kmem, size_t o)
{
    struct kmem_slab *hdr;
    void *ptr;
    ssize_t i;

    /* Partial list is available. */
    hdr = kmem->slab.gslabs[o].partial;
    ptr = (void *)((reg_t)hdr->obj_head + hdr->free
                   * (1ULL << (o + KMEM_SLAB_BASE_ORDER)));
    hdr->marks[hdr->free] = 1;
    hdr->nused++;
    if ( hdr->nr <= hdr->nused ) {
        /* Becomes full */
        hdr->free = -1;
        kmem->slab.gslabs[o].partial = hdr->next;
        /* Prepend to the full list */
        hdr->next = kmem->slab.gslabs[o].full;
        kmem->slab.gslabs[o].full = hdr;
    } else {
        /* Search free space for the next allocation */
        for ( i = 0; i < hdr->nr; i++ ) {
            if ( 0 == hdr->marks[i] ) {
                hdr->free = i;
                break;
            }
        }
    }

    return ptr;
}

/*
 * Allocate memory from free slab list
 */
static void *
_kmalloc_slab_free(struct kmem *kmem, size_t o)
{
    struct kmem_slab *hdr;
    void *ptr;
    ssize_t i;

    /* Partial list is empty, but free list is available. */
    hdr = kmem->slab.gslabs[o].free;
    ptr = (void *)((reg_t)hdr->obj_head + hdr->free
                   * (1ULL << (o + KMEM_SLAB_BASE_ORDER)));
    hdr->marks[hdr->free] = 1;
    hdr->nused++;
    if ( hdr->nr <= hdr->nused ) {
        /* Becomes full */
        hdr->free = -1;
        kmem->slab.gslabs[o].partial = hdr->next;
        /* Prepend to the full list */
        hdr->next = kmem->slab.gslabs[o].full;
        kmem->slab.gslabs[o].full = hdr;
    } else {
        /* Prepend to the partial list */
        hdr->next = kmem->slab.gslabs[o].partial;
        kmem->slab.gslabs[o].partial = hdr;
        /* Search free space for the next allocation */
        for ( i = 0; i < hdr->nr; i++ ) {
            if ( 0 == hdr->marks[i] ) {
                hdr->free = i;
                break;
            }
        }
    }

    return ptr;
}

/*
 * Allocate memory from a new slab objects
 */
static void *
_kmalloc_slab_new(struct kmem *kmem, size_t o)
{
    struct kmem_slab *hdr;
    void *ptr;
    ssize_t i;
    size_t s;
    size_t nr;

    /* No free space, then allocate new page for slab objects */
    s = (1ULL << (o + KMEM_SLAB_BASE_ORDER + KMEM_SLAB_NR_OBJ_ORDER))
        + sizeof(struct kmem_slab);
    /* Align the page to fit to the buddy system, and get the order */
    nr = DIV_CEIL(s, PAGESIZE);
    /* Allocate pages */
    hdr = kmem_alloc_pages(kmem, nr);
    if ( NULL == hdr ) {
        return NULL;
    }
    /* Calculate the number of slab objects in this block; N.B., + 1 in the
       denominator is the `marks' for each objects. */
    hdr->nr = (nr * PAGESIZE - sizeof(struct kmem_slab))
        / ((1ULL << (o + KMEM_SLAB_BASE_ORDER)) + 1);
    /* Reset counters */
    hdr->nused = 0;
    hdr->free = 0;
    /* Set the address of the first slab object */
    hdr->obj_head = (void *)((u64)hdr + (nr * PAGESIZE)
                             - ((1ULL << (o + KMEM_SLAB_BASE_ORDER))
                                * hdr->nr));
    /* Reset marks and next cache */
    kmemset(hdr->marks, 0, hdr->nr);
    hdr->next = NULL;

    /* Retrieve a slab */
    ptr = (void *)((u64)hdr->obj_head + hdr->free
                   * (1ULL << (o + KMEM_SLAB_BASE_ORDER)));
    hdr->marks[hdr->free] = 1;
    hdr->nused++;

    if ( hdr->nr <= hdr->nused ) {
        /* Becomes full */
        hdr->free = -1;
        kmem->slab.gslabs[o].partial = hdr->next;
        /* Prepend to the full list */
        hdr->next = kmem->slab.gslabs[o].full;
        kmem->slab.gslabs[o].full = hdr;
    } else {
        /* Prepend to the partial list */
        hdr->next = kmem->slab.gslabs[o].partial;
        kmem->slab.gslabs[o].partial = hdr;
        /* Search free space for the next allocation */
        for ( i = 0; i < hdr->nr; i++ ) {
            if ( 0 == hdr->marks[i] ) {
                hdr->free = i;
                break;
            }
        }
    }

    return ptr;
}

/*
 * Allocate memory from the slab allocator
 */
static void *
_kmalloc_pages(struct kmem *kmem, size_t size)
{
    void *ptr;

    /* Lock */
    spin_lock(&kmem->slab_lock);

    /* Large object: Page allocator */
    ptr = kmem_alloc_pages(kmem, DIV_CEIL(size, PAGESIZE));

    /* Unlock */
    spin_unlock(&kmem->slab_lock);

    return ptr;
}

/*
 * Deallocate memory space pointed by ptr
 *
 * SYNOPSIS
 *      void
 *      kfree(void *ptr);
 *
 * DESCRIPTION
 *      The kfree() function deallocates the memory allocation pointed by ptr.
 *
 * RETURN VALUES
 *      The kfree() function does not return a value.
 */
void
kfree(void *ptr)
{
    int i;
    int j;
    int found;
    u64 asz;
    struct kmem_slab *hdr;
    struct kmem_slab **hdrp;

    /* Lock */
    spin_lock(&g_kmem->slab_lock);

    if ( 0 == (u64)ptr % SUPERPAGESIZE ) {
        /* Free pages */
        //kmem_free_pages(ptr);
    } else {
        /* Search for each order */
        for ( i = 0; i < KMEM_SLAB_BASE_ORDER; i++ ) {
            asz = (1 << (i + KMEM_SLAB_BASE_ORDER));

            /* Search from partial */
            hdrp = &g_kmem->slab.gslabs[i].partial;

            /* Continue until the corresponding object found */
            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].free;
                        g_kmem->slab.gslabs[i].free = hdr;
                    }
                    spin_unlock(&g_kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }

            /* Search from full */
            hdrp = &g_kmem->slab.gslabs[i].full;

            /* Continue until the corresponding object found */
            while ( NULL != *hdrp ) {
                hdr = *hdrp;

                found = -1;
                for ( j = 0; j < hdr->nr; j++ ) {
                    if ( ptr == (void *)((u64)hdr->obj_head + j * asz) ) {
                        /* Found */
                        found = j;
                        break;
                    }
                }
                if ( found >= 0 ) {
                    hdr->nused--;
                    hdr->marks[found] = 0;
                    hdr->free = found;
                    if ( hdr->nused <= 0 ) {
                        /* To free list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].free;
                        g_kmem->slab.gslabs[i].free = hdr;
                    } else {
                        /* To partial list */
                        *hdrp = hdr->next;
                        hdr->next = g_kmem->slab.gslabs[i].partial;
                        g_kmem->slab.gslabs[i].partial = hdr;
                    }
                    spin_unlock(&g_kmem->slab_lock);
                    return;
                }
                hdrp = &hdr->next;
            }
        }
    }

    /* Unlock */
    spin_unlock(&g_kmem->slab_lock);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
