#include "common.h"

#define NUM_OF_BITS_IN_BYTE (8)

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

int32_t highest_on_bit(uint32_t num)
{
    int32_t i;
    for (i = NUM_OF_BITS_IN_BYTE * sizeof(uint32_t) - 1; i >= 0; i--)
    {
        if (num & (1 << i))
        {
            return i;
        }
    }
    return -1;
}
