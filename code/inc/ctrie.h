#pragma once

#include <stdint.h>

#include "hazard_pointer.h"

#define OK          (0)
#define FAILED      (-1)
#define NOTFOUND    (-2)
#define RESTART     (-3)

typedef struct ctrie_t
{
    root_node_t* root;
    uint8_t  readonly;
    int      (*insert)  (struct ctrie_t* ctrie, int key, int value, thread_args_t* thread_args);
    int      (*remove)  (struct ctrie_t* ctrie, int key, thread_args_t* thread_args);
    int      (*lookup)  (struct ctrie_t* ctrie, int key, thread_args_t* thread_args);
    struct ctrie_t* (*snapshot)(struct ctrie_t* ctrie);
    void     (*free)    (struct ctrie_t* ctrie);
} ctrie_t;

ctrie_t* create_ctrie();

