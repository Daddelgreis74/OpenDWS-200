#pragma once

/* OpenDWS-200 Firmware Configuration (Phase 1 - Safe Bring-Up) */
#ifndef OPENDWS_VERSION_STR
#define OPENDWS_VERSION_STR "v0.1.1-bringup"
#endif

/* Phase 1 Safe Bring-Up: Heater output (PA2) is permanently locked OFF. */
#define OPENDWS_SAFE_MODE    1
#define OPENDWS_ALLOW_HEATER 0

#if OPENDWS_ALLOW_HEATER != 0
#error "Phase 1 build must keep OPENDWS_ALLOW_HEATER == 0!"
#endif

/* Flash & Memory Map (AT32F415RCT7: 256 KB Flash, 32 KB SRAM) */
#define APP_BASE_ADDR       0x08006000u
#define APP_FLASH_MAX_BYTES 0x00039800u /* 230 KB: 0x08006000 .. 0x0803F7FF */
#define SETTINGS_FLASH_ADDR 0x0803F800u /* Last 2 KB Flash page (read-only in Phase 1) */
#define SRAM_BASE_ADDR      0x20000000u
#define SRAM_SIZE_BYTES     0x00008000u /* 32 KB */

/* System Clocks (AT32F415RCT7 @ 144 MHz from 8 MHz HEXT) */
#define HEXT_HZ             8000000u
#define SYSCLK_HZ           144000000u
#define AHB_HZ              144000000u
#define APB1_HZ             72000000u
#define APB2_HZ             72000000u
#define TMR3_CLK_HZ         144000000u

/* ST7789V Display */
#define LCD_WIDTH           320u
#define LCD_HEIGHT          240u

/* Timing & Safety */
#define WDT_TIMEOUT_MS      2000u
#define SENSOR_SAMPLE_MS    50u
#define UI_REFRESH_MS       66u
#define STREAM_DEFAULT_MS   100u
#define STREAM_MIN_MS       50u
#define STREAM_MAX_MS       2000u
