#include <stdio.h>
#include <stdlib.h>
#include <nodes.h>

#include "common.h"
#include "ctrie.h"

/*************
 * CONSTANTS *
 *************/

#define OK          (0)
#define NOTFOUND    (0)
#define RESTART     (-1)
#define FAILED      (-2)

/*************************
 * Functions Declaration *
 *************************/

/***********************
 * CTrie API functions *
 ***********************/

static int  ctrie_insert(ctrie_t* ctrie, int key, int value);
static int  ctrie_remove(ctrie_t* ctrie, int key);
static int  ctrie_lookup(ctrie_t* ctrie, int key);
static void ctrie_free  (ctrie_t* ctrie);

/******************
 * Free functions *
 ******************/

static void branch_free   (branch_t* branch);
static void inode_free    (inode_t* inode);
static void lnode_free    (lnode_t* lnode);
static void main_node_free(main_node_t* main_node);

/*******************
 * Clean functions *
 *******************/

static void         clean        (inode_t* inode, int lev);
static void         clean_parent (inode_t* parent, inode_t* inode, int key_hash, int lev);
static main_node_t* to_compressed(cnode_t* old_cnode, int lev);
static main_node_t* to_contracted(main_node_t* main_node, int lev);

/*******************
 * Death functions *
 *******************/

static tnode_t   entomb   (snode_t* snode);
static branch_t* resurrect(inode_t* inode);

/***********************
 * Internals functions *
 ***********************/

static int internal_lookup(inode_t* inode, int key, int lev, inode_t* parent);
static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent);
static int internal_remove(inode_t* inode, int key, int lev, inode_t* parent);

/*******************
 * CNode functions *
 *******************/

static main_node_t* cnode_insert(main_node_t* main_node, int pos, int flag, int key, int value);
static main_node_t* cnode_update(main_node_t* main_node, int pos, int key, int value);
static main_node_t* cnode_update_branch(main_node_t* main_node, int pos, branch_t* branch);
static main_node_t* cnode_remove(main_node_t* main_node, int pos, int flag);

/*******************
 * LNode functions *
 *******************/

static main_node_t* lnode_insert(main_node_t* main_node, snode_t* snode);
static main_node_t* lnode_copy  (main_node_t* main_node);
static main_node_t* lnode_remove(main_node_t* main_node, int key, int* error, int* value);
static int          lnode_length(lnode_t* lnode);
static int          lnode_lookup(lnode_t* lnode, int key);

/*********
 * Other *
 *********/

static int          hash         (int key);
static branch_t*    create_branch(int lev, snode_t* old_snode, snode_t* new_snode);

/*******************
 * MACRO FUNCTIONS *
 *******************/

#define CAS(ptr, old, new) __sync_bool_compare_and_swap(ptr, old, new)
#define CAS_OR_RESTART(CASed, old, new, msg) do {   \
    if (new == NULL)                        \
        FAIL(msg);                          \
    if (CAS(CASed, old, new))               \
        return OK;                          \
    else {                                  \
        main_node_free(new);                \
        return RESTART;                     \
    }                                       \
} while (0)

/**
 * Creates CTrie instance.
 * @return On success initialized CTrie instance is returned, otherwise NULL is returned.
 **/
ctrie_t* create_ctrie()
{
    ctrie_t*        ctrie       = NULL;
    inode_t*        inode       = NULL;
    main_node_t*    main_node   = NULL;
    MALLOC(ctrie, ctrie_t);
    MALLOC(inode, inode_t);
    MALLOC(main_node, main_node_t);

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
    free_them_all(3, ctrie, inode, main_node);
    return NULL;
}

/**
 * Frees `branch` and all its decendants.
 * @param branch: branch pointer to be freed.
 * @note not thread-safe.
 **/
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
 * @param lnode : lnode pointer to be freed.
 * @note not thread-safe.
 **/
static void lnode_free(lnode_t* lnode)
{
    if (lnode != NULL)
    {
        lnode_free(lnode->next);
        free(lnode);
    }
}

/**
 * Frees `main_node` and all its decendants.
 * @param main_node: main node pointer to be freed.
 * @note not thread-safe.
 **/
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
 * Frees `inode` and all its decendants
 * @param inode: inode pointer to be freed.
 * @note not thread-safe.
 **/
