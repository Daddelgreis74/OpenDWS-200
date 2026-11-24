# Changelog

Alle wesentlichen Meilensteine von **OpenDWS-200** (Firmware + **OpenDWS-200 Studio**).

## [v1.0.0] — Finales kombiniertes Release
- **Saubere Release-Dateien ohne Zusatz-Checksums:**
  - `OpenDWS-200-Firmware-v1.0.0.bin`
  - `OpenDWS-200-Studio-Setup-v1.0.0.exe` (Windows NSIS Installer)
  - `OpenDWS-200-Studio-Portable-v1.0.0.exe`
- **Dokumentation & Vorschau:** `README.md`, `docs/usb-protocol.md` und alle UI-Screenshots (`docs/screenshots/*.png`) auf den finalen Stand gebracht.
- **PC Studio Updater:** Erkennt neben `APP_DWS_200_*.bin` nun bevorzugt `OpenDWS-200-Firmware-*.bin`.

## [v0.3.1] — Editierbare Presets (`CH1–CH3`)
- **PC Studio:** `CH1`, `CH2` und `CH3` unter den Preset-Tasten direkt editierbar (`100..450 °C`, im `localStorage` gesichert und bidirektional mit der Station synchronisiert).
- **Firmware USB-Protokoll:** `PRESET <1..3> [100..450]` zum Lesen/Ändern von Presets per USB sowie `presets=p1,p2,p3` im `STAT`-Telemetrie-Stream.

## [v0.3.0] — Stock-Kaltstelle (`+25 °C`), 3-Punkt-Kalibrierung & ruhige Anzeige
- **Stock-V1.5-Kaltstellen-Offset:** Festes `+25,0 °C`-Baseline-Offset (`TIP_COLD_JUNCTION_DECI_C = 250`) nach der 5-Segment-LUT ergänzt.
- **3-Punkt-Spitzenkalibrierung (`150 °C`, `250 °C`, `350 °C`):** Getrennt für `F245` und `F210` in Flash-Settings (`SETTINGS_VERSION = 6`), Stationsmenü (`Cal 150 / 250 / 350`), USB (`CAL [150|250|350] <-50..50>`) und PC Studio.
- **Ruhige Temperaturanzeige:** Adaptiver Glättungsfilter mit `±0,7 °C`-Hysterese und Ganzzahl-Rundung auf TFT und PC-Studio-KPI (PID, Graph und CSV behalten volle `0,1 °C`-Auflösung).

## [v0.2.7-phase2] — Phase-2 PID, Auto-Ständer, Auto-Off & PC Studio Basis
- Geschlossene `100 Hz` / `91 µs`-PID-Regelung, automatische `F245`/`F210`-Erkennung (`PB6` + `PC13..PC15`), Standby-Schirm, konfigurierbarer Auto-Off-Timer, `GRAPH`-Modus und Flash-Settings.
- **OpenDWS-200 Studio:** Live-Plotter, Auto-USB-Reconnect, 3D-Icon, Windows-Setup-Installer, CSV-Logger und integrierter `BOOTLOADER`-Firmware-Updater.
