#pragma once

#include <stddef.h>
#include <stdint.h>

#define SETTINGS_MAGIC   0x5357444Fu /* "ODWS" */
#define SETTINGS_VERSION 2u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t target_c;
    uint16_t presets_c[3];
    uint16_t sleep_temp_c;
    int16_t  cal_f245_deci_c;
    int16_t  cal_f210_deci_c;
    uint16_t stream_interval_ms;
    uint8_t  sleep_en;
    uint8_t  backlight_pct;
    uint8_t  buzzer_en;
    uint8_t  led_test_mask;
    uint8_t  stream_enabled;
    uint8_t  reserved[3];
    uint32_t crc32;
} opendws_settings_t;

uint32_t settings_crc32(const void *data, size_t len);
void settings_defaults(opendws_settings_t *out);
void settings_finalize(opendws_settings_t *s);
int settings_validate(const opendws_settings_t *s);
int settings_load_from_buffer(const void *buf, size_t len, opendws_settings_t *out);
void settings_load_safe(opendws_settings_t *out);
int settings_save_flash(const opendws_settings_t *s);
