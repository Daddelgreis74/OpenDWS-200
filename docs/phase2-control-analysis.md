# Phase-2 Reverse-Engineering Analysis (`APP_DWS_200_V1.5_250922.bin`)

This specification documents the heating, sensing, handle-switching, PID control, and protection architecture verified from stock firmware `APP_DWS_200_V1.5_250922.bin` (Artery `AT32F415RCT7`, app base `0x08006000`) as the direct foundation for OpenDWS-200 Phase 2.

---

## 1. Timer & Blanking Architecture (`TMR1` + `TMR2`)

Stock DWS-200 does **not** use AC zero-crossing; instead it runs a **DC-pulsed `10 ms` (`100 Hz`) heating and measurement time slice** driven by two hardware timers:

### 1.1 `TMR1` – `1.0 ms` Scheduler (`0x08014ab4`, `TMR1_OVF_IRQHandler` @ `0x080098f8`)

- **Configuration:** `144 MHz / (143 + 1) = 1 MHz` counter clock, period `PR = 999` → **`1.0 ms` (`1 kHz`)** overflow interrupt (IRQ 25).
- **10-Slot Round-Robin (`slot = counter % 10`, full period `10 ms` / `100 Hz`):**
  - **Slot 0 (`t = 0 ms`, `0x080123b0`):** Key scan (`0x080114dc`), handle detect (`PB6` → `PC13..PC15`, `0x08011898`), sleep stand (`PB5`, `0x08011310`), soft-power check (`PB0`/`PB1`, `0x08011834`).
  - **Slot 1 (`t = 1 ms`, `0x08009960`):** Calls `Heater_Ctrl(0)` (`0x08012cfc`) → **forces `PA2 = LOW`**, starting the **blanking window** so heater current settles cleanly before thermocouple sampling.
  - **Slot 2 (`t = 2 ms`, `0x080113a4`):** Exactly **`1.0 ms` after `PA2 = LOW`** → read thermocouple `PC0` (`0x0800fd84`), open-tip check (`tip_adc > 4000`), run incremental PID (`0x080129f0`), and fire the new heater pulse (`0x08012cfc`).
  - **Slot 3 (`t = 3 ms`, `0x080123c8`):** Sleep/standby countdown (`6000 × 10 ms` factor, `0x1770`) and buzzer stop timer.
  - **Slot 4 (`t = 4 ms`, `0x0801196c`):** Every 50 cycles (`500 ms`), sample `PA3` (`ADC1_CH3`) → `V_rail = (adc * 3.3 / 4095.0) * 16.0` (`24 V` rail; verified on real hardware: `1.434 V × 16 = 22.94 V` on mains, `0.145 V × 16 = 2.32 V` on USB power).
  - **Slot 5 (`t = 5 ms`, `0x08009988`):** Sample `PC1` (`ADC1_CH11`) Hall current sensor → `I = |(adc * 3.3 / 4095.0 - 1.65) / 0.066|`. If `I > 5.0 A`, increments sustained overload counter (`3000` ticks = `30 s` → fault `[0x20000340 + 0x22] = 1`).

### 1.2 `TMR2` – `91 µs` Pulse Width & Overcurrent Timer (`0x08014b18`, `TMR2_IRQHandler` @ `0x08009a68`)

- **Configuration:** `144 MHz / (143 + 1) = 1 MHz` counter clock, period `PR = 90` (`0x5A`) → **`91 µs`** (`~10.99 kHz`, IRQ 28).
- **Pulse Cutoff:** Decrements remaining pulse ticks (`[0x20000340 + 0x3d]`) and drives `PA2 = LOW` (`0x08009bb0`) upon reaching `0`.
- **Short-Circuit & Load Sensing (`0x08009abc`):** Exactly **4 ticks (`364 µs`) after pulse start** (`remaining == initial - 4`), samples `PC1` (`ADC1_CH11`) and computes `I`:
  - Immediate emergency shutdown (`[0x20000340 + 0x23] = 1`):
    - **`handle_type == 1`:** `I > 10.0 A` (`0x40240000`)
    - **`handle_type == 0`:** `I > 15.0 A` (`0x402E0000`)
  - PID parameter set selection (`[0x20000340 + 0x24]`): `I < 5.0 A` (`0x40140000`) sets `flag24 = 1`.

---

## 2. Handle Detection (`PB6`) & Gain/Mux Switching (`PC13`, `PC14`, `PC15`)

- `Stand_Read` (`0x080146d0`) samples `PB5`; `Handle_Read` (`0x08006c74`) samples `PB6`.
- `Sleep_Stand_0x08011310` debounces `PB6` over `5` cycles (`50 ms`), switches analog mux lines `PC13`, `PC14`, `PC15`, and locks the PID for `100` cycles (`1.0 s`, `[0x20000028 + 5] = 100`) via `0x08011360` while the thermocouple front-end settles:

| `PB6` State | Stock `handle_type` / Handle | `PC13` | `PC14` | `PC15` | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`PB6 == 1`** | `1` — **`F245`** *(hardware verified)* | `LOW` | `HIGH` | `LOW` | `200 W` range, max duty `80..90` ticks, overcurrent `10.0 A` |
| **`PB6 == 0`** | `0` — **`F210`** | `HIGH` | `LOW` | `HIGH` | Hard-capped at `30` ticks (`2.73 ms`), overcurrent `15.0 A` |

> [!NOTE]
> The mapping **`PB6 == 1` = `F245`** was verified on the real station. At boot and on every debounced handle change (`50 ms`), OpenDWS-200 configures `PC13..PC15` (after unlocking backup domain `PWC_CTRL.BPWEN` + `CRM_BPDC.LEXTEN=0`) and enforces a `1.0 s` settling lockout.

