#pragma once

#include <stddef.h>
#include <stdint.h>

#define SETTINGS_MAGIC   0x5357444Fu /* "ODWS" */
#define SETTINGS_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t backlight_pct;
    uint8_t buzzer_en;
    uint8_t led_test_mask;
    uint8_t stream_enabled;
    uint16_t stream_interval_ms;
    uint32_t crc32;
} opendws_settings_t;

uint32_t settings_crc32(const void *data, size_t len);
void settings_defaults(opendws_settings_t *out);
void settings_finalize(opendws_settings_t *s);
int settings_validate(const opendws_settings_t *s);
int settings_load_from_buffer(const void *buf, size_t len, opendws_settings_t *out);
void settings_load_safe(opendws_settings_t *out);
