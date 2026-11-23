#include "heater.h"

#include "board.h"
#include "hw.h"
#include "sensors.h"

#include <stddef.h>

static void reset_pid(heater_ctrl_t *c) {
    c->pid_out_ut = 0;
    c->err_prev1_dc = 0;
    c->err_prev2_dc = 0;
    c->pid_primed = 0u;
    c->runaway_cnt = 0u;
    c->runaway_ref_dc = 0;
}

static uint16_t clamp_target_c(uint16_t t) {
    if (t < TEMP_MIN_C) return TEMP_MIN_C;
    if (t > TEMP_MAX_C) return TEMP_MAX_C;
    return t;
}

static uint16_t clamp_sleep_temp_c(uint16_t t) {
    if (t == 0u) return 0u;
    if (t < SLEEP_TEMP_MIN_C) return SLEEP_TEMP_MIN_C;
    if (t > SLEEP_TEMP_MAX_C) return SLEEP_TEMP_MAX_C;
    return t;
}

static int16_t clamp_cal_dc(int16_t cal_dc) {
    if (cal_dc < (int16_t)CAL_OFFSET_MIN_C * 10) return (int16_t)(CAL_OFFSET_MIN_C * 10);
    if (cal_dc > (int16_t)CAL_OFFSET_MAX_C * 10) return (int16_t)(CAL_OFFSET_MAX_C * 10);
    return cal_dc;
}

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
) {
    if (c == NULL) return;

    c->target_c = clamp_target_c(target_c);
    c->sleep_temp_c = clamp_sleep_temp_c(sleep_temp_c);
    c->sleep_en = sleep_en ? 1u : 0u;
    c->cal_f245_dc = clamp_cal_dc(cal_f245_dc);
    c->cal_f210_dc = clamp_cal_dc(cal_f210_dc);

    c->state = HEATER_STANDBY;
    c->armed = 0u; /* Boot-safe invariant: NEVER start heating to full target without lift/cmd */
    c->boot_auto_standby_done = 0u;
    c->is_f245 = initial_pb6 ? 1u : 0u;
    c->pb6_debounce_cnt = 0u;
    c->settle_ticks = HANDLE_SETTLE_TICKS;
    c->stand_cnt = 0u;
    /* PB5==0 -> in stand; if already out of stand at boot (PB5==1), NO auto-wake edge! */
    c->in_stand = (initial_pb5 == 0u) ? 1u : 0u;
    c->fault_mask = FAULT_SETTLING;

    c->tip_raw = 0u;
    c->tip_deci_c = 0;
    c->ema_initialized = 0u;
    c->aux_raw = 0u;
    c->rail_mv = 0u;
    c->cur_raw = 2048u;
    c->cur_zero_mv = CUR_ZERO_DEFAULT_MV;
    c->cur_ma = 0;
    c->cur_peak_ma = 0;
    c->low_load_flag = 0u;

    c->open_tip_recover_cnt = 0u;
    c->probe_pending = 1u;
    c->overload_cnt = 0u;
    c->duty_ticks = 0u;
    c->duty_pct = 0u;
    c->active_target_c = c->target_c;
    c->set_mux_cb = set_mux_cb;
    reset_pid(c);

    if (c->set_mux_cb != NULL) {
        c->set_mux_cb(c->is_f245);
    }
}

void heater_ctrl_update_rail(heater_ctrl_t *c, uint16_t aux_raw) {
    if (c == NULL) return;
    if (aux_raw > 4095u) aux_raw = 4095u;

    c->aux_raw = aux_raw;
    c->rail_mv = sensors_rail_adc_to_mv(aux_raw);

    if (c->rail_mv < RAIL_MIN_MV) {
        c->fault_mask = (uint16_t)((c->fault_mask & ~FAULT_OVERVOLT) | FAULT_UNDERVOLT);
        c->armed = 0u;
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        reset_pid(c);
        c->state = HEATER_FAULT;
    } else if (c->rail_mv > RAIL_MAX_MV) {
        c->fault_mask = (uint16_t)((c->fault_mask & ~FAULT_UNDERVOLT) | FAULT_OVERVOLT);
        c->armed = 0u;
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        reset_pid(c);
        c->state = HEATER_FAULT;
    } else {
        c->fault_mask &= (uint16_t)~(FAULT_UNDERVOLT | FAULT_OVERVOLT);
        if (c->state == HEATER_FAULT && (c->fault_mask & (uint16_t)~FAULT_SETTLING) == 0u) {
            c->state = HEATER_STANDBY;
        }
    }
}

