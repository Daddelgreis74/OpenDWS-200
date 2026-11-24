#include "menu.h"

#include "app_config.h"
#include "fmt.h"
#include "input.h"

void menu_init(menu_state_t *m) {
    if (m == NULL) return;
    m->open = 0u;
    m->cursor = 0u;
    m->scroll_top = 0u;
    m->editing = 0u;
}

static void sync_scroll(menu_state_t *m) {
    if (m->cursor < m->scroll_top) {
        m->scroll_top = m->cursor;
    } else if (m->cursor >= (uint8_t)(m->scroll_top + MENU_VISIBLE_ROWS)) {
        m->scroll_top = (uint8_t)(m->cursor - MENU_VISIBLE_ROWS + 1u);
    }
    if (m->scroll_top > (uint8_t)(MENU_ITEM_COUNT - MENU_VISIBLE_ROWS)) {
        m->scroll_top = (uint8_t)(MENU_ITEM_COUNT - MENU_VISIBLE_ROWS);
    }
}

static uint16_t step_u16(uint16_t val, int16_t delta, uint16_t lo, uint16_t hi) {
    int32_t next = (int32_t)val + (int32_t)delta;
    if (next < (int32_t)lo) next = (int32_t)lo;
    if (next > (int32_t)hi) next = (int32_t)hi;
    return (uint16_t)next;
}

static uint8_t step_auto_off_min(uint8_t cur, int8_t dir) {
    static const uint8_t opts[6] = { 0u, 5u, 10u, 15u, 30u, 60u };
    int32_t idx = 0;
    for (int32_t i = 0; i < 6; ++i) {
        if (cur >= opts[i]) {
            idx = i;
        }
    }
    if (dir < 0 && idx > 0) {
        idx--;
    } else if (dir > 0 && idx < 5) {
        idx++;
    }
    return opts[idx];
}

static uint32_t adjust_item(menu_item_t item, opendws_settings_t *s, uint8_t is_f245, int8_t dir) {
    if (s == NULL) return MENU_ACT_NONE;
    switch (item) {
    case MENU_ITEM_SLEEP_TEMP:
        if (dir < 0) {
            if (s->sleep_temp_c <= SLEEP_TEMP_MIN_C) {
                s->sleep_temp_c = 0u;
            } else {
                s->sleep_temp_c = step_u16(s->sleep_temp_c, -10, (uint16_t)SLEEP_TEMP_MIN_C, (uint16_t)SLEEP_TEMP_MAX_C);
            }
        } else {
            if (s->sleep_temp_c == 0u) {
                s->sleep_temp_c = (uint16_t)SLEEP_TEMP_MIN_C;
            } else {
                s->sleep_temp_c = step_u16(s->sleep_temp_c, 10, (uint16_t)SLEEP_TEMP_MIN_C, (uint16_t)SLEEP_TEMP_MAX_C);
            }
        }
        return MENU_ACT_CHANGED_SLEEP;
    case MENU_ITEM_SLEEP_EN:
        s->sleep_en = s->sleep_en ? 0u : 1u;
        return MENU_ACT_CHANGED_SLEEP;
    case MENU_ITEM_STANDBY_SCR:
        s->standby_screen_en = s->standby_screen_en ? 0u : 1u;
        return MENU_ACT_NONE;
    case MENU_ITEM_AUTO_OFF:
        s->auto_off_min = step_auto_off_min(s->auto_off_min, dir);
        return MENU_ACT_CHANGED_SLEEP;
    case MENU_ITEM_PRESET1:
    case MENU_ITEM_PRESET2:
    case MENU_ITEM_PRESET3: {
        uint32_t idx = (uint32_t)(item - MENU_ITEM_PRESET1);
        s->presets_c[idx] = step_u16(s->presets_c[idx], (int16_t)(dir * 5), (uint16_t)TEMP_MIN_C, (uint16_t)TEMP_MAX_C);
        return MENU_ACT_CHANGED_PRESET;
    }
    case MENU_ITEM_CAL_150:
    case MENU_ITEM_CAL_250:
    case MENU_ITEM_CAL_350: {
        uint32_t cal_idx = (uint32_t)(item - MENU_ITEM_CAL_150);
        int16_t *cal_arr = is_f245 ? s->cal_f245_dc : s->cal_f210_dc;
        int32_t cur_c = (int32_t)cal_arr[cal_idx] / 10;
        cur_c += (int32_t)dir;
        if (cur_c < CAL_OFFSET_MIN_C) cur_c = CAL_OFFSET_MIN_C;
        if (cur_c > CAL_OFFSET_MAX_C) cur_c = CAL_OFFSET_MAX_C;
        cal_arr[cal_idx] = (int16_t)(cur_c * 10);
        return MENU_ACT_CHANGED_CAL;
    }
    case MENU_ITEM_BACKLIGHT: {
        int32_t bl = (int32_t)s->backlight_pct + (int32_t)(dir * 10);
        if (bl < 10) bl = 10;
        if (bl > 100) bl = 100;
        s->backlight_pct = (uint8_t)bl;
        return MENU_ACT_CHANGED_BL;
    }
    case MENU_ITEM_BUZZER:
        s->buzzer_en = s->buzzer_en ? 0u : 1u;
        return MENU_ACT_NONE;
    case MENU_ITEM_STREAM:
        s->stream_enabled = s->stream_enabled ? 0u : 1u;
        return MENU_ACT_CHANGED_STREAM;
    case MENU_ITEM_SAVE:
    default:
        return MENU_ACT_NONE;
    }
}

