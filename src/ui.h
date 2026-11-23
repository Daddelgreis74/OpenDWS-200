#pragma once

#include "heater.h"
#include "input.h"
#include "menu.h"
#include "sensors.h"
#include "settings.h"

#include <stdint.h>

typedef struct {
    heater_status_t hst;
    menu_state_t menu;
    opendws_settings_t settings;
    uint16_t presets_c[3];
    uint8_t active_preset; /* 0..2 or 0xFF if custom temp */
    int16_t cal_offset_c;
    uint8_t usb_connected;
    uint8_t stream_enabled;
    uint16_t stream_interval_ms;
    uint8_t backlight_pct;
    uint8_t led_mask;
    uint8_t save_toast_active;
    uint32_t uptime_ms;
} ui_status_t;

void ui_draw_static(void);
void ui_draw_dynamic(const sensor_snapshot_t *snap, const input_state_t *inp, const ui_status_t *st);