uint8_t heater_ctrl_step_10ms(heater_ctrl_t *c, uint16_t tip_raw, uint8_t pb6_raw, uint8_t pb5_raw) {
    if (c == NULL) return 0u;

    if (tip_raw > 4095u) tip_raw = 4095u;
    c->tip_raw = tip_raw;
    pb6_raw = pb6_raw ? 1u : 0u;
    pb5_raw = pb5_raw ? 1u : 0u;

    /* 1. Handle detect debounce (PB6) + PC13..PC15 analog mux switch + 1.0 s settle */
    if (pb6_raw != c->is_f245) {
        c->pb6_debounce_cnt++;
        if (c->pb6_debounce_cnt >= HANDLE_DEBOUNCE_TICKS) {
            c->is_f245 = pb6_raw;
            c->pb6_debounce_cnt = 0u;
            c->settle_ticks = HANDLE_SETTLE_TICKS;
            c->fault_mask |= FAULT_SETTLING;
            c->ema_initialized = 0u;
            c->probe_pending = 1u;
            c->duty_ticks = 0u;
            c->duty_pct = 0u;
            reset_pid(c);
            if (c->set_mux_cb != NULL) {
                c->set_mux_cb(c->is_f245);
            }
        }
        return 0u;
    }
    c->pb6_debounce_cnt = 0u;

    /* 2. Sleep stand debounce (PB5: LOW = resting in stand, HIGH = lifted) */
    uint8_t raw_in_stand = (pb5_raw == 0u) ? 1u : 0u;
    uint8_t lifted_edge = 0u;
    if (c->settle_ticks == 0u && raw_in_stand != c->in_stand) {
        c->stand_cnt++;
        uint8_t needed = raw_in_stand ? STAND_IN_DEBOUNCE_TICKS : STAND_OUT_DEBOUNCE_TICKS;
        if (c->stand_cnt >= needed) {
            if (c->in_stand != 0u && raw_in_stand == 0u) {
                lifted_edge = 1u;
            }
            c->in_stand = raw_in_stand;
            c->stand_cnt = 0u;
        }
    } else if (raw_in_stand == c->in_stand) {
        c->stand_cnt = 0u;
    }

    /* 3. Handle-switch analog settling lockout (1.0 s) */
    if (c->settle_ticks > 0u) {
        c->settle_ticks--;
        c->fault_mask |= FAULT_SETTLING;
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        if ((c->fault_mask & (uint16_t)~FAULT_SETTLING) == 0u && c->armed == 0u) {
            c->state = HEATER_STANDBY;
        }
        return 0u;
    }
    c->fault_mask &= (uint16_t)~FAULT_SETTLING;

    /* 4. Open-tip check (PC0 > 4000) & 500 ms re-insertion debounce + probe arm */
    if (tip_raw > OPEN_TIP_ADC) {
        c->fault_mask |= FAULT_OPEN_TIP;
        c->open_tip_recover_cnt = 0u;
        c->probe_pending = 1u;
        c->ema_initialized = 0u;
        c->tip_deci_c = (int32_t)TEMP_HARD_CUTOFF_C * 10;
        reset_pid(c);
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        c->state = HEATER_FAULT;
        return 0u;
    }
    if ((c->fault_mask & FAULT_OPEN_TIP) != 0u) {
        if (c->open_tip_recover_cnt < OPEN_TIP_RECOVER_TICKS) {
            c->open_tip_recover_cnt++;
            c->duty_ticks = 0u;
            c->duty_pct = 0u;
            c->state = HEATER_FAULT;
            return 0u;
        }
        c->fault_mask &= (uint16_t)~FAULT_OPEN_TIP;
        c->open_tip_recover_cnt = 0u;
        c->probe_pending = 1u;
    }

    /* 5. Thermocouple conversion + EMA filter (alpha = 0.2) */
    int16_t cal_dc = c->is_f245 ? c->cal_f245_dc : c->cal_f210_dc;
    int32_t sample_dc = sensors_tip_adc_to_deci_c(tip_raw, c->is_f245, cal_dc);
    if (c->ema_initialized == 0u) {
        c->tip_deci_c = sample_dc;
        c->ema_initialized = 1u;
    } else {
        c->tip_deci_c = (c->tip_deci_c * 4 + sample_dc + 2) / 5;
    }

    /* 6. Absolute over-temperature guard (> 480 C) */
    if (c->tip_deci_c > (int32_t)TEMP_HARD_CUTOFF_C * 10) {
        c->fault_mask |= FAULT_OVERTEMP;
        c->armed = 0u;
        reset_pid(c);
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        c->state = HEATER_FAULT;
        return 0u;
    }

    /* 7. Rail / latched fault check & Auto-Wake / Standby check */
    uint16_t active_faults = c->fault_mask & (uint16_t)~FAULT_SETTLING;
    if (active_faults != 0u) {
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        reset_pid(c);
        c->state = HEATER_FAULT;
        return 0u;
    }

    /* Automatic standby preheat after boot when resting in stand with valid rail & tip */
    if (c->armed == 0u && c->boot_auto_standby_done == 0u &&
        c->sleep_en != 0u && c->in_stand != 0u && c->sleep_temp_c > 0u) {
        c->armed = 1u;
        c->boot_auto_standby_done = 1u;
        c->probe_pending = 1u;
        c->cur_peak_ma = 0;
        reset_pid(c);
    }

    /* Automatic wake-up on stand lift (PB5: 0 -> 1) when Auto Stand enabled & safe */
    if (lifted_edge != 0u && c->sleep_en != 0u) {
        if (c->armed == 0u) {
            c->armed = 1u;
            c->probe_pending = 1u;
            c->cur_peak_ma = 0;
            reset_pid(c);
        } else if (c->sleep_temp_c == 0u) {
            c->probe_pending = 1u;
            reset_pid(c);
        }
    }

    if (c->armed == 0u) {
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        c->active_target_c = c->target_c;
        reset_pid(c);
        c->state = HEATER_STANDBY;
        return 0u;
    }

    /* 8. Single 455 us probe pulse before closing PID loop (short-circuit check at 364 us) */
    if (c->probe_pending != 0u) {
        c->probe_pending = 0u;
        c->state = HEATER_PROBE;
        c->duty_ticks = PROBE_PULSE_TICKS;
        c->duty_pct = 5u;
        return PROBE_PULSE_TICKS;
    }

    /* 9. Active setpoint (Sleep in stand vs Active target) */
    if (c->sleep_en != 0u && c->in_stand != 0u) {
        c->state = HEATER_SLEEP;
        if (c->sleep_temp_c == 0u) {
            c->active_target_c = 0u;
            c->duty_ticks = 0u;
            c->duty_pct = 0u;
            reset_pid(c);
            return 0u;
        }
        c->active_target_c = (c->sleep_temp_c < c->target_c) ? c->sleep_temp_c : c->target_c;
    } else {
        c->active_target_c = c->target_c;
        c->state = HEATER_HEATING;
    }

    /* 10. Stock incremental PID + handle-specific duty ceiling */
    uint8_t max_ticks;
    if (c->is_f245 != 0u) {
        max_ticks = (c->tip_deci_c < 1000) ? MAX_DUTY_F245_COLD : MAX_DUTY_F245_HOT;
    } else {
        max_ticks = MAX_DUTY_F210;
    }
    int32_t max_ut = (int32_t)max_ticks * 10000;

    int32_t err_dc = (int32_t)c->active_target_c * 10 - c->tip_deci_c;
    if (c->pid_primed == 0u) {
        c->err_prev1_dc = err_dc;
        c->err_prev2_dc = err_dc;
        c->pid_primed = 1u;
    }

    if (err_dc > 500) {
        /* > +50 C error: full permitted duty */
        c->pid_out_ut = max_ut;
    } else if (err_dc < -50) {
        /* > +5 C overshoot: hard cut off */
        c->pid_out_ut = 0;
    } else {
        int32_t kp_m;
        int32_t ki_m;
        int32_t kd_m;
        if (c->is_f245 != 0u) {
            if (c->low_load_flag == 0u) {
                kp_m = 2280;
                ki_m = 920;
                kd_m = 280;
            } else {
                kp_m = 2480;
                ki_m = 15;
                kd_m = 20;
            }
        } else {
            ki_m = 20;
            kd_m = 10;
            if (c->tip_deci_c <= 1500) {
                kp_m = 300;
            } else if (c->tip_deci_c <= 2500) {
                kp_m = 400;
            } else if (c->tip_deci_c <= 3500) {
                kp_m = 500;
            } else {
                kp_m = 600;
            }
        }

        int32_t d1 = err_dc - c->err_prev1_dc;
        int32_t d2 = err_dc - 2 * c->err_prev1_dc + c->err_prev2_dc;
        int32_t delta_ut = kp_m * d1 + ki_m * err_dc + kd_m * d2;
        c->pid_out_ut += delta_ut;
        if (c->pid_out_ut < 0) c->pid_out_ut = 0;
        if (c->pid_out_ut > max_ut) c->pid_out_ut = max_ut;
    }

    c->err_prev2_dc = c->err_prev1_dc;
    c->err_prev1_dc = err_dc;

    uint8_t duty = (uint8_t)((c->pid_out_ut + 5000) / 10000);
    if (duty > max_ticks) {
        duty = max_ticks;
    }

    /* 11. Thermal runaway guard: >= RUNAWAY_MIN_DUTY for 8 s with < 5 C temperature rise */
    if (duty >= RUNAWAY_MIN_DUTY && err_dc > 200) {
        if (c->runaway_cnt == 0u) {
            c->runaway_ref_dc = c->tip_deci_c;
        }
        c->runaway_cnt++;
        if (c->tip_deci_c >= c->runaway_ref_dc + RUNAWAY_MIN_RISE_DC) {
            c->runaway_cnt = 0u;
            c->runaway_ref_dc = c->tip_deci_c;
        } else if (c->runaway_cnt >= RUNAWAY_MAX_TICKS) {
            c->fault_mask |= FAULT_RUNAWAY;
            c->armed = 0u;
            reset_pid(c);
            c->duty_ticks = 0u;
            c->duty_pct = 0u;
            c->state = HEATER_FAULT;
            return 0u;
        }
    } else {
        c->runaway_cnt = 0u;
    }

    c->duty_ticks = duty;
    c->duty_pct = (uint8_t)(((uint32_t)duty * 100u + ((uint32_t)max_ticks / 2u)) / (uint32_t)max_ticks);
    return duty;
}

