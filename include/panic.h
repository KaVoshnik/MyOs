#ifndef _MYOS_PANIC_H
#define _MYOS_PANIC_H

#include <stddef.h>

/* Печатает сообщение об ошибке и останавливает систему. */
__attribute__((noreturn)) void panic(const char *message, const char *file, int line);

/* Удобные макросы для стабильности/инвариантов. */
#define PANIC(msg) panic((msg), __FILE__, __LINE__)

#define ASSERT(cond)                          \
    do {                                     \
        if (!(cond)) {                       \
            panic("ASSERT(" #cond ") failed", __FILE__, __LINE__); \
        }                                    \
    } while (0)

#endif /* _MYOS_PANIC_H */

