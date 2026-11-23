#include "ui.h"

#include "app_config.h"
#include "fmt.h"
#include "gfx.h"

#define COL_BG          RGB565(11, 16, 27)
#define COL_CARD        RGB565(20, 29, 46)
#define COL_BORDER      RGB565(31, 45, 70)
#define COL_TEXT        RGB565(241, 245, 249)
#define COL_MUTED       RGB565(122, 140, 165)
#define COL_CYAN        RGB565(0, 229, 255)
#define COL_CYAN_DARK   RGB565(10, 51, 66)
#define COL_EMERALD     RGB565(16, 185, 129)
#define COL_GREEN_DARK  RGB565(12, 51, 38)
#define COL_AMBER       RGB565(245, 158, 11)
#define COL_AMBER_DARK  RGB565(64, 41, 8)
#define COL_ORANGE      RGB565(249, 115, 22)
#define COL_ORANGE_DARK RGB565(68, 28, 10)
#define COL_RED         RGB565(239, 68, 68)
#define COL_RED_DARK    RGB565(69, 20, 26)
#define COL_SPARK_BG    RGB565(13, 20, 33)

static uint8_t g_rendered_menu_mode = 0u;

static size_t append_fixed_1(char *buf, size_t pos, size_t cap, int32_t deci, uint8_t whole_w) {
    if (deci < 0) deci = 0;
    uint32_t whole = (uint32_t)deci / 10u;
    uint32_t frac = (uint32_t)deci % 10u;
    char tmp[16];
    fmt_uint_pad(tmp, sizeof(tmp), whole, whole_w, ' ');
    pos = fmt_append_str(buf, pos, cap, tmp);
    pos = fmt_append_str(buf, pos, cap, ".");
    fmt_uint_pad(tmp, sizeof(tmp), frac, 1u, '0');
    return fmt_append_str(buf, pos, cap, tmp);
}

static size_t append_ma_as_amps(char *buf, size_t pos, size_t cap, int32_t ma, uint8_t show_sign) {
    char tmp[8];
    if (ma < 0) {
        pos = fmt_append_str(buf, pos, cap, "-");
        ma = -ma;
    } else if (show_sign != 0u) {
        pos = fmt_append_str(buf, pos, cap, "+");
    } else {
        pos = fmt_append_str(buf, pos, cap, " ");
    }
    uint32_t uma = (uint32_t)ma;
    uint32_t amps = uma / 1000u;
    uint32_t c_amps = (uma % 1000u) / 10u;
    fmt_uint_pad(tmp, sizeof(tmp), amps, 2u, ' ');
    pos = fmt_append_str(buf, pos, cap, tmp);
    pos = fmt_append_str(buf, pos, cap, ".");
    fmt_uint_pad(tmp, sizeof(tmp), c_amps, 2u, '0');
    return fmt_append_str(buf, pos, cap, tmp);
}

static void pick_state_style(
    const heater_status_t *h,
    const char **label,
    uint16_t *fg,
    uint16_t *bg,
    uint16_t *border
) {
    if (h->state == HEATER_FAULT || (h->fault_mask & (uint16_t)~FAULT_SETTLING) != 0u) {
        *label = heater_fault_name(h->fault_mask);
        *fg = COL_RED;
        *bg = COL_RED_DARK;
        *border = COL_RED;
        return;
    }
    if (h->state == HEATER_HEATING) {
        int32_t diff = (int32_t)h->active_target_c * 10 - h->tip_deci_c;
        if (diff >= -30 && diff <= 30) {
            *label = "READY";
            *fg = COL_EMERALD;
            *bg = COL_GREEN_DARK;
            *border = COL_EMERALD;
        } else {
            *label = "HEATING";
            *fg = COL_ORANGE;
            *bg = COL_ORANGE_DARK;
            *border = COL_ORANGE;
        }
        return;
    }
    if (h->state == HEATER_SLEEP) {
        *label = "SLEEP";
        *fg = COL_CYAN;
        *bg = COL_CYAN_DARK;
        *border = COL_CYAN;
        return;
    }
    if (h->state == HEATER_PROBE) {
        *label = "PROBE";
        *fg = COL_CYAN;
        *bg = COL_CYAN_DARK;
        *border = COL_CYAN;
        return;
    }
    *label = "STANDBY";
    *fg = COL_AMBER;
    *bg = COL_AMBER_DARK;
    *border = COL_AMBER;
}

