#include "ui.h"

#include "app_config.h"
#include "fmt.h"
#include "gfx.h"

#define COL_BG         RGB565(11, 16, 27)
#define COL_CARD       RGB565(20, 29, 46)
#define COL_BORDER     RGB565(31, 45, 70)
#define COL_TEXT       RGB565(241, 245, 249)
#define COL_MUTED      RGB565(122, 140, 165)
#define COL_CYAN       RGB565(0, 229, 255)
#define COL_CYAN_DARK  RGB565(10, 51, 66)
#define COL_EMERALD    RGB565(16, 185, 129)
#define COL_GREEN_DARK RGB565(12, 51, 38)
#define COL_AMBER      RGB565(245, 158, 11)
#define COL_RED        RGB565(239, 68, 68)
#define COL_RED_DARK   RGB565(69, 20, 26)
#define COL_SPARK_BG   RGB565(13, 20, 33)

static const uint16_t CARD_X[3] = { 6u, 111u, 216u };
static const uint16_t CARD_W    = 98u;
static const uint16_t CARD_Y    = 30u;
static const uint16_t CARD_H    = 88u;

static const uint16_t KEY_TILE_X[5] = { 12u, 72u, 132u, 192u, 252u };
static const char *const KEY_LABELS[5] = { "PD2", "PB3", "PB4", "PC12", "PC11" };

void ui_draw_static(void) {
    gfx_fill_rect(0u, 0u, LCD_WIDTH, LCD_HEIGHT, COL_BG);

    /* Top header bar */
    gfx_fill_rect(0u, 0u, LCD_WIDTH, 25u, COL_CARD);
    gfx_draw_hline(0u, 25u, LCD_WIDTH, COL_BORDER);
    gfx_draw_text(6u, 9u, "OpenDWS-200", COL_TEXT, COL_CARD, 1u);
    gfx_draw_badge(78u, 4u, 96u, 18u, "HEATER OFF", COL_RED, COL_RED_DARK, COL_RED);

    /* Three live ADC telemetry cards */
    static const char *const card_titles[3] = { "TIP PC0/10", "CUR PC1/11", "AUX PA3/3" };
    const uint16_t card_cols[3] = { COL_CYAN, COL_EMERALD, COL_AMBER };
    for (uint32_t i = 0u; i < 3u; ++i) {
        gfx_fill_rect(CARD_X[i], CARD_Y, CARD_W, CARD_H, COL_CARD);
        gfx_draw_rect(CARD_X[i], CARD_Y, CARD_W, CARD_H, COL_BORDER);
        gfx_draw_text((uint16_t)(CARD_X[i] + 6u), (uint16_t)(CARD_Y + 6u), card_titles[i], card_cols[i], COL_CARD, 1u);
        gfx_draw_rect((uint16_t)(CARD_X[i] + 4u), (uint16_t)(CARD_Y + 49u), (uint16_t)(CARD_W - 8u), 34u, COL_BORDER);
    }

    /* GPIO / key diagnostic panel */
    gfx_fill_rect(6u, 124u, 308u, 86u, COL_CARD);
    gfx_draw_rect(6u, 124u, 308u, 86u, COL_BORDER);

    /* Bottom hint bar */
    gfx_draw_hline(0u, 215u, LCD_WIDTH, COL_BORDER);
    gfx_fill_rect(0u, 216u, LCD_WIDTH, 24u, COL_CARD);
    gfx_draw_text(10u, 224u, "PD2:LED  PB3/4:BL-/+  PC12:USB  PC11:BEEP", COL_MUTED, COL_CARD, 1u);
}

