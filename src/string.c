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

/* Minimal snprintf: supports %%, %c, %s, %d, %u, %x (no width/precision). */
int snprintf(char *str, size_t size, const char *format, ...) {
    if (!str || size == 0 || !format) {
        return 0;
    }

    /* We use a simple va_list walk.  To avoid pulling in <stdarg.h> from the
     * host, we rely on the compiler built-in — GCC/Clang provide it even in
     * freestanding mode. */
    __builtin_va_list ap;
    __builtin_va_start(ap, format);

    size_t out = 0;         /* bytes written (excluding NUL) */
    size_t limit = size - 1; /* max bytes before the NUL */

#define PUTC(c) do { if (out < limit) str[out] = (char)(c); out++; } while (0)

    while (*format) {
        if (*format != '%') {
            PUTC(*format++);
            continue;
        }
        format++; /* skip '%' */

        switch (*format++) {
            case '%':
                PUTC('%');
                break;

            case 'c': {
                int c = __builtin_va_arg(ap, int);
                PUTC((char)c);
                break;
            }

            case 's': {
                const char *s = __builtin_va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) { PUTC(*s++); }
                break;
            }

            case 'd': {
                int val = __builtin_va_arg(ap, int);
                char tmp[12];
                int neg = 0;
                unsigned int uval;
                if (val < 0) { neg = 1; uval = (unsigned int)(-(val + 1)) + 1u; }
                else          { uval = (unsigned int)val; }
                int ti = 0;
                if (uval == 0) { tmp[ti++] = '0'; }
                else { while (uval) { tmp[ti++] = (char)('0' + uval % 10); uval /= 10; } }
                if (neg) { tmp[ti++] = '-'; }
                while (ti-- > 0) { PUTC(tmp[ti]); } /* reversed */ \
                /* restore ti after loop */ \
                break;
            }

            case 'u': {
                unsigned int uval = __builtin_va_arg(ap, unsigned int);
                char tmp[11];
                int ti = 0;
                if (uval == 0) { tmp[ti++] = '0'; }
                else { while (uval) { tmp[ti++] = (char)('0' + uval % 10); uval /= 10; } }
                while (ti-- > 0) { PUTC(tmp[ti]); }
                break;
            }

            case 'x': {
                unsigned int uval = __builtin_va_arg(ap, unsigned int);
                static const char hx[] = "0123456789abcdef";
                char tmp[9];
                int ti = 0;
                if (uval == 0) { tmp[ti++] = '0'; }
                else { while (uval) { tmp[ti++] = hx[uval & 0xF]; uval >>= 4; } }
                while (ti-- > 0) { PUTC(tmp[ti]); }
                break;
            }

            default:
                /* Unknown specifier — emit literally */
                PUTC(*(format - 1));
                break;
        }
    }

#undef PUTC

    str[out < size ? out : size - 1] = '\0';
    __builtin_va_end(ap);
    return (int)out;
}

