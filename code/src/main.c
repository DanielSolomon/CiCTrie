#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

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

typedef struct {
    thread_args_t*  thread_arg;
    lookups_t*      lookups;
    int             offset;
    int             size;
} lookup_thread_arg_t;

typedef struct {
    thread_args_t*  thread_arg;
    removes_t*      removes;
    int             offset;
    int             size;
} remove_thread_arg_t;

int64_t get_time()
{
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1)
    {
        FAIL("Failed to get time");
    }
    return tp.tv_sec * 1000000000 + tp.tv_nsec;

CLEANUP:
    return -1;
}

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

void lookup_test_thread(lookup_thread_arg_t* lookup_thread_arg)
{
    int i;
    int size    = lookup_thread_arg->size;
    int offset  = lookup_thread_arg->offset;
    for (i = 0; i < size; i++)
    {
        lookup_t lookup = lookup_thread_arg->lookups->lookups[offset + i];
        int ret = ctrie->lookup(ctrie, lookup.key, lookup_thread_arg->thread_arg);
        PRINT("lookuped %d key=%d ret=%d", i, lookup.key, ret);
    }
    PRINT("out of for");
    release_hazard_pointers(lookup_thread_arg->thread_arg->hp_lists[lookup_thread_arg->thread_arg->index]);
    PRINT("after release");
}

void remove_test_thread(remove_thread_arg_t* remove_thread_arg)
{
    int i;
    int size    = remove_thread_arg->size;
    int offset  = remove_thread_arg->offset;
    for (i = 0; i < size; i++)
    {
        remove_t remove = remove_thread_arg->removes->removes[offset + i];
        ctrie->remove(ctrie, remove.key, remove_thread_arg->thread_arg);
        PRINT("removed %d key=%d", i, remove.key);
    }
    PRINT("out of for");
    release_hazard_pointers(remove_thread_arg->thread_arg->hp_lists[remove_thread_arg->thread_arg->index]);
    PRINT("after release");
}

int64_t insert_test(inserts_t* inserts, thread_args_t threads_args[])
{
    int i;
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

    int64_t start_time = get_time();
    if (start_time == -1)
    {
        return -1;
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
    int64_t end_time = get_time();

    int j = 0;
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        for (j = 0; j < threads_args[i].free_list->length; j++)
        {
            DEBUG("free %p", threads_args[i].free_list->free_list[j]);
            free(threads_args[i].free_list->free_list[j]);
        }
        threads_args[i].free_list->length = 0;
    }

    if (end_time == -1)
    {
        return -1;
    }
    return end_time - start_time;
}

int64_t lookup_test(lookups_t* lookups, thread_args_t threads_args[])
{
    int i;
    lookup_thread_arg_t lookup_threads_args[NUM_OF_THREADS] = {0};

    int total_actions = lookups->n;
    int size = total_actions / NUM_OF_THREADS;

    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        lookup_threads_args[i] = (lookup_thread_arg_t) {
            .thread_arg = &(threads_args[i]),
            .lookups    = lookups,
            .offset     = i * size,
            .size       = size,
        };
    }

    int64_t start_time = get_time();
    if (start_time == -1)
    {
        return -1;
    }
    pthread_t tids[NUM_OF_THREADS];
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_create(&(tids[i]), NULL, (void*(*)(void*))lookup_test_thread, &(lookup_threads_args[i]));
    }
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_join(tids[i], NULL);
    }
    int64_t end_time = get_time();

    int j = 0;
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        for (j = 0; j < threads_args[i].free_list->length; j++)
        {
            DEBUG("free %p", threads_args[i].free_list->free_list[j]);
            free(threads_args[i].free_list->free_list[j]);
        }
        threads_args[i].free_list->length = 0;
    }

    if (end_time == -1)
    {
        return -1;
    }
    return end_time - start_time;
}

