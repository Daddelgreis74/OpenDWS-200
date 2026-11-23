# FNIRSI DWS-200 Hardware Map (AT32F415RCT7)

Alle Register, Taktteiler und Pinbelegungen wurden aus der offiziellen Firmware `APP_DWS_200_V1.5_250922.bin` verifiziert.

## 1. Mikrocontroller & Speicher-Layout

| Eigenschaft | Wert |
| :--- | :--- |
| **MCU** | Artery `AT32F415RCT7` (LQFP64) |
| **Kern** | ARM Cortex-M4 **ohne Hardware-FPU** (`-mfloat-abi=soft`) |
| **Systemtakt** | `144 MHz` aus `8 MHz` externem Quarz (`HEXT / 2 × 36`) |
| **Bus-Takte** | `AHB = 144 MHz`, `APB1 = 72 MHz`, `APB2 = 72 MHz`, `ADC = 12 MHz`, `USB = 48 MHz` (`PLL / 3`) |
| **SRAM** | `32 KB` (`0x20000000 – 0x20007FFF`) |
| **Bootloader** | `0x08000000 – 0x08005FFF` (`24 KB`, USB Mass Storage — unverändert) |
| **OpenDWS App Flash** | `0x08006000 – 0x0803F7FF` (`230 KB` maximal nutzbar) |
| **Settings Flash Page** | `0x0803F800 – 0x0803FFFF` (`2 KB`, in Phase 1 nur lesend genutzt) |

## 2. Takt & CRM-Konfiguration

- `FLASH_PSR` (`0x40022000`): `|= 0x154` (Waitstates + Prefetch für 144 MHz)
- `CRM_CFG` (`0x40021004`):
  - `PLLSRC = 1` (`HEXT`), `PLLHEXTDIV = 1` (`4 MHz`), `PLLMULT = 0x23` (`×36 = 144 MHz`)
  - `USBDIV = 0b101` (`/3 = 48 MHz`, Bits `[23:22] = 0b01` und Bit `27 = 1`)
  - `ADCDIV = 0b010` (`/6 = 12 MHz`)
- `CRM_MISC2` (`0x40021054`): Bits `[5:4] = 0b11` (`AUTO_STEP_EN`) beim Umschalten auf 144-MHz-PLL aktiv und danach wieder deaktiviert.

## 3. Pinout-Tabelle

| Pin | Funktion | Modus / Konfiguration | Beschreibung |
| :--- | :--- | :--- | :--- |
| `PA2` | **Heizung (`TMR1`/`TMR2`)** | Output Push-Pull (Start `LOW`) | Startet immer im sicheren `STANDBY` (`LOW`); gepulste `10 ms`-Regelung über `TMR1` (Blanking) + `TMR2` (`91 µs`, siehe [phase2-control-analysis.md](phase2-control-analysis.md)) |
| `PB1` | Soft-Power-Latch | Output Push-Pull `HIGH` | Hält Versorgung aktiv; wird über `PB0`-Flankensequenz auf `LOW` abgeschaltet |
| `PB12` | Display `CS` | Output Push-Pull (`HIGH` idle) | ST7789V Chip Select |
| `PB13` | Display `SCK` | AF Push-Pull 50 MHz | `SPI2_SCK` (Mode 3, 18 MHz) |
| `PB14` | Display `MISO` | Input Floating | `SPI2_MISO` |
| `PB15` | Display `MOSI` | AF Push-Pull 50 MHz | `SPI2_MOSI` |
| `PC6` | Display `DC` | Output Push-Pull | `LOW`=Command, `HIGH`=Data |
| `PC7` | Display `RST` | Output Push-Pull | Active-LOW Hardware-Reset |
| `PC8` | Piezo-Buzzer | AF Push-Pull (`TMR3_CH3`) | ~2,7 kHz PWM (`IOMUX_REMAP4` `[11:8]=0x3`) |
| `PC9` | Display Backlight | AF Push-Pull (`TMR3_CH4`) | ~2,7 kHz PWM (`IOMUX_REMAP4` `[11:8]=0x3`), Active-HIGH |
| `PC0` | `ADC1_IN10` | Analog (239,5 Cycl.) | Lötspitzen-Thermoelement (Blanking `1,0 ms` nach `PA2 = LOW`) |
| `PC1` | `ADC1_IN11` | Analog (55,5 Cycl.) | Hall-Stromsensor (`66 mV/A`, Nullpunkt `1,65 V`, Kurzschluss-Check nach `364 µs`) |
| `PA3` | `ADC1_IN3` | Analog (55,5 Cycl.) | 24-V-Rail-Monitor (`V_rail = adc_V * 16,0`; `1,434 V × 16 = 22,94 V`) |
| `PC11` | Taste 1 (links): `CH / Return` | Input Pull-Up (Active-LOW) | Phase 2: Preset-Wechsel (`CH1/2/3`) bzw. `CLEAR` bei Fault |
| `PC12` | Taste 2: `Pfeil links (<)` | Input Pull-Up (Active-LOW) | Phase 2: Solltemperatur `-5 °C` (Langdruck `-25 °C`) |
| `PD2` | Taste 3 (Mitte): `MODE / OK` | Input Pull-Up (Active-LOW) | Phase 2: `HEAT` ↔ `STANDBY` umschalten; Langdruck speichert im Flash |
| `PB3` | Taste 4: `Pfeil rechts (>)` | Input Pull-Up (Active-LOW) | Phase 2: Solltemperatur `+5 °C` (Langdruck `+25 °C`) |
| `PB4` | Taste 5 (rechts): `Menü` | Input Pull-Up (Active-LOW) | Phase 2: Backlight-Stufen `20/50/80/100 %`; Langdruck USB-Stream |
| `PB5` | Sleep-Ablage | Input Pull-Down + `EXINT5` | Interrupt auf steigende/fallende Flanke (`LOW` = im Ständer, `HIGH` = entnommen) |
| `PB6` | Griff-Erkennung | Input Pull-Up | **Verifiziert:** `HIGH` (`1`) = **`F245`**, `LOW` (`0`) = **`F210`** |
| `PC13, PC14, PC15` | Griff-Mux / Gain-Steuerung | Output Push-Pull | Analogbereich-Umschaltung via Backup-Domain-Unlock (`F245`: `PC14=1, PC13/15=0`; `F210`: `PC14=0, PC13/15=1`) |
| `PB0` | Soft-Power-Taster / Diag | Input Pull-Up | Flankensequenz steuert Power-Latch `PB1` |
| `PB2` | Diag-Eingang | Input Pull-Down | Live-Monitoring |
| `PB10, PB11` | Bit-Bang `I2C` | Output / Open-Drain | Nebenbus für Slave-Adresse `0x78` (optionales Power-/Display-IC) |
| `PF4, PF5` | Front-LEDs 0..1 | Output Push-Pull | Status-LEDs |
| `PA4..PA7` | Front-LEDs 2..5 | Output Push-Pull | Status-LEDs |
| `PC4, PC5` | Front-LEDs 6..7 | Output Push-Pull | Status-LEDs |
| `PA11, PA12` | USB `D-` / `D+` | `OTGFS1` (`0x50000000`) | USB 2.0 Full-Speed CDC-ACM (IRQ 67) |

