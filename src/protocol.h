#pragma once

#include "input.h"
#include "sensors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*write)(const char *data, size_t len, void *user);
    void (*set_backlight)(uint8_t pct, void *user);
    void (*beep)(uint16_t ms, void *user);
    void (*set_leds)(uint8_t mask, void *user);
    void (*reboot)(void *user);
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
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
);

void protocol_tick(
    protocol_ctx_t *ctx,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t now_ms
);

void protocol_handle_line(
    protocol_ctx_t *ctx,
    const char *line,
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    uint8_t bl_pct,
    uint8_t led_mask,
    uint32_t uptime_ms
);
