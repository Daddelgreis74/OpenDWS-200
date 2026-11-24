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

static uint8_t g_rendered_screen_mode = 0u; /* 0 = MAIN, 1 = MENU, 2 = GRAPH */

static int32_t abs_i32(int32_t v) {
    return (v < 0) ? -v : v;
}

void ui_temp_filter_init(ui_temp_filter_t *f) {
    if (f == NULL) return;
    f->filtered_dc = 0;
    f->shown_c = 0;
    f->initialized = 0u;
}

int16_t ui_temp_filter_step(
    ui_temp_filter_t *f,
    int32_t raw_deci_c,
    uint16_t active_target_c,
    uint8_t settled_mode
) {
    if (raw_deci_c < 0) raw_deci_c = 0;
    int16_t rounded = (int16_t)((raw_deci_c + 5) / 10);
    if (f == NULL) return rounded;

    if (f->initialized == 0u) {
        f->filtered_dc = raw_deci_c;
        f->shown_c = rounded;
        f->initialized = 1u;
    } else if (abs_i32(raw_deci_c - f->filtered_dc) >= 30) {
        /* Track fast ramps (>= 3.0 C delta) quickly without lag */
        f->filtered_dc = (f->filtered_dc + raw_deci_c) / 2;
    } else {
        /* Heavy smoothing at steady state so ADC jitter stays invisible */
        f->filtered_dc = (f->filtered_dc * 5 + raw_deci_c + 3) / 6;
    }

    if (settled_mode != 0u && active_target_c > 0u &&
        abs_i32(f->filtered_dc - (int32_t)active_target_c * 10) <= 10) {
        f->shown_c = (int16_t)active_target_c;
    } else if (abs_i32(f->filtered_dc - (int32_t)f->shown_c * 10) >= 7) {
        f->shown_c = (int16_t)((f->filtered_dc + 5) / 10);
    }

    if (f->shown_c < 0) {
        f->shown_c = 0;
    } else if (f->shown_c > 999) {
        f->shown_c = 999;
    }
    return f->shown_c;
}

static int16_t ui_display_temp_c(const ui_status_t *st) {
    if (st->display_temp_c > 0) {
        return st->display_temp_c;
    }
    int32_t dc = st->hst.tip_deci_c;
    if (dc < 0) dc = 0;
    int32_t c = (dc + 5) / 10;
    if (c > 999) c = 999;
    return (int16_t)c;
}

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

static void ui_begin_screen(uint8_t mode, const char *title, uint16_t title_col) {
    g_rendered_screen_mode = mode;
    gfx_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, COL_BG);

    /* Top header bar */
    gfx_fill_rect(0u, 0u, LCD_WIDTH, 25u, COL_CARD);
    gfx_draw_hline(0u, 25u, LCD_WIDTH, COL_BORDER);
    gfx_draw_text(6u, 9u, title, title_col, COL_CARD, 1u);

    /* Bottom 5-button bar */
    gfx_draw_hline(0u, 213u, LCD_WIDTH, COL_BORDER);
    gfx_fill_rect(0u, 214u, LCD_WIDTH, 26u, COL_BG);
}

void ui_draw_static(void) {
    ui_begin_screen(0u, "OpenDWS-200", COL_TEXT);

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
}

static void ui_draw_menu_static(void) {
    ui_begin_screen(1u, "SETTINGS", COL_CYAN);

    /* Settings list card */
    gfx_fill_rect(6u, 30u, 308u, 178u, COL_CARD);
    gfx_draw_rect(6u, 30u, 308u, 178u, COL_BORDER);
}

