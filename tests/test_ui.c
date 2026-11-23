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
        sensors_update_snapshot(&sst, (uint16_t)(200u + i * 50u), 2400u, 1780u, NULL);
        sensors_record_temp_c(&sst, (uint16_t)(25u + i * 9u), NULL);
        sensors_record_power_pct(&sst, (uint8_t)(90u - i * 2u), NULL);
    }

    input_state_t inp = {
        .held_mask = KEY_BIT_PD2 | KEY_BIT_PB3,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 12u
    };

    ui_status_t st = {
        .hst = {
            .state = HEATER_FAULT,
            .armed = 0u,
            .is_f245 = 1u,
            .in_stand = 0u,
            .sleep_en = 1u,
            .fault_mask = FAULT_OVERCURRENT,
            .target_c = 300u,
            .active_target_c = 300u,
            .sleep_temp_c = 100u,
            .tip_deci_c = 3120,
            .rail_mv = 22920u,
            .cur_ma = 0,
            .cur_peak_ma = 11200,
            .cur_zero_mv = 1642u,
            .duty_ticks = 0u,
            .duty_pct = 0u,
            .tip_raw = 2350u,
            .cur_raw = 2038u,
            .aux_raw = 1780u
        },
        .presets_c = { 200u, 300u, 350u },
        .active_preset = 1u,
        .cal_offset_c = 0,
        .usb_connected = 1u,
        .stream_enabled = 1u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0xAAu,
        .uptime_ms = 65000u
    };
    settings_defaults(&st.settings);
    menu_init(&st.menu);

    ui_draw_static();
    ui_draw_dynamic(&sst.snap, &inp, &st);

    uint32_t non_black = 0u;
    uint32_t red_badge_pixels = 0u;
    for (uint16_t y = 0u; y < LCD_HEIGHT; ++y) {
        for (uint16_t x = 0u; x < LCD_WIDTH; ++x) {
            uint16_t px = host_display_pixel(x, y);
            if (px != 0u) non_black++;
            if (y >= 4u && y < 22u && x >= 78u && x < 166u) {
                uint8_t r = (uint8_t)((px >> 11) & 0x1Fu);
                uint8_t b = (uint8_t)(px & 0x1Fu);
                if (r > b + 4u) red_badge_pixels++;
            }
        }
    }

    assert(non_black == (uint32_t)LCD_WIDTH * (uint32_t)LCD_HEIGHT);
    assert(red_badge_pixels > 400u);

    /* Verify interactive Settings Menu screen transition & rendering */
    st.hst.state = HEATER_HEATING;
    st.hst.armed = 1u;
    st.hst.fault_mask = FAULT_NONE;
    st.menu.open = 1u;
    st.menu.cursor = MENU_ITEM_SLEEP_TEMP;
    st.menu.editing = 1u;
    ui_draw_dynamic(&sst.snap, &inp, &st);

    non_black = 0u;
    for (uint16_t y = 0u; y < LCD_HEIGHT; ++y) {
        for (uint16_t x = 0u; x < LCD_WIDTH; ++x) {
            if (host_display_pixel(x, y) != 0u) non_black++;
        }
    }
    assert(non_black == (uint32_t)LCD_WIDTH * (uint32_t)LCD_HEIGHT);

    /* Verify clean exit back to main screen */
    st.menu.open = 0u;
    st.menu.editing = 0u;
    ui_draw_dynamic(&sst.snap, &inp, &st);

    /* Verify Temperature + Power Graph mode rendering (PD2 toggle) */
    st.graph_mode = 1u;
    ui_draw_dynamic(&sst.snap, &inp, &st);
    non_black = 0u;
    for (uint16_t y = 0u; y < LCD_HEIGHT; ++y) {
        for (uint16_t x = 0u; x < LCD_WIDTH; ++x) {
            if (host_display_pixel(x, y) != 0u) non_black++;
        }
    }
    assert(non_black == (uint32_t)LCD_WIDTH * (uint32_t)LCD_HEIGHT);

    /* Switch back to main screen cleanly */
    st.graph_mode = 0u;
    ui_draw_dynamic(&sst.snap, &inp, &st);

    printf("test_ui: PASS\n");
    return 0;
}
