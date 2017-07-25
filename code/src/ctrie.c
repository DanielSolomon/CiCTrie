#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "ctrie.h"

#define NOTFOUND (0)
#define RESTART -1
#define FAILED -2
#define OK 0
#define CAS(ptr, old, new) __sync_bool_compare_and_swap(ptr, old, new)

/*
#define CAS_IT(node, msg) do {  \
    
                if (new_main_node == NULL)
                {
                    return FAILED;
                }
                if (CAS(&(inode->main), main_node, new_main_node))
                {
                    return OK;
                }
                else
                {
                    main_node_free(new_main_node);
                    return RESTART;
                }
*/

static int  ctrie_insert(ctrie_t* ctrie, int key, int value);
static int  ctrie_remove(ctrie_t* ctrie, int key);
static int  ctrie_lookup(ctrie_t* ctrie, int key);
static void ctrie_free  (ctrie_t* ctrie);

static void lnode_free(lnode_t* lnode);

static void main_node_free  (main_node_t* main_node);

ctrie_t* create_ctrie()
{
    ctrie_t* ctrie = malloc(sizeof(ctrie_t));
    if (ctrie == NULL) 
    {
        FAIL("allocating %d bytes failed", sizeof(ctrie_t));
    }
    inode_t* inode = malloc(sizeof(inode_t));
    if (inode == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    main_node_t* main_node = malloc(sizeof(main_node_t));
    if (main_node == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }

    cnode_t cnode           = {0};
    main_node->type         = CNODE;
    main_node->node.cnode   = cnode;
    inode->main             = main_node;
    ctrie->inode            = inode;
    ctrie->readonly         = 0;
    ctrie->insert           = ctrie_insert;
    ctrie->remove           = ctrie_remove;
    ctrie->lookup           = ctrie_lookup;
    ctrie->free             = ctrie_free;
    return ctrie;

CLEANUP:
    if (ctrie != NULL)
    {
        free(ctrie);
    }
    if (inode != NULL)
    {
        free(inode);
    }
    return NULL;
}

/**
 * Frees `branch` and all it's decendants.
 * NOTE: not thread-safe.
 */
static void branch_free(branch_t* branch)
{
    if (branch != NULL)
    {
        switch (branch->type)
        {
        case INODE:
            main_node_free(branch->node.inode.main);
            break;
        case SNODE:
            break;
        default:
            break;
        }
        free(branch);
    }
}

/**
 * Frees the lnode-list beginning with `lnode`.
 * NOTE: not thread-safe.
 */
static void lnode_free(lnode_t* lnode)
{
    if (lnode != NULL)
    {
        lnode_free(lnode->next);
        free(lnode);
    }
}

/**
 * Frees `main_node` and all it's decendants
 * NOTE: not thread-safe.
 */
static void main_node_free(main_node_t* main_node)
{
    int i = 0;
    if (main_node != NULL) 
    {
        switch (main_node->type)
        {
        case CNODE:
            for (i = 0; i < MAX_BRANCHES; i++)
            {
                branch_free(main_node->node.cnode.array[i]);
            }
            break;
        case TNODE:
            break;
        case LNODE:
            lnode_free(main_node->node.lnode.next);
            break;
        default:
            break;
        }
        free(main_node);
    }
}

/**
 * Frees `inode` and all it's decendants
 * NOTE: not thread-safe.
 */
static void inode_free(inode_t* inode)
{
    if (inode != NULL)
    {
        main_node_free(inode->main);
        free(inode);
    }
}

// TODO Consider making ctrie_free thread-safe by a single mutex only for integrity of the code.
/**
 * Frees the entire ctrie
 * NOTE: not thread-safe.
 */
static void ctrie_free(ctrie_t* ctrie)
{
    if (ctrie != NULL) 
    {
        inode_free(ctrie->inode);
        free(ctrie);
    }
}

/**
 * Searches for `key` in the lnode-list beginning with `lnode`.
 * Returns the value if found, else returns NOTFOUND.
 */
static int lnode_lookup(lnode_t* lnode, int key)
{
    lnode_t* ptr = lnode;
    while (ptr != NULL)
    {
        if (ptr->snode.key == key)
        {
            return ptr->snode.value;
        }
        ptr = ptr->next;
    }
    return NOTFOUND;
}

static int hash(int key)
{
    // TODO hash
    return key;
}

static void clean(inode_t* inode, int lev)
{
    // TODO
}

/**
 * Searches for `key` in `inode`'s decendants.
 * Returns the value if found, NOTFOUND if the key doesn't exists, or RESTART if the lookup needs to be called again.
 */
static int internal_lookup(inode_t* inode, int key, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return NOTFOUND;
    }

    int pos  = 0;
    int flag = 0;
    branch_t* branch = NULL;

    // Check the inode's child.
    switch(inode->main->type)
    {
    case CNODE:
        // CNode - compute the branch with the relevant hash bits and search in it.
        pos = (hash(key) >> lev) & 0x1f;
        flag = 1 << pos;
        // Check if the branch is empty.
        if ((flag & inode->main->node.cnode.bmp) == 0)
        {
            return NOTFOUND;
        }
        branch = inode->main->node.cnode.array[pos];
        switch (branch->type)
        {
        case INODE:
            // INode - recursively lookup.
            return internal_lookup(&(branch->node.inode), key, lev + W, inode);
        case SNODE:
            // SNode - simply compare the keys.
            if (key == branch->node.snode.key)
            {
                return branch->node.snode.value;
            }
            return NOTFOUND;
        default:
            break;
        }
    case TNODE:
        // TNode - help resurrect it and restart.
        clean(parent, lev - W);
        return RESTART;
    case LNODE:
        // LNode - search the linked list.
        return lnode_lookup(&(inode->main->node.lnode), key);
    default:
        // TODO Dafuq?
        return NOTFOUND;
    }
}

