#pragma once

#include <stdint.h>

void display_init(void);
void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
void display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);

#ifdef OPENDWS_HOST_TEST
void host_display_init(void);
void host_display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
void host_display_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
#endif