## 4. ST7789V Display (`320 × 240`, RGB565)

- SPI Mode 3 (`CLKPOL=1, CLKPHA=1`), 8-Bit MSB-first, 18 MHz (`APB1 / 4`).
- `MADCTL (0x36) = 0xA0`, `COLMOD (0x3A) = 0x05`, keine Invertierung (`0x21` aus), X/Y-Offset `0, 0`.

## 5. USB & IOMUX Besonderheiten (AT32F415)

- `OTG_GCCFG` (`0x50000038`) muss `0x002D0000` setzen (`PWRDOWN | AVALIDSEN | BVALIDSEN | VBUSIG` = Bits `16, 18, 19, 21`), **bevor** `OTG_GUSBCFG` in den Device-Mode (`FDEVMODE`, `TRDT = 5`) umgeschaltet wird (verifiziert gegen `IPS3608` auf identischem AT32F415-Silizium).
- `IOMUX_REMAP` (`0x40010004`) setzt `SWJTAG_CONF = 0b010`, um `PB3`/`PB4` von JTAG zu lösen, ohne SWD zu deaktivieren.
- `IOMUX_REMAP4` (`0x40010024`) setzt Bits `[11:8] = 0x3` (`TMR3_GMUX_0011`), um `TMR3_CH3/CH4` auf `PC8/PC9` zu legen (verifiziert aus `APP_DWS_200_V1.5` `0x08013152`).

## 6. Verifizierte Phase-1-Messwerte (v0.1.1-bringup, COM7)

- **USB-CDC Enumeration:** `USB\VID_2E3C&PID_5740\DWS200-P1` (`COM7`, `144 MHz` PLL / `48 MHz` USB-Takt verifiziert).
- **`PC0` (`TIP`):** `11..25 raw` (`9..20 mV`) bei Raumtemperatur.
- **`PC1` (`CUR`):** `2037..2038 raw` (`1642 mV`, ca. `-8 mV` Ruhe-Offset zum theoretischen `1650 mV`-Nullpunkt).
- **`PA3` (`AUX` / 24-V-Rail):** Initial `180 raw` (`145 mV × 16 = 2,32 V` reiner USB-Betrieb), nach Netzeinschalten `1777..1780 raw` (`1432..1434 mV × 16 = 22,94 V`); Details zu PID, Kennlinien & Timing siehe [phase2-control-analysis.md](phase2-control-analysis.md).
- **`PB5` (`EXINT5`):** Als mechanischer Ständer-/Schlafkontakt bestätigt (`pb5_cnt = 115` Prellflanken beim Bewegen im Ständer, im Ruhezustand konstant).
- **Sicherheitsverriegelung:** `PA2` bleibt permanent `LOW` (`heater=0`), `HEAT 1` und `TEMP 350` liefern `ERR HEATER_LOCKED_IN_SAFE_MODE`.
