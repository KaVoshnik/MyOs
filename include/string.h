#ifndef _MYOS_STRING_H
#define _MYOS_STRING_H

#include <stddef.h>

size_t strlen(const char *str);
void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);

#endif /* _MYOS_STRING_H */