void ui_draw_static(void) {
    g_rendered_menu_mode = 0u;
    gfx_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, COL_BG);

    /* Top header bar */
    gfx_fill_rect(0u, 0u, LCD_WIDTH, 25u, COL_CARD);
    gfx_draw_hline(0u, 25u, LCD_WIDTH, COL_BORDER);
    gfx_draw_text(6u, 9u, "OpenDWS-200", COL_TEXT, COL_CARD, 1u);

    /* Hero card */
    gfx_fill_rect(6u, 30u, 308u, 92u, COL_CARD);
    gfx_draw_rect(6u, 30u, 308u, 92u, COL_BORDER);
    gfx_draw_vline(196u, 36u, 80u, COL_BORDER);
    gfx_draw_rect(204u, 47u, 102u, 11u, COL_BORDER);

    /* Middle telemetry + sparkline card */
    gfx_fill_rect(6u, 127u, 308u, 82u, COL_CARD);
    gfx_draw_rect(6u, 127u, 308u, 82u, COL_BORDER);
    gfx_draw_text(12u, 132u, "TEMP TRACE (0..480C)", COL_MUTED, COL_CARD, 1u);
    gfx_draw_rect(12u, 142u, 152u, 62u, COL_BORDER);

    /* Bottom 5-button bar */
    gfx_draw_hline(0u, 213u, LCD_WIDTH, COL_BORDER);
    gfx_fill_rect(0u, 214u, LCD_WIDTH, 26u, COL_BG);
}

static void ui_draw_menu_static(void) {
    g_rendered_menu_mode = 1u;
    gfx_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, COL_BG);

    /* Top header bar */
    gfx_fill_rect(0u, 0u, LCD_WIDTH, 25u, COL_CARD);
    gfx_draw_hline(0u, 25u, LCD_WIDTH, COL_BORDER);
    gfx_draw_text(6u, 9u, "SETTINGS", COL_CYAN, COL_CARD, 1u);

    /* Settings list card */
    gfx_fill_rect(6u, 30u, 308u, 178u, COL_CARD);
    gfx_draw_rect(6u, 30u, 308u, 178u, COL_BORDER);

    /* Bottom 5-button bar */
    gfx_draw_hline(0u, 213u, LCD_WIDTH, COL_BORDER);
    gfx_fill_rect(0u, 214u, LCD_WIDTH, 26u, COL_BG);
}

static void draw_padded_16(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg) {
    char buf[17];
    size_t i = 0u;
    if (s != NULL) {
        while (s[i] != '\0' && i < 16u) {
            buf[i] = s[i];
            i++;
        }
    }
    while (i < 16u) {
        buf[i++] = ' ';
    }
    buf[16] = '\0';
    gfx_draw_text(x, y, buf, fg, bg, 1u);
}

static void draw_padded_18_right(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg) {
    char buf[19];
    size_t len = 0u;
    if (s != NULL) {
        while (s[len] != '\0' && len < 18u) {
            len++;
        }
    }
    size_t pad = 18u - len;
    for (size_t i = 0u; i < pad; ++i) {
        buf[i] = ' ';
    }
    for (size_t i = 0u; i < len; ++i) {
        buf[pad + i] = s[i];
    }
    buf[18] = '\0';
    gfx_draw_text(x, y, buf, fg, bg, 1u);
}