int heater_ctrl_check_pulse_current(heater_ctrl_t *c, uint16_t cur_raw) {
    if (c == NULL) return 1;
    if (cur_raw > 4095u) cur_raw = 4095u;

    int32_t ma = sensors_cur_adc_to_ma_zero(cur_raw, c->cur_zero_mv);
    int32_t abs_ma = (ma < 0) ? -ma : ma;
    c->cur_raw = cur_raw;
    c->cur_ma = ma;
    c->cur_peak_ma = abs_ma;
    c->low_load_flag = (abs_ma < CUR_LOW_LOAD_MA) ? 1u : 0u;

    int32_t limit_ma = (c->is_f245 != 0u) ? CUR_LIMIT_F245_MA : CUR_LIMIT_F210_MA;
    if (abs_ma > limit_ma) {
        c->fault_mask |= FAULT_OVERCURRENT;
        c->armed = 0u;
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        reset_pid(c);
        c->state = HEATER_FAULT;
        return 1;
    }
    return 0;
}

void heater_ctrl_sample_current_slot5(heater_ctrl_t *c, uint16_t cur_raw, uint8_t heater_on) {
    if (c == NULL) return;
    if (cur_raw > 4095u) cur_raw = 4095u;

    c->cur_raw = cur_raw;
    uint32_t mv = sensors_adc_to_mv(cur_raw);

    if (heater_on == 0u) {
        /* Slowly track resting zero-offset (e.g. ~1642 mV on real board) when PA2 is OFF */
        if (mv >= 1450u && mv <= 1850u) {
            c->cur_zero_mv = (uint16_t)((((uint32_t)c->cur_zero_mv * 7u) + mv + 4u) / 8u);
        }
        if (c->overload_cnt > 0u) {
            c->overload_cnt--;
        }
        c->cur_ma = sensors_cur_adc_to_ma_zero(cur_raw, c->cur_zero_mv);
        return;
    }

    int32_t ma = sensors_cur_adc_to_ma_zero(cur_raw, c->cur_zero_mv);
    int32_t abs_ma = (ma < 0) ? -ma : ma;
    c->cur_ma = ma;
    if (abs_ma > c->cur_peak_ma) {
        c->cur_peak_ma = abs_ma;
    }

    if (abs_ma > CUR_OVERLOAD_MA) {
        c->overload_cnt++;
        if (c->overload_cnt >= OVERLOAD_MAX_TICKS) {
            c->fault_mask |= FAULT_OVERLOAD;
            c->armed = 0u;
            c->duty_ticks = 0u;
            c->duty_pct = 0u;
            reset_pid(c);
            c->state = HEATER_FAULT;
        }
    } else if (c->overload_cnt > 0u) {
        c->overload_cnt--;
    }
}

