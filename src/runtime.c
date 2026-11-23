#include "runtime.h"

#ifndef OPENDWS_HOST_TEST

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0u) {
        return dst;
    }
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t v = (uint8_t)c;
    while (n--) {
        *d++ = v;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (n--) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0u;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n--) {
        uint8_t ca = (uint8_t)*a++;
        uint8_t cb = (uint8_t)*b++;
        if (ca != cb) {
            return (int)ca - (int)cb;
        }
        if (ca == '\0') {
            return 0;
        }
    }
    return 0;
}

/* ARM EABI runtime helpers emitted by clang/gcc on Cortex-M */
void __aeabi_memcpy(void *dst, const void *src, size_t n) {
    (void)memcpy(dst, src, n);
}

void __aeabi_memcpy4(void *dst, const void *src, size_t n) {
    (void)memcpy(dst, src, n);
}

void __aeabi_memcpy8(void *dst, const void *src, size_t n) {
    (void)memcpy(dst, src, n);
}

void __aeabi_memmove(void *dst, const void *src, size_t n) {
    (void)memmove(dst, src, n);
}

void __aeabi_memmove4(void *dst, const void *src, size_t n) {
    (void)memmove(dst, src, n);
}

void __aeabi_memset(void *dst, size_t n, int c) {
    (void)memset(dst, c, n);
}

void __aeabi_memset4(void *dst, size_t n, int c) {
    (void)memset(dst, c, n);
}

void __aeabi_memclr(void *dst, size_t n) {
    (void)memset(dst, 0, n);
}

void __aeabi_memclr4(void *dst, size_t n) {
    (void)memset(dst, 0, n);
}

void __aeabi_memclr8(void *dst, size_t n) {
    (void)memset(dst, 0, n);
}

#endif /* OPENDWS_HOST_TEST */