/**
 * Searches for `key` in the ctrie.
 * Returns the value if found, else returns NOTFOUND.
 */
static int ctrie_lookup(struct ctrie_t* ctrie, int key)
{
    int res = RESTART;
    do {
        res = internal_lookup(ctrie->inode, key, 0, NULL);
    }
    while (res == RESTART);
    return  res;
}

/**
 * Creates a copy of `cnode`, with a branch to an SNode of (`key`, `value`) in position `pos`.
 * Returns the created CNode if successful, else returns NULL.
 */
static main_node_t* cnode_insert(main_node_t* main_node, int pos, int flag, int key, int value)
{
    main_node_t* new_main_node = NULL;
    branch_t* branch = NULL;
    MALLOC(branch, branch_t);
    MALLOC(new_main_node, main_node_t);

    branch->type = SNODE;
    branch->node.snode.key = key;
    branch->node.snode.value = value;

    new_main_node->type = CNODE;
    new_main_node->node.cnode = main_node->node.cnode;
    new_main_node->node.cnode.bmp |= flag;
    new_main_node->node.cnode.array[pos] = branch;
    
    return new_main_node;
CLEANUP:
    if (branch != NULL)
    {
        free(branch);
    }
    if (new_main_node != NULL)
    {
        free(new_main_node);
    }
    return NULL;
}

/**
 * Creates a copy of `cnode`, but updated the branch in position `pos` to an SNode of (`key`, `value`).
 * Returns the created CNode if successful, else returns NULL.
 * TODO This is very similar to cnode_insert...
 */
static main_node_t* cnode_update(main_node_t* main_node, int pos, int key, int value)
{
    // cnode_insert and cnode_update differ only in updating flag (passing flag 0 does nothing).
    return cnode_insert(main_node, pos, 0, key, value);
}

static main_node_t* cnode_update_branch(main_node_t* main_node, int pos, branch_t* branch)
{
    main_node_t* new_main_node = NULL;
    MALLOC(new_main_node, main_node_t);

    new_main_node->type = CNODE;
    new_main_node->node.cnode = main_node->node.cnode;
    new_main_node->node.cnode.array[pos] = branch;
    
    return new_main_node;

CLEANUP:
    if (new_main_node != NULL)
    {
        free(new_main_node);
    }
    return NULL;
}

static branch_t* create_branch(int lev, snode_t* old_snode, snode_t* new_snode)
{
    branch_t* branch = NULL;
    branch_t* child  = NULL;
    branch_t* sibling1 = NULL;
    branch_t* sibling2 = NULL;
    lnode_t*  next   = NULL;
    main_node_t* main_node = NULL;

    MALLOC(main_node, main_node_t);
    MALLOC(branch, branch_t);

    if (lev < MAX_BRANCHES) 
    {
        cnode_t cnode = {0};
        int pos1 = (hash(old_snode->key) >> lev) & 0x1f;
        int pos2 = (hash(new_snode->key) >> lev) & 0x1f;
        if (pos1 == pos2)
        {
            child = create_branch(lev + W, old_snode, new_snode);
            if (child == NULL)
            {
                FAIL("failed to create child branch");
            }
            cnode.array[pos1] = child;
        }
        else 
        {
            MALLOC(sibling1, branch_t);
            MALLOC(sibling2, branch_t);
            sibling1->type = SNODE;
            sibling2->type = SNODE;
            sibling1->node.snode = *old_snode;
            sibling2->node.snode = *new_snode;
            cnode.array[pos1] = sibling1;
            cnode.array[pos2] = sibling2;
        }
        cnode.bmp = (1 << pos1) | (1 << pos2);
        main_node->type = CNODE;
        main_node->node.cnode = cnode;
    }
    else
    {
        MALLOC(next, lnode_t);
        next->snode = *new_snode;
        main_node->type = LNODE;
        main_node->node.lnode.snode  = *old_snode;
        main_node->node.lnode.next   = next;
    }

    branch->type = INODE;
    branch->node.inode.main = main_node;
    return branch;

CLEANUP:
    if (next != NULL)
    {
        free(next);
    }
    if (main_node != NULL)
    {
        free(main_node);
    }
    if (branch != NULL)
    {
        free(branch);
    }
    if (child  != NULL)
    {
        free(child);
    }
    if (sibling1 != NULL)
    {
        free(sibling1);
    }
    if (sibling2 != NULL)
    {
        free(sibling2);
    }
    return NULL;
}

