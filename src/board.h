#pragma once

#include <stdint.h>

void board_init(void);
void board_heater_force_off(void);
void board_set_backlight(uint8_t pct);
uint8_t board_get_backlight(void);
void board_buzzer_beep(uint16_t ms);
void board_buzzer_tick(uint32_t now_ms);
void board_set_leds(uint8_t mask);
uint8_t board_get_leds(void);
uint32_t board_ticks_ms(void);
void board_delay_ms(uint32_t ms);
void board_wdt_feed(void);
void board_reboot(void);
