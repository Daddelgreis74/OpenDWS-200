#pragma once

#include "app_config.h"

#include <stdint.h>

typedef enum {
    HEATER_STANDBY = 0,
    HEATER_SLEEP   = 1,
    HEATER_PROBE   = 2,
    HEATER_HEATING = 3,
    HEATER_FAULT   = 4
} heater_state_t;

typedef enum {
    HEATER_CMD_OK           = 0,
    HEATER_CMD_ERR_RANGE    = -1,
    HEATER_CMD_ERR_NO_24V   = -2,
    HEATER_CMD_ERR_OVERVOLT = -3,
    HEATER_CMD_ERR_OPEN_TIP = -4,
    HEATER_CMD_ERR_FAULT    = -5,
    HEATER_CMD_ERR_SETTLING = -6
} heater_cmd_err_t;

#define FAULT_NONE        0x0000u
#define FAULT_OPEN_TIP    (1u << 0)
#define FAULT_OVERCURRENT (1u << 1)
#define FAULT_OVERLOAD    (1u << 2)
#define FAULT_OVERTEMP    (1u << 3)
#define FAULT_UNDERVOLT   (1u << 4)
#define FAULT_OVERVOLT    (1u << 5)
#define FAULT_RUNAWAY     (1u << 6)
#define FAULT_SETTLING    (1u << 7)

typedef struct {
    /* User configuration */
    uint16_t target_c;
    uint16_t sleep_temp_c;
    uint8_t  sleep_en;
    int16_t  cal_f245_dc;
    int16_t  cal_f210_dc;

    /* Operating state */
    heater_state_t state;
    uint8_t  armed;
    uint8_t  is_f245;
    uint8_t  pb6_debounce_cnt;
    uint16_t settle_ticks;
    uint8_t  stand_cnt;
    uint8_t  in_stand;
    uint16_t fault_mask;

    /* Live telemetry */
    uint16_t tip_raw;
    int32_t  tip_deci_c;
    uint8_t  ema_initialized;
    uint16_t aux_raw;
    uint32_t rail_mv;
    uint16_t cur_raw;
    uint16_t cur_zero_mv;
    int32_t  cur_ma;
    int32_t  cur_peak_ma;
    uint8_t  low_load_flag;

    /* Safety & probe timers */
    uint16_t open_tip_recover_cnt;
    uint8_t  probe_pending;
    uint16_t overload_cnt;
    uint16_t runaway_cnt;
    int32_t  runaway_ref_dc;

    /* Incremental PID state (1 unit = 0.0001 tick) */
    int32_t  pid_out_ut;
    int32_t  err_prev1_dc;
    int32_t  err_prev2_dc;
    uint8_t  pid_primed;
    uint8_t  duty_ticks;
    uint8_t  duty_pct;
    uint16_t active_target_c;

    void (*set_mux_cb)(uint8_t is_f245);
} heater_ctrl_t;

typedef struct {
    heater_state_t state;
    uint8_t  armed;
    uint8_t  is_f245;
    uint8_t  in_stand;
    uint8_t  sleep_en;
    uint16_t fault_mask;
    uint16_t target_c;
    uint16_t active_target_c;
    uint16_t sleep_temp_c;
    int32_t  tip_deci_c;
    uint32_t rail_mv;
    int32_t  cur_ma;
    int32_t  cur_peak_ma;
    uint16_t cur_zero_mv;
    uint8_t  duty_ticks;
    uint8_t  duty_pct;
    uint8_t  heater_pin_on;
    uint8_t  low_load_flag;
    uint16_t tip_raw;
    uint16_t cur_raw;
    uint16_t aux_raw;
} heater_status_t;

/* Pure, host-testable control logic */
void heater_ctrl_init(
    heater_ctrl_t *c,
    uint8_t initial_pb6,
    uint8_t initial_pb5,
    uint16_t target_c,
    uint16_t sleep_temp_c,
    uint8_t sleep_en,
    int16_t cal_f245_dc,
    int16_t cal_f210_dc,
    void (*set_mux_cb)(uint8_t is_f245)
);

void heater_ctrl_update_rail(heater_ctrl_t *c, uint16_t aux_raw);
uint8_t heater_ctrl_step_10ms(heater_ctrl_t *c, uint16_t tip_raw, uint8_t pb6_raw, uint8_t pb5_raw);
int heater_ctrl_check_pulse_current(heater_ctrl_t *c, uint16_t cur_raw);
void heater_ctrl_sample_current_slot5(heater_ctrl_t *c, uint16_t cur_raw, uint8_t heater_on);

heater_cmd_err_t heater_ctrl_set_enable(heater_ctrl_t *c, uint8_t enable);
heater_cmd_err_t heater_ctrl_set_target(heater_ctrl_t *c, uint16_t temp_c);
heater_cmd_err_t heater_ctrl_set_sleep(heater_ctrl_t *c, uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp);
heater_cmd_err_t heater_ctrl_set_cal(heater_ctrl_t *c, int16_t offset_c);
heater_cmd_err_t heater_ctrl_clear_fault(heater_ctrl_t *c);
void heater_ctrl_get_status(const heater_ctrl_t *c, uint8_t pin_on, heater_status_t *out);

/* String formatting helpers */
const char *heater_state_name(heater_state_t st);
const char *heater_fault_name(uint16_t fault_mask);
const char *heater_err_msg(heater_cmd_err_t err);

/* Singleton hardware controller API (used by main loop & USB protocol) */
void heater_init(
    uint16_t target_c,
    uint16_t sleep_temp_c,
    uint8_t sleep_en,
    int16_t cal_f245_dc,
    int16_t cal_f210_dc
);
void heater_hw_start(void);
heater_cmd_err_t heater_set_enable(uint8_t enable);
heater_cmd_err_t heater_set_target(uint16_t temp_c);
heater_cmd_err_t heater_set_sleep(uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp);
heater_cmd_err_t heater_set_cal(int16_t offset_c);
heater_cmd_err_t heater_clear_fault(void);
void heater_get_status(heater_status_t *out);
void heater_get_latest_adc(uint16_t *tip_raw, uint16_t *cur_raw, uint16_t *aux_raw);
