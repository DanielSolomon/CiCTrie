#include <stdlib.h>
#include "hazard_pointer.h"
#include "common.h"

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

static int compare(const void* left_pointer, const void* right_pointer)
{
    void* left = *((void**)left_pointer);
    void* right = *((void**)right_pointer);
    if (left < right)
        return -1;
    else if (left == right)
        return 0;
    else
        return 1;
}

/**
 * Gathers the hazard pointers from all the threads into one sorted array.
 * Returns this array if successful, otherwise returns NULL.
 */
static void** prepare_hazard_pointers(thread_args_t* thread_args)
{    
    void** hazard_pointers = NULL;
    int i = 0;
    int j = 0;
    hazard_pointers = malloc(TOTAL_HAZARD_POINTERS(thread_args) * sizeof(void*));
    if (hazard_pointers == NULL)
    {
        return NULL;
    }

    for (i = 0; i < thread_args->num_of_threads; i++)
    {
        int k = 0;
        for (k = 0; k < MAX_HAZARD_POINTERS; k++)
        {
            hazard_pointers[j] = thread_args->hp_lists[i]->hazard_pointers[k];
            j++;
        }
        for (k = 0; k < MAX_LIST_HAZARD_POINTERS; k++)
        {
            hazard_pointers[j] = thread_args->hp_lists[i]->list_hazard_pointers[k];
            j++;
        }
    }

    qsort(hazard_pointers, TOTAL_HAZARD_POINTERS(thread_args), sizeof(void*), compare);     
    return hazard_pointers;
}

/**
 * Scans through the free list and attempts to free the nodes.
 */
static void scan(thread_args_t* thread_args)
{
    void** hazard_pointers = NULL;
    void* failed_list[FREE_LIST_SIZE] = {};
    int failed_length = 0;
    int i = 0;

    hazard_pointers = prepare_hazard_pointers(thread_args);
    if (hazard_pointers == NULL)
    {
        goto CLEANUP;
    }

    while (thread_args->free_list->length != 0)
    {
        void* arg = thread_args->free_list->free_list[thread_args->free_list->length-1];
        thread_args->free_list->length--;
        
        if (NULL == bsearch(&arg, hazard_pointers, TOTAL_HAZARD_POINTERS(thread_args), sizeof(void*), compare))
        {
            PRINT("FREEING FREE LIST %p", arg);
            free(arg);
        }
        else
        {
            PRINT("Failed to free %p", arg);
            failed_list[failed_length] = arg;
            failed_length++;
        } 
    }

    for (i = 0; i < failed_length; i++)
    {
        thread_args->free_list->free_list[i] = failed_list[i];
    }
    thread_args->free_list->length = failed_length;
    
CLEANUP:
    if (hazard_pointers != NULL)
    {
        free(hazard_pointers);
    }
}

void add_to_free_list(thread_args_t* thread_args, void* arg)
{
    free_list_t* free_list = thread_args->free_list;

    // TODO is this ok?
    while (free_list->length == FREE_LIST_SIZE)
    {
        scan(thread_args);
    }
    free_list->free_list[free_list->length] = arg;
    free_list->length++;
}
