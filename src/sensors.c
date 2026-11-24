#include "sensors.h"

#include "app_config.h"
#include "hw.h"

#include <stddef.h>

static sensor_state_t g_sensors;

uint32_t sensors_adc_to_mv(uint16_t raw) {
    if (raw > 4095u) {
        raw = 4095u;
    }
    return ((uint32_t)raw * 3300u + 2047u) / 4095u;
}

uint32_t sensors_rail_adc_to_mv(uint16_t aux_raw) {
    return sensors_adc_to_mv(aux_raw) * 16u;
}

int32_t sensors_cur_adc_to_ma_zero(uint16_t raw, uint16_t zero_mv) {
    int32_t mv = (int32_t)sensors_adc_to_mv(raw);
    return ((mv - (int32_t)zero_mv) * 1000) / 66;
}

int32_t sensors_cur_adc_to_ma(uint16_t raw) {
    return sensors_cur_adc_to_ma_zero(raw, 1650u);
}

int32_t sensors_tip_adc_to_deci_c_3pt(uint16_t raw, uint8_t is_f245, const int16_t cal_dc[3]) {
    /*
     * Stock APP_DWS_200_V1.5 (0x0800FD84 Temp_Read + 0x0801243E +25 C CJ) 5-segment LUT:
     *   F245 (handle_type=1): (0,0), (1100,150+c0), (1870,250+c1), (2610,350+c2), (3340,450+c2)
     *   F210 (handle_type=0): (0,0), ( 276,150+c0), ( 608,250+c1), ( 882,350+c2), (1210,450+c2)
     * Stock then adds the fixed +25.0 C (+250 deci-C) cold-junction baseline to the result.
     */
    static const uint16_t f245_x[5] = { 0u, 1100u, 1870u, 2610u, 3340u };
    static const uint16_t f210_x[5] = { 0u,  276u,  608u,  882u, 1210u };

    const int32_t c0 = (cal_dc != NULL) ? (int32_t)cal_dc[0] : 0;
    const int32_t c1 = (cal_dc != NULL) ? (int32_t)cal_dc[1] : 0;
    const int32_t c2 = (cal_dc != NULL) ? (int32_t)cal_dc[2] : 0;
    const int32_t lut_y[5] = {
        0,
        1500 + c0,
        2500 + c1,
        3500 + c2,
        4500 + c2
    };

    if (raw > 4095u) {
        raw = 4095u;
    }

    const uint16_t *kx = (is_f245 != 0u) ? f245_x : f210_x;
    int32_t dc;

    if (raw >= kx[4]) {
        int32_t dx = (int32_t)(kx[4] - kx[3]);
        int32_t dy = lut_y[4] - lut_y[3];
        int32_t num = ((int32_t)raw - (int32_t)kx[4]) * dy;
        dc = lut_y[4] + (num + (dx / 2)) / dx;
    } else {
        uint32_t seg = 0u;
        while (seg < 3u && raw > kx[seg + 1u]) {
            seg++;
        }
        int32_t dx = (int32_t)(kx[seg + 1u] - kx[seg]);
        int32_t dy = lut_y[seg + 1u] - lut_y[seg];
        int32_t num = ((int32_t)raw - (int32_t)kx[seg]) * dy;
        dc = lut_y[seg] + (num + (dx / 2)) / dx;
    }

    dc += TIP_COLD_JUNCTION_DECI_C;
    if (dc < 0) {
        dc = 0;
    } else if (dc > 9999) {
        dc = 9999;
    }
    return dc;
}

int32_t sensors_tip_adc_to_deci_c(uint16_t raw, uint8_t is_f245, int16_t cal_offset_deci_c) {
    const int16_t cal[3] = { cal_offset_deci_c, cal_offset_deci_c, cal_offset_deci_c };
    return sensors_tip_adc_to_deci_c_3pt(raw, is_f245, cal);
}

void sensors_filter_init(sensor_filter_t *f) {
    for (uint32_t i = 0u; i < SENSOR_FILTER_LEN; ++i) {
        f->samples[i] = 0u;
    }
    f->sum = 0u;
    f->index = 0u;
    f->count = 0u;
}

void sensors_filter_push(sensor_filter_t *f, uint16_t sample) {
    if (sample > 4095u) {
        sample = 4095u;
    }
    if (f->count < SENSOR_FILTER_LEN) {
        f->samples[f->index] = sample;
        f->sum += sample;
        f->count++;
    } else {
        f->sum -= f->samples[f->index];
        f->samples[f->index] = sample;
        f->sum += sample;
    }
    f->index = (uint8_t)((f->index + 1u) % SENSOR_FILTER_LEN);
}

