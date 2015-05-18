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

#include <aos/const.h>
#include "rbtree.h"
#include "kernel.h"

/*
 * Prototype declaration
 */
static struct rbtree_node * _node_new(void);
static void _node_delete(struct rbtree_node *);
static void
_node_recursive_delete(struct rbtree_node *, void (*)(void *, void *), void *);
static void *
_search(struct rbtree_node *, void *, int (*)(const void *, const void *));
static int
_insert(struct rbtree_node **, void *, int (*)(const void *, const void *));
static int _insert_case1(struct rbtree_node *);
static int _insert_case2(struct rbtree_node *);
static int _insert_case3(struct rbtree_node *);
static int _insert_case4(struct rbtree_node *);
static int _insert_case5(struct rbtree_node *);
static void *
_delete(struct rbtree_node **, void *, int (*)(const void *, const void *));
static void * _pop(struct rbtree_node **);
static int _delete_one_child(struct rbtree_node **);
static void _delete_case1(struct rbtree_node *);
static void _delete_case2(struct rbtree_node *);
static void _delete_case3(struct rbtree_node *);
static void _delete_case4(struct rbtree_node *);
static void _delete_case5(struct rbtree_node *);
static void _delete_case6(struct rbtree_node *);
static void _exec(struct rbtree_node *, void (*)(void *, void *), void *);
static void _rotate_left(struct rbtree_node *);
static void _rotate_right(struct rbtree_node *);
static struct rbtree_node * _grandparent(struct rbtree_node *);
static struct rbtree_node * _uncle(struct rbtree_node *);
static struct rbtree_node * _sibling(struct rbtree_node *);
static int _is_leaf(struct rbtree_node *);

/*
 * Allocate new node
 */
static struct rbtree_node *
_node_new(void)
{
    struct rbtree_node *node;

    node = kmalloc(sizeof(struct rbtree_node));
    if ( NULL == node ) {
        return NULL;
    }

    /* Initialize */
    node->key = NULL;
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;

    return node;
}

/*
 * Delete node
 */
static void
_node_delete(struct rbtree_node *node)
{
    kfree(node);
}

/*
 * Delete node recursively
 */
static void
_node_recursive_delete(
    struct rbtree_node *node, void (*func)(void *, void *), void *userdata)
{
    if ( NULL != node ) {
        _node_recursive_delete(node->left, func, userdata);
        _node_recursive_delete(node->right, func, userdata);
        if ( NULL != func ) {
            func(node->key, userdata);
        }
        _node_delete(node);
    }
}

/*
 * Search
 */
static void *
_search(
    struct rbtree_node *node, void *key,
    int (*compare)(const void *, const void *))
{
    int ret;

    if ( NULL == node ) {
        /* Not found */
        return NULL;
    } else if ( NULL == node->key ) {
        /* Leaf with no information */
        return NULL;
    }

    ret = compare(key, node->key);
    if ( 0 == ret ) {
        /* Found */
        return node->key;
    } else if ( ret < 0 ) {
        /* Search the left branch */
        return _search(node->left, key, compare);
    } else {
        /* Search the right branch */
        return _search(node->right, key, compare);
    }

    /* Not to be reached here */
    return NULL;
}

/*
 * Insert
 */
static int
_insert(
    struct rbtree_node **node, void *key,
    int (*compare)(const void *, const void *))
{
    int ret;

    /* For the root */
    if ( NULL == *node ) {
        *node = _node_new();
        if ( NULL == *node ) {
            return -1;
        }
        (*node)->color = RBTREE_BLACK;
    }

    /* Check whether it is a leaf */
    if ( NULL == (*node)->key ) {
        /* Not found, then insert */
        (*node)->key = key;
        (*node)->color = RBTREE_RED;
        /* Add leaves */
        (*node)->left = _node_new();
        if ( NULL == (*node)->left ) {
            return -1;
        }
        (*node)->left->color = RBTREE_BLACK;
        (*node)->left->parent = *node;
        (*node)->right = _node_new();
        if ( NULL == (*node)->right ) {
            _node_delete((*node)->left);
            return -1;
        }
        (*node)->right->color = RBTREE_BLACK;
        (*node)->right->parent = *node;

        return _insert_case1(*node);
    }

    /* Search children */
    ret = compare(key, (*node)->key);
    if ( 0 == ret ) {
        /* Found */
        return 0;
    } else if ( ret < 0 ) {
        /* Search the left branch */
        return _insert(&(*node)->left, key, compare);
    } else {
        /* Search the right branch */
        return _insert(&(*node)->right, key, compare);
    }

    /* Not to be reached here */
    return 0;
}

