#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define MESSAGE_SIZE (4096)

#define PRINT(fmt, ...) do {                                                                    \
    char __BUFFER[MESSAGE_SIZE + 1] = {0};                                                      \
    snprintf(__BUFFER, MESSAGE_SIZE, "[%d] [%s: %d] %s.\n", (int)syscall(SYS_gettid), __FILE__, __LINE__, fmt); \
    printf(__BUFFER, ##__VA_ARGS__);                                                            \
    fflush(stdout);\
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
    memset(var, 0, sizeof(type));                           \
} while (0);

void    free_them_all(int count, ...);
int32_t highest_on_bit(uint32_t num);

