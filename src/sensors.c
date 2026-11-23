#include "sensors.h"

#include "hw.h"

#include <stddef.h>

static sensor_state_t g_sensors;

uint32_t sensors_adc_to_mv(uint16_t raw) {
    if (raw > 4095u) {
        raw = 4095u;
    }
    return ((uint32_t)raw * 3300u + 2047u) / 4095u;
}

int32_t sensors_cur_adc_to_ma(uint16_t raw) {
    int32_t mv = (int32_t)sensors_adc_to_mv(raw);
    return ((mv - 1650) * 1000) / 66;
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
    st->snap.hist_head = 0u;
    st->snap.hist_count = 0u;
    for (uint32_t i = 0u; i < SENSOR_HISTORY_LEN; ++i) {
        st->snap.tip_hist[i] = 0u;
        st->snap.cur_hist[i] = 0u;
        st->snap.aux_hist[i] = 0u;
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