static void ui_draw_graph_static(void) {
    ui_begin_screen(2u, "OpenDWS-200", COL_TEXT);

    /* Compact live telemetry strip card */
    gfx_fill_rect(6u, 29u, 308u, 34u, COL_CARD);
    gfx_draw_rect(6u, 29u, 308u, 34u, COL_BORDER);
    gfx_draw_vline(192u, 33u, 26u, COL_BORDER);

    /* Large Dual Graph card (Temp + Power) */
    gfx_fill_rect(6u, 67u, 308u, 142u, COL_CARD);
    gfx_draw_rect(6u, 67u, 308u, 142u, COL_BORDER);

    /* Graph legend */
    gfx_draw_text(12u, 71u, "TEMP", COL_CYAN, COL_CARD, 1u);
    gfx_draw_text(42u, 71u, "0..480C", COL_MUTED, COL_CARD, 1u);
    gfx_draw_text(102u, 71u, "-- SET", COL_MUTED, COL_CARD, 1u);
    gfx_draw_text(158u, 71u, "PWR", COL_AMBER, COL_CARD, 1u);
    gfx_draw_text(182u, 71u, "0..100%", COL_MUTED, COL_CARD, 1u);

    /* Left Y-axis labels (Temperature 0..480 C) */
    gfx_draw_text(10u, 83u, "480", COL_CYAN, COL_CARD, 1u);
    gfx_draw_text(10u, 120u, "320", COL_CYAN, COL_CARD, 1u);
    gfx_draw_text(10u, 158u, "160", COL_CYAN, COL_CARD, 1u);
    gfx_draw_text(22u, 195u, "0", COL_CYAN, COL_CARD, 1u);

    /* Plot border (252x120 plot area inside) */
    gfx_draw_rect(32u, 82u, 254u, 122u, COL_BORDER);

    /* Right Y-axis labels (Power 0..100 %) */
    gfx_draw_text(289u, 83u, "100", COL_AMBER, COL_CARD, 1u);
    gfx_draw_text(289u, 139u, " 50", COL_AMBER, COL_CARD, 1u);
    gfx_draw_text(289u, 195u, " 0%", COL_AMBER, COL_CARD, 1u);
}

static void ui_draw_notip_static(void) {
    ui_begin_screen(3u, "OpenDWS-200", COL_TEXT);

    /* High-contrast graphical No-Tip warning card */
    gfx_fill_rect(10u, 32u, 300u, 174u, COL_RED_DARK);
    gfx_draw_rect(10u, 32u, 300u, 174u, COL_RED);
    gfx_draw_rect(11u, 33u, 298u, 172u, COL_RED);

    /* Left graphical warning box (!) */
    gfx_fill_rect(24u, 44u, 44u, 44u, COL_AMBER_DARK);
    gfx_draw_rect(24u, 44u, 44u, 44u, COL_RED);
    gfx_fill_rect(43u, 50u, 6u, 21u, COL_AMBER);
    gfx_fill_rect(43u, 76u, 6u, 6u, COL_AMBER);

    /* Title */
    gfx_draw_text(80u, 55u, "NO TIP!", COL_RED, COL_RED_DARK, 3u);

    /* Right graphical disconnected cartridge icon (handle + break gap + tip) */
    gfx_fill_rect(218u, 58u, 26u, 14u, COL_MUTED);
    gfx_fill_rect(244u, 60u, 5u, 10u, COL_TEXT);
    gfx_fill_rect(254u, 56u, 2u, 18u, COL_RED);
    gfx_fill_rect(260u, 56u, 2u, 18u, COL_RED);
    gfx_fill_rect(268u, 62u, 22u, 6u, COL_AMBER);
    gfx_fill_rect(290u, 64u, 6u, 2u, COL_AMBER);

    /* German + English guidance */
    gfx_draw_text(82u, 102u, "KEINE SPITZE!", COL_TEXT, COL_RED_DARK, 2u);
    gfx_draw_text(76u, 132u, "INSERT CARTRIDGE INTO HANDLE", COL_AMBER, COL_RED_DARK, 1u);
    gfx_draw_hline(24u, 154u, 272u, COL_RED);
}