static void inode_free(inode_t* inode)
{
    if (inode != NULL)
    {
        main_node_free(inode->main);
        free(inode);
    }
}

/**
 * Frees the entire `ctrie`.
 * @param ctrie: ctrie pointer to be freed.
 * @note not thread-safe.
 * @todo consider making ctrie_free thread-safe by a single mutex only for the integrity of the code.
 **/
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
 * @param lnode: lnode-list to search in.
 * @param key: to search for.
 * @return if key is found returns the related value, otherwise returns NOTFOUND.
 **/
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

/**
 * Calculates the hash (32-bit) value of a `key`.
 * @param key: key to find its hash value.
 * @return the hash of the given key.
 **/
static int hash(int key)
{
    // TODO: hash
    return key;
}

/**
 * Wraps tnode around snode.
 * @param snode: snode pointer which will become tnode.
 * @return tnode that wraps a copy of the given snode.
 **/
static tnode_t entomb(snode_t* snode)
{
    return (tnode_t) {.snode = *snode};
}

/**
 * Revives inode content.
 * @param inode: inode pointer to revive its content if needed.
 * @return On success branch_t pointer is returned contains the revived snode or old inode, otherwise NULL is returned.
 **/
// CR: If the inode doesn't point to a tnode (so it doesn't need to be resurrected), why bother replacing the branch?
static branch_t* resurrect(inode_t* inode)
{
    branch_t* new_branch = NULL;
    MALLOC(new_branch, branch_t);

    if (inode->main->type == TNODE)
    {
        new_branch->type        = SNODE;
        new_branch->node.snode  = inode->main->node.tnode.snode;
    }
    else
    {
        new_branch->type        = INODE;
        new_branch->node.inode  = *inode;
    }
    return new_branch;

CLEANUP:
    return NULL;
}

/**
 * Contracts main node if points to a 1-length CNode.
 * @param main_node: main node pointer to be contracted.
 * @param lev: hash level.
 * @return if needed returns new contracted main node, otherwise old main node is returned.
 **/
static main_node_t* to_contracted(main_node_t* main_node, int lev)
{
    if (main_node->type != CNODE)
    {
        return main_node;
    }
    cnode_t* cnode = &(main_node->node.cnode);
    if (lev > 0 && cnode->length == 1)
    {
        int index = highest_on_bit(cnode->bmp);
        if (cnode->array[index]->type == SNODE)
        {
            // CR: We need to allocate a new main_node, changing the original main_node is not good.
            // CR: This also means we can't free the original branch.
            tnode_t tnode = entomb(&(cnode->array[index]->node.snode));
            branch_free(cnode->array[index]);
            main_node->type         = TNODE;
            main_node->node.tnode   = tnode;
        }
    }
    return main_node;
}

/**
 * Compress cnode and wraps it with a main node.
 * @param old_cnode: old cnode to be compressed.
 * @param lev: hash level.
 * @return On success compresed cnode wrapped by a new main_node is returned, otherwise NULL is returned.
 **/
static main_node_t* to_compressed(cnode_t* old_cnode, int lev)
{
    main_node_t* new_main_node  = NULL;
    branch_t*    curr_branch    = NULL;
    branch_t*    new_branch     = NULL;

    MALLOC(new_main_node, main_node_t);
    cnode_t new_cnode           = {0};
    new_cnode.bmp               = old_cnode->bmp;
    new_cnode.length            = old_cnode->length;
    new_main_node->type         = CNODE;
    new_main_node->node.cnode   = new_cnode;

    // CR: wat. You've just set new_main_node->node.cnode to be new_cnode.
    new_cnode = new_main_node->node.cnode;
    int i = 0;
    for (i = 0; i < MAX_BRANCHES; i++)
    {
        // CR: did you mean old_cnode.array[i]?
        if (new_cnode.array[i] != NULL)
        {
            // CR: did you mean old_cnode.array[i]?
            curr_branch = new_cnode.array[i];
            switch (curr_branch->type)
            {
            case SNODE:
                MALLOC(new_branch, branch_t);
                new_branch->type        = SNODE;
                new_branch->node.snode  = curr_branch->node.snode;
                new_cnode.array[i]      = new_branch;
                break;
            case INODE:
                new_branch = resurrect(&(curr_branch->node.inode));
                if (new_branch == NULL)
                {
                    FAIL("Failed to resurrect");
                }
                new_cnode.array[i] = new_branch;
            default:
                break;
            }
        }
    }
    return to_contracted(new_main_node, lev);

CLEANUP:
    main_node_free(new_main_node);
    return NULL;
}

