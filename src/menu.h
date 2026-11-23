#pragma once

#include "settings.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MENU_ITEM_SLEEP_TEMP  = 0,
    MENU_ITEM_SLEEP_EN    = 1,
    MENU_ITEM_STANDBY_SCR = 2,
    MENU_ITEM_AUTO_OFF    = 3,
    MENU_ITEM_PRESET1     = 4,
    MENU_ITEM_PRESET2     = 5,
    MENU_ITEM_PRESET3     = 6,
    MENU_ITEM_CAL         = 7,
    MENU_ITEM_BACKLIGHT   = 8,
    MENU_ITEM_BUZZER      = 9,
    MENU_ITEM_STREAM      = 10,
    MENU_ITEM_SAVE        = 11,
    MENU_ITEM_COUNT       = 12
} menu_item_t;

#define MENU_VISIBLE_ROWS 6u

#define MENU_ACT_NONE           0u
#define MENU_ACT_BEEP_NAV       (1u << 0)
#define MENU_ACT_BEEP_OK        (1u << 1)
#define MENU_ACT_CHANGED_SLEEP  (1u << 2)
#define MENU_ACT_CHANGED_PRESET (1u << 3)
#define MENU_ACT_CHANGED_CAL    (1u << 4)
#define MENU_ACT_CHANGED_BL     (1u << 5)
#define MENU_ACT_CHANGED_STREAM (1u << 6)
#define MENU_ACT_SAVE_FLASH     (1u << 7)

typedef struct {
    uint8_t open;
    uint8_t cursor;
    uint8_t scroll_top;
    uint8_t editing;
} menu_state_t;

void menu_init(menu_state_t *m);
uint32_t menu_handle_key(
    menu_state_t *m,
    uint8_t pressed_mask,
    opendws_settings_t *s,
    uint8_t is_f245
);
const char *menu_item_label(menu_item_t item, uint8_t is_f245);
void menu_format_value(
    menu_item_t item,
    const opendws_settings_t *s,
    uint8_t is_f245,
    uint8_t saved_toast,
    char *buf,
    size_t cap
);