/*
 * Insert: Case 1
 */
static int
_insert_case1(struct rbtree_node *node)
{
    if ( NULL == node->parent ) {
        /* Root */
        node->color = RBTREE_BLACK;
        return 0;
    } else {
        return _insert_case2(node);
    }

    return 0;
}

/*
 * Insert: Case 2
 */
static int
_insert_case2(struct rbtree_node *node)
{
    if ( RBTREE_BLACK == node->parent->color ) {
        /* Valid */
        return 0;
    } else {
        return _insert_case3(node);
    }
}

/*
 * Insert: Case 3
 */
static int
_insert_case3(struct rbtree_node *node)
{
    struct rbtree_node *uncle;
    struct rbtree_node *grandparent;

    uncle = _uncle(node);
    if ( NULL != uncle && RBTREE_RED == uncle->color ) {
        node->parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        grandparent = _grandparent(node);
        grandparent->color = RBTREE_RED;
        return _insert_case1(grandparent);
    } else {
        return _insert_case4(node);
    }
}

/*
 * Insert: Case 4
 */
static int
_insert_case4(struct rbtree_node *node)
{
    struct rbtree_node *grandparent;

    grandparent = _grandparent(node);
    if ( node == node->parent->right && node->parent == grandparent->left ) {
        _rotate_left(node->parent);
        node = node->left;
    } else if ( node == node->parent->left
                && node->parent == grandparent->right ) {
        _rotate_right(node->parent);
        node = node->right;
    }
    _insert_case5(node);

    return 0;
}

/*
 * Insert: Case 5
 */
static int
_insert_case5(struct rbtree_node *node)
{
    struct rbtree_node *grandparent;

    grandparent = _grandparent(node);
    node->parent->color = RBTREE_BLACK;
    grandparent->color = RBTREE_RED;

    if ( node == node->parent->left && node->parent == grandparent->left ) {
        _rotate_right(grandparent);
    } else {
        /* node == node->parent->right && node->parent == grandparent->right */
        _rotate_left(grandparent);
    }

    return 0;
}

/*
 * Delete
 */
static void *
_delete(
    struct rbtree_node **node, void *key,
    int (*compare)(const void *, const void *))
{
    int ret;
    struct rbtree_node **min_node;
    void *deleted_key;

    /* For the root */
    if ( NULL == *node ) {
        /* Not found */
        return NULL;
    }

    /* Check whether it is a leaf */
    if ( NULL == (*node)->key ) {
        /* Not found */
        return NULL;
    }

    /* Search children */
    ret = compare(key, (*node)->key);
    if ( 0 == ret ) {
        /* Found */
        if ( _is_leaf((*node)->left) || _is_leaf((*node)->right) ) {
            /* Get the deleted key */
            deleted_key = (*node)->key;
            /* If at most one non-leaf child */
            _delete_one_child(node);
        } else {
            /* Both children are not leaves */
            /* Search minimum node from the right subtree */
            min_node = &(*node)->right;
            while ( !_is_leaf((*min_node)->left) ){
                min_node = &(*min_node)->left;
            }
            /* Copy the value */
            deleted_key = (*node)->key;
            (*node)->key = (*min_node)->key;

            _delete_one_child(min_node);
        }

        return deleted_key;
    } else if ( ret < 0 ) {
        /* Search the left branch */
        return _delete(&(*node)->left, key, compare);
    } else {
        /* Search the right branch */
        return _delete(&(*node)->right, key, compare);
    }

    /* Not to be reached here */
    return NULL;
}

/*
 * Pop
 */
static void *
_pop(struct rbtree_node **node)
{
    void *deleted_key;

    /* For the root */
    if ( NULL == *node ) {
        /* Not found */
        return NULL;
    }

    /* Check whether it is a leaf */
    if ( NULL == (*node)->key ) {
        /* Not found */
        return NULL;
    }

    /* Search children */
    if ( _is_leaf((*node)->left) ) {
        /* Found */
        deleted_key = (*node)->key;
        /* If at most one non-leaf child */
        _delete_one_child(node);

        return deleted_key;
    } else {
        /* Search left branch */
        return _pop(&(*node)->left);
    }
}

/*
 * Min
 */
static void *
_min(struct rbtree_node *node)
{
    /* For the root */
    if ( NULL == node ) {
        /* Not found */
        return NULL;
    }

    /* Check whether it is a leaf */
    if ( NULL == node->key ) {
        /* Not found */
        return NULL;
    }

    /* Search children */
    if ( _is_leaf(node->left) ) {
        /* Found */
        return node->key;
    } else {
        /* Search left branch */
        return _min(node->left);
    }
}