heater_cmd_err_t heater_ctrl_set_enable(heater_ctrl_t *c, uint8_t enable) {
    if (c == NULL) return HEATER_CMD_ERR_FAULT;

    if (enable == 0u) {
        c->armed = 0u;
        c->boot_auto_standby_done = 1u;
        c->duty_ticks = 0u;
        c->duty_pct = 0u;
        reset_pid(c);
        if ((c->fault_mask & (uint16_t)~FAULT_SETTLING) == 0u) {
            c->state = HEATER_STANDBY;
        }
        return HEATER_CMD_OK;
    }

    if ((c->fault_mask & FAULT_UNDERVOLT) != 0u) {
        return HEATER_CMD_ERR_NO_24V;
    }
    if ((c->fault_mask & FAULT_OVERVOLT) != 0u) {
        return HEATER_CMD_ERR_OVERVOLT;
    }
    if ((c->fault_mask & FAULT_OPEN_TIP) != 0u) {
        return HEATER_CMD_ERR_OPEN_TIP;
    }
    if ((c->fault_mask & (uint16_t)~FAULT_SETTLING) != 0u) {
        return HEATER_CMD_ERR_FAULT;
    }
    if (c->settle_ticks > 0u) {
        return HEATER_CMD_ERR_SETTLING;
    }

    if (c->armed == 0u) {
        c->armed = 1u;
        c->probe_pending = 1u;
        c->cur_peak_ma = 0;
        reset_pid(c);
    }
    return HEATER_CMD_OK;
}

