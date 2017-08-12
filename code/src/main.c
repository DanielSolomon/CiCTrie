#include "nodes.h"
#include "common.h"
#include "ctrie.h"

ctrie_t* ctrie = NULL;

void t1(thread_args_t* thread_args)
{

}

int main()
{
    PRINT("Start");

    hp_list_t hp_list = {.next_hp=0, .next_list_hp=0};
    hp_list_t* hp_array = &hp_list;
    free_list_t free_list = {.length=0};
    thread_args_t thread_arg = {
        .hp_lists = &hp_array,
        .free_list = &free_list,
        .index = 0,
        .num_of_threads = 1
    };

    ctrie = create_ctrie();
    if (ctrie == NULL)
    {
        FAIL("Failed to create ctrie");
    }
    ctrie->insert(ctrie, 1, 100, &thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, &thread_arg));
    ctrie->insert(ctrie, 2, 200, &thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, &thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, &thread_arg));
    ctrie->remove(ctrie, 2, &thread_arg);
    PRINT("key %d, val %d", 1, ctrie->lookup(ctrie, 1, &thread_arg));
    PRINT("key %d, val %d", 2, ctrie->lookup(ctrie, 2, &thread_arg));

CLEANUP:

    if (ctrie != NULL)
    {
        ctrie->free(ctrie);
    }

    int i=0;
    for (i = 0; i < free_list.length; i++)
    {
        free(free_list.free_list[i]);
    }

    return 0;
}
