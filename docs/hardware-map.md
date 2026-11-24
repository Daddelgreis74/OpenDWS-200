# FNIRSI DWS-200 Hardware Map (`AT32F415RCT7`)

All registers, clock dividers, and pin assignments were verified against the official stock firmware `APP_DWS_200_V1.5_250922.bin`.

## 1. Microcontroller & Memory Layout

| Property | Value |
| :--- | :--- |
| **MCU** | Artery `AT32F415RCT7` (LQFP64) |
| **Core** | ARM Cortex-M4 **without hardware FPU** (`-mfloat-abi=soft`) |
| **System Clock** | `144 MHz` from `8 MHz` external crystal (`HEXT / 2 × 36`) |
| **Bus Clocks** | `AHB = 144 MHz`, `APB1 = 72 MHz`, `APB2 = 72 MHz`, `ADC = 12 MHz`, `USB = 48 MHz` (`PLL / 3`) |
| **SRAM** | `32 KB` (`0x20000000 – 0x20007FFF`) |
| **Bootloader** | `0x08000000 – 0x08005FFF` (`24 KB`, USB Mass Storage — unmodified) |
| **OpenDWS App Flash** | `0x08006000 – 0x0803F7FF` (`230 KB` maximum usable size) |
| **Settings Flash Page** | `0x0803F800 – 0x0803FFFF` (`2 KB`, read-only in Phase 1; CRC32-protected settings page in `v1.0.0`) |

## 2. Clock & CRM Configuration

- `FLASH_PSR` (`0x40022000`): `|= 0x154` (wait states + prefetch for `144 MHz`)
- `CRM_CFG` (`0x40021004`):
  - `PLLSRC = 1` (`HEXT`), `PLLHEXTDIV = 1` (`4 MHz`), `PLLMULT = 0x23` (`×36 = 144 MHz`)
  - `USBDIV = 0b101` (`/3 = 48 MHz`, bits `[23:22] = 0b01` and bit `27 = 1`)
  - `ADCDIV = 0b010` (`/6 = 12 MHz`)
- `CRM_MISC2` (`0x40021054`): Bits `[5:4] = 0b11` (`AUTO_STEP_EN`) enabled while switching to the `144 MHz` PLL, then cleared.

## 3. Pinout Table

| Pin | Function | Mode / Configuration | Description |
| :--- | :--- | :--- | :--- |
| `PA2` | **Heater (`TMR1`/`TMR2`)** | Output Push-Pull (boots `LOW`) | Always starts in safe `STANDBY` (`LOW`); pulsed `10 ms` control via `TMR1` (blanking) + `TMR2` (`91 µs`, see [phase2-control-analysis.md](phase2-control-analysis.md)) |
| `PB1` | Soft-Power Latch | Output Push-Pull `HIGH` | Keeps power rail latched on; pulled `LOW` via `PB0` power-off sequence |
| `PB12` | Display `CS` | Output Push-Pull (`HIGH` idle) | ST7789V Chip Select |
| `PB13` | Display `SCK` | AF Push-Pull 50 MHz | `SPI2_SCK` (Mode 3, `18 MHz`) |
| `PB14` | Display `MISO` | Input Floating | `SPI2_MISO` |
| `PB15` | Display `MOSI` | AF Push-Pull 50 MHz | `SPI2_MOSI` |
| `PC6` | Display `DC` | Output Push-Pull | `LOW` = Command, `HIGH` = Data |
| `PC7` | Display `RST` | Output Push-Pull | Active-LOW hardware reset |
| `PC8` | Piezo Buzzer | AF Push-Pull (`TMR3_CH3`) | ~2.7 kHz PWM (`IOMUX_REMAP4` `[11:8]=0x3`) |
| `PC9` | Display Backlight | AF Push-Pull (`TMR3_CH4`) | ~2.7 kHz PWM (`IOMUX_REMAP4` `[11:8]=0x3`), Active-HIGH |
| `PC0` | `ADC1_IN10` | Analog (239.5 cycles) | Tip thermocouple (`1.0 ms` blanking after `PA2 = LOW`) |
| `PC1` | `ADC1_IN11` | Analog (55.5 cycles) | Hall current sensor (`66 mV/A`, `1.65 V` midpoint, overcurrent check at `364 µs`) |
| `PA3` | `ADC1_IN3` | Analog (55.5 cycles) | `24 V` rail monitor (`V_rail = adc_V * 16.0`; `1.434 V × 16 = 22.94 V`) |
| `PC11` | Key 1 (left): `CH / Return` | Input Pull-Up (Active-LOW) | Cycles `CH1/2/3` presets or clears active fault (`CLEAR`); exits edit/menu in menu mode |
| `PC12` | Key 2: `Left (<)` | Input Pull-Up (Active-LOW) | Target `-5 °C` (long press `-25 °C`); scrolls up / decrements in menu |
| `PD2` | Key 3 (center): `MODE / OK` | Input Pull-Up (Active-LOW) | Main: toggles Main ↔ `GRAPH` view (does **not** toggle heater; long press saves to Flash); Menu: enters/confirms (`OK`) |
| `PB3` | Key 4: `Right (>)` | Input Pull-Up (Active-LOW) | Target `+5 °C` (long press `+25 °C`); scrolls down / increments in menu |
| `PB4` | Key 5 (right): `MENU` | Input Pull-Up (Active-LOW) | Opens and closes the **on-screen settings menu** (Sleep `100 °C`, Auto Stand, **Standby Screen**, **Auto Off**, Presets, 3-Point Calibration, Backlight, Buzzer, Stream, Save) |
| `PB0` | Soft-Power Button | Input Pull-Up | Momentary switch (`HIGH→LOW→HIGH` on release saves modified settings and drives `PB1 = LOW`) |
| `PB5` | Sleep Stand | Input Pull-Down + `EXINT5` | `LOW` = in stand (**Standby screen + `100 °C` preheat**), `HIGH` = lifted (**heats automatically to setpoint**; safe `0 W` if booted outside stand) |
| `PB2` | Diag Input | Input Pull-Down | Live diagnostics input |
| `PB10, PB11` | Bit-Bang `I2C` | Output / Open-Drain | Auxiliary bus for slave address `0x78` (optional power/display IC) |
| `PF4, PF5` | Front LEDs 0..1 | Output Push-Pull | Status LEDs |
| `PA4..PA7` | Front LEDs 2..5 | Output Push-Pull | Status LEDs |
| `PC4, PC5` | Front LEDs 6..7 | Output Push-Pull | Status LEDs |
| `PA11, PA12` | USB `D-` / `D+` | `OTGFS1` (`0x50000000`) | USB 2.0 Full-Speed CDC-ACM (IRQ 67) |