heater_cmd_err_t heater_ctrl_set_target(heater_ctrl_t *c, uint16_t temp_c) {
    if (c == NULL) return HEATER_CMD_ERR_FAULT;
    if (temp_c < TEMP_MIN_C || temp_c > TEMP_MAX_C) {
        return HEATER_CMD_ERR_RANGE;
    }
    c->target_c = temp_c;
    if (c->armed == 0u || (c->sleep_en == 0u || c->in_stand == 0u)) {
        c->active_target_c = temp_c;
    }
    return HEATER_CMD_OK;
}

heater_cmd_err_t heater_ctrl_set_sleep(heater_ctrl_t *c, uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp) {
    if (c == NULL) return HEATER_CMD_ERR_FAULT;
    if (update_temp != 0u) {
        if (sleep_temp_c != 0u && (sleep_temp_c < SLEEP_TEMP_MIN_C || sleep_temp_c > SLEEP_TEMP_MAX_C)) {
            return HEATER_CMD_ERR_RANGE;
        }
        c->sleep_temp_c = sleep_temp_c;
    }
    c->sleep_en = enable ? 1u : 0u;
    return HEATER_CMD_OK;
}

heater_cmd_err_t heater_ctrl_set_cal(heater_ctrl_t *c, int16_t offset_c) {
    if (c == NULL) return HEATER_CMD_ERR_FAULT;
    if (offset_c < CAL_OFFSET_MIN_C || offset_c > CAL_OFFSET_MAX_C) {
        return HEATER_CMD_ERR_RANGE;
    }
    int16_t dc = (int16_t)(offset_c * 10);
    if (c->is_f245 != 0u) {
        c->cal_f245_dc = dc;
    } else {
        c->cal_f210_dc = dc;
    }
    return HEATER_CMD_OK;
}

heater_cmd_err_t heater_ctrl_clear_fault(heater_ctrl_t *c) {
    if (c == NULL) return HEATER_CMD_ERR_FAULT;
    /* Re-verify live conditions; keep persistent hardware faults if cause remains */
    uint16_t clearable = FAULT_OVERCURRENT | FAULT_OVERLOAD | FAULT_OVERTEMP | FAULT_RUNAWAY;
    c->fault_mask &= (uint16_t)~clearable;
    c->overload_cnt = 0u;
    reset_pid(c);
    if ((c->fault_mask & (uint16_t)~FAULT_SETTLING) == 0u) {
        c->state = HEATER_STANDBY;
        return HEATER_CMD_OK;
    }
    return HEATER_CMD_ERR_FAULT;
}

void heater_ctrl_get_status(const heater_ctrl_t *c, uint8_t pin_on, heater_status_t *out) {
    if (c == NULL || out == NULL) return;
    out->state = c->state;
    out->armed = c->armed;
    out->is_f245 = c->is_f245;
    out->in_stand = c->in_stand;
    out->sleep_en = c->sleep_en;
    out->fault_mask = c->fault_mask;
    out->target_c = c->target_c;
    out->active_target_c = c->active_target_c;
    out->sleep_temp_c = c->sleep_temp_c;
    out->tip_deci_c = c->tip_deci_c;
    out->rail_mv = c->rail_mv;
    out->cur_ma = c->cur_ma;
    out->cur_peak_ma = c->cur_peak_ma;
    out->cur_zero_mv = c->cur_zero_mv;
    out->duty_ticks = c->duty_ticks;
    out->duty_pct = c->duty_pct;
    out->heater_pin_on = pin_on ? 1u : 0u;
    out->low_load_flag = c->low_load_flag;
    out->tip_raw = c->tip_raw;
    out->cur_raw = c->cur_raw;
    out->aux_raw = c->aux_raw;
}

void heater_ready_notify_init(heater_ready_notify_t *n) {
    if (n == NULL) {
        return;
    }
    n->announced = 0u;
    n->last_target_c = 0u;
}

