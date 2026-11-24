# Changelog

All notable releases of **OpenDWS-200** (Firmware + **OpenDWS-200 Studio**).

## [v1.0.0] — Final Combined Release
- **Clean release filenames (without extra `.sha256` files):**
  - `OpenDWS-200-Firmware-v1.0.0.bin`
  - `OpenDWS-200-Studio-Setup-v1.0.0.exe` (Windows NSIS Installer)
  - `OpenDWS-200-Studio-Portable-v1.0.0.exe`
- **Documentation & Previews:** Updated `README.md`, `docs/usb-protocol.md`, `Makefile` default version, and all UI preview screenshots (`docs/screenshots/*.png`) for the final release.
- **PC Studio Updater:** Prioritizes `OpenDWS-200-Firmware-*.bin` assets while staying backwards-compatible with `APP_DWS_200_*.bin`.

## [v0.3.1] — Editable Presets (`CH1–CH3`)
- **PC Studio:** Made `CH1`, `CH2`, and `CH3` editable directly beneath the preset buttons (`100..450 °C`, saved locally in `localStorage` and synchronized bidirectionally with the station).
- **Firmware USB Protocol:** Extended `PRESET <1..3> [100..450]` to query or update preset channels over USB and added `presets=p1,p2,p3` to `STAT` telemetry.

## [v0.3.0] — Stock Cold-Junction Baseline (`+25 °C`), 3-Point Calibration & Calm Display
- **Stock V1.5 Cold-Junction Offset:** Added the stock-verified `+25.0 °C` cold-junction baseline (`TIP_COLD_JUNCTION_DECI_C = 250`) after the 5-segment thermocouple LUT.
- **3-Point Tip Calibration (`150 °C`, `250 °C`, `350 °C`):** Stored per handle (`F245` and `F210`) in Flash settings (`SETTINGS_VERSION = 6`), exposed in the station menu (`Cal 150 / 250 / 350`), USB protocol (`CAL [150|250|350] <-50..50>`), and PC Studio.
- **Calm Whole-Degree Readout:** Adaptive smoothing + `±0.7 °C` hysteresis rounded to steady whole `°C` on the station TFT and PC Studio hero KPI (while PID control, live chart, and CSV logging retain full `0.1 °C` resolution).

## [v0.2.7-phase2] — Phase-2 PID, Auto-Stand, Auto-Off & PC Studio Foundation
- Closed-loop `100 Hz` / `91 µs` incremental PID control, automatic `F245`/`F210` detection (`PB6` + `PC13..PC15`), full-screen standby preheat, configurable auto-off timer, live `GRAPH` mode, and Flash settings.
- **OpenDWS-200 Studio:** Live plotter, automatic USB-CDC connect/reconnect (`Auto-USB`), custom 3D icon, Windows setup installer, CSV logger, and built-in `BOOTLOADER` firmware updater.
