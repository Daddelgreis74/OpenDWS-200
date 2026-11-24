#pragma once

#include "heater.h"
#include "input.h"
#include "sensors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*write)(const char *data, size_t len, void *user);
    void (*set_backlight)(uint8_t pct, void *user);
    void (*beep)(uint16_t ms, void *user);
    void (*set_leds)(uint8_t mask, void *user);
    void (*set_leds_auto)(void *user);
    void (*reboot)(void *user);
    heater_cmd_err_t (*set_heater)(uint8_t enable, void *user);
    heater_cmd_err_t (*set_target)(uint16_t temp_c, void *user);
    heater_cmd_err_t (*select_preset)(uint8_t idx_1_to_3, uint16_t *out_temp_c, void *user);
    heater_cmd_err_t (*set_sleep)(uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp, void *user);
    heater_cmd_err_t (*set_cal)(int16_t offset_c, void *user);
    heater_cmd_err_t (*set_cal_point)(uint8_t point_idx, int16_t offset_c, void *user);
    heater_cmd_err_t (*clear_fault)(void *user);
    int (*save_settings)(void *user);
    void *user;
} protocol_ops_t;

typedef struct {
    protocol_ops_t ops;
    uint8_t stream_enabled;
    uint16_t stream_interval_ms;
    uint32_t last_stream_ms;
} protocol_ctx_t;

void protocol_init(
    protocol_ctx_t *ctx,
    const protocol_ops_t *ops,
    uint8_t stream_enabled,
    uint16_t stream_interval_ms
);

void protocol_emit_stat(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
);

void protocol_tick(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t now_ms
);

void protocol_handle_line(
    protocol_ctx_t *ctx,
    const char *line,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const heater_status_t *hst,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
);