uint32_t menu_handle_key(
    menu_state_t *m,
    uint8_t pressed_mask,
    opendws_settings_t *s,
    uint8_t is_f245
) {
    if (m == NULL || s == NULL || pressed_mask == 0u) {
        return MENU_ACT_NONE;
    }

    if (m->open == 0u) {
        if ((pressed_mask & KEY_BIT_PB4) != 0u) {
            m->open = 1u;
            m->editing = 0u;
            sync_scroll(m);
            return MENU_ACT_BEEP_OK;
        }
        return MENU_ACT_NONE;
    }

    /* MENU button (PB4) exits menu immediately */
    if ((pressed_mask & KEY_BIT_PB4) != 0u) {
        m->open = 0u;
        m->editing = 0u;
        return MENU_ACT_BEEP_NAV;
    }

    /* CH / Return button (PC11) leaves edit mode or closes menu */
    if ((pressed_mask & KEY_BIT_PC11) != 0u) {
        if (m->editing != 0u) {
            m->editing = 0u;
        } else {
            m->open = 0u;
        }
        return MENU_ACT_BEEP_NAV;
    }

    menu_item_t item = (menu_item_t)m->cursor;

    /* MODE / OK button (PD2): Execute Save, toggle booleans, or toggle edit mode */
    if ((pressed_mask & KEY_BIT_PD2) != 0u) {
        if (item == MENU_ITEM_SAVE) {
            m->editing = 0u;
            return MENU_ACT_SAVE_FLASH | MENU_ACT_BEEP_OK;
        }
        if (item == MENU_ITEM_SLEEP_EN || item == MENU_ITEM_STANDBY_SCR ||
            item == MENU_ITEM_BUZZER || item == MENU_ITEM_STREAM) {
            m->editing = 0u;
            return adjust_item(item, s, is_f245, 1) | MENU_ACT_BEEP_OK;
        }
        m->editing = m->editing ? 0u : 1u;
        return MENU_ACT_BEEP_OK;
    }

    /* Left (<, PC12): Scroll Up or decrement edited value */
    if ((pressed_mask & KEY_BIT_PC12) != 0u) {
        if (m->editing != 0u) {
            return adjust_item(item, s, is_f245, -1) | MENU_ACT_BEEP_NAV;
        }
        if (m->cursor == 0u) {
            m->cursor = (uint8_t)(MENU_ITEM_COUNT - 1u);
        } else {
            m->cursor--;
        }
        sync_scroll(m);
        return MENU_ACT_BEEP_NAV;
    }

    /* Right (>, PB3): Scroll Down or increment edited value */
    if ((pressed_mask & KEY_BIT_PB3) != 0u) {
        if (m->editing != 0u) {
            return adjust_item(item, s, is_f245, 1) | MENU_ACT_BEEP_NAV;
        }
        m->cursor = (uint8_t)((m->cursor + 1u) % (uint8_t)MENU_ITEM_COUNT);
        sync_scroll(m);
        return MENU_ACT_BEEP_NAV;
    }

    return MENU_ACT_NONE;
}

