# OpenDWS-200 ⚡🪛

[![Build & Release](https://github.com/Daddelgreis74/OpenDWS-200/actions/workflows/build-and-release.yml/badge.svg)](https://github.com/Daddelgreis74/OpenDWS-200/actions/workflows/build-and-release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Alternative **Open-Source Bare-Metal-Firmware** für die **FNIRSI DWS-200** 200-W-Präzisionslötstation (MCU: Artery `AT32F415RCT7`, ARM Cortex-M4 ohne FPU, `144 MHz`).

> [!IMPORTANT]
> **Phase 2 — Geschlossene PID-Regelung, Standby-Vorheizen & Soft-Power (`v0.2.6-phase2`)**
> Nach dem Einschalten fährt die Station (sofern der Kolben im Ständer ruht und eine Spitze steckt) automatisch in den **Standby-Bildschirm (`100 °C` Vorheizen)** und wartet auf die Entnahme des Kolbens (`PB5`). Fehlt die Spitze (`PC0 > 4000`), erscheint automatisch die grafische **`NO TIP! / KEINE SPITZE!`**-Warnung mit wiederkehrendem Signalton (`1,5 s`), und Heizen bleibt strikt gesperrt. Der **Power-Button (`PB0`/`PB1`)** speichert geänderte Settings und schaltet sauber ab.

---

## 🌟 Features (Phase 2)

- **⚡ Originalgetreuer Auto-Ständer (`PB5`), Standby-Vorheizen & No-Tip-Alarm:**
  - Start im Ständer (`PB5 = LOW`) heizt nach dem `455-µs`-Prüfpuls automatisch auf Standby-Temperatur (`100 °C` Standard) vor und zeigt den Standby-Schirm bis zum Entnehmen des Kolbens (`PB5: LOW→HIGH`). Liegt der Kolben beim Einschalten bereits außerhalb des Ständers, bleibt die Heizung aus Sicherheitsgründen aus (`PLACE IRON IN STAND FIRST`).
  - Bei fehlender Spitze (`FAULT_OPEN_TIP`) erscheint ein eigener roter Grafik-Warnbildschirm (`NO TIP! / KEINE SPITZE!`) mit alle `1,5 s` wiederkehrendem Alarmton, bis eine Spitze eingesteckt wird.
  - Power-Button (`PB0`) speichert geänderte Settings im Flash `0x0803F800` und schaltet über `PB1 = LOW` hart ab.

- **🔥 `100 Hz` Blanking & `91 µs` PID-Regelung (`TMR1` + `TMR2`):**
  - Verifiziertes DC-Blanking aus `APP_DWS_200_V1.5`: `1,0 ms` nach `PA2 = LOW` misst `PC0` die Thermospannung ohne Heizstrom-Störung (`alpha = 0,2` EMA).
  - Automatische Griff-Erkennung über `PB6` + Backup-Domain Analog-Mux (`PC13..PC15`) inkl. `1,0 s` Einschwing-Sperre:
    - **`F245` (`PB6 = 1`):** `200 W` Profil, `5`-Segment-Kennlinie, max. `80..90` Ticks (`7,3..8,2 ms`), Kurzschluss-Cutoff `10,0 A`.
    - **`F210` (`PB6 = 0`):** `C210`-Kennlinie, hart begrenzt auf max. `30` Ticks (`2,73 ms`), Kurzschluss-Cutoff `15,0 A`.
- **🛡️ Mehrschichtige Sicherheitsarchitektur:**
  - `364 µs` Sofort-Überstromabschaltung im `TMR2`-Interrupt + `455 µs` Prüfpuls bei Erststart und Spitzenwechsel.
  - `24-V`-Schienenüberwachung (`PA3 × 16`), Open-Tip (`> 4000 raw`), `30 s`-Dauerüberlast (`> 5 A`), `480 °C`-Hartlimit, *Thermal Runaway Protection* (`8 s`) und Hardware-Watchdog (`2 s`).
- **🖥️ Aurora-Dark Löt-UI (`320 × 240`, ST7789V) & 5-Tasten-Steuerung:**
  - Hauptschirm (v. links nach rechts): `CH/CLR` (`PC11`: Presets `200/300/350 °C` / Fault löschen) | `-5 °C` (`PC12`) | `MODE/OK` (`PD2`: kurz zwischen Hauptansicht & Temperatur-/Leistungs-Diagramm `GRAPH` wechseln, lang in Flash `0x0803F800` speichern; im Menü: Eingaben mit `OK` bestätigen) | `+5 °C` (`PB3`) | `MENU` (`PB4`: Einstellungsmenü öffnen).
  - **On-Screen-Einstellungsmenü (`PB4`):** Sleep-Temp (`50–250 °C`, Standard `100 °C`), Sleep-Ständer `ON/OFF`, **Standby Screen `ON/OFF`**, Presets `CH1..3`, Spitzenkalibrierung je Griff, Backlight `10–100 %`, Buzzer `ON/OFF`, USB-Stream `ON/OFF` und `Save to Flash`.
- **🔌 USB-CDC Live-Telemetrie & Fernsteuerung (`VID 0x2E3C / PID 0x5740`):**
  - Vollständiges ASCII-Protokoll (`HELP`, `INFO`, `STAT`, `HEAT`, `TEMP`, `PRESET`, `SLEEP`, `CAL`, `CLEAR`, `SAVE`, `STREAM`, `BL`, `BEEP`, `LED`, `REBOOT`).

---

## 🔌 Flashen & Zurückflashen (ohne Programmer)

1. Lötstation am rückseitigen Netzschalter ausschalten.
2. **MODE-Taste** (`PD2`) gedrückt halten und per **USB-C-Datenkabel** mit dem PC verbinden.
3. Im Windows-Explorer erscheint das Laufwerk `BOOTLOADER`.
4. `APP_DWS_200_OpenDWS-*.bin` (aus den [GitHub Releases](../../releases)) auf `BOOTLOADER` kopieren und neu starten.
5. Rückkehr zur Stock-Firmware jederzeit durch Aufspielen von `APP_DWS_200_V1.5_250922.bin` im `BOOTLOADER`-Modus.

---

## 📚 Dokumentation & Tests

- [Hardware-Map & Pinout](docs/hardware-map.md)
- [Phase-2 Regelungs- & Stock-Analyse](docs/phase2-control-analysis.md)
- [USB-CDC Protokoll-Referenz](docs/usb-protocol.md)

```bash
# Host-Unit-Tests (ASan + UBSan)
make test

# ARM-Firmware bauen + Binärdatei prüfen (CI mit clang + lld + llvm)
make firmware
make verify-bin

# UI-Vorschau-Screenshots erzeugen
make screenshots
```

---

## 📜 Lizenz

[MIT License](LICENSE) — Copyright (c) 2026 Daddelgreis74.