uint8_t heater_ready_notify_step(heater_ready_notify_t *n, const heater_status_t *st) {
    if (n == NULL || st == NULL) {
        return 0u;
    }
    if (st->state != HEATER_HEATING || st->active_target_c == 0u) {
        n->announced = 0u;
        n->last_target_c = st->active_target_c;
        return 0u;
    }
    if (st->active_target_c != n->last_target_c) {
        n->announced = 0u;
        n->last_target_c = st->active_target_c;
    }
    if (n->announced == 0u) {
        int32_t diff = (int32_t)st->active_target_c * 10 - st->tip_deci_c;
        if (diff >= -30 && diff <= 30) {
            n->announced = 1u;
            return 1u;
        }
    }
    return 0u;
}

void heater_open_tip_alarm_init(heater_open_tip_alarm_t *a) {
    if (a == NULL) {
        return;
    }
    a->active = 0u;
    a->next_beep_ms = 0u;
}

uint8_t heater_open_tip_alarm_step(heater_open_tip_alarm_t *a, uint16_t fault_mask, uint32_t now_ms) {
    if (a == NULL) {
        return 0u;
    }
    if ((fault_mask & FAULT_OPEN_TIP) == 0u) {
        a->active = 0u;
        return 0u;
    }
    if (a->active == 0u) {
        a->active = 1u;
        a->next_beep_ms = now_ms + OPEN_TIP_BEEP_INTERVAL_MS;
        return 1u;
    }
    if ((int32_t)(now_ms - a->next_beep_ms) >= 0) {
        a->next_beep_ms = now_ms + OPEN_TIP_BEEP_INTERVAL_MS;
        return 1u;
    }
    return 0u;
}

const char *heater_state_name(heater_state_t st) {
    switch (st) {
    case HEATER_STANDBY: return "STANDBY";
    case HEATER_SLEEP:   return "SLEEP";
    case HEATER_PROBE:   return "PROBE";
    case HEATER_HEATING: return "HEAT";
    case HEATER_FAULT:   return "FAULT";
    default:             return "UNK";
    }
}

const char *heater_fault_name(uint16_t fault_mask) {
    if (fault_mask & FAULT_OVERCURRENT) return "OVERCUR";
    if (fault_mask & FAULT_OVERTEMP)    return "OVERTEMP";
    if (fault_mask & FAULT_RUNAWAY)     return "RUNAWAY";
    if (fault_mask & FAULT_OVERLOAD)    return "OVERLOAD";
    if (fault_mask & FAULT_UNDERVOLT)   return "NO_24V";
    if (fault_mask & FAULT_OVERVOLT)    return "OVERVOLT";
    if (fault_mask & FAULT_OPEN_TIP)    return "OPEN_TIP";
    if (fault_mask & FAULT_SETTLING)    return "SETTLING";
    return "NONE";
}

const char *heater_err_msg(heater_cmd_err_t err) {
    switch (err) {
    case HEATER_CMD_OK:           return "OK";
    case HEATER_CMD_ERR_RANGE:    return "ERR RANGE";
    case HEATER_CMD_ERR_NO_24V:   return "ERR NO_24V_RAIL";
    case HEATER_CMD_ERR_OVERVOLT: return "ERR RAIL_OVERVOLT";
    case HEATER_CMD_ERR_OPEN_TIP: return "ERR OPEN_TIP";
    case HEATER_CMD_ERR_SETTLING: return "ERR HANDLE_SETTLING";
    case HEATER_CMD_ERR_FAULT:
    default:                      return "ERR FAULT_ACTIVE";
    }
}

/* =========================================================================
 * Target Hardware Singleton + TMR1 (1 ms / 10-slot) & TMR2 (91 us) ISRs
 * ========================================================================= */

static heater_ctrl_t g_ctrl;

#ifndef OPENDWS_HOST_TEST

static volatile uint8_t g_slot = 0u;
static volatile uint8_t g_rail_div = 0u;
static volatile uint8_t g_pulse_init = 0u;
static volatile uint8_t g_pulse_rem = 0u;

static inline void lock_control_irqs(void) {
    irq_disable(TMR2_IRQn);
    irq_disable(TMR1_OVF_IRQn);
}

static inline void unlock_control_irqs(void) {
    irq_enable(TMR1_OVF_IRQn);
    irq_enable(TMR2_IRQn);
}

