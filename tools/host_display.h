#pragma once

#include "app_config.h"
#include "display.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint16_t host_fb[LCD_HEIGHT][LCD_WIDTH];

void host_display_init(void) {
    memset(host_fb, 0, sizeof(host_fb));
}

void host_display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0u || h == 0u) return;
    uint32_t x_end = (uint32_t)x + w;
    uint32_t y_end = (uint32_t)y + h;
    if (x_end > LCD_WIDTH) x_end = LCD_WIDTH;
    if (y_end > LCD_HEIGHT) y_end = LCD_HEIGHT;
    for (uint32_t py = y; py < y_end; ++py) {
        for (uint32_t px = x; px < x_end; ++px) {
            host_fb[py][px] = rgb565;
        }
    }
}

void host_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    if (pixels == NULL || x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0u || h == 0u) return;
    for (uint32_t row = 0u; row < h; ++row) {
        uint32_t py = (uint32_t)y + row;
        if (py >= LCD_HEIGHT) break;
        for (uint32_t col = 0u; col < w; ++col) {
            uint32_t px = (uint32_t)x + col;
            if (px >= LCD_WIDTH) break;
            host_fb[py][px] = pixels[row * w + col];
        }
    }
}

static inline uint16_t host_display_pixel(uint16_t x, uint16_t y) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return 0u;
    return host_fb[y][x];
}

static inline int host_display_write_ppm(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P6\n%u %u\n255\n", (unsigned)LCD_WIDTH, (unsigned)LCD_HEIGHT);
    for (uint32_t y = 0u; y < LCD_HEIGHT; ++y) {
        for (uint32_t x = 0u; x < LCD_WIDTH; ++x) {
            uint16_t c = host_fb[y][x];
            uint8_t rgb[3] = {
                (uint8_t)(((c >> 11) & 0x1Fu) * 255u / 31u),
                (uint8_t)(((c >> 5) & 0x3Fu) * 255u / 63u),
                (uint8_t)((c & 0x1Fu) * 255u / 31u)
            };
            if (fwrite(rgb, 1u, 3u, fp) != 3u) {
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);
    return 0;
}
