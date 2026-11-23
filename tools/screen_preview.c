#include "../tools/host_display.h"
#include "input.h"
#include "menu.h"
#include "sensors.h"
#include "settings.h"
#include "ui.h"

#include <stdio.h>

#ifndef SCREEN_PREVIEW_DIR
#define SCREEN_PREVIEW_DIR "build/preview"
#endif

static void populate_sample_trace(
    sensor_state_t *st,
    uint16_t tip_base,
    uint16_t cur_base,
    uint16_t aux_base,
    uint16_t start_c,
    uint16_t end_c
) {
    sensors_state_init(st);
    for (uint32_t i = 0u; i < 64u; ++i) {
        uint16_t temp_c = (uint16_t)(start_c + (((int32_t)end_c - (int32_t)start_c) * (int32_t)i) / 63);
        uint8_t pwr_pct = 0u;
        if (end_c > start_c) {
            pwr_pct = (i < 24u) ? 95u : (uint8_t)(95u - ((i - 24u) * 65u) / 39u);
        }
        sensors_update_snapshot(st, tip_base, cur_base, aux_base, NULL);
        sensors_record_temp_c(st, temp_c, NULL);
        sensors_record_power_pct(st, pwr_pct, NULL);
    }
}

int main(void) {
    sensor_state_t st;
    input_state_t inp;
    ui_status_t ui_st;
    char path[256];

    /* Screen 1: Boot Auto-Standby Preheat (F245 in stand, preheating/holding 100 C) */
    display_init();
    populate_sample_trace(&st, 720u, 2130u, 1779u, 25u, 100u);
    inp = (input_state_t){
        .held_mask = 0u,
        .pb5_sleep = 0u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 0u
    };
    ui_st = (ui_status_t){
        .hst = {
            .state = HEATER_SLEEP,
            .armed = 1u,
            .is_f245 = 1u,
            .in_stand = 1u,
            .sleep_en = 1u,
            .fault_mask = FAULT_NONE,
            .target_c = 300u,
            .active_target_c = 100u,
            .sleep_temp_c = 100u,
            .tip_deci_c = 1002,
            .rail_mv = 22940u,
            .cur_ma = 1450,
            .cur_peak_ma = 7200,
            .cur_zero_mv = 1642u,
            .duty_ticks = 11u,
            .duty_pct = 12u,
            .tip_raw = 720u,
            .cur_raw = 2130u,
            .aux_raw = 1779u
        },
        .presets_c = { 200u, 300u, 350u },
        .active_preset = 1u,
        .cal_offset_c = 0,
        .usb_connected = 1u,
        .stream_enabled = 0u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0x01u,
        .uptime_ms = 12000u
    };
    settings_defaults(&ui_st.settings);
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/01_standby_f245.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    /* Screen 2: Active Heating (F245 @ 315.4C -> 320C, 6.45A, USB Stream) */
    display_init();
    populate_sample_trace(&st, 2360u, 2560u, 1778u, 25u, 315u);
    inp = (input_state_t){
        .held_mask = KEY_BIT_PB3,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 8u
    };
    ui_st = (ui_status_t){
        .hst = {
            .state = HEATER_HEATING,
            .armed = 1u,
            .is_f245 = 1u,
            .in_stand = 0u,
            .sleep_en = 1u,
            .fault_mask = FAULT_NONE,
            .target_c = 320u,
            .active_target_c = 320u,
            .sleep_temp_c = 100u,
            .tip_deci_c = 3154,
            .rail_mv = 22920u,
            .cur_ma = 6450,
            .cur_peak_ma = 7820,
            .cur_zero_mv = 1642u,
            .duty_ticks = 64u,
            .duty_pct = 71u,
            .tip_raw = 2360u,
            .cur_raw = 2560u,
            .aux_raw = 1778u
        },
        .presets_c = { 200u, 300u, 350u },
        .active_preset = 0xFFu,
        .cal_offset_c = 2,
        .usb_connected = 1u,
        .stream_enabled = 1u,
        .stream_interval_ms = 100u,
        .backlight_pct = 100u,
        .led_mask = 0x3Fu,
        .uptime_ms = 185000u
    };
    settings_defaults(&ui_st.settings);
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/02_heating_f245.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    /* Screen 3: Safety Fault (Open Tip / No 24V USB-only protection) */
    display_init();
    populate_sample_trace(&st, 4080u, 2038u, 180u, 180u, 45u);
    inp = (input_state_t){
        .held_mask = KEY_BIT_PC11,
        .pb5_sleep = 0u,
        .pb6_handle = 0u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 9u
    };
    ui_st = (ui_status_t){
        .hst = {
            .state = HEATER_FAULT,
            .armed = 0u,
            .is_f245 = 0u,
            .in_stand = 1u,
            .sleep_en = 1u,
            .fault_mask = FAULT_OPEN_TIP | FAULT_UNDERVOLT,
            .target_c = 300u,
            .active_target_c = 300u,
            .sleep_temp_c = 100u,
            .tip_deci_c = 4800,
            .rail_mv = 2320u,
            .cur_ma = 0,
            .cur_peak_ma = 0,
            .cur_zero_mv = 1642u,
            .duty_ticks = 0u,
            .duty_pct = 0u,
            .tip_raw = 4080u,
            .cur_raw = 2038u,
            .aux_raw = 180u
        },
        .presets_c = { 200u, 300u, 350u },
        .active_preset = 1u,
        .cal_offset_c = 0,
        .usb_connected = 1u,
        .stream_enabled = 0u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0xAAu,
        .uptime_ms = 60000u
    };
    settings_defaults(&ui_st.settings);
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/03_fault_protection.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    /* Screen 4: Open Settings Menu editing Sleep Temp (100 C) with live header telemetry */
    display_init();
    populate_sample_trace(&st, 2250u, 2410u, 1779u, 100u, 300u);
    inp = (input_state_t){
        .held_mask = KEY_BIT_PD2,
        .pb5_sleep = 1u,
        .pb6_handle = 1u,
        .pb0_raw = 1u,
        .pb2_raw = 0u,
        .pb5_edges = 10u
    };
    ui_st = (ui_status_t){
        .hst = {
            .state = HEATER_HEATING,
            .armed = 1u,
            .is_f245 = 1u,
            .in_stand = 0u,
            .sleep_en = 1u,
            .fault_mask = FAULT_NONE,
            .target_c = 300u,
            .active_target_c = 300u,
            .sleep_temp_c = 100u,
            .tip_deci_c = 2998,
            .rail_mv = 22920u,
            .cur_ma = 2150,
            .cur_peak_ma = 5400,
            .cur_zero_mv = 1642u,
            .duty_ticks = 28u,
            .duty_pct = 31u,
            .tip_raw = 2250u,
            .cur_raw = 2410u,
            .aux_raw = 1779u
        },
        .presets_c = { 200u, 300u, 350u },
        .active_preset = 1u,
        .cal_offset_c = 0,
        .usb_connected = 1u,
        .stream_enabled = 0u,
        .stream_interval_ms = 100u,
        .backlight_pct = 80u,
        .led_mask = 0x07u,
        .uptime_ms = 92000u
    };
    settings_defaults(&ui_st.settings);
    ui_st.menu.open = 1u;
    ui_st.menu.cursor = MENU_ITEM_SLEEP_TEMP;
    ui_st.menu.scroll_top = 0u;
    ui_st.menu.editing = 1u;
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/04_settings_menu.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    /* Screen 5: Temperature & Power Dual-Curve Graph Mode (PD2 MODE toggle) */
    display_init();
    populate_sample_trace(&st, 2320u, 2260u, 1779u, 28u, 315u);
    inp.held_mask = 0u;
    ui_st.menu.open = 0u;
    ui_st.menu.editing = 0u;
    ui_st.graph_mode = 1u;
    ui_st.hst.target_c = 320u;
    ui_st.hst.active_target_c = 320u;
    ui_st.hst.tip_deci_c = 3154;
    ui_st.hst.cur_ma = 2850;
    ui_st.hst.duty_ticks = 29u;
    ui_st.hst.duty_pct = 32u;
    ui_draw_static();
    ui_draw_dynamic(&st.snap, &inp, &ui_st);
    snprintf(path, sizeof(path), "%s/05_graph_temp_power.ppm", SCREEN_PREVIEW_DIR);
    if (host_display_write_ppm(path) != 0) return 1;

    return 0;
}
