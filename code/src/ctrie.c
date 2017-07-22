#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "ctrie.h"

#define NOTFOUND 0
#define RESTART -1
#define FAILED -2
#define OK 0
#define CAS(ptr, old, new) __sync_bool_compare_and_swap(ptr, old, new)

static int  ctrie_insert(ctrie_t* ctrie, int key, int value);
static int  ctrie_remove(ctrie_t* ctrie, int key);
static int  ctrie_lookup(ctrie_t* ctrie, int key);
static void ctrie_free  (ctrie_t* ctrie);

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

static void lnode_free(lnode_t* lnode)
{
    if (lnode != NULL)
    {
        lnode_free(lnode->next);
        free(lnode);
    }
}

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
static void inode_free(inode_t* inode)
{
    if (inode != NULL)
    {
        main_node_free(inode->main);
        free(inode);
    }
}

static void ctrie_free(ctrie_t* ctrie)
{
    if (ctrie != NULL) 
    {
        inode_free(ctrie->inode);
        free(ctrie);
    }
}

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

static int internal_lookup(inode_t* inode, int key, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return NOTFOUND;
    }

    int pos  = 0;
    int flag = 0;
    branch_t* branch = NULL;
    switch(inode->main->type)
    {
    case CNODE:
        pos = (hash(key) >> lev) & 0x1f;
        flag = 1 << pos;
        if ((flag & inode->main->node.cnode.bmp) == 0)
        {
            return NOTFOUND;
        }
        branch = inode->main->node.cnode.array[pos];
        switch (branch->type)
        {
        case INODE:
            return internal_lookup(&(branch->node.inode), key, lev + W, inode);
        case SNODE:
            if (key == branch->node.snode.key)
            {
                return branch->node.snode.value;
            }
            return NOTFOUND;
        default:
            break;
        }
    case TNODE:
        clean(parent, lev - W);
        return RESTART;
    case LNODE:
        return lnode_lookup(&(inode->main->node.lnode), key);
    default:
        // TODO Dafuq?
        return NOTFOUND;
    }
}

static int ctrie_lookup(struct ctrie_t* ctrie, int key)
{
    int res = RESTART;
    do {
        res = internal_lookup(ctrie->inode, key, 0, NULL);
    }
    while (res == RESTART);
    return  res;
}

static cnode_t* cnode_insert(cnode_t* cnode, int pos, int flag, int key, int value)
{
    branch_t* branch = malloc(sizeof(branch_t));
    if (branch == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    cnode_t* new_cnode = malloc(sizeof(cnode_t));
    if (new_cnode == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }

    branch->type = SNODE;
    branch->node.snode.key = key;
    branch->node.snode.value = value;
    memcpy(new_cnode, cnode, sizeof(cnode_t));
    new_cnode->bmp = new_cnode->bmp | flag;
    new_cnode->array[pos] = branch;
    
    return new_cnode;
CLEANUP:
    if (branch != NULL)
    {
        free(branch);
    }
    return NULL;
}

static cnode_t* cnode_update(cnode_t* cnode, int pos, int key, int value)
{
    branch_t* new_branch = malloc(sizeof(branch_t));
    if (new_branch == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    cnode_t* new_cnode = malloc(sizeof(cnode_t));
    if (new_cnode == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    new_branch->type = SNODE;
    new_branch->node.snode = (snode_t) {
        .key    = key,
        .value  = value,
    };
    memcpy(new_cnode, cnode, sizeof(cnode_t));
    new_cnode->array[pos] = new_branch;

    return new_cnode;

CLEANUP:
    if (new_branch != NULL)
    {
        free(new_branch);
    }
    return NULL;
}

static inode_t* create_inode(int lev, snode_t* old_snode, snode_t* new_snode)
{
    cnode_t*  cnode  = NULL;
    branch_t* branch = NULL;
    lnode_t*  lnode  = NULL;
    inode_t* inode = malloc(sizeof(inode_t));
    if (inode == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }

    if (lev < MAX_BRANCHES) 
    {
        cnode = malloc(sizeof(cnode_t));
        if (cnode == NULL)
        {
            printf("malloc failed.\n");
            goto CLEANUP;
        }
    }

CLEANUP:
    return NULL;
}

static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return FAILED;
    }

    int pos  = 0;
    int flag = 0;
    branch_t* branch = NULL;
    cnode_t* cnode = NULL;
    switch(inode->main->type)
    {
    case CNODE:
        cnode = &(inode->main->node.cnode);
        pos = (hash(key) >> lev) & 0x1f;
        flag = 1 << pos;
        if ((flag & cnode->bmp) == 0)
        {
            cnode_t* new_cnode = cnode_insert(cnode, pos, flag, key, value);
            if (new_cnode == NULL)
            {
                return FAILED;
            }
            if (CAS(&(inode->main), cnode, new_cnode))
            {
                return OK;
            }
            else
            {
                return RESTART;
            }
        }
        branch = inode->main->node.cnode.array[pos];
        switch (branch->type)
        {
        case INODE:
            return internal_insert(&(branch->node.inode), key, value, lev + W, inode);
        case SNODE:
            if (key == branch->node.snode.key)
            {
                cnode_t* new_cnode = cnode_update(cnode, pos, key, value);
                if (new_cnode == NULL)
                {
                    return FAILED;
                }
                if (CAS(&(inode->main), cnode, new_cnode))
                {
                    return OK;
                }
                else
                {
                    return RESTART;
                }
            }
            else 
            {
                
            }
        default:
            break;
        }
    case TNODE:
        clean(parent, lev - W);
        return RESTART;
    case LNODE:
        return lnode_lookup(&(inode->main->node.lnode), key);
    default:
        // TODO Dafuq?
        return NOTFOUND;
    }
}

static int ctrie_insert(ctrie_t* ctrie, int key, int value)
{
    return FAILED;
}

static int ctrie_remove(ctrie_t* ctrie, int key)
{
    return FAILED;
}
