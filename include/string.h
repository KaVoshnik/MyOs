#ifndef _MYOS_STRING_H
#define _MYOS_STRING_H

#include <stddef.h>

size_t strlen(const char *str);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
const char *strstr(const char *haystack, const char *needle);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
const char *strchr(const char *str, int ch);
const char *strrchr(const char *str, int ch);
void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *ptr1, const void *ptr2, size_t count);
int snprintf(char *str, size_t size, const char *format, ...);

#endif /* _MYOS_STRING_H */

