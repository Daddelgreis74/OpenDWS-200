#include "input.h"

#include "hw.h"

static input_debouncer_t g_debouncer;
static volatile uint32_t g_pb5_edge_count = 0u;

void input_debouncer_init(input_debouncer_t *db) {
    db->stable_mask = 0u;
    db->last_raw_mask = 0u;
    db->long_fired_mask = 0u;
    for (uint32_t i = 0u; i < KEY_COUNT; ++i) {
        db->change_ms[i] = 0u;
        db->press_ms[i] = 0u;
    }
}

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
) {
    uint8_t pressed = 0u;
    uint8_t released = 0u;
    uint8_t long_ev = 0u;

    raw_keys_mask &= 0x1Fu;

    for (uint32_t i = 0u; i < KEY_COUNT; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        uint8_t raw_on = (raw_keys_mask & bit) != 0u ? 1u : 0u;
        uint8_t prev_raw = (db->last_raw_mask & bit) != 0u ? 1u : 0u;
        uint8_t cur_stable = (db->stable_mask & bit) != 0u ? 1u : 0u;

        if (raw_on != prev_raw) {
            db->change_ms[i] = now_ms;
            if (raw_on) {
                db->last_raw_mask |= bit;
            } else {
                db->last_raw_mask &= (uint8_t)~bit;
            }
        } else if (raw_on != cur_stable && (uint32_t)(now_ms - db->change_ms[i]) >= INPUT_DEBOUNCE_MS) {
            if (raw_on) {
                db->stable_mask |= bit;
                db->press_ms[i] = now_ms;
                db->long_fired_mask &= (uint8_t)~bit;
                pressed |= bit;
            } else {
                db->stable_mask &= (uint8_t)~bit;
                db->long_fired_mask &= (uint8_t)~bit;
                released |= bit;
            }
        }

        if ((db->stable_mask & bit) != 0u && (db->long_fired_mask & bit) == 0u) {
            if ((uint32_t)(now_ms - db->press_ms[i]) >= INPUT_LONG_PRESS_MS) {
                db->long_fired_mask |= bit;
                long_ev |= bit;
            }
        }
    }

    out->held_mask = db->stable_mask;
    out->pressed_mask = pressed;
    out->released_mask = released;
    out->long_mask = long_ev;
    out->raw_mask = raw_keys_mask;
    out->pb5_sleep = pb5_sleep ? 1u : 0u;
    out->pb5_edges = pb5_edges;
    out->pb6_handle = pb6_handle ? 1u : 0u;
    out->pb0_raw = pb0_raw ? 1u : 0u;
    out->pb2_raw = pb2_raw ? 1u : 0u;
}

#ifndef OPENDWS_HOST_TEST

void EXINT9_5_IRQHandler(void) {
    if ((EXINT_INTSTS & (1u << 5)) != 0u) {
        g_pb5_edge_count++;
        EXINT_INTSTS = (1u << 5);
    }
}

void input_init(void) {
    input_debouncer_init(&g_debouncer);
    g_pb5_edge_count = 0u;

    /* 5 front buttons: Active-LOW with Pull-Up */
    gpio_config_pin(GPIOD_BASE, 2u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOB_BASE, 3u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOB_BASE, 4u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOC_BASE, 12u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOC_BASE, 11u, GPIO_MODE_IN_PULL, 1u);

    /* Handle ID (PB6 Pull-Up), Diag pins (PB0 Pull-Up, PB2 Pull-Down) */
    gpio_config_pin(GPIOB_BASE, 6u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOB_BASE, 0u, GPIO_MODE_IN_PULL, 1u);
    gpio_config_pin(GPIOB_BASE, 2u, GPIO_MODE_IN_PULL, 0u);

    /* Sleep stand (PB5 Pull-Down) + EXINT5 on Port B (rising + falling) */
    gpio_config_pin(GPIOB_BASE, 5u, GPIO_MODE_IN_PULL, 0u);
    IOMUX_EXINTC2 = (IOMUX_EXINTC2 & ~(0xFu << 4)) | (1u << 4);
    EXINT_POLCFG1 |= (1u << 5);
    EXINT_POLCFG2 |= (1u << 5);
    EXINT_INTSTS = (1u << 5);
    EXINT_INTEN |= (1u << 5);
    irq_set_priority(EXINT9_5_IRQn, 0x90u);
    irq_enable(EXINT9_5_IRQn);
}

void input_poll(uint32_t now_ms, input_state_t *out) {
    uint32_t idt_b = GPIO_IDT(GPIOB_BASE);
    uint32_t idt_c = GPIO_IDT(GPIOC_BASE);
    uint32_t idt_d = GPIO_IDT(GPIOD_BASE);

    uint8_t raw_keys = 0u;
    if ((idt_d & (1u << 2)) == 0u)  raw_keys |= KEY_BIT_PD2;
    if ((idt_b & (1u << 3)) == 0u)  raw_keys |= KEY_BIT_PB3;
    if ((idt_b & (1u << 4)) == 0u)  raw_keys |= KEY_BIT_PB4;
    if ((idt_c & (1u << 12)) == 0u) raw_keys |= KEY_BIT_PC12;
    if ((idt_c & (1u << 11)) == 0u) raw_keys |= KEY_BIT_PC11;

    uint8_t pb5 = (idt_b & (1u << 5)) != 0u ? 1u : 0u;
    uint8_t pb6 = (idt_b & (1u << 6)) != 0u ? 1u : 0u;
    uint8_t pb0 = (idt_b & (1u << 0)) != 0u ? 1u : 0u;
    uint8_t pb2 = (idt_b & (1u << 2)) != 0u ? 1u : 0u;

    input_update_from_raw(
        &g_debouncer,
        raw_keys,
        pb5,
        g_pb5_edge_count,
        pb6,
        pb0,
        pb2,
        now_ms,
        out
    );
}

#else

void input_init(void) {
    input_debouncer_init(&g_debouncer);
    g_pb5_edge_count = 0u;
}

void input_poll(uint32_t now_ms, input_state_t *out) {
    input_update_from_raw(&g_debouncer, 0u, 0u, g_pb5_edge_count, 0u, 1u, 0u, now_ms, out);
}

#endif /* OPENDWS_HOST_TEST */
