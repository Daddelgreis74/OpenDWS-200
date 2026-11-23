#pragma once

#include <stdint.h>

#define SENSOR_FILTER_LEN  16u
#define SENSOR_HISTORY_LEN 64u

#define ADC_CH_TIP 10u /* PC0: Thermocouple amplifier */
#define ADC_CH_CUR 11u /* PC1: Heater current sensor (66 mV/A, 1.65V zero) */
#define ADC_CH_AUX 3u  /* PA3: Aux / cold-junction reference */

typedef struct {
    uint16_t samples[SENSOR_FILTER_LEN];
    uint32_t sum;
    uint8_t index;
    uint8_t count;
} sensor_filter_t;

typedef struct {
    uint16_t tip_raw;
    uint16_t tip_avg;
    uint32_t tip_mv;

    uint16_t cur_raw;
    uint16_t cur_avg;
    uint32_t cur_mv;
    int32_t cur_ma;

    uint16_t aux_raw;
    uint16_t aux_avg;
    uint32_t aux_mv;
    uint32_t rail_mv;

    uint16_t tip_hist[SENSOR_HISTORY_LEN];
    uint16_t cur_hist[SENSOR_HISTORY_LEN];
    uint16_t aux_hist[SENSOR_HISTORY_LEN];
    uint16_t temp_hist[SENSOR_HISTORY_LEN];
    uint8_t pwr_hist[SENSOR_HISTORY_LEN];
    uint8_t hist_head;
    uint8_t hist_count;
} sensor_snapshot_t;

typedef struct {
    sensor_filter_t tip_f;
    sensor_filter_t cur_f;
    sensor_filter_t aux_f;
    sensor_snapshot_t snap;
} sensor_state_t;

uint32_t sensors_adc_to_mv(uint16_t raw);
uint32_t sensors_rail_adc_to_mv(uint16_t aux_raw);
int32_t sensors_cur_adc_to_ma(uint16_t raw);
int32_t sensors_cur_adc_to_ma_zero(uint16_t raw, uint16_t zero_mv);
int32_t sensors_tip_adc_to_deci_c(uint16_t raw, uint8_t is_f245, int16_t cal_offset_deci_c);

void sensors_filter_init(sensor_filter_t *f);
void sensors_filter_push(sensor_filter_t *f, uint16_t sample);
uint16_t sensors_filter_avg(const sensor_filter_t *f);

void sensors_state_init(sensor_state_t *st);
void sensors_update_snapshot(
    sensor_state_t *st,
    uint16_t tip_raw,
    uint16_t cur_raw,
    uint16_t aux_raw,
    sensor_snapshot_t *out
);
void sensors_record_temp_c(sensor_state_t *st, uint16_t temp_c, sensor_snapshot_t *out);
void sensors_record_power_pct(sensor_state_t *st, uint8_t pwr_pct, sensor_snapshot_t *out);

void sensors_init(void);
uint16_t sensors_read_channel(uint8_t ch, uint8_t sample_time);
void sensors_sample_all(sensor_snapshot_t *out);
