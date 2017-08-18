#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "hazard_pointer.h"
#include "nodes.h"
#include "common.h"
#include "ctrie.h"
#include "parser.h"

ctrie_t* ctrie = NULL;

typedef struct {
    thread_args_t*  thread_arg;
    inserts_t*      inserts;
    int             offset;
    int             size;
} insert_thread_arg_t;

void insert_test_thread(insert_thread_arg_t* insert_thread_arg)
{
    int i;
    int size    = insert_thread_arg->size;
    int offset  = insert_thread_arg->offset;
    for (i = 0; i < size; i++)
    {
        insert_t insert = insert_thread_arg->inserts->inserts[offset + i];
        ctrie->insert(ctrie, insert.key, insert.value, insert_thread_arg->thread_arg);
        PRINT("inserted %d key=%d", i, insert.key);
    }
    PRINT("out of for");
    release_hazard_pointers(insert_thread_arg->thread_arg->hp_lists[insert_thread_arg->thread_arg->index]);
    PRINT("after release");
}

int main(int argc, char* argv[])
{
    int i       = 0;
    FILE* fp    = NULL;
    char* data  = NULL;

    if (argc != 2)
    {
        PRINT("Usage: %s <action_file>", argv[0]);
        return -1;
    }

    PRINT("Start");
    PRINT("Setting up %d threads", NUM_OF_THREADS);

    hp_list_t*  hp_array[NUM_OF_THREADS]    = {0};
    hp_list_t   hp_lists[NUM_OF_THREADS]    = {0};

    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        hp_array[i] = &(hp_lists[i]);
    }

    free_list_t free_lists[NUM_OF_THREADS]      = {0};
    thread_args_t threads_args[NUM_OF_THREADS]  = {0};
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        threads_args[i] = (thread_args_t) {
            .hp_lists   = hp_array,
            .free_list  = &(free_lists[i]),
            .index      = i,
            .num_of_threads = NUM_OF_THREADS,
        };
    }

    ctrie = create_ctrie();
    if (ctrie == NULL)
    {
        FAIL("Failed to create ctrie");
    }

    fp = fopen(argv[1], "r");
    if (fp == NULL)
    {
        FAIL("Failed to fopen %s", argv[1]);
    }
    struct stat file_status = {0};
    if (fstat(fileno(fp), &file_status) < 0)
    {
        FAIL("Failed to stat file: %s (%d)", argv[1], errno);
    }
    PRINT("File size is: %d", file_status.st_size);
    data = malloc(file_status.st_size);
    if (data == NULL)
    {
        FAIL("Failed to allocate %d bytes for data", file_status.st_size);
    }
    if (fread(data, file_status.st_size, 1, fp) != 1)
    {
        FAIL("Failed to read %d bytes from fp", file_status.st_size);
    }

    inserts_t* inserts = (inserts_t*) data;

    insert_thread_arg_t insert_threads_args[NUM_OF_THREADS] = {0};

    int total_actions = inserts->n;
    int size = total_actions / NUM_OF_THREADS;

    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        insert_threads_args[i] = (insert_thread_arg_t) {
            .thread_arg = &(threads_args[i]),
            .inserts    = inserts,
            .offset     = i * size,
            .size       = size,
        };
    }

    pthread_t tids[NUM_OF_THREADS];
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_create(&(tids[i]), NULL, (void*(*)(void*))insert_test_thread, &(insert_threads_args[i]));
    }
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_join(tids[i], NULL);
    }

CLEANUP:

    if (ctrie != NULL)
    {
        ctrie->free(ctrie);
    }

    if (fp != NULL)
    {
        fclose(fp);
    }

    if (data != NULL)
    {
        free(data);
    }

    int j = 0;
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        for (j = 0; j < threads_args[i].free_list->length; j++)
        {
            free(threads_args[i].free_list->free_list[j]);
        }
    }

    return 0;
}
