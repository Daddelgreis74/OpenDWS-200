#pragma once

#include <stddef.h>
#include <stdint.h>

size_t fmt_append_str(char *buf, size_t pos, size_t cap, const char *s);
size_t fmt_uint_pad(char *buf, size_t cap, uint32_t val, uint8_t width, char pad);
size_t fmt_int_pad(char *buf, size_t cap, int32_t val, uint8_t width, char pad, uint8_t always_sign);
size_t fmt_hex8(char *buf, size_t cap, uint8_t val);
