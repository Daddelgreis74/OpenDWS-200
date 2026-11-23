#include "../tools/host_display.h"
#include "input.h"
#include "sensors.h"
#include "ui.h"

#include <stdio.h>

#ifndef SCREEN_PREVIEW_DIR
#define SCREEN_PREVIEW_DIR "build/preview"
#endif

static void populate_sample_wave(sensor_state_t *st, uint16_t tip_base, uint16_t cur_base, uint16_t aux_base) {
    sensors_state_init(st);
    for (uint32_t i = 0u; i < 64u; ++i) {
        uint16_t tip = (uint16_t)(tip_base + ((i * 19u) % 240u));
        uint16_t cur = (uint16_t)(cur_base + ((i % 8u) * 4u) - 16u);
        uint16_t aux = (uint16_t)(aux_base + ((i % 5u) * 6u));
        sensors_update_snapshot(st, tip, cur, aux, NULL);
    }
}

int main(void) {
    sensor_state_t st;
    input_state_t inp;
    ui_status_t ui_st;
    char path[256];

    /* Screen 1: Safe Bring-Up Idle (USB disconnected, iron in stand PB5=1) */
    display_init();
    populate_sample_wave(&st, 120u, 2048u, 1480u);
    inp = (input_state_t){
        .held_mask = 0u,
        .pressed_mask = 0u,
        .released_mask = 0u,
        .long_mask = 0u,
        .raw_mask = 0u,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 3u
    };
    ui_st = (ui_status_t){
        .usb_connected = 0u,
        .stream_enabled = 0u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0x00u,
        .uptime_ms = 42000u
    };
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/01_safe_idle.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    /* Screen 2: USB CDC active, keys pressed, LED test running */
    display_init();
    populate_sample_wave(&st, 860u, 2240u, 1620u);
    inp = (input_state_t){
        .held_mask = KEY_BIT_PD2 | KEY_BIT_PC12,
        .pressed_mask = 0u,
        .released_mask = 0u,
        .long_mask = 0u,
        .raw_mask = KEY_BIT_PD2 | KEY_BIT_PC12,
        .pb5_sleep = 0u,
        .pb6_handle = 0u,
        .pb0_raw = 1u,
        .pb2_raw = 1u,
        .pb5_edges = 17u
    };
    ui_st = (ui_status_t){
        .usb_connected = 1u,
        .stream_enabled = 1u,
        .stream_interval_ms = 100u,
        .backlight_pct = 100u,
        .led_mask = 0x5Au,
        .uptime_ms = 185000u
    };
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/02_usb_keys_active.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    return 0;
}