/*
 * Delete one chile node
 */
static int
_delete_one_child(struct rbtree_node **ptr)
{
    struct rbtree_node *child;
    struct rbtree_node *leaf;
    struct rbtree_node *parent;
    struct rbtree_node *node;

    node = *ptr;

    /* Note that node has at most one non-leaf child */

    /* Get non-leaf child (might be a leaf) */
    if ( _is_leaf(node->right) ) {
        child = node->left;
        leaf = node->right;
    } else {
        child = node->right;
        leaf = node->left;
    }

    /* Substitute child into node */
    parent = node->parent;
    if ( NULL != parent ) {
        if ( parent->left == node ) {
            parent->left = child;
        } else {
            parent->right = child;
        }
        child->parent = parent;
    } else {
        child->parent = NULL;
    }

    if ( RBTREE_BLACK == node->color ) {
        if ( RBTREE_RED == child->color ) {
            child->color = RBTREE_BLACK;
        } else {
            _delete_case1(child);
        }
    }

    /* Post process */
    /* Delete one of leaves */
    _node_delete(leaf);
    /* Free deleted node */
    kfree(node);
    /* Root replacement */
    if ( NULL == child->parent ) {
        *ptr = child;
    }

    return 0;
}

/*
 * Delete: Case 1
 */
static void
_delete_case1(struct rbtree_node *node)
{
    if ( NULL != node->parent ) {
        _delete_case2(node);
    }
}

/*
 * Delete: Case 2
 */
static void
_delete_case2(struct rbtree_node *node)
{
    struct rbtree_node *sibling;

    sibling = _sibling(node);

    if ( RBTREE_RED == sibling->color ) {
        node->parent->color = RBTREE_RED;
        sibling->color = RBTREE_BLACK;
        if ( node == node->parent->left ) {
            _rotate_left(node->parent);
        } else {
            _rotate_right(node->parent);
        }
    }
    _delete_case3(node);
}

/*
 * Delete: Case 3
 */
static void
_delete_case3(struct rbtree_node *node)
{
    struct rbtree_node *sibling;

    sibling = _sibling(node);

    if ( RBTREE_BLACK == node->parent->color
         && RBTREE_BLACK == sibling->color
         && RBTREE_BLACK == sibling->left->color
         && RBTREE_BLACK == sibling->right->color ) {
        sibling->color = RBTREE_RED;
        _delete_case1(node->parent);
    } else {
        _delete_case4(node);
    }
}

/*
 * Delete: Case 4
 */
static void
_delete_case4(struct rbtree_node *node)
{
    struct rbtree_node *sibling;

    sibling = _sibling(node);

    if ( RBTREE_RED == node->parent->color
         && RBTREE_BLACK == sibling->color
         && RBTREE_BLACK == sibling->left->color
         && RBTREE_BLACK == sibling->right->color ) {
        sibling->color = RBTREE_RED;
        node->parent->color = RBTREE_BLACK;
    } else {
        _delete_case5(node);
    }
}

/*
 * Delete: Case 5
 */
static void
_delete_case5(struct rbtree_node *node)
{
    struct rbtree_node *sibling;

    sibling = _sibling(node);

    if ( RBTREE_BLACK == sibling->color ) {
        /* This if statement is trivial, due to Case 2 (even though Case two
           changed the sibling to a sibling's child, the sibling's child can't
           be red, since no red parent can have a red child). */
        /* The following statements just force the red to be on the left of the
           left left of the parent, or right of the right, so case six will
           rotate correctly. */
        if ( node == node->parent->left
             && RBTREE_BLACK == sibling->right->color
             && RBTREE_RED == sibling->left->color ) {
            /* This last test is trivial too due to cases 2-4. */
            sibling->color = RBTREE_RED;
            sibling->left->color = RBTREE_BLACK;
            _rotate_right(sibling);
        } else if ( node == node->parent->right
                    && RBTREE_BLACK == sibling->left->color
                    && RBTREE_RED == sibling->right->color ) {
            /* This last test is trivial too due to cases 2-4. */
            sibling->color = RBTREE_RED;
            sibling->right->color = RBTREE_BLACK;
            _rotate_left(sibling);
        }
    }
    _delete_case6(node);
}

/*
 * Delete: Case 6
 */