int64_t remove_test(removes_t* removes, thread_args_t threads_args[])
{
    int i;
    remove_thread_arg_t remove_threads_args[NUM_OF_THREADS] = {0};

    int total_actions = removes->n;
    int size = total_actions / NUM_OF_THREADS;

    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        remove_threads_args[i] = (remove_thread_arg_t) {
            .thread_arg = &(threads_args[i]),
            .removes    = removes,
            .offset     = i * size,
            .size       = size,
        };
    }

    int64_t start_time = get_time();
    if (start_time == -1)
    {
        return -1;
    }
    pthread_t tids[NUM_OF_THREADS];
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_create(&(tids[i]), NULL, (void*(*)(void*))remove_test_thread, &(remove_threads_args[i]));
    }
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        pthread_join(tids[i], NULL);
    }
    int64_t end_time = get_time();

    int j = 0;
    for (i = 0; i < NUM_OF_THREADS; i++)
    {
        for (j = 0; j < threads_args[i].free_list->length; j++)
        {
            DEBUG("free %p", threads_args[i].free_list->free_list[j]);
            free(threads_args[i].free_list->free_list[j]);
        }
        threads_args[i].free_list->length = 0;
    }

    if (end_time == -1)
    {
        return -1;
    }
    return end_time - start_time;
}

char* read_file(const char* path)
{
    FILE* fp    = NULL;
    char* data  = NULL;

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        FAIL("Failed to fopen %s", path);
    }
    struct stat file_status = {0};
    if (fstat(fileno(fp), &file_status) < 0)
    {
        FAIL("Failed to stat file: %s (%d)", path, errno);
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

    fclose(fp);
    return data;

CLEANUP:
    
    if (fp != NULL)
    {
        fclose(fp);
    }
    if (data != NULL)
    {
        free(data);
    }
    return NULL;
}

void handle_insert(const char* path, thread_args_t threads_args[])
{
    char* data = NULL;
    data = read_file(path);
    if (data == NULL)
    {
        FAIL("Failed to read file");
    }
    inserts_t* inserts = (inserts_t*) data;
    int64_t time = insert_test(inserts, threads_args);
    printf("insert took %ld nsecs\n", time);

CLEANUP:
    if (data != NULL)
    {
        free(data);
    }
}

void handle_lookup(const char* path, thread_args_t threads_args[])
{
    char* data = NULL;
    data = read_file(path);
    if (data == NULL)
    {
        FAIL("Failed to read file");
    }
    lookups_t* lookups = (lookups_t*) data;
    int64_t time = lookup_test(lookups, threads_args);
    printf("lookup took %ld nsecs\n", time);

CLEANUP:
    if (data != NULL)
    {
        free(data);
    }
}

void handle_remove(const char* path, thread_args_t threads_args[])
{
    char* data = NULL;
    data = read_file(path);
    if (data == NULL)
    {
        FAIL("Failed to read file");
    }
    removes_t* removes = (removes_t*) data;
    int64_t time = remove_test(removes, threads_args);
    printf("remove took %ld nsecs\n", time);

CLEANUP:
    if (data != NULL)
    {
        free(data);
    }
}

int main(int argc, char* argv[])
{
    int i = 0;

    if ((argc & 1) == 0)
    {
        PRINT("Usage: %s [<insert|lookup|remove> <action_file>]*", argv[0]);
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

    for (i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "insert") == 0)
        {
            PRINT("Handle insert..");
            handle_insert(argv[i + 1], threads_args);
            PRINT("Handled insert");
        }
        else if (strcmp(argv[i], "lookup") == 0)
        {
            PRINT("Handle lookup..");
            handle_lookup(argv[i + 1], threads_args);
            PRINT("Handled lookup");
        }
        else if (strcmp(argv[i], "remove") == 0)
        {
            PRINT("Handle remove..");
            handle_remove(argv[i + 1], threads_args);
            PRINT("Handled remove");
        }
        else
        {
            FAIL("Unknown action: %s", argv[i]);
        }
    }

CLEANUP:

    if (ctrie != NULL)
    {
        ctrie->free(ctrie);
    }

    return 0;
}
