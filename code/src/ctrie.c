#include <stdio.h>
#include <stdlib.h>

#include "nodes.h"
#include "common.h"
#include "ctrie.h"
#include "hazard_pointer.h"

/*************
 * CONSTANTS *
 *************/

#define OK          (0)
#define FAILED      (-1)
#define NOTFOUND    (-2)
#define RESTART     (-3)

/*************************
 * Functions Declaration *
 *************************/

/***********************
 * CTrie API functions *
 ***********************/

static int  ctrie_insert(ctrie_t* ctrie, int key, int value, thread_args_t* thread_args);
static int  ctrie_remove(ctrie_t* ctrie, int key, thread_args_t* thread_args);
static int  ctrie_lookup(ctrie_t* ctrie, int key, thread_args_t* thread_args);
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

static void         clean        (inode_t* inode, int lev, thread_args_t* thread_args);
static void         clean_parent (inode_t* parent, inode_t* inode, int key_hash, int lev, thread_args_t* thread_args);
static void         compress(main_node_t **cas_address, main_node_t *old_main_node, int lev, thread_args_t *thread_args);
static int          to_contracted(main_node_t* main_node, int lev, branch_t** old_branch, thread_args_t* thread_args);

/*******************
 * Death functions *
 *******************/

static tnode_t   entomb   (snode_t* snode);
static branch_t* resurrect(main_node_t* inode);

/***********************
 * Internals functions *
 ***********************/

static int internal_lookup(inode_t* inode, int key, int lev, inode_t* parent, thread_args_t* thread_args);
static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent, thread_args_t* thread_args);
static int internal_remove(inode_t* inode, int key, int lev, inode_t* parent, thread_args_t* thread_args);

/*******************
 * CNode functions *
 *******************/

static main_node_t* cnode_insert(main_node_t* main_node, int pos, int flag, int key, int value, branch_t** new_branch);
static main_node_t* cnode_update(main_node_t* main_node, int pos, int key, int value, branch_t** new_branch);
static main_node_t* cnode_update_branch(main_node_t* main_node, int pos, branch_t* branch);
static main_node_t* cnode_remove(main_node_t* main_node, int pos, int flag);

/*******************
 * LNode functions *
 *******************/

static int lnode_insert(main_node_t* main_node, snode_t* snode, main_node_t** new_main_node, thread_args_t* thread_args);
static int lnode_copy  (main_node_t* main_node, main_node_t** new_main_node, thread_args_t* thread_args);
static int lnode_remove(main_node_t* main_node, int key, main_node_t** new_main_node, int* value, thread_args_t* thread_args);
static int lnode_lookup(lnode_t* lnode, int key, thread_args_t* thread_args);

/*********
 * Other *
 *********/

static int          hash         (int key);
static branch_t*    create_branch(int lev, snode_t* old_snode, snode_t* new_snode);

/*******************
 * MACRO FUNCTIONS *
 *******************/

