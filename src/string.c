#include <string.h>
#include <stddef.h>

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        ++len;
    }
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n-- > 0) {
        if (*a != *b || *a == '\0' || *b == '\0') {
            return (unsigned char)*a - (unsigned char)*b;
        }
        ++a;
        ++b;
    }
    return 0;
}

const char *strstr(const char *haystack, const char *needle) {
    if (!needle || *needle == '\0') {
        return haystack;
    }
    if (!haystack) {
        return NULL;
    }
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            ++h;
            ++n;
        }
        if (*n == '\0') {
            return haystack;
        }
        ++haystack;
    }
    return NULL;
}

void *memset(void *dest, int value, size_t count) {
    unsigned char *ptr = (unsigned char *)dest;
    while (count--) {
        *ptr++ = (unsigned char)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }

    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) {
        /* empty */
    }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        d[i] = src[i];
        ++i;
    }
    while (i < n) {
        d[i++] = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) {
        ++d;
    }
    while ((*d++ = *src++)) {
        /* empty */
    }
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) {
        ++d;
    }
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        d[i] = src[i];
        ++i;
    }
    d[i] = '\0';
    return dest;
}

const char *strchr(const char *str, int ch) {
    char c = (char)ch;
    while (*str) {
        if (*str == c) {
            return str;
        }
        ++str;
    }
    if (c == '\0') {
        return str;
    }
    return NULL;
}

const char *strrchr(const char *str, int ch) {
    char c = (char)ch;
    const char *last = NULL;
    while (*str) {
        if (*str == c) {
            last = str;
        }
        ++str;
    }
    if (c == '\0') {
        return str;
    }
    return last;
}

int memcmp(const void *ptr1, const void *ptr2, size_t count) {
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;
    while (count--) {
        if (*p1 != *p2) {
            return (int)(*p1 - *p2);
        }
        ++p1;
        ++p2;
    }
    return 0;
}

/* Simple snprintf implementation for basic cases */
int snprintf(char *str, size_t size, const char *format, ...) {
    (void)format; /* For now, just support simple cases */
    if (size == 0) {
        return 0;
    }
    str[0] = '\0';
    return 0;
}

