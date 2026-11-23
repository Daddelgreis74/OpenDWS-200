#include "input.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    input_debouncer_t db;
    input_state_t st;
    input_debouncer_init(&db);

    /* Short glitch (< 20 ms) must not register as a press */
    input_update_from_raw(&db, KEY_BIT_PD2, 0u, 0u, 1u, 1u, 0u, 10u, &st);
    assert(st.held_mask == 0u && st.pressed_mask == 0u);
    input_update_from_raw(&db, KEY_BIT_PD2, 0u, 0u, 1u, 1u, 0u, 25u, &st);
    assert(st.held_mask == 0u && st.pressed_mask == 0u);
    input_update_from_raw(&db, 0u, 0u, 0u, 1u, 1u, 0u, 26u, &st);
    assert(st.held_mask == 0u);

    /* Stable press (>= 20 ms) fires pressed_mask once */
    input_update_from_raw(&db, KEY_BIT_PB3, 1u, 42u, 0u, 1u, 1u, 100u, &st);
    assert(st.held_mask == 0u);
    assert(st.pb5_sleep == 1u && st.pb5_edges == 42u && st.pb6_handle == 0u);

    input_update_from_raw(&db, KEY_BIT_PB3, 1u, 42u, 0u, 1u, 1u, 120u, &st);
    assert(st.held_mask == KEY_BIT_PB3);
    assert(st.pressed_mask == KEY_BIT_PB3);
    assert(st.long_mask == 0u);

    /* Next poll keeps held_mask but clears one-shot pressed_mask */
    input_update_from_raw(&db, KEY_BIT_PB3, 1u, 42u, 0u, 1u, 1u, 200u, &st);
    assert(st.held_mask == KEY_BIT_PB3);
    assert(st.pressed_mask == 0u);

    /* Holding >= 700 ms fires long_mask once */
    input_update_from_raw(&db, KEY_BIT_PB3, 1u, 42u, 0u, 1u, 1u, 820u, &st);
    assert(st.long_mask == KEY_BIT_PB3);
    input_update_from_raw(&db, KEY_BIT_PB3, 1u, 42u, 0u, 1u, 1u, 900u, &st);
    assert(st.long_mask == 0u);

    /* Release debounces over 20 ms */
    input_update_from_raw(&db, 0u, 0u, 43u, 1u, 0u, 0u, 910u, &st);
    assert(st.held_mask == KEY_BIT_PB3);
    input_update_from_raw(&db, 0u, 0u, 43u, 1u, 0u, 0u, 930u, &st);
    assert(st.held_mask == 0u);
    assert(st.released_mask == KEY_BIT_PB3);

    /* Soft-power PB0 state machine: boot held LOW stays WAIT_IDLE, then HIGH->LOW->HIGH triggers OFF */
    soft_power_t sp;
    soft_power_init(&sp);
    assert(soft_power_step(&sp, 0u, 100u) == 0u && sp.state == SOFT_PWR_WAIT_IDLE);
    assert(soft_power_step(&sp, 0u, 600u) == 0u && sp.state == SOFT_PWR_WAIT_IDLE);
    assert(soft_power_step(&sp, 1u, 610u) == 0u && sp.state == SOFT_PWR_WAIT_IDLE);
    assert(soft_power_step(&sp, 1u, 640u) == 0u && sp.state == SOFT_PWR_ARMED);

    /* Glitch < 30 ms must not arm power-off */
    assert(soft_power_step(&sp, 0u, 700u) == 0u);
    assert(soft_power_step(&sp, 1u, 710u) == 0u && sp.state == SOFT_PWR_ARMED);

    /* Real press (>= 30 ms) enters PRESSED, release (>= 20 ms) returns 1 (POWER OFF) */
    assert(soft_power_step(&sp, 0u, 800u) == 0u);
    assert(soft_power_step(&sp, 0u, 830u) == 0u && sp.state == SOFT_PWR_PRESSED);
    assert(soft_power_step(&sp, 1u, 900u) == 0u && sp.state == SOFT_PWR_PRESSED);
    assert(soft_power_step(&sp, 1u, 920u) == 1u && sp.state == SOFT_PWR_OFF);

    printf("test_input: PASS\n");
    return 0;
}