## 4. ST7789V Display (`320 × 240`, RGB565)

- SPI Mode 3 (`CLKPOL=1, CLKPHA=1`), 8-bit MSB-first, `18 MHz` (`APB1 / 4`).
- `MADCTL (0x36) = 0xA0`, `COLMOD (0x3A) = 0x05`, inversion off (`0x21` off), X/Y offset `0, 0`.

## 5. USB & IOMUX Notes (`AT32F415`)

- `OTG_GCCFG` (`0x50000038`) must set `0x002D0000` (`PWRDOWN | AVALIDSEN | BVALIDSEN | VBUSIG` = bits `16, 18, 19, 21`) **before** switching `OTG_GUSBCFG` into device mode (`FDEVMODE`, `TRDT = 5`).
- `IOMUX_REMAP` (`0x40010004`) sets `SWJTAG_CONF = 0b010` to free `PB3`/`PB4` from JTAG while keeping SWD active.
- `IOMUX_REMAP4` (`0x40010024`) sets bits `[11:8] = 0x3` (`TMR3_GMUX_0011`) to route `TMR3_CH3/CH4` to `PC8/PC9` (verified from `APP_DWS_200_V1.5` `0x08013152`).

## 6. Verified Bring-Up Measurements (`v0.1.1-bringup`, COM7)

- **USB-CDC Enumeration:** `USB\VID_2E3C&PID_5740\DWS200-P1` (`COM7`, `144 MHz` PLL / `48 MHz` USB verified).
- **`PC0` (`TIP`):** `11..25 raw` (`9..20 mV`) at room temperature.
- **`PC1` (`CUR`):** `2037..2038 raw` (`1642 mV`, ~`-8 mV` idle offset relative to ideal `1650 mV`).
- **`PA3` (`AUX` / `24 V` rail):** Initial `180 raw` (`145 mV × 16 = 2.32 V` USB-only), powered on `1777..1780 raw` (`1432..1434 mV × 16 = 22.94 V`); see [phase2-control-analysis.md](phase2-control-analysis.md) for PID, LUT, and timing details.
- **`PB5` (`EXINT5`):** Confirmed mechanical stand contact (`pb5_cnt = 115` bounce edges when rocking in stand, stable when stationary).
- **Phase-1 Safety Lock:** `PA2` stayed permanently `LOW` (`heater=0`); `HEAT 1` and `TEMP 350` returned `ERR HEATER_LOCKED_IN_SAFE_MODE`.
