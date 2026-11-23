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

void gfx_draw_dual_graph(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    const uint16_t *temp_samples,
    const uint8_t *pwr_samples,
    uint8_t count,
    uint8_t head,
    uint16_t temp_max,
    uint16_t target_c,
    uint16_t temp_col,
    uint16_t pwr_col,
    uint16_t pwr_fill_col,
    uint16_t target_col,
    uint16_t bg
) {
    if (w == 0u || h < 8u || temp_samples == NULL || pwr_samples == NULL || temp_max == 0u) return;
    if (w > 300u) w = 300u;

    uint16_t grid = RGB565(28, 42, 66);
    uint16_t strip_rows = (uint16_t)(sizeof(g_tile_buf) / (sizeof(uint16_t) * w));
    if (strip_rows == 0u) return;

    uint16_t target_y = 0u;
    uint8_t has_target = 0u;
    if (target_c > 0u) {
        uint32_t tval = (target_c > temp_max) ? temp_max : target_c;
        target_y = (uint16_t)((h - 1u) - ((tval * (uint32_t)(h - 2u)) / temp_max));
        has_target = 1u;
    }

    uint16_t vx1 = (uint16_t)(w / 4u);
    uint16_t vx2 = (uint16_t)(w / 2u);
    uint16_t vx3 = (uint16_t)((3u * (uint32_t)w) / 4u);
    uint16_t hy1 = (uint16_t)(h / 4u);
    uint16_t hy2 = (uint16_t)(h / 2u);
    uint16_t hy3 = (uint16_t)((3u * (uint32_t)h) / 4u);

    for (uint16_t row0 = 0u; row0 < h; row0 = (uint16_t)(row0 + strip_rows)) {
        uint16_t chunk_h = (uint16_t)(h - row0);
        if (chunk_h > strip_rows) chunk_h = strip_rows;

        for (uint16_t r = 0u; r < chunk_h; ++r) {
            uint16_t gy = (uint16_t)(row0 + r);
            uint8_t is_hgrid = (gy == hy1 || gy == hy2 || gy == hy3) ? 1u : 0u;
            uint32_t off = (uint32_t)r * w;
            for (uint16_t c = 0u; c < w; ++c) {
                uint8_t is_vgrid = (c == vx1 || c == vx2 || c == vx3) ? 1u : 0u;
                uint16_t col = (is_hgrid || is_vgrid) ? grid : bg;
                if (has_target && gy == target_y && (c & 7u) < 4u) {
                    col = target_col;
                }
                g_tile_buf[off + c] = col;
            }
        }

        if (count > 0u) {
            uint16_t prev_pwr_y = (uint16_t)(h - 1u);
            uint16_t prev_temp_y = (uint16_t)(h - 1u);
            for (uint16_t px = 0u; px < w; ++px) {
                uint32_t idx_rel = ((uint32_t)px * count) / w;
                uint32_t oldest = (head + 64u - count) & 63u;
                uint32_t idx = (oldest + idx_rel) & 63u;

                uint32_t pval = pwr_samples[idx];
                if (pval > 100u) pval = 100u;
                uint16_t pwr_y = (uint16_t)((h - 1u) - ((pval * (uint32_t)(h - 2u)) / 100u));
                if (px == 0u) prev_pwr_y = pwr_y;

                /* Fill power area strictly below pwr_y */
                uint16_t fill_start = (uint16_t)(pwr_y + 1u);
                if (fill_start < (uint16_t)(row0 + chunk_h)) {
                    uint16_t y0 = (fill_start > row0) ? fill_start : row0;
                    uint16_t y1 = (uint16_t)(row0 + chunk_h);
                    for (uint16_t ly = y0; ly < y1; ++ly) {
                        g_tile_buf[(uint32_t)(ly - row0) * w + px] = pwr_fill_col;
                    }
                }

                /* Power trace */
                uint16_t py_lo = (pwr_y < prev_pwr_y) ? pwr_y : prev_pwr_y;
                uint16_t py_hi = (pwr_y > prev_pwr_y) ? pwr_y : prev_pwr_y;
                for (uint16_t ly = py_lo; ly <= py_hi; ++ly) {
                    if (ly >= row0 && ly < row0 + chunk_h) {
                        g_tile_buf[(uint32_t)(ly - row0) * w + px] = pwr_col;
                    }
                }
                prev_pwr_y = pwr_y;

                /* Target dashed line redrawn cleanly above power fill if uncovered by temp */
                if (has_target && target_y >= row0 && target_y < row0 + chunk_h && (px & 7u) < 4u) {
                    g_tile_buf[(uint32_t)(target_y - row0) * w + px] = target_col;
                }

                /* Temperature trace (2px thick so it stands out over power curve) */
                uint32_t tval = temp_samples[idx];
                if (tval > temp_max) tval = temp_max;
                uint16_t temp_y = (uint16_t)((h - 1u) - ((tval * (uint32_t)(h - 2u)) / temp_max));
                if (px == 0u) prev_temp_y = temp_y;

                uint16_t ty_lo = (temp_y < prev_temp_y) ? temp_y : prev_temp_y;
                uint16_t ty_hi = (temp_y > prev_temp_y) ? temp_y : prev_temp_y;
                if ((uint16_t)(ty_hi + 1u) < h) ty_hi++;
                for (uint16_t ly = ty_lo; ly <= ty_hi; ++ly) {
                    if (ly >= row0 && ly < row0 + chunk_h) {
                        g_tile_buf[(uint32_t)(ly - row0) * w + px] = temp_col;
                    }
                }
                prev_temp_y = temp_y;
            }
        }
        display_blit_rgb565(x, (uint16_t)(y + row0), w, chunk_h, g_tile_buf);
    }
}
