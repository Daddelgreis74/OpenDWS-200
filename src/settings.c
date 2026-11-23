#include "settings.h"

#include "app_config.h"
#include "board.h"
#include "hw.h"
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

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int16_t clamp_i16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void settings_defaults(opendws_settings_t *out) {
    memset(out, 0, sizeof(*out));
    out->magic = SETTINGS_MAGIC;
    out->version = SETTINGS_VERSION;
    out->target_c = TEMP_DEFAULT_C;
    out->presets_c[0] = 200u;
    out->presets_c[1] = 300u;
    out->presets_c[2] = 350u;
    out->sleep_temp_c = SLEEP_TEMP_DEFAULT_C;
    out->cal_f245_deci_c = 0;
    out->cal_f210_deci_c = 0;
    out->stream_interval_ms = STREAM_DEFAULT_MS;
    out->sleep_en = 1u;
    out->backlight_pct = 80u;
    out->buzzer_en = 1u;
    out->led_test_mask = 0u;
    out->stream_enabled = 0u;
    out->standby_screen_en = 1u;
    out->auto_off_min = AUTO_OFF_DEFAULT_MIN;
    settings_finalize(out);
}

void settings_finalize(opendws_settings_t *s) {
    s->magic = SETTINGS_MAGIC;
    s->version = SETTINGS_VERSION;
    s->target_c = clamp_u16(s->target_c, (uint16_t)TEMP_MIN_C, (uint16_t)TEMP_MAX_C);
    for (uint32_t i = 0u; i < 3u; ++i) {
        s->presets_c[i] = clamp_u16(s->presets_c[i], (uint16_t)TEMP_MIN_C, (uint16_t)TEMP_MAX_C);
    }
    if (s->sleep_temp_c != 0u) {
        s->sleep_temp_c = clamp_u16(s->sleep_temp_c, (uint16_t)SLEEP_TEMP_MIN_C, (uint16_t)SLEEP_TEMP_MAX_C);
    }
    s->cal_f245_deci_c = clamp_i16(s->cal_f245_deci_c, (int16_t)(CAL_OFFSET_MIN_C * 10), (int16_t)(CAL_OFFSET_MAX_C * 10));
    s->cal_f210_deci_c = clamp_i16(s->cal_f210_deci_c, (int16_t)(CAL_OFFSET_MIN_C * 10), (int16_t)(CAL_OFFSET_MAX_C * 10));
    s->stream_interval_ms = clamp_u16(s->stream_interval_ms, (uint16_t)STREAM_MIN_MS, (uint16_t)STREAM_MAX_MS);
    s->sleep_en = s->sleep_en ? 1u : 0u;
    if (s->backlight_pct < 10u) {
        s->backlight_pct = 10u;
    } else if (s->backlight_pct > 100u) {
        s->backlight_pct = 100u;
    }
    s->buzzer_en = s->buzzer_en ? 1u : 0u;
    s->stream_enabled = s->stream_enabled ? 1u : 0u;
    s->standby_screen_en = s->standby_screen_en ? 1u : 0u;
    if (s->auto_off_min > AUTO_OFF_MAX_MIN) {
        s->auto_off_min = AUTO_OFF_MAX_MIN;
    }
    s->reserved[0] = 0u;
    s->crc32 = settings_crc32(s, offsetof(opendws_settings_t, crc32));
}