static void ui_draw_standby_static(void) {
    g_rendered_screen_mode = 4u;
    gfx_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, COL_BG);

    /* Calm full-screen Standby frame */
    gfx_fill_rect(10u, 10u, 300u, 198u, COL_CARD);
    gfx_draw_rect(10u, 10u, 300u, 198u, COL_CYAN);
    gfx_draw_hline(24u, 48u, 272u, COL_BORDER);
    gfx_draw_rect(60u, 98u, 200u, 8u, COL_BORDER);

    /* Bottom 5-button bar */
    gfx_draw_hline(0u, 213u, LCD_WIDTH, COL_BORDER);
    gfx_fill_rect(0u, 214u, LCD_WIDTH, 26u, COL_BG);
}

static void draw_main_header_badges(
    const ui_status_t *st,
    const char *st_label,
    uint16_t st_fg,
    uint16_t st_bg,
    uint16_t st_bd
) {
    const heater_status_t *h = &st->hst;
    if (st->power_off_pending != 0u) {
        gfx_draw_badge(78u, 4u, 88u, 18u, "POWER OFF!", COL_RED, COL_RED_DARK, COL_RED);
    } else {
        gfx_draw_badge(78u, 4u, 88u, 18u, st_label, st_fg, st_bg, st_bd);
    }

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
}

static void draw_bottom_buttons(
    const input_state_t *inp,
    const char *const lbls[5],
    uint8_t has_accent,
    uint16_t acc_fg,
    uint16_t acc_bg,
    uint16_t acc_bd
) {
    static const uint16_t btn_x[5] = { 4u, 67u, 130u, 195u, 258u };
    static const uint16_t btn_w[5] = { 58u, 58u, 60u, 58u, 58u };
    static const uint8_t  btn_bit[5] = { KEY_BIT_PC11, KEY_BIT_PC12, KEY_BIT_PD2, KEY_BIT_PB3, KEY_BIT_PB4 };

    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t held = (inp->held_mask & btn_bit[i]) != 0u;
        uint16_t fg = held ? COL_CYAN : COL_TEXT;
        uint16_t bg = held ? COL_CYAN_DARK : COL_CARD;
        uint16_t bd = held ? COL_CYAN : COL_BORDER;
        if (!held && has_accent != 0u && i == 2u) {
            fg = acc_fg;
            bg = acc_bg;
            bd = acc_bd;
        }
        gfx_draw_badge(btn_x[i], 216u, btn_w[i], 20u, lbls[i], fg, bg, bd);
    }
}

