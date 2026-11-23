#pragma once

#include <stddef.h>
#include <stdint.h>

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | (((b) & 0xF8u) >> 3)))

void gfx_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gfx_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void gfx_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void gfx_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gfx_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale);
void gfx_draw_text(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale);
void gfx_draw_badge(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *label, uint16_t fg, uint16_t bg, uint16_t border);
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
);