/**
 * Tries to clean inode if it points to a compressable CNode.
 * @param inode: inode to clean.
 * @param lev: hash level.
 **/
static void clean(inode_t* inode, int lev)
{
    main_node_t* old_main_node = inode->main;
    if (inode->main->type == CNODE)
    {
        main_node_t* new_main_node = to_compressed(&(old_main_node->node.cnode), lev);
        if (new_main_node != NULL)
        {
            if (!CAS(&(inode->main), old_main_node, new_main_node)) {
                main_node_free(new_main_node);
            }
            // TODO add old_main_node to free list.
        }
    }
}

/**
 * Persistent cleans the related key_hash member in the CNode pointed by the parent main node.
 * @param parent: parent node to clean its child.
 * @param inode: child of parent.
 * @param key_hash: a hash of the key to be cleaned.
 * @param lev: hash level.
 * @todo this function may fail...
 */
static void clean_parent(inode_t* parent, inode_t* inode, int key_hash, int lev)
{
    main_node_t* parent_main_node   = parent->main;
    main_node_t* main_node          = inode->main;
    main_node_t* new_main_node      = NULL;

    if (parent_main_node->type == CNODE)
    {
        int pos  = (key_hash >> lev) & 0x1f;
        int flag = 1 << pos;
        cnode_t*    parent_cnode    = &(parent_main_node->node.cnode);
        branch_t*   branch          = parent_cnode->array[pos];
        if (parent_cnode->bmp & flag && branch->type == INODE && &(branch->node.inode) == inode && main_node->type == TNODE) {
            // CR: you could use the tnode's key and value directly, without assigning them to a snode.
            snode_t snode = main_node->node.tnode.snode;
            new_main_node = cnode_update(parent_main_node, pos, snode.key, snode.value);
            if (new_main_node == NULL) {
                FAIL("Failed to update cnode");
            }
            new_main_node = to_contracted(new_main_node, lev);
            if (!CAS(&(parent->main), parent_main_node, new_main_node))
            {
                main_node_free(new_main_node);
                clean_parent(parent, inode, key_hash, lev);
            }
        }
        // CR: return aftet this.
    }
CLEANUP:
    main_node_free(new_main_node);
}

/**
 * Searches for `key` in `inode`'s descendants.
 * @param inode: inode to be searched in.
 * @param key: key to be searched for.
 * @param lev: hash level.
 * @param parent: parent inode pointer.
 * @return On success returns the value related to the found key if found, NOTFOUND if the key doesn't exists, or RESTART if the lookup needs to be called again.
 **/
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
            return NOTFOUND;
        }
    case TNODE:
        // TNode - help resurrect it and restart.
        clean(parent, lev - W);
        return RESTART;
    case LNODE:
        // LNode - search the linked list.
        return lnode_lookup(&(inode->main->node.lnode), key);
    default:
        return NOTFOUND;
    }
}

/**
 * Searches for `key` in the ctrie.
 * @param ctrie
 * @param key
 * @return If `key` is found, its value is returned, otherwise NOTFOUND is returned.
 **/
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
 * @param main_node
 * @param pos
 * @param flag
 * @param key
 * @param value
 * @return On success an updated CNode is returned wrapped by main node, otherwise NULL is returned.
 **/
static main_node_t* cnode_insert(main_node_t* main_node, int pos, int flag, int key, int value)
{
    main_node_t*    new_main_node   = NULL;
    branch_t*       branch          = NULL;
    MALLOC(branch, branch_t);
    MALLOC(new_main_node, main_node_t);

    branch->type                = SNODE;
    branch->node.snode.key      = key;
    branch->node.snode.value    = value;

    new_main_node->type                     = CNODE;
    new_main_node->node.cnode               = main_node->node.cnode;
    new_main_node->node.cnode.bmp          |= flag;
    new_main_node->node.cnode.array[pos]    = branch;
    // CR: update length.
    
    return new_main_node;
CLEANUP:
    free_them_all(2, branch, new_main_node);
    return NULL;
}