---

## 3. Thermocouple Lookup Tables (`PC0` / `ADC1_CH10` → °C, `0x0800fd84`)

After the `1.0 ms` blanking pause, `0x0800fd84` samples `ADC1_CH10` (`PC0`, `tip_adc`) and converts via **5-segment piecewise-linear interpolation** with 3-point calibration (`150 °C`, `250 °C`, `350 °C`, where `450 °C` shares the `350 °C` offset) plus the fixed **stock V1.5 cold-junction baseline `+25.0 °C`** (`0x0801243E` / `0x08012A30` / `0x08012F0A`). PID control (`0.1 °C`) uses an EMA filter (`alpha = 0.2`), while TFT and PC Studio displays apply an additional smoothed whole-degree filter with `±0.7 °C` hysteresis:

| Knot | `handle_type == 1` (`PB6 == 1`) | `handle_type == 0` (`PB6 == 0`) |
| :--- | :--- | :--- |
| `0 °C` (+`25 °C` CJ) | `0 raw` (`0 mV`) | `0 raw` (`0 mV`) |
| `150 °C` (+`25 °C` CJ) | `1100 raw` (`0x044C`, `~886 mV`) | `276 raw` (`0x0114`, `~222 mV`) |
| `250 °C` (+`25 °C` CJ) | `1870 raw` (`0x074E`, `~1507 mV`) | `608 raw` (`0x0260`, `~490 mV`) |
| `350 °C` (+`25 °C` CJ) | `2610 raw` (`0x0A32`, `~2103 mV`) | `882 raw` (`0x0372`, `~711 mV`) |
| `450 °C` (+`25 °C` CJ) | `3340 raw` (`0x0D0C`, `~2692 mV`) | `1210 raw` (`0x04BA`, `~975 mV`) |

*(Stock firmware always adds the `+25 °C` cold-junction baseline after LUT interpolation (`25 °C` at `0 raw`), so measured room-temperature readings of `11..25 raw` correspond to `~26.5..28.4 °C`.)*

---

## 4. Incremental PID Controller & Duty Limits (`0x080129f0`, `0x08012cfc`)

The controller executes every `10 ms` as an incremental PID (`err = target_temp - tip_temp`) with bang-bang thresholds:

- **Bang-Bang Limits:**
  - `err > +50 °C` (`0x40490000`) → `100 %` (clamped by active duty cap)
  - `err < -5 °C` (`0xC0140000`) → `0 %` (`PA2` off)
- **PID Gains (`Kp`, `Ki`, `Kd`) & Pulse Limits (`1 tick = 91 µs`):**

| Mode | Condition | `Kp` | `Ki` | `Kd` | Max Pulse Width / `10 ms` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `handle_type == 1` | Load `I >= 5 A` (`flag24 == 0`) | `2.28` | `0.92` | `0.28` | `80` ticks (`7.28 ms`) for `T < 100 °C`, else `100` (eff. `~9.0 ms` before Slot 1) |
| `handle_type == 1` | Light load `I < 5 A` (`flag24 == 1`) | `2.48` | `0.015` | `0.02` | same as above |
| `handle_type == 0` | `T <= 150 °C` | `0.30` | `0.02` | `0.01` | Hard-capped at `30` ticks (`2.73 ms` ≈ `27.3 %`) |
| `handle_type == 0` | `T > 150 °C` | `0.40` | `0.02` | `0.01` | Hard-capped at `30` ticks (`2.73 ms`) |
| `handle_type == 0` | `T > 250 °C` | `0.50` | `0.02` | `0.01` | Hard-capped at `30` ticks (`2.73 ms`) |
| `handle_type == 0` | `T > 350 °C` | `0.60` | `0.02` | `0.01` | Hard-capped at `30` ticks (`2.73 ms`) |

---

## 5. Protection, Power & Peripherals

- **Open-Tip / Cartridge Swap (`0x08011400`):** `tip_adc > 4000` (`0x0FA0`) reports missing cartridge and cuts heater drive immediately. Upon re-insertion, firmware waits `50` cycles (`500 ms`), fires a single **`5`-tick probe pulse (`455 µs`)** (`0x0801143e`) to check for shorts (`364 µs` sample in `TMR2`), and then re-enables PID control.
- **Short-Circuit & Sustained Overload Protection:** Instant cutoff in `TMR2` at `I > 10 A` (`handle_type 1`) or `> 15 A` (`handle_type 0`) plus `30 s` sustained overload trip (`I > 5 A`).
- **Soft-Power Latch (`PB0` / `PB1`, `0x08011834`):** `PB1` holds the system powered (`HIGH`); a `PB0` button sequence drops `PB1 = LOW`.
- **Auxiliary I2C (`PB10` / `PB11`, `0x08014304`):** Bit-banged I2C for slave address `0x78`.
- **Flash Settings (`0x0803F800`):** `200` bytes (`0x64` halfwords in stock, magic `0x001F`, including separate 3-point calibration offsets for `handle_type 1` and `handle_type 0`).
- **OpenDWS-200 Phase-2 Additional Safety Guarantees:**
  1. Hardware watchdog (`WDT` ~2 s) resets on hang with `PA2 = LOW` from instruction 1.
  2. **`24 V` Rail Lockout (`PA3`):** Heating stays strictly blocked while `V_rail < 15 V` (prevents heating attempts on `5 V` USB power).
  3. **Hall Sensor Auto-Zero (`PC1`):** Samples idle offset while `heater == 0` (`~1642 mV` measured vs ideal `1650 mV`).
  4. **Absolute Software Cutoff:** Immediate fault trip above `480 °C`, missing handle signal, or *Thermal Runaway* (`8 s` at high duty without temperature rise).