static void format_setpoint_str(const heater_status_t *h, char *line, size_t cap) {
    char num[16];
    size_t p = 0u;
    if (h->state == HEATER_SLEEP) {
        if (h->active_target_c == 0u) {
            (void)fmt_append_str(line, 0u, cap, "SLP: OFF");
        } else {
            p = fmt_append_str(line, p, cap, "SLP:");
            fmt_uint_pad(num, sizeof(num), h->active_target_c, 3u, ' ');
            p = fmt_append_str(line, p, cap, num);
            (void)fmt_append_str(line, p, cap, "C");
        }
    } else {
        p = fmt_append_str(line, p, cap, "SET:");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, cap, num);
        (void)fmt_append_str(line, p, cap, "C");
    }
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
    if (st->power_off_pending != 0u) {
        gfx_draw_badge(62u, 4u, 112u, 18u, "POWER OFF...", COL_RED, COL_RED_DARK, COL_RED);
    } else if ((h->fault_mask & FAULT_OPEN_TIP) != 0u) {
        gfx_draw_badge(62u, 4u, 112u, 18u, "OPEN! NO TIP", COL_RED, COL_RED_DARK, COL_RED);
    } else {
        size_t p = 0u;
        fmt_uint_pad(num, sizeof(num), (uint32_t)ui_display_temp_c(st), 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
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
    const char *const lbl_browse[5] = { "BACK", "UP", "OK", "DOWN", "EXIT" };
    const char *const lbl_edit[5]   = { "DONE", "-",  "OK", "+",    "EXIT" };
    const char *const *lbls = st->menu.editing ? lbl_edit : lbl_browse;
    uint16_t acc_fg = st->menu.editing ? COL_AMBER : COL_EMERALD;
    uint16_t acc_bg = st->menu.editing ? COL_AMBER_DARK : COL_GREEN_DARK;
    draw_bottom_buttons(inp, lbls, 1u, acc_fg, acc_bg, acc_fg);
}

static uint16_t pick_temp_color(const heater_status_t *h, uint16_t st_fg) {
    if (h->state == HEATER_FAULT || (h->fault_mask & FAULT_OVERTEMP) != 0u) {
        return COL_RED;
    }
    if (h->state == HEATER_HEATING) {
        return st_fg;
    }
    if (h->state == HEATER_SLEEP) {
        return COL_CYAN;
    }
    return COL_TEXT;
}

static void ui_draw_graph_dynamic(
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const ui_status_t *st
) {
    const heater_status_t *h = &st->hst;
    char line[64];
    char num[16];

    const char *st_label;
    uint16_t st_fg, st_bg, st_bd;
    pick_state_style(h, &st_label, &st_fg, &st_bg, &st_bd);
    draw_main_header_badges(st, st_label, st_fg, st_bg, st_bd);

    /* Compact live telemetry left: temperature + setpoint/mode */
    if ((h->fault_mask & FAULT_OPEN_TIP) != 0u) {
        gfx_draw_text(14u, 35u, "OPEN!  ", COL_RED, COL_CARD, 2u);
    } else {
        fmt_uint_pad(line, sizeof(line), (uint32_t)ui_display_temp_c(st), 3u, ' ');
        uint16_t temp_col = pick_temp_color(h, st_fg);
        gfx_draw_text(14u, 35u, line, temp_col, COL_CARD, 2u);
        gfx_fill_rect(50u, 35u, 48u, 14u, COL_CARD);
        gfx_draw_rect(52u, 35u, 4u, 4u, temp_col);
        gfx_draw_text(58u, 36u, "C", temp_col, COL_CARD, 1u);
    }

    format_setpoint_str(h, line, sizeof(line));
    size_t sp_len = 0u;
    while (line[sp_len] != '\0') sp_len++;
    if (h->state == HEATER_SLEEP) {
        sp_len = fmt_append_str(line, sp_len, sizeof(line), " (SLEEP)   ");
    } else if (h->armed == 0u) {
        sp_len = fmt_append_str(line, sp_len, sizeof(line), " (STANDBY) ");
    } else {
        sp_len = fmt_append_str(line, sp_len, sizeof(line), h->is_f245 ? " (F245 PID)" : " (F210 CAP)");
    }
    (void)sp_len;
    gfx_draw_text(14u, 52u, line, (h->state == HEATER_SLEEP) ? COL_CYAN : COL_MUTED, COL_CARD, 1u);

    /* Compact live telemetry right: power % / ticks + current / rail voltage */
    size_t p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "PWR:");
    fmt_uint_pad(num, sizeof(num), h->duty_pct, 3u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "% (");
    fmt_uint_pad(num, sizeof(num), h->duty_ticks, 2u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    (void)fmt_append_str(line, p, sizeof(line), "t)  ");
    gfx_draw_text(198u, 35u, line, COL_AMBER, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "CUR:");
    p = append_ma_as_amps(line, p, sizeof(line), h->cur_ma, 0u);
    p = fmt_append_str(line, p, sizeof(line), "A ");
    p = append_fixed_1(line, p, sizeof(line), (int32_t)((h->rail_mv + 50u) / 100u), 2u);
    (void)fmt_append_str(line, p, sizeof(line), "V");
    gfx_draw_text(198u, 49u, line, COL_TEXT, COL_CARD, 1u);

    /* Save toast in graph card header */
    if (st->save_toast_active != 0u) {
        gfx_draw_text(270u, 71u, "SAVED!", COL_EMERALD, COL_CARD, 1u);
    } else {
        gfx_draw_text(270u, 71u, "      ", COL_CARD, COL_CARD, 1u);
    }

    /* Large Dual Graph (Temperature 0..480 C + Power 0..100 % + dashed Setpoint) */
    gfx_draw_dual_graph(
        33u, 83u, 252u, 120u,
        snap->temp_hist, snap->pwr_hist,
        snap->hist_count, snap->hist_head,
        TEMP_HARD_CUTOFF_C, h->active_target_c,
        (h->state == HEATER_FAULT) ? COL_RED : COL_CYAN,
        COL_AMBER, COL_AMBER_DARK, COL_MUTED, COL_SPARK_BG
    );

    uint16_t non_settle = h->fault_mask & (uint16_t)~FAULT_SETTLING;
    const char *const btn_lbl[5] = {
        non_settle ? "CLR" : "CH",
        "-5C",
        "MAIN",
        "+5C",
        "MENU"
    };
    draw_bottom_buttons(inp, btn_lbl, 1u, COL_CYAN, COL_CYAN_DARK, COL_CYAN);
}

static void ui_draw_notip_dynamic(
    const sensor_snapshot_t *snap,
    const input_state_t *inp,
    const ui_status_t *st
) {
    const heater_status_t *h = &st->hst;
    char line[64];
    char num[16];

    draw_main_header_badges(st, "NO TIP!", COL_RED, COL_RED_DARK, COL_RED);

    size_t p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "RAIL:");
    p = append_fixed_1(line, p, sizeof(line), (int32_t)((h->rail_mv + 50u) / 100u), 2u);
    p = fmt_append_str(line, p, sizeof(line), "V   TC:");
    fmt_uint_pad(num, sizeof(num), snap->tip_avg, 4u, ' ');
    (void)fmt_append_str(line, p, sizeof(line), num);
    gfx_draw_text(100u, 166u, line, COL_TEXT, COL_RED_DARK, 1u);

    gfx_draw_text(
        76u, 182u,
        st->settings.buzzer_en ? "HEATING LOCKED  |  ALARM: ON " : "HEATING LOCKED  |  ALARM: OFF",
        COL_AMBER, COL_RED_DARK, 1u
    );

    const char *const btn_lbl[5] = { "CLR", "-5C", "GRAPH", "+5C", "MENU" };
    draw_bottom_buttons(inp, btn_lbl, 0u, COL_TEXT, COL_CARD, COL_BORDER);
}

