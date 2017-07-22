#pragma once
#include <stdint.h>
#include "nodes.h"

typedef struct ctrie_t
{
    inode_t*  inode;
    uint8_t  readonly;
    int      (*insert) (struct ctrie_t* ctrie, int key, int value);
    int      (*remove) (struct ctrie_t* ctrie, int key);
    int      (*lookup) (struct ctrie_t* ctrie, int key);
    void     (*free)   (struct ctrie_t* ctrie);
} ctrie_t;

ctrie_t* create_ctrie();

