#pragma once

/* OpenDWS-200 Firmware Configuration (Phase 2 - Closed-Loop PID & Safety) */
#ifndef OPENDWS_VERSION_STR
#define OPENDWS_VERSION_STR "v0.2.1-phase2"
#endif

/* Phase 2: Heater enabled ONLY on explicit user command, ALWAYS boots in STANDBY (PA2 LOW). */
#define OPENDWS_SAFE_MODE       0
#define OPENDWS_ALLOW_HEATER    1
#define OPENDWS_BOOT_STANDBY    1

#if OPENDWS_BOOT_STANDBY != 1
#error "OpenDWS-200 must ALWAYS boot in STANDBY with PA2 LOW!"
#endif

/* Flash & Memory Map (AT32F415RCT7: 256 KB Flash, 32 KB SRAM) */
#define APP_BASE_ADDR           0x08006000u
#define APP_FLASH_MAX_BYTES     0x00039800u /* 230 KB: 0x08006000 .. 0x0803F7FF */
#define SETTINGS_FLASH_ADDR     0x0803F800u /* Last 2 KB Flash page */
#define SRAM_BASE_ADDR          0x20000000u
#define SRAM_SIZE_BYTES         0x00008000u /* 32 KB */

/* System Clocks (AT32F415RCT7 @ 144 MHz from 8 MHz HEXT) */
#define HEXT_HZ                 8000000u
#define SYSCLK_HZ               144000000u
#define AHB_HZ                  144000000u
#define APB1_HZ                 72000000u
#define APB2_HZ                 72000000u
#define TMR1_CLK_HZ             144000000u
#define TMR2_CLK_HZ             144000000u
#define TMR3_CLK_HZ             144000000u

/* ST7789V Display */
#define LCD_WIDTH               320u
#define LCD_HEIGHT              240u

/* Loop & Stream Timing */
#define WDT_TIMEOUT_MS          2000u
#define SENSOR_SAMPLE_MS        50u
#define UI_REFRESH_MS           66u
#define STREAM_DEFAULT_MS       100u
#define STREAM_MIN_MS           50u
#define STREAM_MAX_MS           2000u

/* Temperature Limits & Presets (degrees C) */
#define TEMP_MIN_C              100u
#define TEMP_MAX_C              450u
#define TEMP_DEFAULT_C          300u
#define TEMP_HARD_CUTOFF_C      480u
#define SLEEP_TEMP_MIN_C        50u
#define SLEEP_TEMP_MAX_C        250u
#define SLEEP_TEMP_DEFAULT_C    100u
#define CAL_OFFSET_MIN_C        (-50)
#define CAL_OFFSET_MAX_C        50

/* Hardware Protection & Blanking Constants (10 ms control period = 100 Hz) */
#define RAIL_MIN_MV             15000u
#define RAIL_MAX_MV             30000u
#define OPEN_TIP_ADC            4000u
#define CUR_ZERO_DEFAULT_MV     1650u
#define CUR_LIMIT_F245_MA       10000
#define CUR_LIMIT_F210_MA       15000
#define CUR_OVERLOAD_MA         5000
#define CUR_LOW_LOAD_MA         5000
#define OVERLOAD_MAX_TICKS      3000u  /* 30 s @ 100 Hz */
#define RUNAWAY_MAX_TICKS       800u   /* 8 s @ 100 Hz */
#define RUNAWAY_MIN_DUTY        25u
#define RUNAWAY_MIN_RISE_DC     50     /* 5.0 C rise required */
#define OPEN_TIP_RECOVER_TICKS  50u    /* 500 ms @ 100 Hz */
#define PROBE_PULSE_TICKS       5u     /* 5 * 91 us = 455 us short-circuit probe */
#define HANDLE_DEBOUNCE_TICKS   5u     /* 50 ms @ 100 Hz */
#define HANDLE_SETTLE_TICKS     100u   /* 1.0 s @ 100 Hz */
#define STAND_IN_DEBOUNCE_TICKS 50u    /* 500 ms @ 100 Hz */
#define STAND_OUT_DEBOUNCE_TICKS 8u    /* 80 ms @ 100 Hz */
#define MAX_DUTY_F245_COLD      80u    /* T < 100 C -> 7.28 ms */
#define MAX_DUTY_F245_HOT       90u    /* T >= 100 C -> 8.19 ms (>= 1 ms blanking before Slot 2) */
#define MAX_DUTY_F210           30u    /* Hard 2.73 ms cap (~27.3%) for F210 @ 24 V */
