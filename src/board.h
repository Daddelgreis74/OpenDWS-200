#pragma once

#include <stdint.h>

void board_init(void);
void board_heater_force_off(void);
void board_heater_set(uint8_t on);
uint8_t board_heater_is_on(void);
void board_set_handle_mux(uint8_t is_f245);
uint8_t board_get_handle_mux(void);
uint8_t board_read_pb6_raw(void);
uint8_t board_read_pb5_raw(void);
void board_timers_init(void);
void board_timers_pause(void);
void board_timers_resume(void);
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