static void
_delete_case6(struct rbtree_node *node)
{
    struct rbtree_node *sibling;

    sibling = _sibling(node);

    sibling->color = node->parent->color;
    node->parent->color = RBTREE_BLACK;

    if ( node == node->parent->left ) {
        sibling->right->color = RBTREE_BLACK;
        _rotate_left(node->parent);
    } else {
        sibling->left->color = RBTREE_BLACK;
        _rotate_right(node->parent);
    }
}

/*
 * Execute
 */
static void
_exec(struct rbtree_node *node, void (*func)(void *, void *), void *userdata)
{
    if ( NULL == node ) {
        /* Not found */
        return;
    } else if ( NULL == node->key ) {
        /* Leaf with no information */
        return;
    }

    _exec(node->left, func, userdata);
    func(node->key, userdata);
    _exec(node->right, func, userdata);
}

/*
 * Left rotation
 */
static void
_rotate_left(struct rbtree_node *p)
{
    struct rbtree_node *q;
    struct rbtree_node *qc;

    q = p->right;
    qc = q->left;

    if ( NULL != p->parent ) {
        if ( p == p->parent->left ) {
            p->parent->left = q;
            q->parent = p->parent;
        } else {
            p->parent->right = q;
            q->parent = p->parent;
        }
    } else {
        q->parent = p->parent;
    }

    q->left = p;
    p->parent = q;

    p->right = qc;
    qc->parent = p;
}

/*
 * Right rotation
 */
static void
_rotate_right(struct rbtree_node *p)
{
    struct rbtree_node *q;
    struct rbtree_node *qc;

    q = p->left;
    qc = q->right;

    if ( NULL != p->parent ) {
        if ( p == p->parent->left ) {
            p->parent->left = q;
            q->parent = p->parent;
        } else {
            p->parent->right = q;
            q->parent = p->parent;
        }
    } else {
        q->parent = p->parent;
    }

    q->right = p;
    p->parent = q;

    p->left = qc;
    qc->parent = p;
}

/*
 * Get grandparent
 */
static struct rbtree_node *
_grandparent(struct rbtree_node *node)
{
    if ( NULL != node && NULL != node->parent ) {
        /* Return parent's parent, i.e., grandparent */
        return node->parent->parent;
    } else {
        /* No grandparent */
        return NULL;
    }
}

/*
 * Get uncle
 */
static struct rbtree_node *
_uncle(struct rbtree_node *node)
{
    struct rbtree_node *grandparent;

    grandparent = _grandparent(node);
    if ( NULL == grandparent ) {
        /* No grandparent, i.e., no uncle */
        return NULL;
    } else {
        if ( node->parent == grandparent->left ) {
            return grandparent->right;
        } else {
            return grandparent->left;
        }
    }

    /* Not to be reached here */
    return NULL;
}

/*
 * Get sibling
 */
static struct rbtree_node *
_sibling(struct rbtree_node *node)
{
    if ( node == node->parent->left ) {
        return node->parent->right;
    } else {
        return node->parent->left;
    }
}

/*
 * Is leaf node?
 */
static int
_is_leaf(struct rbtree_node *node)
{
    if ( NULL == node->key ) {
        return 1;
    }
    return 0;
}


/*
 * Initialize red-black tree
 */
struct rbtree *
rbtree_init(struct rbtree *ptr, int (*compare)(const void *, const void *))
{
    if ( NULL == ptr ) {
        /* Allocate new */
        ptr = kmalloc(sizeof(struct rbtree));
        if ( NULL == ptr ) {
            /* Memory error */
            return NULL;
        }
        ptr->_need_to_free = 1;
    } else {
        ptr->_need_to_free = 0;
    }
    ptr->root = NULL;
    ptr->compare = compare;

    return ptr;
}

/*
 * Release red-black tree
 */
void
rbtree_release(struct rbtree *rbtree)
{
    _node_recursive_delete(rbtree->root, NULL, NULL);
    rbtree->root = NULL;
    if ( rbtree->_need_to_free ) {
        kfree(rbtree);
    }
}

/*
 * Release red-black tree with callback function
 */
void
rbtree_release_callback(
    struct rbtree *rbtree, void (*func)(void *, void *), void *userdata)
{
    _node_recursive_delete(rbtree->root, func, userdata);
    rbtree->root = NULL;
    if ( rbtree->_need_to_free ) {
        kfree(rbtree);
    }
}

/*
 * Search a node by specified key
 */
void *
rbtree_search(struct rbtree *rbtree, void *key)
{
    return _search(rbtree->root, key, rbtree->compare);
}

/*
 * Insert new node
 */