/**
 * Creates a copy of `cnode`, but updated the branch in position `pos` to an SNode of (`key`, `value`).
 * @param main_node
 * @param pos
 * @param key
 * @param value
 * @return On success updated CNode wrapped by main node is returned, otherwise NULL is returned.
 **/
static main_node_t* cnode_update(main_node_t* main_node, int pos, int key, int value)
{
    // cnode_insert and cnode_update differ only in updating flag (passing flag 0 does nothing).
    return cnode_insert(main_node, pos, 0, key, value);
}

/**
 * Creates new main node which will contain CNode pointed by `main_node` updated by `branch` in position `pos`.
 * @param main_node: main node points to a CNode.
 * @param pos: position where to insert `branch`.
 * @param branch: `branch` to be inserted.
 * @return On success updated CNode wrapped by a main node is returned.
 **/
static main_node_t* cnode_update_branch(main_node_t* main_node, int pos, branch_t* branch)
{
    main_node_t* new_main_node = NULL;
    MALLOC(new_main_node, main_node_t);

    new_main_node->type                     = CNODE;
    new_main_node->node.cnode               = main_node->node.cnode;
    new_main_node->node.cnode.array[pos]    = branch;
    
    return new_main_node;

CLEANUP:
    free_them_all(1, new_main_node);
    return NULL;
}

/**
 * Creates branch points to cnode that contains both old snode and new snode, if needed lnode is created.
 * @param lev: hash level.
 * @param old_snode
 * @param new_snode
 * @return On sucess branch_t* is returned, otherwise NULL is reutrned.
 **/
static branch_t* create_branch(int lev, snode_t* old_snode, snode_t* new_snode)
{
    branch_t*    branch     = NULL;
    branch_t*    child      = NULL;
    branch_t*    sibling1   = NULL;
    branch_t*    sibling2   = NULL;
    lnode_t*     next       = NULL;
    main_node_t* main_node  = NULL;

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
    free_them_all(6, next, main_node, branch, child, sibling1, sibling2);
    return NULL;
}

/**
 * Attempts to insert `snode` into lnode.
 * @param main_node: main node pointer points to a LNode.
 * @param snode: snode to insert into lnode pointed by main_node.
 * @return On success new lnode is returned containing snode wrapped by main node, otherwise NULL is returned.
 **/
static main_node_t* lnode_insert(main_node_t* main_node, snode_t* snode)
{
    main_node_t*    new_main_node   = NULL;
    lnode_t*        next            = NULL;
    MALLOC(new_main_node, main_node_t);
    MALLOC(next, lnode_t);

    next->snode = main_node->node.lnode.snode;
    next->next  = main_node->node.lnode.next;

    new_main_node->type             = LNODE;
    new_main_node->node.lnode.snode = *snode;
    new_main_node->node.lnode.next  = next;

    return new_main_node;

CLEANUP:
    free_them_all(2, new_main_node, next);
    return NULL;
}

/**
 * Copies lnode.
 * @param main_node: main node pointer points to a LNode.
 * @return On success returns a full copy of lnode wrapped by main node, otherwise NULL is returned.
 */
static main_node_t* lnode_copy(main_node_t* main_node)
{
    main_node_t* new_main_node = NULL;
    MALLOC(new_main_node, main_node_t);

    if (main_node->type != LNODE)
    {
        FAIL("Expected lnode, got %d", main_node->type);
    }

    new_main_node->type = LNODE;
    lnode_t* new_lnode  = &(new_main_node->node.lnode);
    lnode_t* old_lnode  = &(main_node->node.lnode);
    new_lnode->snode    = old_lnode->snode;
    new_lnode->next     = NULL;

    while (old_lnode->next)
    {
        old_lnode    = old_lnode->next;
        lnode_t* new = NULL;
        MALLOC(new, lnode_t);

        new->snode      = old_lnode->snode;
        new->next       = NULL;
        new_lnode->next = new;
        new_lnode       = new_lnode->next;
    }

    return new_main_node;

CLEANUP:

    main_node_free(new_main_node);
    return NULL;
}