int settings_validate(const opendws_settings_t *s) {
    if (s == NULL) {
        return 0;
    }
    if (s->magic != SETTINGS_MAGIC || s->version != SETTINGS_VERSION) {
        return 0;
    }
    if (s->target_c < TEMP_MIN_C || s->target_c > TEMP_MAX_C) {
        return 0;
    }
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (s->presets_c[i] < TEMP_MIN_C || s->presets_c[i] > TEMP_MAX_C) {
            return 0;
        }
    }
    if (s->sleep_temp_c != 0u &&
        (s->sleep_temp_c < SLEEP_TEMP_MIN_C || s->sleep_temp_c > SLEEP_TEMP_MAX_C)) {
        return 0;
    }
    if (s->cal_f245_deci_c < (int16_t)(CAL_OFFSET_MIN_C * 10) ||
        s->cal_f245_deci_c > (int16_t)(CAL_OFFSET_MAX_C * 10)) {
        return 0;
    }
    if (s->cal_f210_deci_c < (int16_t)(CAL_OFFSET_MIN_C * 10) ||
        s->cal_f210_deci_c > (int16_t)(CAL_OFFSET_MAX_C * 10)) {
        return 0;
    }
    if (s->backlight_pct < 10u || s->backlight_pct > 100u) {
        return 0;
    }
    if (s->stream_interval_ms < STREAM_MIN_MS || s->stream_interval_ms > STREAM_MAX_MS) {
        return 0;
    }
    if (s->auto_off_min > AUTO_OFF_MAX_MIN) {
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

#ifndef OPENDWS_HOST_TEST

void settings_load_safe(opendws_settings_t *out) {
    const void *flash_ptr = (const void *)(uintptr_t)SETTINGS_FLASH_ADDR;
    (void)settings_load_from_buffer(flash_ptr, sizeof(opendws_settings_t), out);
}

int settings_matches_flash(const opendws_settings_t *s) {
    if (s == NULL) {
        return 0;
    }
    opendws_settings_t copy;
    memcpy(&copy, s, sizeof(copy));
    settings_finalize(&copy);
    return (memcmp((const void *)(uintptr_t)SETTINGS_FLASH_ADDR, &copy, sizeof(copy)) == 0) ? 1 : 0;
}

int settings_save_flash(const opendws_settings_t *s) {
    if (s == NULL) {
        return 0;
    }

    opendws_settings_t copy;
    memcpy(&copy, s, sizeof(copy));
    settings_finalize(&copy);

    uint16_t hw[sizeof(opendws_settings_t) / sizeof(uint16_t)];
    memcpy(hw, &copy, sizeof(copy));

    /* Safety critical: pause TMR1/TMR2 and force PA2 LOW before Flash erase/program */
    board_timers_pause();
    board_wdt_feed();

    for (uint32_t t = 0u; t < 200000u && (FLASH_STS & FLASH_STS_OBF) != 0u; ++t) {
    }

    FLASH_UNLOCK = FLASH_KEY1;
    FLASH_UNLOCK = FLASH_KEY2;
    FLASH_STS = FLASH_STS_PRGMERR | FLASH_STS_EPPERR | FLASH_STS_ODF;

    /* Erase 2 KB sector at 0x0803F800 */
    FLASH_CTRL = FLASH_CTRL_SECERS;
    FLASH_ADDR = SETTINGS_FLASH_ADDR;
    FLASH_CTRL = FLASH_CTRL_SECERS | FLASH_CTRL_ERSTR;
    for (uint32_t t = 0u; t < 500000u && (FLASH_STS & FLASH_STS_OBF) != 0u; ++t) {
    }
    int ok = ((FLASH_STS & (FLASH_STS_PRGMERR | FLASH_STS_EPPERR)) == 0u) ? 1 : 0;
    FLASH_STS = FLASH_STS_PRGMERR | FLASH_STS_EPPERR | FLASH_STS_ODF;
    FLASH_CTRL = 0u;

    board_wdt_feed();

    /* Program halfwords */
    if (ok != 0) {
        size_t hw_count = sizeof(copy) / sizeof(uint16_t);
        for (size_t i = 0u; i < hw_count; ++i) {
            FLASH_CTRL = FLASH_CTRL_FPRGM;
            *(volatile uint16_t *)(uintptr_t)(SETTINGS_FLASH_ADDR + (uint32_t)(i * 2u)) = hw[i];
            for (uint32_t t = 0u; t < 200000u && (FLASH_STS & FLASH_STS_OBF) != 0u; ++t) {
            }
            if ((FLASH_STS & (FLASH_STS_PRGMERR | FLASH_STS_EPPERR)) != 0u) {
                ok = 0;
                FLASH_STS = FLASH_STS_PRGMERR | FLASH_STS_EPPERR | FLASH_STS_ODF;
                break;
            }
            FLASH_STS = FLASH_STS_ODF;
        }
        FLASH_CTRL = 0u;
    }

    FLASH_CTRL = FLASH_CTRL_OPLK;

    if (ok != 0 && memcmp((const void *)(uintptr_t)SETTINGS_FLASH_ADDR, &copy, sizeof(copy)) != 0) {
        ok = 0;
    }

    board_wdt_feed();
    board_timers_resume();
    return ok;
}

#else

static opendws_settings_t g_host_flash_sim;
static uint8_t g_host_flash_valid = 0u;

void settings_load_safe(opendws_settings_t *out) {
    if (g_host_flash_valid != 0u &&
        settings_load_from_buffer(&g_host_flash_sim, sizeof(g_host_flash_sim), out) != 0) {
        return;
    }
    settings_defaults(out);
}

int settings_matches_flash(const opendws_settings_t *s) {
    if (s == NULL || g_host_flash_valid == 0u) {
        return 0;
    }
    opendws_settings_t copy;
    memcpy(&copy, s, sizeof(copy));
    settings_finalize(&copy);
    return (memcmp(&g_host_flash_sim, &copy, sizeof(copy)) == 0) ? 1 : 0;
}

int settings_save_flash(const opendws_settings_t *s) {
    if (s == NULL) {
        return 0;
    }
    memcpy(&g_host_flash_sim, s, sizeof(g_host_flash_sim));
    settings_finalize(&g_host_flash_sim);
    g_host_flash_valid = 1u;
    return 1;
}

#endif /* OPENDWS_HOST_TEST */