#define CAS(ptr, old, new) __sync_bool_compare_and_swap(ptr, old, new)
#define CAS_OR_RESTART(CASed, old, new, msg, thread_args, new_branch) do {   \
    if (new == NULL)                                \
        FAIL(msg);                                  \
    if (CAS(CASed, old, new))                       \
    {                                               \
        DEBUG("CASed old %p and new %p", old, new); \
        old->node.cnode.marked = 1;                 \
        FENCE;                                      \
        add_to_free_list(thread_args, old);         \
    }                                               \
    else                                            \
    {                                               \
        DEBUG("CAS failed");                        \
        free(new);                                  \
        branch_free(new_branch);                    \
        return RESTART;                             \
    }                                               \
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
    DEBUG("branch_free %p", branch);
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
    DEBUG("lnode free %p", lnode);
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
    DEBUG("main_node_free %p", main_node);
    int i = 0;
    if (main_node != NULL) 
    {
        switch (main_node->type)
        {
        case CNODE:
            for (i = 0; i < MAX_BRANCHES; i++)
            {
                if (main_node->node.cnode.bmp & (1 << i))
                {
                    branch_free(main_node->node.cnode.array[i]);
                }
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
 * Frees `main_node` and all the decendants according to the given bmp.
 * @param main_node: main node pointer to be freed.
 * @param bmp: a bitmap of the decendants to be freed.
 * @note not thread-safe. Assumes bmp is included in the main_node's bmp.
 **/
static void selective_main_node_free(main_node_t* main_node, int32_t bmp)
{
    DEBUG("selective_main_node_free %p", main_node);
    int i = 0;
    if (main_node != NULL) 
    {
        switch (main_node->type)
        {
        case CNODE:
            for (i = 0; i < MAX_BRANCHES; i++)
            {
                if (bmp & (1 << i))
                {
                    branch_free(main_node->node.cnode.array[i]);
                }
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
    DEBUG("inode_free %p", inode);
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
 * @param key: the key to search for.
 * @param thread_args: the thread arguments.
 * @return if key is found returns the related value, returns RESTART if some race occurred, otherwise returns NOTFOUND.
 **/
static int lnode_lookup(lnode_t* lnode, int key, thread_args_t* thread_args)
{
    lnode_t* ptr = lnode;
    while (ptr != NULL)
    {
        if (ptr->snode.key == key)
        {
            return ptr->snode.value;
        }
        PLACE_LIST_HP(thread_args, ptr->next);
        if (ptr->marked)
        {
            return RESTART;
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
    //return key / 10;
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
 * Revives main_node's, if it is a tnode.
 * @param main_node: the main node to revive, if needed.
 * @return On success branch_t pointer is returned contains the revived snode, otherwise NULL is returned.
 **/
static branch_t* resurrect(main_node_t* main_node)
{
    DEBUG("resurrecting main_node %p", main_node);
    if (main_node->type != TNODE)
    {
        return NULL;
    }

    branch_t* new_branch = NULL;
    MALLOC(new_branch, branch_t);
    new_branch->type        = SNODE;
    new_branch->node.snode  = main_node->node.tnode.snode;

    return new_branch;

CLEANUP:
    return NULL;
}

/**
 * Contracts main node if points to a 1-length CNode.
 * @param main_node: main node pointer to be contracted.
 * @param lev: hash level.
 * @param old_branch: an out parameter, will contain the replaced branch if the node was contracted. Will be set to NULL if no contraction happened.
 * @param thread_args: the thread arguments.
 * @return RESTART if a race occured, otherwise returns OK.
 **/
static int to_contracted(main_node_t* main_node, int lev, branch_t** old_branch, thread_args_t* thread_args)
{
    *old_branch = NULL;
    if (main_node->type != CNODE)
    {
        return OK;
    }
    cnode_t* cnode = &(main_node->node.cnode);
    if (lev > 0 && cnode->length == 1)
    {
        int index = highest_on_bit(cnode->bmp);
        branch_t* branch = cnode->array[index];
        REPLACE_LAST_HP(thread_args, branch);
        if (cnode->marked || cnode->array[index] != branch)
        {
            return RESTART;
        }
        if (branch->type == SNODE)
        {
            tnode_t tnode           = entomb(&(branch->node.snode));
            *old_branch             = branch;
            main_node->type         = TNODE;
            main_node->node.tnode   = tnode;
        }
    }
    DEBUG("contracted main node %p old branch %p", main_node, *old_branch);
    return OK;
}

/**
 * Compresses old_main_node and tries to replace it.
 * @param cas_address: the address to be CAS'ed.
 * @param old_main_node: the main node to be compressed.
 * @param lev: hash level.
 * @param thread_args: the thread arguments.
 **/
static void compress(main_node_t **cas_address, main_node_t *old_main_node, int lev, thread_args_t *thread_args)
{
    main_node_t* new_main_node  = NULL;
    int32_t      delete_map     = 0;

    cnode_t* cnode              = &(old_main_node->node.cnode);
    MALLOC(new_main_node, main_node_t);
    new_main_node->type         = CNODE;
    new_main_node->node.cnode   = *cnode;

    int i = 0;
    for (i = 0; i < MAX_BRANCHES; i++)
    {
        if (cnode->bmp & (1 << i))
        {
            branch_t* curr_branch = cnode->array[i];
            PLACE_TMP_HP(thread_args, curr_branch);
            if (cnode->marked)
            {
                DEBUG("Failed compress: m: %d !=: %d", cnode->marked, cnode->array[i] != curr_branch);
                goto CLEANUP;
            }
            if (curr_branch->type == INODE && curr_branch->node.inode.main->type == TNODE)
            {
                inode_t*     tmp_inode      = &(curr_branch->node.inode);
                main_node_t* tmp_main_node  = tmp_inode->main;
                DEBUG("Replacing branch %p - main_node %p", curr_branch, tmp_main_node);
                PLACE_TMP_HP(thread_args, tmp_main_node);
                // TODO accessing tmp_inode after replacing HP.
                if (tmp_inode->marked || tmp_inode->main != tmp_main_node)
                {
                    DEBUG("SHEET");
                    goto CLEANUP;
                }
                branch_t* new_branch = resurrect(tmp_main_node);
                if (new_branch == NULL)
                {
                    FAIL("Failed to resurrect");
                }
                new_main_node->node.cnode.array[i] = new_branch;
                delete_map |= 1 << i;
            }
        }
    }
    branch_t* old_branch = NULL;
    if (to_contracted(new_main_node, lev, &old_branch, thread_args) == RESTART)
    {
        // clean is a best effort, if we fail, we clean and return.
        goto CLEANUP;
    }
    DEBUG("to contracted 3");
    if (!CAS(cas_address, old_main_node, new_main_node))
    {
        goto CLEANUP;
    }
    DEBUG("compressed main_node %p new main_node %p", old_main_node, new_main_node);
    cnode->marked = 1;
    for (i = 0; i < MAX_BRANCHES; i++)
    {
        if (delete_map & (1 << i))
        {
            branch_t* branch = cnode->array[i];
            branch->node.inode.marked = 1;
        }
    }
    FENCE;
    for (i = 0; i < MAX_BRANCHES; i++)
    {
        if (delete_map & (1 << i))
        {
            branch_t* branch = cnode->array[i];
            add_to_free_list(thread_args, branch);
            add_to_free_list(thread_args, branch->node.inode.main);
        }
    }
    add_to_free_list(thread_args, old_main_node);
    if (old_branch != NULL)
    {
        add_to_free_list(thread_args, old_branch);
    }
    return;

CLEANUP:
    selective_main_node_free(new_main_node, delete_map);
}

/**
 * Tries to clean inode if it points to a compressable CNode.
 * @param inode: inode to clean.
 * @param lev: hash level.
 * @param thread_args: the thread arguments.
 * @note Assumes that inode and inode->main are protected with HP.
 **/
static void clean(inode_t* inode, int lev, thread_args_t* thread_args)
{
    DEBUG("cleaning inode %p", inode);
    main_node_t* old_main_node = inode->main;
    if (inode->main->type == CNODE)
    {
        compress(&(inode->main), old_main_node, lev, thread_args);
    }
}

/**
 * Persistent cleans the related key_hash member in the CNode pointed by the parent main node.
 * @param parent: parent node to clean its child.
 * @param inode: child of parent.
 * @param key_hash: a hash of the key to be cleaned.
 * @param lev: hash level.
 * @param thread_args: the thread arguments.
 * @todo this function may fail...
 */
static void clean_parent(inode_t* parent, inode_t* inode, int key_hash, int lev, thread_args_t* thread_args)
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
        if (parent_cnode->bmp & flag && branch->type == INODE && &(branch->node.inode) == inode && main_node->type == TNODE) 
        {
            branch_t* new_branch = NULL;
            new_main_node = cnode_update(parent_main_node, pos, main_node->node.tnode.snode.key, main_node->node.tnode.snode.value, &new_branch);
            if (new_main_node == NULL) 
            {
                FAIL("Failed to update cnode");
            }
            branch_t* old_branch = NULL;
            if (to_contracted(new_main_node, lev, &old_branch, thread_args) == RESTART)
            {
                free(new_main_node);
                branch_free(new_branch);
                clean_parent(parent, inode, key_hash, lev, thread_args);
            }
            DEBUG("to contracted 1");
            if (!CAS(&(parent->main), parent_main_node, new_main_node))
            {
                free(new_main_node);
                branch_free(new_branch);
                clean_parent(parent, inode, key_hash, lev, thread_args);
            }

            parent_main_node->node.cnode.marked = 1;
            inode->marked                       = 1;

            FENCE;

            add_to_free_list(thread_args, parent_main_node);
            add_to_free_list(thread_args, branch);
            add_to_free_list(thread_args, main_node);
            if (old_branch != NULL)
            {
                add_to_free_list(thread_args, old_branch);
            }
        }
        return;
    }
CLEANUP:
    return;
}

/**
 * Searches for `key` in `inode`'s descendants.
 * @param inode: inode to be searched in.
 * @param key: key to be searched for.
 * @param lev: hash level.
 * @param parent: parent inode pointer.
 * @param thread_args: the thread arguments.
 * @return On success returns the value related to the found key if found, NOTFOUND if the key doesn't exists, or RESTART if the lookup needs to be called again.
 **/
static int internal_lookup(inode_t* inode, int key, int lev, inode_t* parent, thread_args_t* thread_args)
{
    main_node_t* main_node = inode->main;

    if (main_node == NULL)
    {
        return NOTFOUND;
    }

    PLACE_HP(thread_args, main_node);
    if (inode->marked || inode->main != main_node)
    {
        return RESTART;
    }

    int pos  = 0;
    int flag = 0;
    branch_t* branch = NULL;

    // Check the inode's child.
    switch(main_node->type)
    {
    case CNODE:
        // CNode - compute the branch with the relevant hash bits and search in it.
        pos = (hash(key) >> lev) & 0x1f;
        flag = 1 << pos;
        // Check if the branch is empty.
        if ((flag & main_node->node.cnode.bmp) == 0)
        {
            return NOTFOUND;
        }
        branch = main_node->node.cnode.array[pos];
        PLACE_HP(thread_args, branch);
        if (main_node->node.cnode.marked || main_node->node.cnode.array[pos] != branch)
        {
            return RESTART;
        }
        switch (branch->type)
        {
        case INODE:
            // INode - recursively lookup.
            return internal_lookup(&(branch->node.inode), key, lev + W, inode, thread_args);
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
        clean(parent, lev - W, thread_args);
        return RESTART;
    case LNODE:
        // LNode - search the linked list.
        return lnode_lookup(&(main_node->node.lnode), key, thread_args);
    default:
        return NOTFOUND;
    }
}

/**
 * Searches for `key` in the ctrie.
 * @param ctrie: the ctrie.
 * @param key: the key to be searched for.
 * @param thread_args: the thread arguments.
 * @return If `key` is found, its value is returned, otherwise NOTFOUND is returned.
 **/
static int ctrie_lookup(struct ctrie_t* ctrie, int key, thread_args_t* thread_args)
{
    int res = RESTART;
    do {
        res = internal_lookup(ctrie->inode, key, 0, NULL, thread_args);
        if (res == RESTART)
        {
            DEBUG("restarting lookup!");
        }
    }
    while (res == RESTART);
    return  res;
}

/**
 * Creates a copy of the cnode, with a branch to an SNode of (`key`, `value`) in position `pos`.
 * @param main_node: the main node which contains the cnode.
 * @param pos: the position into which the new snode will be inserted.
 * @param flag: the bitmap flag to be turend on.
 * @param key: the new key.
 * @param value: the new value.
 * @param new_branch: an out parameter that is set to the newly created branch.
 * @return On success an updated cnode is returned wrapped by a main node, otherwise NULL is returned.
 **/
static main_node_t* cnode_insert(main_node_t* main_node, int pos, int flag, int key, int value, branch_t** new_branch)
{
    DEBUG("inserting %d %d to cnode %p", key, value, main_node);
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
    new_main_node->node.cnode.length++;

    *new_branch = branch;

    return new_main_node;
CLEANUP:
    free_them_all(2, branch, new_main_node);
    return NULL;
}

/**
 * Creates a copy of the cnode, and updates the branch in position `pos` to an SNode of (`key`, `value`).
 * @param main_node: the main node which contains the cnode.
 * @param pos: the position in the array to be updated.
 * @param key: the new key.
 * @param value: the new value.
 * @param new_branch: an out parameter that is set to the newly created branch.
 * @return On success the updated cnode wrapped by a main node is returned, otherwise NULL is returned.
 **/
static main_node_t* cnode_update(main_node_t* main_node, int pos, int key, int value, branch_t** new_branch)
{
    // cnode_insert and cnode_update differ only in updating flag (passing flag 0 does nothing).
    DEBUG("updating %d %d to cnode %p", key, value, main_node);
    main_node_t* new_main_node = cnode_insert(main_node, pos, 0, key, value, new_branch);
    new_main_node->node.cnode.length--;
    return new_main_node;
}

/**
 * Creates a copy of the cnode, and updates the branch in position `pos` to be `branch`.
 * @param main_node: the main node which contains the cnode.
 * @param pos: the position in the array to be updated.
 * @param branch: the new branch.
 * @return On success the updated cnode wrapped by a main node is returned, otherwise NULL is returned.
 **/
static main_node_t* cnode_update_branch(main_node_t* main_node, int pos, branch_t* branch)
{
    DEBUG("updating branch %p in pos %d of cnode %p", branch, pos, main_node);
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
 * Creates a branch chain which points to cnode that contains both old snode and new snode. If needed a lnode is created.
 * @param lev: the hash level.
 * @param old_snode: the old snode.
 * @param new_snode: the new snode.
 * @return On sucess the created branch is returned, otherwise NULL is reutrned.
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
            DEBUG("calling create_branch recursively");
            child = create_branch(lev + W, old_snode, new_snode);
            if (child == NULL)
            {
                FAIL("failed to create child branch");
            }
            cnode.array[pos1] = child;
            cnode.length = 1;
        }
        else
        {
            MALLOC(sibling1, branch_t);
            MALLOC(sibling2, branch_t);
            DEBUG("creating siblings %p %p", sibling1, sibling2);
            sibling1->type = SNODE;
            sibling2->type = SNODE;
            sibling1->node.snode = *old_snode;
            sibling2->node.snode = *new_snode;
            cnode.array[pos1] = sibling1;
            cnode.array[pos2] = sibling2;
            cnode.length = 2;
        }
        cnode.bmp = (1 << pos1) | (1 << pos2);
        main_node->type = CNODE;
        main_node->node.cnode = cnode;
    }
    else
    {
        MALLOC(next, lnode_t);
        DEBUG("creating lnode %p", next);
        next->snode = *new_snode;
        main_node->type = LNODE;
        main_node->node.lnode.snode  = *old_snode;
        main_node->node.lnode.next   = next;
    }

    branch->type = INODE;
    branch->node.inode.main = main_node;
    branch->node.inode.marked = 0;
    DEBUG("created branch %p", branch);
    return branch;

CLEANUP:
    free_them_all(6, next, main_node, branch, child, sibling1, sibling2);
    return NULL;
}

/**
 * Attempts to insert `snode` into the lnode list.
 * @param main_node: the main node which contains the lnode.
 * @param snode: the new snode to be inserted.
 * @param new_main_node: an out paramter that is set to the new lnode wrapped by a main node.
 * @param thread_args: the thread arguments.
 * @return OK is returned on success, RESTART if a race occurred and FAILED is returned otherwise.
 **/
static int lnode_insert(main_node_t* main_node, snode_t* snode, main_node_t** new_main_node, thread_args_t* thread_args)
{
    lnode_t* next   = NULL;
    int res = lnode_copy(main_node, new_main_node, thread_args);
    if (res != OK)
    {
        return res;
    }
    MALLOC(next, lnode_t);

    next->snode = (*new_main_node)->node.lnode.snode;
    next->next  = (*new_main_node)->node.lnode.next;

    (*new_main_node)->node.lnode.snode = *snode;
    (*new_main_node)->node.lnode.next  = next;

    return OK;

CLEANUP:
    free_them_all(1, next);
    main_node_free(*new_main_node);
    return FAILED;
}

/**
 * Copies lnode.
 * @param main_node: the main node which contains the lnode.
 * @param new_main_node: an out paramter that is set to the new lnode wrapped by a main node.
 * @param thread_args: the thread arguments.
 * @return OK is returned on success, RESTART if a race occurred and FAILED is returned otherwise.
 */
static int lnode_copy(main_node_t* main_node, main_node_t** new_main_node, thread_args_t* thread_args)
{
    int res = FAILED;
    *new_main_node = NULL;
    MALLOC(*new_main_node, main_node_t);

    if (main_node->type != LNODE)
    {
        FAIL("Expected lnode, got %d", main_node->type);
    }

    (*new_main_node)->type = LNODE;
    lnode_t* new_lnode  = &((*new_main_node)->node.lnode);
    lnode_t* old_lnode  = &(main_node->node.lnode);
    new_lnode->snode    = old_lnode->snode;
    new_lnode->next     = NULL;

    while (old_lnode->next)
    {
        PLACE_LIST_HP(thread_args, old_lnode->next);
        if (old_lnode->marked)
        {
            res = RESTART;
            goto CLEANUP;
        }
        old_lnode    = old_lnode->next;
        lnode_t* new = NULL;
        MALLOC(new, lnode_t);

        new->snode      = old_lnode->snode;
        new->next       = NULL;
        new_lnode->next = new;
        new_lnode       = new_lnode->next;
    }

    return OK;

CLEANUP:

    main_node_free(*new_main_node);
    return res;
}

/**
 * Attempts to remove `key` from `main_node` points to LNode.
 * @param main_node
 * @param key: the key to be removed.
 * @param new_main_node: an out paramter that is set to the new lnode wrapped by a main node.
 * @param value: an out parameter that is set to `key`'s value if it is removed.
 * @param thread_args: the thread arguments.
 * @return Returns OK if successful, FAILED if an error occurred or NOTFOUND if the key wasn't found.
 **/
static int lnode_remove(main_node_t* main_node, int key, main_node_t** new_main_node, int* value, thread_args_t* thread_args)
{
    *new_main_node = NULL;
    int res = FAILED;

    if (main_node->type != LNODE)
    {
        FAIL("Expected lnode, got %d", main_node->type);
    }

    res = lnode_copy(main_node, new_main_node, thread_args);
    if (res != OK)
    {
        return res;
    }

    lnode_t* ptr    = &((*new_main_node)->node.lnode);
    lnode_t* temp   = NULL;

    if (ptr->snode.key == key)
    {
        *value      = ptr->snode.value;
        ptr->snode  = ptr->next->snode;
        temp        = ptr->next;
        ptr->next   = temp->next;
        free(temp);
        goto DONE;
    }
    while (ptr->next != NULL)
    {
        if (ptr->next->snode.key == key)
        {
            temp      = ptr->next;
            ptr->next = temp->next;
            *value    = temp->snode.value;
            free(temp);
            goto DONE;
        }
        ptr = ptr->next;
    }
    res = NOTFOUND;
    goto CLEANUP;

DONE:
    if ((*new_main_node)->node.lnode.next == NULL)
    {
        tnode_t tnode = entomb(&((*new_main_node)->node.lnode.snode));
        (*new_main_node)->type = TNODE;
        (*new_main_node)->node.tnode = tnode;
    }
    return OK;

CLEANUP:
    main_node_free(*new_main_node);
    return res;
}

/**
 * Attempts to insert (`key`, `value`) to the subtree of `inode`.
 * @param inode: the current inode.
 * @param key: the key to be inserted.
 * @param value: the value to be inserted.
 * @param lev: the hash level.
 * @param parent: the parent inode.
 * @param thread_args: the thread arguments.
 * @return On failure FAILED is returned, otherwise OK is returned if (`key`, `value`) was inserted, or RESTART if the insert should be called again.
 */
static int internal_insert(inode_t* inode, int key, int value, int lev, inode_t* parent, thread_args_t* thread_args)
{
    main_node_t* main_node  = inode->main;

    if (main_node == NULL)
    {
        return FAILED;
    }

    int pos  = 0;
    int flag = 0;
    branch_t*    branch     = NULL;
    branch_t*    child      = NULL;

    PLACE_HP(thread_args, main_node);
    if (inode->marked || inode->main != main_node)
    {
        return RESTART;
    }

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
            branch_t* new_branch = NULL;
            main_node_t* new_main_node = cnode_insert(main_node, pos, flag, key, value, &new_branch);
            CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to insert into cnode", thread_args, new_branch);
            //DEBUG("inode %p key %d bmp %x length %d", inode, key, new_main_node->node.cnode.bmp, new_main_node->node.cnode.length);
            return OK;
        }
        // Check the branch.
        branch = main_node->node.cnode.array[pos];
        PLACE_HP(thread_args, branch);
        if (main_node->node.cnode.marked || main_node->node.cnode.array[pos] != branch)
        {
            return RESTART;
        }
        switch (branch->type)
        {
        case INODE:
            // INode - recursively insert.
            return internal_insert(&(branch->node.inode), key, value, lev + W, inode, thread_args);
        case SNODE:
            if (key == branch->node.snode.key)
            {
                branch_t* new_branch = NULL;
                main_node_t* new_main_node = cnode_update(main_node, pos, key, value, &new_branch);
                CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to update cnode", thread_args, new_branch);
                add_to_free_list(thread_args, branch);
                //DEBUG("inode %p key %d bmp %x length %d", inode, key, new_main_node->node.cnode.bmp, new_main_node->node.cnode.length);
                return OK;
            }
            else
            {
                snode_t new_snode = { .key = key, .value = value };
                child = create_branch(lev + W, &(branch->node.snode), &new_snode);
                if (child == NULL)
                {
                    return FAILED;
                }
                main_node_t* new_main_node = cnode_update_branch(main_node, pos, child);
                CAS_OR_RESTART(&(inode->main), main_node, new_main_node, "Failed to update cnode branch", thread_args, child);
                add_to_free_list(thread_args, branch);
                //DEBUG("inode %p key %d bmp %x length %d", inode, key, new_main_node->node.cnode.bmp, new_main_node->node.cnode.length);
                return OK;
            }
        default:
            break;
        }
        break;
    case TNODE:
        clean(parent, lev - W, thread_args);
        break;
    case LNODE:
    {
        snode_t new_snode = { .key = key, .value = value };
        main_node_t* new_main_node = NULL;
        int res = lnode_insert(main_node, &new_snode, &new_main_node, thread_args);
        if (res == OK)
        {
            if (NULL == new_main_node)
            {
                FAIL("failed to insert to lnode list");
            }
            if (CAS(&(inode->main), main_node, new_main_node))
            {
                lnode_t* ptr = main_node->node.lnode.next;
                while (ptr != NULL)
                {
                    ptr->marked = 1;
                    ptr = ptr->next;
                }
                FENCE;
                ptr = main_node->node.lnode.next;
                while (ptr != NULL)
                {
                    add_to_free_list(thread_args, ptr);
                    ptr = ptr->next;
                }
                add_to_free_list(thread_args, main_node);
                return OK;
            }
            else
            {
                main_node_free(new_main_node);
                return RESTART;
            }
        }
        return res;
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
 * @param ctrie: the ctrie.
 * @param key: the new key to be inserted.
 * @param value: the new value to be inserted.
 * @param thread_args: the thread arguments.
 * @return On success, OK is returned, otherwise FAILED is returned.
 **/
static int ctrie_insert(ctrie_t* ctrie, int key, int value, thread_args_t* thread_args)
{
    int res = RESTART;
    do {
        res = internal_insert(ctrie->inode, key, value, 0, NULL, thread_args);
        if (res == RESTART)
        {
            DEBUG("restarting insert!");
        }
    }
    while (res == RESTART);
    return res;
}

/**
 * Removes the branch in position `pos` from a cnode.
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
    new_main_node->node.cnode.bmp          &= ~flag;
    new_main_node->node.cnode.array[pos]    = NULL;
    new_main_node->node.cnode.length--;

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
 * @param thread_args: the thread arguments.
 * @return On failure, FAILED is returned, otherwise if `key` was removed its value is returned, if `key` couldn't be found NOTFOUND is returned, RESTART my be the result if `internal_remove` shoud be called again.
 **/
static int internal_remove(inode_t* inode, int key, int lev, inode_t* parent, thread_args_t* thread_args)
{
    main_node_t* main_node  = inode->main;

    if (main_node == NULL)
    {
        return FAILED;
    }

    int          pos        = 0;
    int          flag       = 0;
    branch_t*    branch     = NULL;

    PLACE_HP(thread_args, main_node);
    if (inode->marked || inode->main != main_node)
    {
        return RESTART;
    }

    // Check the inode's child.
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
            PLACE_HP(thread_args, branch);
            if (main_node->node.cnode.marked || main_node->node.cnode.array[pos] != branch)
            {
                return RESTART;
            }
            switch (branch->type)
            {
                case INODE:
                    // INode - recursively remove.
                    res = internal_remove(&(branch->node.inode), key, lev + W, inode, thread_args);
                    break;
                case SNODE:
                    if (key != branch->node.snode.key)
                    {
                        res = NOTFOUND;
                    }
                    else
                    {
                        res = branch->node.snode.value;
                        main_node_t *new_main_node = cnode_remove(main_node, pos, flag);
                        if (new_main_node == NULL)
                        {
                            FAIL("Failed to remove %d from cnode", key);
                        }
                        branch_t* old_branch = NULL;
                        if (to_contracted(new_main_node, lev, &old_branch, thread_args) == RESTART)
                        {
                            res = RESTART;
                            free(new_main_node);
                            goto DONE;
                        }
                        DEBUG("to contracted 2");
                        if (!CAS(&(inode->main), main_node, new_main_node))
                        {
                            res = RESTART;
                            free(new_main_node);
                            goto DONE;
                        }
                        if (old_branch != NULL)
                        {
                            add_to_free_list(thread_args, old_branch);
                        }
                        main_node->node.cnode.marked = 1;
                        FENCE;
                        add_to_free_list(thread_args, branch);
                        add_to_free_list(thread_args, main_node);
                    }
                default:
                    break;
            }
        DONE:
            if (res == NOTFOUND || res == RESTART)
            {
                return res;
            }
            if (main_node->type == TNODE)
            {
                clean_parent(parent, inode, hash(key), lev - W, thread_args);
            }
            return res;
        }
        case TNODE:
            clean(parent, lev - W, thread_args);
            return RESTART;
        case LNODE:
        {
            int old_value = 0;
            main_node_t* new_main_node = NULL;
            int res = lnode_remove(main_node, key, &new_main_node, &old_value, thread_args);
            switch (res)
            {
            case NOTFOUND:
                return NOTFOUND;
            case FAILED:
                FAIL("failed to remove %d from lnode list", key);
            case OK:
                if (CAS(&(inode->main), main_node, new_main_node))
                {
                    lnode_t* ptr = main_node->node.lnode.next;
                    while (ptr != NULL)
                    {
                        ptr->marked = 1;
                        ptr = ptr->next;
                    }
                    FENCE;
                    ptr = main_node->node.lnode.next;
                    while (ptr != NULL)
                    {
                        add_to_free_list(thread_args, ptr);
                        ptr = ptr->next;
                    }
                    add_to_free_list(thread_args, main_node);
                    return old_value;
                }
                else
                {
                    main_node_free(new_main_node);
                    return RESTART;
                }
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
 * @param thread_args: the thread arguments.
 * @return On failure FAILED is returned, otherwise, if `key` was found, its value is returned and if not NOTFOUND is returned.
 **/
static int ctrie_remove(ctrie_t* ctrie, int key, thread_args_t* thread_args)
{
    int res = RESTART;
    do {
        res = internal_remove(ctrie->inode, key, 0, NULL, thread_args);
        if (res == RESTART)
        {
            DEBUG("restarting remove!");
        }
    } while (res == RESTART);
    return res;
}
