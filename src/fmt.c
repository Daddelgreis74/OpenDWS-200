#include "fmt.h"

size_t fmt_append_str(char *buf, size_t pos, size_t cap, const char *s) {
    if (buf == NULL || cap == 0u || s == NULL) {
        return pos;
    }
    while (*s != '\0' && pos + 1u < cap) {
        buf[pos++] = *s++;
    }
    buf[pos] = '\0';
    return pos;
}

size_t fmt_uint_pad(char *buf, size_t cap, uint32_t val, uint8_t width, char pad) {
    char tmp[12];
    size_t n = 0u;
    do {
        tmp[n++] = (char)('0' + (val % 10u));
        val /= 10u;
    } while (val > 0u && n < sizeof(tmp));

    size_t out = 0u;
    while (n + out < width && out + 1u < cap) {
        buf[out++] = pad;
    }
    while (n > 0u && out + 1u < cap) {
        buf[out++] = tmp[--n];
    }
    if (cap > 0u) {
        buf[out] = '\0';
    }
    return out;
}

size_t fmt_int_pad(char *buf, size_t cap, int32_t val, uint8_t width, char pad, uint8_t always_sign) {
    if (cap == 0u) return 0u;
    char tmp[12];
    uint32_t u = (val < 0) ? (0u - (uint32_t)val) : (uint32_t)val;
    char sign = (val < 0) ? '-' : (always_sign ? '+' : '\0');

    size_t n = 0u;
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u > 0u && n < sizeof(tmp));
    if (sign != '\0' && n < sizeof(tmp)) {
        tmp[n++] = sign;
    }

    size_t out = 0u;
    while (n + out < width && out + 1u < cap) {
        buf[out++] = pad;
    }
    while (n > 0u && out + 1u < cap) {
        buf[out++] = tmp[--n];
    }
    buf[out] = '\0';
    return out;
}

size_t fmt_hex8(char *buf, size_t cap, uint8_t val) {
    static const char hex[] = "0123456789ABCDEF";
    if (cap < 3u) {
        if (cap > 0u) buf[0] = '\0';
        return 0u;
    }
    buf[0] = hex[(val >> 4) & 0xFu];
    buf[1] = hex[val & 0xFu];
    buf[2] = '\0';
    return 2u;
}
