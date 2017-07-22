#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ctrie.h"

#define RESTART -1
#define NOTFOUND 0

static int  ctrie_insert(struct ctrie_t* ctrie, int key, int value);
static int  ctrie_remove(struct ctrie_t* ctrie, int key);
static int  ctrie_lookup(struct ctrie_t* ctrie, int key);
static void ctrie_free  (struct ctrie_t* ctrie);

static void main_node_free  (main_node_t* main_node);

ctrie_t* create_ctrie()
{
    ctrie_t* ctrie = malloc(sizeof(ctrie_t));
    if (ctrie == NULL) 
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    inode_t* inode = malloc(sizeof(inode_t));
    if (inode == NULL)
    {
        printf("malloc failed.\n");
        goto CLEANUP;
    }
    inode->main     = NULL;
    ctrie->inode    = inode;
    ctrie->readonly = 0;
    ctrie->insert   = ctrie_insert;
    ctrie->remove   = ctrie_remove;
    ctrie->lookup   = ctrie_lookup;
    ctrie->free     = ctrie_free;
        
    return ctrie;

CLEANUP:
    if (ctrie != NULL)
    {
        free(ctrie);
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
        break;
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
