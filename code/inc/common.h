#include <stdio.h>

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
