#pragma once

#include <stdint.h>

#define KEY_BIT_PD2  (1u << 0)
#define KEY_BIT_PB3  (1u << 1)
#define KEY_BIT_PB4  (1u << 2)
#define KEY_BIT_PC12 (1u << 3)
#define KEY_BIT_PC11 (1u << 4)
#define KEY_COUNT    5u

#define INPUT_DEBOUNCE_MS   20u
#define INPUT_LONG_PRESS_MS 700u

typedef struct {
    uint8_t held_mask;
    uint8_t pressed_mask;
    uint8_t released_mask;
    uint8_t long_mask;
    uint8_t raw_mask;
    uint8_t pb5_sleep;
    uint8_t pb6_handle;
    uint8_t pb0_raw;
    uint8_t pb2_raw;
    uint32_t pb5_edges;
} input_state_t;

typedef struct {
    uint8_t stable_mask;
    uint8_t last_raw_mask;
    uint8_t long_fired_mask;
    uint32_t change_ms[KEY_COUNT];
    uint32_t press_ms[KEY_COUNT];
} input_debouncer_t;

void input_debouncer_init(input_debouncer_t *db);

void input_update_from_raw(
    input_debouncer_t *db,
    uint8_t raw_keys_mask,
    uint8_t pb5_sleep,
    uint32_t pb5_edges,
    uint8_t pb6_handle,
    uint8_t pb0_raw,
    uint8_t pb2_raw,
    uint32_t now_ms,
    input_state_t *out
);

void input_init(void);
void input_poll(uint32_t now_ms, input_state_t *out);

typedef enum {
    SOFT_PWR_WAIT_IDLE = 0,
    SOFT_PWR_ARMED     = 1,
    SOFT_PWR_PRESSED   = 2,
    SOFT_PWR_OFF       = 3
} soft_pwr_state_t;

typedef struct {
    soft_pwr_state_t state;
    uint32_t edge_ms;
    uint8_t candidate;
} soft_power_t;

void soft_power_init(soft_power_t *sp);
uint8_t soft_power_step(soft_power_t *sp, uint8_t pb0_raw, uint32_t now_ms);
