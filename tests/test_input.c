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

    printf("test_input: PASS\n");
    return 0;
}