static void ui_draw_standby_dynamic(
    const input_state_t *inp,
    const ui_status_t *st
) {
    const heater_status_t *h = &st->hst;
    char line[64];
    char num[16];

    /* Top corner status tags + centered 9-char title */
    gfx_draw_text(18u, 18u, h->is_f245 ? "F245" : "F210", COL_MUTED, COL_CARD, 1u);
    if (st->power_off_pending != 0u) {
        gfx_draw_text(79u, 20u, "POWER OFF", COL_RED, COL_CARD, 3u);
    } else {
        gfx_draw_text(79u, 20u, " STANDBY ", COL_CYAN, COL_CARD, 3u);
    }
    gfx_draw_text(
        254u, 18u,
        st->usb_connected ? "USB:ON " : "USB:-- ",
        st->usb_connected ? COL_EMERALD : COL_MUTED,
        COL_CARD, 1u
    );

    /* Centerpiece live temperature (watch warm-up to 100 C) */
    fmt_uint_pad(line, sizeof(line), (uint32_t)ui_display_temp_c(st), 3u, ' ');
    uint16_t temp_col = (h->state == HEATER_SLEEP) ? COL_CYAN : COL_TEXT;
    gfx_draw_text(114u, 58u, line, temp_col, COL_CARD, 4u);
    gfx_draw_rect(190u, 58u, 5u, 5u, temp_col);
    gfx_draw_text(198u, 60u, "C", temp_col, COL_CARD, 2u);

    /* Centered preheat progress bar (198 px inside 60..260 border) */
    uint16_t pct = (h->duty_pct > 100u) ? 100u : (uint16_t)h->duty_pct;
    uint16_t bar_w = (uint16_t)(((uint32_t)pct * 198u) / 100u);
    if (bar_w > 0u) {
        gfx_fill_rect(61u, 99u, bar_w, 6u, COL_CYAN);
    }
    if (bar_w < 198u) {
        gfx_fill_rect((uint16_t)(61u + bar_w), 99u, (uint16_t)(198u - bar_w), 6u, COL_SPARK_BG);
    }

    /* Centered 28-char status line: Preheat + Auto-Off Countdown or Next Target */
    size_t p = 0u;
    uint16_t status_col = COL_MUTED;
    if (h->auto_off_triggered != 0u) {
        p = fmt_append_str(line, p, sizeof(line), "AUTO-OFF (0W)  -> NEXT ");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "C");
        status_col = COL_AMBER;
    } else if (h->state == HEATER_SLEEP && h->active_target_c > 0u) {
        p = fmt_append_str(line, p, sizeof(line), "PREHEAT ");
        fmt_uint_pad(num, sizeof(num), h->active_target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        if (h->auto_off_rem_s > 0u) {
            uint16_t mm = h->auto_off_rem_s / 60u;
            uint16_t ss = h->auto_off_rem_s % 60u;
            p = fmt_append_str(line, p, sizeof(line), "C | OFF IN ");
            fmt_uint_pad(num, sizeof(num), mm, 2u, '0');
            p = fmt_append_str(line, p, sizeof(line), num);
            p = fmt_append_str(line, p, sizeof(line), ":");
            fmt_uint_pad(num, sizeof(num), ss, 2u, '0');
            p = fmt_append_str(line, p, sizeof(line), num);
        } else {
            p = fmt_append_str(line, p, sizeof(line), "C  ->  NEXT ");
            fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
            p = fmt_append_str(line, p, sizeof(line), num);
            p = fmt_append_str(line, p, sizeof(line), "C");
        }
        status_col = COL_TEXT;
    } else if (h->state == HEATER_SLEEP) {
        p = fmt_append_str(line, p, sizeof(line), "SLEEP 0W (COLD) | NEXT ");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "C");
    } else {
        p = fmt_append_str(line, p, sizeof(line), "IDLE / SAFE    |  NEXT ");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "C");
    }
    while (p < 28u) {
        line[p++] = ' ';
    }
    line[p] = '\0';
    gfx_draw_text(76u, 114u, line, status_col, COL_CARD, 1u);

    /* Centered 31-char telemetry footer */
    if (st->save_toast_active != 0u) {
        gfx_draw_text(67u, 134u, "        SAVED TO FLASH!        ", COL_EMERALD, COL_CARD, 1u);
    } else {
        p = 0u;
        p = fmt_append_str(line, p, sizeof(line), "NEXT:");
        fmt_uint_pad(num, sizeof(num), h->target_c, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "C  RAIL:");
        p = append_fixed_1(line, p, sizeof(line), (int32_t)((h->rail_mv + 50u) / 100u), 2u);
        p = fmt_append_str(line, p, sizeof(line), "V  PWR:");
        fmt_uint_pad(num, sizeof(num), pct, 3u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        (void)fmt_append_str(line, p, sizeof(line), "%");
        gfx_draw_text(67u, 134u, line, COL_MUTED, COL_CARD, 1u);
    }

    if (h->in_stand != 0u) {
        gfx_draw_badge(24u, 158u, 272u, 32u, "LIFT IRON FROM STAND TO HEAT", COL_CYAN, COL_CYAN_DARK, COL_CYAN);
    } else {
        gfx_draw_badge(24u, 158u, 272u, 32u, "PLACE IRON IN STAND FIRST", COL_AMBER, COL_AMBER_DARK, COL_AMBER);
    }

    uint16_t non_settle = h->fault_mask & (uint16_t)~FAULT_SETTLING;
    const char *const btn_lbl[5] = {
        non_settle ? "CLR" : "CH",
        "-5C",
        "GRAPH",
        "+5C",
        "MENU"
    };
    draw_bottom_buttons(inp, btn_lbl, 0u, COL_TEXT, COL_CARD, COL_BORDER);
}

