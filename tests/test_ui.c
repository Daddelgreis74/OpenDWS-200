#include "../tools/host_display.h"
#include "input.h"
#include "sensors.h"
#include "ui.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    display_init();

    sensor_state_t sst;
    sensors_state_init(&sst);
    for (uint16_t i = 0u; i < 32u; ++i) {
        sensors_update_snapshot(&sst, (uint16_t)(200u + i * 10u), 2048u, 1500u, NULL);
    }

    input_state_t inp = {
        .held_mask = KEY_BIT_PD2 | KEY_BIT_PB4,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 12u
    };

    ui_status_t st = {
        .usb_connected = 1u,
        .stream_enabled = 1u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0x3Cu,
        .uptime_ms = 65000u
    };

    ui_draw_static();
    ui_draw_dynamic(&sst.snap, &inp, &st);

    uint32_t non_black = 0u;
    uint32_t red_badge_pixels = 0u;
    for (uint16_t y = 0u; y < LCD_HEIGHT; ++y) {
        for (uint16_t x = 0u; x < LCD_WIDTH; ++x) {
            uint16_t px = host_display_pixel(x, y);
            if (px != 0u) non_black++;
            if (y >= 4u && y < 22u && x >= 78u && x < 174u) {
                uint8_t r = (uint8_t)((px >> 11) & 0x1Fu);
                uint8_t b = (uint8_t)(px & 0x1Fu);
                if (r > b + 4u) red_badge_pixels++;
            }
        }
    }

    assert(non_black == (uint32_t)LCD_WIDTH * (uint32_t)LCD_HEIGHT);
    assert(red_badge_pixels > 500u);

    printf("test_ui: PASS\n");
    return 0;
}
