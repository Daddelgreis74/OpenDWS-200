#include "settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    opendws_settings_t s;
    settings_defaults(&s);
    assert(s.magic == SETTINGS_MAGIC);
    assert(s.version == SETTINGS_VERSION);
    assert(s.target_c == 300u);
    assert(s.presets_c[0] == 200u && s.presets_c[1] == 300u && s.presets_c[2] == 350u);
    assert(s.sleep_temp_c == 150u);
    assert(s.sleep_en == 1u);
    assert(s.backlight_pct == 80u);
    assert(settings_validate(&s) == 1);

    /* Valid round-trip through raw buffer & simulated Flash save/load */
    s.target_c = 365u;
    s.sleep_en = 0u;
    s.sleep_temp_c = 180u;
    s.cal_f245_deci_c = -30;
    s.backlight_pct = 45u;
    s.stream_enabled = 1u;
    s.stream_interval_ms = 250u;
    settings_finalize(&s);
    assert(settings_validate(&s) == 1);

    opendws_settings_t loaded;
    assert(settings_load_from_buffer(&s, sizeof(s), &loaded) == 1);
    assert(loaded.target_c == 365u);
    assert(loaded.sleep_en == 0u);
    assert(loaded.sleep_temp_c == 180u);
    assert(loaded.cal_f245_deci_c == -30);
    assert(loaded.backlight_pct == 45u);
    assert(loaded.stream_enabled == 1u);
    assert(loaded.stream_interval_ms == 250u);

    assert(settings_save_flash(&s) == 1);
    memset(&loaded, 0, sizeof(loaded));
    settings_load_safe(&loaded);
    assert(loaded.target_c == 365u);
    assert(loaded.cal_f245_deci_c == -30);

    /* Corrupted CRC must fall back to defaults */
    uint8_t raw[sizeof(s)];
    memcpy(raw, &s, sizeof(s));
    raw[6] ^= 0x55u;
    assert(settings_load_from_buffer(raw, sizeof(raw), &loaded) == 0);
    assert(loaded.target_c == 300u);
    assert(loaded.backlight_pct == 80u);

    /* Foreign stock firmware data (0xFF / arbitrary bytes) must fall back to defaults */
    uint8_t stock_garbage[200];
    memset(stock_garbage, 0xFF, sizeof(stock_garbage));
    assert(settings_load_from_buffer(stock_garbage, sizeof(stock_garbage), &loaded) == 0);
    assert(settings_validate(&loaded) == 1);

    printf("test_settings: PASS\n");
    return 0;
}