static void ui_draw_menu_dynamic(const input_state_t *inp, const ui_status_t *st) {
    const heater_status_t *h = &st->hst;
    char line[32];
    char num[16];

    /* Top header live temp/state badge + scroll counter + handle */
    const char *st_label;
    uint16_t st_fg, st_bg, st_bd;
    pick_state_style(h, &st_label, &st_fg, &st_bg, &st_bd);
    if ((h->fault_mask & FAULT_OPEN_TIP) != 0u) {
        gfx_draw_badge(62u, 4u, 112u, 18u, "OPEN! NO TIP", COL_RED, COL_RED_DARK, COL_RED);
    } else {
        size_t p = 0u;
        p = append_fixed_1(line, p, sizeof(line), h->tip_deci_c, 3u);
        p = fmt_append_str(line, p, sizeof(line), "C ");
        (void)fmt_append_str(line, p, sizeof(line), st_label);
        gfx_draw_badge(62u, 4u, 112u, 18u, line, st_fg, st_bg, st_bd);
    }

    size_t cp = 0u;
    fmt_uint_pad(num, sizeof(num), (uint32_t)(st->menu.cursor + 1u), 2u, ' ');
    cp = fmt_append_str(line, cp, sizeof(line), num);
    cp = fmt_append_str(line, cp, sizeof(line), "/");
    fmt_uint_pad(num, sizeof(num), MENU_ITEM_COUNT, 2u, '0');
    (void)fmt_append_str(line, cp, sizeof(line), num);
    gfx_draw_text(184u, 9u, line, COL_MUTED, COL_CARD, 1u);

    gfx_draw_badge(
        246u, 4u, 68u, 18u,
        h->is_f245 ? "HDL:F245" : "HDL:F210",
        h->is_f245 ? COL_CYAN : COL_AMBER,
        h->is_f245 ? COL_CYAN_DARK : COL_AMBER_DARK,
        h->is_f245 ? COL_CYAN : COL_AMBER
    );

    /* Visible 6 settings rows */
    for (uint32_t r = 0u; r < MENU_VISIBLE_ROWS; ++r) {
        uint8_t idx = (uint8_t)(st->menu.scroll_top + r);
        uint16_t row_y = (uint16_t)(35u + r * 28u);
        if (idx >= MENU_ITEM_COUNT) {
            gfx_fill_rect(12u, row_y, 296u, 24u, COL_CARD);
            continue;
        }

        uint8_t selected = (idx == st->menu.cursor) ? 1u : 0u;
        uint8_t editing = (selected && st->menu.editing) ? 1u : 0u;
        uint16_t fg_lbl = selected ? COL_TEXT : COL_MUTED;
        uint16_t fg_val = editing ? COL_AMBER : (selected ? COL_CYAN : COL_TEXT);
        uint16_t bg = editing ? COL_AMBER_DARK : (selected ? COL_CYAN_DARK : COL_BG);
        uint16_t bd = editing ? COL_AMBER : (selected ? COL_CYAN : COL_BORDER);

        if ((menu_item_t)idx == MENU_ITEM_SAVE && st->save_toast_active) {
            fg_val = COL_EMERALD;
        }

        char raw_val[24];
        menu_format_value((menu_item_t)idx, &st->settings, h->is_f245, st->save_toast_active, raw_val, sizeof(raw_val));
        char disp_val[24];
        if (editing != 0u) {
            size_t vp = fmt_append_str(disp_val, 0u, sizeof(disp_val), "< ");
            vp = fmt_append_str(disp_val, vp, sizeof(disp_val), raw_val);
            (void)fmt_append_str(disp_val, vp, sizeof(disp_val), " >");
        } else {
            (void)fmt_append_str(disp_val, 0u, sizeof(disp_val), raw_val);
        }

        gfx_draw_rect(12u, row_y, 296u, 24u, bd);
        gfx_fill_rect(13u, (uint16_t)(row_y + 1u), 294u, 7u, bg);
        gfx_fill_rect(13u, (uint16_t)(row_y + 8u), 7u, 7u, bg);
        draw_padded_16(20u, (uint16_t)(row_y + 8u), menu_item_label((menu_item_t)idx, h->is_f245), fg_lbl, bg);
        gfx_fill_rect(116u, (uint16_t)(row_y + 8u), 70u, 7u, bg);
        draw_padded_18_right(186u, (uint16_t)(row_y + 8u), disp_val, fg_val, bg);
        gfx_fill_rect(294u, (uint16_t)(row_y + 8u), 13u, 7u, bg);
        gfx_fill_rect(13u, (uint16_t)(row_y + 15u), 294u, 8u, bg);
    }

    /* Bottom 5-button navigation hints */
    static const uint16_t btn_x[5] = { 4u, 67u, 130u, 195u, 258u };
    static const uint16_t btn_w[5] = { 58u, 58u, 60u, 58u, 58u };
    static const uint8_t  btn_bit[5] = { KEY_BIT_PC11, KEY_BIT_PC12, KEY_BIT_PD2, KEY_BIT_PB3, KEY_BIT_PB4 };
    const char *lbl_browse[5] = { "BACK", "UP", "OK", "DOWN", "EXIT" };
    const char *lbl_edit[5]   = { "DONE", "-",  "OK", "+",    "EXIT" };
    const char *const *lbls = st->menu.editing ? lbl_edit : lbl_browse;

    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t held = (inp->held_mask & btn_bit[i]) != 0u;
        uint16_t fg = held ? COL_CYAN : COL_TEXT;
        uint16_t bg = held ? COL_CYAN_DARK : COL_CARD;
        uint16_t bd = held ? COL_CYAN : COL_BORDER;
        if (!held && i == 2u) {
            fg = st->menu.editing ? COL_AMBER : COL_EMERALD;
            bg = st->menu.editing ? COL_AMBER_DARK : COL_GREEN_DARK;
            bd = fg;
        }
        gfx_draw_badge(btn_x[i], 216u, btn_w[i], 20u, lbls[i], fg, bg, bd);
    }
}

