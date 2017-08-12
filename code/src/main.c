#include <pthread.h>
#include "nodes.h"
#include "common.h"
#include "ctrie.h"

ctrie_t* ctrie = NULL;

void t1(thread_args_t* thread_arg)
{
    ctrie->insert(ctrie, 1, 100, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    ctrie->insert(ctrie, 2, 200, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, thread_arg));
    ctrie->remove(ctrie, 2, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, thread_arg));
    release_hazard_pointers(thread_arg->hp_lists[thread_arg->index]);
}

void t2(thread_args_t* thread_arg)
{
    ctrie->insert(ctrie, 1, 300, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    ctrie->insert(ctrie, 2, 400, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, thread_arg));
    ctrie->remove(ctrie, 2, thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, thread_arg));
    release_hazard_pointers(thread_arg->hp_lists[thread_arg->index]);
}

int main()
{
    PRINT("Start");

    hp_list_t hp_list1 = {.next_hp=0, .next_list_hp=0};
    hp_list_t hp_list2 = {.next_hp=0, .next_list_hp=0};
    hp_list_t* hp_array[2] = {
        &hp_list1,
        &hp_list2
    };
    free_list_t free_list1 = {.length=0};
    thread_args_t thread1_arg = {
        .hp_lists = hp_array,
        .free_list = &free_list1,
        .index = 0,
        .num_of_threads = 2
    };
    free_list_t free_list2 = {.length=0};
    thread_args_t thread2_arg = {
        .hp_lists = hp_array,
        .free_list = &free_list2,
        .index = 1,
        .num_of_threads = 2
    };
    ctrie = create_ctrie();
    if (ctrie == NULL)
    {
        FAIL("Failed to create ctrie");
    }

    pthread_t tid1 = 0;
    pthread_t tid2 = 0; 
    pthread_create(&tid1, NULL, (void*(*)(void*))t1, &thread1_arg);
    pthread_create(&tid2, NULL, (void*(*)(void*))t2, &thread2_arg);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

CLEANUP:

    if (ctrie != NULL)
    {
        ctrie->free(ctrie);
    }

    int i=0;
    for (i = 0; i < free_list1.length; i++)
    {
        free(free_list1.free_list[i]);
    }
    for (i = 0; i < free_list2.length; i++)
    {
        free(free_list2.free_list[i]);
    }

    return 0;
}