void ui_draw_dynamic(const sensor_snapshot_t *snap, const input_state_t *inp, const ui_status_t *st) {
    if (snap == NULL || inp == NULL || st == NULL) return;

    char line[64];
    char num[16];

    /* Top badges */
    if (st->usb_connected) {
        gfx_draw_badge(180u, 4u, 64u, 18u, "USB:CDC", COL_EMERALD, COL_GREEN_DARK, COL_EMERALD);
    } else {
        gfx_draw_badge(180u, 4u, 64u, 18u, "USB:---", COL_MUTED, COL_BG, COL_BORDER);
    }
    gfx_draw_badge(
        250u, 4u, 64u, 18u,
        inp->pb6_handle ? "PB6:HI" : "PB6:LO",
        inp->pb6_handle ? COL_CYAN : COL_AMBER,
        COL_BG,
        COL_BORDER
    );

    /* Card 1: TIP (PC0 / CH10) */
    fmt_uint_pad(num, sizeof(num), snap->tip_avg, 4u, ' ');
    gfx_draw_text((uint16_t)(CARD_X[0] + 6u), (uint16_t)(CARD_Y + 18u), num, COL_TEXT, COL_CARD, 2u);
    gfx_draw_text((uint16_t)(CARD_X[0] + 58u), (uint16_t)(CARD_Y + 24u), "raw", COL_MUTED, COL_CARD, 1u);

    size_t p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "mV:");
    fmt_uint_pad(num, sizeof(num), snap->tip_mv, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " r:");
    fmt_uint_pad(num, sizeof(num), snap->tip_raw, 4u, '0');
    (void)fmt_append_str(line, p, sizeof(line), num);
    gfx_draw_text((uint16_t)(CARD_X[0] + 6u), (uint16_t)(CARD_Y + 37u), line, COL_MUTED, COL_CARD, 1u);

    gfx_draw_sparkline(
        (uint16_t)(CARD_X[0] + 5u), (uint16_t)(CARD_Y + 50u),
        (uint16_t)(CARD_W - 10u), 32u,
        snap->tip_hist, snap->hist_count, snap->hist_head, 4095u,
        COL_CYAN, COL_SPARK_BG
    );

    /* Card 2: CUR (PC1 / CH11) */
    fmt_int_pad(num, sizeof(num), snap->cur_ma, 6u, ' ', 1u);
    gfx_draw_text((uint16_t)(CARD_X[1] + 6u), (uint16_t)(CARD_Y + 18u), num, COL_TEXT, COL_CARD, 2u);
    gfx_draw_text((uint16_t)(CARD_X[1] + 80u), (uint16_t)(CARD_Y + 24u), "mA", COL_MUTED, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "mV:");
    fmt_uint_pad(num, sizeof(num), snap->cur_mv, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " r:");
    fmt_uint_pad(num, sizeof(num), snap->cur_avg, 4u, '0');
    (void)fmt_append_str(line, p, sizeof(line), num);
    gfx_draw_text((uint16_t)(CARD_X[1] + 6u), (uint16_t)(CARD_Y + 37u), line, COL_MUTED, COL_CARD, 1u);

    gfx_draw_sparkline(
        (uint16_t)(CARD_X[1] + 5u), (uint16_t)(CARD_Y + 50u),
        (uint16_t)(CARD_W - 10u), 32u,
        snap->cur_hist, snap->hist_count, snap->hist_head, 4095u,
        COL_EMERALD, COL_SPARK_BG
    );

    /* Card 3: AUX (PA3 / CH3) */
    fmt_uint_pad(num, sizeof(num), snap->aux_avg, 4u, ' ');
    gfx_draw_text((uint16_t)(CARD_X[2] + 6u), (uint16_t)(CARD_Y + 18u), num, COL_TEXT, COL_CARD, 2u);
    gfx_draw_text((uint16_t)(CARD_X[2] + 58u), (uint16_t)(CARD_Y + 24u), "raw", COL_MUTED, COL_CARD, 1u);

    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "mV:");
    fmt_uint_pad(num, sizeof(num), snap->aux_mv, 4u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), " r:");
    fmt_uint_pad(num, sizeof(num), snap->aux_raw, 4u, '0');
    (void)fmt_append_str(line, p, sizeof(line), num);
    gfx_draw_text((uint16_t)(CARD_X[2] + 6u), (uint16_t)(CARD_Y + 37u), line, COL_MUTED, COL_CARD, 1u);

    gfx_draw_sparkline(
        (uint16_t)(CARD_X[2] + 5u), (uint16_t)(CARD_Y + 50u),
        (uint16_t)(CARD_W - 10u), 32u,
        snap->aux_hist, snap->hist_count, snap->hist_head, 4095u,
        COL_AMBER, COL_SPARK_BG
    );

    /* Key tiles */
    for (uint32_t i = 0u; i < KEY_COUNT; ++i) {
        uint8_t on = (inp->held_mask & (1u << i)) != 0u;
        gfx_draw_badge(
            KEY_TILE_X[i], 130u, 56u, 22u, KEY_LABELS[i],
            on ? COL_CYAN : COL_TEXT,
            on ? COL_CYAN_DARK : COL_BG,
            on ? COL_CYAN : COL_BORDER
        );
    }

    /* GPIO / EXINT status line */
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "PB5(SLP):");
    p = fmt_append_str(line, p, sizeof(line), inp->pb5_sleep ? "1" : "0");
    p = fmt_append_str(line, p, sizeof(line), " cnt:");
    fmt_uint_pad(num, sizeof(num), inp->pb5_edges % 100000u, 5u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "  PB0:");
    p = fmt_append_str(line, p, sizeof(line), inp->pb0_raw ? "1" : "0");
    p = fmt_append_str(line, p, sizeof(line), "  PB2:");
    p = fmt_append_str(line, p, sizeof(line), inp->pb2_raw ? "1" : "0");
    p = fmt_append_str(line, p, sizeof(line), "  HDL:");
    (void)fmt_append_str(line, p, sizeof(line), inp->pb6_handle ? "1" : "0");
    gfx_draw_text(14u, 159u, line, COL_TEXT, COL_CARD, 1u);

    /* Safety, backlight & LED line */
    p = 0u;
    p = fmt_append_str(line, p, sizeof(line), "PA2(HTR):0 [LOCKED]  BL:");
    fmt_uint_pad(num, sizeof(num), st->backlight_pct, 3u, ' ');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), "%  LED:0x");
    fmt_hex8(num, sizeof(num), st->led_mask);
    (void)fmt_append_str(line, p, sizeof(line), num);
    gfx_draw_text(14u, 175u, line, COL_RED, COL_CARD, 1u);

    /* Stream / USB debug & uptime line */
    uint32_t total_s = st->uptime_ms / 1000u;
    uint32_t hh = (total_s / 3600u) % 100u;
    uint32_t mm = (total_s / 60u) % 60u;
    uint32_t ss = total_s % 60u;
    p = 0u;
    if (st->usb_connected != 0u) {
        p = fmt_append_str(line, p, sizeof(line), "STREAM:");
        p = fmt_append_str(line, p, sizeof(line), st->stream_enabled ? "ON " : "OFF");
        p = fmt_append_str(line, p, sizeof(line), " (");
        fmt_uint_pad(num, sizeof(num), st->stream_interval_ms, 4u, ' ');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "ms)  UP:");
    } else {
        p = fmt_append_str(line, p, sizeof(line), "USB I:");
        fmt_uint_pad(num, sizeof(num), st->usb_irq_cnt % 1000u, 3u, '0');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), " R:");
        fmt_uint_pad(num, sizeof(num), st->usb_rst_cnt % 100u, 2u, '0');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), " S:");
        fmt_uint_pad(num, sizeof(num), st->usb_setup_cnt % 100u, 2u, '0');
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), " G:");
        fmt_hex8(num, sizeof(num), st->usb_gccfg_hi);
        p = fmt_append_str(line, p, sizeof(line), num);
        p = fmt_append_str(line, p, sizeof(line), "  UP:");
    }
    fmt_uint_pad(num, sizeof(num), hh, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), ":");
    fmt_uint_pad(num, sizeof(num), mm, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    p = fmt_append_str(line, p, sizeof(line), ":");
    fmt_uint_pad(num, sizeof(num), ss, 2u, '0');
    p = fmt_append_str(line, p, sizeof(line), num);
    if (st->usb_connected != 0u) {
        (void)fmt_append_str(line, p, sizeof(line), "     ");
    }
    gfx_draw_text(14u, 191u, line, COL_MUTED, COL_CARD, 1u);
}
