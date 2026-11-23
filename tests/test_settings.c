#include "settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    opendws_settings_t s;
    settings_defaults(&s);
    assert(s.magic == SETTINGS_MAGIC);
    assert(s.version == SETTINGS_VERSION);
    assert(s.backlight_pct == 80u);
    assert(settings_validate(&s) == 1);

    /* Valid round-trip through raw buffer */
    s.backlight_pct = 45u;
    s.stream_enabled = 1u;
    s.stream_interval_ms = 250u;
    settings_finalize(&s);
    assert(settings_validate(&s) == 1);

    opendws_settings_t loaded;
    assert(settings_load_from_buffer(&s, sizeof(s), &loaded) == 1);
    assert(loaded.backlight_pct == 45u);
    assert(loaded.stream_enabled == 1u);
    assert(loaded.stream_interval_ms == 250u);

    /* Corrupted CRC must fall back to defaults */
    uint8_t raw[sizeof(s)];
    memcpy(raw, &s, sizeof(s));
    raw[6] ^= 0x55u;
    assert(settings_load_from_buffer(raw, sizeof(raw), &loaded) == 0);
    assert(loaded.backlight_pct == 80u);

    /* Foreign stock firmware data (0xFF / arbitrary bytes) must fall back to defaults */
    uint8_t stock_garbage[64];
    memset(stock_garbage, 0xFF, sizeof(stock_garbage));
    assert(settings_load_from_buffer(stock_garbage, sizeof(stock_garbage), &loaded) == 0);
    assert(settings_validate(&loaded) == 1);

    printf("test_settings: PASS\n");
    return 0;
}
