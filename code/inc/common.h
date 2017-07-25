#include <stdio.h>
#include <stdlib.h>

#define MESSAGE_SIZE (4096)

#define PRINT(fmt, ...) do {                                                    \
    char __BUFFER[MESSAGE_SIZE + 1] = {0};                                      \
    snprintf(__BUFFER, MESSAGE_SIZE, "[%s: %d] %s.\n", __FILE__, __LINE__, fmt);\
    printf(__BUFFER, ##__VA_ARGS__);                                            \
} while (0)   

#define FAIL(fmt, ...) do {     \
    PRINT(fmt, ##__VA_ARGS__);  \
    goto CLEANUP;               \
} while (0)   

#define MALLOC(var, type) do {                              \
    var = malloc(sizeof(type));                             \
    if (var == NULL)                                        \
    {                                                       \
        FAIL("failed allocating %d bytes", sizeof(type));   \
    }                                                       \
} while (0);
    