void ui_draw_dynamic(const sensor_snapshot_t *snap, const input_state_t *inp, const ui_status_t *st) {
    if (snap == NULL || inp == NULL || st == NULL) return;

    uint8_t active_faults = ((st->hst.fault_mask & (uint16_t)~FAULT_SETTLING) != 0u ||
                             st->hst.state == HEATER_FAULT) ? 1u : 0u;
    uint8_t want_mode = 0u;
    if (st->menu.open != 0u) {
        want_mode = 1u;
    } else if ((st->hst.fault_mask & FAULT_OPEN_TIP) != 0u) {
        want_mode = 3u;
    } else if (st->graph_mode != 0u) {
        want_mode = 2u;
    } else if (st->settings.standby_screen_en != 0u &&
               active_faults == 0u &&
               (st->hst.state == HEATER_SLEEP ||
                st->hst.state == HEATER_STANDBY ||
                (st->hst.state == HEATER_PROBE && st->hst.in_stand != 0u))) {
        want_mode = 4u;
    }

    if (want_mode != g_rendered_screen_mode) {
        if (want_mode == 1u) {
            ui_draw_menu_static();
        } else if (want_mode == 2u) {
            ui_draw_graph_static();
        } else if (want_mode == 3u) {
            ui_draw_notip_static();
        } else if (want_mode == 4u) {
            ui_draw_standby_static();
        } else {
            ui_draw_static();
        }
    }

    if (want_mode == 1u) {
        ui_draw_menu_dynamic(inp, st);
        return;
    }
    if (want_mode == 2u) {
        ui_draw_graph_dynamic(snap, inp, st);
        return;
    }
    if (want_mode == 3u) {
        ui_draw_notip_dynamic(snap, inp, st);
        return;
    }
    if (want_mode == 4u) {
        ui_draw_standby_dynamic(inp, st);
        return;
    }

    const heater_status_t *h = &st->hst;
    char line[64];
    char num[16];

    /* Header badges */
    const char *st_label;
    uint16_t st_fg, st_bg, st_bd;
    pick_state_style(h, &st_label, &st_fg, &st_bg, &st_bd);
    draw_main_header_badges(st, st_label, st_fg, st_bg, st_bd);

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
        fmt_uint_pad(line, sizeof(line), (uint32_t)ui_display_temp_c(st), 3u, ' ');
        uint16_t temp_col = pick_temp_color(h, st_fg);
        gfx_draw_text(14u, 48u, line, temp_col, COL_CARD, 4u);
        gfx_fill_rect(86u, 48u, 80u, 28u, COL_CARD);
        gfx_draw_rect(90u, 48u, 5u, 5u, temp_col);
        gfx_draw_text(98u, 50u, "C", temp_col, COL_CARD, 2u);
    }

    /* Setpoint line */
    format_setpoint_str(h, line, sizeof(line));
    gfx_draw_text(14u, 84u, line, (h->state == HEATER_SLEEP) ? COL_CYAN : COL_TEXT, COL_CARD, 2u);

    size_t p = 0u;
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
        p = fmt_append_str(line, p, sizeof(line), h->sleep_en ? "IN (AUTO)" : "IN (MAN) ");
    } else {
        p = fmt_append_str(line, p, sizeof(line), h->sleep_en ? "OUT(AUTO)" : "OUT(MAN) ");
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
    const char *const btn_lbl[5] = {
        non_settle ? "CLR" : "CH",
        "-5C",
        "GRAPH",
        "+5C",
        "MENU"
    };
    draw_bottom_buttons(inp, btn_lbl, 0u, COL_TEXT, COL_CARD, COL_BORDER);
}
