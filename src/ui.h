#pragma once

#include "input.h"
#include "sensors.h"

#include <stdint.h>

typedef struct {
    uint8_t usb_connected;
    uint8_t stream_enabled;
    uint16_t stream_interval_ms;
    uint8_t backlight_pct;
    uint8_t led_mask;
    uint32_t uptime_ms;
    uint16_t usb_irq_cnt;
    uint16_t usb_rst_cnt;
    uint16_t usb_setup_cnt;
    uint8_t  usb_gccfg_hi;
} ui_status_t;

void ui_draw_static(void);
void ui_draw_dynamic(const sensor_snapshot_t *snap, const input_state_t *inp, const ui_status_t *st);