/**
 * Attempts to remove `key` from `main_node` points to LNode.
 * @param main_node
 * @param key
 * @param error: int pointer that will be filled with `1` if an error occured, `0` otherwise.
 * @param value: int pointer that will be filled with `key`'s value if it will be removed.
 * @return On success, lnode without `key` wrapped by main node is returned, otherwise NULL is returned.
 **/
// CR: Usually functions return the error code and put the value in an OUT param.
static main_node_t* lnode_remove(main_node_t* main_node, int key, int* error, int* value)
{
    main_node_t* new_main_node = NULL;
    *error = 1;

    if (main_node->type != LNODE)
    {
        FAIL("Expected lnode, got %d", main_node->type);
    }

    new_main_node = lnode_copy(main_node);
    if (new_main_node == NULL)
    {
        FAIL("Failed to copy lnode");
    }

    lnode_t* ptr    = &(new_main_node->node.lnode);
    lnode_t* temp   = NULL;

    *error = 0;
    if (ptr->snode.key == key)
    {
        *value      = ptr->snode.value;
        ptr->snode  = ptr->next->snode;
        temp        = ptr->next;
        ptr->next   = temp->next;
        free(temp);
        return new_main_node;
    }
    while (ptr->next != NULL)
    {
        if (ptr->next->snode.key == key)
        {
            temp      = ptr->next;
            ptr->next = temp->next;
            *value    = temp->snode.value;
            free(temp);
            return new_main_node;
        }
        ptr = ptr->next;
    }

CLEANUP:
    main_node_free(new_main_node);
    return NULL;
}

/**
 * Attempts to insert (`key`, `value`) to the subtree of `inode`.
 * @param inode
 * @param key
 * @param value
 * @param lev
 * @param parent
 * @return On failure FAILED is returned, otherwise OK is returned if (`key`, `value`) was inserted, or RESTART if the insert should be called again.
 */
static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return FAILED;
    }

    int pos  = 0;
    int flag = 0;
    branch_t*    branch     = NULL;
    main_node_t* main_node  = NULL;
    branch_t*    child      = NULL;

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
            CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to insert into cnode");
        }
        // Check the branch.
        branch = main_node->node.cnode.array[pos];
        switch (branch->type)
        {
        case INODE:
            // INode - recurively insert.
            return internal_insert(&(branch->node.inode), key, value, lev + W, inode);
        case SNODE:
            if (key == branch->node.snode.key)
            {
                main_node_t* new_main_node = cnode_update(main_node, pos, key, value);
                CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to update cnode");
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
                CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to update cnode branch");
            }
        default:
            break;
        }
    case TNODE:
        clean(parent, lev - W);
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
        return FAILED;
    }

CLEANUP:
    branch_free(child);
    return FAILED;
}

/**
 * Attempts to insert (`key`, `value`) to the ctrie.
 * @param ctrie
 * @param key
 * @param value
 * @return On success, OK is returned, otherwise FAILED is returned.
 **/
static int ctrie_insert(ctrie_t* ctrie, int key, int value)
{
    int res = RESTART;
    do {
        res = internal_insert(ctrie->inode, key, value, 0, NULL);
    }
    while (res == RESTART);
    return res;
}

/**
 * Calculates `lnode` length.
 * @param lnode: lnode pointer.
 * @return `lnode`'s length.
 */
static int lnode_length(lnode_t* lnode)
{
    int length = 0;
    if (lnode == NULL)
    {
        return length;
    }
    // CR: do-while instead of incrementing manually before the loop. This will also cover the check above.
    length++;
    while (lnode->next != NULL)
    {
        length++;
        lnode = lnode->next;
    }
    return length;
}

/**
 *
 * @param main_node: main node which points to a cnode.
 * @param pos: position to removed.
 * @param flag: bmp flag to set off.
 * @return On success a new cnode wrapped by main node is returned (without pos memeber), otherwise NULL is returned.
 **/
static main_node_t* cnode_remove(main_node_t* main_node, int pos, int flag)
{
    main_node_t* new_main_node = NULL;
    MALLOC(new_main_node, main_node_t);

    new_main_node->type                     = CNODE;
    new_main_node->node.cnode               = main_node->node.cnode;
    // CR: Should be &=
    new_main_node->node.cnode.bmp          |= ~flag;
    new_main_node->node.cnode.array[pos]    = NULL;
    new_main_node->node.cnode.length--;

    // TODO free the deleted branch.

    return new_main_node;

CLEANUP:

    return NULL;
}

