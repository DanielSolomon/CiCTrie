#pragma once

#define MAX_HAZARD_POINTERS (4)
#define MAX_LIST_HAZARD_POINTERS (2)
#define NUM_OF_HAZARD_POINTERS (MAX_HAZARD_POINTERS + MAX_LIST_HAZARD_POINTERS)
#define TOTAL_HAZARD_POINTERS(thread_args) (thread_args->num_of_threads * NUM_OF_HAZARD_POINTERS)
#define FREE_LIST_SIZE (2)
//#define FREE_LIST_SIZE (3 * MAX_HAZARD_POINTERS)
#define FENCE do {__sync_synchronize();} while(0)
#define PLACE_HP(thread_args, arg) place_hazard_pointer((thread_args)->hp_lists[(thread_args)->index], arg)
#define PLACE_LIST_HP(thread_args, arg) place_list_hazard_pointer((thread_args)->hp_lists[(thread_args)->index], arg)
#define PLACE_TMP_HP(thread_args, arg) PLACE_LIST_HP(thread_args, arg)

typedef struct {
    void* hazard_pointers[MAX_HAZARD_POINTERS];
    int next_hp;
    void* list_hazard_pointers[MAX_LIST_HAZARD_POINTERS];
    int next_list_hp;
} hp_list_t;

//TODO stack
typedef struct {
    void* free_list[FREE_LIST_SIZE];
    int length;
} free_list_t;

typedef struct {
    hp_list_t** hp_lists;
    free_list_t* free_list;
    int index;
    int num_of_threads;
} thread_args_t;

void place_hazard_pointer(hp_list_t* hp_list, void* arg);
void place_list_hazard_pointer(hp_list_t* hp_list, void* arg);
void release_hazard_pointers(hp_list_t* hp_list);
void add_to_free_list(thread_args_t* thread_args, void* arg);