void TMR1_OVF_TMR10_IRQHandler(void) {
    TMR_ISTS(TMR1_BASE) = 0u;

    uint8_t slot = g_slot;
    g_slot = (uint8_t)((slot + 1u) % 10u);

    if (slot == 1u) {
        /* Slot 1 (t = 1 ms): Unconditionally force PA2 LOW -> start blanking window */
        irq_disable(TMR2_IRQn);
        board_heater_force_off();
        g_pulse_rem = 0u;
        g_pulse_init = 0u;
        irq_enable(TMR2_IRQn);
    } else if (slot == 2u) {
        /* Slot 2 (t = 2 ms, exactly 1.0 ms after PA2=LOW): Read TC & run 100 Hz PID */
        irq_disable(TMR2_IRQn);
        uint16_t tip = sensors_read_channel(ADC_CH_TIP, 7u);
        uint8_t duty = heater_ctrl_step_10ms(&g_ctrl, tip, board_read_pb6_raw(), board_read_pb5_raw());
        if (duty > 0u) {
            g_pulse_init = duty;
            g_pulse_rem = duty;
            TMR_CVAL(TMR2_BASE) = 0u;
            TMR_ISTS(TMR2_BASE) = 0u;
            board_heater_set(1u);
        } else {
            g_pulse_init = 0u;
            g_pulse_rem = 0u;
            board_heater_force_off();
        }
        irq_enable(TMR2_IRQn);
    } else if (slot == 4u) {
        /* Slot 4 (t = 4 ms, every 500 ms): Sample 24 V rail monitor PA3 */
        if (g_rail_div == 0u) {
            irq_disable(TMR2_IRQn);
            uint16_t aux = sensors_read_channel(ADC_CH_AUX, 5u);
            irq_enable(TMR2_IRQn);
            heater_ctrl_update_rail(&g_ctrl, aux);
            if ((g_ctrl.fault_mask & (FAULT_UNDERVOLT | FAULT_OVERVOLT)) != 0u) {
                board_heater_force_off();
                g_pulse_rem = 0u;
                g_pulse_init = 0u;
            }
            g_rail_div = 49u;
        } else {
            g_rail_div--;
        }
    } else if (slot == 5u) {
        /* Slot 5 (t = 5 ms): Sample Hall current PC1 -> auto-zero when OFF, overload when ON */
        irq_disable(TMR2_IRQn);
        uint16_t cur = sensors_read_channel(ADC_CH_CUR, 5u);
        irq_enable(TMR2_IRQn);
        heater_ctrl_sample_current_slot5(&g_ctrl, cur, board_heater_is_on());
        if ((g_ctrl.fault_mask & FAULT_OVERLOAD) != 0u) {
            board_heater_force_off();
            g_pulse_rem = 0u;
            g_pulse_init = 0u;
        }
    }
}

void TMR2_IRQHandler(void) {
    TMR_ISTS(TMR2_BASE) = 0u;

    if (g_pulse_rem == 0u) {
        board_heater_force_off();
        return;
    }

    /* At t = 4 * 91 us = 364 us into pulse (rem == init - 3 on 4th IRQ), check short-circuit */
    if (g_pulse_init >= 4u && g_pulse_rem == (uint8_t)(g_pulse_init - 3u)) {
        uint16_t cur = sensors_read_channel(ADC_CH_CUR, 5u);
        if (heater_ctrl_check_pulse_current(&g_ctrl, cur) != 0) {
            board_heater_force_off();
            g_pulse_rem = 0u;
            g_pulse_init = 0u;
            return;
        }
    }

    g_pulse_rem--;
    if (g_pulse_rem == 0u) {
        board_heater_force_off();
    }
}

void heater_init(
    uint16_t target_c,
    uint16_t sleep_temp_c,
    uint8_t sleep_en,
    int16_t cal_f245_dc,
    int16_t cal_f210_dc
) {
    heater_ctrl_init(
        &g_ctrl,
        board_read_pb6_raw(),
        board_read_pb5_raw(),
        target_c,
        sleep_temp_c,
        sleep_en,
        cal_f245_dc,
        cal_f210_dc,
        board_set_handle_mux
    );
    /* Initial synchronous ADC baseline read BEFORE timers start */
    uint16_t aux = sensors_read_channel(ADC_CH_AUX, 5u);
    uint16_t cur = sensors_read_channel(ADC_CH_CUR, 5u);
    uint16_t tip = sensors_read_channel(ADC_CH_TIP, 7u);
    g_ctrl.tip_raw = tip;
    if (tip > OPEN_TIP_ADC) {
        g_ctrl.fault_mask |= FAULT_OPEN_TIP;
        g_ctrl.tip_deci_c = (int32_t)TEMP_HARD_CUTOFF_C * 10;
        g_ctrl.state = HEATER_FAULT;
    } else {
        int16_t cal = g_ctrl.is_f245 ? g_ctrl.cal_f245_dc : g_ctrl.cal_f210_dc;
        g_ctrl.tip_deci_c = sensors_tip_adc_to_deci_c(tip, g_ctrl.is_f245, cal);
        g_ctrl.ema_initialized = 1u;
    }
    heater_ctrl_update_rail(&g_ctrl, aux);
    heater_ctrl_sample_current_slot5(&g_ctrl, cur, 0u);
    g_slot = 0u;
    g_rail_div = 0u;
    g_pulse_init = 0u;
    g_pulse_rem = 0u;
}

