#ifndef LNODE_H
#define LNODE_H
#include "CSError.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Linked list node
struct LNode {
        unsigned       integer;
        void *         data;
        struct LNode * next;
};

static inline
CSError LNode_prepend(struct LNode ** root, void * data, unsigned integer)
{
        assert(root);
        if (NULL == root) { goto fail;}
        struct LNode * new_root = malloc(sizeof(struct LNode));
        if (NULL == new_root) { goto bad_alloc1; }
        new_root->next = *root;
        new_root->data = data;
        new_root->integer = integer;
        *root = new_root;
        return NO_FAILURE;
fail:
        return FAIL_PARAM;
bad_alloc1:
        return FAIL_ALLOC;
}
static inline
void * LNode_pop(struct LNode ** root)
{
        if (NULL == root || NULL == *root) {
                goto no_root;
        }
        void * ret = (*root)->data;
        struct LNode * next = (*root)->next;
        free(*root);
        *root = next;
        return ret;
no_root:
        return NULL;
}
static inline
void LNode_destroy(struct LNode * root)
{
	    while (root) {
                struct LNode * next = root->next;
                free(root);
				root = next;
        }
}
static inline
unsigned LNode_count(struct LNode * root)
{
        if (root == NULL) {
                return 0;
        } else {
                return 1 + LNode_count(root->next);
        }
}
static inline
CSError LNode_remove_node(struct LNode ** root, void * data)
{
        if (root == NULL || *root == NULL) {
                return FAILURE;
        }
        if ((*root)->data == data) {
                struct LNode * next = (*root)->next;
                free(*root);
                *root = next;
                return NO_FAILURE;
        } else {
                return LNode_remove_node(&(*root)->next, data);
        }
}

#endif
