#include "sensors.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* ADC mV conversion */
    assert(sensors_adc_to_mv(0u) == 0u);
    assert(sensors_adc_to_mv(2048u) == 1650u);
    assert(sensors_adc_to_mv(4095u) == 3300u);
    assert(sensors_adc_to_mv(5000u) == 3300u); /* clamped */

    /* Current sensor (66 mV/A, 1.65 V zero offset) */
    assert(sensors_cur_adc_to_ma(2048u) == 0);
    assert(sensors_cur_adc_to_ma(0u) == -25000);
    assert(sensors_cur_adc_to_ma(4095u) == 25000);
    assert(sensors_cur_adc_to_ma_zero(2038u, 1642u) == 0);

    /* 24 V rail monitor (PA3 * 16) */
    assert(sensors_rail_adc_to_mv(0u) == 0u);
    assert(sensors_rail_adc_to_mv(180u) == 145u * 16u);
    assert(sensors_rail_adc_to_mv(1780u) == 1434u * 16u);

    /* Thermocouple 5-segment piecewise LUTs (F245 & F210) */
    assert(sensors_tip_adc_to_deci_c(0u, 1u, 0) == 0);
    assert(sensors_tip_adc_to_deci_c(1100u, 1u, 0) == 1500);
    assert(sensors_tip_adc_to_deci_c(1870u, 1u, 0) == 2500);
    assert(sensors_tip_adc_to_deci_c(2610u, 1u, 0) == 3500);
    assert(sensors_tip_adc_to_deci_c(3340u, 1u, 0) == 4500);
    assert(sensors_tip_adc_to_deci_c(1870u, 1u, 50) == 2550); /* +5.0 C cal offset */

    assert(sensors_tip_adc_to_deci_c(0u, 0u, 0) == 0);
    assert(sensors_tip_adc_to_deci_c(276u, 0u, 0) == 1500);
    assert(sensors_tip_adc_to_deci_c(608u, 0u, 0) == 2500);
    assert(sensors_tip_adc_to_deci_c(882u, 0u, 0) == 3500);
    assert(sensors_tip_adc_to_deci_c(1210u, 0u, 0) == 4500);

    /* Moving-average filter (16 samples) */
    sensor_filter_t f;
    sensors_filter_init(&f);
    assert(sensors_filter_avg(&f) == 0u);
    for (uint16_t i = 0u; i < 16u; ++i) {
        sensors_filter_push(&f, 1000u);
    }
    assert(sensors_filter_avg(&f) == 1000u);

    for (uint16_t i = 0u; i < 16u; ++i) {
        sensors_filter_push(&f, 2000u);
    }
    assert(sensors_filter_avg(&f) == 2000u);

    /* Snapshot & history buffer wraparound */
    sensor_state_t st;
    sensor_snapshot_t snap;
    sensors_state_init(&st);
    for (uint16_t i = 0u; i < 70u; ++i) {
        sensors_update_snapshot(&st, (uint16_t)(100u + i), 2048u, 1500u, &snap);
        sensors_record_temp_c(&st, (uint16_t)(20u + i), &snap);
    }
    assert(snap.hist_count == SENSOR_HISTORY_LEN);
    assert(snap.hist_head == (70u % SENSOR_HISTORY_LEN));
    assert(snap.cur_ma == 0);
    assert(snap.rail_mv == sensors_rail_adc_to_mv(1500u));

    printf("test_sensors: PASS\n");
    return 0;
}
