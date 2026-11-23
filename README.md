# OpenDWS-200 ⚡🪛

[![Build & Release](https://github.com/Daddelgreis74/OpenDWS-200/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/Daddelgreis74/OpenDWS-200/actions/workflows/build-and-release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Alternative **Open-Source Bare-Metal-Firmware** für die **FNIRSI DWS-200** 200-W-Präzisionslötstation (MCU: Artery `AT32F415RCT7`, ARM Cortex-M4 ohne FPU, `144 MHz`).

> [!WARNING]
> **Phase 1 — Safe Bring-Up (Heizung fest gesperrt!)**
> In dieser ersten Projektphase sind `OPENDWS_SAFE_MODE = 1` und `OPENDWS_ALLOW_HEATER = 0` hart eincompiliert. Der Heizungs-Ausgang `PA2` wird bereits in den allerersten CPU-Instruktionen von `Reset_Handler`, in jedem Loop-Durchlauf und in allen Fault-Handlern auf `LOW` gezwungen.
> Zweck von Phase 1 ist die risikofreie Verifikation von **Display, Backlight, Buzzer, LEDs, Tasten, Sleep-Sensor, ADC-Kanälen und USB-CDC Telemetrie** auf echter Hardware, bevor in Phase 2 die PID-Leistungsregelung freigeschaltet wird.

---

## 🌟 Features (Phase 1)

- **🖥️ Natives ST7789V Aurora-Dark UI (`320 × 240`, SPI2):**
  - Live-Anzeige aller drei 12-Bit-ADC-Kanäle (`PC0` Thermoelement, `PC1` Hall-Stromsensor, `PA3` Aux) inklusive 64-Punkt-Sparklines.
  - Interaktive Live-Kacheln für alle 5 Fronttasten (`PD2`, `PB3`, `PB4`, `PC12`, `PC11`), Griff-Erkennung (`PB6`), Sleep-Ablage (`PB5` + `EXINT5` Flankenzähler) und Hardware-Safety-Badge (`HEATER OFF`).
- **🔌 USB-CDC Live-Telemetrie & Fernsteuerung (`OTGFS1`):**
  - Virtueller COM-Port (`VID 0x2E3C / PID 0x5740`) direkt über die rückseitige USB-C-Buchse — etwas, das die originale FNIRSI-Firmware im Normalbetrieb nicht unterstützt!
  - Befehle: `HELP`, `INFO`, `STAT`, `STREAM ON/OFF/<ms>`, `BL <0..100>`, `BEEP <ms>`, `LED <mask>`, `REBOOT`.
- **🛡️ Bootloader- & Flash-Schutz:**
  - Startet ab `0x08006000` (max. 230 KB); der originale `24 KB` USB-Bootloader (`0x08000000–0x08005FFF`) und die originale Settings-Page (`0x0803F800`) bleiben unangetastet.
  - Ein Wechsel zurück auf die originale FNIRSI-Firmware `APP_DWS_200_V1.5_250922.bin` ist jederzeit möglich.
- **🧪 Qualitätsgesichert:**
  - Host-Unit-Tests mit ASan/UBSan (`sensors`, `input`, `protocol`, `settings`, `ui`), automatische `.bin`- & Disassembly-Prüfung (`tools/verify_bin.js`: garantiert u. a. **0 FPU-Befehle** auf dem AT32F415) und automatische UI-Screenshots.

---

## 🔌 Flashen & Zurückflashen (ohne Programmer)

1. Lötstation am rückseitigen Netzschalter ausschalten.
2. **MODE / Drehknopf-Taste** gedrückt halten und mit einem **USB-C-Datenkabel** (kein reines Ladekabel!) mit dem PC verbinden bzw. einschalten.
3. Im Windows-Explorer erscheint das virtuelle Laufwerk `BOOTLOADER`.
4. `APP_DWS_200_OpenDWS-*.bin` (aus den [GitHub Releases](../../releases) oder CI-Artifacts) in das Hauptverzeichnis von `BOOTLOADER` kopieren.
5. USB trennen / neu starten.

Zum **Zurückflashen auf Stock-Firmware** einfach wieder in den `BOOTLOADER`-Modus gehen und deine originale `APP_DWS_200_V1.5_250922.bin` auf das Laufwerk kopieren.

---

## 🗺️ Roadmap

- **Phase 1 (aktuell):** Safe Bring-Up — Takt, Display, Eingänge, ADC-Diagnose, Watchdog & USB-CDC (`PA2` gesperrt).
- **Phase 2:** Vollständiges Reverse Engineering von `TMR1`/`TMR2`/Zero-Cross-Timing, Kalibrierkurven (`F245`/`F210`), PID-Regler mit Hardware-Überstrom-/Unterbrechungs-Schutz und Flash-Settings.
- **Phase 3:** Vollständiges Löt-UI (Sollwert, Presets, Echtzeit-Leistungskurve, Standby/Sleep) & Desktop-Suite (`FNIRSI DWS-200 Studio`).

---

## 📚 Dokumentation & Entwicklung

- [Hardware-Map & Pinout](docs/hardware-map.md)
- [USB-CDC Protokoll-Referenz](docs/usb-protocol.md)

```bash
# Host-Unit-Tests (ASan + UBSan)
make test

# ARM-Firmware bauen + Binärdatei prüfen (Linux / CI mit clang + lld + llvm)
make firmware
make verify-bin

# UI-Vorschau-Screenshots erzeugen
make screenshots
```

---

## 📜 Lizenz

[MIT License](LICENSE) — Copyright (c) 2026 Daddelgreis74.