void heater_hw_start(void) {
    board_timers_init();
}

heater_cmd_err_t heater_set_enable(uint8_t enable) {
    lock_control_irqs();
    heater_cmd_err_t r = heater_ctrl_set_enable(&g_ctrl, enable);
    if (enable == 0u || r != HEATER_CMD_OK) {
        board_heater_force_off();
        g_pulse_rem = 0u;
        g_pulse_init = 0u;
    }
    unlock_control_irqs();
    return r;
}

heater_cmd_err_t heater_set_target(uint16_t temp_c) {
    lock_control_irqs();
    heater_cmd_err_t r = heater_ctrl_set_target(&g_ctrl, temp_c);
    unlock_control_irqs();
    return r;
}

heater_cmd_err_t heater_set_sleep(uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp) {
    lock_control_irqs();
    heater_cmd_err_t r = heater_ctrl_set_sleep(&g_ctrl, enable, sleep_temp_c, update_temp);
    unlock_control_irqs();
    return r;
}

heater_cmd_err_t heater_set_cal(int16_t offset_c) {
    lock_control_irqs();
    heater_cmd_err_t r = heater_ctrl_set_cal(&g_ctrl, offset_c);
    unlock_control_irqs();
    return r;
}

heater_cmd_err_t heater_clear_fault(void) {
    lock_control_irqs();
    heater_cmd_err_t r = heater_ctrl_clear_fault(&g_ctrl);
    unlock_control_irqs();
    return r;
}

void heater_get_status(heater_status_t *out) {
    lock_control_irqs();
    heater_ctrl_get_status(&g_ctrl, board_heater_is_on(), out);
    unlock_control_irqs();
}

void heater_get_latest_adc(uint16_t *tip_raw, uint16_t *cur_raw, uint16_t *aux_raw) {
    lock_control_irqs();
    if (tip_raw != NULL) *tip_raw = g_ctrl.tip_raw;
    if (cur_raw != NULL) *cur_raw = g_ctrl.cur_raw;
    if (aux_raw != NULL) *aux_raw = g_ctrl.aux_raw;
    unlock_control_irqs();
}

#else

void heater_init(
    uint16_t target_c,
    uint16_t sleep_temp_c,
    uint8_t sleep_en,
    int16_t cal_f245_dc,
    int16_t cal_f210_dc
) {
    heater_ctrl_init(
        &g_ctrl, 1u, 0u, target_c, sleep_temp_c, sleep_en, cal_f245_dc, cal_f210_dc, board_set_handle_mux
    );
    heater_ctrl_update_rail(&g_ctrl, 1780u);
    heater_ctrl_sample_current_slot5(&g_ctrl, 2048u, 0u);
    (void)heater_ctrl_step_10ms(&g_ctrl, 22u, 1u, 0u);
}

void heater_hw_start(void) { board_timers_init(); }
heater_cmd_err_t heater_set_enable(uint8_t enable) { return heater_ctrl_set_enable(&g_ctrl, enable); }
heater_cmd_err_t heater_set_target(uint16_t temp_c) { return heater_ctrl_set_target(&g_ctrl, temp_c); }
heater_cmd_err_t heater_set_sleep(uint8_t enable, uint16_t sleep_temp_c, uint8_t update_temp) {
    return heater_ctrl_set_sleep(&g_ctrl, enable, sleep_temp_c, update_temp);
}
heater_cmd_err_t heater_set_cal(int16_t offset_c) { return heater_ctrl_set_cal(&g_ctrl, offset_c); }
heater_cmd_err_t heater_clear_fault(void) { return heater_ctrl_clear_fault(&g_ctrl); }
void heater_get_status(heater_status_t *out) { heater_ctrl_get_status(&g_ctrl, board_heater_is_on(), out); }
void heater_get_latest_adc(uint16_t *tip_raw, uint16_t *cur_raw, uint16_t *aux_raw) {
    if (tip_raw != NULL) *tip_raw = g_ctrl.tip_raw;
    if (cur_raw != NULL) *cur_raw = g_ctrl.cur_raw;
    if (aux_raw != NULL) *aux_raw = g_ctrl.aux_raw;
}

#endif /* OPENDWS_HOST_TEST */
