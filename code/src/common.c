#include "common.h"

void free_them_all(int count, ...)
{
    va_list args;
    va_start(args, count);
    int i = 0;
    void* ptr = NULL;
    for (i = 0; i < count; i++)
    {
        ptr = va_arg(args, void*);
        if (ptr != NULL)
        {
            free(ptr);
        }
    }
    va_end(args);
}
