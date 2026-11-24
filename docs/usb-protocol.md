# OpenDWS-200 USB-CDC Telemetrie- & Steuerungsprotokoll (Phase 2)

Im laufenden Betrieb meldet sich OpenDWS-200 am USB-C-Port als **USB CDC-ACM (Virtueller COM-Port)** mit `VID 0x2E3C` / `PID 0x5740` (`OpenDWS-200 Phase-2 CDC`).

- **Zeilenformat:** ASCII, abgeschlossen mit `\r` oder `\n` (Antworten enden mit `\r\n`).
- **Groß-/Kleinschreibung:** Kommandos sind case-insensitiv.
- **Baudrate:** Beliebig (z. B. `115200 8N1`), da reines USB Full-Speed Bulk.

---

## Befehlsübersicht

| Befehl | Beschreibung | Beispielantwort |
| :--- | :--- | :--- |
| `HELP` oder `?` | Liste aller Befehle | `OK CMDS: HELP \| INFO \| STAT \| ...` |
| `INFO` | Firmware, MCU, Griff, Zustand, 24-V-Rail, Uptime | `INFO fw=v0.3.1 mcu=AT32F415RCT7 clk=144MHz safe_mode=0 handle=F245 state=STANDBY rail_mv=22940 uptime_ms=12540` |
| `STAT` | Vollständiger Telemetrie-Schnappschuss | `STAT ms=12540 state=HEAT fault=0x00(NONE) handle=F245 tip_c=300.2 target_c=300 act_c=300 duty=45 ticks=40 rail_mv=22928 ... cal=0,0,0 presets=200,300,350` |
| `HEAT <ON\|OFF\|1\|0>` | Heizung aktivieren / in `STANDBY` setzen (verweigert bei `NO_24V_RAIL` oder aktivem Fault) | `OK HEAT=ON` oder `ERR NO_24V_RAIL` |
| `TEMP <100..450>` | Solltemperatur in °C setzen (`SET <t>` als Alias) | `OK TEMP=320` |
| `PRESET <1..3> [100..450]` | Preset CH1–CH3 laden oder neuen Temperaturwert (`100..450 °C`) setzen | `OK PRESET=2 TEMP=325` |
| `SLEEP <ON\|OFF\|0\|50..250>` | Auto-Stand aktivieren/deaktivieren oder Sleep-Temp (`0` = `0 W` kalt, `50..250 °C`, Standard `100 °C`) setzen | `OK SLEEP=ON` / `OK SLEEP=100` |
| `CAL [150\|250\|350] <-50..50>` | 3-Punkt-Kalibrier-Offset (`150`/`250`/`350 °C`) in °C für aktiven Griff setzen (ohne Stützpunkt: alle 3 Punkte) | `OK CAL 250=12` oder `OK CAL=2` |
| `CLEAR` | Gelatchte Faults (`OVERCUR`, `OVERLOAD`, `OVERTEMP`, `RUNAWAY`) zurücksetzen | `OK CLEAR` |
| `SAVE` | Aktuelle Settings sicher (inkl. `PA2=LOW`-Pause) im Flash `0x0803F800` speichern | `OK SAVE` |
| `STREAM [ON\|OFF\|50..2000]` | Zyklischen `STAT`-Stream steuern | `OK STREAM=ON INTERVAL=100` |
| `BL <0..100>` | Display-Backlight in % setzen | `OK BL=80` |
| `BEEP [10..1000]` | Piezo-Buzzer für `ms` (Standard `80 ms`) | `OK BEEP=80` |
| `LED <0..255\|0x..\|AUTO>` | Front-LEDs manuell setzen oder zurück in `AUTO`-Modus (Leistungs-/Fault-Anzeige) | `OK LED=AUTO` |
| `REBOOT` | Watchdog-/System-Reset auslösen | `OK REBOOT` |

---

## Wichtige Felder von `STAT`

- `state`: `STANDBY`, `SLEEP`, `PROBE`, `HEAT` oder `FAULT`.
- `fault`: Aktive Bitmaske und Primärfehler (`NONE`, `SETTLING`, `OPEN_TIP`, `NO_24V`, `OVERVOLT`, `OVERCUR`, `OVERLOAD`, `OVERTEMP`, `RUNAWAY`).
- `handle`: Aktiver Handgriff (`F245` bei `PB6=1`, `F210` bei `PB6=0`).
- `tip_c`, `target_c`, `act_c`: Gefilterte Spitzen-Isttemperatur (`0,1 °C`) für PID/Logger, eingestellter Sollwert und aktiver Sollwert (TFT- und PC-Anzeige runden/glätten separat auf ruhige ganze `°C`).
- `cal`: Aktuelle 3-Punkt-Kalibrier-Offsets (`150,250,350` °C) des aktiven Griffs (`F245`/`F210`).
- `presets`: Aktuelle Sollwerte von `CH1,CH2,CH3` in °C (z. B. `200,300,350`).
- `duty`, `ticks`: Aktive PWM-Aussteuerung in % sowie Pulsbreite in `91-µs`-Ticks.
- `rail_mv`: Gemessene 24-V-Versorgungsschiene (`PA3 × 16`; `< 15000 mV` sperrt die Heizung automatisch).
- `cur_ma`, `cur_pk`, `zero_mv`: Live-Strom, Spitzenstrom bei `364 µs` und automatisch nachgeführter Hall-Nullpunkt (`PC1`).