uint16_t sensors_filter_avg(const sensor_filter_t *f) {
    if (f->count == 0u) {
        return 0u;
    }
    return (uint16_t)((f->sum + (uint32_t)(f->count / 2u)) / (uint32_t)f->count);
}

void sensors_state_init(sensor_state_t *st) {
    sensors_filter_init(&st->tip_f);
    sensors_filter_init(&st->cur_f);
    sensors_filter_init(&st->aux_f);
    st->snap.tip_raw = 0u;
    st->snap.tip_avg = 0u;
    st->snap.tip_mv = 0u;
    st->snap.cur_raw = 0u;
    st->snap.cur_avg = 0u;
    st->snap.cur_mv = 0u;
    st->snap.cur_ma = 0;
    st->snap.aux_raw = 0u;
    st->snap.aux_avg = 0u;
    st->snap.aux_mv = 0u;
    st->snap.rail_mv = 0u;
    st->snap.hist_head = 0u;
    st->snap.hist_count = 0u;
    for (uint32_t i = 0u; i < SENSOR_HISTORY_LEN; ++i) {
        st->snap.tip_hist[i] = 0u;
        st->snap.cur_hist[i] = 0u;
        st->snap.aux_hist[i] = 0u;
        st->snap.temp_hist[i] = 0u;
        st->snap.pwr_hist[i] = 0u;
    }
}

void sensors_update_snapshot(
    sensor_state_t *st,
    uint16_t tip_raw,
    uint16_t cur_raw,
    uint16_t aux_raw,
    sensor_snapshot_t *out
) {
    if (tip_raw > 4095u) tip_raw = 4095u;
    if (cur_raw > 4095u) cur_raw = 4095u;
    if (aux_raw > 4095u) aux_raw = 4095u;

    sensors_filter_push(&st->tip_f, tip_raw);
    sensors_filter_push(&st->cur_f, cur_raw);
    sensors_filter_push(&st->aux_f, aux_raw);

    st->snap.tip_raw = tip_raw;
    st->snap.tip_avg = sensors_filter_avg(&st->tip_f);
    st->snap.tip_mv = sensors_adc_to_mv(st->snap.tip_avg);

    st->snap.cur_raw = cur_raw;
    st->snap.cur_avg = sensors_filter_avg(&st->cur_f);
    st->snap.cur_mv = sensors_adc_to_mv(st->snap.cur_avg);
    st->snap.cur_ma = sensors_cur_adc_to_ma(st->snap.cur_avg);

    st->snap.aux_raw = aux_raw;
    st->snap.aux_avg = sensors_filter_avg(&st->aux_f);
    st->snap.aux_mv = sensors_adc_to_mv(st->snap.aux_avg);
    st->snap.rail_mv = sensors_rail_adc_to_mv(st->snap.aux_avg);

    st->snap.tip_hist[st->snap.hist_head] = st->snap.tip_avg;
    st->snap.cur_hist[st->snap.hist_head] = st->snap.cur_avg;
    st->snap.aux_hist[st->snap.hist_head] = st->snap.aux_avg;
    st->snap.hist_head = (uint8_t)((st->snap.hist_head + 1u) % SENSOR_HISTORY_LEN);
    if (st->snap.hist_count < SENSOR_HISTORY_LEN) {
        st->snap.hist_count++;
    }

    if (out != NULL) {
        *out = st->snap;
    }
}

void sensors_record_temp_c(sensor_state_t *st, uint16_t temp_c, sensor_snapshot_t *out) {
    if (st == NULL) return;
    if (temp_c > 500u) temp_c = 500u;
    /* Store into the most recently pushed slot (hist_head - 1) */
    uint8_t idx = (st->snap.hist_head == 0u)
        ? (uint8_t)(SENSOR_HISTORY_LEN - 1u)
        : (uint8_t)(st->snap.hist_head - 1u);
    st->snap.temp_hist[idx] = temp_c;
    if (out != NULL) {
        out->temp_hist[idx] = temp_c;
    }
}

