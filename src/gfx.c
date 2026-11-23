#include "gfx.h"

#include "app_config.h"
#include "display.h"
#include "font.h"

/* Small shared blit tile buffer (2.5 KB in BSS, safe within 32 KB SRAM) */
static uint16_t g_tile_buf[1280];

void gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    display_fill_rect(x, y, w, h, color);
}

void gfx_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    display_fill_rect(x, y, w, 1u, color);
}

void gfx_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color) {
    display_fill_rect(x, y, 1u, h, color);
}

void gfx_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0u || h == 0u) return;
    gfx_draw_hline(x, y, w, color);
    gfx_draw_hline(x, (uint16_t)(y + h - 1u), w, color);
    gfx_draw_vline(x, y, h, color);
    gfx_draw_vline((uint16_t)(x + w - 1u), y, h, color);
}

void gfx_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (scale == 0u) scale = 1u;
    if (scale > 5u) scale = 5u;

    uint16_t gw = (uint16_t)(FONT5X7_ADVANCE * scale);
    uint16_t gh = (uint16_t)(FONT5X7_HEIGHT * scale);
    const uint8_t *glyph = font5x7_glyph(ch);

    for (uint16_t py = 0u; py < gh; ++py) {
        uint16_t sy = (uint16_t)(py / scale);
        uint32_t row_off = (uint32_t)py * gw;
        for (uint16_t px = 0u; px < gw; ++px) {
            uint16_t sx = (uint16_t)(px / scale);
            uint16_t col = bg;
            if (sx < FONT5X7_WIDTH && ((glyph[sx] >> sy) & 1u) != 0u) {
                col = fg;
            }
            g_tile_buf[row_off + px] = col;
        }
    }
    display_blit_rgb565(x, y, gw, gh, g_tile_buf);
}

void gfx_draw_text(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (str == NULL) return;
    if (scale == 0u) scale = 1u;
    if (scale > 5u) scale = 5u;

    uint16_t step = (uint16_t)(FONT5X7_ADVANCE * scale);
    uint16_t cur_x = x;
    while (*str != '\0') {
        if ((uint32_t)cur_x + step > LCD_WIDTH) {
            break;
        }
        gfx_draw_char(cur_x, y, *str, fg, bg, scale);
        cur_x = (uint16_t)(cur_x + step);
        str++;
    }
}

void gfx_draw_badge(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const char *label,
    uint16_t fg,
    uint16_t bg,
    uint16_t border
) {
    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, border);
    if (label == NULL) return;

    uint16_t len = 0u;
    while (label[len] != '\0') len++;
    uint16_t text_w = (uint16_t)(len * FONT5X7_ADVANCE);
    uint16_t tx = (w > text_w) ? (uint16_t)(x + (w - text_w) / 2u) : (uint16_t)(x + 2u);
    uint16_t ty = (h > FONT5X7_HEIGHT) ? (uint16_t)(y + (h - FONT5X7_HEIGHT) / 2u) : (uint16_t)(y + 1u);
    gfx_draw_text(tx, ty, label, fg, bg, 1u);
}

void gfx_draw_sparkline(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t *samples,
    uint8_t count,
    uint8_t head,
    uint16_t max_val,
    uint16_t fg,
    uint16_t bg
) {
    if (w == 0u || h < 4u || samples == NULL || max_val == 0u) return;
    if (w > 160u) w = 160u;

    uint16_t grid = RGB565(28, 42, 66);
    uint16_t strip_rows = (uint16_t)(sizeof(g_tile_buf) / (sizeof(uint16_t) * w));
    if (strip_rows == 0u) return;

    for (uint16_t row0 = 0u; row0 < h; row0 = (uint16_t)(row0 + strip_rows)) {
        uint16_t chunk_h = (uint16_t)(h - row0);
        if (chunk_h > strip_rows) chunk_h = strip_rows;

        for (uint16_t r = 0u; r < chunk_h; ++r) {
            uint16_t gy = (uint16_t)(row0 + r);
            uint16_t base_col = (gy == h / 2u) ? grid : bg;
            uint32_t off = (uint32_t)r * w;
            for (uint16_t c = 0u; c < w; ++c) {
                g_tile_buf[off + c] = base_col;
            }
        }

        if (count > 0u) {
            uint16_t prev_local = h - 1u;
            for (uint16_t px = 0u; px < w; ++px) {
                uint32_t idx_rel = ((uint32_t)px * count) / w;
                uint32_t oldest = (head + 64u - count) & 63u;
                uint32_t idx = (oldest + idx_rel) & 63u;
                uint32_t val = samples[idx];
                if (val > max_val) val = max_val;
                uint16_t py = (uint16_t)((h - 1u) - ((val * (uint32_t)(h - 2u)) / max_val));
                if (px == 0u) prev_local = py;

                uint16_t y_lo = (py < prev_local) ? py : prev_local;
                uint16_t y_hi = (py > prev_local) ? py : prev_local;
                for (uint16_t ly = y_lo; ly <= y_hi; ++ly) {
                    if (ly >= row0 && ly < row0 + chunk_h) {
                        g_tile_buf[(uint32_t)(ly - row0) * w + px] = fg;
                    }
                }
                prev_local = py;
            }
        }
        display_blit_rgb565(x, (uint16_t)(y + row0), w, chunk_h, g_tile_buf);
    }
}