static main_node_t* lnode_insert(main_node_t* main_node, snode_t* snode)
{
    main_node_t* new_main_node = NULL;
    lnode_t* next = NULL;
    MALLOC(new_main_node, main_node_t);
    MALLOC(next, lnode_t);

    next->snode = main_node->node.lnode.snode;
    next->next = main_node->node.lnode.next;

    new_main_node->type = LNODE;
    new_main_node->node.lnode.snode = *snode;
    new_main_node->node.lnode.next = next;

    return new_main_node;

CLEANUP:
    if (new_main_node != NULL)
    {
        free(new_main_node);
    }
    if (next != NULL)
    {
        free(next);
    }
    return NULL;
}

/**
 * Attempts to insert (`key`, `value`) to the subtree of `inode`.
 * Returns OK if successful, FAILED if an error occured or RESTART if the insert should be called again.
 */
static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return FAILED;
    }

    int pos  = 0;
    int flag = 0;
    branch_t* branch = NULL;
    main_node_t* main_node = NULL;
    branch_t* child = NULL;
    // Check the inode's child.
    main_node = inode->main;
    switch(main_node->type)
    {
    case CNODE:
        // CNode - compute the branch with the relevant hash bits and insert in it.
        pos = (hash(key) >> lev) & 0x1f;
        flag = 1 << pos;
        // Check if the branch is empty.
        if ((flag & main_node->node.cnode.bmp) == 0)
        {
            // If so, simply create a new branch to an SNode and insert it.
            main_node_t* new_main_node = cnode_insert(main_node, pos, flag, key, value);
            if (new_main_node == NULL)
            {
                return FAILED;
            }
            if (CAS(&(inode->main), main_node, new_main_node))
            {
                return OK;
            }
            else
            {
                main_node_free(new_main_node);
                return RESTART;
            }
        }
        // Check the branch.
        branch = main_node->node.cnode.array[pos];
        switch (branch->type)
        {
        case INODE:
            // INode - recursively insert.
            return internal_insert(&(branch->node.inode), key, value, lev + W, inode);
        case SNODE:
            if (key == branch->node.snode.key)
            {
                main_node_t* new_main_node = cnode_update(main_node, pos, key, value);
                if (new_main_node == NULL)
                {
                    FAIL("Failed to update cnode");
                }
                if (CAS(&(inode->main), main_node, new_main_node))
                {
                    return OK;
                }
                else
                {
                    main_node_free(new_main_node);
                    return RESTART;
                }
            }
            else 
            {
                snode_t new_snode = { .key = key, .value = value };
                child = create_branch(lev, &(branch->node.snode), &new_snode);
                if (child == NULL)
                {
                    return FAILED;
                }
                main_node_t* new_main_node = cnode_update_branch(main_node, pos, child);
                if (new_main_node == NULL)
                {
                    FAIL("Failed to update cnode branch");
                }

                if (CAS(&(inode->main), main_node, new_main_node))
                {
                    return OK;
                }
                else
                {
                    main_node_free(new_main_node);
                    return RESTART;
                }
            }
        default:
            break;
        }
    case TNODE:
        //TODO
        break;
    case LNODE:
    {
        snode_t new_snode = { .key = key, .value = value };
        main_node_t* new_main_node = lnode_insert(main_node, &new_snode);
        if (NULL == new_main_node)
        {
            FAIL("failed to insert to lnode list");
        }
        if (CAS(&(inode->main), main_node, new_main_node))
        {
            return OK;
        }
        else
        {
            main_node_free(new_main_node);
            return RESTART;
        }
    }
    default:
        // TODO Dafuq?
        return FAILED;
    }

CLEANUP:
    if (child != NULL)
    {
        branch_free(child);
    }
    return FAILED;
}

/**
 * Attempts to insert (`key`, `value`) to the ctrie.
 * Returns OK if successful or FAILED if an error occured.
 */
static int ctrie_insert(ctrie_t* ctrie, int key, int value)
{
    int res = RESTART;
    do {
        res = internal_insert(ctrie->inode, key, value, 0, NULL);
    }
    while (res == RESTART);
    return  res;
}

static int ctrie_remove(ctrie_t* ctrie, int key)
{
    return FAILED;
}
