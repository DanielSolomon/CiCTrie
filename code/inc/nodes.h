#pragma once

#include <stdint.h>

#define W 5
// The maximun number of branches going out of a CNode. Must match the bitmap size.
#define MAX_BRANCHES (2 << W)


// TODO Consider change all complicated objects to pointers (for efficiency [copy by value] and generalizing cases).

typedef struct main_node_t main_node_t;

typedef enum 
{
    CNODE,
    SNODE,
    INODE,
    TNODE,
    LNODE
} node_type_t;

typedef struct
{
    int key;
    int value;
} snode_t;

typedef struct 
{
    snode_t snode;
} tnode_t;

typedef struct lnode_t
{
    snode_t snode;
    struct lnode_t* next;
} lnode_t;

typedef struct
{
    main_node_t* main;
} inode_t;

typedef struct
{
    node_type_t type;
    union
    {
        inode_t inode;
        snode_t snode;
    } node;
} branch_t;

typedef struct
{
    uint32_t bmp;
    uint32_t length;
    branch_t* array[MAX_BRANCHES];
} cnode_t;

struct main_node_t 
{
    node_type_t type;
    union
    {
        cnode_t cnode;
        tnode_t tnode;
        lnode_t lnode;
    } node;
};

