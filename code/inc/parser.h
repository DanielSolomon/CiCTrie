#pragma once

typedef enum 
{
    LOOKUP,
    INSERT,
    REMOVE,
} action_type_t;

typedef struct 
{
    int key;
} lookup_t;

typedef struct 
{
    int      n;
    lookup_t lookups[];
} lookups_t;

typedef struct 
{
    int key;
    int value;
} insert_t;

typedef struct 
{
    int      n;
    insert_t inserts[];
} inserts_t;

typedef struct 
{
    int key;
} remove_t;

typedef struct 
{
    int      n;
    remove_t removes[];
} removes_t;

typedef struct 
{
    action_type_t type;
    union 
    {
        lookup_t lookup;
        insert_t insert;
        remove_t remove;
    } action;
} action_t;

typedef struct 
{
    int      n;
    action_t actions[];
} actions_t;