int
rbtree_insert(struct rbtree *rbtree, void *key)
{
    int ret;
    struct rbtree_node *node;

    ret = _insert(&rbtree->root, key, rbtree->compare);
    if ( 0 != ret ) {
        return ret;
    }
    /* Find new root */
    node = rbtree->root;
    while ( NULL != node->parent ) {
        node = node->parent;
    }
    rbtree->root = node;

    return 0;
}

/*
 * Delete a key
 */
void *
rbtree_delete(struct rbtree *rbtree, void *key)
{
    void *deleted_key;
    struct rbtree_node *node;

    /* Delete the specified key */
    deleted_key = _delete(&rbtree->root, key, rbtree->compare);
    if ( NULL == deleted_key ) {
        return NULL;
    }
    /* Find new root */
    node = rbtree->root;
    while ( NULL != node->parent ) {
        node = node->parent;
    }
    rbtree->root = node;

    return deleted_key;
}

/*
 * Pop a key
 */
void *
rbtree_pop(struct rbtree *rbtree)
{
    void *deleted_key;
    struct rbtree_node *node;

    /* Delete the specified key */
    deleted_key = _pop(&rbtree->root);
    if ( NULL == deleted_key ) {
        return NULL;
    }
    /* Find new root */
    node = rbtree->root;
    while ( NULL != node->parent ) {
        node = node->parent;
    }
    rbtree->root = node;

    return deleted_key;
}

/*
 * Get the min key
 */
void *
rbtree_min(struct rbtree *rbtree)
{
    return _min(rbtree->root);
}

/*
 * Execute a function for all values
 */
void
rbtree_exec_all(
    struct rbtree *rbtree, void (*func)(void *, void *), void *userdata)
{
    _exec(rbtree->root, func, userdata);
}

/*
 * Initialize iterator
 */
struct rbtree_iterator *
rbtree_iterator_init(struct rbtree_iterator *iter)
{
    if ( NULL == iter ) {
        iter = kmalloc(sizeof(struct rbtree_iterator));
        if ( NULL == iter ) {
            /* Memory error */
            return NULL;
        }
        iter->_need_to_free = 1;
    } else {
        iter->_need_to_free = 0;
    }
    iter->cur = NULL;
    iter->prev = NULL;

    return iter;
}

/*
 * Release iterator
 */
void
rbtree_iterator_release(struct rbtree_iterator *iter)
{
    if ( iter->_need_to_free ) {
        kfree(iter);
    }
}

/*
 * Iteration operations
 */
void *
rbtree_iterator_cur(struct rbtree_iterator *iter)
{
    if ( NULL != iter->cur ) {
        return iter->cur->key;
    }
    return NULL;
}
void *
rbtree_iterator_next(struct rbtree *rbtree, struct rbtree_iterator *iter)
{
    int term;

    if ( NULL == iter->cur ) {
        /* Root */
        if ( NULL != rbtree->root && NULL != rbtree->root->key ) {
            iter->cur = rbtree->root;
        }
    } else {
        /* Next */
        term = 0;
        while ( !term ) {
            if ( iter->prev == iter->cur->parent ) {
                /* From parent, then search left, right, parent */
                if ( NULL != iter->cur->left->key ) {
                    /* Left */
                    iter->prev = iter->cur;
                    iter->cur = iter->cur->left;
                    term = 1;
                } else if ( NULL != iter->cur->right->key ) {
                    /* Right */
                    iter->prev = iter->cur;
                    iter->cur = iter->cur->right;
                    term = 1;
                } else {
                    /* Parent */
                    iter->prev = iter->cur;
                    iter->cur = iter->cur->parent;
                }
            } else if ( iter->prev == iter->cur->left ) {
                /* From left child, then search right, parent */
                if ( NULL != iter->cur->right->key ) {
                    /* Right */
                    iter->prev = iter->cur;
                    iter->cur = iter->cur->right;
                    term = 1;
                } else {
                    /* Parent */
                    iter->prev = iter->cur;
                    iter->cur = iter->cur->parent;
                }
            } else if ( iter->prev == iter->cur->right ) {
                /* From right child, then search parent */
                iter->prev = iter->cur;
                iter->cur = iter->cur->parent;
            } else {
                /* Not to be reached here */
                return NULL;
            }
            /* Ended */
            if ( NULL == iter->cur ) {
                term = 1;
            }
        }
    }

    if ( NULL == iter->cur || NULL == iter->cur->key ) {
        return NULL;
    } else {
        return iter->cur->key;
    }
}
void
rbtree_iterator_rewind(struct rbtree_iterator *iter)
{
    iter->cur = NULL;
    iter->prev = NULL;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