void sensors_record_power_pct(sensor_state_t *st, uint8_t pwr_pct, sensor_snapshot_t *out) {
    if (st == NULL) return;
    if (pwr_pct > 100u) pwr_pct = 100u;
    /* Store into the most recently pushed slot (hist_head - 1) */
    uint8_t idx = (st->snap.hist_head == 0u)
        ? (uint8_t)(SENSOR_HISTORY_LEN - 1u)
        : (uint8_t)(st->snap.hist_head - 1u);
    st->snap.pwr_hist[idx] = pwr_pct;
    if (out != NULL) {
        out->pwr_hist[idx] = pwr_pct;
    }
}

#ifndef OPENDWS_HOST_TEST

void sensors_init(void) {
    sensors_state_init(&g_sensors);

    CRM_APB2EN |= CRM_APB2_ADC1 | CRM_APB2_GPIOA | CRM_APB2_GPIOC;

    /* Analog inputs: PC0 (CH10), PC1 (CH11), PA3 (CH3) */
    gpio_config_pin(GPIOC_BASE, 0u, GPIO_MODE_ANALOG, 0u);
    gpio_config_pin(GPIOC_BASE, 1u, GPIO_MODE_ANALOG, 0u);
    gpio_config_pin(GPIOA_BASE, 3u, GPIO_MODE_ANALOG, 0u);

    ADC_CTRL1(ADC1_BASE) = 0u;
    ADC_CTRL2(ADC1_BASE) = (7u << 17) | ADC_CTRL2_OCTEN | ADC_CTRL2_ADCEN;
    for (volatile uint32_t i = 0u; i < 1000u; ++i) {
        __asm__ volatile("nop");
    }

    /* Reset and run ADC1 calibration */
    ADC_CTRL2(ADC1_BASE) |= ADC_CTRL2_ADCALINIT;
    for (uint32_t t = 0u; t < 100000u && (ADC_CTRL2(ADC1_BASE) & ADC_CTRL2_ADCALINIT) != 0u; ++t) {
    }
    ADC_CTRL2(ADC1_BASE) |= ADC_CTRL2_ADCAL;
    for (uint32_t t = 0u; t < 100000u && (ADC_CTRL2(ADC1_BASE) & ADC_CTRL2_ADCAL) != 0u; ++t) {
    }
}

uint16_t sensors_read_channel(uint8_t ch, uint8_t sample_time) {
    uint32_t spt = (uint32_t)(sample_time & 0x7u);
    if (ch >= 10u) {
        uint32_t shift = (uint32_t)(ch - 10u) * 3u;
        ADC_SPT1(ADC1_BASE) = (ADC_SPT1(ADC1_BASE) & ~(0x7u << shift)) | (spt << shift);
    } else {
        uint32_t shift = (uint32_t)ch * 3u;
        ADC_SPT2(ADC1_BASE) = (ADC_SPT2(ADC1_BASE) & ~(0x7u << shift)) | (spt << shift);
    }

    ADC_OSQ1(ADC1_BASE) = 0u;
    ADC_OSQ3(ADC1_BASE) = (uint32_t)(ch & 0x1Fu);
    ADC_STS(ADC1_BASE) = 0u;
    ADC_CTRL2(ADC1_BASE) |= ADC_CTRL2_OCSWTRG;

    for (uint32_t t = 0u; t < 50000u; ++t) {
        if ((ADC_STS(ADC1_BASE) & ADC_STS_OCCE) != 0u) {
            return (uint16_t)(ADC_ODT(ADC1_BASE) & 0x0FFFu);
        }
    }
    return 0u;
}

void sensors_sample_all(sensor_snapshot_t *out) {
    /* Sample times match stock firmware: CH10=7 (239.5 cyc), CH11/CH3=5 (55.5 cyc) */
    uint16_t tip = sensors_read_channel(ADC_CH_TIP, 7u);
    uint16_t cur = sensors_read_channel(ADC_CH_CUR, 5u);
    uint16_t aux = sensors_read_channel(ADC_CH_AUX, 5u);
    sensors_update_snapshot(&g_sensors, tip, cur, aux, out);
}

#else

void sensors_init(void) {
    sensors_state_init(&g_sensors);
}

uint16_t sensors_read_channel(uint8_t ch, uint8_t sample_time) {
    (void)sample_time;
    if (ch == ADC_CH_CUR) return 2048u;
    if (ch == ADC_CH_AUX) return 1500u;
    return 120u;
}

void sensors_sample_all(sensor_snapshot_t *out) {
    sensors_update_snapshot(&g_sensors, 120u, 2048u, 1500u, out);
}

#endif /* OPENDWS_HOST_TEST */