void ui_draw_dynamic(const sensor_snapshot_t *snap, const input_state_t *inp, const ui_status_t *st) {
    if (snap == NULL || inp == NULL || st == NULL) return;

    uint8_t want_menu = st->menu.open ? 1u : 0u;
    if (want_menu != g_rendered_menu_mode) {
        if (want_menu != 0u) {
            ui_draw_menu_static();
        } else {
            ui_draw_static();
        }
    }

    if (want_menu != 0u) {
        ui_draw_menu_dynamic(inp, st);
        return;
    }

    const heater_status_t *h = &st->hst;
    char line[64];
    char num[16];

    /* Header badges */
    const char *st_label;
    uint16_t st_fg, st_bg, st_bd;
    pick_state_style(h, &st_label, &st_fg, &st_bg, &st_bd);
    gfx_draw_badge(78u, 4u, 88u, 18u, st_label, st_fg, st_bg, st_bd);

    if (st->usb_connected) {
        gfx_draw_badge(172u, 4u, 68u, 18u, "USB:CDC", COL_EMERALD, COL_GREEN_DARK, COL_EMERALD);
    } else {
        gfx_draw_badge(172u, 4u, 68u, 18u, "USB:---", COL_MUTED, COL_BG, COL_BORDER);
    }

    gfx_draw_badge(
        246u, 4u, 68u, 18u,
        h->is_f245 ? "HDL:F245" : "HDL:F210",
        h->is_f245 ? COL_CYAN : COL_AMBER,
        h->is_f245 ? COL_CYAN_DARK : COL_AMBER_DARK,
        h->is_f245 ? COL_CYAN : COL_AMBER
    );

    /* Hero Left: Subtitle & Big Temperature */
    if ((h->fault_mask & FAULT_OPEN_TIP) != 0u) {
        gfx_draw_text(14u, 36u, "NO CARTRIDGE / OPEN!", COL_RED, COL_CARD, 1u);
        gfx_draw_text(14u, 48u, "OPEN!", COL_RED, COL_CARD, 4u);
        gfx_fill_rect(136u, 48u, 30u, 28u, COL_CARD);
    } else {
        if ((h->fault_mask & FAULT_SETTLING) != 0u) {
            gfx_draw_text(14u, 36u, "HANDLE SETTLING...  ", COL_AMBER, COL_CARD, 1u);
        } else {
            gfx_draw_text(14u, 36u, "TIP TEMPERATURE     ", COL_MUTED, COL_CARD, 1u);
        }
        size_t tp = append_fixed_1(line, 0u, sizeof(line), h->tip_deci_c, 3u);
        line[tp] = '\0';
        uint16_t temp_col = COL_TEXT;
        if (h->state == HEATER_FAULT || (h->fault_mask & FAULT_OVERTEMP) != 0u) {
            temp_col = COL_RED;
        } else if (h->state == HEATER_HEATING) {
            temp_col = st_fg;
        } else if (h->state == HEATER_SLEEP) {
            temp_col = COL_CYAN;
        }
        gfx_draw_text(14u, 48u, line, temp_col, COL_CARD, 4u);
        gfx_draw_rect(138u, 48u, 5u, 5u, temp_col);
        gfx_draw_text(145u, 50u, "C", temp_col, COL_CARD, 2u);
    }

    /* Setpoint line */
    size_t p = 0u;
    if (h->state == HEATER_SLEEP) {
        p = fmt_append_str(line, p, sizeof(line), "SLP:");
        fmt_uint_pad(num, sizeof(num), h->active_target_c, 3u, ' ');
    } else {
        p = fmt_append_str(line, p, sizeof(line), "SET:");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
    }
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "C");
    gfx_draw_text(14u, 84u, line, (h->state == HEATER_SLEEP) ? COL_CYAN : COL_TEXT, COL_CARD, 2u);

    p = 0u;
    if (h->state == HEATER_SLEEP) {
        p = fmt_append_str(line, p, sizeof(line), "(SET:");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "C)");
    } else if (h->armed == 0u) {
        p = fmt_append_str(line, p, sizeof(line), "(STANDBY) ");
    } else {
        p = fmt_append_str(line, p, sizeof(line), h->is_f245 ? "(F245 PID)" : "(F210 CAP)");
    }
    gfx_draw_text(114u, 88u, line, COL_MUTED, COL_CARD, 1u);

    /* Calibration & raw TC info */
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "TC:");
    fmt_uint_pad(num, sizeof(num), snap->tip_avg, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " Z:");
    fmt_uint_pad(num, sizeof(num), h->cur_zero_mv, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " CAL:");
    fmt_int_pad(num, sizeof(num), st->cal_offset_c, 3u, ' ', 1u);
    p = fmt_append_str(line, p, sizeof(line), num);
    (void)fmt_append_str(line, p, sizeof(line), "C");
    gfx_draw_text(14u, 108u, line, COL_MUTED, COL_CARD, 1u);

    /* Hero Right: Duty bar + Rail + Current + Stand */
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "PWR:");
    fmt_uint_pad(num, sizeof(num), h->duty_pct, 3u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "% (");
    fmt_uint_pad(num, sizeof(num), h->duty_ticks, 2u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    (void)fmt_append_str(line, p, sizeof(line), "t)  ");
    gfx_draw_text(204u, 36u, line, COL_TEXT, COL_CARD, 1u);

    uint16_t bar_w = (uint16_t)(((uint32_t)h->duty_pct * 100u) / 100u);
    if (bar_w > 100u) bar_w = 100u;
    if (bar_w > 0u) {
        gfx_fill_rect(205u, 48u, bar_w, 9u, st_fg);
    }
    if (bar_w < 100u) {
        gfx_fill_rect((uint16_t)(205u + bar_w), 48u, (uint16_t)(100u - bar_w), 9u, COL_SPARK_BG);
    }

    uint16_t rail_col = (h->rail_mv >= RAIL_MIN_MV && h->rail_mv <= RAIL_MAX_MV) ? COL_EMERALD : COL_RED;
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "RAIL:");
    p = append_fixed_1(line, p, sizeof(line), (int32_t)((h->rail_mv + 50u) / 100u), 2u);
    p = fmt_append_str(line, p, sizeof(line), "V      ");
    gfx_draw_text(204u, 65u, line, rail_col, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "CUR :");
    p = append_ma_as_amps(line, p, sizeof(line), h->cur_ma, 1u);
    p = fmt_append_str(line, p, sizeof(line), "A    ");
    gfx_draw_text(204u, 79u, line, COL_TEXT, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "PEAK:");
    p = append_ma_as_amps(line, p, sizeof(line), h->cur_peak_ma, 0u);
    p = fmt_append_str(line, p, sizeof(line), "A    ");
    gfx_draw_text(204u, 93u, line, COL_MUTED, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "STND:");
    if (h->in_stand != 0u) {
        p = fmt_append_str(line, p, sizeof(line), h->sleep_en ? "IN (SLP)" : "IN (IGN)");
    } else {
        p = fmt_append_str(line, p, sizeof(line), "OUT     ");
    }
    gfx_draw_text(204u, 107u, line, h->in_stand ? COL_CYAN : COL_MUTED, COL_CARD, 1u);

    /* Middle Card: Sparkline (150x60) */
    gfx_draw_sparkline(
        13u, 143u, 150u, 60u,
        snap->temp_hist, snap->hist_count, snap->hist_head, TEMP_HARD_CUTOFF_C,
        (h->state == HEATER_FAULT) ? COL_RED : COL_CYAN,
        COL_SPARK_BG
    );

    /* Middle Right: Presets */
    static const uint16_t preset_x[3] = { 172u, 217u, 262u };
    for (uint32_t i = 0u; i < 3u; ++i) {
        p = 0u;
        fmt_uint_pad(num, sizeof(num), (uint32_t)(i + 1u), 1u, '0');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), ":");
        fmt_uint_pad(num, sizeof(num), st->presets_c[i], 3u, '0');
        (void)fmt_append_str(line, p, sizeof(line), num);
        uint8_t active = (st->active_preset == i || h->target_c == st->presets_c[i]) ? 1u : 0u;
        gfx_draw_badge(
            preset_x[i], 132u, 42u, 18u, line,
            active ? COL_CYAN : COL_MUTED,
            active ? COL_CYAN_DARK : COL_BG,
            active ? COL_CYAN : COL_BORDER
        );
    }

    /* Fault / Toast / Raw ADC / Uptime lines */
    uint16_t non_settle = h->fault_mask & (uint16_t)~FAULT_SETTLING;
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "FLT:");
    p = fmt_append_str(line, p, sizeof(line), heater_fault_name(h->fault_mask));
    while (p < 20u) line[p++] = ' ';
    line[p] = '\0';
    gfx_draw_text(172u, 156u, line, non_settle ? COL_RED : COL_EMERALD, COL_CARD, 1u);

    if (st->save_toast_active != 0u) {
        gfx_draw_text(172u, 170u, "SAVED TO FLASH!     ", COL_EMERALD, COL_CARD, 1u);
    } else {
        p = 0u;
        p = fmt_append_str(line, p, sizeof(line), "BL:");
        fmt_uint_pad(num, sizeof(num), st->backlight_pct, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "%  SLP:");
        p = fmt_append_str(line, p, sizeof(line), h->sleep_en ? "ON " : "OFF");
        p = fmt_append_str(line, p, sizeof(line), "   ");
        gfx_draw_text(172u, 170u, line, COL_MUTED, COL_CARD, 1u);
    }

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "CUR:");
    fmt_uint_pad(num, sizeof(num), snap->cur_avg, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " AUX:");
    fmt_uint_pad(num, sizeof(num), snap->aux_avg, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "  ");
    gfx_draw_text(172u, 184u, line, COL_MUTED, COL_CARD, 1u);

    uint32_t total_s = st->uptime_ms / 1000u;
    uint32_t hh = (total_s / 3600u) % 100u;
    uint32_t mm = (total_s / 60u) % 60u;
    uint32_t ss = total_s % 60u;
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "UP:");
    fmt_uint_pad(num, sizeof(num), hh, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), ":");
    fmt_uint_pad(num, sizeof(num), mm, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), ":");
    fmt_uint_pad(num, sizeof(num), ss, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "  LED:");
    fmt_hex8(num, sizeof(num), st->led_mask);
    p = fmt_append_str(line, p, sizeof(line), num);
    (void)fmt_append_str(line, p, sizeof(line), " ");
    gfx_draw_text(172u, 197u, line, COL_MUTED, COL_CARD, 1u);

    /* Bottom verified 5-button bar (Left->Right: PC11, PC12, PD2, PB3, PB4) */
    static const uint16_t btn_x[5] = { 4u, 67u, 130u, 195u, 258u };
    static const uint16_t btn_w[5] = { 58u, 58u, 60u, 58u, 58u };
    static const uint8_t  btn_bit[5] = { KEY_BIT_PC11, KEY_BIT_PC12, KEY_BIT_PD2, KEY_BIT_PB3, KEY_BIT_PB4 };
    const char *btn_lbl[5] = {
        non_settle ? "CLR" : "CH",
        "-5C",
        h->armed ? "STOP" : "HEAT",
        "+5C",
        "MENU"
    };

    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t held = (inp->held_mask & btn_bit[i]) != 0u;
        uint16_t fg = held ? COL_CYAN : COL_TEXT;
        uint16_t bg = held ? COL_CYAN_DARK : COL_CARD;
        uint16_t bd = held ? COL_CYAN : COL_BORDER;
        if (!held && i == 2u) {
            fg = h->armed ? COL_RED : COL_EMERALD;
            bg = h->armed ? COL_RED_DARK : COL_GREEN_DARK;
            bd = fg;
        }
        gfx_draw_badge(btn_x[i], 216u, btn_w[i], 20u, btn_lbl[i], fg, bg, bd);
    }
}
