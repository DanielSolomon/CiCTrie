#pragma once

#include <stdint.h>

#define W 5
// The maximun number of branches going out of a CNode. Must match the bitmap size.
#define MAX_BRANCHES (1 << W)

typedef struct main_node_t main_node_t;

typedef enum 
{
    CNODE,
    SNODE,
    INODE,
    TNODE,
    LNODE,
    FNODE,
    RDCSS_DESC
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
    uint8_t marked;
} lnode_t;

typedef struct
{
    main_node_t* main;
    uint8_t marked;
    int gen;
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
    uint8_t marked;
} cnode_t;

typedef struct
{
    main_node_t* prev;
} fnode_t;

typedef struct
{
    inode_t* old_inode;
    main_node_t* expected_main_node;
    inode_t* new_inode;
    int committed;
} rdcss_desc_t;

struct main_node_t 
{
    node_type_t type;
    union
    {
        cnode_t cnode;
        tnode_t tnode;
        lnode_t lnode;
        fnode_t fnode;
    } node;
    main_node_t * prev;
    int gen;
};

typedef struct
{
    node_type_t type;
    union
    {
        inode_t* inode;
        rdcss_desc_t* desc;
    } node;
} root_node_t;
