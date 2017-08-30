#pragma once

#define MAX_HAZARD_POINTERS                 (4)
#define TOTAL_HAZARD_POINTERS(thread_args)  (thread_args->num_of_threads * MAX_HAZARD_POINTERS)
#define FREE_LIST_SIZE                      (NUM_OF_THREADS * MAX_HAZARD_POINTERS)
#define FENCE                               do {__sync_synchronize();} while(0)
#define PLACE_HP(thread_args, arg, pair)    place_hazard_pointer((thread_args)->hp_lists[(thread_args)->index], arg, pair)

typedef struct {
    void*   hazard_pointers[MAX_HAZARD_POINTERS];
    int     next_hp;
} hp_list_t;

typedef struct {
    void*   free_list[FREE_LIST_SIZE];
    int     length;
} free_list_t;

typedef struct {
    hp_list_t**     hp_lists;
    free_list_t*    free_list;
    int             index;
    int             num_of_threads;
} thread_args_t;

void place_hazard_pointer(hp_list_t* hp_list, void* arg, int pair);
void release_hazard_pointers(hp_list_t* hp_list);
void add_to_free_list(thread_args_t* thread_args, void* arg);