const char *menu_item_label(menu_item_t item, uint8_t is_f245) {
    switch (item) {
    case MENU_ITEM_SLEEP_TEMP:  return "Sleep Temp";
    case MENU_ITEM_SLEEP_EN:    return "Auto Stand";
    case MENU_ITEM_STANDBY_SCR: return "Standby Screen";
    case MENU_ITEM_AUTO_OFF:    return "Auto Off";
    case MENU_ITEM_PRESET1:    return "Preset CH1";
    case MENU_ITEM_PRESET2:    return "Preset CH2";
    case MENU_ITEM_PRESET3:    return "Preset CH3";
    case MENU_ITEM_CAL_150:    return is_f245 ? "Cal 150 (F245)" : "Cal 150 (F210)";
    case MENU_ITEM_CAL_250:    return is_f245 ? "Cal 250 (F245)" : "Cal 250 (F210)";
    case MENU_ITEM_CAL_350:    return is_f245 ? "Cal 350 (F245)" : "Cal 350 (F210)";
    case MENU_ITEM_BACKLIGHT:  return "Backlight";
    case MENU_ITEM_BUZZER:     return "Buzzer";
    case MENU_ITEM_STREAM:     return "USB Stream";
    case MENU_ITEM_SAVE:       return "Save to Flash";
    default:                   return "";
    }
}

void menu_format_value(
    menu_item_t item,
    const opendws_settings_t *s,
    uint8_t is_f245,
    uint8_t saved_toast,
    char *buf,
    size_t cap
) {
    if (buf == NULL || cap == 0u) return;
    buf[0] = '\0';
    if (s == NULL) return;

    char num[16];
    size_t p = 0u;

    switch (item) {
    case MENU_ITEM_SLEEP_TEMP:
        if (s->sleep_temp_c == 0u) {
            (void)fmt_append_str(buf, p, cap, "OFF (0W)");
        } else {
            fmt_uint_pad(num, sizeof(num), s->sleep_temp_c, 1u, '0');
            p = fmt_append_str(buf, p, cap, num);
            (void)fmt_append_str(buf, p, cap, " C");
        }
        break;
    case MENU_ITEM_SLEEP_EN:
        (void)fmt_append_str(buf, p, cap, s->sleep_en ? "ON" : "OFF");
        break;
    case MENU_ITEM_STANDBY_SCR:
        (void)fmt_append_str(buf, p, cap, s->standby_screen_en ? "ON" : "OFF");
        break;
    case MENU_ITEM_AUTO_OFF:
        if (s->auto_off_min == 0u) {
            (void)fmt_append_str(buf, p, cap, "OFF");
        } else {
            fmt_uint_pad(num, sizeof(num), s->auto_off_min, 1u, '0');
            p = fmt_append_str(buf, p, cap, num);
            (void)fmt_append_str(buf, p, cap, " min");
        }
        break;
    case MENU_ITEM_PRESET1:
    case MENU_ITEM_PRESET2:
    case MENU_ITEM_PRESET3: {
        uint32_t idx = (uint32_t)(item - MENU_ITEM_PRESET1);
        fmt_uint_pad(num, sizeof(num), s->presets_c[idx], 1u, '0');
        p = fmt_append_str(buf, p, cap, num);
        (void)fmt_append_str(buf, p, cap, " C");
        break;
    }
    case MENU_ITEM_CAL_150:
    case MENU_ITEM_CAL_250:
    case MENU_ITEM_CAL_350: {
        uint32_t cal_idx = (uint32_t)(item - MENU_ITEM_CAL_150);
        const int16_t *cal_arr = is_f245 ? s->cal_f245_dc : s->cal_f210_dc;
        int32_t cal_c = (int32_t)cal_arr[cal_idx] / 10;
        fmt_int_pad(num, sizeof(num), cal_c, 1u, '0', 1u);
        p = fmt_append_str(buf, p, cap, num);
        (void)fmt_append_str(buf, p, cap, " C");
        break;
    }
    case MENU_ITEM_BACKLIGHT:
        fmt_uint_pad(num, sizeof(num), s->backlight_pct, 1u, '0');
        p = fmt_append_str(buf, p, cap, num);
        (void)fmt_append_str(buf, p, cap, " %");
        break;
    case MENU_ITEM_BUZZER:
        (void)fmt_append_str(buf, p, cap, s->buzzer_en ? "ON" : "OFF");
        break;
    case MENU_ITEM_STREAM:
        (void)fmt_append_str(buf, p, cap, s->stream_enabled ? "ON" : "OFF");
        break;
    case MENU_ITEM_SAVE:
        (void)fmt_append_str(buf, p, cap, saved_toast ? "SAVED!" : "[PRESS OK]");
        break;
    default:
        break;
    }
}
