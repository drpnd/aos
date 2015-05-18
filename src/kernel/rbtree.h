/*_
 * Copyright (c) 2010,2015 Hirochika Asai <asai@jar.jp>
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

#ifndef _KERNEL_RBTREE_H
#define _KERNEL_RBTREE_H

#include <aos/const.h>
#include <aos/types.h>

enum rbtree_color {
    RBTREE_BLACK,
    RBTREE_RED,
};

struct rbtree_node {
    void *key;
    struct rbtree_node *parent;
    struct rbtree_node *left;
    struct rbtree_node *right;
    enum rbtree_color color;
};
struct rbtree {
    struct rbtree_node *root;
    int (*compare)(const void *, const void *);
    /* Need to free on release? */
    int _need_to_free:1;
};
struct rbtree_iterator {
    struct rbtree_node *cur;
    struct rbtree_node *prev;
    /* Need to free on release? */
    int _need_to_free:1;
};

struct rbtree *
rbtree_init(struct rbtree *, int (*)(const void *, const void *));
void rbtree_release(struct rbtree *);
void rbtree_release_callback(struct rbtree *, void (*)(void *, void *), void *);
void * rbtree_search(struct rbtree *, void *);
int rbtree_insert(struct rbtree *, void *);
void * rbtree_delete(struct rbtree *, void *);
void * rbtree_pop(struct rbtree *);
void * rbtree_min(struct rbtree *);
void rbtree_exec_all(struct rbtree *, void (*)(void *, void *), void *);

struct rbtree_iterator * rbtree_iterator_init(struct rbtree_iterator *);
void rbtree_iterator_release(struct rbtree_iterator *);
void * rbtree_iterator_cur(struct rbtree_iterator *);
void * rbtree_iterator_next(struct rbtree *, struct rbtree_iterator *);
void rbtree_iterator_rewind(struct rbtree_iterator *);

#endif /* _KERNEL_RBTREE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
