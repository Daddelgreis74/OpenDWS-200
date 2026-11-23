#include "settings.h"

#include "app_config.h"
#include "runtime.h"

uint32_t settings_crc32(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0u; i < len; ++i) {
        crc ^= (uint32_t)p[i];
        for (uint32_t b = 0u; b < 8u; ++b) {
            uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

void settings_defaults(opendws_settings_t *out) {
    memset(out, 0, sizeof(*out));
    out->magic = SETTINGS_MAGIC;
    out->version = SETTINGS_VERSION;
    out->backlight_pct = 80u;
    out->buzzer_en = 1u;
    out->led_test_mask = 0u;
    out->stream_enabled = 0u;
    out->stream_interval_ms = STREAM_DEFAULT_MS;
    settings_finalize(out);
}

void settings_finalize(opendws_settings_t *s) {
    s->magic = SETTINGS_MAGIC;
    s->version = SETTINGS_VERSION;
    if (s->backlight_pct < 10u) {
        s->backlight_pct = 10u;
    } else if (s->backlight_pct > 100u) {
        s->backlight_pct = 100u;
    }
    if (s->stream_interval_ms < STREAM_MIN_MS) {
        s->stream_interval_ms = STREAM_MIN_MS;
    } else if (s->stream_interval_ms > STREAM_MAX_MS) {
        s->stream_interval_ms = STREAM_MAX_MS;
    }
    s->crc32 = settings_crc32(s, offsetof(opendws_settings_t, crc32));
}

int settings_validate(const opendws_settings_t *s) {
    if (s == NULL) {
        return 0;
    }
    if (s->magic != SETTINGS_MAGIC || s->version != SETTINGS_VERSION) {
        return 0;
    }
    if (s->backlight_pct < 10u || s->backlight_pct > 100u) {
        return 0;
    }
    if (s->stream_interval_ms < STREAM_MIN_MS || s->stream_interval_ms > STREAM_MAX_MS) {
        return 0;
    }
    uint32_t expected = settings_crc32(s, offsetof(opendws_settings_t, crc32));
    return (s->crc32 == expected) ? 1 : 0;
}

int settings_load_from_buffer(const void *buf, size_t len, opendws_settings_t *out) {
    if (buf != NULL && len >= sizeof(opendws_settings_t)) {
        opendws_settings_t tmp;
        memcpy(&tmp, buf, sizeof(tmp));
        if (settings_validate(&tmp)) {
            *out = tmp;
            return 1;
        }
    }
    settings_defaults(out);
    return 0;
}

void settings_load_safe(opendws_settings_t *out) {
#ifndef OPENDWS_HOST_TEST
    /* Phase 1: Read-only check on Flash page 0x0803F800; never erases or writes stock settings. */
    const void *flash_ptr = (const void *)(uintptr_t)SETTINGS_FLASH_ADDR;
    (void)settings_load_from_buffer(flash_ptr, sizeof(opendws_settings_t), out);
#else
    settings_defaults(out);
#endif
}
