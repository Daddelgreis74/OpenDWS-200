#include "heater.h"
#include "sensors.h"

#include <assert.h>
#include <stdio.h>

static uint8_t g_mux_calls = 0u;
static uint8_t g_mux_last = 0u;

static void mock_mux(uint8_t is_f245) {
    g_mux_calls++;
    g_mux_last = is_f245;
}

static uint16_t f245_dc_to_adc(int32_t dc) {
    static const int32_t y[5] = { 0, 1500, 2500, 3500, 4500 };
    static const int32_t x[5] = { 0, 1100, 1870, 2610, 3340 };
    if (dc <= 0) return 0u;
    if (dc >= 4500) {
        int32_t v = x[4] + ((dc - y[4]) * (x[4] - x[3])) / 1000;
        return (v > 4095) ? 4095u : (uint16_t)v;
    }
    int seg = 0;
    while (seg < 3 && dc > y[seg + 1]) seg++;
    int32_t v = x[seg] + ((dc - y[seg]) * (x[seg + 1] - x[seg])) / (y[seg + 1] - y[seg]);
    return (uint16_t)v;
}

int main(void) {
    heater_ctrl_t c;

    /* 1. Boot-safe Standby (iron IN stand PB5=0) & F245 initialization */
    g_mux_calls = 0u;
    heater_ctrl_init(&c, 1u, 0u, 300u, 100u, 1u, 0, 0, mock_mux);
    assert(c.armed == 0u);
    assert(c.in_stand == 1u);
    assert(c.state == HEATER_STANDBY);
    assert(c.is_f245 == 1u);
    assert(g_mux_calls == 1u && g_mux_last == 1u);

    /* Boot with iron ALREADY out of stand (PB5=1) must NEVER auto-heat */
    heater_ctrl_t out_boot;
    heater_ctrl_init(&out_boot, 1u, 1u, 300u, 100u, 1u, 0, 0, NULL);
    heater_ctrl_update_rail(&out_boot, 1780u);
    for (uint32_t i = 0u; i < HANDLE_SETTLE_TICKS + 30u; ++i) {
        assert(heater_ctrl_step_10ms(&out_boot, 200u, 1u, 1u) == 0u);
    }
    assert(out_boot.armed == 0u && out_boot.state == HEATER_STANDBY);

    /* USB-only rail (2.32 V -> 180 raw) blocks manual & auto-lift heating */
    heater_ctrl_update_rail(&c, 180u);
    assert((c.fault_mask & FAULT_UNDERVOLT) != 0u);
    assert(heater_ctrl_set_enable(&c, 1u) == HEATER_CMD_ERR_NO_24V);
    assert(heater_ctrl_step_10ms(&c, 100u, 1u, 0u) == 0u);

    /* Overvoltage rail (> 30 V -> 2500 raw ~32.2 V) blocks heating */
    heater_ctrl_update_rail(&c, 2500u);
    assert((c.fault_mask & FAULT_OVERVOLT) != 0u);
    assert(heater_ctrl_set_enable(&c, 1u) == HEATER_CMD_ERR_OVERVOLT);

    /* Normal 22.9 V rail (1780 raw) clears rail faults, auto-arms into 100 C Standby preheat in stand */
    heater_ctrl_update_rail(&c, 1780u);
    assert((c.fault_mask & (FAULT_UNDERVOLT | FAULT_OVERVOLT)) == 0u);
    assert(c.armed == 0u);
    for (uint32_t i = 0u; i < HANDLE_SETTLE_TICKS - 1u; ++i) {
        assert(heater_ctrl_step_10ms(&c, 200u, 1u, 0u) == 0u);
    }
    assert(c.settle_ticks == 0u);
    uint8_t duty = heater_ctrl_step_10ms(&c, 200u, 1u, 0u);
    assert((c.fault_mask & FAULT_SETTLING) == 0u);
    assert(c.armed == 1u);
    assert(duty == PROBE_PULSE_TICKS);
    assert(c.state == HEATER_PROBE);
    assert(heater_ctrl_check_pulse_current(&c, 2539u) == 0);

    /* Next cycle in stand enters HEATER_SLEEP preheating toward 100 C */
    duty = heater_ctrl_step_10ms(&c, 200u, 1u, 0u);
    assert(c.state == HEATER_SLEEP);
    assert(c.active_target_c == 100u);
    assert(duty > 0u);

    /* 2. Lifting iron out of stand (PB5: 0 -> 1 for 80 ms) switches from 100 C Standby to 300 C Heating! */
    for (uint32_t i = 0u; i < STAND_OUT_DEBOUNCE_TICKS - 1u; ++i) {
        (void)heater_ctrl_step_10ms(&c, 200u, 1u, 1u);
        assert(c.in_stand == 1u);
    }
    duty = heater_ctrl_step_10ms(&c, 200u, 1u, 1u);
    assert(c.armed == 1u);
    assert(c.in_stand == 0u);
    assert(c.state == HEATER_HEATING);
    assert(c.active_target_c == 300u);
    assert(duty > 0u);

    /* Manual disarm in stand still wakes with 5-tick probe pulse when lifted */
    heater_ctrl_t lift_wake;
    heater_ctrl_init(&lift_wake, 1u, 0u, 300u, 100u, 1u, 0, 0, NULL);
    heater_ctrl_update_rail(&lift_wake, 1780u);
    for (uint32_t i = 0u; i < HANDLE_SETTLE_TICKS; ++i) {
        (void)heater_ctrl_step_10ms(&lift_wake, 200u, 1u, 0u);
    }
    assert(heater_ctrl_set_enable(&lift_wake, 0u) == HEATER_CMD_OK);
    assert(heater_ctrl_step_10ms(&lift_wake, 200u, 1u, 0u) == 0u);
    assert(lift_wake.armed == 0u);
    for (uint32_t i = 0u; i < STAND_OUT_DEBOUNCE_TICKS - 1u; ++i) {
        assert(heater_ctrl_step_10ms(&lift_wake, 200u, 1u, 1u) == 0u);
    }
    assert(heater_ctrl_step_10ms(&lift_wake, 200u, 1u, 1u) == PROBE_PULSE_TICKS);
    assert(lift_wake.armed == 1u && lift_wake.state == HEATER_PROBE);

    /* Healthy current (~6.0 A = 1650 + 396 = 2046 mV -> ~2539 raw) passes 364 us check */
    assert(heater_ctrl_check_pulse_current(&c, 2539u) == 0);
    assert(c.low_load_flag == 0u);

    /* 3. Closed-loop PID converges on F245 without exceeding limits */
    int32_t plant_dc = 250; /* 25.0 C */
    for (uint32_t tick = 0u; tick < 600u; ++tick) {
        uint16_t raw = f245_dc_to_adc(plant_dc);
        duty = heater_ctrl_step_10ms(&c, raw, 1u, 1u);
        uint8_t cap = (c.tip_deci_c < 1000) ? MAX_DUTY_F245_COLD : MAX_DUTY_F245_HOT;
        assert(duty <= cap);
        /* Thermal plant: heating minus ambient cooling */
        plant_dc += (int32_t)duty * 15 / 100 - (plant_dc - 250) / 500;
        if (plant_dc < 250) plant_dc = 250;
    }
    assert(plant_dc >= 2850 && plant_dc <= 3150);

    /* 4. Sleep stand debounce (PB5=0 -> in stand -> active_target = 100 C), then 0 W mode */
    for (uint32_t i = 0u; i < STAND_IN_DEBOUNCE_TICKS; ++i) {
        (void)heater_ctrl_step_10ms(&c, f245_dc_to_adc(plant_dc), 1u, 0u);
    }
    assert(c.in_stand == 1u);
    assert(c.state == HEATER_SLEEP);
    assert(c.active_target_c == 100u);

    /* Zero-watt sleep_temp_c == 0 cuts duty to 0 while remaining armed for lift */
    assert(heater_ctrl_set_sleep(&c, 1u, 0u, 1u) == HEATER_CMD_OK);
    assert(heater_ctrl_step_10ms(&c, f245_dc_to_adc(plant_dc), 1u, 0u) == 0u);
    assert(c.state == HEATER_SLEEP && c.active_target_c == 0u);
    assert(heater_ctrl_set_sleep(&c, 1u, 100u, 1u) == HEATER_CMD_OK);

    /* Lifted out of stand (PB5=1 for 80 ms) -> resumes 300 C heating */
    for (uint32_t i = 0u; i < STAND_OUT_DEBOUNCE_TICKS; ++i) {
        (void)heater_ctrl_step_10ms(&c, f245_dc_to_adc(plant_dc), 1u, 1u);
    }
    assert(c.in_stand == 0u);
    assert(c.state == HEATER_HEATING);
    assert(c.active_target_c == 300u);

    /* 5. Open-tip > 4000 cutoff and 500 ms recovery -> 455 us probe pulse */
    assert(heater_ctrl_step_10ms(&c, 4050u, 1u, 1u) == 0u);
    assert((c.fault_mask & FAULT_OPEN_TIP) != 0u);
    assert(c.state == HEATER_FAULT);
    for (uint32_t i = 0u; i < OPEN_TIP_RECOVER_TICKS; ++i) {
        assert(heater_ctrl_step_10ms(&c, 1500u, 1u, 1u) == 0u);
    }
    duty = heater_ctrl_step_10ms(&c, 1500u, 1u, 1u);
    assert((c.fault_mask & FAULT_OPEN_TIP) == 0u);
    assert(duty == PROBE_PULSE_TICKS);

    /* 6. Short-circuit 364 us overcurrent trip on F245 (> 10 A) */
    /* 12 A -> 1650 + 792 = 2442 mV -> ~3030 raw */
    assert(heater_ctrl_check_pulse_current(&c, 3030u) == 1);
    assert((c.fault_mask & FAULT_OVERCURRENT) != 0u);
    assert(c.armed == 0u);
    assert(heater_ctrl_clear_fault(&c) == HEATER_CMD_OK);

    /* 7. Handle switch PB6=1 -> PB6=0 (F210): debounce 50 ms, mux call, 1.0 s settle, 30-tick cap */
    for (uint32_t i = 0u; i < HANDLE_DEBOUNCE_TICKS; ++i) {
        assert(heater_ctrl_step_10ms(&c, 200u, 0u, 1u) == 0u);
    }
    assert(c.is_f245 == 0u);
    assert(g_mux_last == 0u);
    for (uint32_t i = 0u; i <= HANDLE_SETTLE_TICKS; ++i) {
        assert(heater_ctrl_step_10ms(&c, 200u, 0u, 1u) == 0u);
    }
    assert(heater_ctrl_set_enable(&c, 1u) == HEATER_CMD_OK);
    assert(heater_ctrl_step_10ms(&c, 200u, 0u, 1u) == PROBE_PULSE_TICKS); /* probe */
    /* F210 tolerates 14 A (1650 + 924 = 2574 mV -> 3194 raw) and trips > 15 A (3500 raw) */
    assert(heater_ctrl_check_pulse_current(&c, 3194u) == 0);
    duty = heater_ctrl_step_10ms(&c, 200u, 0u, 1u);
    assert(duty > 0u && duty <= MAX_DUTY_F210);
    assert(heater_ctrl_check_pulse_current(&c, 3500u) == 1);
    assert(heater_ctrl_clear_fault(&c) == HEATER_CMD_OK);

    /* 8. Over-temperature > 480 C guard */
    assert(heater_ctrl_set_enable(&c, 1u) == HEATER_CMD_OK);
    /* F210 1400 raw > 500 C -> hard overtemp cutoff */
    for (uint32_t i = 0u; i < 20u; ++i) {
        (void)heater_ctrl_step_10ms(&c, 1400u, 0u, 1u);
    }
    assert((c.fault_mask & FAULT_OVERTEMP) != 0u);
    assert(c.armed == 0u);
    assert(heater_ctrl_clear_fault(&c) == HEATER_CMD_OK);

    /* 9. Thermal runaway guard (800 ticks at high duty with frozen 150 C tip) */
    (void)heater_ctrl_step_10ms(&c, 276u, 0u, 1u); /* stabilize EMA around 150 C */
    for (uint32_t i = 0u; i < 25u; ++i) {
        (void)heater_ctrl_step_10ms(&c, 276u, 0u, 1u);
    }
    assert(heater_ctrl_set_enable(&c, 1u) == HEATER_CMD_OK);
    (void)heater_ctrl_step_10ms(&c, 276u, 0u, 1u); /* consume probe pulse */
    for (uint32_t i = 0u; i < RUNAWAY_MAX_TICKS + 10u; ++i) {
        (void)heater_ctrl_step_10ms(&c, 276u, 0u, 1u);
    }
    assert((c.fault_mask & FAULT_RUNAWAY) != 0u);
    assert(c.armed == 0u);
    assert(heater_ctrl_clear_fault(&c) == HEATER_CMD_OK);

    /* 10. Auto-zero current tracking when OFF & 30 s sustained > 5 A overload */
    for (uint32_t i = 0u; i < 40u; ++i) {
        heater_ctrl_sample_current_slot5(&c, 2038u, 0u); /* 1642 mV resting zero */
    }
    assert(c.cur_zero_mv < 1650u && c.cur_zero_mv >= 1640u);
    for (uint32_t i = 0u; i < OVERLOAD_MAX_TICKS + 5u; ++i) {
        heater_ctrl_sample_current_slot5(&c, 2550u, 1u); /* > 5 A while ON */
    }
    assert((c.fault_mask & FAULT_OVERLOAD) != 0u);
    assert(c.armed == 0u);

    /* 11. Target-reached one-shot ready beep notification (+/- 3.0 C band) */
    heater_ready_notify_t rn;
    heater_ready_notify_init(&rn);
    heater_status_t st;
    st.state = HEATER_STANDBY;
    st.active_target_c = 300u;
    st.tip_deci_c = 3000;
    assert(heater_ready_notify_step(&rn, &st) == 0u); /* No beep in STANDBY */

    st.state = HEATER_HEATING;
    st.tip_deci_c = 2969; /* 296.9 C: 3.1 C below target */
    assert(heater_ready_notify_step(&rn, &st) == 0u);

    st.tip_deci_c = 2970; /* 297.0 C: within +/- 3.0 C READY band */
    assert(heater_ready_notify_step(&rn, &st) == 1u);

    st.tip_deci_c = 3000; /* Already announced for 300 C */
    assert(heater_ready_notify_step(&rn, &st) == 0u);
    st.tip_deci_c = 2950; /* Transient drop while soldering does not re-trigger */
    assert(heater_ready_notify_step(&rn, &st) == 0u);
    st.tip_deci_c = 3000;
    assert(heater_ready_notify_step(&rn, &st) == 0u);

    /* Changing target re-arms notification until new target is reached */
    st.active_target_c = 350u;
    assert(heater_ready_notify_step(&rn, &st) == 0u);
    st.tip_deci_c = 3480;
    assert(heater_ready_notify_step(&rn, &st) == 1u);
    assert(heater_ready_notify_step(&rn, &st) == 0u);

    /* Entering SLEEP resets notification so next wake-up beeps when ready */
    st.state = HEATER_SLEEP;
    st.active_target_c = 100u;
    st.tip_deci_c = 1000;
    assert(heater_ready_notify_step(&rn, &st) == 0u);

    st.state = HEATER_HEATING;
    st.active_target_c = 350u;
    st.tip_deci_c = 3475;
    assert(heater_ready_notify_step(&rn, &st) == 1u);

    /* 12. Repeating open-tip warning beep */
    heater_open_tip_alarm_t ota;
    heater_open_tip_alarm_init(&ota);
    assert(heater_open_tip_alarm_step(&ota, FAULT_NONE, 100u) == 0u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 200u) == 1u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 1699u) == 0u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 1700u) == 1u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 3199u) == 0u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 3200u) == 1u);
    /* Inserting tip resets alarm; removing later beeps immediately again */
    assert(heater_open_tip_alarm_step(&ota, FAULT_NONE, 3300u) == 0u);
    assert(heater_open_tip_alarm_step(&ota, FAULT_OPEN_TIP, 3400u) == 1u);

    printf("test_heater: PASS\n");
    return 0;
}