/**
 * Attempts to remove `key` from the subtree of `inode`.
 * @param inode: subtree from which to remove `key`.
 * @param key: key to be removed.
 * @param lev: hash level.
 * @param parent: parent inode of `inode`.
 * @return On failure, FAILED is returned, otherwise if `key` was removed its value is returned, if `key` couldn't be found NOTFOUND is returned, RESTART my be the result if `internal_remove` shoud be called again.
 **/
static int internal_remove(inode_t* inode, int key, int lev, inode_t* parent)
{
    if (inode->main == NULL)
    {
        return FAILED;
    }

    int          pos        = 0;
    int          flag       = 0;
    branch_t*    branch     = NULL;
    main_node_t* main_node  = NULL;

    // Check the inode's child.
    main_node = inode->main;
    switch(main_node->type)
    {
        case CNODE:
        {
            int res = NOTFOUND;
            // CNode - compute the branch with the relevant hash bits and remove from it.
            pos = (hash(key) >> lev) & 0x1f;
            flag = 1 << pos;
            // Check if the branch is empty.
            if ((flag & main_node->node.cnode.bmp) == 0) {
                // If so, key is not found.
                res = NOTFOUND;
                goto DONE;
            }
            // Check the branch.
            branch = main_node->node.cnode.array[pos];
            switch (branch->type) {
                case INODE:
                    // INode - recursively remove.
                    res = internal_remove(&(branch->node.inode), key, lev + W, inode);
                    break;
                case SNODE:
                    if (key != branch->node.snode.key) {
                        res = NOTFOUND;
                    } else {
                        res = branch->node.snode.value;
                        main_node_t *new_main_node = cnode_remove(main_node, pos, flag);
                        if (new_main_node == NULL) {
                            FAIL("Failed to remove %d from cnode", key);
                        }
                        new_main_node = to_contracted(new_main_node, lev);
                        if (!CAS(&(inode->main), main_node, new_main_node)) {
                            res = RESTART;
                            main_node_free(new_main_node);
                        }
                    }
                default:
                    break;
            }
        DONE:
            if (res == NOTFOUND || res == RESTART) {
                return res;
            }
            if (inode->main->type == TNODE) {
                clean_parent(parent, inode, hash(key), lev - W);
            }
            return res;
        }
        case TNODE:
            clean(parent, lev - W);
            return RESTART;
        case LNODE:
        {
            int error = 0;
            int old_value = 0;
            main_node_t* new_main_node = lnode_remove(main_node, key, &error, &old_value);
            if (error)
            {
                FAIL("failed to remove %d from lnode list", key);
            }
            if (new_main_node == NULL)
            {
                return NOTFOUND;
            }
            // CR: maybe move this into lnode_remove?
            // CR: also, we only use lnode_length to check whether an lnode list has 1 node. Wouldn't it be simpler to check this excplicitly (lnode->next == NULL)?
            if (lnode_length(&(new_main_node->node.lnode)) == 1)
            {
                tnode_t tnode = entomb(&(new_main_node->node.lnode.snode));
                // CR: you can't free here, the CAS might fail.
                lnode_free(&(new_main_node->node.lnode));
                new_main_node->type = TNODE;
                new_main_node->node.tnode = tnode;
            }
            if (CAS(&(inode->main), main_node, new_main_node))
            {
                return old_value;
            }
            else
            {
                main_node_free(new_main_node);
                return RESTART;
            }
        }
        default:
            return FAILED;
    }

CLEANUP:
    return FAILED;
}

/**
 * Removes `key` from `ctrie`.
 * @param ctrie: ctrie pointer from which key will be removed.
 * @param key: key to be removed.
 * @return On failure FAILED is returned, otherwise, if `key` was found, its value is returned and if not NOTFOUND is returned.
 **/
static int ctrie_remove(ctrie_t* ctrie, int key)
{
    int res = RESTART;
    do {
        res = internal_remove(ctrie->inode, key, 0, NULL);
    } while (res == RESTART);
    return res;
}





