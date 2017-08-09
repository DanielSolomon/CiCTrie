#include <hazard_pointer.h>
#include "hazard_pointer.h"

void place_hazard_pointer(hp_list_t* hp_list, void* arg)
{
    hp_list->hazard_pointers[hp_list->next_hp] = arg;
    hp_list->next_hp++;
    if (hp_list->next_hp == MAX_HAZARD_POINTERS)
    {
        hp_list->next_hp = 0;
    }
    FENCE;
}

void place_list_hazard_pointer(hp_list_t* hp_list, void* arg)
{
    hp_list->list_hazard_pointers[hp_list->next_list_hp] = arg;
    hp_list->next_list_hp++;
    if (hp_list->next_list_hp == MAX_LIST_HAZARD_POINTERS)
    {
        hp_list->next_list_hp = 0;
    }
    FENCE;
}

void add_to_free_list(thread_args_t* thread_args, void* arg)
{
    // TODO push
    free_list_t* free_list = thread_args->free_list;
    free_list->free_list[free_list->length] = arg;
    free_list->length++;

    if (free_list->length == FREE_LIST_SIZE)
    {
        // REAL FREE THEM ALL
    }
}
