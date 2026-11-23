#include "app_config.h"
#include "input.h"
#include "menu.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    opendws_settings_t s;
    settings_defaults(&s);
    assert(s.sleep_temp_c == 100u);

    menu_state_t m;
    menu_init(&m);
    assert(m.open == 0u);

    /* Non-PB4 key when closed does nothing */
    assert(menu_handle_key(&m, KEY_BIT_PD2, &s, 1u) == MENU_ACT_NONE);
    assert(m.open == 0u);

    /* PB4 opens settings menu */
    uint32_t act = menu_handle_key(&m, KEY_BIT_PB4, &s, 1u);
    assert((act & MENU_ACT_BEEP_OK) != 0u);
    assert(m.open == 1u && m.cursor == 0u && m.editing == 0u);

    /* PD2 enters edit mode on Sleep Temp (item 0), PB3 (+10) & PC12 (-10), clamped 50..250 */
    act = menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    assert(m.editing == 1u);
    act = menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert((act & MENU_ACT_CHANGED_SLEEP) != 0u);
    assert(s.sleep_temp_c == 110u);
    for (int i = 0; i < 10; ++i) {
        (void)menu_handle_key(&m, KEY_BIT_PC12, &s, 1u);
    }
    assert(s.sleep_temp_c == 0u); /* 50 C -> 0 (OFF 0W) */
    char sval[24];
    menu_format_value(MENU_ITEM_SLEEP_TEMP, &s, 1u, 0u, sval, sizeof(sval));
    assert(strcmp(sval, "OFF (0W)") == 0);
    (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert(s.sleep_temp_c == SLEEP_TEMP_MIN_C); /* back to 50 C */
    /* Exit edit mode via PC11 (Back), keep menu open */
    (void)menu_handle_key(&m, KEY_BIT_PC11, &s, 1u);
    assert(m.open == 1u && m.editing == 0u);

    /* Scroll down (PB3) to Sleep Stand (item 1) and toggle immediately with PD2 */
    (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert(m.cursor == MENU_ITEM_SLEEP_EN);
    act = menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    assert((act & MENU_ACT_CHANGED_SLEEP) != 0u);
    assert(s.sleep_en == 0u && m.editing == 0u);

    /* Step down to Standby Screen (item 2), verify label & PD2 toggle ON <-> OFF */
    (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert(m.cursor == MENU_ITEM_STANDBY_SCR);
    assert(strcmp(menu_item_label(MENU_ITEM_STANDBY_SCR, 1u), "Standby Screen") == 0);
    char sbval[16];
    menu_format_value(MENU_ITEM_STANDBY_SCR, &s, 1u, 0u, sbval, sizeof(sbval));
    assert(strcmp(sbval, "ON") == 0);
    (void)menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    assert(s.standby_screen_en == 0u && m.editing == 0u);
    menu_format_value(MENU_ITEM_STANDBY_SCR, &s, 1u, 0u, sbval, sizeof(sbval));
    assert(strcmp(sbval, "OFF") == 0);
    (void)menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    assert(s.standby_screen_en == 1u);
    (void)menu_handle_key(&m, KEY_BIT_PC12, &s, 1u); /* back to item 1 */

    /* Wrap-around UP (PC12) from cursor 1 -> 0 -> 10 (Save to Flash), verify scroll_top */
    (void)menu_handle_key(&m, KEY_BIT_PC12, &s, 1u);
    assert(m.cursor == 0u);
    (void)menu_handle_key(&m, KEY_BIT_PC12, &s, 1u);
    assert(m.cursor == MENU_ITEM_SAVE);
    assert(m.scroll_top == (MENU_ITEM_COUNT - MENU_VISIBLE_ROWS));

    /* PD2 on Save returns MENU_ACT_SAVE_FLASH */
    act = menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    assert((act & MENU_ACT_SAVE_FLASH) != 0u);

    /* Step down wraps 10 -> 0 */
    (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert(m.cursor == 0u && m.scroll_top == 0u);

    /* Jump to Cal (item 6) for F245 vs F210 */
    for (int i = 0; i < 6; ++i) {
        (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    }
    assert(m.cursor == MENU_ITEM_CAL);
    assert(strcmp(menu_item_label(MENU_ITEM_CAL, 1u), "Cal (F245)") == 0);
    assert(strcmp(menu_item_label(MENU_ITEM_CAL, 0u), "Cal (F210)") == 0);
    (void)menu_handle_key(&m, KEY_BIT_PD2, &s, 1u);
    act = menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert((act & MENU_ACT_CHANGED_CAL) != 0u);
    assert(s.cal_f245_deci_c == 10);

    char val[24];
    menu_format_value(MENU_ITEM_CAL, &s, 1u, 0u, val, sizeof(val));
    assert(strcmp(val, "+1 C") == 0);
    menu_format_value(MENU_ITEM_SAVE, &s, 1u, 1u, val, sizeof(val));
    assert(strcmp(val, "SAVED!") == 0);

    /* Backlight clamp check */
    s.backlight_pct = 100u;
    (void)menu_handle_key(&m, KEY_BIT_PD2, &s, 1u); /* exit Cal edit */
    (void)menu_handle_key(&m, KEY_BIT_PB3, &s, 1u); /* Backlight */
    assert(m.cursor == MENU_ITEM_BACKLIGHT);
    (void)menu_handle_key(&m, KEY_BIT_PD2, &s, 1u); /* edit Backlight */
    act = menu_handle_key(&m, KEY_BIT_PB3, &s, 1u);
    assert((act & MENU_ACT_CHANGED_BL) != 0u && s.backlight_pct == 100u);
    act = menu_handle_key(&m, KEY_BIT_PC12, &s, 1u);
    assert(s.backlight_pct == 90u);

    /* PB4 exits menu cleanly regardless of edit state */
    (void)menu_handle_key(&m, KEY_BIT_PB4, &s, 1u);
    assert(m.open == 0u && m.editing == 0u);

    printf("test_menu: PASS\n");
    return 0;
}
