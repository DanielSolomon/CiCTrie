#pragma once

typedef struct {
    int key;
} lookup_t;

typedef struct {
    int      n;
    lookup_t lookups[];
} lookups_t;

typedef struct {
    int key;
    int value;
} insert_t;

typedef struct {
    int      n;
    insert_t inserts[];
} inserts_t;

typedef struct {
    int key;
} remove_t;

typedef struct {
    int      n;
    remove_t removes[];
} removes_t;

